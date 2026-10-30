#include <stdint.h>
#include <string.h>

#include <ppu-types.h>
#include <audio/audio.h>
#include <sys/event_queue.h>
#include <sys/thread.h>

#include "e8910.h"

#define PSG_RATE 22050
#define PSG_MAX_SAMPLES 1024

static void psg_pull(int16_t *dst, int n)
{
    Uint8 samples[PSG_MAX_SAMPLES];
    e8910_callback(NULL, samples, n);

    for (int i = 0; i < n; i++) {
        dst[i] = (int16_t)(((int)samples[i] - 8) * 4096);
    }
}

static void psg_init(void)
{
    e8910_init_sound();
}

static void psg_done(void)
{
    e8910_done_sound();
}

#define OUT_RATE    48000.0
#define NUM_BLOCKS  AUDIO_BLOCK_8
#define BLOCK_SAMPS AUDIO_BLOCK_SAMPLES                 /* 256 */
#define BLOCK_FLTS  (BLOCK_SAMPS * 2)                   /* stereo */
#define GAIN        0.5f

static u32 port_num;
static float *port_data;
static sys_event_queue_t snd_queue;
static sys_ipc_key_t snd_key;
static sys_ppu_thread_t snd_thread;
static volatile int snd_running = 1;
static volatile int snd_paused = 0;

static int snd_ok;

static int16_t fifo[2048];
static int fcnt;
static double phase;

void audio_pause(void)
{
    snd_paused = 1;
}

void audio_unpause(void)
{
    snd_paused = 0;
}

static void fill_block(float *out)
{
    if (snd_paused) {
        memset(out, 0, BLOCK_FLTS * sizeof(float));
        fcnt  = 0;      /* drop stale samples so resume doesn't play old audio */
        phase = 0.0;
        return;
    }

    const double step = (double)PSG_RATE / OUT_RATE;
    int need = (int)(phase + step * BLOCK_SAMPS) + 2;

    if (fcnt < need) {
        psg_pull(fifo + fcnt, need - fcnt);
        fcnt = need;
    }
    for (int i = 0; i < BLOCK_SAMPS; i++) {
        int k = (int)phase;
        double fr = phase - k;
        float s = (float)((1.0 - fr) * fifo[k] + fr * fifo[k + 1]) / 32768.0f;
        s *= GAIN;
        out[2 * i]     = s;
        out[2 * i + 1] = s;
        phase += step;
    }
    int used = (int)phase;
    phase -= used;
    memmove(fifo, fifo + used, (fcnt - used) * sizeof(int16_t));
    fcnt -= used;
}

static void audio_thread(void *arg)
{
    (void)arg;
    sys_event_t ev;
    unsigned long long k = 0;

    while (snd_running) {
        if (sysEventQueueReceive(snd_queue, &ev, 20 * 1000) != 0)
            continue;

        int blk = (int)((k + NUM_BLOCKS / 2) % NUM_BLOCKS);
        fill_block(port_data + blk * BLOCK_FLTS);
        k++;
    }
    sysThreadExit(0);
}

int audio_init(void)
{
    fcnt = 0;
    phase = 0.0;
    psg_init();

    if (audioInit() != 0) 
        return -1;
        

    audioPortParam params;
    audioPortConfig config;
    memset(&params, 0, sizeof params);
    params.numChannels = AUDIO_PORT_2CH;
    params.numBlocks   = NUM_BLOCKS;
    params.attrib      = 0;
    params.level       = 0.5f;

    if (audioPortOpen(&params, &port_num) != 0) return -2;
    if (audioGetPortConfig(port_num, &config) != 0) return -3;
    port_data = (float *)(uintptr_t)config.audioDataStart;

    //shhhh
    memset(port_data, 0, NUM_BLOCKS * BLOCK_FLTS * sizeof(float));

    if (audioCreateNotifyEventQueue(&snd_queue, &snd_key) != 0) return -4;
    if (audioSetNotifyEventQueue(snd_key) != 0) return -5;
    sysEventQueueDrain(snd_queue);

    snd_running = 1;
    sysThreadCreate(&snd_thread, audio_thread, NULL, 1500, 0x4000,
                    THREAD_JOINABLE, "vecx_audio");

    if (audioPortStart(port_num) != 0) return -6;
    snd_ok = 1;
    return 0;
}

void audio_shutdown(void)
{
    if (!snd_ok)
        return;

    u64 ret;
    snd_running = 0;

    sysThreadJoin(snd_thread, &ret);

    audioPortStop(port_num);
    audioRemoveNotifyEventQueue(snd_key);
    audioPortClose(port_num);

    sysEventQueueDestroy(snd_queue, 0);
    audioQuit();
    psg_done();

    snd_ok = 0;
}