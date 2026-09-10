/* br_dltri2.c -- drawing.  The z-buffered G_TRI2 handler, 0x1001FA30, in
 * its own translation unit.
 *
 * WHY ITS OWN FILE: br_dlcmd.c's residue note for this function measured
 * that a third user of the index-form triangle macro in that TU un-matches
 * both single-triangle siblings (0x1001ECF0 and 0x10020900); re-measured
 * 2026-09-09, 0x10020900 went byte-exact -> 31 diffs the moment this body
 * joined the file.  Surroundings decide codegen, so the macros are
 * DUPLICATED here verbatim from br_dlcmd.c (the same duplicated-leaf pattern
 * br_dl.c / br_dlcmd.c use for the quarter-pixel snap).  Keep them in step.
 */
#include "br_dlcmd.h"

#include <math.h>
#include <string.h>

#ifdef BR_MATCHING_BUILD

extern BrDlVtx g_aBrDlVtxPool[];    /* 0x105CE318, stride 0x68 */
extern float   g_brDlTexScaleS;     /* 0x118ED1A4 */
extern float   g_brDlTexScaleT;     /* 0x118ED1A8 */
extern void    BrDlClipTri(BrDlVtx *a, BrDlVtx *b, BrDlVtx *c);  /* 0x1001EE70 */
/* 0x100729EA is the glide2x grDrawTriangle import thunk; Glide is __stdcall. */
extern void __stdcall grDrawTriangle(BrDlVtx *a, BrDlVtx *b, BrDlVtx *c);
#define BrDlDrawTri grDrawTriangle

/* The clip node overlaid on a vertex at +0x40 -- see br_dlcmd.c. */
typedef struct BrDlClipSt {
    float aLink[4];
    float s, t;
} BrDlClipSt;
extern void BrDlVtxFinishTex(BrDlVtx *v, const BrDlClipSt *pSt);  /* 0x1001FCF0 */

#define BR_DL_PUN(dst, src)                                          \
    (*(uint32_t *)(void *)&(dst) = *(const uint32_t *)(const void *)&(src))

#define V(i) g_aBrDlVtxPool[i]

/* Verbatim from br_dlcmd.c: one pointer, and only for the stores. */
#define BR_DLCMD_FINISH_VTX_I(i, u_)                                \
    do {                                                            \
        BrDlVtx *pv_ = &V(i);                                       \
        uint32_t w_;                                                \
        BR_DL_PUN(w_, V(i).oow);                                    \
        BR_DL_PUN(pv_->tmu1[2], w_);                                \
        BR_DL_PUN(pv_->tmu0[2], w_);                                \
        (u_) = V(i).s * g_brDlTexScaleS * pv_->oow;                 \
        BR_DL_PUN(pv_->tmu1[0], (u_));                              \
        BR_DL_PUN(pv_->tmu0[0], (u_));                              \
        (u_) = V(i).t * g_brDlTexScaleT * pv_->oow;                 \
        BR_DL_PUN(pv_->tmu1[1], (u_));                              \
        BR_DL_PUN(pv_->tmu0[1], (u_));                              \
    } while (0)

/* The second triangle's first two corners: the `s * scale` product is NAMED
 * so it is formed before the multiply by 1/w (the flat product canonicalises
 * the other way round there; measured on the no-Z twin 0x10020D70). */
#define BR_DLCMD_FINISH_VTX_I_N(i, u_)                              \
    do {                                                            \
        BrDlVtx *pv_ = &V(i);                                       \
        uint32_t w_;                                                \
        float    ts_;                                               \
        BR_DL_PUN(w_, V(i).oow);                                    \
        BR_DL_PUN(pv_->tmu1[2], w_);                                \
        BR_DL_PUN(pv_->tmu0[2], w_);                                \
        ts_ = V(i).s * g_brDlTexScaleS;                             \
        (u_) = ts_ * pv_->oow;                                      \
        BR_DL_PUN(pv_->tmu1[0], (u_));                              \
        BR_DL_PUN(pv_->tmu0[0], (u_));                              \
        ts_ = V(i).t * g_brDlTexScaleT;                             \
        (u_) = ts_ * pv_->oow;                                      \
        BR_DL_PUN(pv_->tmu1[1], (u_));                              \
        BR_DL_PUN(pv_->tmu0[1], (u_));                              \
    } while (0)

#define BR_DLCMD_TRI_I(ia, ib, ic, u_)                                  \
    do {                                                                \
        if ((V(ia).outcode & (V(ib).outcode & V(ic).outcode)) == 0) {    \
            if ((V(ib).outcode | V(ic).outcode | V(ia).outcode) != 0) {  \
                BrDlClipTri(&V(ia), &V(ib), &V(ic));                     \
            } else {                                                     \
                BR_DLCMD_FINISH_VTX_I_N(ia, u_);                         \
                BR_DLCMD_FINISH_VTX_I_N(ib, u_);                         \
                BR_DLCMD_FINISH_VTX_I_N(ic, u_);                         \
                BrDlDrawTri(&V(ia), &V(ib), &V(ic));                     \
            }                                                            \
        }                                                                \
    } while (0)

/* The second triangle: its LAST corner is finished by the out-of-line
 * BrDlVtxFinishTex against its own clip node. */
/* The ib corner of the textured pair: w_ through the POINTER (the no-Z
 * twin's residue site, probed per-TU here). */
#define BR_DLCMD_FINISH_VTX_I_N2(i, u_)                             \
    do {                                                            \
        BrDlVtx *pv_ = &V(i);                                       \
        uint32_t w_;                                                \
        float    ts_;                                               \
        BR_DL_PUN(w_, pv_->oow);                                    \
        BR_DL_PUN(pv_->tmu1[2], w_);                                \
        BR_DL_PUN(pv_->tmu0[2], w_);                                \
        ts_ = V(i).s * g_brDlTexScaleS;                             \
        (u_) = ts_ * pv_->oow;                                      \
        BR_DL_PUN(pv_->tmu1[0], (u_));                              \
        BR_DL_PUN(pv_->tmu0[0], (u_));                              \
        ts_ = V(i).t * g_brDlTexScaleT;                             \
        (u_) = ts_ * pv_->oow;                                      \
        BR_DL_PUN(pv_->tmu1[1], (u_));                              \
        BR_DL_PUN(pv_->tmu0[1], (u_));                              \
    } while (0)

#define BR_DLCMD_TRI_I_TEX(ia, ib, ic, u_)                              \
    do {                                                                \
        if ((V(ia).outcode & (V(ib).outcode & V(ic).outcode)) == 0) {    \
            if ((V(ib).outcode | V(ic).outcode | V(ia).outcode) != 0) {  \
                BrDlClipTri(&V(ia), &V(ib), &V(ic));                     \
            } else {                                                     \
                BR_DLCMD_FINISH_VTX_I_N(ia, u_);                         \
                BR_DLCMD_FINISH_VTX_I_N2(ib, u_);                        \
                BrDlVtxFinishTex(&V(ic), (const BrDlClipSt *)(const void *)&V(ic).f40); \
                BrDlDrawTri(&V(ia), &V(ib), &V(ic));                     \
            }                                                            \
        }                                                                \
    } while (0)

/* WHAT IT DOES: draws two triangles from one command -- the packing the game
 * uses for most of its geometry, since flat surfaces come in pairs.  Each is
 * dropped if all three corners are off the same screen edge, handed to the
 * trimmer if any corner is off screen, and otherwise finished and sent to the
 * card; the second triangle's last corner is finished through the clip-node
 * path.  Whatever happens to the first, the second is still considered.
 * Returns the pointer to the next 8-byte command. */
/* T2 2026-09-09: 701/696 B, 200/199 insns, register-blind 2+1 (was 624 B /
 * 527 diffs in the pointer form).  Structure is now the original's: the
 * `push ecx` frame slot, the outcode spill/reload through it, both clip
 * arms, and the second triangle's FinishTex corner.  Residue: (1) the
 * prologue's byte-read schedule -- the original zeroes ecx, pushes ebp,
 * reads p[1] into cl, pushes esi/edi, copies it to edi, THEN reads p[2]
 * and p[0]; ours reads all three before the pushes (one extra `mov R,R`);
 * (2) the same pointer-form 1/w load (`[edi+0x20]`) as the no-Z twin
 * 0x10020D70, see its note in br_dlcmd.c.
 * Levers that landed: `float u` FIRST keeps the frame slot (u after any
 * index: FIRSTDIV 0, 2+7); declaration order ib, ia, ic gives the
 * original's 1, 2, 0 reads (ia, ib, ic reads 2, 1, 0); the named `s*scale`
 * product is needed on ALL six corners in this TU (in br_dlcmd.c's TU the
 * first triangle canonicalised the other way).
 * Dead probes (fn.py): u second / third / last. */
/* @t4-pass 0x1001FA30 1 2026-09-09 probes 10 bytes 698 insns 200 regions 5 rows 1 census no  (decl merges/splits, *(p+n) forms, statement spacing, cast/+0 spellings, operand commute, guard commute: 10 byte-identical) */
/* @t4-pass 0x1001FA30 2 2026-09-09 probes 10 bytes 698 insns 200 regions 5 rows 1 census yes  (register hints, macro paren/array forms, struct decl split, comment width, p[4+n], w_/ts_ decl order: 10 byte-identical; full-length mnemonic histograms equal except mov 68/69 -- the one prologue copy) */
/* @t3 0x1001FA30 2026-09-09 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 698/696 insns 200/199 rows 0+1 regions 5 oracle UNCLASSIFIED
 * @t3-effort passes 2 zero-movement 1 2
 * Residue: one extra prologue register copy (mov R,R singleton) from the
 * byte-read schedule -- the original reads p[1] between the pushes, ours
 * reads all three above them; every read-order, decl and spelling probe in
 * the two passes is byte-identical.  The ib corner's pointer-form 1/w load
 * (the no-Z twin's residue) LANDED here (q1, -3 B, rows 1+2 -> 0+1) -- the
 * twin's dead list does not transfer between TUs.
 * Do not reopen before the end-grind (CLAUDE.md rule 12). */
/* @implements 0x1001FA30 glide BrDlCmdTri2 */
const uint8_t *BrDlCmdTri2(const uint8_t *p)
{
    float u;                    /* ONE slot, shared by all six corners */
    int   ib = p[1];            /* the original reads 1, 2, 0 */
    int   ia = p[2];
    int   ic = p[0];

    BR_DLCMD_TRI_I(ia, ib, ic, u);

    ib = p[5];                  /* the original reads 5, 6, 4 */
    ia = p[6];
    ic = p[4];
    BR_DLCMD_TRI_I_TEX(ia, ib, ic, u);
    return p + 8;
}

#endif /* BR_MATCHING_BUILD */
