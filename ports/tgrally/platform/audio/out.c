/* out.c: the audio interface's buffers to the host's output.
 *
 * The audio interface (os/io.c) hands over what it has played of the
 * buffers the game queued: 16-bit stereo, big-endian as the N64's RAM held
 * it, at the rate the game set.  It goes into a ring the host's audio
 * callback drains, resampled to the device's rate.  The game paces itself on the modelled interface (os/io.c), never on
 * this ring, so audio cannot change what the game does.
 *
 * The resampling is band-limited, as the N64's own output was (its DAC and
 * the analog filter after it): each output sample is the ring's signal
 * between two of its samples, read through a 32-tap windowed sinc cut at
 * 0.45 of the game's rate.  Taking the nearest sample instead (the game's
 * 22 kHz onto 48 kHz) repeats samples two or three times unevenly, which
 * folds distortion back into the audible band: -22 dB on a 1 kHz tone,
 * -12 dB at 3 kHz, heard as a scratchy sound; through the sinc it is -70
 * and -60 dB.
 *
 * The same sinc can cut lower (tgr_audio_lowpass, TGR_AUDIO_LOWPASS=Hz): the
 * game's mix carries a fizz at 8 to 11 kHz, stronger than all of 4 to 8 kHz,
 * that the console's analog output and a television softened.  Cut at 7 kHz
 * the sinc is flat to 6 kHz, -25 dB at 8 kHz and -77 dB at 9 kHz. */
#include <math.h>
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

#define TAPS 32                     /* the sinc's taps: samples r - 15 .. r + 16 about position r + frac */
#define PHASES 256                  /* the fractions it is tabulated at */
static float s_sinc[PHASES + 1][TAPS];
static volatile int s_lowpass;      /* the cut asked for, Hz (0: 0.45 of the game's rate) */
static int s_sinc_hz = -1, s_sinc_rate;   /* what the table was made for */

void tgr_audio_lowpass(int hz) { s_lowpass = hz > 0 ? hz : 0; }

/* the table for the game's rate and the cut asked for (made again when either changes) */
static void sinc_init(int rate)
{
    const double pi = 3.14159265358979323846;
    double cut = 0.45;
    int p, j;
    if (s_lowpass == s_sinc_hz && rate == s_sinc_rate)
        return;
    s_sinc_hz = s_lowpass;
    s_sinc_rate = rate;
    if (s_lowpass > 0 && rate > 0 && (double)s_lowpass / rate < cut)
        cut = (double)s_lowpass / rate;
    for (p = 0; p <= PHASES; p++) {
        double sum = 0, h[TAPS];
        for (j = 0; j < TAPS; j++) {
            double d = (j - (TAPS / 2 - 1)) - (double)p / PHASES, x = 2 * cut * d;
            double w = 0.42 + 0.5 * cos(pi * d / (TAPS / 2)) + 0.08 * cos(2 * pi * d / (TAPS / 2));   /* Blackman */
            h[j] = 2 * cut * (x == 0 ? 1 : sin(pi * x) / (pi * x)) * w;
            sum += h[j];
        }
        for (j = 0; j < TAPS; j++)
            s_sinc[p][j] = (float)(h[j] / sum);                  /* unity gain at every phase */
    }
}

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
    sinc_init((int)(s_step * s_devrate + 0.5));
    trim();
    for (i = 0; i < frames; i++) {
        uint32_t have = s_w - s_r;
        if (have < TAPS / 2 + 1) {          /* the sinc reaches 16 samples ahead */
            lr[2 * i] = lr[2 * i + 1] = 0.0f;
            continue;
        }
        {
            int ph = (int)(s_pos * PHASES + 0.5);
            const float *h = s_sinc[ph > PHASES ? PHASES : ph];
            uint32_t k = s_r - (TAPS / 2 - 1);
            float l = 0, r = 0;
            int j;
            for (j = 0; j < TAPS; j++, k++) {
                l += h[j] * s_ring[k & (RING - 1)][0];
                r += h[j] * s_ring[k & (RING - 1)][1];
            }
            lr[2 * i] = l;
            lr[2 * i + 1] = r;
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
    if (getenv("TGR_AUDIO_LOWPASS"))
        tgr_audio_lowpass(atoi(getenv("TGR_AUDIO_LOWPASS")));
    sinc_init(22000);
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
    if (s_w - s_r < TAPS) {             /* starting, or the game was paused: lead with silence */
        uint32_t lead = (uint32_t)rate * LEAD_MS / 1000;
        for (i = 0; i < (int)lead; i++, s_w++)
            s_ring[s_w & (RING - 1)][0] = s_ring[s_w & (RING - 1)][1] = 0.0f;
    }
    for (i = 0; i < frames; i++) {
        uint32_t k;
        if (s_w - s_r >= RING - TAPS)    /* (the sinc reads 15 samples behind the read point) */
            break;
        k = s_w & (RING - 1);
        s_ring[k][0] = (int16_t)(b[4 * i] << 8 | b[4 * i + 1]) / 32768.0f;
        s_ring[k][1] = (int16_t)(b[4 * i + 2] << 8 | b[4 * i + 3]) / 32768.0f;
        s_w++;
    }
    host_mutex_unlock(s_m);
}
