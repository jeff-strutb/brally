/* br_snd.c: the game's sound effects (br_snd.h), mixed for the host.
 *
 * Every sample (br_sndbuf.c) is converted to floats once, when it is made,
 * and then driven by the game with its volume, pan and rate, play (one-shot
 * or looping), stop, seek and the playing test. The host's audio callback
 * (audio.c) sums the playing samples, resampled to the output rate by
 * 4-point Hermite interpolation; a volume or pan change ramps over one
 * block so continuous changes do not click.
 *
 * While the host's mixer runs it is the play cursor: a one-shot stops when
 * its last sample has been played. Without one (no audio device, headless)
 * the cursor is the clock, so the game's state moves the same and nothing
 * is heard. */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "plat.h"
#include "host.h"
#include "br_snd.h"

#define OUT_RATE 44100

struct BrSnd {
    uint32_t size, rate, align;    /* bytes, samples a second, bytes a frame */
    int      ch;
    float   *pcm;                  /* the samples as floats, ch interleaved */
    uint32_t frames;
    int      playing, looping, live;
    uint32_t pos;                  /* the cursor when stopped, bytes */
    uint32_t t0;                   /* the clock at play / seek */
    int32_t  vol, pan;
    uint32_t freq;
    double   fpos;                 /* the mixer's cursor, frames */
    float    gl, gr;               /* gains at the end of the last block */
};
typedef struct BrSnd dsbuf;

static host_mutex *s_lock;
static dsbuf *s_live[512];
static int s_nlive;
static int s_mixing;               /* the host mixer runs: it moves the cursors */

static void lock(void)
{
    if (!s_lock)
        s_lock = host_mutex_new();
    host_mutex_lock(s_lock);
}
static void unlock(void) { host_mutex_unlock(s_lock); }

/* ---- the mix --------------------------------------------------------------------- */
static float herm(float s0, float s1, float s2, float s3, float x)
{
    float c1 = 0.5f * (s2 - s0);
    float c2 = s0 - 2.5f * s1 + 2.0f * s2 - 0.5f * s3;
    float c3 = 0.5f * (s3 - s0) + 1.5f * (s1 - s2);
    return ((c3 * x + c2) * x + c1) * x + s1;
}

static float at(const dsbuf *b, int64_t i, int k)
{
    if (i < 0 || i >= (int64_t)b->frames) {
        if (!b->looping || !b->frames)
            return 0;
        i %= (int64_t)b->frames;
        if (i < 0)
            i += b->frames;
    }
    return b->pcm[i * b->ch + k];
}

static float cb_gain(int32_t cb) { return cb <= -10000 ? 0.0f : cb >= 0 ? 1.0f : powf(10.0f, (float)cb / 2000.0f); }

static void unlive(dsbuf *b)
{
    int i;
    if (!b->live)
        return;
    for (i = 0; i < s_nlive; i++)
        if (s_live[i] == b) {
            s_live[i] = s_live[--s_nlive];
            break;
        }
    b->live = 0;
}

int br_snd_live(void) { return s_nlive; }

/* the playing samples, added into lr (audio.c's mixer) */
void br_snd_mix(float *lr, int n)
{
    int v, i;
    lock();
    for (v = 0; v < s_nlive; v++) {
        dsbuf *b = s_live[v];
        float g = cb_gain(b->vol);
        float gl = g * (b->pan > 0 ? cb_gain(-b->pan) : 1.0f);
        float gr = g * (b->pan < 0 ? cb_gain(b->pan) : 1.0f);
        float dl = (gl - b->gl) / (float)n, dr = (gr - b->gr) / (float)n, al = b->gl, ar = b->gr;
        double step = (double)(b->freq ? b->freq : b->rate) / OUT_RATE, p = b->fpos;
        if (!b->playing || !b->pcm || !b->frames) {
            b->live = 0;
            s_live[v--] = s_live[--s_nlive];
            continue;
        }
        for (i = 0; i < n; i++) {
            int64_t k = (int64_t)p;
            float x = (float)(p - (double)k);
            al += dl;
            ar += dr;
            if (b->ch == 1) {
                float s = herm(at(b, k - 1, 0), at(b, k, 0), at(b, k + 1, 0), at(b, k + 2, 0), x);
                lr[2 * i] += s * al;
                lr[2 * i + 1] += s * ar;
            } else {
                lr[2 * i] += herm(at(b, k - 1, 0), at(b, k, 0), at(b, k + 1, 0), at(b, k + 2, 0), x) * al;
                lr[2 * i + 1] += herm(at(b, k - 1, 1), at(b, k, 1), at(b, k + 1, 1), at(b, k + 2, 1), x) * ar;
            }
            p += step;
            if (p >= b->frames) {
                if (b->looping)
                    p = fmod(p, (double)b->frames);
                else {
                    b->playing = 0;
                    b->pos = 0;
                    p = 0;
                    break;
                }
            }
        }
        b->fpos = p;
        b->gl = gl;
        b->gr = gr;
        if (!b->playing) {
            b->live = 0;
            s_live[v--] = s_live[--s_nlive];
        }
    }
    unlock();
}

/* ---- samples ------------------------------------------------------------------------ */
/* The play cursor in bytes; called locked. While the host's mixer runs it is
 * the cursor; without one the clock is, so the game's state moves the same
 * and nothing is heard. The clock is the one the game reads (timeGetTime),
 * so a BR_VCLOCK run reads it where the original's DirectSound did. */
static uint32_t cursor(dsbuf *b)
{
    uint64_t p;
    if (s_mixing) {
        p = (uint64_t)b->fpos * b->align;
        return (uint32_t)(p < b->size ? p : 0);
    }
    if (!b->playing)
        return b->pos;
    p = b->pos + (uint64_t)(timeGetTime() - b->t0) * b->rate * b->align / 1000;
    p -= p % b->align;
    if (p >= b->size) {
        if (b->looping)
            p %= b->size;
        else {
            b->playing = 0;
            b->pos = 0;
            return 0;
        }
    }
    return (uint32_t)p;
}

/* the output, started on first use (audio.c; BR_SFX=0 keeps it off: the clock
 * is the cursor) */
static int s_out = 1;               /* what br_snd_open hands the game */

void *br_snd_open(void)
{
    static int opened;
    if (!opened) {
        opened = 1;
        if (plat_audio_start()) {
            lock();
            s_mixing = 1;
            unlock();
        }
    }
    /* the original started its primary buffer playing here, which read the
       clock: read it too, so a BR_VCLOCK run keeps the original's timeline */
    (void)timeGetTime();
    return &s_out;
}

void br_snd_close(void)
{
    /* the original stopped its primary buffer here, which read the clock */
    (void)timeGetTime();
}

/* the samples, as floats: PCM, 8-bit unsigned or 16-bit signed; any other
 * format makes a sample that plays silence */
BrSnd *br_snd_new(const BrWavFmt *w, const void *data, uint32_t bytes)
{
    dsbuf *b;
    uint32_t frames, i, n;
    int k, bits;
    const uint8_t *mem = (const uint8_t *)data;
    if (!w || !bytes)
        return NULL;
    b = (dsbuf *)calloc(1, sizeof *b);
    if (!b)
        return NULL;
    bits = w->wBitsPerSample;
    b->ch = w->nChannels ? w->nChannels : 1;
    b->rate = w->nSamplesPerSec ? w->nSamplesPerSec : 22050;
    b->align = w->nBlockAlign ? w->nBlockAlign : (uint32_t)(b->ch * bits / 8);
    if (!b->align)
        b->align = 1;
    b->size = bytes;
    if (w->wFormatTag != 1 || (bits != 8 && bits != 16))
        return b;
    frames = bytes / b->align;
    n = frames * (uint32_t)b->ch;
    b->pcm = (float *)malloc((n ? n : 1) * sizeof *b->pcm);
    if (!b->pcm) {
        free(b);
        return NULL;
    }
    for (i = 0; i < frames; i++)
        for (k = 0; k < b->ch; k++) {
            const uint8_t *s = mem + i * b->align + (uint32_t)k * (uint32_t)(bits / 8);
            b->pcm[i * (uint32_t)b->ch + (uint32_t)k] = bits == 8 ? ((int)s[0] - 128) / 128.0f
                                                                 : (int16_t)(s[0] | s[1] << 8) / 32768.0f;
        }
    b->frames = frames;
    return b;
}

void br_snd_free(BrSnd *b)
{
    if (!b)
        return;
    lock();
    unlive(b);
    b->playing = 0;
    unlock();
    free(b->pcm);
    free(b);
}

int br_snd_playing(BrSnd *b)
{
    int r;
    lock();
    cursor(b);
    r = b->playing;
    unlock();
    return r;
}

void br_snd_play(BrSnd *b, int loop)
{
    lock();
    if (!b->playing) {
        b->playing = 1;
        b->t0 = timeGetTime();
    }
    b->looping = loop != 0;
    if (b->pcm && !b->live && s_nlive < (int)(sizeof s_live / sizeof s_live[0])) {
        s_live[s_nlive++] = b;
        b->live = 1;
    }
    unlock();
}

void br_snd_seek(BrSnd *b, uint32_t p)
{
    lock();
    b->pos = p % (b->size ? b->size : 1);
    b->pos -= b->pos % b->align;
    b->t0 = timeGetTime();
    b->fpos = b->pos / b->align;
    unlock();
}

static int32_t clampl(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : v; }

void br_snd_volume(BrSnd *b, int32_t v)
{
    lock();
    b->vol = clampl(v, -10000, 0);
    unlock();
}

void br_snd_pan(BrSnd *b, int32_t v)
{
    lock();
    b->pan = clampl(v, -10000, 10000);
    unlock();
}

void br_snd_rate(BrSnd *b, uint32_t v)
{
    lock();
    b->freq = v ? (uint32_t)clampl((int32_t)v, 100, 200000) : 0;
    unlock();
}

void br_snd_stop(BrSnd *b)
{
    lock();
    b->pos = cursor(b);
    b->playing = 0;
    unlive(b);
    unlock();
}
