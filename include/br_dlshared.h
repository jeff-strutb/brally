/* br_dlshared.h -- the display-list routines that BOTH builds share, each
 * transcribed ONCE.
 *
 * WHY THIS FILE EXISTS
 *
 * `port/src/drawing/br_dl.c` models the Glide display-list interpreter over a
 * raw byte stream; `port/src/slice2_16.c` models the D3D one over a typed
 * `BrGfxWords` pair.  Both are legitimate, and neither wants the other's
 * pointer model.  What they must NOT do is transcribe the same original
 * function twice, because that is how 0x10022120 / 0x10022DC0 came to
 * disagree about NaN for a long stretch -- one copy rejected an unordered
 * vertex and the other admitted it, and nothing structural could have found
 * that.  See CONVENTIONS.md, "Aliased storage: a link-clean bug".
 *
 * So the ARITHMETIC lives here, once, taking the two command words as plain
 * integers and handing back plain integers.  The two interpreters keep their
 * own decoding of the byte stream -- which is each module's own business --
 * and share the body.
 *
 * Every entry names both builds' addresses.  `config/shared.csv` classes all
 * of them `shared`/`body`, i.e. byte-identical after normalisation, and each
 * was re-checked here instruction by instruction against both DLLs: the only
 * differences are relocated global addresses.
 */
#ifndef BR_DLSHARED_H
#define BR_DLSHARED_H

#include <stdint.h>

/* --- the default handler: 0x10021240 (glide) / 0x100243D0 (d3d) ---------
 *
 *     mov eax, dword ptr [esp + 4]
 *     add eax, 8
 *     ret
 *
 * Eight bytes in both images.  228 of the 256 dispatch slots point at it.
 * There is no arithmetic to share beyond the step itself, so it is stated
 * here as the constant both interpreters advance by, rather than written out
 * twice as a number. */
#define BR_DLS_SKIP_BYTES 8

/* --- 0xF2 G_SETTILESIZE: 0x1001EC30 (glide) / 0x1001CF30 (d3d) ---------
 * 178 bytes in both.  Four 12-bit fields, each sign-folded by the original's
 * `cmp 0x800 / sub 0x1000` pair, then two spans computed as
 * `(lr - ul + 4) >> 2` with an ARITHMETIC shift -- so a negative span rounds
 * toward -inf and stays negative.  Preserved. */
typedef struct BrDlsTileSize {
    int32_t uls, ult, lrs, lrt;   /* 10.2, sign-folded  */
    int32_t tileW, tileH;         /* (lr - ul + 4) >> 2 */
} BrDlsTileSize;

void BrDlsTileSizeDecode(uint32_t w0, uint32_t w1, BrDlsTileSize *pOut);

/* --- the textured screen rectangles -------------------------------------
 *   0xE4  0x10021570 (glide) / 0x10021510 (d3d)   79 bytes, THREE commands
 *   0xE3  0x100219D0 (glide) / 0x10021B80 (d3d)   75 bytes, one command
 *
 * Both end in the same five-argument call (`0x100215C0` in BRGlide,
 * `0x10021560` in BRD3D) and the arguments are, in order,
 *
 *     ( (w1 >> 12) & 0xFFF,  w1 & 0xFFF,
 *       (w0 >> 12) & 0xFFF,  w0 & 0xFFF,  (w1 >> 24) & 7 )
 *
 * UNSIGNED throughout -- `and 0xFFF` with no sign fold, unlike 0xF2 above.
 *
 * THE ONE DIFFERENCE BETWEEN THE TWO, and it is the opposite of what
 * br_dl.c used to say: 0xE3 shifts every corner LEFT by two on the way in
 * (`shl edx,2` at 0x100219EE / 0x10021A04, and `shr ecx,0xA / and 0x3FFC`
 * for the high fields, which is the same `<<2` folded into the shift).  0xE4
 * shifts nothing.  So BOTH forms reach the callee in quarter-pixels, 0xE4
 * because its fields already are and 0xE3 because its whole-pixel fields are
 * scaled up.  Nothing anywhere divides by four.
 *
 * `fInteger` selects 0xE3.  The corners come back in QUARTER-PIXELS, which is
 * the unit the original passes on; a caller that wants whole pixels divides,
 * and should say so where it does. */
typedef struct BrDlsTileRect {
    int32_t ulx, uly;    /* from w1 -- arguments 0 and 1 */
    int32_t lrx, lry;    /* from w0 -- arguments 2 and 3 */
    int32_t tile;        /* argument 4 */
} BrDlsTileRect;

void BrDlsTileRectDecode(uint32_t w0, uint32_t w1, int fInteger,
                         BrDlsTileRect *pOut);

/* Command sizes, from the returns: 0xE4 leaves `esi` at cmd+0x10 and then
 * adds 8; 0xE3 returns `esi + 8`. */
#define BR_DLS_TILERECT_E4_BYTES 0x18
#define BR_DLS_TILERECT_E3_BYTES 0x08

/* --- frustum outcodes: 0x10022120 (glide) / 0x10022DC0 (d3d) -----------
 * 162 bytes in both.  Seven tests, every one of them
 * `fcomp 0.0 / fnstsw ax / test ah,1`, i.e. C0 -- and an UNORDERED compare
 * sets C0 as well, so a NaN takes the clip side and the vertex is rejected.
 * That is why all seven are written negated; `v < 0.0f` is false for NaN and
 * would report a NaN vertex as inside.
 *
 * THIS IS THE FUNCTION THE DUPLICATION COST.  The two copies disagreed on
 * exactly this for as long as both existed, and the banner over one of them
 * named the other.  There is now one.
 *
 * The bits are br_dl.h's BR_DL_CLIP_*: 0x01 w, 0x02 near, 0x04 far,
 * 0x08 left, 0x10 right, 0x20 bottom, 0x40 top -- and the ORDER of the
 * middle five is z, z, x, x, y, y, which is not the order the bit names
 * suggest.  Both builds agree on it. */
int32_t BrDlsClipCodes(float cx, float cy, float cz, float cw);

#endif /* BR_DLSHARED_H */
