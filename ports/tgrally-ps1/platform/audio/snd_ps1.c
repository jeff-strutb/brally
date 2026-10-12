/* snd_ps1.c: the game's sound on the SPU.
 *
 * The game mixes its voices in software (mixer.s: six music voices, five
 * effect voices and a second effect source, 8-bit samples stepped by 32.32
 * rates); the music player and the effects code set each voice's sample
 * position, rate and volume, loop a sample by moving its position back by
 * the loop length, and read the positions back.  Here the SPU plays them:
 * each sample in use is converted once to ADPCM in sound RAM with its loop
 * in the blocks' flags (padded at the front so the loop starts on a block,
 * the loop repeated so it ends on one), each game voice is an SPU voice
 * keyed on where the game starts it, and the mixer entry points only move
 * the positions on by what they would have mixed (the arithmetic the game
 * sees is the same; the sound is the SPU's). */
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "plat.h"
#include "ps1.h"
#include "spu.h"
#include "tgr_core.h"

typedef struct {            /* a mixer voice in game memory (0x18 bytes, as mixer.c has it) */
    uint32_t pos, frac;
    uint64_t rate;
    int32_t vol, baseVol;
} Voice;

typedef struct {            /* a module instrument's sample header (music.c's BrSample) */
    uint8_t len[4];
    int32_t x4;
    uint8_t loopLen[4];
    uint8_t vol, pad0d, loops, pad0f;
} SampleHdr;

#define MUSIC 0x802A4790u   /* the music ring position, then the voices from +8 */
#define SFX   0x802A4918u   /* the effects lane mask, voices from +8, the second source at +0x98 */
extern void *D_803787D0[];  /* the module's instruments' sample headers (native pointers) */
extern uint8_t D_80378DD0[];    /* the module channels, 0x18 bytes each: the instrument at +0x0D */
extern uint32_t D_80378F50[];   /* the effect voices' samples: start, length, loop length */
extern uint32_t tgr_ai_rate(void);

/* ---- ADPCM --------------------------------------------------------------------- */
static const int FILTER[5][2] = { { 0, 0 }, { 60, 0 }, { 115, -52 }, { 98, -55 }, { 122, -60 } };

/* one 16-byte block from 28 samples; s1, s2 the decoder's history, kept */
static void adpcm_block(const int16_t *in, int32_t *s1, int32_t *s2, uint8_t *out, uint8_t flags)
{
    int best_f = 0, best_sh = 12, f;
    uint32_t best_err = 0xFFFFFFFF;
    uint8_t best[14];
    int32_t bs1 = *s1, bs2 = *s2;
    for (f = 0; f < 4; f++) {
        int32_t p1 = *s1, p2 = *s2, maxr = 0, i, sh;
        uint32_t err = 0;
        uint8_t nib[28];
        /* the shift from the largest residual against the input's own history */
        {
            int32_t q1 = *s1, q2 = *s2;
            for (i = 0; i < 28; i++) {
                int32_t pred = (q1 * FILTER[f][0] + q2 * FILTER[f][1] + 32) >> 6;
                int32_t r = in[i] - pred;
                if (r < 0) r = -r;
                if (r > maxr) maxr = r;
                q2 = q1;
                q1 = in[i];
            }
        }
        for (sh = 12; sh > 0 && (maxr >> (12 - sh)) > 7; sh--)
            ;
        for (i = 0; i < 28; i++) {
            int32_t pred = (p1 * FILTER[f][0] + p2 * FILTER[f][1] + 32) >> 6;
            int32_t r = in[i] - pred, q, d;
            q = r >= 0 ? (r + ((1 << (12 - sh)) >> 1)) >> (12 - sh) : -((-r + ((1 << (12 - sh)) >> 1)) >> (12 - sh));
            if (q > 7) q = 7;
            if (q < -8) q = -8;
            d = ((q << 12) >> sh) + pred;
            if (d > 32767) d = 32767;
            if (d < -32768) d = -32768;
            err += (uint32_t)((in[i] - d) < 0 ? d - in[i] : in[i] - d);
            nib[i] = (uint8_t)(q & 15);
            p2 = p1;
            p1 = d;
        }
        if (err < best_err) {
            best_err = err;
            best_f = f;
            best_sh = sh;
            for (i = 0; i < 14; i++)
                best[i] = (uint8_t)(nib[2 * i] | nib[2 * i + 1] << 4);
            bs1 = p1;
            bs2 = p2;
        }
    }
    out[0] = (uint8_t)(best_sh | best_f << 4);
    out[1] = flags;
    memcpy(out + 2, best, 14);
    *s1 = bs1;
    *s2 = bs2;
}

/* ---- samples in sound RAM ---------------------------------------------------------- */
#define NSAMP 96
typedef struct {
    uint32_t start, len, loop;      /* the game's sample (start address, bytes, loop length; 0 one-shot) */
    uint32_t check;                 /* a few of its bytes: the same sample still there */
    uint32_t spu;                   /* its first block in sound RAM */
    uint32_t pad;                   /* silent samples before it (the loop's alignment) */
} Sample;
static Sample s_samp[NSAMP];
static int s_nsamp;
static uint32_t s_sgen;                         /* (bumped when the samples start again: a sample_get memo is stale) */
static uint32_t s_spu_top = SPU_RAM_FIRST + 16;
#define SILENCE SPU_RAM_FIRST       /* a block of silence that loops on itself */

static uint32_t sample_check(uint32_t start, uint32_t len)
{
    const uint8_t *p = TGR_PTR(const uint8_t *, start);
    uint32_t h = len * 2654435761u, i, step = len > 64 ? len / 32 : 1;
    for (i = 0; i < len; i += step)
        h = (h ^ p[i]) * 16777619u;
    return h;
}

static int16_t s_pcm[28];

/* the n-th sample of the converted sequence: the padding, the data, the
 * loop repeated (or silence after a one-shot) */
static int16_t seq(const Sample *s, const uint8_t *d, uint32_t i)
{
    uint32_t ls;
    if (i < s->pad)
        return 0;
    i -= s->pad;
    if (i < s->len)
        return (int16_t)((int8_t)d[i] * 256);
    if (!s->loop)
        return 0;
    ls = s->len - s->loop;
    return (int16_t)((int8_t)d[ls + (i - ls) % s->loop] * 256);
}

static const Sample *sample_get_(uint32_t start, uint32_t len, uint32_t loop);

/* a sample found again within the same retrace: as it was (its bytes checked
 * once a retrace, not at every voice's look) */
static const Sample *sample_get(uint32_t start, uint32_t len, uint32_t loop)
{
    static struct { uint32_t start, len, loop, frame, n; const Sample *s; } memo[16];
    uint32_t h = ((start >> 4) ^ (start >> 12) ^ len) & 15, f = tgr_frame();
    if (memo[h].s && memo[h].start == start && memo[h].len == len && memo[h].loop == loop && memo[h].frame == f &&
        memo[h].n == s_sgen)
        return memo[h].s;
    memo[h].s = sample_get_(start, len, loop);
    memo[h].start = start;
    memo[h].len = len;
    memo[h].loop = loop;
    memo[h].frame = f;
    memo[h].n = s_sgen;
    return memo[h].s;
}

static const Sample *sample_get_(uint32_t start, uint32_t len, uint32_t loop)
{
    uint32_t check = sample_check(start, len), total, nblk, loop_blk = 0, b, k;
    Sample *s;
    int i;
    const uint8_t *d = TGR_PTR(const uint8_t *, start);
    int32_t s1 = 0, s2 = 0;
    uint8_t blk[16];
    for (i = 0; i < s_nsamp; i++)
        if (s_samp[i].start == start && s_samp[i].len == len && s_samp[i].loop == loop && s_samp[i].check == check)
            return &s_samp[i];
    if (loop > len)
        loop = len;
    /* the length in sound RAM: the loop's start padded onto a block, the loop
     * repeated until it ends on one (or as near as 28 repeats come) */
    {
        uint32_t ls = len - loop, pad = loop ? (28 - ls % 28) % 28 : 0, reps = 1, body;
        if (loop) {
            for (k = 1; k <= 28 && k * loop <= 16384; k++)
                if ((k * loop) % 28 == 0) {
                    reps = k;
                    break;
                }
            body = (reps * loop + 27) / 28 * 28;
            total = pad + ls + body;
            loop_blk = (pad + ls) / 28;
        } else {
            total = (len + 27) / 28 * 28;
        }
        nblk = total / 28;
        if (s_spu_top + nblk * 16 + 16 > SPU_RAM_END || s_nsamp == NSAMP) {
            /* sound RAM full: start again (every voice is keyed again on use) */
            s_nsamp = 0;
            s_sgen++;
            s_spu_top = SPU_RAM_FIRST + 16;
        }
        s = &s_samp[s_nsamp++];
        s->start = start;
        s->len = len;
        s->loop = loop;
        s->check = check;
        s->pad = pad;
        s->spu = s_spu_top;
    }
    for (b = 0; b < nblk; b++) {
        uint8_t flags = 0;
        for (k = 0; k < 28; k++)
            s_pcm[k] = seq(s, d, b * 28 + k);
        if (s->loop && b == loop_blk)
            flags |= 4;
        if (s->loop && b == nblk - 1)
            flags |= 3;                         /* the end: back to the loop's start */
        adpcm_block(s_pcm, &s1, &s2, blk, flags);
        spu_write(s_spu_top + b * 16, blk, 16);
    }
    s_spu_top += nblk * 16;
    if (getenv("TGR_SNDLOG"))
        fprintf(stderr, "snd: sample %08X len %u loop %u -> %05X (%u blocks)\n", (unsigned)start, (unsigned)len,
                (unsigned)loop, (unsigned)s->spu, (unsigned)nblk);
    if (!s->loop) {                             /* a one-shot ends in the looping silence */
        memset(blk, 0, 16);
        blk[1] = 7;
        spu_write(s_spu_top, blk, 16);
        s_spu_top += 16;
    }
    return s;
}

/* ---- voices ------------------------------------------------------------------------ */
typedef struct {
    const Sample *smp;
    uint32_t expect;            /* where the voice will be when the game looks again */
    int on;
} HwVoice;
static HwVoice s_hw[24];
#define SPU_MUSIC 0             /* music voices 0..11 */
#define SPU_SFX   12            /* effect voices 12..16, the second source 17 */

static uint32_t pitch_of(uint64_t rate, uint32_t out_rate)
{
    return (uint32_t)((rate * out_rate / 44100u) >> 20);
}

/* the voice played by SPU voice hv: keyed on where the game put it (unless it
 * is where the SPU's own playing, or its loop, brings it), its pitch and
 * levels set, and its position moved on by `frames` steps */
static void voice_sync(int hv, Voice *v, const Sample *smp, uint32_t frames, uint32_t out_rate, int l, int r)
{
    HwVoice *h = &s_hw[hv];
    uint64_t acc;
    if (!v->rate || !smp) {
        if (h->on)
            spu_voice_off(hv);
        h->on = 0;
        return;
    }
    if (!h->on || h->smp != smp ||
        (v->pos != h->expect && !(smp->loop && v->pos == h->expect - smp->loop))) {
        uint32_t off = v->pos - smp->start;
        if (off >= smp->len) {
            if (h->on)
                spu_voice_off(hv);
            h->on = 0;
        } else {
            spu_voice_start(hv, smp->spu + (off + smp->pad) / 28 * 16);
            h->on = 1;
            h->smp = smp;
        }
    }
    spu_voice_pitch(hv, pitch_of(v->rate, out_rate));
    spu_voice_volume(hv, l, r);
    acc = ((uint64_t)v->pos << 32 | v->frac) + v->rate * frames;
    v->pos = (uint32_t)(acc >> 32);
    v->frac = (uint32_t)acc;
    h->expect = v->pos;
}

static void ring_move(uint32_t bytes)
{
    uint16_t *ring = TGR_PTR(uint16_t *, MUSIC);
    uint32_t p = *ring + bytes;
    if (p > 0x4000)
        p -= 0x4000;
    *ring = (uint16_t)p;
}

static const Sample *music_sample(int ch)
{
    int n = D_80378DD0[ch * 0x18 + 0x0D];
    const SampleHdr *hdr;
    if (n == 0 || !D_803787D0[n - 1])
        return NULL;
    hdr = (const SampleHdr *)D_803787D0[n - 1];
    return sample_get(tgr_addr32(hdr) + 0x28, tgr_rd32(hdr->len), hdr->loops ? tgr_rd32(hdr->loopLen) : 0);
}

static int vol_of(int32_t v) { return v * 64; }

void BrMixMusic(short *buf, int bytes)
{
    Voice *v = TGR_PTR(Voice *, MUSIC + 8);
    uint32_t rate = tgr_ai_rate(), frames = (uint32_t)bytes / 4;
    int i;
    (void)buf;
    for (i = 0; i < 6; i++)                     /* 0, 2, 4 on the left; 1, 3, 5 on the right */
        voice_sync(SPU_MUSIC + i, &v[i], music_sample(i), frames, rate, (i & 1) ? 0 : vol_of(v[i].vol),
                   (i & 1) ? vol_of(v[i].vol) : 0);
    ring_move((uint32_t)bytes);
    spu_keys();
}

void BrMixMusicVoice(short *buf, int bytes, int voice)
{
    Voice *v = TGR_PTR(Voice *, MUSIC + 0x98 + voice);      /* this voice, and the next for the right */
    int ch = (int)((voice + 0x90) / 0x18);
    uint32_t rate = tgr_ai_rate(), frames = (uint32_t)bytes / 4;
    (void)buf;
    if (ch + 1 < 12) {
        voice_sync(SPU_MUSIC + ch, &v[0], music_sample(ch), frames, rate, vol_of(v[0].vol), 0);
        voice_sync(SPU_MUSIC + ch + 1, &v[1], music_sample(ch + 1), frames, rate, 0, vol_of(v[1].vol));
    }
    ring_move((uint32_t)bytes);
    spu_keys();
}

void BrMixSfx(short *buf, unsigned int bytes, unsigned int pos)
{
    uint8_t *blk = TGR_PTR(uint8_t *, SFX);
    Voice *v = (Voice *)(blk + 8);
    uint32_t mask = *(uint32_t *)blk | *(uint32_t *)(blk + 4), second = *(uint32_t *)(blk + 0x98);
    uint32_t rate = tgr_ai_rate() / 2, frames = bytes / 8;  /* a step every two output frames */
    int i;
    (void)buf;
    (void)pos;
    for (i = 0; i < 5; i++) {
        const Sample *smp = NULL;
        int l = 0, r = 0;
        if (v[i].rate && D_80378F50[i * 3 + 1])
            smp = sample_get(D_80378F50[i * 3], D_80378F50[i * 3 + 1], D_80378F50[i * 3 + 2]);
        if (mask) {
            l = vol_of((int32_t)((uint32_t)v[i].vol >> 16));
            r = vol_of(v[i].vol & 0xFFFF);
        }
        if (i == 0 && second && smp) {
            /* the second source: the bytes `second` on from voice 0's, at its rate and levels */
            Voice w = v[0];
            const Sample *s2 = sample_get(D_80378F50[0] + second, D_80378F50[1], D_80378F50[2]);
            w.pos += second;
            voice_sync(SPU_SFX + 5, &w, s2, frames, rate, l, r);
        } else if (i == 0 && s_hw[SPU_SFX + 5].on) {
            spu_voice_off(SPU_SFX + 5);
            s_hw[SPU_SFX + 5].on = 0;
        }
        voice_sync(SPU_SFX + i, &v[i], smp, frames, rate, l, r);
    }
    spu_keys();
}

void snd_init(void)
{
    uint8_t blk[16];
    spu_init();
    memset(blk, 0, 16);
    blk[1] = 7;
    spu_write(SILENCE, blk, 16);
}

/* ---- the rest of mixer.c's helpers (the copies the game calls) ------------------------ */
void BrCopy16(void *dst, unsigned int src)
{
    memcpy(dst, TGR_PTR(void *, src), 16);
}

void BrFill64(void *dst, int n, int value)
{
    uint8_t *p = (uint8_t *)dst;
    uint32_t len = ((uint32_t)n + 0x3F) & 0xFFC0;
    uint32_t i;
    if (len == 0)
        len = 0x10000;
    for (i = 0; i < len; i += 4)
        tgr_wr32(p + i, (uint32_t)value);
}

int BrFloatToInt(float f)
{
    if (!(f > -2147483648.0f && f < 2147483648.0f))
        return 0x7FFFFFFF;
    return (int)f;
}
