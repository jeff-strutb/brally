/* br_dltrim.c -- drawing: the clipped-triangle trimmers.
 *
 * A display-list triangle whose three corners are not all on screen, and
 * not all off the same edge, is handed here.  The trimmer threads the three
 * vertices' clip nodes (the 0x28-byte record overlaid on BrDlVtx at +0x40)
 * into a circular list, runs it through the seven homogeneous clip planes in
 * br_dlclip.c, and either gives the borrowed pool nodes back (fewer than
 * three corners survived) or projects every survivor into a Glide vertex on
 * the stack and draws the polygon.
 *
 * THE FAMILY.  Four functions, ONE body: they differ only in whether the
 * colour comes from the vertex or from three extra arguments (FLAT), and in
 * whether the card is depth-buffering (Z) or not:
 *
 *     0x1001EE70   Z    shaded    607 B   (br_dl.c carries the port of it)
 *     0x10020190   Z    flat      607 B
 *     0x10020690   noZ  flat      609 B
 *     0x10020A80   noZ  shaded    609 B
 *
 * 0x10020690 and 0x10020A80 differ in 54 bytes, every one of them a call
 * displacement, a colour load, or a stack slot that moved because the flat
 * variant's colour lives in the argument slots.  The Z / noZ pair differ in
 * exactly two things: the no-Z body snaps through a stack `int` instead of
 * the global 0x105CE310, and it overwrites 1/w with 1/65536 once x and y
 * have been projected, so the card gets a constant w and linear texturing.
 *
 * THE SNAP IS INLINE ASM.  Quarter-pixel snapping rounds through a bare
 * `fistp` with no control-word change, which VC5 cannot emit from C (every
 * `(int)float` is a `__ftol` call; docs/VC5-IDIOMS.md).  The original
 * therefore carried an `__asm` block, and that is also why all four keep an
 * EBP frame while their unclipped siblings (0x1001FF60, 0x10020460) do not:
 * VC5 does not omit the frame pointer in a function containing inline asm.
 * The matching arm spells the block out; the port arm is in br_dl.c.
 */
#include <stdint.h>
#include "br_dl.h"       /* BrDlVtx -- the 0x68-byte pool record             */
#include "slice1_03.h"   /* BrClipVert, BrClipList, the seven planes         */

#ifdef BR_MATCHING_BUILD

/* The Glide 2.x GrVertex, two TMUs: 0x3C bytes.  BrDlVtx's first 0x3C bytes
 * are one of these, but the trimmer builds its OUTPUT on the stack at the
 * bare stride, so it needs the type on its own. */
typedef struct BrGrVtx {
    float x, y, z;
    float r, g, b;
    float ooz;
    float a;
    float oow;
    float tmu0[3];
    float tmu1[3];
} BrGrVtx;

/* Nine of them: the frame is 0x224 = 8 (the list) + 9 * 0x3C. */
#define BR_TRIM_OUT_MAX  9

extern BrClipVert *DAT_105cda00;      /* free-list head                    */
extern int32_t     DAT_105ce310;      /* the Z variants' fistp scratch     */
extern float       DAT_105ccd48;      /* viewport scale X                  */
extern float       DAT_105cd9f8;      /* viewport translate X              */
extern float       DAT_105ccfdc;      /* viewport scale Y                  */
extern float       DAT_105cd9fc;      /* viewport translate Y              */
extern float       g_brDlTexScaleS;   /* 0x118ED1A4                        */
extern float       g_brDlTexScaleT;   /* 0x118ED1A8                        */

/* glide2x, through the import thunks.  Both are __stdcall: no `add esp`
 * follows either call in any of the four originals. */
extern void __stdcall grDrawTriangle(const void *a, const void *b,
                                     const void *c);           /* 0x100729EA */
extern void __stdcall grDrawPolygonVertexList(int n,
                                              const void *v);  /* 0x100729FC */

#define BR_TRIM_POOL_LO  0x105CCFF0uL
#define BR_TRIM_POOL_HI  0x105CD9F0uL

/* The quarter-pixel snap.  `fld tmp; fistp i; fild i; fstp tmp` round-trips
 * through the x87 with the startup control word, i.e. round to nearest,
 * ties to even. */
#define BR_TRIM_SNAP(fld_, dst_, pre_, back_)                           \
    do {                                                                \
        (pre_) = (fld_) * 4.0f;                                         \
        __asm { fld pre_ }                                              \
        __asm { fistp dst_ }                                            \
        (back_) = (float)(dst_);                                        \
        (fld_) = (back_) * 0.25f;                                       \
    } while (0)

/* The colour sources.  Both variants store the three colours from cr/cg/cb;
 * the flat variant has them as arguments, the shaded one loads them from the
 * node at the top of the loop body (Ghidra reads b, then g, then r there). */
#define BR_TRIM_COLDECL_FLAT
#define BR_TRIM_COLDECL_VTX     float cr, cg, cb;
#define BR_TRIM_COLLOAD_FLAT
#define BR_TRIM_COLLOAD_VTX     cb = pN->f24; cg = pN->f20; cr = pN->f1C;

/* What the two depth modes do after y is projected. */
#define BR_TRIM_Z_KEEP(pv_)   ((void)0)
#define BR_TRIM_Z_FLATTEN(pv_) ((pv_)->oow = 1.0f / 65535.0f)

/* The clip node overlaid on a pool vertex at +0x40. */
#define BR_TRIM_NODE(v)  ((BrClipVert *)&(v)->f40)

/* One plane, then "is it still a polygon?" -- the seven are chained with
 * && so the first failure skips the rest. */
#define BR_TRIM_STEP(fn)  ((fn)(&list), list.cVerts >= 3)

#define BR_TRIM_BODY(NAME, ARGS, COLDECL, COLLOAD, DEPTH, SNAPDST, SNAPDECL, SNAPINIT, SNAPPRE) \
void NAME ARGS                                                              \
{                                                                           \
    BrClipList  list;                                                       \
    BrGrVtx     out[BR_TRIM_OUT_MAX];                                       \
    BrClipVert *pN;                                                         \
    BrClipVert *pC, *pB, *pA;                                               \
    BrGrVtx    *pV;                                                         \
    float       invW;                                                       \
    float       tmp;                                                        \
    int         i, n;                                                       \
    SNAPDECL                                                                \
    COLDECL                                                                 \
                                                                            \
    pC = BR_TRIM_NODE(c);                                                   \
    pB = BR_TRIM_NODE(b);                                                   \
    pA = BR_TRIM_NODE(a);                                                   \
    c->f40 = 0.0f;                                                          \
    pB->pNext = pC;                                                         \
    pA->pNext = pB;                                                         \
    pC->pNext = list.pHead = pA;                                            \
    list.cVerts = 3;                                                        \
                                                                            \
    if (!BR_TRIM_STEP(BrClipPlaneWPlusF0C))  goto fail;                     \
    if (!BR_TRIM_STEP(BrClipPlaneWPlusF04))  goto fail;                     \
    if (!BR_TRIM_STEP(BrClipPlaneWMinusF04)) goto fail;                     \
    if (!BR_TRIM_STEP(BrClipPlaneWMinusF08)) goto fail;                     \
    if (!BR_TRIM_STEP(BrClipPlaneWMinusF0C)) goto fail;                     \
    if (!BR_TRIM_STEP(BrClipPlaneWPlusF08))  goto fail;                     \
    if (!BR_TRIM_STEP(BrClipPlaneW)) {                                      \
    fail:                                                                   \
        for (n = list.cVerts; n > 0; n--) {                                 \
            pN = list.pHead;                                                \
            list.pHead = pN->pNext;                                         \
            if ((unsigned long)pN >= BR_TRIM_POOL_LO &&                     \
                (unsigned long)pN <  BR_TRIM_POOL_HI) {                     \
                pN->pNext = DAT_105cda00;                                   \
                DAT_105cda00 = pN;                                          \
            }                                                               \
        }                                                                   \
    } else {                                                                \
        pV = out;                                                           \
        for (i = 0; i < list.cVerts; i++, pV++) {                           \
            pN = list.pHead;                                                \
            list.pHead = pN->pNext;                                         \
            COLLOAD                                                         \
            invW = 1.0f / pN->f18;                                          \
            *(uint32_t *)&pV->oow = *(uint32_t *)&invW;                     \
            pV->x = ((pN->f04) * DAT_105ccd48) * invW + DAT_105cd9f8;       \
            SNAPINIT                                                        \
            pV->y = ((pN->f08) * DAT_105ccfdc) * pV->oow + DAT_105cd9fc;    \
            pV->r = cr;                                                     \
            pV->g = cg;                                                     \
            pV->b = cb;                                                     \
            DEPTH(pV);                                                      \
            BR_TRIM_SNAP(pV->x, SNAPDST, SNAPPRE, SNAPPRE);                 \
            BR_TRIM_SNAP(pV->y, SNAPDST, SNAPPRE, invW);                    \
            pV->tmu1[2] = pV->oow;                                          \
            pV->tmu0[2] = pV->oow;                                          \
            invW = ((pN->f10) * g_brDlTexScaleS) * pV->oow;                 \
            pV->tmu1[0] = invW;                                             \
            pV->tmu0[0] = invW;                                             \
            invW = ((pN->f14) * g_brDlTexScaleT) * pV->oow;                 \
            pV->tmu1[1] = invW;                                             \
            pV->tmu0[1] = invW;                                             \
            if ((unsigned long)pN >= BR_TRIM_POOL_LO &&                     \
                (unsigned long)pN <  BR_TRIM_POOL_HI) {                     \
                pN->pNext = DAT_105cda00;                                   \
                DAT_105cda00 = pN;                                          \
            }                                                               \
        }                                                                   \
        if (list.cVerts == 3) {                                             \
            grDrawTriangle(&out[0], &out[1], &out[2]);                      \
            return;                                                         \
        }                                                                   \
        grDrawPolygonVertexList(list.cVerts, out);                          \
    }                                                                       \
}

#define BR_TRIM_ARGS_FLAT   (BrDlVtx *a, BrDlVtx *b, BrDlVtx *c,            \
                             float cr, float cg, float cb)
#define BR_TRIM_ARGS_VTX    (BrDlVtx *a, BrDlVtx *b, BrDlVtx *c)
#define BR_TRIM_LOCAL_SNAP  int l;
#define BR_TRIM_GLOBAL_SNAP
#define BR_TRIM_GLOBAL_INIT
#define BR_TRIM_LOCAL_INIT  l = 0;
#define BR_TRIM_NO_LOCAL


/* ‼ WHAT DECIDED THE LAYOUT, 2026-09-05 -- both instantiations byte-exact.
 * The body was 196/196 instructions with every instruction right for a whole
 * session; the only defect was where VC5 PUT two blocks:
 *
 *     original   [prologue][7 plane calls][GIVE-UP loop][EMIT loop][draws]
 *     ours       [prologue][7 plane calls][EMIT loop][draws][GIVE-UP loop]
 *
 * so the original's first six plane checks are SHORT `jl` (2 bytes) into a
 * give-up block sitting inline and ours were NEAR `jl` (6 bytes) to one at
 * the end: +24 bytes, 609 -> 633, all of it in those six jumps.
 *
 * THE LEVER: the give-up code must be the THEN arm of the LAST plane test,
 * with the `fail:` label INSIDE that arm, and the six earlier tests reaching
 * it by `goto fail`.  Written that way the compiler lays the failure arm
 * first and the six early exits become short jumps into it.  Everything else
 * about the shape is inert -- `< 3` versus `!(>= 3)`, `for (n = cVerts; n >
 * 0; n--)` versus a while loop, the arm ending in `return;` with the emit
 * code following versus the emit code in an `else`, and the step spelled out
 * versus wrapped in BR_TRIM_STEP.
 *
 * ‼ AND ARM ORDER IS THE WHOLE THING: `if (cVerts >= 3) { emit } else {
 * fail: giveup }` -- same control-flow graph, same goto, label still inside
 * an arm -- reverts exactly to the 633-byte defect.  The FAILURE arm has to
 * be the one the compiler lays first.  See docs/VC5-IDIOMS.md, "a lone
 * if (x) F else S is failure-first".
 *
 * DEAD, do not re-run (all /O2 /Op, all give the give-up block at the END
 * with near jumps, byte-for-byte identical output):
 *   - `if ((NEAR(),n<3) || ...) { giveup; return; } emit...`
 *   - the same with `{ giveup } else { emit }`
 *   - `if ((NEAR(),n>=3) && ...) { emit } else { giveup }`
 *   - `... && ...) goto emit; giveup; return; emit: ...`
 *   - `... && ...) { emit; return; } giveup`
 *   - per-step `if (n < 3) goto fail;` with the `fail:` label at FUNCTION
 *     scope rather than inside the last test's arm
 *   - seven nested `if (n >= 3) {` with the emit block innermost
 *
 * The two Z variants, 0x10020190 and 0x1001EE70, are this body again with the
 * snap going through the global 0x105CE310 instead of a stack slot and
 * without the 1/w flatten; they are not instantiated here yet. */
/* WHAT IT DOES: trims one flat-coloured triangle against the screen edges
 * and the near/far planes with the depth buffer OFF, then draws whatever is
 * left as a Glide polygon.  The three colour arguments are the flat colour;
 * every emitted corner gets 1/w forced to 1/65535 so the card interpolates
 * texture linearly. */
/* @implements 0x10020690 glide BrDlClipTriFlatNoZ */
BR_TRIM_BODY(BrDlClipTriFlatNoZ, BR_TRIM_ARGS_FLAT,
             BR_TRIM_COLDECL_FLAT, BR_TRIM_COLLOAD_FLAT, BR_TRIM_Z_FLATTEN,
             l, BR_TRIM_LOCAL_SNAP, BR_TRIM_LOCAL_INIT, tmp)

/* WHAT IT DOES: trims one Gouraud-coloured triangle against the screen
 * edges and the near/far planes with the depth buffer OFF, carrying each
 * corner's own colour across the cut, then draws whatever is left as a
 * Glide polygon with 1/w forced to 1/65535. */
/* @implements 0x10020A80 glide BrDlClipTriNoZ */
BR_TRIM_BODY(BrDlClipTriNoZ, BR_TRIM_ARGS_VTX,
             BR_TRIM_COLDECL_VTX, BR_TRIM_COLLOAD_VTX, BR_TRIM_Z_FLATTEN,
             l, BR_TRIM_LOCAL_SNAP, BR_TRIM_LOCAL_INIT, tmp)

/* WHAT IT DOES: trims one flat-coloured triangle against the screen edges
 * and the near/far planes with the depth buffer ON, then draws whatever is
 * left as a Glide polygon.  The three colour arguments are the flat colour;
 * each surviving corner keeps its own 1/w, so the card interpolates the
 * texture perspective-correctly. */
/* @implements 0x10020190 glide BrDlClipTriFlatZ */
BR_TRIM_BODY(BrDlClipTriFlatZ, BR_TRIM_ARGS_FLAT,
             BR_TRIM_COLDECL_FLAT, BR_TRIM_COLLOAD_FLAT, BR_TRIM_Z_KEEP,
             DAT_105ce310, BR_TRIM_GLOBAL_SNAP, BR_TRIM_GLOBAL_INIT, tmp)

/* WHAT IT DOES: trims one Gouraud-coloured triangle against the screen edges
 * and the near/far planes with the depth buffer ON, carrying each corner's
 * own colour across the cut, then draws whatever is left as a Glide polygon
 * with each corner keeping its own 1/w. */
/* @implements 0x1001EE70 glide BrDlClipTriZ */
BR_TRIM_BODY(BrDlClipTriZ, BR_TRIM_ARGS_VTX,
             BR_TRIM_COLDECL_VTX, BR_TRIM_COLLOAD_VTX, BR_TRIM_Z_KEEP,
             DAT_105ce310, BR_TRIM_GLOBAL_SNAP, BR_TRIM_GLOBAL_INIT, tmp)

#endif /* BR_MATCHING_BUILD */
