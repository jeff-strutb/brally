/* mixer.c: the game's software mixer (src/tgrally/audio/mixer.s, hand-written
 * MIPS III) in C, instruction for instruction in what it computes.
 *
 * A voice's position is its current 8-bit sample and a 32-bit fraction past
 * it (tgr/mix.h): the original's 32.32 accumulator, whose high word was the
 * sample's address.  Adding a rate adds its low word to the fraction and its
 * high word, with the carry, to the sample pointer.
 * The output ring (16 KB of 16-bit stereo) is kept big-endian, as the N64's
 * RAM held it and the audio interface read it; BrMixSfx adds into it as the
 * original did, four samples at a time in one 64-bit register. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "plat.h"
#include "tgr_core.h"
#include "tgr/mix.h"

typedef struct {            /* a voice's position while mixing */
    const signed char *p;
    uint32_t f;
} Acc;

static inline Acc acc_of(const BrMixVoice *v) { Acc a; a.p = v->pos; a.f = v->frac; return a; }
static inline void acc_set(BrMixVoice *v, Acc a) { v->pos = (signed char *)a.p; v->frac = a.f; }
static inline void acc_add(Acc *a, uint64_t rate)
{
    uint64_t f = (uint64_t)a->f + (uint32_t)rate;
    a->f = (uint32_t)f;
    a->p += (uint32_t)(rate >> 32) + (uint32_t)(f >> 32);
}
static inline int32_t smp(Acc a) { return *a.p; }

extern unsigned short D_802A4790;   /* the ring position */
extern int D_802A4918;              /* the effects' lane mask, a u64 as two words, high first */
extern unsigned int D_802A491C;
extern int D_802A49B0;              /* the effects' second source: an offset from voice 0's sample */

/* BrCopy16: two doublewords, from an original address */
void BrCopy16(void *dst, unsigned int src)
{
    memcpy(dst, TGR_PTR(void *, src), 16);
}

/* BrFill64: the value doubled into a doubleword, 64 bytes at a time */
void BrFill64(void *dst, int n, int value)
{
    uint8_t *p = (uint8_t *)dst;
    uint32_t len = ((uint32_t)n + 0x3F) & 0xFFC0;
    uint32_t i;
    if (len == 0)
        len = 0x10000;              /* the loop runs until it wraps back round */
    for (i = 0; i < len; i += 4)
        tgr_wr32(p + i, (uint32_t)value);
}

/* BrFloatToInt: trunc.w.s, an invalid result 0x7FFFFFFF */
int BrFloatToInt(float f)
{
    if (!(f > -2147483648.0f && f < 2147483648.0f))
        return 0x7FFFFFFF;
    return (int)f;
}

/* the ring position and the chunk split at the ring's end */
static uint8_t *ring_start(short *buf, int bytes, int32_t *first, int32_t *second)
{
    uint16_t pos = D_802A4790;
    int32_t over;
    over = (int32_t)pos + bytes - 0x4000;
    if (over <= 0)
        over = 0;
    *first = bytes - over;
    *second = over;
    return (uint8_t *)buf + pos;
}

void BrMixMusic(short *buf, int bytes)
{
    BrMixVoice *v = D_802A4798;

    Acc a0 = acc_of(&v[0]), a1 = acc_of(&v[1]), a2 = acc_of(&v[2]);
    Acc a3 = acc_of(&v[3]), a4 = acc_of(&v[4]), a5 = acc_of(&v[5]);
    int32_t n, rest;
    uint8_t *p = ring_start(buf, bytes, &n, &rest);
    for (;;) {
        if (n != 0) {
            do {
                int32_t l, r;
                acc_add(&a0, v[0].rate); acc_add(&a2, v[2].rate); acc_add(&a1, v[1].rate);
                acc_add(&a3, v[3].rate); acc_add(&a4, v[4].rate); acc_add(&a5, v[5].rate);
                n -= 4;
                l = v[0].vol * smp(a0) + v[2].vol * smp(a2) + v[4].vol * smp(a4);
                tgr_wr16(p, (uint16_t)l);
                r = v[1].vol * smp(a1) + v[3].vol * smp(a3) + v[5].vol * smp(a5);
                tgr_wr16(p + 2, (uint16_t)r);
                p += 4;
            } while (n > 0);
        }
        if (rest == 0)
            break;
        n = rest;
        p -= 0x4000;
        rest = 0;
    }
    acc_set(&v[0], a0); acc_set(&v[2], a2); acc_set(&v[1], a1);
    acc_set(&v[3], a3); acc_set(&v[4], a4); acc_set(&v[5], a5);
    D_802A4790 = (uint16_t)(p - (uint8_t *)buf);
}

void BrMixMusicVoice(short *buf, int bytes, int voice)
{
    BrMixVoice *v = &D_802A4798[voice];     /* this voice, and the next for the right */
    Acc a = acc_of(&v[0]), b = acc_of(&v[1]);
    int32_t n, rest;
    uint8_t *p = ring_start(buf, bytes, &n, &rest);
    for (;;) {
        if (n != 0) {
            do {
                int32_t l, r;
                acc_add(&a, v[0].rate);
                acc_add(&b, v[1].rate);
                n -= 4;
                l = v[0].vol * smp(a);
                r = v[1].vol * smp(b);
                tgr_wr16(p, (uint16_t)((int16_t)tgr_rd16(p) + l));
                tgr_wr16(p + 2, (uint16_t)((int16_t)tgr_rd16(p + 2) + r));
                p += 4;
            } while (n > 0);
        }
        if (rest == 0)
            break;
        n = rest;
        p -= 0x4000;
        rest = 0;
    }
    acc_set(&v[0], a);
    acc_set(&v[1], b);
    D_802A4790 = (uint16_t)(p - (uint8_t *)buf);
}

static inline uint64_t rd64(const uint8_t *p) { return (uint64_t)tgr_rd32(p) << 32 | tgr_rd32(p + 4); }
static inline void wr64(uint8_t *p, uint64_t v) { tgr_wr32(p, (uint32_t)(v >> 32)); tgr_wr32(p + 4, (uint32_t)v); }

void BrMixSfx(short *buf, unsigned int bytes, unsigned int pos)
{
    {
    BrMixVoice *v = D_802A4920;
    Acc a0 = acc_of(&v[0]), a1 = acc_of(&v[1]), a2 = acc_of(&v[2]);
    Acc a3 = acc_of(&v[3]), a4 = acc_of(&v[4]);
    uint64_t mask = (uint64_t)(uint32_t)D_802A4918 << 32 | D_802A491C;
    int32_t second = D_802A49B0;
    int32_t n, rest, over;
    uint8_t *p;
    over = (int32_t)(pos + bytes) - 0x4000;
    if (over <= 0)
        over = 0;
    n = (int32_t)bytes - over;
    rest = over;
    p = (uint8_t *)buf + pos;
    for (;;) {
        if (n != 0) {
            do {
                int32_t s, t;
                uint64_t old, sum;
                acc_add(&a0, v[0].rate); acc_add(&a1, v[1].rate); acc_add(&a2, v[2].rate); acc_add(&a3, v[3].rate); acc_add(&a4, v[4].rate);
                old = rd64(p);
                t = 0;
                if (second != 0)
                    t = a0.p[second];
                s = v[0].vol * (t + smp(a0));
                s += v[1].vol * smp(a1);
                s += v[2].vol * smp(a2);
                s += v[3].vol * smp(a3);
                s += v[4].vol * smp(a4);
                /* and $24,$24,$14 on the sign-extended 32-bit sum */
                sum = (uint64_t)(int64_t)s & mask;
                sum = (sum << 32) + sum;
                sum = (sum + (old & mask)) & mask;
                n -= 8;
                wr64(p, sum);
                p += 8;
            } while (n > 0);
        }
        if (rest == 0)
            break;
        n = rest;
        p -= 0x4000;
        rest = 0;
    }
    acc_set(&v[0], a0); acc_set(&v[1], a1); acc_set(&v[2], a2); acc_set(&v[3], a3); acc_set(&v[4], a4);
    }
}
