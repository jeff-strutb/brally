/* br_dlvtx_litdecal.c -- drawing: lit vertex transform handler, DECAL row.
 *
 * ONE function, 0x100221D0 (BrDlVtxLitDecal), the G_VTX handler 0x1001E8FB
 * installs in place of the lit handler 0x10021C70 when the DECAL combiner row
 * is selected.  Same light cache and transform as its siblings
 * (br_dlvtx_texgen.c), with the per-vertex lighting open-coded.
 *
 * Filed on its own, like its siblings: br_dl.c's TU context carries
 * byte-exact functions whose x87 scheduling moves when a body is added there.
 *
 * NEVER RUN IN THE PC RELEASE -- re-derived 2026-09-28 (the 2026-09-24
 * proof's premise was wrong; its conclusion holds):
 *  - The DECAL combine IS sent, every frame the sky is drawn: BrSceneSetupFrame
 *    0x10015630 builds it through BrRdpSetCombineLERP (0x1001CF90), which packs
 *    the words from 16 small fields at run time -- why a search for the
 *    constants 0xFC317E02 / 0x5FFEF3FA found nothing.  w1 is 0x5FFEF3FA, or
 *    0x51FEF3FA in fog.  Measured: ~7,200 per race on every track and weather,
 *    39,810 in the championship script.  The old "no PC code emits it, all
 *    scripts UNCOVERED" was false.
 *  - The handler is installed only when the decal flag 0x105CDA04 is set AND
 *    the geometry mode has Z + LIGHTING without TEXGEN (0x1001FE1C, and the
 *    swap at 0x1001E909 when the lit handler 0x10021C70 is current).  The flag
 *    is cleared by every combine.  The sky pass sends the decal combine, clears
 *    Z and lighting (B6 0x000F0205), draws the track's sky display list, then
 *    re-sets Z + lighting (B7 0x00020205) -- so the flag would reach lit
 *    geometry only if a sky display list carried no combine of its own.
 *  - Measured over 154 sessions (all 14 selectable tracks x 5 weathers, 32
 *    cars, camera views, the attract demo, credits, championship,
 *    multiplayer): the next combine always closes the window before any
 *    Z+lit vertex load; 0 calls.  gamewin.trk (the only track not raced) is
 *    reachable only in game mode 5, which no front-end path keeps.
 *  - BRD3D.dll handles the same mode differently (0x1001C941).  The N64
 *    original (ROM 0x2F634) emits it in its 2-cycle frame setup.
 *  Consequence: the live oracle (A5) can never reach it, so T3 is closed to
 *  this function; only byte-exact T4 finishes it.
 *  STATUS: EXCLUDED (T4) -- byte-exact 2026-09-28; listed in
 *  config/excluded.csv, outside the completion target.
 */
#include "br_coretypes.h"   /* br_globals: its objects */
#include <stdint.h>
#include "br_dl.h"

/* TU state.  Three things in this preamble decide codegen (traced in the
 * backend with tools/c2emu.py, docs/VC5-IDIOMS.md tail):
 *  - the light rows are written without the redundant inner parentheses
 *    where the original had none: a parenthesised sub-sum becomes a c2
 *    precision node, one more DAG level, which moves the last direction
 *    fild after the lightScale[2] store;
 *  - operand order inside each sum is c2's key sort, a XOR hash over symbol
 *    indices: the symbol count ahead of the function (these headers) and
 *    the order of the file-scope externs, the block-scope externs and the
 *    locals below are what put every product and x-term in the original's
 *    order.  Exactly this count works (+-1 does not). */
#include <windows.h>
#include <limits.h>
#include <stdlib.h>
#include <stddef.h>
#include "br_dlcmd.h"
#include "br_vecd.h"
#include <math.h>
#include <ddraw.h>
#include <dsound.h>
/* y column and the three colour-scale factors are absolute derefs: that
 * operand kind takes the fld side over the vertex field. */
#define DAT_105ccd44 BrGbiRectG_5CCD44
#define DAT_105cd9f4 BrGbiRectG_5CD9F4
#define DAT_105cccf8 BrGbiRectG_5CCCF8
/* BrDlMtx: br_coretypes.h */
typedef struct {
    unsigned char col[4];
    unsigned char colc[4];
    signed char   dir[4];
    unsigned char pad[4];
} BrDlLight;
typedef struct { float x, y, z, s, t, n0, n1, n2; } BrDlSrcVtxL;
/* 64-bit core: declared once, in br_globals.h or its struct's header */  /* MVP x column (4x4 row-major at 0x105D1760) */
/* 64-bit core: declared once, in br_globals.h or its struct's header */  /* N64 Lights: directional, ambient */
/* 64-bit core: declared once, in br_globals.h or its struct's header */  /* MVP translation row */
/* 64-bit core: declared once, in br_globals.h or its struct's header */  /* 128.0f */
/* 64-bit core: declared once, in br_globals.h or its struct's header */  /* lightAmb[3] */
/* 64-bit core: declared once, in br_globals.h or its struct's header */  /* fLightCached */
/* 64-bit core: declared once, in br_globals.h or its struct's header */  /* lightScale[3] */
/* 64-bit core: declared once, in br_globals.h or its struct's header */  /* model matrices, 1-based: [i-1] = 0x105CCD10 + i*0x40 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */  /* iModel */
/* 64-bit core: declared once, in br_globals.h or its struct's header */  /* nLights */
/* 64-bit core: declared once, in br_globals.h or its struct's header */  /* MVP z column */

/* WHAT IT DOES: loads a batch of vertices for a decal-lit surface.  It first
 * refreshes the cached light (colour, direction pulled into model space and
 * normalised, ambient) if anything has invalidated it, then for each vertex:
 * transforms the position by the combined matrix, copies the texture
 * coordinates, shades it (ambient plus the light's share by how squarely the
 * normal faces it, capped at 255; back-facing gets ambient, no lights gets the
 * flat primitive colour), scales the colour, and computes the clip codes,
 * projecting the vertex straight away when it is fully on screen. */
/* @implements 0x100221D0 glide BrDlVtxLitDecal */
const uint8_t *BrDlVtxLitDecal(const uint8_t *p)
{
    float fy, fz, fx;
    int n;
    BrDlVtx *pV;
    int v0;
    uint32_t w0;
    BrDlVtx *pVc;
    const BrDlSrcVtxL *pSrc;
    float *m;
    float t, v;
    int32_t oc;
    int i;
    /* 64-bit core: declared once, in br_globals.h or its struct's header */
/* FUN_10022120: prototype in br_funcs.h */
    /* 64-bit core: declared once, in br_globals.h or its struct's header */
/* FUN_10022070: prototype in br_funcs.h */
    /* 64-bit core: declared once, in br_globals.h or its struct's header */
/* FUN_100344D0: prototype in br_funcs.h */

    if (!DAT_105d17d0) {
        if (DAT_105ccfd0 != 0) {
            if (DAT_100a9a50 != 0)
                m = DAT_105ccd50[DAT_100a9a50 - 1].m;
            else
                m = NULL;

            DAT_105ce210 = (float)(*(BrDlLight (*)[2])&DAT_105ccc78)[0].col[0];
            DAT_105ce214 = (float)(*(BrDlLight (*)[2])&DAT_105ccc78)[0].col[1];
            DAT_105ce218 = (float)(*(BrDlLight (*)[2])&DAT_105ccc78)[0].col[2];
            fy = (float)(*(BrDlLight (*)[2])&DAT_105ccc78)[0].dir[1];
            fz = (float)(*(BrDlLight (*)[2])&DAT_105ccc78)[0].dir[2];
            fx = (float)(*(BrDlLight (*)[2])&DAT_105ccc78)[0].dir[0];
            /* The direction is converted once into three FLOAT locals (stored to
             * [esp+0x14/0x18/0x1c]); each row's association is read off the x87 trace. */
            DAT_105ce21c = (m[1] * fy + m[2] * fz + m[0] * fx) / DAT_10077420;
            DAT_105ce220 = (m[4] * fx + m[6] * fz + m[5] * fy) / DAT_10077420;
            DAT_105ce224 = ((m[10] * fz + m[9] * fy) + m[8] * fx) / DAT_10077420;

            br_dl_normalise(&DAT_105ce21c);

            DAT_105ce228 = (float)(*(BrDlLight (*)[2])&DAT_105ccc78)[1].col[0];
            DAT_105ce22c = (float)(*(BrDlLight (*)[2])&DAT_105ccc78)[1].col[1];
            DAT_105ce230 = (float)(*(BrDlLight (*)[2])&DAT_105ccc78)[1].col[2];
        }
        DAT_105d17d0 = 1;
    }

    w0 = *(const uint32_t *)p;
    pSrc = *(const BrDlSrcVtxL **)(p + 4);
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
            t = (pSrc->n1 * DAT_105ce220 + pSrc->n0 * DAT_105ce21c) + pSrc->n2 * DAT_105ce224;
            if (t >= 0.0f) {
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
        pV[i].n0 = DAT_105ccd44 * pV[i].n0;
        pV[i].n1 = DAT_105cd9f4 * pV[i].n1;
        pV[i].n2 = DAT_105cccf8 * pV[i].n2;

        oc = BrDlsClipCodes(&pV[i].f40);
        pV[i].outcode = oc;
        if (oc == 0)
            br_dl_project(pVc, &pV[i].f40, pV[i].n0, pV[i].n1, pV[i].n2);
        pSrc++;
        pVc++;
    }
    return p + 8;
}

