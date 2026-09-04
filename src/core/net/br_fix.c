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
