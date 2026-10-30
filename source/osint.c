
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>

#include <ppu-types.h>
#include <sys/process.h>
#include <sysutil/sysutil.h>
#include <sysutil/video.h>
#include <rsx/rsx.h>
#include <io/pad.h>

#include "osint.h" 
#include "vecx.h"

SYS_PROCESS_PARAM(1001, 0x100000);

#define USRDIR "/dev_hdd0/tmp/ps3vecx/"

typedef struct { int w, h; uint32_t *ptr; uint32_t offset; } Buf;

static gcmContextData *ctx;
static Buf bufs[2];
static int draw_idx = 0; 
static int shown_idx = 1;
static int dirty = 0;
static volatile int running = 1;

static void make_buf(Buf *b, int w, int h, int id)
{
    b->w = w; b->h = h;
    b->ptr = (uint32_t *)rsxMemalign(64, w * h * 4);
    rsxAddressToOffset(b->ptr, &b->offset);
    gcmSetDisplayBuffer(id, b->offset, w * 4, w, h);
    memset(b->ptr, 0, w * h * 4);
}

static void present(int idx)
{
    gcmResetFlipStatus();
    gcmSetFlip(ctx, idx);
    rsxFlushBuffer(ctx);
    gcmSetWaitFlip(ctx);
}

static void wait_flip(void)
{
    while (gcmGetFlipStatus() != 0)
        usleep(200);
}

static void video_init(void)
{
    void *host = memalign(1024 * 1024, 1024 * 1024);
    rsxInit(&ctx, 0x10000, 1024 * 1024, host);

    videoState st; videoResolution res; videoConfiguration vc;
    videoGetState(0, 0, &st);
    videoGetResolution(st.displayMode.resolution, &res);

    memset(&vc, 0, sizeof vc);
    vc.resolution = st.displayMode.resolution;
    vc.format = VIDEO_BUFFER_FORMAT_XRGB;
    vc.pitch = res.width * 4;
    videoConfigure(0, &vc, NULL, 0);

    gcmSetFlipMode(GCM_FLIP_VSYNC);
    make_buf(&bufs[0], res.width, res.height, 0);
    make_buf(&bufs[1], res.width, res.height, 1);
    present(shown_idx);
    wait_flip();
}

static inline void plot(Buf *b, int x, int y, uint32_t c)
{
    if ((unsigned)x < (unsigned)b->w && (unsigned)y < (unsigned)b->h)
        b->ptr[y * b->w + x] = c;
}

static void line(Buf *b, int x0, int y0, int x1, int y1, uint32_t c)
{
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        plot(b, x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void osint_render(void)
{
    Buf *b = &bufs[draw_idx];
    memset(b->ptr, 0, b->w * b->h * 4);

    int h = b->h;
    int w = (int)((long)h * ALG_MAX_X / ALG_MAX_Y);
    int ox = (b->w - w) / 2;

    for (long i = 0; i < vector_draw_cnt; i++) {
        vector_t *v = &vectors_draw[i];
        if (v->color >= VECTREX_COLORS)
            continue;
        int c = v->color * 2;
        if (c > 255) c = 255;
        uint32_t px = ((uint32_t)c << 16) | ((uint32_t)c << 8) | (uint32_t)c;

        line(b,
             ox + (int)(v->x0 * w / ALG_MAX_X), (int)(v->y0 * h / ALG_MAX_Y),
             ox + (int)(v->x1 * w / ALG_MAX_X), (int)(v->y1 * h / ALG_MAX_Y),
             px);
    }
    dirty = 1;
}

static void poll_input(void)
{
    padInfo info; padData d;
    ioPadGetInfo(&info);
    if (!info.status[0]) return;
    ioPadGetData(0, &d);

    alg_jch0 = d.ANA_L_H; 
    alg_jch1 = 0xff - d.ANA_L_V;    
    if (d.BTN_LEFT)  alg_jch0 = 0x00;
    if (d.BTN_RIGHT) alg_jch0 = 0xff;
    if (d.BTN_UP)    alg_jch1 = 0xff;
    if (d.BTN_DOWN)  alg_jch1 = 0x00;

    snd_regs[14] |= 0x0f; 
    if (d.BTN_CROSS)    snd_regs[14] &= ~0x01;
    if (d.BTN_CIRCLE)   snd_regs[14] &= ~0x02;
    if (d.BTN_SQUARE)   snd_regs[14] &= ~0x04;
    if (d.BTN_TRIANGLE) snd_regs[14] &= ~0x08;
}


static int load_file(const char *path, void *dst, size_t max)
{
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    size_t n = fread(dst, 1, max, f);
    fclose(f);
    return (int)n;
}

static void sysutil_cb(u64 status, u64 param, void *usrdata)
{
    (void)param; (void)usrdata;
    if (status == SYSUTIL_EXIT_GAME)
        running = 0;
}

int main(int argc, char **argv)
{
    (void)argc; (void)argv;

    sysUtilRegisterCallback(0, sysutil_cb, NULL);
    ioPadInit(7);
    video_init();

    if (load_file(USRDIR "rom.dat", rom, sizeof rom) != (int)sizeof rom) { //rom is Minestorm, thats the bios REQUIRED
        while (running) { sysUtilCheckCallback(); usleep(16000); }
        return 1;
    }

    memset(cart, 0, sizeof cart);
    load_file(USRDIR "cart.bin", cart, sizeof cart); //rename any rom to cart.bin

    vecx_reset();

    while (running) {
        sysUtilCheckCallback();
        poll_input();

        vecx_emu(VECTREX_MHZ / 60);    

        if (dirty) { 
            shown_idx = draw_idx;
            draw_idx ^= 1;
            dirty = 0;
        }
        present(shown_idx);             //flip
        wait_flip();
    }

    return 0;
}