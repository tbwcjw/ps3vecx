#include <stdint.h>
#include <string.h>

#include <ppu-types.h>
#include <audio/audio.h>
#include <sys/event_queue.h>
#include <sys/thread.h>

#include "debug.h"

#include "e8910.h"
#include "vecx.h"

/* AY tone counters run at clock/8; e8910 advances four counter ticks per sample. */
#define PSG_RATE 48000
#define PSG_MAX_SAMPLES 4096

#define OUT_RATE    48000.0
#define NUM_BLOCKS  AUDIO_BLOCK_8
#define BLOCK_SAMPS 256
#define BLOCK_FLTS  (BLOCK_SAMPS * 2)

static float gain = 1.0f;

static u32 port_num;
static float *port_data;

static sys_event_queue_t snd_queue;
static sys_ipc_key_t snd_key;
static sys_ppu_thread_t snd_thread;

static volatile int snd_running;
static volatile int snd_paused;

static int audio_initialized;
static int psg_initialized;
static int port_open;
static int queue_created;
static int notify_registered;
static int thread_created;
static int port_started;

static int16_t fifo[2048];
static int fcnt;
static double phase;

void audio_set_gain(float g)
{
    if (g < 0.0f)
        g = 0.0f;

    if (g > 2.0f)
        g = 2.0f;

    gain = g;
}

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
        fcnt = 0;
        phase = 0.0;
        return;
    }

    const double step = (double)PSG_RATE / OUT_RATE;
    int need = (int)(phase + step * BLOCK_SAMPS) + 2;

    if (need > (int)(sizeof(fifo) / sizeof(fifo[0]))) {
        memset(out, 0, BLOCK_FLTS * sizeof(float));
        fcnt = 0;
        phase = 0.0;
        return;
    }

    if (fcnt < need) {
        e8910_callback(NULL, fifo + fcnt, need - fcnt);
        fcnt = need;
    }

    for (int i = 0; i < BLOCK_SAMPS; i++) {
        int k = (int)phase;
        double fr = phase - k;

        float s = (float)(
            (1.0 - fr) * fifo[k] +
            fr * fifo[k + 1]
        ) / 32768.0f;

        s *= gain;

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
    unsigned long long empty_receives = 0;
    int notification_logged = 0;

    while (snd_running) {
        int rc = sysEventQueueReceive(snd_queue, &ev, 20 * 1000);
        if (rc != 0) {
            if (snd_running && ++empty_receives == 50)
                netDebug("audio notifications not received; last status: %d", rc);
            continue;
        }

        if (!snd_running)
            break;

        empty_receives = 0;
        if (!notification_logged) {
            netDebug("audio notification received");
            notification_logged = 1;
        }

        int blk = (int)(k % NUM_BLOCKS); //ring buffer begin at 0

        fill_block(port_data + blk * BLOCK_FLTS);

        k++;
    }

    sysThreadExit(0);
}

int audio_cleanup(void)
{
    snd_running = 0;

    if (port_started) {
        int rc = audioPortStop(port_num);
        if (rc == 0)
            port_started = 0;
        else
            netDebug("audioPortStop failed: %d", rc);
    }

    if (thread_created) {
        u64 ret;
        int rc = sysThreadJoin(snd_thread, &ret);
        if (rc != 0) {
            netDebug("audio worker join failed: %d", rc);
            return -1;
        }
        thread_created = 0;
    }

    if (notify_registered) {
        int rc = audioRemoveNotifyEventQueue(snd_key);
        if (rc != 0) {
            netDebug("audioRemoveNotifyEventQueue failed: %d", rc);
            return -1;
        }
        notify_registered = 0;
    }

    if (port_open) {
        int rc = audioPortClose(port_num);
        if (rc != 0) {
            netDebug("audioPortClose failed: %d", rc);
            return -1;
        }
        port_open = 0;
        port_started = 0;
    }

    if (queue_created) {
        int rc = sysEventQueueDestroy(snd_queue, 0);
        if (rc != 0) {
            netDebug("audio event queue destroy failed: %d", rc);
            return -1;
        }
        queue_created = 0;
    }

    port_data = NULL;

    if (audio_initialized) {
        int rc = audioQuit();
        if (rc != 0) {
            netDebug("audioQuit failed: %d", rc);
            return -1;
        }
        audio_initialized = 0;
    }

    if (psg_initialized) {
        e8910_done_sound();
        psg_initialized = 0;
    }

    snd_paused = 0;
    fcnt = 0;
    phase = 0.0;
    return 0;
}

int audio_init(void)
{
    if (psg_initialized || audio_initialized || port_open || queue_created || notify_registered || thread_created || port_started) {
        return -1;
    }

    int rc;
    fcnt = 0;
    phase = 0.0;
    snd_paused = 0;

    e8910_init_sound();
    psg_initialized = 1;
    /* Restore the reset register values before the worker starts consuming samples. */
    for (int reg = 0; reg < 16; reg++)
        e8910_write(reg, snd_regs[reg]);

    rc = audioInit();
    if (rc != 0) {
        netDebug("audioInit failed: %d", rc);
        goto fail;
    }

    audio_initialized = 1;

    audioPortParam params;
    audioPortConfig config;

    memset(&params, 0, sizeof params);

    params.numChannels = AUDIO_PORT_2CH;
    params.numBlocks   = NUM_BLOCKS;
    params.attrib      = 0;
    params.level       = 0.5f;

    rc = audioPortOpen(&params, &port_num);
    if (rc != 0) {
        netDebug("audioPortOpen failed: %d", rc);
        goto fail;
    }

    port_open = 1;

    rc = audioGetPortConfig(port_num, &config);
    if (rc != 0) {
        netDebug("audioGetPortConfig failed: %d", rc);
        goto fail;
    }

    port_data = (float *)(uintptr_t)config.audioDataStart;

    if (!port_data) {
        netDebug("audio port returned a null buffer");
        goto fail;
    }

    memset(port_data, 0, NUM_BLOCKS * BLOCK_FLTS * sizeof(float));

    rc = audioCreateNotifyEventQueue(&snd_queue, &snd_key);
    if (rc != 0) {
        netDebug("audioCreateNotifyEventQueue failed: %d", rc);
        goto fail;
    }

    queue_created = 1;

    rc = audioSetNotifyEventQueue(snd_key);
    if (rc != 0) {
        netDebug("audioSetNotifyEventQueue failed: %d", rc);
        goto fail;
    }

    notify_registered = 1;

    sysEventQueueDrain(snd_queue);

    snd_running = 1;

    rc = sysThreadCreate(&snd_thread, audio_thread, NULL, 0x300, 0x6000, THREAD_JOINABLE, "vecx_audio");
    if (rc != 0) {
        netDebug("audio worker creation failed: %d", rc);
        goto fail;
    }

    thread_created = 1;

    rc = audioPortStart(port_num);
    if (rc != 0) {
        netDebug("audioPortStart failed: %d", rc);
        goto fail;
    }

    port_started = 1;

    netDebug("audio initialized");
    return 0;

fail:
    audio_cleanup();
    return -1;
}