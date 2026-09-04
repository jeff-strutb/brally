/* br_fix.c -- net.
 *
 * Filed out of the address batches: these functions were
 * matched first and grouped by what they are afterwards.
 * Every function carries its original address.
 */
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import
 * table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include <stdint.h>
#include <math.h>
#include <string.h>

#ifdef BR_MATCHING_BUILD


/* 0x10007250.  Scale 0x1008F11C = -0.0078125f (= -1/128).
 * The sign test is on bit 5 of the low byte; the original ORs in 0xC0 to
 * extend, or ANDs with 0x3F when clear -- both branches then `movsx al`. */
/* WHAT IT DOES: unpacks a small signed fraction that was sent in 6 bits,
 * sign-extending it by hand before scaling. This is the exact inverse of the
 * matching packer. */
/* @implements 0x10007250 d3d BrFixUnpackS6Q7Neg */
/* @implements 0x100075C0 glide BrFixUnpackS6Q7Neg */
float BrFixUnpackS6Q7Neg(int32_t v)
{
    unsigned char b = (unsigned char)v;
    int32_t       s;

    if (b & 0x20) {
        b |= 0xC0u;
    } else {
        b &= 0x3Fu;
    }
    s = (signed char)b;
    return (float)(s * -0.0078125f);
}



/* 0x10007280.  Scale 0x1008F120 = -3.0517578125e-05f (= -1/32768). */
/* WHAT IT DOES: unpacks one component of a car's facing direction from the
 * 16 bits it was sent in. The scale is negative, which cancels the negative
 * scale the packer used, so the value comes back unchanged. */
/* @implements 0x10007280 d3d BrFixUnpackS16Q15Neg */
/* @implements 0x100075F0 glide BrFixUnpackS16Q15Neg */
float BrFixUnpackS16Q15Neg(int32_t v)
{
    return (float)((int16_t)v * -0.000030517578125f);
}



/* 0x100072A0.  Scale 0x1008F124 = 1.41015625f (= 361/256). */
/* WHAT IT DOES: turns a byte back into an angle in degrees, using a scale
 * chosen so the byte covers a full circle. */
/* @implements 0x100072A0 d3d BrFixUnpackU8Angle */
/* @implements 0x10007610 glide BrFixUnpackU8Angle */
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
/* @implements 0x10007630 glide BrFixUnpackU8Range */
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
/* @implements 0x10007650 glide BrFixUnpackLevel */
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
/* @implements 0x10007680 glide BrFixUnpackU32Q13 */
float BrFixUnpackU32Q13(uint32_t v)
{
    return (float)(v * 0.0001220703125f);
}



/* 0x10007340.  Scale 0x1008F0D0 = 0.5f; sign bit is bit 23. */
/* WHAT IT DOES: turns a packed 24-bit signed value back into a float at
 * half-unit resolution. */
/* @implements 0x10007340 d3d BrFixUnpackS24Q1 */
/* @implements 0x100076B0 glide BrFixUnpackS24Q1 */
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
/* @implements 0x100076F0 glide BrFixUnpackS16Q7 */
float BrFixUnpackS16Q7(int32_t v)
{
    return (float)((int16_t)v * 0.0078125f);
}



/* 0x100073A0.  Scale 0x1008F140 = 0.00390625f (= 1/256). */
/* WHAT IT DOES: turns a packed 16-bit signed value back into a float at
 * 1/256 resolution. */
/* @implements 0x100073A0 d3d BrFixUnpackS16Q8 */
/* @implements 0x10007710 glide BrFixUnpackS16Q8 */
float BrFixUnpackS16Q8(int32_t v)
{
    return (float)((int16_t)v * 0.00390625f);
}



/* 0x100073C0.  Scale 0x1008F144 = 0.125f. */
/* WHAT IT DOES: turns a packed signed byte back into a float at 1/8
 * resolution. */
/* @implements 0x100073C0 d3d BrFixUnpackS8Q3 */
/* @implements 0x10007730 glide BrFixUnpackS8Q3 */
float BrFixUnpackS8Q3(int32_t v)
{
    return (float)((int8_t)v * 0.125f);
}

#endif /* BR_MATCHING_BUILD */

/* =====================================================================
 * The packers: the wire side of the same codecs.
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

/* .rdata constants BrFixPackLevel compares against, read out of the shipped
 * DLL rather than inferred.  slice2_12.c keeps its own copies for the
 * encoders that stayed behind; these are static, so both are private. */
static const float BrK08F0CC =  128.0f;                  /* 0x1008F0CC */
static const float BrK08F0E4 =  171.0f;                  /* 0x1008F0E4 */
static const float BrK08F0E8 =  213.0f;                  /* 0x1008F0E8 */

/* =====================================================================
 * 2. Quantisers
 * ===================================================================== */

/* 0x100065A0.  0.5 - v*128, then clamp the INTEGER to [-32, 31]. */
/* WHAT IT DOES: squashes a small signed fraction down to a 6-bit number so
 * it fits in a network packet. Rounding is to the nearest, with halves going
 * up, and the result is pinned inside what 6 bits can hold. The decoder on
 * the other end undoes this exactly. */
/* @implements 0x100065A0 d3d BrFixPackS6Q7Neg */
int8_t BrFixPackS6Q7Neg(float v)
{
    int32_t n = (int32_t)BrFloor(0.5f - v * 128.0f);

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
int16_t BrFixPackS16Q15Neg(float v)
{
    int32_t n = (int32_t)BrFloor(0.5f - v * 32768.0f);

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
    int32_t n = (int32_t)BrFloor(0.5f - v * -0.7091412544250488f);

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
int8_t BrFixPackU8Range(float v)
{
    int32_t n = (int32_t)BrFloor(0.5f - (v - 400.0f) * -0.008289474062621593f);

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
int8_t BrFixPackLevel(float v)
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
    int32_t n = (int32_t)BrFloor(0.5f - v * -256.0f);

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
    int32_t n = (int32_t)BrFloor(0.5f - v * -8.0f);

    if (n < -128)
        n = -128;
    if (n > 127)
        n = 127;
    return n;
}
