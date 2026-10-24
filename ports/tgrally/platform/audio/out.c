/* out.c: the game's sound to the host's output.
 *
 * The music thread (src/audio/music.c), woken every 10 ms, asks how much to
 * mix (tgr_audio_want), mixes it into its ring and hands the stretch over
 * (tgr_audio_submit): 16-bit stereo, big-endian as the game writes it, at
 * the rate it set.  It goes into a ring the host's audio callback drains,
 * resampled to the device's rate; the music thread keeps LEAD_MS of sound
 * there.  Without a device (headless, or a check) the sound plays out on
 * the game's clock instead, read 0x1000 bytes behind the mixing as the
 * cartridge's mixer ran, so a run mixes the same stretches every time.
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
#define LEAD_MS 60                  /* kept in the ring: the music thread's 10 ms wake-ups and the device's own buffer */
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


static void cb(float *lr, int frames, void *user)
{
    int i;
    (void)user;
    host_mutex_lock(s_m);
    sinc_init((int)(s_step * s_devrate + 0.5));
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
        s_pos += s_step;
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
/* TGR_CHECK_NOSFX=1, in check mode only: the effects are left out, so a check
 * can compare the music alone */
int tgr_audio_check_nosfx(void)
{
    extern int tgr_check;
    static int on = -1;
    if (on < 0)
        on = tgr_check && getenv("TGR_CHECK_NOSFX") && atoi(getenv("TGR_CHECK_NOSFX"));
    return on;
}

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

/* ---- the game's side ------------------------------------------------------- */
static uint32_t s_rate = 22050;     /* the game's rate */
static int s_started;
static uint64_t s_t0;               /* the game's clock when the sound started (no device) */
static uint64_t s_mixed;            /* bytes handed over since the start (no device) */

int32_t tgr_audio_rate(uint32_t f)
{
    s_rate = f ? f : 1;
    s_step = (double)s_rate / s_devrate;
    return (int32_t)s_rate;
}

void tgr_audio_start(void)
{
    s_t0 = tgr_count();
    s_mixed = 0;
    s_started = 1;
}

/* No device: how far the sound has played on the game's clock, in bytes,
 * as the console's audio interface read it: 0x4000-byte stretches back to
 * back, each 0x4000 * 46875000 / (rate * 4) counts long, its read position
 * in steps of 8 bytes. */
static uint64_t played(void)
{
    uint64_t span = 0x4000ull * 46875000ull / ((uint64_t)s_rate * 4), e = tgr_count() - s_t0, k, in;
    k = e / span;
    in = (e - k * span) * s_rate * 4 / 46875000ull;
    if (in >= 0x4000) {
        k++;
        in = 0;
    }
    return k * 0x4000 + 0x4000 - ((0x4000 - in) & ~7ull);
}

uint32_t tgr_audio_want(void)
{
    uint64_t want;
    if (!s_started)
        return 0;
    if (s_open) {
        uint32_t have, lead = s_rate * LEAD_MS / 1000;
        host_mutex_lock(s_m);
        have = s_w - s_r;
        host_mutex_unlock(s_m);
        want = have < lead ? (uint64_t)(lead - have) * 4 : 0;
    } else {
        uint64_t to = played() + 0x1000;            /* mixed 0x1000 bytes ahead of the reading */
        want = to > s_mixed ? to - s_mixed : 0;
        if (want > 0x4000) {                        /* the host stalled: what fell behind is skipped */
            s_mixed = to - 0x4000;
            want = 0x4000;
        }
    }
    want &= ~7ull;
    return (uint32_t)(want > 0x4000 ? 0x4000 : want);
}

void tgr_audio_submit(const void *ring, uint32_t size, uint32_t pos, uint32_t n)
{
    const uint8_t *b = (const uint8_t *)ring;
    uint32_t i;
    s_mixed += n;
    for (i = 0; i < n; i += 4) {
        uint32_t k = (pos + i) % size;
        wav_dump(b + k, 1);
    }
    if (!s_open)
        return;
    host_mutex_lock(s_m);
    for (i = 0; i < n; i += 4) {
        const uint8_t *f = b + (pos + i) % size;
        uint32_t k;
        if (s_w - s_r >= RING - TAPS)    /* (the sinc reads 15 samples behind the read point) */
            break;
        k = s_w & (RING - 1);
        s_ring[k][0] = (int16_t)(f[0] << 8 | f[1]) / 32768.0f;
        s_ring[k][1] = (int16_t)(f[2] << 8 | f[3]) / 32768.0f;
        s_w++;
    }
    host_mutex_unlock(s_m);
}
