/* br_carpack.c -- net.
 *
 * The fixed 22-byte car-state record: the second, simpler wire format the
 * game uses alongside the bit stream.
 *
 * Filed out of the address batches: these functions were
 * matched first and grouped by what they are afterwards.
 * Every function carries its original address.
 */
#ifdef BR_MATCHING_BUILD
/* The two 16-bit quantisers return `short` in the original: their results
 * are shifted in AX (`sar ax,8` / `sar ax,1`) and only then widened
 * (`movsx ecx,ax`), which VC5 only does when the value is known to be
 * sign-extended from 16 bits.  The headers have to spell them int32_t for
 * the port, so rename those prototypes and re-declare them narrow. */
#define BrFixPackS16Q15Neg BrFixPackS16Q15Neg_int_hdr
#define BrFixPackS16Q7     BrFixPackS16Q7_int_hdr
#endif
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include "slice2_12.h"
#ifdef BR_MATCHING_BUILD
#undef BrFixPackS16Q15Neg
#undef BrFixPackS16Q7
int16_t BrFixPackS16Q15Neg(float v);
int16_t BrFixPackS16Q7(float v);
#endif

/* 0x10006BD0 */
/* WHAT IT DOES: packs a car's state into a fixed 22-byte record -- a second,
 * simpler wire format that has no bit stream behind it. Flags are hidden in
 * the spare low bits of the quantised numbers, and two bytes are
 * deliberately written on top of the high byte of an earlier value, so the
 * order of the stores matters. Unlike the surrounding code this record is
 * stored least-significant byte first. */
/* @implements 0x10006F40 glide BrCarStatePack */
void BrCarStatePack(BrCarPacked *pDst, const BrCarState *pSrc)
{
    uint8_t *b = pDst->b;

    /* Orientation, low bit stolen for a flag. */
    *(uint16_t *)(b + 0x00) =
        (uint16_t)(((uint32_t)BrFixPackS16Q15Neg(pSrc->f00) & 0xFFFEu)
                   | (uint32_t)(pSrc->f8C != 0.0f));
    *(uint16_t *)(b + 0x02) =
        (uint16_t)(((uint32_t)BrFixPackS16Q15Neg(pSrc->f04) & 0xFFFEu)
                   | (uint32_t)(pSrc->f90 != 0.0f));
    *(uint16_t *)(b + 0x04) =
        (uint16_t)(((uint32_t)BrFixPackS16Q15Neg(pSrc->f08) & 0xFFFEu)
                   | (uint32_t)(pSrc->f94 != 0.0f));
    *(uint16_t *)(b + 0x06) =
        (uint16_t)(((uint32_t)BrFixPackS16Q15Neg(pSrc->f0C) & 0xFFFEu)
                   | (uint32_t)(pSrc->f98 != 0.0f));

    /* Position axis 1: three low bits are flags, the top byte is reused
     * below by the b[0x0B] store. */
    {
        /* Hoist order (f6C, f88, f9C) and the un-distributed `shl` both
         * come from spelling the flags as prior statements. */
        int32_t t6C = (pSrc->f6C != 0.0f);
        int32_t t88 = (pSrc->f88 != 0.0f);
        int32_t t9C = (pSrc->f9C != 0.0f);
        *(uint32_t *)(b + 0x08) =
            ((uint32_t)BrFixPackU24Q13(pSrc->f10) & 0xFFFFF8u)
            | ((uint32_t)((t9C << 1) | t88) << 1)
            | (uint32_t)t6C;
    }

    /* Position axis 2: two low bits are flags. */
    *(uint32_t *)(b + 0x0C) =
        ((uint32_t)BrFixPackU24Q13(pSrc->f14) & 0xFFFFFCu)
        | ((uint32_t)(pSrc->f70 != 0.0f) * 2)
        | (uint32_t)(pSrc->f74 != 0.0f);

    *(uint16_t *)(b + 0x10) = (uint16_t)BrFixPackS16Q7(pSrc->f18);
    b[0x12] = (uint8_t)BrFixPackS8Q3(pSrc->f34);
    {
        uint8_t t = (uint8_t)((uint8_t)BrFixPackS6Q7Neg(pSrc->f38) & 0x3Fu);
        t |= (uint8_t)(BrFixPackLevel(pSrc->f80) << 6);
        b[0x13] = t;
    }

    /* Overwrites the high byte of the dword at 0x08, which the mask above
     * left clear. Order is load-bearing. */
    b[0x0B] = (uint8_t)BrFixPackU8Angle(pSrc->f3C);

    /* Built incrementally: the original stores b[0x14]/b[0x15] after every
     * OR, exactly as these statements read. */
    b[0x14] = (uint8_t)((pSrc->f4C != 0.0f) ? 0x80u : 0u);
    b[0x14] |= (uint8_t)(((uint8_t)(int32_t)pSrc->f5C & 7u) << 4);
    b[0x14] |= (uint8_t)((pSrc->f50 != 0.0f) ? 8u : 0u);
    b[0x14] |= (uint8_t)((uint8_t)(int32_t)pSrc->f60 & 7u);

    b[0x15] = (uint8_t)((pSrc->f54 != 0.0f) ? 0x80u : 0u);
    b[0x15] |= (uint8_t)(((uint8_t)(int32_t)pSrc->f64 & 7u) << 4);
    b[0x15] |= (uint8_t)((pSrc->f58 != 0.0f) ? 8u : 0u);
    b[0x15] |= (uint8_t)((uint8_t)(int32_t)pSrc->f68 & 7u);

    /* Overwrites the high byte of the dword at 0x0C. */
    {
        uint8_t t = (uint8_t)((uint8_t)BrFixPackU8Range(pSrc->f7C) & 0x3Fu);
        t |= (uint8_t)(BrFixPackLevel(pSrc->f84) << 6);
        b[0x0F] = t;
    }
}
