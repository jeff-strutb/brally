/* out.c: the audio interface's buffers to the host's output.
 *
 * The audio interface (os/io.c) hands over what it has played of the
 * buffers the game queued: 16-bit stereo, big-endian as the N64's RAM held
 * it, at the rate the game set.  It goes into a ring the host's audio
 * callback drains, resampled to the device's rate.  The game paces itself on the modelled interface (os/io.c), never on
 * this ring, so audio cannot change what the game does. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "host.h"
#include "plat.h"

#define RING (1 << 16)              /* stereo frames */
#define LEAD_MS 60                  /* the host's cushion against the wall clock's jitter */
static float s_ring[RING][2];
static volatile uint32_t s_w, s_r;
static double s_pos, s_step = 1.0;
static int s_open, s_devrate = 48000;
static host_mutex *s_m;

/* the device's clock is not the game's: over a long session the buffered
 * audio would creep.  Its average over a few seconds is held at what it
 * settled to once the game started, by playing up to 0.2% faster or slower
 * (inaudible), so the latency stays the N64's own plus LEAD_MS. */
static double s_avg = -1, s_target = -1, s_trim = 1.0;
static uint64_t s_played;

static void trim(void)
{
    double have = (double)(s_w - s_r), err;
    s_avg = s_avg < 0 ? have : s_avg + (have - s_avg) * 0.002;
    if (s_target < 0) {
        if (s_played > (uint64_t)s_devrate * 5)       /* settled: the game has been queueing for 5 s */
            s_target = s_avg;
        return;
    }
    err = (s_avg - s_target) / (s_target > 1 ? s_target : 1);
    s_trim = 1.0 + (err > 1 ? 1 : err < -1 ? -1 : err) * 0.002;
}

static void cb(float *lr, int frames, void *user)
{
    int i;
    (void)user;
    host_mutex_lock(s_m);
    s_played += (uint64_t)frames;
    trim();
    for (i = 0; i < frames; i++) {
        uint32_t have = s_w - s_r;
        if (have < 2) {
            lr[2 * i] = lr[2 * i + 1] = 0.0f;
            continue;
        }
        {
            uint32_t k = s_r & (RING - 1);
            lr[2 * i] = s_ring[k][0];
            lr[2 * i + 1] = s_ring[k][1];
        }
        s_pos += s_step * s_trim;
        while (s_pos >= 1.0 && s_w != s_r) {
            s_pos -= 1.0;
            s_r++;
        }
    }
    host_mutex_unlock(s_m);
}

void tgr_audio_init(void)
{
    s_m = host_mutex_new();
    if (g_tgr.headless || getenv("TGR_NOSOUND"))
        return;
    s_open = host_audio_open(s_devrate, cb, NULL) != 0;
}

/* how much the host has yet to play, in milliseconds (TGR_STATS) */
int tgr_audio_buffered_ms(void)
{
    return s_open ? (int)((uint64_t)(s_w - s_r) * 1000 / (uint32_t)(s_devrate * (s_step > 0 ? s_step : 1))) : -1;
}

/* TGR_WAVDUMP=FILE@F0: what the game plays from retrace F0 on, raw 16-bit stereo
 * (little-endian) at its rate, for tools */
static void wav_dump(const uint8_t *b, int frames)
{
    static FILE *f;
    static int init;
    static unsigned from;
    int i;
    if (!init) {
        char path[512];
        init = 1;
        if (getenv("TGR_WAVDUMP") && sscanf(getenv("TGR_WAVDUMP"), "%511[^@]@%u", path, &from) == 2)
            f = fopen(path, "wb");
    }
    if (!f || tgr_frame() < from)
        return;
    for (i = 0; i < 2 * frames; i++) {
        int16_t v = (int16_t)(b[2 * i] << 8 | b[2 * i + 1]);
        fwrite(&v, 2, 1, f);
    }
}

void tgr_audio_buffer(const int16_t *lr, int frames, int rate)
{
    const uint8_t *b = (const uint8_t *)lr;
    int i;
    wav_dump(b, frames);
    if (!s_open || rate <= 0)
        return;
    host_mutex_lock(s_m);
    s_step = (double)rate / s_devrate;
    if (s_w - s_r < 2) {                /* starting, or the game was paused: lead with silence */
        uint32_t lead = (uint32_t)rate * LEAD_MS / 1000;
        for (i = 0; i < (int)lead; i++, s_w++)
            s_ring[s_w & (RING - 1)][0] = s_ring[s_w & (RING - 1)][1] = 0.0f;
    }
    for (i = 0; i < frames; i++) {
        uint32_t k;
        if (s_w - s_r >= RING - 1)
            break;
        k = s_w & (RING - 1);
        s_ring[k][0] = (int16_t)(b[4 * i] << 8 | b[4 * i + 1]) / 32768.0f;
        s_ring[k][1] = (int16_t)(b[4 * i + 2] << 8 | b[4 * i + 3]) / 32768.0f;
        s_w++;
    }
    host_mutex_unlock(s_m);
}
