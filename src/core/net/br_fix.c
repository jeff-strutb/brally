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

#endif /* BR_MATCHING_BUILD */
