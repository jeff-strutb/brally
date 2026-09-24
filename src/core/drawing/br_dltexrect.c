/* br_dltexrect.c -- drawing: the Glide build's textured screen rectangle.
 *
 * RESPONSIBILITY: drawing/ -- turn geometry and images into pixels.
 *
 * Filed out of slice4_51.c, an address batch.  0x100215C0 sits between the
 * two display-list rect handlers that call it (0x10021570 and 0x100219D0, in
 * br_dlshared.c); it has its own file rather than joining them because
 * br_dlshared.c carries T3-certified rows whose whole-image run is keyed to
 * that file's age.
 *
 * The D3D build fills the same renderer slot with a different body,
 * 0x10021560 (1,567 B): a pointer-reached state block, a null guard and a
 * deferred-state flush this one has none of.  See slice4_51.c's header note.
 *
 * BUILD FLAG: /O2 /Op.  Every int->float conversion in the original is
 * followed by `fstp dword [tmp]; fld dword [tmp]`, which /O2 alone never
 * emits; /Op also keeps `/ 2.0f` and `/ 4.0f` as divides.
 */
#include "slice4_51.h"

#define BR_GBI_RECT_C_ONE    1.0f    /* 0x10077404 -- becomes w           */
#define BR_GBI_RECT_C_UVSCL  8.0f    /* 0x1007741C -- scissor -> texcoord */

/* WHAT IT DOES: actually draws a screen rectangle. It converts the four
 * edges from the display list's pixel coordinates into the renderer's own
 * space, flipping the vertical axis and dividing through by the perspective
 * factor, builds the four corners with their texture coordinates and colour,
 * and hands over two triangles, wound one way or the other by a mode bit.
 * The tile number it is given is never used, in the original or here. */
/* @implements 0x100215C0 glide BrGbiCall10021560 */
/* Byte-exact (1,032 B), hand-transcribed from the Glide bytes; every named
 * relocation checked against the original's absolute and the three pooled
 * literals (1.0, 255.0, 8.0) against its .rdata.  Source facts:
 *  - the edges are UNSIGNED: each converts with the lo/hi=0 `fild qword`
 *    pair, MSVC's unsigned->double sequence;
 *  - the width is converted before the height (the first `fild` is
 *    0x100A7514); the height is kept as `cy` and reused as the memory
 *    operand of the flip;
 *  - the vertical flip happens where each edge is converted
 *    (`fLrt = cy - edge / FIXED`), so the x and y chains are the same depth
 *    and the scheduler runs them in source order.  Flipping inside the
 *    y expression instead costs two fxch;
 *  - the corner stores are written in the order the original emits them,
 *    which VC5 keeps as written. */
/* --- BRGlide.dll flat globals this function reads directly ------------ */
#define BR_GBI_RECT_C_RGB   255.0f   /* 0x10077418; the else-arm literal is
                                      * 0x437F0000 = 255.0f in the original */
extern float        BrGbiRectK_HALF;    /* 0x10077414 -- not foldable */
extern float        BrGbiRectK_FIXED;   /* 0x10077408 -- not foldable */
extern int          BrGbiRectG_A7514;
extern int          BrGbiRectG_A7518;
extern float        BrGbiRectG_A9A54;
extern float        BrGbiRectG_5D17C4;
extern int          BrGbiRectG_5CDA04;
extern float        BrGbiRectG_5CCD44;
extern float        BrGbiRectG_5CD9F4;
extern float        BrGbiRectG_5CCCF8;
extern float        BrGbiRectG_5D17A4;
extern float        BrGbiRectG_5D17B4;
extern float        BrGbiRectG_5CE2D0;
extern int          BrGbiRectG_5D17C8;
extern int          BrGbiRectG_18ED198;
extern int          BrGbiRectG_186C954;
extern int          BrGbiRectG_186C950;
extern int          BrGbiRectG_18EC988;

void BrGbiCall10021560(int lrs, int lrt, int uls, int ult, int tile)
{
    BrGbiRectVert v[4];
    float cy, w2, h2;
    float fLrs, fLrt, fUls, fUlt;
    float xLrs, xUls, yLrt, yUlt;
    float u0, u1, vt0, vt1;

    (void)tile;

    fLrs = (float)(unsigned int)lrs / BrGbiRectK_FIXED;

    w2 = (float)BrGbiRectG_A7514 / BrGbiRectK_HALF;
    cy = (float)BrGbiRectG_A7518;
    h2 = cy / BrGbiRectK_HALF;

    fUls = (float)(unsigned int)uls / BrGbiRectK_FIXED;
    fLrt = cy - (float)(unsigned int)lrt / BrGbiRectK_FIXED;
    fUlt = cy - (float)(unsigned int)ult / BrGbiRectK_FIXED;

    xLrs = ((fLrs - w2) / w2) / BrGbiRectG_A9A54;
    yLrt = ((fLrt - h2) / h2) / BrGbiRectG_A9A54;
    xUls = ((fUls - w2) / w2) / BrGbiRectG_A9A54;
    yUlt = ((fUlt - h2) / h2) / BrGbiRectG_A9A54;

    v[3].node.f04 = xLrs;
    v[1].node.f04 = xLrs;
    v[3].node.f08 = yLrt;
    v[0].node.f04 = xUls;
    v[0].node.f08 = yLrt;
    v[1].node.f08 = yUlt;
    v[2].node.f04 = xUls;
    v[2].node.f08 = yUlt;

    v[2].node.f0C = BrGbiRectG_5D17C4 / BrGbiRectG_A9A54;
    v[1].node.f0C = v[2].node.f0C;
    v[0].node.f0C = v[2].node.f0C;
    v[3].node.f0C = v[2].node.f0C;

    v[2].node.f18 = BR_GBI_RECT_C_ONE / BrGbiRectG_A9A54;
    v[1].node.f18 = v[2].node.f18;
    v[0].node.f18 = v[2].node.f18;
    v[3].node.f18 = v[2].node.f18;

    if (BrGbiRectG_5CDA04 != 0) {
        v[0].node.f1C = BrGbiRectG_5CCD44 * BR_GBI_RECT_C_RGB;
        v[3].node.f1C = v[0].node.f1C;
        v[0].node.f20 = BrGbiRectG_5CD9F4 * BR_GBI_RECT_C_RGB;
        v[3].node.f20 = v[0].node.f20;
        v[0].node.f24 = BrGbiRectG_5CCCF8 * BR_GBI_RECT_C_RGB;
        v[3].node.f24 = v[0].node.f24;

        v[2].node.f1C = BrGbiRectG_5D17A4;
        v[1].node.f1C = BrGbiRectG_5D17A4;
        v[2].node.f20 = BrGbiRectG_5D17B4;
        v[1].node.f20 = BrGbiRectG_5D17B4;
        v[2].node.f24 = BrGbiRectG_5CE2D0;
        v[1].node.f24 = BrGbiRectG_5CE2D0;
    } else {
        v[2].node.f1C = BR_GBI_RECT_C_RGB;
        v[1].node.f1C = BR_GBI_RECT_C_RGB;
        v[0].node.f1C = BR_GBI_RECT_C_RGB;
        v[3].node.f1C = BR_GBI_RECT_C_RGB;
        v[2].node.f20 = BR_GBI_RECT_C_RGB;
        v[1].node.f20 = BR_GBI_RECT_C_RGB;
        v[0].node.f20 = BR_GBI_RECT_C_RGB;
        v[3].node.f20 = BR_GBI_RECT_C_RGB;
        v[2].node.f24 = BR_GBI_RECT_C_RGB;
        v[1].node.f24 = BR_GBI_RECT_C_RGB;
        v[0].node.f24 = BR_GBI_RECT_C_RGB;
        v[3].node.f24 = BR_GBI_RECT_C_RGB;
    }

    u0  = (float)BrGbiRectG_18ED198 * BR_GBI_RECT_C_UVSCL;
    u1  = (float)BrGbiRectG_186C954 * BR_GBI_RECT_C_UVSCL;
    vt0 = (float)BrGbiRectG_186C950 * BR_GBI_RECT_C_UVSCL;
    vt1 = (float)BrGbiRectG_18EC988 * BR_GBI_RECT_C_UVSCL;

    v[3].node.f10 = u0;
    v[0].node.f10 = u1;
    v[3].node.f14 = vt1;
    v[0].node.f14 = vt1;
    v[1].node.f10 = u0;
    v[1].node.f14 = vt0;
    v[2].node.f10 = u1;
    v[2].node.f14 = vt0;

    if ((BrGbiRectG_5D17C8 & 0x1000) != 0) {
        BrGbiCall1001D420(&v[3], &v[0], &v[1]);
        BrGbiCall1001D420(&v[0], &v[2], &v[1]);
    } else {
        BrGbiCall1001D420(&v[1], &v[0], &v[3]);
        BrGbiCall1001D420(&v[1], &v[2], &v[0]);
    }
}
