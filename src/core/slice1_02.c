/* slice1_02.c -- BRD3D.dll 0x100049C0-0x100079E0. See slice1_02.h. */

#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
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
/* @d3donly 0x1007C8A0 BrFtol -- absent from BRGlide (D3D-only / dynamically-imported CRT); no Glide twin exists */
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
#ifdef BR_MATCHING_BUILD
    /* The scale and bias are FLOAT constants (single-precision fmul/fsubr);
     * the clamp stays in ST(0), and the final `(int32_t)` is the plain cast
     * VC5 tail-jumps to __ftol -- the same form as BrFixPackS24Q1. */
    double d = BrFloor(0.5f - v * -8192.0f);

    if (!(d >= 0.0))
        d = 0.0;
    if (d > 16777215.0)
        d = 16777215.0;
    return (int32_t)d;
#else
    double d = BrFloor(0.5 - (double)v * -8192.0);

    if (!(d >= 0.0))
        d = 0.0;
    if (d > 16777215.0)
        d = 16777215.0;
    return BrFtol(d);
#endif
}

/* 0x10006730.  0x1008F100 = -2.0f. Clamp is on the INTEGER, after __ftol, and
 * with signed compares (jge/jle). */
/* WHAT IT DOES: packs a signed value at half-unit resolution into 24 bits
 * for the network, pinned to that range. */
/* @implements 0x10006730 d3d BrFixPackS24Q1 */
int32_t BrFixPackS24Q1(float v)
{
#ifdef BR_MATCHING_BUILD
    /* The original leaves the scaled value in ST(0) and `call __ftol`, and a
     * plain cast is the only form VC5 compiles to that. */
    int32_t r = (int32_t)BrFloor(0.5f - v * -2.0f);
#else
    /* Host: the cast is undefined once the scaled value leaves int32, and
     * ARM64 saturates where __ftol wraps. The clamp below is applied to the
     * INTEGER, so it sees the wrapped value -- go through BrFtol to keep it. */
    int32_t r = BrFtol(BrFloor(0.5f - v * -2.0f));
#endif

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
#ifdef BR_MATCHING_BUILD
    /* As BrFixPackS24Q1 above: plain cast so VC5 emits `call __ftol`. */
    int32_t r = (int32_t)BrFloor(0.5f - v * -128.0f);
#else
    /* Host: __ftol's low-dword wrap, which the integer clamp below relies on.
     * 1.0e9 scales to ~1.28e11 -- fits in int64, so it wraps NEGATIVE and
     * clamps to -32768 rather than saturating to +32767. */
    int32_t r = BrFtol(BrFloor(0.5f - v * -128.0f));
#endif

    if (r < -32768)
        r = -32768;
    if (r > 32767)
        r = 32767;
    return r;
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
#ifdef BR_MATCHING_BUILD
/* The original takes no arguments and reaches every field as a loose global:
 * the slot array runs from 0x1021CE58, and the shared state is scattered from
 * 0x1021C81C to 0x105CCB80.  The mutex is the raw Win32 pair
 * WaitForSingleObject(h, INFINITE) / ReleaseMutex(h) through the import table,
 * not the port's BrNetMutexLock/Unlock wrappers.  pNet is the header's
 * signature and is unused here. */
__declspec(dllimport) unsigned long __stdcall WaitForSingleObject(void *, unsigned long);
__declspec(dllimport) int __stdcall ReleaseMutex(void *);

extern int DAT_1021ce64;   /* slot[0] + 0x00C -- the walk pointer */
extern int DAT_102265e4;   /* one past the last slot; also the pair array */
extern int DAT_10226624;
extern int DAT_10226a54;
extern int DAT_10226a28;
extern int DAT_10226a38;
extern unsigned char DAT_1021c9b0;
extern int DAT_10226a58;
extern int DAT_1021ce00;
extern int DAT_10226a5c;
extern int DAT_1021c904;
extern int DAT_10226a60;
extern int DAT_1021ce48;
extern int DAT_1021ce54;
extern int DAT_102265d8;
extern int DAT_10226a34;
extern int DAT_1021c90c;
extern int DAT_1021ce44;
extern int DAT_1021ce4c;
extern int DAT_1021c900;
extern int DAT_1021c81c;
extern int DAT_10226a30;
extern int DAT_1021c908;
extern int DAT_10226a6c;
extern int DAT_10226a50;
extern int DAT_105ccb80;

int BrNetReset(BrNetState *pNet)
{
    int *p;
    int *q;

    (void)pNet;

    /* q walks &slot->f00C (slot + 0x0C), one 0x978-byte record per turn; p is
     * the record base.  Two things are load-bearing here:
     *   - q, not p, is the loop variable.  VC5 substitutes the loop pointer
     *     with the first derived address it has to materialise, so a p-based
     *     loop moves the induction register to &slot->f038 and costs a `lea`
     *     at the bottom test.  With q primary the test is `cmp esi, END`.
     *   - the ReleaseMutex handle is read through q, not p.  Read through the
     *     same pointer as the +0x558 stores, VC5 proves non-aliasing and
     *     hoists `mov ecx,[esi-0xc]; push ecx` above them; through the other
     *     pointer it cannot, and the load stays after the stores like orig. */
    q = &DAT_1021ce64;
    do {
        p = q - 3;
        WaitForSingleObject((void *)p[0], 0xffffffff);
        p[2] = 0;                    /* +0x008 */
        memset(q, 0, 32);            /* +0x00C..+0x02B  (rep stosd, 8 dwords) */
        p[11] = 0;                   /* +0x02C */
        memset(q + 11, 0, 32);       /* +0x038..+0x057 */
        p[0x156] = 0;                /* +0x558 */
        p[0x157] = 0;                /* +0x55C */
        p[0x158] = -1;               /* +0x560 */
        p[0x15a] = 0;                /* +0x568 */
        p[0x15b] = 0;                /* +0x56C */
        p[0x159] = 0;                /* +0x564 */
        p[0x25d] = 0;                /* +0x974 */
        ReleaseMutex((void *)q[-3]);
        q += 0x25e;
    } while ((int)q < (int)&DAT_102265e4);

    WaitForSingleObject((void *)DAT_10226a54, 0xffffffff);
    DAT_10226a28 = -1;
    DAT_10226a38 = 0;
    DAT_1021c9b0 = 0;
    ReleaseMutex((void *)DAT_10226a54);

    WaitForSingleObject((void *)DAT_10226a58, 0xffffffff);
    memset(&DAT_1021ce00, 0, 64);
    ReleaseMutex((void *)DAT_10226a58);

    WaitForSingleObject((void *)DAT_10226a5c, 0xffffffff);
    DAT_1021c904 = -1;
    ReleaseMutex((void *)DAT_10226a5c);

    WaitForSingleObject((void *)DAT_10226a60, 0xffffffff);
    DAT_1021ce48 = -1;
    ReleaseMutex((void *)DAT_10226a60);

    WaitForSingleObject((void *)DAT_1021ce54, 0xffffffff);
    DAT_102265d8 = 0;
    ReleaseMutex((void *)DAT_1021ce54);

    WaitForSingleObject((void *)DAT_10226a34, 0xffffffff);
    DAT_10226624 = 0;
    ReleaseMutex((void *)DAT_10226a34);

    WaitForSingleObject((void *)DAT_1021c90c, 0xffffffff);
    DAT_1021ce44 = 0;
    ReleaseMutex((void *)DAT_1021c90c);

    WaitForSingleObject((void *)DAT_1021ce4c, 0xffffffff);
    DAT_1021c900 = 0;
    ReleaseMutex((void *)DAT_1021ce4c);

    WaitForSingleObject((void *)DAT_1021c81c, 0xffffffff);
    DAT_10226a30 = -1;
    ReleaseMutex((void *)DAT_1021c81c);

    DAT_1021c908 = 0;
    DAT_10226a6c = -1;

    p = &DAT_102265e4;
    do {
        p[-1] = 0;
        *p = 0;
        p += 2;
    } while ((int)p < (int)&DAT_10226624);

    DAT_10226a50 = 0;
    DAT_105ccb80 = 0;
    return 1;
}


extern int FUN_1006ba60(int a, int b);
extern unsigned char *DAT_104abb20;
extern int DAT_104abb24;

/* WHAT IT DOES: release the pending input one-shot under the message mutex
 * -- silences its sound, frees the slot, and restores the default message
 * table and level if one was armed. */
/* @implements 0x10006460 glide FUN_10006460 */
/* Mutex-guarded release of one input one-shot: stop the pending sound and
 * reset its slot, and if the arm flag is set restore the default table
 * pointer (&DAT_1021c9b0) and its 3.0f level.  Same lock idiom as BrNetReset. */
void FUN_10006460(void)
{
    WaitForSingleObject((void *)DAT_10226a54, 0xffffffff);
    if (DAT_10226a28 >= 0) {
        FUN_1006ba60(DAT_10226a28, 0x200020);
        DAT_10226a28 = -1;
    }
    if (DAT_10226a38 != 0) {
        DAT_104abb20 = &DAT_1021c9b0;
        DAT_104abb24 = 0x40400000;
        DAT_10226a38 = 0;
    }
    ReleaseMutex((void *)DAT_10226a54);
}
#else
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
#endif

/* 0x10004A10 */
/* WHAT IT DOES: reads a player slot's status word under that slot's lock. */
/* port-only body; Glide match is src/core/generated/0x10004D80.c */
int32_t BrNetSlotGetF02C(BrNetState *pNet, int32_t slot)
{
    BrNetSlot *p = &pNet->aSlots[slot];
    int32_t    value;

    BrNetMutexLock(p->hMutex);
    value = p->f02C;
    BrNetMutexUnlock(p->hMutex);
    return value;
}

/* 0x10004DC0 / d3d 0x10004A50 */
/* WHAT IT DOES: writes a player slot's status word under that slot's lock. */
/* @implements 0x10004DC0 glide BrNetSlotSetF02C */
#ifdef BR_MATCHING_BUILD
/* Orig is two-arg cdecl (index at [esp+4], value at [esp+8]), not the port's
 * (pNet, slot, value).  Mutex lock is the raw import, same shape as BrNetReset:
 * WaitForSingleObject(h, INFINITE) / ReleaseMutex(h), not BrNetMutexLock.
 * Three indexings of the global so the ReleaseMutex handle reloads; a cached
 * pointer would materialise the base into esi and encode [esi]/[esi+0x2c]
 * instead of orig's [esi+0x1021ce58]/[esi+0x1021ce84]. */
__declspec(dllimport) unsigned long __stdcall WaitForSingleObject(void *, unsigned long);
__declspec(dllimport) int __stdcall ReleaseMutex(void *);

typedef struct BrNetSlot978 {
    void *hMutex;                 /* +0x000 = 0x1021ce58 */
    char  pad004[0x28];
    int   f02C;                   /* +0x02C = 0x1021ce84 */
    char  rest[0x978 - 0x30];
} BrNetSlot978;

typedef char br_assert_slot978[(sizeof(BrNetSlot978) == 0x978) ? 1 : -1];

extern BrNetSlot978 slots[];      /* 0x1021ce58, stride 0x978 */

void BrNetSlotSetF02C(int param_1, int param_2)
{
    WaitForSingleObject((void *)slots[param_1].hMutex, 0xffffffff);
    slots[param_1].f02C = param_2;
    ReleaseMutex((void *)slots[param_1].hMutex);
}
#else
void BrNetSlotSetF02C(BrNetState *pNet, int32_t slot, int32_t value)
{
    BrNetSlot *p = &pNet->aSlots[slot];

    BrNetMutexLock(p->hMutex);
    p->f02C = value;
    BrNetMutexUnlock(p->hMutex);
}
#endif

/* 0x10005CF0 */
/* WHAT IT DOES: reads one identifying number out of a player slot under that
 * slot's lock. */
/* port-only body; Glide match is src/core/generated/0x10006060.c */
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
/* port-only body; Glide match is src/core/generated/0x100061E0.c */
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
/* port-only body; Glide match is src/core/generated/0x10006350.c -- the
 * original takes only the key and reaches every slot through the globals,
 * which this signature cannot express. */
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
#ifdef BR_MATCHING_BUILD
/* The original takes no arguments: index is 0x10094294, table is
 * 0x100B37D0, dest is 0x10AD0854.  The port signature is the header's.
 * volatile on the index stops VC5 CSEing the three loads into one lea. */
extern volatile int32_t g_br094294; /* 0x10094294 */
extern uint8_t g_aBr0B37D0[];       /* 0x100B37D0 */
extern uint8_t g_brAD0854[3];       /* 0x10AD0854 */

void BrPalFetch(const uint8_t *pTable, int32_t index, uint8_t aOut[3])
{
    int i0, i1, i2, b0, b1, b2;

    i0 = g_br094294;
    i1 = g_br094294;
    b2 = g_aBr0B37D0[i0 * 3 + 2];
    i2 = g_br094294;
    b1 = g_aBr0B37D0[i1 * 3 + 1];
    b0 = g_aBr0B37D0[i2 * 3];
    g_brAD0854[2] = (uint8_t)b2;
    g_brAD0854[1] = (uint8_t)b1;
    g_brAD0854[0] = (uint8_t)b0;
}
#else
void BrPalFetch(const uint8_t *pTable, int32_t index, uint8_t aOut[3])
{
    const uint8_t *p = pTable + (ptrdiff_t)index * 3;

    aOut[2] = p[2];
    aOut[1] = p[1];
    aOut[0] = p[0];
}
#endif

#ifdef BR_MATCHING_BUILD
__declspec(dllimport) unsigned long __stdcall WaitForSingleObject(void *, unsigned long);
__declspec(dllimport) int __stdcall ReleaseMutex(void *);

extern void    *g_brH221324;
extern int32_t  g_br22AAA8;
extern void    *g_brH22AF04;
extern int32_t  g_br22AAF4;

/* WHAT IT DOES: under the mutex, turns on the broadcast-enable flag. */
/* @implements 0x10004BB0 d3d BrNetLockSet22AAA8 */
int BrNetLockSet22AAA8(void)
{
    WaitForSingleObject(g_brH221324, (unsigned long)-1);
    g_br22AAA8 = 1;
    ReleaseMutex(g_brH221324);
    return 1;
}

/* WHAT IT DOES: seeds the keepalive counter if it is sitting at zero. */
/* @implements 0x10004BE0 d3d BrNetLockSetIfZero22AAF4 */
int BrNetLockSetIfZero22AAF4(void)
{
    WaitForSingleObject(g_brH22AF04, (unsigned long)-1);
    if (g_br22AAF4 == 0)
        g_br22AAF4 = 1;
    ReleaseMutex(g_brH22AF04);
    return 1;
}
#else
int BrNetLockSet22AAA8(BrNetState *pNet)
{
    BrNetMutexLock(pNet->h10221324);
    pNet->f1022AAA8 = 1;
    BrNetMutexUnlock(pNet->h10221324);
    return 1;
}

int BrNetLockSetIfZero22AAF4(BrNetState *pNet)
{
    BrNetMutexLock(pNet->h1022AF04);
    if (pNet->f1022AAF4 == 0)
        pNet->f1022AAF4 = 1;
    BrNetMutexUnlock(pNet->h1022AF04);
    return 1;
}
#endif

/* ── Ghidra-matched functions ─────────────────────────── */
#ifdef BR_MATCHING_BUILD
#include <windows.h>
extern int DAT_1021c81c;
extern int DAT_1021c908;
extern int DAT_1021ce40;
extern int DAT_1021ce4c;
extern int DAT_1021ce58;
extern int DAT_10226a54;
extern int DAT_10226a58;
extern int DAT_10226a5c;
extern int DAT_10226a64;
extern int g_brH220DDC;
extern int g_h1022AF30;
int BrNetReset();

/* WHAT IT DOES: create Win32 mutexes for the net/multiplayer subsystem and reset the network layer. */
/* @implements 0x10005E80 glide BrNetMutexInit */

int BrNetMutexInit(void)

{
  HANDLE pvVar1;
  int *puVar2;
  
  puVar2 = &DAT_1021ce58;
  do {
    pvVar1 = CreateMutexA((LPSECURITY_ATTRIBUTES)0x0,0,(LPCSTR)0x0);
    *puVar2 = pvVar1;
    puVar2 = puVar2 + 0x25e;
  } while ((int)puVar2 < 0x102265d8);
  DAT_10226a54 = CreateMutexA((LPSECURITY_ATTRIBUTES)0x0,0,(LPCSTR)0x0);
  DAT_10226a58 = CreateMutexA((LPSECURITY_ATTRIBUTES)0x0,0,(LPCSTR)0x0);
  DAT_10226a5c = CreateMutexA((LPSECURITY_ATTRIBUTES)0x0,0,(LPCSTR)0x0);
  g_h1022AF30 = CreateMutexA((LPSECURITY_ATTRIBUTES)0x0,0,(LPCSTR)0x0);
  DAT_1021ce40 = 0;
  DAT_1021c908 = 0;
  BrTimeUpdate();
  DAT_10226a64 = CreateMutexA((LPSECURITY_ATTRIBUTES)0x0,0,(LPCSTR)0x0);
  g_brH221324 = CreateMutexA((LPSECURITY_ATTRIBUTES)0x0,0,(LPCSTR)0x0);
  g_brH22AF04 = CreateMutexA((LPSECURITY_ATTRIBUTES)0x0,0,(LPCSTR)0x0);
  g_brH220DDC = CreateMutexA((LPSECURITY_ATTRIBUTES)0x0,0,(LPCSTR)0x0);
  DAT_1021ce4c = CreateMutexA((LPSECURITY_ATTRIBUTES)0x0,0,(LPCSTR)0x0);
  DAT_1021c81c = CreateMutexA((LPSECURITY_ATTRIBUTES)0x0,0,(LPCSTR)0x0);
  ((int (*)())BrNetReset)();
  return 1;
}


extern int DAT_1007b268;
extern float DAT_1021c820;
extern float DAT_1021c990;
extern float DAT_1021c994;
extern float DAT_1021c998;
extern float DAT_1021c99c;
extern float DAT_1021c9a0;
extern float DAT_1021c9a4;
extern float DAT_1021c9a8;
extern int DAT_10226a70;
extern float _DAT_100770ac;
extern float _DAT_1021c898;
int FUN_10004fd0(float *);
int FUN_100051c0(float *, float *);
int FUN_10005330(void);
int BrNetSendFlush(void);

/* WHAT IT DOES: decide whether an incoming car-state update is fresh enough
 * to accept: a new enough one is copied into the shared record and applied,
 * an older one is counted as a miss and only allowed to nudge a couple of
 * values before being dropped. The network's stale-packet filter. */
/* @implements 0x100054A0 glide FUN_100054a0 */
/* auto-filed from ghidra --refine; transforms: as-is */

int FUN_100054a0(float *param_1)
{
  int uVar1;

  if ((param_1[0x1e] >= _DAT_100770ac) && (_DAT_1021c898 < _DAT_100770ac)) {
    memcpy(&DAT_1021c820, param_1, 0xa0);
    uVar1 = FUN_10004fd0(param_1);
    return uVar1;
  }
  DAT_10226a70 = DAT_10226a70 + 1;
  if (DAT_10226a70 < 3) {
    if (DAT_1021c990 < param_1[0x20]) {
      DAT_1021c990 = param_1[0x20];
    }
    if (DAT_1021c994 < param_1[0x21]) {
      DAT_1021c994 = param_1[0x21];
    }
    if (DAT_1021c998 < param_1[0x22]) {
      DAT_1021c998 = param_1[0x22];
    }
    if (DAT_1021c99c < param_1[0x23]) {
      DAT_1021c99c = param_1[0x23];
    }
    if (DAT_1021c9a0 < param_1[0x24]) {
      DAT_1021c9a0 = param_1[0x24];
    }
    if (DAT_1021c9a4 < param_1[0x25]) {
      DAT_1021c9a4 = param_1[0x25];
    }
    if (DAT_1021c9a8 < param_1[0x26]) {
      DAT_1021c9a8 = param_1[0x26];
    }
    return 1;
  }
  if (param_1[0x20] < DAT_1021c990) {
    param_1[0x20] = DAT_1021c990;
  }
  if (param_1[0x21] < DAT_1021c994) {
    param_1[0x21] = DAT_1021c994;
  }
  if (param_1[0x22] < DAT_1021c998) {
    param_1[0x22] = DAT_1021c998;
  }
  if (param_1[0x23] < DAT_1021c99c) {
    param_1[0x23] = DAT_1021c99c;
  }
  if (param_1[0x24] < DAT_1021c9a0) {
    param_1[0x24] = DAT_1021c9a0;
  }
  if (param_1[0x25] < DAT_1021c9a4) {
    param_1[0x25] = DAT_1021c9a4;
  }
  if (param_1[0x26] < DAT_1021c9a8) {
    param_1[0x26] = DAT_1021c9a8;
  }
  DAT_1021c9a8 = 0.0f;
  DAT_1007b268 = DAT_1007b268 + 1;
  DAT_1021c9a4 = 0.0f;
  DAT_1021c9a0 = 0.0f;
  DAT_1021c99c = 0.0f;
  DAT_1021c998 = 0.0f;
  DAT_1021c994 = 0.0f;
  DAT_1021c990 = 0.0f;
  DAT_10226a70 = 0;
  if (DAT_1007b268 % 4 == 0) {
    memcpy(&DAT_1021c820, param_1, 0xa0);
    uVar1 = FUN_10004fd0(param_1);
    return uVar1;
  }
  uVar1 = FUN_100051c0(param_1, &DAT_1021c820);
  BrNetSendFlush();
  FUN_10005330();
  return uVar1;
}

#endif /* BR_MATCHING_BUILD */
