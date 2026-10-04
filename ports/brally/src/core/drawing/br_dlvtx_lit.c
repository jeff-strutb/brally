/* br_dlvtx_lit.c -- drawing: lit vertex transform handler.
 *
 * ONE function, 0x10021C70 (BrDlVtxLit), the G_VTX handler for lit
 * geometry: the one the game actually runs for every lit vertex batch
 * (0x100221D0, the DECAL row, is its unreachable twin).  Same light cache
 * and transform as its siblings (br_dlvtx_texgen.c, br_dlvtx_litdecal.c).
 *
 * Filed on its own, like its siblings: br_dl.c's TU context carries
 * byte-exact functions whose x87 scheduling moves when a body is added
 * there.  br_dl.c keeps the port's split-out form (br_dl_light_setup and
 * br_dl_light_vertex); this file is the original's single body.
 */
#include "br_coretypes.h"   /* br_globals: its objects */
#include <stdint.h>
#include "br_dl.h"


/* MVP matrix, 4x4 row-major at 0x105D1760. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* 64-bit core: declared once, in br_globals.h or its struct's header */          /* fLightCached */
/* 64-bit core: declared once, in br_globals.h or its struct's header */          /* nLights */
/* 64-bit core: declared once, in br_globals.h or its struct's header */          /* iModel */
/* BrDlMtx: br_coretypes.h */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* model matrices, 1-based: [i-1] = 0x105CCD10 + i*0x40 */

/* The two N64 Light records at 0x105CCC78 (directional) and 0x105CCC88
 * (ambient): colour bytes, their copy, signed direction bytes. */
typedef struct {
    unsigned char col[4];
    unsigned char colc[4];
    signed char   dir[4];
    unsigned char pad[4];
} BrDlLight;
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* 64-bit core: declared once, in br_globals.h or its struct's header */  /* lightScale[3] */
/* 64-bit core: declared once, in br_globals.h or its struct's header */  /* lightDir[3] */
/* 64-bit core: declared once, in br_globals.h or its struct's header */  /* lightAmb[3] */
/* 64-bit core: declared once, in br_globals.h or its struct's header */  /* unlit colour */

/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* 64-bit core: declared once, in br_globals.h or its struct's header */        /* 128.0f */

typedef struct { float x, y, z, s, t, n0, n1, n2; } BrDlSrcVtxL;

/* FUN_100344D0: prototype in br_funcs.h */
/* FUN_10022120: prototype in br_funcs.h */
/* FUN_10022070: prototype in br_funcs.h */


/* Transcription notes (2026-09-24, by hand from the asm; compiled /O2 /Op
 * like the rest of its original TU): 59 of 289 instructions still differ
 * (difflib count).  From the sibling 0x10022600: the 1-based matrix stack
 * ([top - 1] gives the `lea`), the mixed absolute/extern MVP columns, the
 * vertex pointer formed before the count.  The helper's output-vertex
 * argument is a separately advanced pointer (the original's ebp), set from
 * pV BEFORE the count is extracted.  `(double)pSrc->x` (0x100221D0's
 * spelling) makes VC5 add the transform products in the original's order.
 * The light rows are written in the original's association,
 * ((m1*dy + m0*dx) + m2*dz): a byte-score search once picked
 * (m1*dy + m2*dz) + m0*dx, which rounds differently -- the whole-image run
 * caught it as a 1-ulp colour difference (2026-09-24).
 * Arithmetic check: every float the function stores, with its rounding
 * points, is the same expression tree as the original's (symbolic x87
 * evaluation of both binaries).
 * Open (x87 scheduling and register choice only): the light setup's
 * byte-load order and fxch/store shuffle, one fxch per transform row.
 * @t4-pass 0x10021C70 1 2026-09-24 probes 2800 bytes 1025 insns 292 regions 7 rows 11 census no  (generator climb over declaration order, light statement order and row term order, matrix form, x-term cast, dot grouping, compare and clamp forms, loop pointers; nothing moved from 49)
 * @t4-pass 0x10021C70 2 2026-09-24 probes 38 bytes 1025 insns 292 regions 7 rows 11 census yes  (census: the rows are the transform fxch and the light block's load/store order -- probed the transform rows' 8 groupings x 4 operand casts and the six loop-pointer initialisations; nothing moved)
 * @t4-pass 0x10021C70 3 2026-09-24 probes 132 bytes 1025 insns 292 regions 8 rows 13 census yes  (after the arithmetic fix; census: the rows are the light block's load order and x87 register choice, which declaration order decides -- moved each local to every other slot; nothing moved)
 * @t4-pass 0x10021C70 4 2026-09-24 probes 720 bytes 1025 insns 292 regions 8 rows 13 census no  (all 720 orders of the light setup's direction and colour statements; nothing moved) */
/* @t3 0x10021C70 2026-09-24 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 1025/1019 insns 292/289 rows 5+8 regions 8 oracle EQUIVALENT
 * @t3-effort passes 4 zero-movement 3 4
 * Residue is x87 scheduling and register choice only (see the Open list
 * above); the arithmetic is the original's, rounding points included.  Do not reopen before the end-grind.
 *
 * Oracle coverage: the 25 scripts run 283 of the 289 instructions (the
 * light refresh 287,585 times), all but the no-lights colour copy and the
 * NULL-matrix path.  t3live, object mode, 8 scripts: 72/72 calls agree;
 * refresh forced with an asymmetric light 72/72; no lights forced 27/27.
 * Planted bugs (a y/z swap in the light rows, a swapped no-lights colour)
 * were caught DIVERGENT.  The NULL-matrix path faults in both. */
/* WHAT IT DOES: loads a batch of lit vertices.  It first refreshes the
 * cached light if anything has invalidated it (the light colour, its
 * direction pulled into model space and normalised, the ambient), then for
 * each vertex: transforms the position by the combined matrix, copies the
 * texture coordinates, shades it (ambient plus the light's colour scaled by
 * how squarely the normal faces the light, capped at 255; back-facing gets
 * ambient only; no lights gets the flat primitive colour), computes the clip
 * codes, and projects it to the screen if it is inside the view. */
/* @implements 0x10021C70 glide BrDlVtxLit */
const uint8_t *BrDlVtxLit(const uint8_t *p)
{
    int32_t oc;
    float dx, dy, dz;
    uint32_t w0;
    const BrDlSrcVtxL *pSrc;
    int i;
    int n;
    BrDlVtx *pV;
    float t;
    int v0;
    float v;
    float *m;
    BrDlVtx *pVc;

    if (!DAT_105d17d0) {
        if (DAT_105ccfd0 != 0) {
            m = DAT_100a9a50 ? DAT_105ccd50[DAT_100a9a50 - 1].m : NULL;
            DAT_105ce210 = (float)(*(BrDlLight (*)[2])&DAT_105ccc78)[0].col[0];
            DAT_105ce214 = (float)(*(BrDlLight (*)[2])&DAT_105ccc78)[0].col[1];
            dz = (float)(*(BrDlLight (*)[2])&DAT_105ccc78)[0].dir[2];
            dx = (float)(*(BrDlLight (*)[2])&DAT_105ccc78)[0].dir[0];
            dy = (float)(*(BrDlLight (*)[2])&DAT_105ccc78)[0].dir[1];
            DAT_105ce218 = (float)(*(BrDlLight (*)[2])&DAT_105ccc78)[0].col[2];
            DAT_105ce21c = ((m[1] * dy + m[0] * dx) + m[2] * dz) / DAT_10077420;
            DAT_105ce220 = ((m[4] * dx + m[5] * dy) + m[6] * dz) / DAT_10077420;
            DAT_105ce224 = ((m[8] * dx + m[9] * dy) + m[10] * dz) / DAT_10077420;
            br_dl_normalise(&DAT_105ce21c);
            DAT_105ce228 = (float)(*(BrDlLight (*)[2])&DAT_105ccc78)[1].col[0];
            DAT_105ce22c = (float)(*(BrDlLight (*)[2])&DAT_105ccc78)[1].col[1];
            DAT_105ce230 = (float)(*(BrDlLight (*)[2])&DAT_105ccc78)[1].col[2];
        }
        DAT_105d17d0 = 1;
    }

    w0 = *(const uint32_t *)p;
    pSrc = BR_AT32(const BrDlSrcVtxL *, p + 4);
    v0 = (w0 >> 16) & 0xFF;
    pV = &g_aBrDlVtxPool[v0];
    pVc = pV;
    n  = (w0 >> 10) & 0x3F;

    for (i = 0; i < n; i++) {
        pV[i].cx = DAT_105d1780 * pSrc->z + DAT_105d1770 * pSrc->y + (double)pSrc->x * DAT_105d1760 + DAT_105d1790;
        pV[i].cy = DAT_105d1784 * pSrc->z + DAT_105d1774 * pSrc->y + (double)pSrc->x * DAT_105d1764 + DAT_105d1794;
        pV[i].cz = DAT_105d1788 * pSrc->z + DAT_105d1778 * pSrc->y + (double)pSrc->x * DAT_105d1768 + DAT_105d1798;
        pV[i].cw = DAT_105d178c * pSrc->z + DAT_105d177c * pSrc->y + (double)pSrc->x * DAT_105d176c + DAT_105d179c;
        pV[i].s = pSrc->s;
        pV[i].t = pSrc->t;

        if (DAT_105ccfd0 != 0) {
            t = (pSrc->n1 * DAT_105ce220 + pSrc->n2 * DAT_105ce224) + pSrc->n0 * DAT_105ce21c;
            if (!(t < 0.0f)) {
                v = t * DAT_105ce210 + DAT_105ce228;
                pV[i].n0 = v > 255.0f ? 255.0f : v;
                v = t * DAT_105ce214 + DAT_105ce22c;
                pV[i].n1 = v > 255.0f ? 255.0f : v;
                v = t * DAT_105ce218 + DAT_105ce230;
                pV[i].n2 = v > 255.0f ? 255.0f : v;
            } else {
                pV[i].n0 = DAT_105ce228;
                pV[i].n1 = DAT_105ce22c;
                pV[i].n2 = DAT_105ce230;
            }
        } else {
            pV[i].n0 = BrGbiRectG_5D17A4;
            pV[i].n1 = BrGbiRectG_5D17B4;
            pV[i].n2 = BrGbiRectG_5CE2D0;
        }

        oc = BrDlsClipCodes(&pV[i].f40);
        pV[i].outcode = oc;
        if (oc == 0)
            br_dl_project(pVc, &pV[i].f40, pV[i].n0, pV[i].n1, pV[i].n2);
        pSrc++;
        pVc++;
    }
    return p + 8;
}

