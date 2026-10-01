/* br_dlvtx_noz.c -- drawing: unlit no-Z vertex transform handler.
 *
 * ONE function, 0x10023110, with ONE caller: the dispatch table (slot 0x04)
 * when geometry mode has neither ZBUFFER nor LIGHTING.  The dispatch table
 * patches described in br_dl.c install it via BrDlVtxRoutine.
 *
 * IT IS THE noZ HALF OF A PAIR.  0x10021A20 (BrDlCmdVtx) is the same body
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
 * The matrix rows' term order (z,y,x here, y,z,x in the twin) is TU state,
 * not source: every source order compiles the same, and the TU's symbol
 * table size flips it.  The residue note sits above the function.
 */
/* TU state only (symbol-table size): with these declarations in front,
 * VC5 gives the matrix products the operand roles the original has.
 * Nothing here uses them. */
#include <dsound.h>
#include "br_x87.h"
#include <stdio.h>
#include <stdint.h>
#include "br_dl.h"


/* Combined model-view-projection matrix, 4x4 row-major at 0x105D1760. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 1.0f  */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 4.0f  */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0.25f */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0.0f  -- clip threshold */

/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* viewport translate X */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* viewport scale  Y */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* viewport translate Y */

/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* vertex array, stride 0x68 */

typedef struct { float x, y, z, s, t, n0, n1, n2; } BrDlSrcVtx;

/* The quarter-pixel snap: round to nearest through the x87. */
#define SNAP(f_, t) do { t = (f_) * DAT_10077408; l = br_fistp(t); t = (float)l; (f_) = t * DAT_1007740c; } while (0)

/* WHAT IT DOES: loads a batch of model vertices, moves each one from model
 * space into clip space through the combined matrix, works out which edges
 * of the view it falls outside, and for the ones that are fully inside does
 * the perspective divide, viewport transform, and quarter-pixel snap.  The
 * source normals are copied straight into the colour fields because there is
 * no lighting, and 1/w is forced to a constant so the card skips perspective-
 * correct texturing. */
/* Byte-exact (2026-09-27, re-transcribed from the asm; /O2 /Op like its
 * siblings).  Source facts:
 * - The snap is four asm instructions (fld, fistp, fild, fstp) on a float
 *   scratch and one int `l`; C then scales the scratch by 0.25.  1/w, the x
 *   snap and the y snap each have their own float (invW, tx, ty): VC5 packs
 *   them into one slot below l, as the original has it; a single shared
 *   float swaps the two slots.
 * - The colours are copied through a float local, so they go fld/fstp.
 * - The vertex fields are addressed as pFirst[i] (VC5 strength-reduces them
 *   to a pointer biased to &cw, ecx) while x goes through a separate pointer
 *   pV that is stepped each pass (edi); the original has both registers.
 * - The y projection and the 1/65535 store go through pW = &pFirst[i]: as
 *   pointer stores VC5 may reorder them, and it puts the 1/65535 store
 *   ahead of the y store as the original does.  Every other site by index.
 * - The viewport scale is declared after the locals (it takes the fld side
 *   of scale * invW), and <dsound.h> + <stdio.h> set the TU state for the
 *   matrix terms' order. */
/* @implements 0x10023110 glide BrDlVtxNoZ */
const uint32_t *BrDlVtxNoZ(const uint32_t *p)
{
    uint32_t w0;
    float invW, tx, ty;
    int l;
    const BrDlSrcVtx *pSrc;
    BrDlVtx *pV;
    int n;
    int oc;
    int i;
    int v0;
    BrDlVtx *pFirst;
    BrDlVtx *pW;
    float c;
    /* Declared here, after the locals, so the scale takes the fld side
     * of scale * invW (the later symbol does). */
    /* 64-bit core: declared once, in br_globals.h or its struct's header */

    w0 = p[0];
    pSrc = (const BrDlSrcVtx *)p[1];
    v0 = (w0 >> 16) & 0xFF;
    pV = &DAT_105ce318[v0];
    pFirst = pV;
    n = (w0 >> 10) & 0x3F;
    for (i = 0; i < n; i++) {
            oc = 0;
            pFirst[i].cx = DAT_105d1780 * pSrc->z + DAT_105d1770 * pSrc->y + pSrc->x * DAT_105d1760 + DAT_105d1790;
            pFirst[i].cy = DAT_105d1784 * pSrc->z + DAT_105d1774 * pSrc->y + pSrc->x * DAT_105d1764 + DAT_105d1794;
            pFirst[i].cz = DAT_105d1788 * pSrc->z + DAT_105d1778 * pSrc->y + pSrc->x * DAT_105d1768 + DAT_105d1798;
            pFirst[i].cw = DAT_105d178c * pSrc->z + DAT_105d177c * pSrc->y + pSrc->x * DAT_105d176c + DAT_105d179c;
            *(uint32_t *)&pFirst[i].s  = *(const uint32_t *)&pSrc->s;
            *(uint32_t *)&pFirst[i].t  = *(const uint32_t *)&pSrc->t;
            *(uint32_t *)&pFirst[i].n0 = *(const uint32_t *)&pSrc->n0;
            *(uint32_t *)&pFirst[i].n1 = *(const uint32_t *)&pSrc->n1;
            *(uint32_t *)&pFirst[i].n2 = *(const uint32_t *)&pSrc->n2;
            if (pFirst[i].cw < DAT_10077410)               oc  = 0x01;
            if (pFirst[i].cz + pFirst[i].cw < DAT_10077410)      oc |= 0x02;
            if (pFirst[i].cw - pFirst[i].cz < DAT_10077410)      oc |= 0x04;
            if (pFirst[i].cx + pFirst[i].cw < DAT_10077410)      oc |= 0x08;
            if (pFirst[i].cw - pFirst[i].cx < DAT_10077410)      oc |= 0x10;
            if (pFirst[i].cy + pFirst[i].cw < DAT_10077410)      oc |= 0x20;
            if (pFirst[i].cw - pFirst[i].cy < DAT_10077410)      oc |= 0x40;
            pFirst[i].outcode = oc;
            if (oc == 0) {
                pW = &pFirst[i];
                invW = DAT_10077404 / pFirst[i].cw;
                l = 0;
                *(uint32_t *)&pFirst[i].oow = *(uint32_t *)&invW;
                pV->x = DAT_105ccd48 * invW * pFirst[i].cx + DAT_105cd9f8;
                pW->y = DAT_105ccfdc * pW->oow * pW->cy + DAT_105cd9fc;
                pW->oow = 1.0f / 65535.0f;
                c = pFirst[i].n0; pFirst[i].r = c;
                c = pFirst[i].n1; pFirst[i].g = c;
                c = pFirst[i].n2; pFirst[i].b = c;
                SNAP(pV->x, tx);
                SNAP(pFirst[i].y, ty);
            }
            pSrc++;
            pV++;
    }
    return p + 2;
}

