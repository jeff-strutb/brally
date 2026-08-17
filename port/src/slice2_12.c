/* slice2_12.c -- BRD3D.dll 0x100053F0-0x10008AA0. See slice2_12.h. */

#include "slice2_12.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* =====================================================================
 * Shared primitives the original reaches through the CRT
 * ===================================================================== */

/* 0x1007DB00 is `floor` (it sets the x87 rounding control toward -infinity),
 * per the project's known-correct facts. */
#define BrFloor(d) floor(d)

/* 0x1007C8A0 is __ftol: chop toward zero, keep the LOW dword of the 64-bit
 * result. Same transcription a later pass used in slice1_02.c, repeated here
 * because that copy is static to its translation unit.
 *
 * DEVIATION: a plain (int32_t) cast is undefined outside int32 range while
 * the original is not, so the two out-of-range cases are written out. */
static int32_t BrFtol(double d)
{
    int64_t  wide;
    uint32_t lo;
    int32_t  out;

    if (!(d >= -9223372036854775808.0) || !(d < 9223372036854775808.0))
        return 0;                       /* x87 indefinite -> low dword is 0 */

    wide = (int64_t)d;
    lo   = (uint32_t)((uint64_t)wide & 0xFFFFFFFFu);
    memcpy(&out, &lo, sizeof out);      /* reinterpret, do not convert */
    return out;
}

/* `sar` on a 32-bit register, written so it does not rely on C's
 * implementation-defined right shift of a negative value. */
static int32_t BrSar(int32_t v, int n)
{
    if (v < 0)
        return (int32_t)~(((uint32_t)~(uint32_t)v) >> n);
    return (int32_t)(((uint32_t)v) >> n);
}

/* `fcomp <mem>; fnstsw ax; test ah,0x40; jne` -- C3 is set for EQUAL and for
 * UNORDERED, so "non-zero" in the original excludes NaN. */
static int BrIsNonZero(float v)
{
    return (v < 0.0f) || (v > 0.0f);
}

/* Every quantiser stores its x87 result as a double before calling floor, so
 * the whole chain is reproduced in double here.
 *
 * DEVIATION: the original computes in 80-bit extended precision and rounds to
 * double only at the `fstp qword`. Products of two floats and sums with 0.5
 * are exact in double for every input the callers can produce, so the two
 * agree; a hand-crafted input sitting exactly on a rounding boundary could
 * still differ by one unit in the last place of the intermediate. */

/* .rdata constants, read out of the shipped DLL rather than inferred. */
static const float BrK08F0A8 =  1.0f;                    /* 0x1008F0A8 */
static const float BrK08F0B0 =  0.0f;                    /* 0x1008F0B0 */
static const float BrK08F0B4 = -1.0f;                    /* 0x1008F0B4 */
static const float BrK08F0B8 =  2048.0f;                 /* 0x1008F0B8 */
static const float BrK08F0BC = -256.0f;                  /* 0x1008F0BC */
static const float BrK08F0C0 =  256.0f;                  /* 0x1008F0C0 */
static const float BrK08F0CC =  128.0f;                  /* 0x1008F0CC */
static const float BrK08F0D0 =  0.5f;                    /* 0x1008F0D0 */
static const float BrK08F0D4 =  32768.0f;                /* 0x1008F0D4 */
static const float BrK08F0D8 = -0.7091412544250488f;     /* 0x1008F0D8 */
static const float BrK08F0DC =  400.0f;                  /* 0x1008F0DC */
static const float BrK08F0E0 = -0.008289474062621593f;   /* 0x1008F0E0 */
static const float BrK08F0E4 =  171.0f;                  /* 0x1008F0E4 */
static const float BrK08F0E8 =  213.0f;                  /* 0x1008F0E8 */
static const float BrK08F108 = -256.0f;                  /* 0x1008F108 */
static const float BrK08F10C = -8.0f;                    /* 0x1008F10C */
static const float BrK08F110 = -35.0f;                   /* 0x1008F110 */
static const float BrK08F114 =  360.0f;                  /* 0x1008F114 */
static const float BrK08F118 =  1.0f;                    /* 0x1008F118 */

/* =====================================================================
 * 1. Car-state field clamps
 * ===================================================================== */

/* 0x100058D0. The low test is `test ah,1` (C0: less, or unordered), so NaN
 * lands on the low bound; the high test is `test ah,0x41` inverted, i.e.
 * strictly greater, which NaN never satisfies. */
/* WHAT IT DOES: keeps one component of a car's facing direction inside the
 * range -1 to 1, in case a network prediction or a bad packet pushed it
 * outside. Note the low bound also catches a value that is not a number at
 * all, while the high bound does not, so nonsense comes out as -1. */
/* @implements 0x100058D0 d3d BrCarClampUnit */
void BrCarClampUnit(float *pv)
{
    if (!(*pv >= BrK08F0B4))
        *pv = BrK08F0B4;                /* -1.0f */
    if (*pv > BrK08F0A8)
        *pv = BrK08F0A8;                /*  1.0f */
}

/* 0x10005900 */
/* WHAT IT DOES: keeps one of a car's two horizontal position coordinates
 * inside the track's 0-to-2048 world, so a rogue value cannot fling a
 * networked car off the map. */
/* @implements 0x10005900 d3d BrCarClampPosXY */
void BrCarClampPosXY(float *pv)
{
    if (!(*pv >= BrK08F0B0))
        *pv = BrK08F0B0;                /* 0.0f */
    if (*pv > BrK08F0B8)
        *pv = BrK08F0B8;                /* 2048.0f */
}

/* 0x10005930 */
/* WHAT IT DOES: keeps a car's height inside -256 to 256, the range the
 * game's position encoding can represent. */
/* @implements 0x10005930 d3d BrCarClampPosZ */
void BrCarClampPosZ(float *pv)
{
    if (!(*pv >= BrK08F0BC))
        *pv = BrK08F0BC;                /* -256.0f */
    if (*pv > BrK08F0C0)
        *pv = BrK08F0C0;                /* 256.0f */
}

/* =====================================================================
 * 2. Quantisers
 * ===================================================================== */

/* 0x100065A0.  0.5 - v*128, then clamp the INTEGER to [-32, 31]. */
/* WHAT IT DOES: squashes a small signed fraction down to a 6-bit number so
 * it fits in a network packet. Rounding is to the nearest, with halves going
 * up, and the result is pinned inside what 6 bits can hold. The decoder on
 * the other end undoes this exactly. */
/* @implements 0x100065A0 d3d BrFixPackS6Q7Neg */
int32_t BrFixPackS6Q7Neg(float v)
{
    int32_t n = BrFtol(BrFloor((double)BrK08F0D0 - (double)v * (double)BrK08F0CC));

    if (n < -32)
        n = -32;
    if (n > 31)
        n = 31;
    return n;
}

/* 0x100065E0.  0.5 - v*32768, clamp to [-32768, 32767]. */
/* WHAT IT DOES: squashes a signed fraction -- one component of a car's
 * facing -- down to a 16-bit number for the network. Its opposite number in
 * the decoder multiplies by a matching negative scale, so the two sign flips
 * cancel and the value round-trips. */
/* @implements 0x100065E0 d3d BrFixPackS16Q15Neg */
int32_t BrFixPackS16Q15Neg(float v)
{
    int32_t n = BrFtol(BrFloor((double)BrK08F0D0 - (double)v * (double)BrK08F0D4));

    if (n < -32768)
        n = -32768;
    if (n > 32767)
        n = 32767;
    return n;
}

/* 0x10006620.  0.5 - v*(-256/361), clamp to [0, 255]. */
/* WHAT IT DOES: turns an angle in degrees into a single byte for the
 * network, using a scale chosen so a whole 360-degree circle just fills the
 * byte and comes back the same on the other end. */
/* @implements 0x10006620 d3d BrFixPackU8Angle */
int32_t BrFixPackU8Angle(float v)
{
    int32_t n = BrFtol(BrFloor((double)BrK08F0D0 - (double)v * (double)BrK08F0D8));

    if (n < 0)
        n = 0;
    if (n > 255)
        n = 255;
    return n;
}

/* 0x10006660.  0.5 - (v - 400)*(-1/120.63491821), clamp to [0, 63]. */
/* WHAT IT DOES: packs a value that lives between 400 and 8000 into 6 bits
 * for the network by subtracting the base and scaling. What the quantity is
 * is not established here; the range is all this code establishes. */
/* @implements 0x10006660 d3d BrFixPackU8Range */
int32_t BrFixPackU8Range(float v)
{
    double  d = ((double)v - (double)BrK08F0DC) * (double)BrK08F0E0;
    int32_t n = BrFtol(BrFloor((double)BrK08F0D0 - d));

    if (n < 0)
        n = 0;
    if (n > 63)
        n = 63;
    return n;
}

/* 0x100066A0.  Three float compares, no arithmetic; the result is returned in
 * AL, so it is a byte in 0..3. Each test is `less, or unordered`. */
/* WHAT IT DOES: sorts a value into one of four bands using three fixed
 * thresholds, giving a 2-bit code for the network. The bands line up exactly
 * with the four values the decoder produces, so the classification survives
 * the round trip. What the quantity means is not established. */
/* @implements 0x100066A0 d3d BrFixPackLevel */
int32_t BrFixPackLevel(float v)
{
    if (!(v >= BrK08F0CC))              /* < 128.0f, or NaN */
        return 0;
    if (!(v >= BrK08F0E4))              /* < 171.0f */
        return 1;
    if (!(v >= BrK08F0E8))              /* < 213.0f */
        return 2;
    return 3;
}

/* 0x100067B0.  0.5 - v*(-256), clamp to [-32768, 32767]. */
/* WHAT IT DOES: packs a signed value with an eighth-of-a-pixel-style scale
 * of 256 into 16 bits for the network. */
/* @implements 0x100067B0 d3d BrFixPackS16Q8 */
int32_t BrFixPackS16Q8(float v)
{
    int32_t n = BrFtol(BrFloor((double)BrK08F0D0 - (double)v * (double)BrK08F108));

    if (n < -32768)
        n = -32768;
    if (n > 32767)
        n = 32767;
    return n;
}

/* 0x100067F0.  0.5 - v*(-8), clamp to [-128, 127]. */
/* WHAT IT DOES: packs a small signed value at a scale of 8 into a single
 * signed byte for the network. */
/* @implements 0x100067F0 d3d BrFixPackS8Q3 */
int32_t BrFixPackS8Q3(float v)
{
    int32_t n = BrFtol(BrFloor((double)BrK08F0D0 - (double)v * (double)BrK08F10C));

    if (n < -128)
        n = -128;
    if (n > 127)
        n = 127;
    return n;
}

/* =====================================================================
 * 3. Bitstream encoders
 * ===================================================================== */

/* 0x100061A0 */
/* WHAT IT DOES: writes one car's complete state -- facing, position, two
 * more values, wheel or suspension figures, an angle, and a dozen or so
 * on/off flags -- into a compact bit stream to be sent to the other players.
 * Each field is quantised to just enough bits, giving 187 bits in all; eight
 * fields of the car state are deliberately never sent. */
/* @implements 0x100061A0 d3d BrCarStateEncode */
void BrCarStateEncode(BrBitStream *pBs, const BrCarState *pSrc)
{
    /* Orientation: s16 at Q15 (negative scale), keep the high byte. */
    BrBitStreamWriteBits(pBs, BrSar(BrFixPackS16Q15Neg(pSrc->f00), 8), 8);
    BrBitStreamWriteBits(pBs, BrSar(BrFixPackS16Q15Neg(pSrc->f04), 8), 8);
    BrBitStreamWriteBits(pBs, BrSar(BrFixPackS16Q15Neg(pSrc->f08), 8), 8);
    BrBitStreamWriteBits(pBs, BrSar(BrFixPackS16Q15Neg(pSrc->f0C), 8), 8);

    /* Position: u24 at Q13, keep the top 17 bits (a LOGICAL shift). */
    BrBitStreamWriteBits(pBs, (int32_t)((uint32_t)BrFixPackU24Q13(pSrc->f10) >> 7), 17);
    BrBitStreamWriteBits(pBs, (int32_t)((uint32_t)BrFixPackU24Q13(pSrc->f14) >> 7), 17);
    BrBitStreamWriteBits(pBs, BrSar(BrFixPackS16Q7(pSrc->f18), 1), 15);

    BrBitStreamWriteBits(pBs, BrFixPackS16Q8(pSrc->f1C), 16);
    BrBitStreamWriteBits(pBs, BrFixPackS16Q8(pSrc->f20), 16);

    BrBitStreamWriteBits(pBs, BrSar(BrFixPackS8Q3(pSrc->f28), 3), 5);
    BrBitStreamWriteBits(pBs, BrSar(BrFixPackS8Q3(pSrc->f2C), 3), 5);
    BrBitStreamWriteBits(pBs, BrSar(BrFixPackS8Q3(pSrc->f30), 3), 5);
    BrBitStreamWriteBits(pBs, BrSar(BrFixPackS8Q3(pSrc->f34), 4), 4);
    BrBitStreamWriteBits(pBs, BrSar(BrFixPackS6Q7Neg(pSrc->f38), 2), 4);
    /* `and eax,0xff` then `shr eax,4` -- masked to a byte before shifting. */
    BrBitStreamWriteBits(pBs,
        (int32_t)(((uint32_t)BrFixPackU8Angle(pSrc->f3C) & 0xFFu) >> 4), 4);

    BrBitStreamWriteBits(pBs, BrIsNonZero(pSrc->f4C), 1);
    BrBitStreamWriteBits(pBs, BrIsNonZero(pSrc->f50), 1);
    BrBitStreamWriteBits(pBs, BrIsNonZero(pSrc->f54), 1);
    BrBitStreamWriteBits(pBs, BrIsNonZero(pSrc->f58), 1);
    BrBitStreamWriteBits(pBs, BrIsNonZero(pSrc->f6C), 1);
    BrBitStreamWriteBits(pBs, BrIsNonZero(pSrc->f70), 1);
    BrBitStreamWriteBits(pBs, BrIsNonZero(pSrc->f74), 1);

    /* Not masked: the full signed 24-bit value goes out. */
    BrBitStreamWriteBits(pBs, BrFixPackS24Q1(pSrc->f78), 24);
    BrBitStreamWriteBits(pBs, (int32_t)((uint32_t)BrFixPackU8Range(pSrc->f7C) & 0xFFu), 6);
    BrBitStreamWriteBits(pBs, (int32_t)((uint32_t)BrFixPackLevel(pSrc->f80) & 0xFFu), 2);
    BrBitStreamWriteBits(pBs, (int32_t)((uint32_t)BrFixPackLevel(pSrc->f84) & 0xFFu), 2);

    BrBitStreamWriteBits(pBs, BrIsNonZero(pSrc->f88), 1);
    BrBitStreamWriteBits(pBs, BrIsNonZero(pSrc->f8C), 1);
    BrBitStreamWriteBits(pBs, BrIsNonZero(pSrc->f90), 1);
    BrBitStreamWriteBits(pBs, BrIsNonZero(pSrc->f94), 1);
    BrBitStreamWriteBits(pBs, BrIsNonZero(pSrc->f98), 1);
    BrBitStreamWriteBits(pBs, BrIsNonZero(pSrc->f9C), 1);
}

/* The delta prefix shared by all four delta-coded fields.
 *
 * `hiMask` selects the part of the value that is NOT transmitted and `step`
 * is its least significant bit. The four codes are 0, step, 2*step and
 * 3*step, and the final mask keeps exactly the two prefix bits. */
static int32_t BrCarStateDeltaCode(int32_t cur, int32_t ref,
                                   int32_t hiMask, int32_t step)
{
    int32_t code;
    int32_t hiRef, hiCur;

    if (((cur ^ ref) & hiMask) == 0)
        return 0;

    hiRef = ref & hiMask;
    hiCur = cur & hiMask;

    if (hiRef + step == hiCur)
        code = step;                    /* exactly one step up */
    else if (hiRef < hiCur)
        code = 2 * step;                /* more than one step up */
    else
        code = 3 * step;                /* at or below the reference */

    return code & (3 * step);
}

/* 0x10006830 */
/* WHAT IT DOES: writes a car's state as a difference from a previously sent
 * one, so a routine update costs far fewer bits. The facing goes out in
 * full, but position, height and one other field send only their low bits
 * plus a two-bit hint about how far the high part moved. That hint is
 * deliberately coarse: a jump of three or more steps cannot be
 * reconstructed, which is a limitation of the original format, not of this
 * transcription. */
/* @implements 0x10006830 d3d BrCarStateEncodeDelta */
void BrCarStateEncodeDelta(BrBitStream *pBs, const BrCarState *pCur,
                           const BrCarState *pRef)
{
    int32_t cur, ref;

    BrBitStreamWriteBits(pBs, BrSar(BrFixPackS16Q15Neg(pCur->f00), 8), 8);
    BrBitStreamWriteBits(pBs, BrSar(BrFixPackS16Q15Neg(pCur->f04), 8), 8);
    BrBitStreamWriteBits(pBs, BrSar(BrFixPackS16Q15Neg(pCur->f08), 8), 8);
    BrBitStreamWriteBits(pBs, BrSar(BrFixPackS16Q15Neg(pCur->f0C), 8), 8);

    /* f10: 17-bit quantity, 12 bits sent plus a 2-bit code on 0x1F000. */
    ref = (int32_t)((uint32_t)BrFixPackU24Q13(pRef->f10) >> 7);
    cur = (int32_t)((uint32_t)BrFixPackU24Q13(pCur->f10) >> 7);
    BrBitStreamWriteBits(pBs,
        BrCarStateDeltaCode(cur, ref, 0x1F000, 0x1000) | (cur & 0xFFF), 14);

    ref = (int32_t)((uint32_t)BrFixPackU24Q13(pRef->f14) >> 7);
    cur = (int32_t)((uint32_t)BrFixPackU24Q13(pCur->f14) >> 7);
    BrBitStreamWriteBits(pBs,
        BrCarStateDeltaCode(cur, ref, 0x1F000, 0x1000) | (cur & 0xFFF), 14);

    /* f18: 15-bit SIGNED quantity, 9 bits sent plus a code on 0x7E00. */
    ref = BrSar(BrFixPackS16Q7(pRef->f18), 1);
    cur = BrSar(BrFixPackS16Q7(pCur->f18), 1);
    BrBitStreamWriteBits(pBs,
        (cur & 0x1FF) | BrCarStateDeltaCode(cur, ref, 0x7E00, 0x200), 11);

    /* f78: 24-bit signed quantity, 7 bits sent plus a code on 0xFFFF80. */
    ref = BrFixPackS24Q1(pRef->f78);
    cur = BrFixPackS24Q1(pCur->f78);
    BrBitStreamWriteBits(pBs,
        (cur & 0x7F) | BrCarStateDeltaCode(cur, ref, 0xFFFF80, 0x80), 9);

    BrBitStreamWriteBits(pBs, (int32_t)((uint32_t)BrFixPackU8Range(pCur->f7C) & 0xFFu), 6);
    BrBitStreamWriteBits(pBs, (int32_t)((uint32_t)BrFixPackLevel(pCur->f80) & 0xFFu), 2);
    BrBitStreamWriteBits(pBs, (int32_t)((uint32_t)BrFixPackLevel(pCur->f84) & 0xFFu), 2);

    BrBitStreamWriteBits(pBs, BrIsNonZero(pCur->f88), 1);
    BrBitStreamWriteBits(pBs, BrIsNonZero(pCur->f8C), 1);
    BrBitStreamWriteBits(pBs, BrIsNonZero(pCur->f90), 1);
    BrBitStreamWriteBits(pBs, BrIsNonZero(pCur->f94), 1);
    BrBitStreamWriteBits(pBs, BrIsNonZero(pCur->f98), 1);
    BrBitStreamWriteBits(pBs, BrIsNonZero(pCur->f9C), 1);
}

/* =====================================================================
 * 4. The 22-byte fixed record
 * ===================================================================== */

/* Little-endian, byte-wise: the original's stores are plain x86 `mov`s. */
static void BrPutU16(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
}

static void BrPutU32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
    p[2] = (uint8_t)((v >> 16) & 0xFFu);
    p[3] = (uint8_t)((v >> 24) & 0xFFu);
}

static uint32_t BrGetU16(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8);
}

static uint32_t BrGetU32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* 0x10006BD0 */
/* WHAT IT DOES: packs a car's state into a fixed 22-byte record -- a second,
 * simpler wire format that has no bit stream behind it. Flags are hidden in
 * the spare low bits of the quantised numbers, and two bytes are
 * deliberately written on top of the high byte of an earlier value, so the
 * order of the stores matters. Unlike the surrounding code this record is
 * stored least-significant byte first. */
/* @implements 0x10006BD0 d3d BrCarStatePack */
void BrCarStatePack(BrCarPacked *pDst, const BrCarState *pSrc)
{
    uint8_t *b = pDst->b;

    /* Orientation, low bit stolen for a flag. */
    BrPutU16(b + 0x00, ((uint32_t)BrFixPackS16Q15Neg(pSrc->f00) & 0xFFFEu)
                       | (uint32_t)BrIsNonZero(pSrc->f8C));
    BrPutU16(b + 0x02, ((uint32_t)BrFixPackS16Q15Neg(pSrc->f04) & 0xFFFEu)
                       | (uint32_t)BrIsNonZero(pSrc->f90));
    BrPutU16(b + 0x04, ((uint32_t)BrFixPackS16Q15Neg(pSrc->f08) & 0xFFFEu)
                       | (uint32_t)BrIsNonZero(pSrc->f94));
    BrPutU16(b + 0x06, ((uint32_t)BrFixPackS16Q15Neg(pSrc->f0C) & 0xFFFEu)
                       | (uint32_t)BrIsNonZero(pSrc->f98));

    /* Position axis 1: three low bits are flags, the top byte is reused
     * below by the b[0x0B] store. */
    BrPutU32(b + 0x08,
             ((uint32_t)BrFixPackU24Q13(pSrc->f10) & 0xFFFFF8u)
             | ((uint32_t)((BrIsNonZero(pSrc->f9C) * 2)
                           | BrIsNonZero(pSrc->f88)) << 1)
             | (uint32_t)BrIsNonZero(pSrc->f6C));

    /* Position axis 2: two low bits are flags. */
    BrPutU32(b + 0x0C,
             ((uint32_t)BrFixPackU24Q13(pSrc->f14) & 0xFFFFFCu)
             | ((uint32_t)BrIsNonZero(pSrc->f70) << 1)
             | (uint32_t)BrIsNonZero(pSrc->f74));

    BrPutU16(b + 0x10, (uint32_t)BrFixPackS16Q7(pSrc->f18) & 0xFFFFu);
    b[0x12] = (uint8_t)((uint32_t)BrFixPackS8Q3(pSrc->f34) & 0xFFu);
    b[0x13] = (uint8_t)(((uint32_t)BrFixPackS6Q7Neg(pSrc->f38) & 0x3Fu)
                        | ((uint32_t)BrFixPackLevel(pSrc->f80) << 6));

    /* Overwrites the high byte of the dword at 0x08, which the mask above
     * left clear. Order is load-bearing. */
    b[0x0B] = (uint8_t)((uint32_t)BrFixPackU8Angle(pSrc->f3C) & 0xFFu);

    b[0x14] = (uint8_t)((BrIsNonZero(pSrc->f4C) ? 0x80u : 0u)
                        | (((uint32_t)BrFtol((double)pSrc->f5C) & 7u) << 4)
                        | (BrIsNonZero(pSrc->f50) ? 8u : 0u)
                        | ((uint32_t)BrFtol((double)pSrc->f60) & 7u));
    b[0x15] = (uint8_t)((BrIsNonZero(pSrc->f54) ? 0x80u : 0u)
                        | (((uint32_t)BrFtol((double)pSrc->f64) & 7u) << 4)
                        | (BrIsNonZero(pSrc->f58) ? 8u : 0u)
                        | ((uint32_t)BrFtol((double)pSrc->f68) & 7u));

    /* Overwrites the high byte of the dword at 0x0C. */
    b[0x0F] = (uint8_t)(((uint32_t)BrFixPackU8Range(pSrc->f7C) & 0x3Fu)
                        | ((uint32_t)BrFixPackLevel(pSrc->f84) << 6));
}

/* 0x10007730 */
void BrCarStateUnpack(BrCarState *pDst, const BrCarPacked *pSrc)
{
    const uint8_t *b = pSrc->b;
    float          angle;

    pDst->f00 = BrFixUnpackS16Q15Neg((int32_t)BrGetU16(b + 0x00));
    pDst->f04 = BrFixUnpackS16Q15Neg((int32_t)BrGetU16(b + 0x02));
    pDst->f08 = BrFixUnpackS16Q15Neg((int32_t)BrGetU16(b + 0x04));
    pDst->f0C = BrFixUnpackS16Q15Neg((int32_t)BrGetU16(b + 0x06));

    pDst->f10 = BrFixUnpackU32Q13(BrGetU32(b + 0x08) & 0xFFFFFFu);
    pDst->f14 = BrFixUnpackU32Q13(BrGetU32(b + 0x0C) & 0xFFFFFFu);
    pDst->f18 = BrFixUnpackS16Q7((int32_t)BrGetU16(b + 0x10));

    pDst->f34 = BrFixUnpackS8Q3((int32_t)b[0x12]);
    pDst->f38 = BrFixUnpackS6Q7Neg((int32_t)(b[0x13] & 0x3Fu));
    pDst->f80 = BrFixUnpackLevel((int32_t)((b[0x13] >> 6) & 3u));

    /* The angle is stored twice, and again 35 degrees on with a single
     * conditional wrap -- one subtraction, not a modulo, so an input beyond
     * 720 would not be brought back into range. It cannot be: the byte
     * decodes to at most 359.6. */
    angle = BrFixUnpackU8Angle((int32_t)b[0x0B]);
    pDst->f40 = angle;
    pDst->f3C = angle;
    angle = angle - BrK08F110;          /* constant is -35.0f, so this adds 35 */
    if (angle >= BrK08F114)             /* `fcom`/C0: NaN would NOT subtract */
        angle = angle - BrK08F114;      /* 360.0f */
    pDst->f48 = angle;
    pDst->f44 = angle;

    pDst->f4C = (float)(int32_t)((b[0x14] >> 7) & 0xFFu);
    pDst->f5C = (float)(int32_t)((b[0x14] >> 4) & 7u);
    pDst->f50 = (float)(int32_t)((b[0x14] >> 3) & 1u);
    pDst->f60 = (float)(int32_t)(b[0x14] & 7u);

    pDst->f54 = (float)(int32_t)((b[0x15] >> 7) & 0xFFu);
    pDst->f64 = (float)(int32_t)((b[0x15] >> 4) & 7u);
    pDst->f58 = (float)(int32_t)((b[0x15] >> 3) & 1u);
    pDst->f68 = (float)(int32_t)(b[0x15] & 7u);

    /* GOTCHA: 128.0f, not 1.0f -- except f70 and f74, which really are 1.0f. */
    pDst->f6C = (b[0x08] & 1u) ? BrK08F0CC : 0.0f;
    pDst->f70 = (b[0x0C] & 2u) ? BrK08F118 : 0.0f;
    pDst->f74 = (b[0x0C] & 1u) ? BrK08F118 : 0.0f;

    pDst->f7C = BrFixUnpackU8Range((int32_t)(b[0x0F] & 0x3Fu));
    pDst->f84 = BrFixUnpackLevel((int32_t)(b[0x0F] >> 6));

    pDst->f88 = (b[0x08] & 2u) ? BrK08F0CC : 0.0f;
    pDst->f8C = (b[0x00] & 1u) ? BrK08F0CC : 0.0f;
    pDst->f90 = (b[0x02] & 1u) ? BrK08F0CC : 0.0f;
    pDst->f94 = (b[0x04] & 1u) ? BrK08F0CC : 0.0f;
    pDst->f98 = (b[0x06] & 1u) ? BrK08F0CC : 0.0f;
    pDst->f9C = (b[0x08] & 4u) ? BrK08F0CC : 0.0f;
}

/* =====================================================================
 * 5. Player table accessors
 * ===================================================================== */

/* 0x10005470.  BR_ENTITY_STRIDE (0x2B68) comes from slice1_09.h.
 *
 * NOTE: the base here is 0x10ACEDB0, which is NOT the 0x10ACDEA8 that pass
 * 09's entity helpers use -- the two differ by 0xF08, not by a whole number of
 * records. Either this walks a different array or it starts 0xF08 into the
 * record; the stride and the "first dword non-zero" test are all this code
 * establishes, so the base stays a parameter. */
/* WHAT IT DOES: counts how many entries in a table of cars or other world
 * objects are in use, by checking each record's first word for a non-zero
 * value. */
/* @implements 0x10005470 d3d BrEntityCountActive */
uint32_t BrEntityCountActive(const void *pvRecords, int32_t cRecords)
{
    const unsigned char *p = (const unsigned char *)pvRecords;
    uint32_t             n = 0;
    int32_t              i;

    for (i = 0; i < cRecords; ++i) {
        uint32_t first;
        memcpy(&first, p, sizeof first);        /* byte order is irrelevant */
        if (first != 0)
            ++n;
        p += BR_ENTITY_STRIDE;
    }
    return n;
}

/* 0x10005D40, and 0x10005D90 over the other pair of globals. */
/* WHAT IT DOES: takes the top entry off one of the networking code's small
 * stacks of free numbers, locking it first so another thread cannot take the
 * same one. A negative index means the stack is empty, and the caller gets
 * -1. Note the top entry sits at the index itself, not one below it. */
/* @implements 0x10005D40 d3d BrNetStackPop */
int32_t BrNetStackPop(void *hMutex, int32_t *paStack, int32_t *piTop)
{
    int32_t v;

    BrNetMutexLock(hMutex);
    if (*piTop < 0) {                   /* `jl` -- negative means empty */
        BrNetMutexUnlock(hMutex);
        return -1;
    }
    v = paStack[*piTop];                /* the top element is AT the index */
    *piTop = *piTop - 1;
    BrNetMutexUnlock(hMutex);
    return v;
}

/* 0x10005E40 */
/* WHAT IT DOES: reads one entry out of a networking table with the table's
 * lock held. There is no bounds check, here or in the original, so a bad
 * index reads whatever is next in memory. */
/* @implements 0x10005E40 d3d BrNetGetA102212D0 */
int32_t BrNetGetA102212D0(BrNetState *pNet, int32_t i)
{
    int32_t v;

    BrNetMutexLock(pNet->h1022AF28);
    v = pNet->a102212D0[i];             /* no range check in the original */
    BrNetMutexUnlock(pNet->h1022AF28);
    return v;
}

/* 0x10005DE0 */
/* WHAT IT DOES: reads four small pieces of one player's network slot at
 * once, under that slot's lock: one number returned directly, and three
 * bytes handed back through pointers. */
/* @implements 0x10005DE0 d3d BrNetSlotGetF030 */
int32_t BrNetSlotGetF030(BrNetState *pNet, int32_t slot,
                         uint8_t *pb34, uint8_t *pb35, uint8_t *pb36)
{
    BrNetSlot *pSlot = &pNet->aSlots[slot];
    int32_t    v;
    uint32_t   packed;

    BrNetMutexLock(pSlot->hMutex);
    v      = pSlot->f030;
    packed = (uint32_t)pSlot->f034;
    /* DEVIATION: the original reads three separate bytes at +0x34..+0x36.
     * A later pass models that region as one int32, so the bytes come out of it
     * in x86 (little-endian) order. */
    *pb34 = (uint8_t)(packed & 0xFFu);
    *pb35 = (uint8_t)((packed >> 8) & 0xFFu);
    *pb36 = (uint8_t)((packed >> 16) & 0xFFu);
    BrNetMutexUnlock(pSlot->hMutex);
    return v;
}

/* 0x10005EE0 */
/* WHAT IT DOES: stores a player's name into their network slot, under that
 * slot's lock. The original copied without limit; this version truncates to
 * the field's size. */
/* @implements 0x10005EE0 d3d BrNetSlotSetName */
void BrNetSlotSetName(BrNetState *pNet, int32_t slot, const char *pszName)
{
    BrNetSlot *pSlot = &pNet->aSlots[slot];
    size_t     cb;

    BrNetMutexLock(pSlot->hMutex);
    /* DEVIATION: the original is an unbounded inlined strcpy. */
    cb = strlen(pszName);
    if (cb > (size_t)(BR_NET_SLOT_NAME - 1))
        cb = (size_t)(BR_NET_SLOT_NAME - 1);
    memcpy(pSlot->f570, pszName, cb);
    pSlot->f570[cb] = '\0';
    BrNetMutexUnlock(pSlot->hMutex);
}

/* 0x10005F40 */
/* WHAT IT DOES: reads a small field of a player's slot, keeps its low six
 * bits and subtracts four, never returning less than zero. Beware that it
 * locks the slot and then calls another routine that locks the same slot
 * again -- harmless on Windows, but a deadlock if the lock is replaced with
 * a non-recursive one. */
/* @implements 0x10005F40 d3d BrNetSlotGetF02CBiased */
int32_t BrNetSlotGetF02CBiased(BrNetState *pNet, int32_t slot)
{
    BrNetSlot *pSlot = &pNet->aSlots[slot];
    int32_t    v;

    BrNetMutexLock(pSlot->hMutex);
    /* Re-enters the same mutex through 0x10004A10; see the header. */
    v = (BrNetSlotGetF02C(pNet, slot) & 0x3F) - 4;
    BrNetMutexUnlock(pSlot->hMutex);

    /* `setle`/`dec`/`and`: max(0, v), branchlessly. */
    return (v > 0) ? v : 0;
}

/* 0x10005F90 */
/* WHAT IT DOES: reads one number out of a player's slot under that slot's
 * lock and reports it, with negative values reported as zero. Note the
 * flooring happens after the lock is released. */
/* @implements 0x10005F90 d3d BrNetSlotGetF974 */
int32_t BrNetSlotGetF974(BrNetState *pNet, int32_t slot)
{
    BrNetSlot *pSlot = &pNet->aSlots[slot];
    int32_t    v;

    BrNetMutexLock(pSlot->hMutex);
    v = pSlot->f974;
    BrNetMutexUnlock(pSlot->hMutex);

    if (v < 0)                          /* clamped AFTER the unlock */
        v = 0;
    return v;
}

/* 0x10006090 */
/* WHAT IT DOES: raises a networking flag under its lock. What the flag
 * signals is not established here. */
/* @implements 0x10006090 d3d BrNetSetF10220DD0 */
void BrNetSetF10220DD0(BrNetState *pNet)
{
    BrNetMutexLock(pNet->h1022131C);
    pNet->f10220DD0 = 1;
    BrNetMutexUnlock(pNet->h1022131C);
}

/* 0x100060C0 */
/* WHAT IT DOES: lowers the same networking flag its neighbour raises, under
 * the same lock. */
/* @implements 0x100060C0 d3d BrNetClearF10220DD0 */
void BrNetClearF10220DD0(BrNetState *pNet)
{
    BrNetMutexLock(pNet->h1022131C);
    pNet->f10220DD0 = 0;
    BrNetMutexUnlock(pNet->h1022131C);
}

/* 0x10006160 */
/* WHAT IT DOES: raises a flag if the deadline the networking code is waiting
 * on has passed. The comparison is unsigned, so the disarmed value of -1
 * reads as an enormous future time and the flag never fires until a real
 * deadline is set -- that is a disarmed timer, not an expired one. */
/* @implements 0x10006160 d3d BrNetCheckDeadline */
void BrNetCheckDeadline(BrNetState *pNet, uint32_t nowTicks, int32_t *pfFlag)
{
    BrNetMutexLock(pNet->h10220CEC);
    if (nowTicks >= (uint32_t)pNet->f1022AF00)      /* `jb` -- unsigned */
        *pfFlag = 1;
    BrNetMutexUnlock(pNet->h10220CEC);
}

/* =====================================================================
 * 6. Dead-reckoning (0x100054A0)
 * ===================================================================== */

/* The three interpolation blocks in the original are three copies of the same
 * code reached from three different bookkeeping paths; factored out here. */
static void BrNetSlotExtrapolate(BrCarState *pDst, const BrCarState *aRec,
                                 int32_t iBest, int32_t iSecond,
                                 uint32_t seqBest, uint32_t dt,
                                 uint32_t nowTicks)
{
    int32_t age = (int32_t)(nowTicks - seqBest);
    float   t, h1, h2;
    float   av[3];

    if (age > 6)                        /* `cmp eax,6 / jle` -- signed */
        age = 6;

    /* `fild` of (age + dt) divided by `fidiv` of dt: both signed 32-bit.
     * The sum is formed unsigned so the port has no signed overflow where the
     * original merely wrapped. */
    t = (float)((double)(int32_t)((uint32_t)age + dt) / (double)(int32_t)dt);

    BrCarStateLerp(pDst, t, &aRec[iSecond], &aRec[iBest]);

    /* Both probes are fed a three-float copy, exactly as the original does. */
    av[0] = aRec[iBest].f68;
    av[1] = aRec[iBest].f6C;
    av[2] = aRec[iBest].f70;
    h1 = BrProbe1006F310(av);

    av[0] = pDst->f10;
    av[1] = pDst->f14;
    av[2] = pDst->f18;
    h2 = BrProbe1006F310(av);

    pDst->f18 = (h1 - h2) + pDst->f18;
}

/* 0x100054A0 */
int BrNetSlotPredict(BrCarState *pDst, int32_t slot, BrNetState *pNet,
                     void *hGlobal, int32_t localSlot, uint32_t nowTicks)
{
    BrNetSlot  *pSlot = &pNet->aSlots[slot];
    BrCarState *aRec  = (BrCarState *)(void *)pSlot->f058;
    float       sum;
    int32_t     k;
    int32_t     iBest   = 0;
    int32_t     iSecond = 0;
    uint32_t    uBest   = 0;
    uint32_t    uSecond = 0;
    uint32_t    dt;

    /* DEVIATION: the original takes both handles in one atomic
     * WaitForMultipleObjects(..., bWaitAll = TRUE). Two sequential locks are
     * not the same thing under contention; the release order below (slot
     * first, then the global) is the original's. */
    BrNetMutexLock(hGlobal);
    BrNetMutexLock(pSlot->hMutex);

    if (slot == localSlot)
        goto finish;                    /* still clamps and normalises pDst */

    if (pSlot->f558 < 2) {
        pDst->f7C = BrK08F0DC;          /* 400.0f */
        BrNetMutexUnlock(pSlot->hMutex);
        BrNetMutexUnlock(hGlobal);
        return 0;
    }

    /* Highest sequence number among the occupied samples. Unsigned compare,
     * and both the index and the running maximum start at 0. */
    for (k = 0; k < 8; ++k) {
        if (pSlot->f038[k] != 0) {
            uint32_t seq = (uint32_t)pSlot->f00C[k];
            if (seq > uBest) {
                iBest = k;
                uBest = seq;
            }
        }
    }

    /* Second highest, skipping the winner. */
    for (k = 0; k < 8; ++k) {
        if (pSlot->f038[k] != 0) {
            uint32_t seq = (uint32_t)pSlot->f00C[k];
            if (seq > uSecond && k != iBest) {
                iSecond = k;
                uSecond = seq;
            }
        }
    }

    dt = (uint32_t)pSlot->f00C[iBest] - (uint32_t)pSlot->f00C[iSecond];

    if (pSlot->f560 == iBest) {
        if (pSlot->f568 < 0xF) {
            pSlot->f568 += 1;
            pSlot->f564 += 1;
        }
        if (dt == 0) {
            /* `rep movsd` of 0x28 dwords out of record[f560]. */
            memcpy(pDst, &aRec[pSlot->f560], sizeof *pDst);
            goto finish;
        }
        BrNetSlotExtrapolate(pDst, aRec, iBest, iSecond,
                             (uint32_t)pSlot->f00C[iBest], dt, nowTicks);
    } else {
        uint32_t next = (uint32_t)pSlot->f564 + 1u;

        pSlot->f560 = iBest;

        if ((uint32_t)pSlot->f00C[iBest] >= next    /* `jae` -- unsigned */
            || pSlot->f56C >= 0x14                  /* `jge` -- signed   */
            || dt == 0) {
            pSlot->f568 = 0;
            pSlot->f56C = 0;
            pSlot->f564 = pSlot->f00C[iBest];
        } else {
            pSlot->f568  = 1;
            pSlot->f56C += 1;
            pSlot->f564  = (int32_t)next;
        }
        BrNetSlotExtrapolate(pDst, aRec, iBest, iSecond,
                             (uint32_t)pSlot->f00C[iBest], dt, nowTicks);
    }

finish:
    BrNetMutexUnlock(pSlot->hMutex);
    BrNetMutexUnlock(hGlobal);

    BrCarClampUnit(&pDst->f00);
    BrCarClampUnit(&pDst->f04);
    BrCarClampUnit(&pDst->f08);
    BrCarClampUnit(&pDst->f0C);

    /* Summed in the order f00, f04, f0C, f08 -- the third and fourth are
     * swapped in the original and float addition is not associative. */
    sum = pDst->f00 + pDst->f04;
    sum = sum + pDst->f0C;
    sum = sum + pDst->f08;

    if (!(sum < 0.0f) && !(sum > 0.0f)) {       /* equal to 0, or unordered */
        pDst->f00 = 1.0f;
        pDst->f04 = 0.0f;
        pDst->f08 = 0.0f;
        pDst->f0C = 0.0f;
    } else {
        /* BrCarState opens with the same four floats as BrVec4. */
        BrVec4Normalise((BrVec4 *)(void *)pDst);
    }

    BrCarClampPosXY(&pDst->f10);
    BrCarClampPosXY(&pDst->f14);
    BrCarClampPosZ(&pDst->f18);
    return 1;
}

/* =====================================================================
 * 7. Key-record cache
 * ===================================================================== */

/* 0x10008670 */
/* WHAT IT DOES: looks through a cache for the record whose 64-byte key
 * matches the one asked for, and reports its position, or -1 if there is no
 * match. Only the middle of each record takes part in the comparison; the
 * first few words are payload the search ignores. What the cache holds is
 * not established here. */
/* @implements 0x10008670 d3d BrKeyCacheFind */
int32_t BrKeyCacheFind(const BrKeyCache *pCache, const int32_t aKey[16])
{
    int32_t i;

    for (i = 0; i < pCache->cEntries; ++i) {
        int32_t j;
        int     fMatch = 1;

        for (j = 0; j < 16; ++j) {
            if (pCache->aEntries[i].aKey[j] != aKey[j]) {
                fMatch = 0;
                break;
            }
        }
        if (fMatch)
            return i;
    }
    return -1;                          /* `or eax,0xffffffff` */
}

/* 0x10008970 */
/* WHAT IT DOES: empties that cache: closes the file it was reading from,
 * frees the records, and zeroes the bookkeeping, leaving only the object's
 * first two words alone. */
/* @implements 0x10008970 d3d BrKeyCacheReset */
void BrKeyCacheReset(BrKeyCache *pCache)
{
    if (pCache->pFile != NULL)
        fclose(pCache->pFile);          /* 0x1007CD50 */
    if (pCache->aEntries != NULL)
        free(pCache->aEntries);         /* 0x1007DE40, operator delete */

    pCache->aEntries = NULL;
    pCache->pFile    = NULL;
    pCache->f420     = 0;

    pCache->f008    = 0;
    pCache->f00C    = 0;
    pCache->cEntries = 0;
    pCache->f014    = 0;

    memset(pCache->a020, 0, sizeof pCache->a020);
    /* The `rep stosd` run starts at +0x008, so the vtable slot and +0x004
     * are the only fields left alone. */
}

/* =====================================================================
 * 8. POD archive writer
 * ===================================================================== */

#define BR_POD_DIR_STRIDE 76

/* 0x100089C0 */
/* WHAT IT DOES: starts writing a POD archive -- the game's own bundle format
 * for its data files. It opens the file, leaves room at the front for a
 * header it can only fill in at the end, and clears the directory it will
 * build up as members are added. */
/* @implements 0x100089C0 d3d BrPodWriteOpen */
int BrPodWriteOpen(BrPodWriter *pW, const char *pszPath)
{
    /* DEVIATION: the original opens through the stream object at +4
     * (0x10008BE0) and seeks with 0x1007C910. */
    pW->pFile = fopen(pszPath, "wb");
    if (pW->pFile == NULL)
        return 1;

    if (fseek(pW->pFile, 0x10, SEEK_SET) != 0) {
        fclose(pW->pFile);
        pW->pFile = NULL;
        return 1;
    }

    memset(pW->aEntries, 0, sizeof pW->aEntries);       /* 0x13000 dwords */
    pW->cEntries = 0;
    return 0;
}

/* 0x10008A00 */
/* WHAT IT DOES: adds one member file to the archive being written: notes
 * where in the file the data will sit, writes the data, and records the name
 * (uppercased) and size in the directory. An over-long name is complained
 * about and then used anyway, and the directory is capped here, which the
 * original did not do. */
/* @implements 0x10008A00 d3d BrPodWriteAdd */
void BrPodWriteAdd(BrPodWriter *pW, const char *pszName,
                   const void *pvData, uint32_t cbData,
                   uint8_t b08, uint8_t b09)
{
    BrPodWriteEntry *pEnt;
    const void      *pNul;
    size_t           cbName;
    size_t           i;
    long             off;

    if (pW->cEntries >= BR_POD_WRITER_MAX)
        return;                         /* DEVIATION: the original never checks */

    pEnt = &pW->aEntries[pW->cEntries];
    pW->cEntries += 1;                  /* the count moves BEFORE the write */
    pEnt->offData = 0;

    BrPodWriterMakeName(pW, pszName, pEnt->szName);

    /* DEVIATION: the original runs a `repne scasb` off the end of the 64-byte
     * field when the name fills it completely. Bounded here. */
    pNul   = memchr(pEnt->szName, '\0', sizeof pEnt->szName);
    cbName = (pNul != NULL)
             ? (size_t)((const char *)pNul - pEnt->szName)
             : sizeof pEnt->szName;

    if (cbName > 0x40)
        fprintf(stderr, "Add: Name is too long to be a pod name.\n");
    /* ...and then adds it anyway, exactly as the original does. */

    for (i = 0; i < cbName; ++i) {      /* 0x1007F240 == _strupr */
        char c = pEnt->szName[i];
        if (c >= 'a' && c <= 'z')
            pEnt->szName[i] = (char)(c - 'a' + 'A');
    }

    off = ftell(pW->pFile);                     /* 0x1007C9F0 */
    pEnt->offData = (uint32_t)off;
    pEnt->cbData  = cbData;
    pEnt->b08     = b08;
    pEnt->b09     = b09;

    if (cbData != 0)
        fwrite(pvData, 1, (size_t)cbData, pW->pFile);   /* 0x10008C90 */
}

/* 0x10008AA0 */
/* WHAT IT DOES: finishes the archive: writes the directory of members at the
 * end, rewinds to the front to fill in the header with the magic word,
 * member count and directory position, and closes the file. */
/* @implements 0x10008AA0 d3d BrPodWriteClose */
void BrPodWriteClose(BrPodWriter *pW)
{
    uint8_t  aRec[BR_POD_DIR_STRIDE];
    uint8_t  aHdr[16];
    uint32_t i;
    long     offDir;

    offDir = ftell(pW->pFile);          /* recorded BEFORE the directory */

    /* DEVIATION: the original writes the in-memory directory array straight
     * out with one call, which is only the on-disk layout because the host is
     * little-endian. Serialised field by field here. */
    for (i = 0; i < pW->cEntries; ++i) {
        const BrPodWriteEntry *pEnt = &pW->aEntries[i];

        memset(aRec, 0, sizeof aRec);
        BrPutU32(aRec + 0x00, pEnt->offData);
        BrPutU32(aRec + 0x04, pEnt->cbData);
        aRec[0x08] = pEnt->b08;
        aRec[0x09] = pEnt->b09;
        aRec[0x0A] = pEnt->b0A;
        aRec[0x0B] = pEnt->b0B;
        memcpy(aRec + 0x0C, pEnt->szName, sizeof pEnt->szName);
        fwrite(aRec, 1, sizeof aRec, pW->pFile);
    }

    fseek(pW->pFile, 0, SEEK_SET);

    aHdr[0] = 'P';
    aHdr[1] = 'O';
    aHdr[2] = 'D';
    aHdr[3] = 0;                        /* DEVIATION: never assigned originally */
    BrPutU32(aHdr + 4, BR_POD_WRITER_MAGIC_EXTRA);
    BrPutU32(aHdr + 8, pW->cEntries);
    BrPutU32(aHdr + 12, (uint32_t)offDir);
    fwrite(aHdr, 1, sizeof aHdr, pW->pFile);

    fclose(pW->pFile);                  /* 0x1007CD50 */
    pW->pFile = NULL;
}
