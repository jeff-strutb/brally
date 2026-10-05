/* mixer.c: the game's software mixer (n64/src/audio/mixer.s, hand-written
 * MIPS III) in C, instruction for instruction in what it computes.
 *
 * A voice's position is a 32.32 accumulator whose high word is the address
 * of its current 8-bit sample (an original address); natively the voice
 * keeps the two words as libultra-era code wrote them, high word first.
 * The output ring (16 KB of 16-bit stereo) is kept big-endian, as the N64's
 * RAM held it and the audio interface read it; BrMixSfx adds into it as the
 * original did, four samples at a time in one 64-bit register. */
#include <stdint.h>
#include <string.h>
#include "plat.h"
#include "tgr_core.h"

typedef struct {            /* a mixer voice in game memory (0x18 bytes) */
    uint32_t pos;           /* 0x00  sample address: the accumulator's high word */
    uint32_t frac;          /* 0x04  its low word */
    uint64_t rate;          /* 0x08 */
    int32_t  vol;           /* 0x10 */
    int32_t  baseVol;       /* 0x14 */
} Voice;

static inline uint64_t acc_of(const Voice *v) { return (uint64_t)v->pos << 32 | v->frac; }
static inline void acc_set(Voice *v, uint64_t a) { v->pos = (uint32_t)(a >> 32); v->frac = (uint32_t)a; }
static inline int32_t smp(uint64_t a) { return (int8_t)*TGR_PTR(uint8_t *, (uint32_t)(a >> 32)); }

#define MUSIC   0x802A4790u     /* u16 ring position, then six voices from +8 */
#define SFX     0x802A4918u     /* u64 lane mask, five voices from +8, a second source at +0x98 */

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
    uint16_t pos = *TGR_PTR(uint16_t *, MUSIC);     /* the game keeps it natively (a short) */
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
    Voice *v = TGR_PTR(Voice *, MUSIC + 8);
    uint64_t a0 = acc_of(&v[0]), a1 = acc_of(&v[1]), a2 = acc_of(&v[2]);
    uint64_t a3 = acc_of(&v[3]), a4 = acc_of(&v[4]), a5 = acc_of(&v[5]);
    int32_t n, rest;
    uint8_t *p = ring_start(buf, bytes, &n, &rest);
    for (;;) {
        if (n != 0) {
            do {
                int32_t l, r;
                a0 += v[0].rate; a2 += v[2].rate; a1 += v[1].rate;
                a3 += v[3].rate; a4 += v[4].rate; a5 += v[5].rate;
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
    *TGR_PTR(uint16_t *, MUSIC) = (uint16_t)(p - (uint8_t *)buf);
}

void BrMixMusicVoice(short *buf, int bytes, int voice)
{
    Voice *v = TGR_PTR(Voice *, MUSIC + 0x98 + voice);      /* this voice, and the next for the right */
    uint64_t a = acc_of(&v[0]), b = acc_of(&v[1]);
    int32_t n, rest;
    uint8_t *p = ring_start(buf, bytes, &n, &rest);
    for (;;) {
        if (n != 0) {
            do {
                int32_t l, r;
                a += v[0].rate;
                b += v[1].rate;
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
    *TGR_PTR(uint16_t *, MUSIC) = (uint16_t)(p - (uint8_t *)buf);
}

static inline uint64_t rd64(const uint8_t *p) { return (uint64_t)tgr_rd32(p) << 32 | tgr_rd32(p + 4); }
static inline void wr64(uint8_t *p, uint64_t v) { tgr_wr32(p, (uint32_t)(v >> 32)); tgr_wr32(p + 4, (uint32_t)v); }

void BrMixSfx(short *buf, unsigned int bytes, unsigned int pos)
{
    uint8_t *blk = TGR_PTR(uint8_t *, SFX);
    Voice *v = (Voice *)(blk + 8);
    uint64_t a0 = acc_of(&v[0]), a1 = acc_of(&v[1]), a2 = acc_of(&v[2]);
    uint64_t a3 = acc_of(&v[3]), a4 = acc_of(&v[4]);
    uint64_t mask = (uint64_t)*(uint32_t *)blk << 32 | *(uint32_t *)(blk + 4);   /* the u64 as two native words, high first */
    uint32_t second = *(uint32_t *)(blk + 0x98);
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
                a0 += v[0].rate; a1 += v[1].rate; a2 += v[2].rate; a3 += v[3].rate; a4 += v[4].rate;
                old = rd64(p);
                t = 0;
                if (second != 0)
                    t = (int8_t)*TGR_PTR(uint8_t *, second + (uint32_t)(a0 >> 32));
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
