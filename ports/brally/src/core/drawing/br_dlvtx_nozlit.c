/* br_dlvtx_nozlit.c -- drawing: lit no-Z vertex transform handler.
 *
 * ONE function, 0x10023360, the vertex handler br_dl.c installs when the
 * geometry mode has LIGHTING but not ZBUFFER.  The lit sibling of 0x10023110
 * (br_dlvtx_noz.c): same light cache as the texgen pair (br_dlvtx_texgen.c,
 * br_dlvtx_gen.c), same transform rows, then a clamped diffuse term per
 * vertex and the no-Z projector 0x10023760 (br_dlproj.c).
 *
 * Filed on its own, like the rest of the family: br_dl.c's TU context
 * carries byte-exact functions whose x87 scheduling moves when a body is
 * added there.  Matching arm only; the port runs br_dl.c's portable path.
 *
 * WHAT IT IS FOR: lighting a model drawn with the depth buffer OFF -- an
 * N64 idiom for lit geometry layered over the scene without depth tests.
 * The handler selector 0x1001FD70 installs it (0x1001FE9E, its only install
 * site) whenever the geometry mode has LIGHTING (0x20000) set and ZBUFFER
 * (0x1) clear.  The PC game DOES enter that state (below), but never loads a
 * vertex in it.
 *
 * RE-DERIVED 2026-09-28 -- the 2026-09-24 notes below said the state never
 * occurs; it does:
 *  - In snow, and in the END camera view, the last world pass leaves LIGHTING
 *    set; the HUD dial BrHudDrawDial 0x100140B0 then clears ZBUFFER alone
 *    (single view).  Measured: 487..712 selections per race.
 *  - With the needle dial (sprite mode 0) the dial clears LIGHTING
 *    (B6 0x00033000) before its own G_VTX quad, so the quad goes to the unlit
 *    no-Z handler.  Cars 9, 10, 25 and 26 have a non-needle dial: the dial
 *    returns right after clearing Z, and the state stays until the next
 *    frame (~5,460 selections per race).
 *  - Everything drawn after the dial in that frame (split times, view message,
 *    text -- a 24-function call tree, three levels deep) emits no G_VTX and no
 *    display-list call; the next frame's setup resets the mode.
 *  - Measured over 154 sessions (all tracks, weathers, 32 cars, camera views,
 *    race menu, attract demo): 0 calls.  Split screen (cViews 2) skips the
 *    dial's Z clear, and is reachable only through a command-line switch no
 *    retail launcher passes.
 * The 2026-09-24 notes follow; their first measured claim is superseded.
 *
 * DEAD CODE IN THE PC RELEASE -- settled 2026-09-24, do not re-litigate:
 *  - The geometry mode 0x105D17C8 has exactly two writers, the display-list
 *    handlers G_CLEARGEOMETRYMODE 0x1001FD40 and G_SETGEOMETRYMODE
 *    0x100211E0; BRally.exe and BossRally.exe never reference it.  Its
 *    initial value is 0.  So the state comes only from 0xB6/0xB7 commands.
 *  - No asset emits those: a census of all 1577 files under the CD (363 of
 *    them with >20 aligned combiner commands, i.e. readable display lists)
 *    finds no geometry-mode command with valid bits.  Every one is built in
 *    BRGlide.dll code: 59 emit sites, each traced to its operand.
 *  - Lighting is set without depth in ONE place, the object builder
 *    0x1000CBA0 (0x1000CEDE / 0x1000CEF6: 0x30004 / 0x20004), called only
 *    from the scene builder 0x1000EAF0.  Every scene-build pass first emits
 *    SET (cull | fog-if-0x106ED6A8 | 0xA0005) at 0x1000F083 / 0x1000F289 -- depth,
 *    lighting and shade -- with no return before it (br_scenedl.c).
 *  - Depth is cleared on its own only by the text drawer 0x100168C0
 *    (0x100168D7), the HUD dial 0x100140B0 (0x10014210, restored at
 *    0x10014729) and frame setup 0x10015630 (0x10015910, CLR 0xF0205, then
 *    SET 0x20205 at 0x10015ADE).  None is reachable from the scene builder:
 *    text runs after each view's world pass (HUD, FPS readout, captions,
 *    frame present); the object builder's one log call (0x1000D9DF, "BAD
 *    VTX DL", a fatal-data path) draws into its own stack display list at
 *    0x10008EF0 and never reaches the frame's.  Frame setup's early returns
 *    (rain/storm/no-sky plain clear; the never-written 0x106ED698) skip its
 *    reset, but the scene builder's own SET re-enables depth regardless.
 *  - Measured: with a code hook on both geometry-mode writers, a rainy
 *    quick race performs 18,528 writes and none leaves LIGHTING set with
 *    ZBUFFER clear; sunny, rainy, snowy and night races and all 25 brbox
 *    scripts call this function 0 times (0x10023110, its unlit twin, 337+).
 *  - BRD3D.dll: same executables, same data, same geometry-mode sources.
 *    The N64 original was not checked for this mode.
 *  Consequence: the live oracle (A5) can never reach it, so T3 is closed to
 *  this function; only byte-exact T4 finishes it.
 * STATUS: EXCLUDED (T2)
 */
#include "br_coretypes.h"   /* br_globals: its objects */
#include "br_dl.h"   /* br_globals: its objects */
#include <stdint.h>
#include <stddef.h>
/* TU state.  Traced in the VC5 backend (tools/brally/c2emu.py, docs/brally/VC5-IDIOMS.md
 * tail): operand order inside each light row is c2's key sort, an XOR hash
 * over symbol indices, so these headers (the symbol count ahead of the
 * function), the order of the file-scope and block-scope externs and the
 * locals below place the products.  The light rows carry no redundant
 * inner parentheses: a parenthesised sub-sum becomes a c2 precision node,
 * one more DAG level, which changes the setup's store/load schedule. */
#include <stdio.h>
#include <dsound.h>
#include <limits.h>
#include <windows.h>
/* BrDlVtx: br_dl.h */

/* MVP matrix, 4x4 row-major at 0x105D1760. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */  /* MVP y column */
/* 64-bit core: declared once, in br_globals.h or its struct's header */  /* MVP z column */

/* BrDlMtx: br_coretypes.h */

typedef struct {
    unsigned char col[4];
    unsigned char colc[4];
    signed char   dir[4];
    unsigned char pad[4];
} BrDlLight;

typedef struct { float x, y, z, s, t, n0, n1, n2; } BrDlSrcVtx;

/* 64-bit core: declared once, in br_globals.h or its struct's header */  /* model matrix stack, 1-based: [top - 1] */
/* 64-bit core: declared once, in br_globals.h or its struct's header */  /* 255.0f */
/* 64-bit core: declared once, in br_globals.h or its struct's header */  /* MVP x column */
/* 64-bit core: declared once, in br_globals.h or its struct's header */  /* lightDir[3] */
/* FUN_100344D0: prototype in br_funcs.h */
/* FUN_10022120: prototype in br_funcs.h */
/* 64-bit core: declared once, in br_globals.h or its struct's header */  /* vertex buffer */
/* 64-bit core: declared once, in br_globals.h or its struct's header */  /* 128.0f */
/* 64-bit core: declared once, in br_globals.h or its struct's header */  /* lightScale[3] */
/* 64-bit core: declared once, in br_globals.h or its struct's header */  /* MVP translation row */

/* Hand-transcribed from the Glide bytes, compiled /O2 /Op like the rest of
 * its original TU (pinned in config/brally/t3_variant_c.csv).  1019/1019 B,
 * 289/289 insns; 9 real diff regions, 11 rows (plus two link-time
 * relocation fields).  Source facts carried from the family
 * (br_dlvtx_texgen.c): light records read as bytes, 1-based matrix stack,
 * pSrc++ at the tail, and the vertex pointer copy formed before the count.
 * 2026-09-29: the four transform rows are exact (all four MVP columns as
 *    externs) and the setup's colour stores and direction loads are exact.
 * Open (x87 operand order / scheduling only):
 *  - the light rows: the original sums (m[2]*dz + m[0]*dx) + m[1]*dy with
 *    dz, dx, dy homed at [esp+18], [esp+14], [esp+1c]; ours pairs dx with
 *    dy first.  Written with the inner parentheses the pairing is right
 *    but the schedule needs the direction load one DAG level higher; in
 *    the emulated backend the load order of each pair is an equal-priority
 *    tie broken by tuple order.  Not reached: 2,048 symbol counts x 1,200
 *    local layouts, statement orders, comparator flips (tools/brally/c2emu.py);
 *  - the ambient block after the normalise call: integer issue order of
 *    `xor edx,edx` / `add esp,4` / `mov bl,[..]`.
 * @t4-pass 0x10023360 1 2026-09-24 probes 74 bytes 1017 insns 288 regions 7 rows 7 census no  (hand, fn.py variants: transform-row term orders, groupings, operand orders, translation position, double casts, x-column kinds, float[4][4] matrix; nothing adopted moved the gate numbers)
 * @t4-pass 0x10023360 2 2026-09-24 probes 78 bytes 1017 insns 288 regions 7 rows 7 census yes  (census: every row is an x87 issue-order pair plus three singleton fxch; light-setup and ambient statement hill-climb, dot-product orders, direction int temps; nothing moved)
 * @t4-pass 0x10023360 3 2026-09-29 probes 9000 bytes 1019 insns 289 regions 9 rows 11 census yes  (c2emu-traced: inner-paren precision nodes, statement order, extern scope/order, local order, header symbol counts, column extern/macro forms; comparator flips, priority and tie-break forcing in the emulated backend) */
/* WHAT IT DOES: transforms a batch of vertices through the combined matrix,
 * copies their texture coordinates, lights each one -- the diffuse term from
 * the one directional light, clamped to 255 per channel, or the ambient
 * colour alone when the vertex faces away, or a fixed colour when lighting
 * is off -- and projects every vertex that lies inside the view. */
/* @implements 0x10023360 glide BrDlVtxNoZLit */
const uint8_t *BrDlVtxNoZLit(const uint8_t *p)
{
    int32_t oc;
    uint32_t w0;
    BrDlVtx *pV, *pVc;
    float *pf;
    int v0;
    int n;
    const BrDlSrcVtx *pSrc;
    float *m;
    float t, c;
    float dx, dy, dz;
    int i;
    /* 64-bit core: declared once, in br_globals.h or its struct's header */
    /* 64-bit core: declared once, in br_globals.h or its struct's header */
    /* 64-bit core: declared once, in br_globals.h or its struct's header */
/* BrDlProjectNoZ: prototype in br_funcs.h */
    /* 64-bit core: declared once, in br_globals.h or its struct's header */
    /* 64-bit core: declared once, in br_globals.h or its struct's header */
    /* 64-bit core: declared once, in br_globals.h or its struct's header */
    /* 64-bit core: declared once, in br_globals.h or its struct's header */

    if (!DAT_105d17d0) {
        if (DAT_105ccfd0 != 0) {
            m = DAT_100a9a50 ? DAT_105ccd50[DAT_100a9a50 - 1].m : NULL;
            DAT_105ce210 = (float)((BrDlLight *)DAT_105ccc78)[0].col[0];
            DAT_105ce214 = (float)((BrDlLight *)DAT_105ccc78)[0].col[1];
            dz = (float)((BrDlLight *)DAT_105ccc78)[0].dir[2];
            dx = (float)((BrDlLight *)DAT_105ccc78)[0].dir[0];
            DAT_105ce218 = (float)((BrDlLight *)DAT_105ccc78)[0].col[2];
            dy = (float)((BrDlLight *)DAT_105ccc78)[0].dir[1];
            DAT_105ce21c = (m[2] * dz + m[0] * dx + m[1] * dy) / DAT_10077420;
            DAT_105ce220 = (m[6] * dz + m[4] * dx + m[5] * dy) / DAT_10077420;
            DAT_105ce224 = (m[10] * dz + m[8] * dx + m[9] * dy) / DAT_10077420;
            br_dl_normalise((struct BrVec3 *)&DAT_105ce21c);
            DAT_105ce228 = (float)((BrDlLight *)DAT_105ccc78)[1].col[0];
            DAT_105ce22c = (float)((BrDlLight *)DAT_105ccc78)[1].col[1];
            DAT_105ce230 = (float)((BrDlLight *)DAT_105ccc78)[1].col[2];
        }
        DAT_105d17d0 = 1;
    }

    w0 = *(const uint32_t *)p;
    pSrc = BR_AT32(const BrDlSrcVtx *, p + 4);
    v0 = (w0 >> 16) & 0xFF;
    pV = &g_aBrDlVtxPool[v0];
    pVc = pV;
    n  = (w0 >> 10) & 0x3F;

    for (i = 0; i < n; i++) {
        pV[i].cx = DAT_105d1780 * pSrc->z + DAT_105d1770 * pSrc->y + pSrc->x * DAT_105d1760 + DAT_105d1790;
        pV[i].cy = DAT_105d1784 * pSrc->z + DAT_105d1774 * pSrc->y + pSrc->x * DAT_105d1764 + DAT_105d1794;
        pV[i].cz = DAT_105d1788 * pSrc->z + DAT_105d1778 * pSrc->y + pSrc->x * DAT_105d1768 + DAT_105d1798;
        pV[i].cw = DAT_105d178c * pSrc->z + DAT_105d177c * pSrc->y + pSrc->x * DAT_105d176c + DAT_105d179c;
        pV[i].s = pSrc->s;
        pV[i].t = pSrc->t;

        if (DAT_105ccfd0 != 0) {
            t = (pSrc->n1 * DAT_105ce220 + pSrc->n2 * DAT_105ce224) + pSrc->n0 * DAT_105ce21c;
            if (t >= DAT_10077410) {
                c = t * DAT_105ce210 + DAT_105ce228;
                pV[i].n0 = (c > DAT_10077418) ? 255.0f : c;
                c = t * DAT_105ce214 + DAT_105ce22c;
                pV[i].n1 = (c > DAT_10077418) ? 255.0f : c;
                c = t * DAT_105ce218 + DAT_105ce230;
                pV[i].n2 = (c > DAT_10077418) ? 255.0f : c;
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

        pf = &pV[i].f40;
        oc = BrDlsClipCodes(pf);
        pV[i].outcode = oc;
        if (oc == 0)
            BrDlProjectNoZ(pVc, pf, pV[i].n0, pV[i].n1, pV[i].n2);
        pSrc++;
        pVc++;
    }
    return p + 8;
}

