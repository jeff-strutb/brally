/* br_dlvtx_cmd.c -- drawing: the unlit, depth-buffered G_VTX handler.
 *
 * 0x10021A20, the link-time occupant of dispatch slot 0x04 (0x1001FD70 puts
 * a lit variant there once a geometry mode arrives).  It is the Z twin of
 * 0x10023110 BrDlVtxNoZ (br_dlvtx_noz.c) and differs from it in exactly two
 * things: the snap rounds through the global int 0x105CE310 instead of a
 * stack int, and 1/w is left as computed.  The port body is BrDlCmdVtx in
 * br_dlcmd.c; this file is the matching arm only.
 */
/* TU state only (symbol-table size): with the DirectSound declarations in
 * front, VC5 orders the matrix row terms y, z, x as the original does.
 * Nothing here uses them. */
#include <dsound.h>
#include "br_x87.h"
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

/* The quarter-pixel snap: round to nearest through the x87, via the global
 * scratch int. */
#define SNAPG(f_, t) do { t = (f_) * BrGbiRectK_FIXED; g_iBrGbiSnap = br_fistp(t); t = (float)g_iBrGbiSnap; (f_) = t * DAT_1007740c; } while (0)
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* WHAT IT DOES: loads a batch of model corner points into the renderer's
 * working set of vertices: each goes through the combined matrix into clip
 * space, gets the outcode of the view edges it lies outside, and when it is
 * inside all of them gets the perspective divide, the viewport transform
 * and a quarter-pixel snap, with the source normals copied into the colour
 * fields (no lighting on this path).  Returns the next command. */
/* Byte-exact (2026-09-27), transcribed from BrDlVtxNoZ's matching arm; see
 * br_dlvtx_noz.c for the source facts (asm snap, float colour copies, the
 * pFirst[i] / stepped-pV pair of pointers, separate float scratches). */
/* @implements 0x10021A20 glide BrDlCmdVtx */
const uint32_t *BrDlCmdVtx(const uint32_t *p)
{
    uint32_t w0;
    float invW, tx, ty;
    const BrDlSrcVtx *pSrc;
    BrDlVtx *pV;
    int n;
    int oc;
    int i;
    int v0;
    BrDlVtx *pFirst;
    float c;
    /* Declared here, after the locals, so the scale takes the fld side
     * of scale * invW (the later symbol does). */
    /* 64-bit core: declared once, in br_globals.h or its struct's header */

    w0 = p[0];
    pSrc = BR_PTR32(const BrDlSrcVtx *, p[1]);
    v0 = (w0 >> 16) & 0xFF;
    pV = &g_aBrDlVtxPool[v0];
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
                invW = DAT_10077404 / pFirst[i].cw;
                *(uint32_t *)&pFirst[i].oow = *(uint32_t *)&invW;
                pV->x = DAT_105ccd48 * invW * pFirst[i].cx + DAT_105cd9f8;
                pFirst[i].y = DAT_105ccfdc * pFirst[i].oow * pFirst[i].cy + DAT_105cd9fc;
                c = pFirst[i].n0; pFirst[i].r = c;
                c = pFirst[i].n1; pFirst[i].g = c;
                c = pFirst[i].n2; pFirst[i].b = c;
                SNAPG(pV->x, tx);
                SNAPG(pFirst[i].y, ty);
            }
            pSrc++;
            pV++;
    }
    return p + 2;
}

