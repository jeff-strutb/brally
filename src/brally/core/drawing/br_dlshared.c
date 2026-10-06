/* br_dlshared.c -- one body each for the display-list routines both builds
 * share.  See br_dlshared.h for why this file exists and for the evidence
 * behind every constant in it.
 */
/* Header prototype is the portable four-float form.  The original takes one
 * clip-vertex pointer; hide that prototype so this TU compiles the matching
 * signature. */
#define BrDlsClipCodes BrDlsClipCodes_Portable
/* Port decoders write a struct.  Originals are 1-arg DL walkers that
 * store globals / call 0x100215C0. */
#define BrDlsTileSizeDecode BrDlsTileSizeDecode_Portable
#define BrDlsTileRectDecode BrDlsTileRectDecode_Portable
#include "br_dlshared.h"
#undef BrDlsClipCodes
#undef BrDlsTileSizeDecode
#undef BrDlsTileRectDecode

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
/* @t4-pass 0x1001EC30 1 2026-09-07 probes 33 bytes 178 insns 45 regions 5 rows 2 census yes  (tools/brally/crank.py) */
/* @t4-pass 0x1001EC30 2 2026-09-07 probes 33 bytes 178 insns 45 regions 5 rows 2 census yes  (tools/brally/crank.py) */
/* @t4-pass 0x1001EC30 3 2026-09-09 probes 10 bytes 178 insns 45 regions 4 rows 0 census no  (hand, fn.py variants: decl orders, mask/shift/guard spellings, q local, register hint, all inert or worse) */
/* @t4-pass 0x1001EC30 4 2026-09-09 probes 11 bytes 178 insns 45 regions 4 rows 0 census yes  (hand, fn.py variants: word temp, param copy, return/diff spellings, all inert; corpus MISS at +0x1 len 12 -- the between-pushes parameter load is proven nowhere) */
/* @implements 0x1001EC30 glide BrDlsTileSizeDecode */
extern int DAT_118ed198;
extern int DAT_1186c950;
extern int DAT_1186c954;
extern int DAT_118ec988;
extern int DAT_1186c958;
extern int DAT_118ed1ac;

/* ult has no local: it is formed, folded and read back through its global
 * (DAT_1186c950).  With a local VC5 gave the local esi and the command
 * pointer edi; the original keeps the pointer in esi and ult in edi, which is
 * what this spelling produces. */
unsigned char *BrDlsTileSizeDecode(unsigned char *p)
{
    int uls, lrs, lrt;

    uls = (*(unsigned *)p >> 12) & 0xFFF;
    DAT_118ed198 = uls;
    if (uls >= 0x800) {
        uls -= 0x1000;
        DAT_118ed198 = uls;
    }
    DAT_1186c950 = *(unsigned *)p & 0xFFF;
    if (DAT_1186c950 >= 0x800)
        DAT_1186c950 -= 0x1000;
    lrs = (*(unsigned *)(p + 4) >> 12) & 0xFFF;
    DAT_1186c954 = lrs;
    if (lrs >= 0x800) {
        lrs -= 0x1000;
        DAT_1186c954 = lrs;
    }
    lrt = *(unsigned *)(p + 4) & 0xFFF;
    DAT_118ec988 = lrt;
    if (lrt >= 0x800) {
        lrt -= 0x1000;
        DAT_118ec988 = lrt;
    }
    DAT_1186c958 = (lrs - uls + 4) >> 2;
    DAT_118ed1ac = (lrt - DAT_1186c950 + 4) >> 2;
    return p + 8;
}

/* WHAT IT DOES: reads the drawing command that puts a piece of a texture
 * straight onto the screen -- the command behind heads-up panels and menu
 * artwork -- and pulls out its four corners and which loaded texture to use.
 * The command comes in two forms, one giving the corners in quarter-pixels
 * and one in whole pixels, and the whole-pixel form is scaled up here so both
 * hand on the same units. */
void FUN_100215c0(int, int, int, int, int);

/* 0xE4 -- 10.2 corners, 24-byte command (texrect + two extra words). */
/* WHAT IT DOES: unpack the display-list command that draws a textured
 * rectangle whose corners are already in QUARTER-pixel units, and hand them
 * to the rectangle drawer. It consumes 0x18 bytes, not 8, because the
 * texture coordinates follow the command. The whole-pixel twin is
 * BrDlsTileRectE3. */
/* Byte-exact 2026-09-28, hand-transcribed.  The original computes every
 * field before the first push (five values live at once, hence ebx), and
 * the source that reproduces that is a few scratch words reused: VC5 sinks a
 * field into the argument list unless the word it was computed from is
 * overwritten before the call.  So w0 is copied to t before its low field is
 * masked and v is reloaded with w1; lrx is taken from t before t is reused
 * for w1; ulx is taken before v is masked in place; the masked v is parked
 * in u and v then carries tile.  The pointer steps are separate statements
 * (+0x10 up front, +4 +4 after the call) -- a single `return p + 8` folds to
 * `lea` where the original adds in place. */
/* @implements 0x10021570 glide BrDlsTileRectE4 */
unsigned char *BrDlsTileRectE4(unsigned char *p)
{
    unsigned v, t, u, lry, lrx, ulx, tile;

    v = *(unsigned *)p;
    p += 0x10;
    t = v;
    lry = v & 0xFFF;
    v = *(unsigned *)(p - 0xc);
    lrx = (t >> 12) & 0xFFF;
    t = v;
    tile = (t >> 24) & 7;
    ulx = (v >> 12) & 0xFFF;
    v &= 0xFFF;
    u = v;
    v = tile;
    FUN_100215c0(ulx, u, lrx, lry, v);
    p += 4;
    p += 4;
    return p;
}

/* 0xE3 -- integer corners scaled <<2, 8-byte command. */
/* WHAT IT DOES: unpack the display-list command that draws a textured
 * rectangle whose corners are given as WHOLE pixels, scaling them up to the
 * quarter-pixel units the renderer works in, and hand the result to the
 * rectangle drawer. The quarter-pixel twin is BrDlsTileRectE4. */
/* @implements 0x100219D0 glide BrDlsTileRectE3 */
unsigned char *BrDlsTileRectE3(unsigned char *p)
{
    unsigned w1, w0;

    w1 = *(unsigned *)(p + 4);
    w0 = *(unsigned *)p;
    FUN_100215c0((w1 >> 10) & 0x3FFC, (w1 & 0xFFF) << 2,
                 (w0 >> 10) & 0x3FFC, (w0 & 0xFFF) << 2, (w1 >> 24) & 7);
    return p + 8;
}

/* WHAT IT DOES: checks whether a transformed vertex has fallen outside the
 * viewing frustum and reports which of the seven boundaries it crossed, so
 * the triangle assembler knows whether to clip the triangle, keep it or throw
 * it away. A coordinate that is not a number counts as outside on every
 * boundary it appears in. */
/* @implements 0x10022120 glide BrDlsClipCodes */
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
