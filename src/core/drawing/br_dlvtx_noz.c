/* br_dlvtx_noz.c -- drawing: unlit no-Z vertex transform handler.
 *
 * ONE function, 0x10023110, with ONE caller: the dispatch table (slot 0x04)
 * when geometry mode has neither ZBUFFER nor LIGHTING.  The dispatch table
 * patches described in br_dl.c install it via BrDlVtxRoutine.
 *
 * IT IS THE noZ HALF OF A PAIR.  0x10021A20 (BrDlVtxPlain) is the same body
 * with the depth buffer ON; the two differ in exactly two things:
 *   - the snap rounds through a stack `int` here and through the global
 *     0x105CE310 there, and
 *   - this one overwrites 1/w with 1/65535 once x and y have been projected,
 *     so the card gets a constant w and textures linearly.
 *
 * THE SNAP IS INLINE ASM, exactly as br_dltrim.c and br_dlproj.c establish:
 * a bare `fistp` with no control-word change is not reachable from VC5 C,
 * because every `(int)float` is a `__ftol` call (docs/VC5-IDIOMS.md).  That
 * is also why this function keeps an EBP frame -- VC5 does not omit the frame
 * pointer in a function containing inline asm.
 *
 * Source pointer (w1) is used directly -- no resolver hook, no bounds check.
 * The clip threshold at 0x10077410 is 0.0f, matching BrDlsClipCodes's test.
 *
 * T2 RESIDUE (592 vs 584 B, 135 divergent lines):
 *   1. x87 product evaluation order -- original evaluates z,y,x per matrix row;
 *      VC5 reorders struct-member accesses to sequential offset (x,y,z).  This
 *      is the TU-state mechanism (see x87-wall-mechanism-2026-09-13.md); the
 *      lever is co-filing with the original-TU predecessor definitions.
 *   2. Register allocation -- original uses a two-pointer scheme (edi=pV base,
 *      ecx=&pV->cw at offset 0x58) giving small signed offsets; our recomp uses
 *      one pointer.  Cascades into: register roles (ebx vs edi counter),
 *      outcode test (test esi,esi vs cmp esi,ebx), colour copies (fld/fstp vs
 *      integer mov), local slot assignment (invW/l swapped), and projection
 *      operand order.
 *   3. byte-slot widening: mov ecx,0 vs xor ecx,ecx (3 B).
 *
 * Walls #1 and #2 are both compiler-decision classes with no known source lever.
 * Co-filing (for #1) and TU-membership audit (for #2) are the next steps.
 */
#include <stdint.h>
#include "br_dl.h"

#ifdef BR_MATCHING_BUILD

/* Combined model-view-projection matrix, 4x4 row-major at 0x105D1760. */
extern float DAT_105d1760, DAT_105d1764, DAT_105d1768, DAT_105d176c;
extern float DAT_105d1770, DAT_105d1774, DAT_105d1778, DAT_105d177c;
extern float DAT_105d1780, DAT_105d1784, DAT_105d1788, DAT_105d178c;
extern float DAT_105d1790, DAT_105d1794, DAT_105d1798, DAT_105d179c;

extern float DAT_10077404;   /* 1.0f  */
extern float DAT_10077408;   /* 4.0f  */
extern float DAT_1007740c;   /* 0.25f */
extern float DAT_10077410;   /* 0.0f  -- clip threshold */

extern float DAT_105ccd48;   /* viewport scale  X */
extern float DAT_105cd9f8;   /* viewport translate X */
extern float DAT_105ccfdc;   /* viewport scale  Y */
extern float DAT_105cd9fc;   /* viewport translate Y */

extern BrDlVtx DAT_105ce318[];   /* vertex array, stride 0x68 */

typedef struct { float x, y, z, s, t, n0, n1, n2; } BrDlSrcVtx;

#define BR_VTX_SNAP(fld_, dst_, pre_, back_)                            \
    do {                                                                \
        (pre_) = (fld_) * 4.0f;                                         \
        __asm { fld pre_ }                                              \
        __asm { fistp dst_ }                                            \
        (back_) = (float)(dst_);                                        \
        (fld_) = (back_) * 0.25f;                                       \
    } while (0)

/* WHAT IT DOES: loads a batch of model vertices, moves each one from model
 * space into clip space through the combined matrix, works out which edges
 * of the view it falls outside, and for the ones that are fully inside does
 * the perspective divide, viewport transform, and quarter-pixel snap.  The
 * source normals are copied straight into the colour fields because there is
 * no lighting, and 1/w is forced to a constant so the card skips perspective-
 * correct texturing. */
/* @implements 0x10023110 glide BrDlVtxNoZ */
const uint32_t *BrDlVtxNoZ(const uint32_t *p)
{
    float   invW;
    int     l;
    uint32_t w0 = p[0];
    const BrDlSrcVtx *pSrc = (const BrDlSrcVtx *)p[1];
    int v0 = (w0 >> 16) & 0xFF;
    int n  = (w0 >> 10) & 0x3F;
    BrDlVtx *pV = &DAT_105ce318[v0];

    if (n > 0) {
        int i;

        for (i = n; i != 0; i--) {
            int oc = 0;

            pV->cx = DAT_105d1780 * pSrc->z + DAT_105d1770 * pSrc->y + pSrc->x * DAT_105d1760 + DAT_105d1790;
            pV->cy = DAT_105d1784 * pSrc->z + DAT_105d1774 * pSrc->y + pSrc->x * DAT_105d1764 + DAT_105d1794;
            pV->cz = DAT_105d1788 * pSrc->z + DAT_105d1778 * pSrc->y + pSrc->x * DAT_105d1768 + DAT_105d1798;
            pV->cw = DAT_105d178c * pSrc->z + DAT_105d177c * pSrc->y + pSrc->x * DAT_105d176c + DAT_105d179c;

            *(uint32_t *)&pV->s  = *(const uint32_t *)&pSrc->s;
            *(uint32_t *)&pV->t  = *(const uint32_t *)&pSrc->t;
            *(uint32_t *)&pV->n0 = *(const uint32_t *)&pSrc->n0;
            *(uint32_t *)&pV->n1 = *(const uint32_t *)&pSrc->n1;
            *(uint32_t *)&pV->n2 = *(const uint32_t *)&pSrc->n2;

            if (!(pV->cw >= DAT_10077410))               oc  = 0x01;
            if (!(pV->cz + pV->cw >= DAT_10077410))      oc |= 0x02;
            if (!(pV->cw - pV->cz >= DAT_10077410))      oc |= 0x04;
            if (!(pV->cx + pV->cw >= DAT_10077410))      oc |= 0x08;
            if (!(pV->cw - pV->cx >= DAT_10077410))      oc |= 0x10;
            if (!(pV->cy + pV->cw >= DAT_10077410))      oc |= 0x20;
            if (!(pV->cw - pV->cy >= DAT_10077410))      oc |= 0x40;

            pV->outcode = oc;

            if (oc == 0) {
                invW = DAT_10077404 / pV->cw;
                l = 0;
                *(uint32_t *)&pV->oow = *(uint32_t *)&invW;
                pV->x = DAT_105ccd48 * invW * pV->cx + DAT_105cd9f8;
                pV->y = DAT_105ccfdc * pV->oow * pV->cy + DAT_105cd9fc;
                pV->oow = 1.0f / 65535.0f;
                pV->r = pV->n0;
                pV->g = pV->n1;
                pV->b = pV->n2;
                BR_VTX_SNAP(pV->x, l, invW, invW);
                BR_VTX_SNAP(pV->y, l, invW, invW);
            }

            pSrc++;
            pV++;
        }
    }
    return p + 2;
}

#endif /* BR_MATCHING_BUILD */
