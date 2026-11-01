#define STB_IMAGE_IMPLEMENTATION

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <math.h>

#include <ppu-types.h>
#include <sys/process.h>
#include <sys/systime.h>
#include <sysutil/sysutil.h>
#include <sysutil/video.h>
#include <rsx/rsx.h>
#include <io/pad.h>

#include "osint.h" 
#include "vecx.h"
#include "audio.h"
#include "debug.h"

SYS_PROCESS_PARAM(1001, 0x100000);

#define USRDIR "/dev_hdd0/game/PS3VECX01/USRDIR/"
#define EMUSELF "VECXEMU.SELF"
#define MENUSELF USRDIR "VECXMENU.SELF"

#define CARTS "carts/"
#define DEFAULTCART USRDIR CARTS "boot.vec"
#define ROM "rom/"
#define OVERLAYS "overlays/"
#define CONFIG "config.txt"

typedef struct { 
    int w, h; 
    uint32_t *ptr; 
    uint32_t offset; 
} Buf;

static gcmContextData *ctx;
static Buf bufs[2];
static int draw_idx = 0; 
static int shown_idx = 1;
static int dirty = 0;

#define EMU_FRAME_US   20000u
#define EMU_MAX_BACKLOG (3u * EMU_FRAME_US)
static volatile int speed_multiplier = 20;

static volatile int running = 1;
static volatile int paused = 0;

static uint32_t *overlay = NULL;

typedef struct {
    char *rom;
    char *cart;
    char *overlay;
    int alpha;
    int thickness;
    int rotation;
    int sync;
    float gain;
} Config;

Config config = {0};

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


static void set_gcm_sync(void)
{
    if(config.sync != 0)
    {
        netDebug("GCM SYNC ON");
        gcmSetFlipMode(GCM_FLIP_VSYNC);
    }
    
}

static void video_init(void)
{
    void *host = memalign(1024 * 1024, 1024 * 1024);
    rsxInit(&ctx, 0x10000, 1024 * 1024, host);

    videoState st; 
    videoResolution res; 
    videoConfiguration vc;

    videoGetState(0, 0, &st);
    videoGetResolution(st.displayMode.resolution, &res);
    set_gcm_sync();

    memset(&vc, 0, sizeof vc);
    vc.resolution = st.displayMode.resolution;
    vc.format = VIDEO_BUFFER_FORMAT_XRGB;
    vc.pitch = res.width * 4;
    videoConfigure(0, &vc, NULL, 0);
    
    
    make_buf(&bufs[0], res.width, res.height, 0);
    make_buf(&bufs[1], res.width, res.height, 1);
    present(shown_idx);
}



static inline void plot(Buf *b, int x, int y, uint32_t c)
{
    if ((unsigned)x >= (unsigned)b->w || (unsigned)y >= (unsigned)b->h)
        return;

    if (!overlay) {
        b->ptr[(size_t)y * b->w + (unsigned)x] = c;
        return;
    }

    size_t index = (size_t)y * b->w + (unsigned)x;
    uint32_t over = overlay[index];
    uint32_t product = (c & 0xffu) * (over >> 24) + 127u;
    uint32_t line_value = (product + 1u + (product >> 8)) >> 8;

    b->ptr[index] = (over & 0x00ffffffu) + line_value * 0x00010101u;
}

static void line(Buf *b, int x0, int y0, int x1, int y1, int thickness, uint32_t c)
{
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    if (thickness > 1) {
        if (thickness > 64)
            thickness = 64;

        struct RowCoverage {
            int y;
            int min_x;
            int max_x;
            int valid;
        } rows[64];

        memset(rows, 0, sizeof(rows));

        const int lo = -((thickness - 1) / 2);   /* top offset of brush    */
        const int hi = thickness / 2;            /* bottom offset of brush */
        const int n  = hi - lo + 1;              /* == thickness           */

        /* Precompute the half-width of the disc on each row of the brush.
           Radius is thickness/2, so the brush is a circle, not a bar. */
        int halfw[64];
        const double rad2 = (thickness * thickness) / 4.0;
        for (int j = lo; j <= hi; j++) {
            double d = rad2 - (double)j * j;
            halfw[j - lo] = d > 0 ? (int)sqrt(d) : 0;
        }

        for (;;) {
            for (int j = lo; j <= hi; j++) {
                const int py = y0 + j;
                const int hw = halfw[j - lo];
                const int cl = x0 - hw;   /* leftmost column on this row  */
                const int ch = x0 + hw;   /* rightmost column on this row */

                const int slot = ((py % n) + n) % n;
                struct RowCoverage *row = &rows[slot];

                if (!row->valid || row->y != py) {
                    row->valid = 1;
                    row->y = py;
                    row->min_x = cl;
                    row->max_x = ch;
                    for (int px = cl; px <= ch; px++)
                        plot(b, px, py, c);
                } else {
                    if (ch > row->max_x) {
                        for (int px = row->max_x + 1; px <= ch; px++)
                            plot(b, px, py, c);
                        row->max_x = ch;
                    }
                    if (cl < row->min_x) {
                        for (int px = cl; px < row->min_x; px++)
                            plot(b, px, py, c);
                        row->min_x = cl;
                    }
                }
            }

            if (x0 == x1 && y0 == y1)
                break;

            int e2 = 2 * err;
            if (e2 >= dy) { err += dy; x0 += sx; }
            if (e2 <= dx) { err += dx; y0 += sy; }
        }
        return;
    }

    /* thickness <= 1: single pixel path (unchanged) */
    for (;;) {
        plot(b, x0, y0, c);

        if (x0 == x1 && y0 == y1)
            break;

        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

static void viewport(const Buf *b, int rotated, int *w, int *h, int *ox, int *oy)
{
    *w = b->w;
    *h = (int)((long)b->w * (rotated == 0 ? ALG_MAX_Y : ALG_MAX_X) /
               (rotated == 0 ? ALG_MAX_X : ALG_MAX_Y));

    if (*h > b->h) {
        *h = b->h;
        *w = (int)((long)b->h * (rotated == 0 ? ALG_MAX_X : ALG_MAX_Y) /
                   (rotated == 0 ? ALG_MAX_Y : ALG_MAX_X));
    }

    *ox = (b->w - *w) / 2;
    *oy = (b->h - *h) / 2;
}

void osint_render(void)
{
    Buf *b = &bufs[draw_idx];

    if (overlay)
        memcpy(b->ptr, overlay, (size_t)b->w * b->h * 4);
    else
        memset(b->ptr, 0, (size_t)b->w * b->h * 4);
        
    int w, h, ox, oy;
    viewport(b, config.rotation, &w, &h, &ox, &oy);

    for (long i = 0; i < vector_draw_cnt; i++) {
        vector_t *v = &vectors_draw[i];
        if (v->color >= VECTREX_COLORS)
            continue;

        int c = v->color * 2;
        if (c > 255) c = 255;
        uint32_t px = ((uint32_t)c << 16) | ((uint32_t)c << 8) | (uint32_t)c;

        if (config.rotation == 0) {
            line(b,
                 ox + (int)((long)v->x0 * w / ALG_MAX_X),
                 oy + (int)((long)v->y0 * h / ALG_MAX_Y),
                 ox + (int)((long)v->x1 * w / ALG_MAX_X),
                 oy + (int)((long)v->y1 * h / ALG_MAX_Y),
                 config.thickness, px);
        } else if (config.rotation == 1) {                                 //cw
            line(b,
                 ox + w - 1 - (int)((long)v->y0 * w / ALG_MAX_Y),
                 oy + (int)((long)v->x0 * h / ALG_MAX_X),
                 ox + w - 1 - (int)((long)v->y1 * w / ALG_MAX_Y),
                 oy + (int)((long)v->x1 * h / ALG_MAX_X),
                 config.thickness, px);
        } else if (config.rotation == -1) {                                //ccw
            line(b,
                 ox + (int)((long)v->y0 * w / ALG_MAX_Y),
                 oy + h - 1 - (int)((long)v->x0 * h / ALG_MAX_X),
                 ox + (int)((long)v->y1 * w / ALG_MAX_Y),
                 oy + h - 1 - (int)((long)v->x1 * h / ALG_MAX_X),
                 config.thickness, px);
        }
    }
    dirty = 1;
}

static void poll_input(void)
{
    padInfo info; 
    padData d;
    
    ioPadGetInfo(&info);
    if (!info.status[0]) 
        return;
    
    memset(&d, 0, sizeof d);
    ioPadGetData(0, &d);

    if(d.len == 0)
        return;

    alg_jch0 = d.ANA_L_H; 
    alg_jch1 = 0xff - d.ANA_L_V;    
    if (d.BTN_LEFT)  alg_jch0 = 0x00;
    if (d.BTN_RIGHT) alg_jch0 = 0xff;
    if (d.BTN_UP)    alg_jch1 = 0xff;
    if (d.BTN_DOWN)  alg_jch1 = 0x00;
    if (d.BTN_SELECT && d.BTN_START) running = 0;           //exit
    if (d.BTN_SELECT) vecx_reset();                         //reset
    if (d.BTN_START)  paused = paused == 1 ? 0 : 1;         //pause
    if (d.BTN_L1)                                           //fast forward
        speed_multiplier = (speed_multiplier == 20) 
            ? 40 : 20;
            
    snd_regs[14] |= 0x0f; 
    if (d.BTN_CROSS)    snd_regs[14] &= ~0x01;
    if (d.BTN_CIRCLE)   snd_regs[14] &= ~0x02;
    if (d.BTN_SQUARE)   snd_regs[14] &= ~0x04;
    if (d.BTN_TRIANGLE) snd_regs[14] &= ~0x08;
}

int load_file(const char *path, void *dst, size_t max)
{
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    size_t n = fread(dst, 1, max, f);
    fclose(f);
    return (int)n;
}

// todo: overlays need translucency
int load_overlay(const char *filename)
{
    int iw, ih, n;
    uint8_t *img = stbi_load(filename, &iw, &ih, &n, 4);
    if (!img) 
        return -1;

    Buf *b = &bufs[0];
    int w, h, ox, oy;
    viewport(b, config.rotation, &w, &h, &ox, &oy);

    size_t pixel_count = (size_t)b->w * b->h;
    overlay = (uint32_t *)malloc(pixel_count * sizeof(*overlay));
    if (!overlay) 
    { 
        stbi_image_free(img); 
        return -2; 
    }
    for (size_t i = 0; i < pixel_count; i++)
        overlay[i] = 0xff000000;  /* transparent black borders */

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int sx, sy;
            if (config.rotation == 0) {        
                sx = x * iw / w;
                sy = y * ih / h;
            } else if (config.rotation == 1) {         //cw
                sx = y * iw / h;
                sy = (w - 1 - x) * ih / w;
            } else if (config.rotation == -1) {        //ccw
                sx = (h - 1 - y) * iw / h;
                sy = x * ih / w;
            } else {
                continue;
            }

            const uint8_t *p = img + ((size_t)sy * iw + sx) * 4;
            uint32_t a = p[3] > config.alpha ? p[3] - config.alpha : 0;
            uint32_t r = p[0] * a / 255;
            uint32_t g = p[1] * a / 255;
            uint32_t bl = p[2] * a / 255;

            overlay[(oy + y) * b->w + ox + x] =
                ((255 - a) << 24) | (r << 16) | (g << 8) | bl;
        }
    }
    stbi_image_free(img);
    return 0;
}

static void sysutil_cb(u64 status, u64 param, void *usrdata)
{
    (void)param; 
    (void)usrdata;

    if (status == SYSUTIL_EXIT_GAME)
    {
        running = 0;
    }

    if(status == SYSUTIL_MENU_OPEN)
    {
        paused = 1;
        audio_pause();
        gcmSetFlipMode(-1);
    } 

    if(status == SYSUTIL_MENU_CLOSE)
    {
        paused = 0;
        audio_unpause();
        set_gcm_sync();
    }
        
}

int sys_ring_buzzer(int beeps) {
    if (beeps < 1) beeps = 1;
    if (beeps > 3) beeps = 3;

    static const uint64_t args[3][3] = {
        {0x1004, 0x4, 0x6},     //single beep
        {0x1004, 0x7, 0x36},    //two beeps
        {0x1004, 0xa, 0x1b6}    //three beeps
    };

    lv2syscall3(392, args[beeps - 1][0], args[beeps - 1][1], args[beeps - 1][2]);
    return 0;
}

static int set_dynamic_string(char** field, const char* value)
{
    char* copy = strdup(value);
    if (copy == NULL)
        return -1;

    free(*field); 
    *field = copy;
    return 0;
}

int parse_config_file(Config* config)
{
    FILE* file = fopen(USRDIR CONFIG, "r");
    if(file == NULL)
    {
        netDebug("FAILED TO OPEN CONFIG %s%s", USRDIR, CONFIG);
        return -1;
    }
    netDebug("READING CONFIG %s%s", USRDIR, CONFIG);

    char line[128+8];
    while(fgets(line, sizeof(line), file) != NULL) {
        line[strcspn(line, "\r\n")] = '\0';
        char* sep = strchr(line, '=');
        if(!sep) continue;

        *sep ='\0';
        char* key = line;
        char* value = sep + 1;

        while(*value == ' ') value++;

        if(strcmp(key, "cart") == 0)                        //cart=
        {
            set_dynamic_string(&config->cart, value);
            netDebug("cart %s", value);
        }
        else if(strcmp(key, "rom") == 0)                    //rom=
        {
            set_dynamic_string(&config->rom, value);
            netDebug("rom %s", value);
        }
        else if(strcmp(key, "overlay") == 0)                //overlay= 
        {
            set_dynamic_string(&config->overlay, value);
            netDebug("overlay %s", value);
        }
        else if(strcmp(key, "alpha") == 0)                  //alpha=
        {
            config->alpha = atoi(value);
            netDebug("alpha %d", atoi(value));
        }
        else if(strcmp(key, "thickness") == 0)             //thickness=
        {
            config->thickness = atoi(value);
            netDebug("thickness %d", atoi(value));
        }
        else if(strcmp(key, "rotation") == 0)               //rotation=
        {
            config->rotation = atoi(value); //todo: 0, 1, and -1 only
            netDebug("rotation %d", atoi(value));
        }
        else if(strcmp(key, "sync") == 0)                   //sync=
        {
            config->sync = atoi(value);
            netDebug("sync %d", atoi(value));
        }
        else if(strcmp(key, "gain") == 0)                  //gain=
        {
            config->gain = strtof(value, NULL);
            netDebug("gain %f", config->gain);
        }

        
    }
    fclose(file);
    return 0;
}

static void wait_flip(void)
{
    while (gcmGetFlipStatus() != 0)
    {
        usleep(200);
    }
        
}

int main(int argc, char **argv)
{
    (void)argc; 
    (void)argv;    

    netInitialize();
    netDebugInit();
    netDebug("Hello");
    sysUtilRegisterCallback(0, sysutil_cb, NULL);
    ioPadInit(7);

    //set config defaults
    config.rom     = strdup("rom.dat");
    config.cart    = strdup("boot.vec");
    config.overlay = strdup("overlay.png");
    config.alpha = 128;
    config.thickness = 2;
    config.rotation = 0;
    config.sync = 1;
    config.gain = 1.0f;


    //load config
    parse_config_file(&config);
    netDebug("Config parsed");

    //load rom
    char romPath[512];
    sprintf(romPath, "%s%s%s", USRDIR, ROM, config.rom);
    if (load_file(romPath, rom, sizeof rom) != (int)sizeof rom) { //rom is Minestorm, thats the bios REQUIRED
        netDebug("Failed to find rom file: %s%s%s", USRDIR, ROM, config.rom);
        //todo: maybe we can do a vsh notify?
        #ifdef PS3LOADX
        sysProcessExitSpawn2("/dev_hdd0/game/PSL145310/RELOAD.SELF", NULL, NULL, NULL, 0, 1001, SYS_PROCESS_SPAWN_STACK_SIZE_1M);
        #endif
        return 0;
    }
    netDebug("ROM: %s%s%s", USRDIR, ROM, config.rom);

    //load our cart.bin
    char cartPath[512];
    sprintf(cartPath, "%s%s%s", USRDIR, CARTS, config.cart);
    memset(cart, 0, sizeof cart);
    
    if(load_file(cartPath, cart, sizeof cart) == -1)
    {
        netDebug("CART %s NOT FOUND", cartPath);

        if(load_file(DEFAULTCART, cart, sizeof cart) == -1)
        {
            netDebug("DEFAULT CART %s NOT FOUND", DEFAULTCART);
        } //not a show stopper, mine storm is in the rom
        
    } else
    {
        netDebug("ROM: %s%s%s", USRDIR, CARTS, config.cart);
    }
    

    //init video before loading the overlay so its framebuffer is availabe
    video_init();
    netDebug("vieo_init");

    //load our overlay
    char overlayPath[512];
    sprintf(overlayPath, "%s%s%s", USRDIR, OVERLAYS, config.overlay);
    int ret = load_overlay(overlayPath);
    if(ret < 0)
    {
        if(ret == -1) netDebug("OVERLAY %s NOT FOUND", overlayPath);
        if(ret == -2) netDebug("OVERLAY FAILED READING");
    } else
    {
        netDebug("OVERLAY: %s%s%s", USRDIR, OVERLAYS, config.overlay);
    }


    int aud = audio_init();
    audio_set_gain(config.gain);
    if(aud < 0)
        return 1;
    netDebug("audio_init");

    //reset emulator
    vecx_reset();
    netDebug("reset");
    
    u64 last       = sysGetSystemTime();
    u64 emu_budget = 0;

    while (running)
    {
        sysUtilCheckCallback();
        poll_input();

        wait_flip();
        u64 now     = sysGetSystemTime();
        u64 elapsed = now - last;
        last = now;

        if (!paused)
        {
            emu_budget += elapsed;
            if (emu_budget > EMU_MAX_BACKLOG)
                emu_budget = EMU_MAX_BACKLOG;

            while (emu_budget >= EMU_FRAME_US)
            {
                vecx_emu((VECTREX_MHZ / 1000) * speed_multiplier);
                emu_budget -= EMU_FRAME_US;
            }
        }
        else
        {
            emu_budget = 0;
        }

        if (dirty) {
            shown_idx = draw_idx;
            draw_idx ^= 1;
            dirty = 0;
        }

        present(shown_idx);
    }
    

    #ifdef PS3LOADX
    sysProcessExitSpawn2(MENUSELF, NULL, NULL, NULL, 0, 1001, SYS_PROCESS_SPAWN_STACK_SIZE_1M);
    #endif

    audio_shutdown();
    //here we will return to the menu with sysProcessExitSpawn2
    return 0;
}