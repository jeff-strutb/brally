/* br_dlproj.c -- drawing: the depth-buffer-off vertex projector.
 *
 * ONE function, 0x10023760, with ONE caller: 0x10023360, the lit vertex
 * transform 0x1001FD70 installs when the geometry mode has lighting on and
 * ZBUFFER off.  The call site (0x1002371E..0x10023731) pushes the clipped
 * vertex's node pointer and the three colour channels the lighting pass has
 * just computed, and it is guarded by the outcode test at 0x1002370F -- an
 * off-screen vertex is never projected.
 *
 * IT IS THE noZ HALF OF A PAIR.  0x10022070 is the same body with the depth
 * buffer ON; the two differ in exactly the two things br_dltrim.c's four
 * trimmers differ in, and for the same reasons:
 *
 *   - the snap rounds through a stack `int` here and through the global
 *     0x105CE310 there, and
 *   - this one overwrites 1/w with 1/65535 once x and y have been
 *     projected, so the card gets a constant w and textures linearly.
 *
 * THE SNAP IS INLINE ASM, exactly as br_dltrim.c establishes: a bare `fistp`
 * with no control-word change is not reachable from VC5 C, because every
 * `(int)float` is a `__ftol` call (docs/VC5-IDIOMS.md).  That is also why
 * this function keeps an EBP frame with no `sub esp` at all -- VC5 does not
 * omit the frame pointer in a function containing inline asm, and the four
 * float temps plus the int scratch are packed into the two dead argument
 * slots [ebp+8] and [ebp+0xc].
 *
 * The body is br_dltrim.c's emit loop with the texture-coordinate tail and
 * the pool recycling removed; every spelling below is that file's, which is
 * where each was proved.  Two placements of the 1/w flatten tie at
 * byte-exact (before the colours, and between r and g); the earlier one is
 * kept because it reads as "finish the position, then fill the vertex".
 *
 * br_dltrim.c's preamble is carried over verbatim.  Nothing is trimmed on
 * the grounds that it looks unused: the surrounding translation unit decides
 * VC5's codegen.
 */
#include <stdint.h>
#include "br_dl.h"       /* BrDlVtx -- the 0x68-byte pool record             */
#include "slice1_03.h"   /* BrClipVert, BrClipList, the seven planes         */

#ifdef BR_MATCHING_BUILD

/* The Glide 2.x GrVertex, two TMUs: 0x3C bytes.  BrDlVtx's first 0x3C bytes
 * are one of these; this function is handed a bare one by its caller, so it
 * needs the type on its own.  Same declaration as br_dltrim.c's. */
typedef struct BrProjGrVtx {
    float x, y, z;
    float r, g, b;
    float ooz;
    float a;
    float oow;
    float tmu0[3];
    float tmu1[3];
} BrProjGrVtx;

extern float       DAT_105ccd48;      /* viewport scale X                  */
extern float       DAT_105cd9f8;      /* viewport translate X              */
extern float       DAT_105ccfdc;      /* viewport scale Y                  */
extern float       DAT_105cd9fc;      /* viewport translate Y              */

/* The quarter-pixel snap.  `fld pre; fistp i; fild i; fstp back` round-trips
 * through the x87 with the startup control word, i.e. round to NEAREST,
 * TIES TO EVEN -- not the truncation `__ftol` would give.  `pre_` and
 * `back_` are named by the caller because the two axes use different
 * temporaries in the original: x's result comes back through the same slot
 * its product used, y's through the one that held 1/w. */
#define BR_PROJ_SNAP(fld_, dst_, pre_, back_)                           \
    do {                                                                \
        (pre_) = (fld_) * 4.0f;                                         \
        __asm { fld pre_ }                                              \
        __asm { fistp dst_ }                                            \
        (back_) = (float)(dst_);                                        \
        (fld_) = (back_) * 0.25f;                                       \
    } while (0)

/* WHAT IT DOES: turns one clipped, already-lit vertex into a finished Glide
 * screen vertex for a frame drawn with the depth buffer OFF.  It divides
 * through by depth to get perspective, applies the viewport scale and
 * offset, snaps the result to the nearest quarter of a pixel -- the
 * resolution the rasteriser works at -- stores the colour it is handed, and
 * then throws the real 1/w away and forces 1/65535 in its place, so the card
 * interpolates the texture linearly instead of perspective-correctly. */
/* @implements 0x10023760 glide BrDlProjectNoZ */
void BrDlProjectNoZ(BrProjGrVtx *pV, BrClipVert *pN,
                    float cr, float cg, float cb)
{
    float invW;
    float tmp;
    int   l;

    invW = 1.0f / pN->f18;
    *(uint32_t *)&pV->oow = *(uint32_t *)&invW;
    pV->x = ((pN->f04) * DAT_105ccd48) * invW + DAT_105cd9f8;
    l = 0;
    pV->y = ((pN->f08) * DAT_105ccfdc) * pV->oow + DAT_105cd9fc;
    pV->oow = 1.0f / 65535.0f;
    pV->r = cr;
    pV->g = cg;
    pV->b = cb;
    BR_PROJ_SNAP(pV->x, l, tmp, tmp);
    BR_PROJ_SNAP(pV->y, l, tmp, invW);
}

#endif /* BR_MATCHING_BUILD */
