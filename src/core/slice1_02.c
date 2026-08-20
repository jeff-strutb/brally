/* slice1_02.c -- BRD3D.dll 0x100049C0-0x100079E0. See slice1_02.h. */

#include "slice1_02.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/* =====================================================================
 * Shared primitives the original reaches through the CRT
 * ===================================================================== */

/* 0x1007DB00 is MSVC's `floor`: it forces the x87 control word to the value at
 * 0x100BD8E0 (0x173F -- RC = 01, round toward -infinity), runs `frndint`, and
 * restores. Verified rather than assumed, because with RC = 11 the same code
 * would be `trunc` and every rounding here would change. */
#define BrFloor(d) floor(d)

/* 0x1007C8A0 is MSVC's __ftol: set RC to chop, `fistp qword`, return the LOW
 * dword of the 64-bit result.
 *
 * DEVIATION: a plain `(int32_t)d` is undefined in C once d leaves int32 range,
 * and the original is well-defined there -- x87 stores the "integer
 * indefinite" 0x8000000000000000 when the value does not fit in 64 bits, whose
 * low dword is 0, and simply truncates the high dword away otherwise. Both are
 * reproduced explicitly so the clamps downstream see what they saw before. */
/* WHAT IT DOES: the compiler's own float-to-integer conversion, transcribed
 * because the game's rounding depends on exactly how it behaves at the
 * edges. It chops toward zero and keeps only the bottom half of the result,
 * and a value too big to convert at all comes out as zero rather than as
 * garbage. */
/* @implements 0x1007C8A0 d3d BrFtol */
static int32_t BrFtol(double d)
{
    int64_t  wide;
    uint32_t lo;
    int32_t  out;

    if (!(d >= -9223372036854775808.0) || !(d < 9223372036854775808.0))
        return 0;                       /* indefinite -> low dword is zero */

    wide = (int64_t)d;                  /* truncates toward zero, like chop */
    lo   = (uint32_t)((uint64_t)wide & 0xFFFFFFFFu);
    memcpy(&out, &lo, sizeof out);      /* reinterpret, do not convert */
    return out;
}

/* The original sign-extends with `movsx`; spelled out here so the result does
 * not depend on implementation-defined narrowing conversions. */
static int32_t BrSext8(uint32_t v)
{
    int32_t x = (int32_t)(v & 0xFFu);
    return (x & 0x80) ? x - 0x100 : x;
}

static int32_t BrSext16(uint32_t v)
{
    int32_t x = (int32_t)(v & 0xFFFFu);
    return (x & 0x8000) ? x - 0x10000 : x;
}

static int32_t BrSext24(uint32_t v)
{
    int32_t x = (int32_t)(v & 0xFFFFFFu);
    return (x & 0x800000) ? x - 0x1000000 : x;
}

/* =====================================================================
 * 1. Fixed-point codecs
 * ===================================================================== */

/* 0x100066E0.  Constants: 0x1008F0EC = -8192.0f, 0x1008F0D0 = 0.5f,
 * 0x1008F0F0 = 0.0 (double), 0x1008F0F8 = 16777215.0 (double).
 *
 * The clamp happens on the FLOAT, before __ftol, so the int conversion can
 * never overflow here. The low guard is written as "C0 set", which x87 also
 * sets for unordered, so a NaN input lands on 0 -- kept via !(d >= 0.0). */
/* WHAT IT DOES: packs a position coordinate into a 24-bit whole number for
 * the network, rounding to nearest with halves going up and pinning the
 * result inside what 24 bits can hold. The pinning happens before the
 * conversion, so it can never overflow. */
/* @implements 0x100066E0 d3d BrFixPackU24Q13 */
int32_t BrFixPackU24Q13(float v)
{
    double d = BrFloor(0.5 - (double)v * -8192.0);

    if (!(d >= 0.0))
        d = 0.0;
    if (d > 16777215.0)
        d = 16777215.0;
    return BrFtol(d);
}

/* 0x10006730.  0x1008F100 = -2.0f. Clamp is on the INTEGER, after __ftol, and
 * with signed compares (jge/jle). */
/* WHAT IT DOES: packs a signed value at half-unit resolution into 24 bits
 * for the network, pinned to that range. */
/* @implements 0x10006730 d3d BrFixPackS24Q1 */
int32_t BrFixPackS24Q1(float v)
{
    int32_t r = (int32_t)BrFloor(0.5f - v * -2.0f);

    if (r < -8388608)
        r = -8388608;
    if (r > 8388607)
        r = 8388607;
    return r;
}

/* 0x10006770.  0x1008F104 = -128.0f. Clamp is on the integer, as above. */
/* WHAT IT DOES: packs a signed height into 16 bits at 1/128 resolution for
 * the network, pinned to that range. */
/* @implements 0x10006770 d3d BrFixPackS16Q7 */
int32_t BrFixPackS16Q7(float v)
{
    int32_t r = (int32_t)BrFloor(0.5f - v * -128.0f);

    if (r < -32768)
        r = -32768;
    if (r > 32767)
        r = 32767;
    return r;
}

/* 0x10007250.  Scale 0x1008F11C = -0.0078125f (= -1/128).
 * The sign test is on bit 5 of the low byte; the original ORs in 0xC0 to
 * extend, or ANDs with 0x3F when clear -- both branches then `movsx al`. */
/* WHAT IT DOES: unpacks a small signed fraction that was sent in 6 bits,
 * sign-extending it by hand before scaling. This is the exact inverse of the
 * matching packer. */
/* @implements 0x10007250 d3d BrFixUnpackS6Q7Neg */
float BrFixUnpackS6Q7Neg(int32_t v)
{
    unsigned char b = (unsigned char)v;
    int32_t       s;

    if (b & 0x20) {
        s = (signed char)(unsigned char)(b | 0xC0u);
        return (float)(s * -0.0078125f);
    }
    s = (signed char)(unsigned char)(b & 0x3Fu);
    return (float)(s * -0.0078125f);
}

/* 0x10007280.  Scale 0x1008F120 = -3.0517578125e-05f (= -1/32768). */
/* WHAT IT DOES: unpacks one component of a car's facing direction from the
 * 16 bits it was sent in. The scale is negative, which cancels the negative
 * scale the packer used, so the value comes back unchanged. */
/* @implements 0x10007280 d3d BrFixUnpackS16Q15Neg */
float BrFixUnpackS16Q15Neg(int32_t v)
{
    return (float)((int16_t)v * -0.000030517578125f);
}

/* 0x100072A0.  Scale 0x1008F124 = 1.41015625f (= 361/256). */
/* WHAT IT DOES: turns a byte back into an angle in degrees, using a scale
 * chosen so the byte covers a full circle. */
/* @implements 0x100072A0 d3d BrFixUnpackU8Angle */
float BrFixUnpackU8Angle(int32_t v)
{
    return (float)((v & 0xFF) * 1.41015625f);
}

/* 0x100072C0.  0x1008F128 = -120.63491821289062f, 0x1008F0DC = 400.0f, and
 * the original's `fsubr` makes that 400 - (v * -120.63...), i.e. a rising
 * ramp. 63 * 120.63491821289062 is 7600 to float precision, so with the 6-bit
 * field its only caller passes, the range is [400, 8000]. */
/* WHAT IT DOES: turns a 6-bit code back into a value between 400 and 8000,
 * the inverse of the matching packer. What the quantity represents is not
 * established here. */
/* @implements 0x100072C0 d3d BrFixUnpackU8Range */
float BrFixUnpackU8Range(int32_t v)
{
    return 400.0f - (v & 0xFF) * -120.63491821289062f;
}

/* 0x100072E0.  A chain of byte compares, not a table lookup: anything above 2
 * falls through to 255. Constants 0x1008F0C8 = 0.0f, 0x1008F12C = 170.0f,
 * 0x1008F130 = 212.0f, 0x1008F134 = 255.0f. */
/* WHAT IT DOES: turns a 2-bit code back into one of four fixed values: 0,
 * 170, 212 or 255. It is written as a chain of comparisons rather than a
 * table, so any code above 2 gives the top value. */
/* @implements 0x100072E0 d3d BrFixUnpackLevel */
float BrFixUnpackLevel(int32_t v)
{
    unsigned char b = (unsigned char)v;

    if (b == 0)
        return 0.0f;
    if (b == 1)
        return 170.0f;
    if (b == 2)
        return 212.0f;
    return 255.0f;
}

/* 0x10007310.  Scale 0x1008F138 = 0.0001220703125f (= 1/8192).
 * `fild qword` over {v, 0} -- the value is UNSIGNED. */
/* WHAT IT DOES: turns a packed whole number back into a position coordinate
 * by scaling it down. The packed value is treated as unsigned. */
/* @implements 0x10007310 d3d BrFixUnpackU32Q13 */
float BrFixUnpackU32Q13(uint32_t v)
{
    return (float)(v * 0.0001220703125f);
}

/* 0x10007340.  Scale 0x1008F0D0 = 0.5f; sign bit is bit 23. */
/* WHAT IT DOES: turns a packed 24-bit signed value back into a float at
 * half-unit resolution. */
/* @implements 0x10007340 d3d BrFixUnpackS24Q1 */
float BrFixUnpackS24Q1(uint32_t v)
{
    int32_t x = (int32_t)v;

    if (x & 0x800000)
        return (float)((x | ~0xFFFFFF) * 0.5f);
    return (float)((x & 0xFFFFFF) * 0.5f);
}

/* 0x10007380.  Scale 0x1008F13C = 0.0078125f (= 1/128). */
/* WHAT IT DOES: turns a packed 16-bit signed value back into a float at
 * 1/128 resolution -- the car's height, among other things. */
/* @implements 0x10007380 d3d BrFixUnpackS16Q7 */
float BrFixUnpackS16Q7(int32_t v)
{
    return (float)((int16_t)v * 0.0078125f);
}

/* 0x100073A0.  Scale 0x1008F140 = 0.00390625f (= 1/256). */
/* WHAT IT DOES: turns a packed 16-bit signed value back into a float at
 * 1/256 resolution. */
/* @implements 0x100073A0 d3d BrFixUnpackS16Q8 */
float BrFixUnpackS16Q8(int32_t v)
{
    return (float)((int16_t)v * 0.00390625f);
}

/* 0x100073C0.  Scale 0x1008F144 = 0.125f. */
/* WHAT IT DOES: turns a packed signed byte back into a float at 1/8
 * resolution. */
/* @implements 0x100073C0 d3d BrFixUnpackS8Q3 */
float BrFixUnpackS8Q3(int32_t v)
{
    return (float)((int8_t)v * 0.125f);
}

/* =====================================================================
 * 2. Car-state packet
 * ===================================================================== */

/* BrCarStateLerp walks the struct as a flat float array, exactly as the
 * original walks the 0xA0-byte record. Catch any accidental padding here
 * rather than at run time. */
typedef char BrCarStateSizeCheck[
    (sizeof(BrCarState) == BR_CARSTATE_FLOATS * sizeof(float)) ? 1 : -1];

/* 0x1008F0CC = 128.0f and 0x1008F118 = 1.0f: the decoder uses BOTH as the
 * "true" value for different 1-bit fields, so they are not interchangeable. */
#define BR_ONE_128  128.0f
#define BR_ONE_1      1.0f

/* The delta code shared by f10, f14, f18 and f78 in 0x100073E0.
 *
 * `prev` is the reference value already re-quantised to the field's integer
 * form. The transmitted word carries a 2-bit code in its top bits and the new
 * low bits underneath. The code adjusts the retained high part by
 * {0, +step, +2*step, -step} -- note the fourth case is a SUBTRACT, not +3.
 *
 * GOTCHA: the original never re-masks after the add/subtract, so the high part
 * is free to carry out of `keepMask` (and, for the -step case starting from
 * zero, to wrap negative). That wraparound is load-bearing -- the s16 fields
 * rely on it to reach negative values -- so it is preserved, which is why this
 * is all unsigned arithmetic. */
static uint32_t BrCarStateDeltaMerge(uint32_t prev, uint32_t bits,
                                     uint32_t codeMask, uint32_t step,
                                     uint32_t keepMask, uint32_t lowMask)
{
    uint32_t code = bits & codeMask;
    uint32_t hi   = prev & keepMask;

    if (code == step)
        hi += step;
    else if (code == step * 2u)
        hi += step * 2u;
    else if (code != 0u)
        hi -= step;

    return hi | (bits & lowMask);
}

/* 0x10006EC0.  Field order is the bitstream order and must not be reordered.
 * The two arguments are (destination, reader); the original loads the reader
 * from the second stack slot into edi before anything else. */
/* WHAT IT DOES: reads one car's complete state out of a network packet and
 * fills in the car record: facing, position, speed-like values, wheel or
 * suspension figures, an angle (which fills four fields, twice raw and twice
 * offset by 35 degrees and wrapped), and a stack of on/off flags. Four
 * fields are deliberately left alone and one is zeroed without any bits
 * being read, so the caller must not assume the whole record was written. */
/* @implements 0x10006EC0 d3d BrCarStateDecode */
void BrCarStateDecode(BrCarState *pDst, BrBitReader *pReader)
{
    float angle, wrapped;

    /* Four s8s, each shifted into the high byte of a 16-bit word first
     * (`xor ecx,ecx / mov ch,al`), so the effective scale is 1/-128. */
    pDst->f00 = BrFixUnpackS16Q15Neg((int32_t)((BrBitReaderRead(pReader, 8) & 0xFFu) << 8));
    pDst->f04 = BrFixUnpackS16Q15Neg((int32_t)((BrBitReaderRead(pReader, 8) & 0xFFu) << 8));
    pDst->f08 = BrFixUnpackS16Q15Neg((int32_t)((BrBitReaderRead(pReader, 8) & 0xFFu) << 8));
    pDst->f0C = BrFixUnpackS16Q15Neg((int32_t)((BrBitReaderRead(pReader, 8) & 0xFFu) << 8));

    pDst->f10 = BrFixUnpackU32Q13(BrBitReaderRead(pReader, 17) << 7);
    pDst->f14 = BrFixUnpackU32Q13(BrBitReaderRead(pReader, 17) << 7);
    pDst->f18 = BrFixUnpackS16Q7((int32_t)(BrBitReaderRead(pReader, 15) << 1));

    pDst->f1C = BrFixUnpackS16Q8((int32_t)BrBitReaderRead(pReader, 16));
    pDst->f20 = BrFixUnpackS16Q8((int32_t)BrBitReaderRead(pReader, 16));

    pDst->f24 = 0.0f;                   /* stored, but no bits are consumed */

    /* `shl al,N` -- an 8-bit shift, so these stay inside one byte. */
    pDst->f28 = BrFixUnpackS8Q3((int32_t)((BrBitReaderRead(pReader, 5) << 3) & 0xFFu));
    pDst->f2C = BrFixUnpackS8Q3((int32_t)((BrBitReaderRead(pReader, 5) << 3) & 0xFFu));
    pDst->f30 = BrFixUnpackS8Q3((int32_t)((BrBitReaderRead(pReader, 5) << 3) & 0xFFu));
    pDst->f34 = BrFixUnpackS8Q3((int32_t)((BrBitReaderRead(pReader, 4) << 4) & 0xFFu));
    pDst->f38 = BrFixUnpackS6Q7Neg((int32_t)((BrBitReaderRead(pReader, 4) << 2) & 0xFFu));

    /* One 4-bit angle feeds four fields: the raw value twice, then the same
     * value biased by +35 degrees and wrapped, twice. 0x1008F110 = -35.0f is
     * subtracted (hence the bias is positive) and 0x1008F114 = 360.0f. */
    angle = BrFixUnpackU8Angle((int32_t)((BrBitReaderRead(pReader, 4) << 4) & 0xFFu));
    pDst->f40 = angle;
    pDst->f3C = angle;
    wrapped = angle - (-35.0f);
    if (wrapped >= 360.0f)
        wrapped -= 360.0f;
    pDst->f48 = wrapped;
    pDst->f44 = wrapped;

    /* 1-bit flags widened through `fild qword`, i.e. plain 0.0f / 1.0f. */
    pDst->f4C = (float)BrBitReaderRead(pReader, 1);
    pDst->f50 = (float)BrBitReaderRead(pReader, 1);
    pDst->f54 = (float)BrBitReaderRead(pReader, 1);
    pDst->f58 = (float)BrBitReaderRead(pReader, 1);

    /* f5C, f60, f64, f68 are never written by this routine. */

    pDst->f6C = BrBitReaderRead(pReader, 1) ? BR_ONE_128 : 0.0f;
    pDst->f70 = BrBitReaderRead(pReader, 1) ? BR_ONE_1   : 0.0f;
    pDst->f74 = BrBitReaderRead(pReader, 1) ? BR_ONE_1   : 0.0f;

    pDst->f78 = BrFixUnpackS24Q1(BrBitReaderRead(pReader, 24));
    pDst->f7C = BrFixUnpackU8Range((int32_t)BrBitReaderRead(pReader, 6));
    pDst->f80 = BrFixUnpackLevel((int32_t)BrBitReaderRead(pReader, 2));
    pDst->f84 = BrFixUnpackLevel((int32_t)BrBitReaderRead(pReader, 2));

    pDst->f88 = BrBitReaderRead(pReader, 1) ? BR_ONE_128 : 0.0f;
    pDst->f8C = BrBitReaderRead(pReader, 1) ? BR_ONE_128 : 0.0f;
    pDst->f90 = BrBitReaderRead(pReader, 1) ? BR_ONE_128 : 0.0f;
    pDst->f94 = BrBitReaderRead(pReader, 1) ? BR_ONE_128 : 0.0f;
    pDst->f98 = BrBitReaderRead(pReader, 1) ? BR_ONE_128 : 0.0f;
    pDst->f9C = BrBitReaderRead(pReader, 1) ? BR_ONE_128 : 0.0f;
}

/* 0x100073E0.  Arguments are (destination, reference, reader): the original
 * loads edi from the third stack slot, ebx from the first and ebp from the
 * second. */
/* WHAT IT DOES: reads a car's state sent as a difference from an earlier
 * one. Facing comes through in full; position, height and one other field
 * arrive as low bits plus a two-bit hint that nudges the high part up one
 * step, up two, or down one. Everything the packet does not mention is left
 * as the caller had it, so the caller must seed the record from the
 * reference first. */
/* @implements 0x100073E0 d3d BrCarStateDecodeDelta */
void BrCarStateDecodeDelta(BrCarState *pDst, const BrCarState *pRef,
                           BrBitReader *pReader)
{
    uint32_t prev, bits;

    pDst->f00 = BrFixUnpackS16Q15Neg((int32_t)((BrBitReaderRead(pReader, 8) & 0xFFu) << 8));
    pDst->f04 = BrFixUnpackS16Q15Neg((int32_t)((BrBitReaderRead(pReader, 8) & 0xFFu) << 8));
    pDst->f08 = BrFixUnpackS16Q15Neg((int32_t)((BrBitReaderRead(pReader, 8) & 0xFFu) << 8));
    pDst->f0C = BrFixUnpackS16Q15Neg((int32_t)((BrBitReaderRead(pReader, 8) & 0xFFu) << 8));

    /* f10/f14: re-quantise the reference to unsigned Q13-in-24, drop the low 7
     * bits (`shr esi,7`, a LOGICAL shift) to get a 17-bit value, then merge
     * 12 transmitted low bits under a 2-bit page code. */
    prev = (uint32_t)BrFixPackU24Q13(pRef->f10) >> 7;
    bits = BrBitReaderRead(pReader, 14);
    pDst->f10 = BrFixUnpackU32Q13(
        BrCarStateDeltaMerge(prev, bits, 0x3000u, 0x1000u, 0x1F000u, 0xFFFu) << 7);

    prev = (uint32_t)BrFixPackU24Q13(pRef->f14) >> 7;
    bits = BrBitReaderRead(pReader, 14);
    pDst->f14 = BrFixUnpackU32Q13(
        BrCarStateDeltaMerge(prev, bits, 0x3000u, 0x1000u, 0x1F000u, 0xFFFu) << 7);

    /* f18: signed Q7-in-16, then `sar ax,1` -- an ARITHMETIC shift on the low
     * 16 bits only -- giving a 15-bit signed value; 9 transmitted low bits. */
    {
        int32_t p16 = BrSext16((uint32_t)BrFixPackS16Q7(pRef->f18));
        prev = (uint32_t)(p16 >> 1);    /* arithmetic shift, matching `sar` */
        bits = BrBitReaderRead(pReader, 11);
        /* `lea eax,[esi+esi]` then a `movsx ax` inside 0x10007380, so only the
         * low 16 bits of the doubled value survive; masked here so the
         * conversion to int32_t is never implementation-defined. */
        pDst->f18 = BrFixUnpackS16Q7((int32_t)(
            (BrCarStateDeltaMerge(prev, bits, 0x600u, 0x200u, 0x7E00u, 0x1FFu)
             * 2u) & 0xFFFFu));
    }

    /* f78: signed Q1-in-24, kept whole; 7 transmitted low bits. */
    prev = (uint32_t)BrFixPackS24Q1(pRef->f78);
    bits = BrBitReaderRead(pReader, 9);
    pDst->f78 = BrFixUnpackS24Q1(
        BrCarStateDeltaMerge(prev, bits, 0x180u, 0x80u, 0xFFFF80u, 0x7Fu));

    pDst->f7C = BrFixUnpackU8Range((int32_t)BrBitReaderRead(pReader, 6));
    pDst->f80 = BrFixUnpackLevel((int32_t)BrBitReaderRead(pReader, 2));
    pDst->f84 = BrFixUnpackLevel((int32_t)BrBitReaderRead(pReader, 2));

    pDst->f88 = BrBitReaderRead(pReader, 1) ? BR_ONE_128 : 0.0f;
    pDst->f8C = BrBitReaderRead(pReader, 1) ? BR_ONE_128 : 0.0f;
    pDst->f90 = BrBitReaderRead(pReader, 1) ? BR_ONE_128 : 0.0f;
    pDst->f94 = BrBitReaderRead(pReader, 1) ? BR_ONE_128 : 0.0f;
    pDst->f98 = BrBitReaderRead(pReader, 1) ? BR_ONE_128 : 0.0f;
    pDst->f9C = BrBitReaderRead(pReader, 1) ? BR_ONE_128 : 0.0f;
}

/* 0x100079E0.  0x1008F0C8 = 0.0f, 0x1008F148 = 10.0f, 0x1008F118 = 1.0f. */
/* WHAT IT DOES: blends between two car states -- the heart of the game's
 * smoothing between network updates. The blend factor may run up to ten
 * times past the second state, because the game is predicting ahead of the
 * last packet it received rather than merely filling in between two it has.
 * When the two facings point opposite ways it flips one of them first, so a
 * car does not spin the long way round; and the last field is copied
 * outright rather than blended. */
/* @implements 0x100079E0 d3d BrCarStateLerp */
void BrCarStateLerp(BrCarState *pDst, float t,
                    const BrCarState *pA, const BrCarState *pB)
{
    const float *a = (const float *)pA;
    const float *b = (const float *)pB;
    float       *d = (float *)pDst;
    int          i;
    int          negate;

    /* Clamp to [0, 10]. Both guards are written so that the x87 "unordered"
     * result takes the same branch a NaN takes here: NaN ends up at 0. */
    if (!(t > 0.0f))
        t = 0.0f;
    else if (!(t < 10.0f))
        t = 10.0f;

    /* Quaternion double-cover fix: only when the leading components have
     * opposite signs AND are at least 1.0 apart. Note this is a magnitude
     * threshold, not a dot-product sign test. */
    negate = (a[0] >= 0.0f && b[0] < 0.0f && (a[0] - b[0]) >= 1.0f)
          || (b[0] >= 0.0f && a[0] < 0.0f && (b[0] - a[0]) >= 1.0f);

    if (negate) {
        for (i = 0; i < 4; ++i)
            d[i] = (-b[i] - a[i]) * t + a[i];
    } else {
        for (i = 0; i < 4; ++i)
            d[i] = (b[i] - a[i]) * t + a[i];
    }

    /* The tail loop is a separate 36-iteration run over +0x10..+0x9C and is
     * NOT affected by the negation. */
    for (i = 4; i < BR_CARSTATE_FLOATS; ++i)
        d[i] = (b[i] - a[i]) * t + a[i];

    /* And then the last field is overwritten outright (a raw dword move in the
     * original), discarding the value the loop just produced. */
    pDst->f9C = pB->f9C;
}

/* =====================================================================
 * 3. Player slot table
 * ===================================================================== */

/* 0x10005960 */
/* WHAT IT DOES: wipes the multiplayer state back to empty at the start of a
 * session: clears every player slot's samples and status under that slot's
 * own lock, and resets the shared counters, queues and timers. A few fields
 * are deliberately stepped over and left as they were, and the disarmed
 * timers are set to -1 rather than zero. */
/* @implements 0x10005960 d3d BrNetReset */
int BrNetReset(BrNetState *pNet)
{
    int i;

    for (i = 0; i < BR_NET_SLOTS; ++i) {
        BrNetSlot *p = &pNet->aSlots[i];

        BrNetMutexLock(p->hMutex);

        /* The original clears +0x08, then +0x0C..+0x28, then +0x2C, then
         * +0x38..+0x54. +0x04, +0x30 and +0x34 are stepped over on purpose. */
        p->f008 = 0;
        memset(p->f00C, 0, sizeof p->f00C);
        p->f02C = 0;
        memset(p->f038, 0, sizeof p->f038);

        p->f558 = 0;
        p->f55C = 0;
        p->f560 = -1;
        p->f568 = 0;
        p->f56C = 0;
        p->f564 = 0;
        p->f974 = 0;

        BrNetMutexUnlock(p->hMutex);
    }

    BrNetMutexLock(pNet->h1022AF24);
    pNet->f1022AEF8 = -1;
    pNet->f1022AF08 = 0;
    pNet->f10220E80 = 0;
    BrNetMutexUnlock(pNet->h1022AF24);

    BrNetMutexLock(pNet->h1022AF28);
    memset(pNet->a102212D0, 0, sizeof pNet->a102212D0);
    BrNetMutexUnlock(pNet->h1022AF28);

    BrNetMutexLock(pNet->h1022AF2C);
    pNet->f10220DD4 = -1;
    BrNetMutexUnlock(pNet->h1022AF2C);

    BrNetMutexLock(pNet->h1022AF30);
    pNet->f10221318 = -1;
    BrNetMutexUnlock(pNet->h1022AF30);

    BrNetMutexLock(pNet->h10221324);
    pNet->f1022AAA8 = 0;
    BrNetMutexUnlock(pNet->h10221324);

    BrNetMutexLock(pNet->h1022AF04);
    pNet->f1022AAF4 = 0;
    BrNetMutexUnlock(pNet->h1022AF04);

    BrNetMutexLock(pNet->h10220DDC);
    pNet->f10221314 = 0;
    BrNetMutexUnlock(pNet->h10220DDC);

    BrNetMutexLock(pNet->h1022131C);
    pNet->f10220DD0 = 0;
    BrNetMutexUnlock(pNet->h1022131C);

    BrNetMutexLock(pNet->h10220CEC);
    pNet->f1022AF00 = -1;
    BrNetMutexUnlock(pNet->h10220CEC);

    /* Tail: no lock is taken for any of these. */
    pNet->f10220DD8 = 0;
    pNet->f1022AF3C = -1;
    memset(pNet->a1022AAB0, 0, sizeof pNet->a1022AAB0);
    pNet->f1022AF20 = 0;
    pNet->f106909D8 = 0;

    return 1;
}

/* 0x10004A10 */
/* WHAT IT DOES: reads a player slot's status word under that slot's lock. */
/* @implements 0x10004A10 d3d BrNetSlotGetF02C */
int32_t BrNetSlotGetF02C(BrNetState *pNet, int32_t slot)
{
    BrNetSlot *p = &pNet->aSlots[slot];
    int32_t    value;

    BrNetMutexLock(p->hMutex);
    value = p->f02C;
    BrNetMutexUnlock(p->hMutex);
    return value;
}

/* 0x10004A50 */
/* WHAT IT DOES: writes a player slot's status word under that slot's lock. */
/* @implements 0x10004A50 d3d BrNetSlotSetF02C */
void BrNetSlotSetF02C(BrNetState *pNet, int32_t slot, int32_t value)
{
    BrNetSlot *p = &pNet->aSlots[slot];

    BrNetMutexLock(p->hMutex);
    p->f02C = value;
    BrNetMutexUnlock(p->hMutex);
}

/* 0x10005CF0 */
/* WHAT IT DOES: reads one identifying number out of a player slot under that
 * slot's lock. */
/* @implements 0x10005CF0 d3d BrNetSlotGetF004 */
int32_t BrNetSlotGetF004(BrNetState *pNet, int32_t slot)
{
    BrNetSlot *p = &pNet->aSlots[slot];
    int32_t    value;

    BrNetMutexLock(p->hMutex);
    value = p->f004;
    BrNetMutexUnlock(p->hMutex);
    return value;
}

/* 0x10005E70 */
/* WHAT IT DOES: hands back a player's name, copied under that slot's lock
 * into a shared scratch buffer. Because the buffer is shared, the answer is
 * only good until the next call. The original copied with no length limit at
 * either end; this one is bounded. */
/* @implements 0x10005E70 d3d BrNetSlotName */
char *BrNetSlotName(BrNetState *pNet, int32_t slot)
{
    BrNetSlot *p = &pNet->aSlots[slot];
    size_t     n;

    BrNetMutexLock(p->hMutex);
    /* DEVIATION: the original is an inlined `repne scasb` + `rep movsd/movsb`
     * strcpy into a fixed global with no length limit at either end. Bounded
     * here, and the source is bounded too in case the record is not
     * terminated. Behaviour is identical for any name that fits. */
    for (n = 0; n < sizeof p->f570 && p->f570[n] != '\0'; ++n)
        ;
    if (n >= sizeof pNet->aNameScratch)
        n = sizeof pNet->aNameScratch - 1;
    memcpy(pNet->aNameScratch, p->f570, n);
    pNet->aNameScratch[n] = '\0';
    BrNetMutexUnlock(p->hMutex);

    return pNet->aNameScratch;
}

/* 0x10005FE0 */
/* WHAT IT DOES: drops every player whose identifier matches the one given --
 * the disconnect path. For each match it puts the slot number on the free
 * list, clears the slot's status, and announces "<name> left the game" to
 * the other players. The announcement really does begin with a literal
 * percent-fifteen, which is a colour code in the game's message text. */
/* @implements 0x10005FE0 d3d BrNetDropMatching */
void BrNetDropMatching(BrNetState *pNet, int32_t key)
{
    char    szMsg[0x400];       /* the original's stack buffer, same size */
    int32_t i;

    for (i = 0; i < BR_NET_SLOTS; ++i) {
        if (BrNetSlotGetF004(pNet, i) != key)
            continue;
        /* `test al,0x3f` -- any of the low six flag bits. */
        if ((BrNetSlotGetF02C(pNet, i) & 0x3F) == 0)
            continue;

        BrNetMutexLock(pNet->h1022AF30);
        /* Pre-increment then store, so the index starts at 0 given the -1
         * BrNetReset leaves behind.
         *
         * DEVIATION: the original has no bound on this push. It cannot
         * overflow in one pass (16 slots, 16 entries) but repeated calls
         * without a reset would walk off the end of the array; guarded. */
        if (pNet->f10221318 + 1 < (int32_t)(sizeof pNet->a10221288 /
                                            sizeof pNet->a10221288[0])) {
            pNet->f10221318 += 1;
            pNet->a10221288[pNet->f10221318] = i;
        }
        BrNetMutexUnlock(pNet->h1022AF30);

        BrNetSlotSetF02C(pNet, i, 0);

        /* DEVIATION: sprintf -> snprintf. The '%%' is in the original literal
         * at 0x10094338, so the rendered text really does begin with "%15". */
        snprintf(szMsg, sizeof szMsg, "%%15%s left the game.",
                 pNet->aSlots[i].f570);
        BrNetAnnounce(szMsg);
    }
}

/* =====================================================================
 * 4. Palette fetch
 * ===================================================================== */

/* 0x100049C0.  Records are 3 bytes (`ecx + ecx*2 + base`) and the copy is
 * straight: source +0 -> dest +0, +1 -> +1, +2 -> +2 (no channel swap). The
 * original writes the last byte first; order is immaterial. */
/* WHAT IT DOES: reads one colour out of a palette: three bytes at the
 * entry's position, copied straight through with no channel reordering. */
/* @implements 0x100049C0 d3d BrPalFetch */
void BrPalFetch(const uint8_t *pTable, int32_t index, uint8_t aOut[3])
{
    const uint8_t *p = pTable + (ptrdiff_t)index * 3;

    aOut[2] = p[2];
    aOut[1] = p[1];
    aOut[0] = p[0];
}
