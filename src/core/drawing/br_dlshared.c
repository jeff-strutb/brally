/* br_dlshared.c -- one body each for the display-list routines both builds
 * share.  See br_dlshared.h for why this file exists and for the evidence
 * behind every constant in it.
 */
#ifdef BR_MATCHING_BUILD
/* Header prototype is the portable four-float form.  The original takes one
 * clip-vertex pointer; hide that prototype so this TU compiles the matching
 * signature. */
#define BrDlsClipCodes BrDlsClipCodes_Portable
#endif
#include "br_dlshared.h"
#ifdef BR_MATCHING_BUILD
#undef BrDlsClipCodes
#endif

/* Sign-fold a 12-bit field.  The original spells it
 *      and edx,0xFFF ; cmp edx,0x800 ; jl .. ; sub edx,0x1000
 * so 0x800 itself IS folded (the branch is `jl`, not `jle`) and the range is
 * -2048 .. 2047. */
static int32_t br_dls_sext12(uint32_t v)
{
    int32_t x = (int32_t)(v & 0xFFFu);
    return (x < 0x800) ? x : (x - 0x1000);
}

/* WHAT IT DOES: reads the drawing command that says which rectangle of a
 * texture the next drawings will use, and works out that rectangle's width
 * and height in texture pixels. Sign is kept throughout, so a rectangle
 * given back to front stays back to front rather than becoming enormous. */
/* @implements 0x1001EC30 glide BrDlsTileSizeDecode */
/* @implements 0x1001CF30 d3d BrDlsTileSizeDecode */
void BrDlsTileSizeDecode(uint32_t w0, uint32_t w1, BrDlsTileSize *pOut)
{
    pOut->uls = br_dls_sext12(w0 >> 12);
    pOut->ult = br_dls_sext12(w0);
    pOut->lrs = br_dls_sext12(w1 >> 12);
    pOut->lrt = br_dls_sext12(w1);
    /* `sar eax,2` -- arithmetic, so a negative span rounds toward -inf. */
    pOut->tileW = (pOut->lrs - pOut->uls + 4) >> 2;
    pOut->tileH = (pOut->lrt - pOut->ult + 4) >> 2;
}

/* WHAT IT DOES: reads the drawing command that puts a piece of a texture
 * straight onto the screen -- the command behind heads-up panels and menu
 * artwork -- and pulls out its four corners and which loaded texture to use.
 * The command comes in two forms, one giving the corners in quarter-pixels
 * and one in whole pixels, and the whole-pixel form is scaled up here so both
 * hand on the same units. */
/* @implements 0x10021570 glide BrDlsTileRectDecode */
/* @implements 0x10021510 d3d BrDlsTileRectDecode */
/* @implements 0x100219D0 glide BrDlsTileRectDecode */
/* @implements 0x10021B80 d3d BrDlsTileRectDecode */
void BrDlsTileRectDecode(uint32_t w0, uint32_t w1, int fInteger,
                         BrDlsTileRect *pOut)
{
    pOut->ulx  = (int32_t)((w1 >> 12) & 0xFFFu);
    pOut->uly  = (int32_t)(w1 & 0xFFFu);
    pOut->lrx  = (int32_t)((w0 >> 12) & 0xFFFu);
    pOut->lry  = (int32_t)(w0 & 0xFFFu);
    pOut->tile = (int32_t)((w1 >> 24) & 7u);

    /* 0xE3 only.  `shl edx,2` on the two low fields and `shr ecx,0xA /
     * and 0x3FFC` on the two high ones -- the same multiply by four, folded
     * into the shift.  0xE4 does none of this. */
    if (fInteger) {
        pOut->ulx <<= 2;
        pOut->uly <<= 2;
        pOut->lrx <<= 2;
        pOut->lry <<= 2;
    }
}

/* WHAT IT DOES: checks whether a transformed vertex has fallen outside the
 * viewing frustum and reports which of the seven boundaries it crossed, so
 * the triangle assembler knows whether to clip the triangle, keep it or throw
 * it away. A coordinate that is not a number counts as outside on every
 * boundary it appears in. */
/* @implements 0x10022120 glide BrDlsClipCodes */
/* @implements 0x10022DC0 d3d BrDlsClipCodes */
#ifdef BR_MATCHING_BUILD
/* Original argument is the clip-vertex record, not four scalars:
 *     +0x04 x, +0x08 y, +0x0C z, +0x18 w
 * First bit is a store (`mov edx,1`) because edx was just zeroed; the rest
 * are `or`. */
int32_t BrDlsClipCodes(const float *pV)
{
    int32_t oc = 0;

    /* NEGATED, ALL SEVEN -- see the header.  `!(v >= 0)` and not `v < 0`,
     * because the original's `test ah,1` is true for UNORDERED too. */
    if (!(pV[6] >= 0.0f))           oc  = 0x01;   /* w      */
    if (!(pV[3] + pV[6] >= 0.0f))   oc |= 0x02;   /* near   */
    if (!(pV[6] - pV[3] >= 0.0f))   oc |= 0x04;   /* far    */
    if (!(pV[1] + pV[6] >= 0.0f))   oc |= 0x08;   /* left   */
    if (!(pV[6] - pV[1] >= 0.0f))   oc |= 0x10;   /* right  */
    if (!(pV[2] + pV[6] >= 0.0f))   oc |= 0x20;   /* bottom */
    if (!(pV[6] - pV[2] >= 0.0f))   oc |= 0x40;   /* top    */
    return oc;
}
#else
int32_t BrDlsClipCodes(float cx, float cy, float cz, float cw)
{
    int32_t oc = 0;

    /* NEGATED, ALL SEVEN -- see the header.  `!(v >= 0)` and not `v < 0`,
     * because the original's `test ah,1` is true for UNORDERED too. */
    if (!(cw >= 0.0f))        oc |= 0x01;   /* w      */
    if (!(cz + cw >= 0.0f))   oc |= 0x02;   /* near   */
    if (!(cw - cz >= 0.0f))   oc |= 0x04;   /* far    */
    if (!(cx + cw >= 0.0f))   oc |= 0x08;   /* left   */
    if (!(cw - cx >= 0.0f))   oc |= 0x10;   /* right  */
    if (!(cy + cw >= 0.0f))   oc |= 0x20;   /* bottom */
    if (!(cw - cy >= 0.0f))   oc |= 0x40;   /* top    */
    return oc;
}
#endif
