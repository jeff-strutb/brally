/* br_framedrive.c -- drawing: the per-frame SCENE DRIVER, 0x10011FA0.
 *
 * RESPONSIBILITY: drawing/ -- turn geometry and images into pixels.
 *
 * ONE function, 4,500 bytes: the thing the race loop calls once per frame
 * to draw everything.  It takes a slot index (five 0x2E0F0-byte slot
 * records, the LRU cache slice2_14.h describes; the four bases below are
 * all `base + slot*0x2E0F0`) and for every view in that slot builds the
 * camera, the scene display list (two passes through BrSceneDlBuild), the
 * cars (opaque, body, translucent), the environment, then the overlays --
 * FPS, split-screen captions, the pause menu, the attract-mode credits --
 * and finally closes the frame.
 *
 * Everything it calls already has a name in the tree except seven leaves
 * that are still Ghidra-named (FUN_1006ec30, FUN_1002af17, FUN_1002b480,
 * FUN_10014e00, FUN_1002cb49, FUN_1002cee9 and the two generated TUs);
 * the d3d twins of three of them are slice2_18's BrFogUpdate /
 * BrHudColorsUpdate / BrFrameEnd.
 *
 * WHAT THE BYTES PIN (read before re-deriving any of it):
 *  - 0x10008D60 is a bare `ret` debug logger, called K&R-style with 0, 1
 *    or 5 arguments.  Its one-argument call pushes 0x3EA8F5C3 (0.33f) as
 *    FOUR bytes; a float through an unprototyped call would be promoted
 *    to a double and pushed as eight, so the source value is spelled as
 *    the int bits.
 *  - the per-view record is the 0x58-byte BrHudView (x,y,w,h, then the
 *    view's car index at +0x10); `&aViews[i]` is the `lea [ebp+ecx*8]`
 *    with ecx = 11*i.
 *  - `g_brCViews > 1` (cmp 1 / jle), NOT `>= 2`: the global compare at the
 *    end (`cmp [g],2; jl`) shows VC5 does not canonicalise `>= 2`.
 *  - three `x >> 1` / `>> 2` sites are bare `sar` with no cdq fix-up:
 *    they are shifts in the source, the `/ 4`, `/ 16`, `/ 8`, `/ 11`
 *    sites are real signed divisions.
 *  - `g - 0x1E` as an ARGUMENT compiles to `add r,-0x1E`; `y -= 5` as a
 *    statement compiles to `sub r,5`.  The credits title's `y - 0x14`
 *    is the former.
 *  - the 32-byte records at 0x100A9368 are the credits pages: a title,
 *    4.0f, six line pointers; a line starting with '`' is a sub-heading
 *    drawn in the small font with the mark stripped.
 *  - 0x100A6B60 = "%ry" and 0x100A6B64 = "%y1": text-markup colour
 *    escapes, the pause menu's "selected / not selected" prefixes.
 *
 * STATE (2026-09-09): 4501/4500 B, 1376/1376 instructions, register-blind
 * 1+1, RAW 6+6, two masked regions.  The lever that got here from 4503 B /
 * 4+3 was STATEMENT ORDER in the mirror block: `hMir = wMir >> 2` FIRST,
 * straight after the wMir switch.  Placed after yMir, VC5 keeps the car
 * record global (DAT_106e9d88) in ebx from the `if` condition to the store
 * (`lea ecx,[ebx+0x27c4]` where the original has `add ecx,0x27c4` and a
 * fresh `mov eax,[g]` later) AND spills the view counter through a
 * top-of-loop reload behind a `jmp`; with hMir first, ebx is taken from
 * the switch on and both artefacts disappear.  Register OCCUPANCY decides
 * a cross-block global-load CSE; the crank's line-level levers never move
 * a statement inside the block, so this class needs a hand.
 *
 * RESIDUE -- 2026-09-09b: site 1 CLOSED, one site left plus its shadow:
 *
 *  (1) CLOSED (probe X7): `pV = &aViews[i]` moved ABOVE the BrPodNop
 *      trace call.  The call clobbers the scratch register holding 11*i,
 *      so the fold is no longer free and VC5 re-derives the read through
 *      esi exactly as the original: 4501 -> 4500 B (size exact), regnorm
 *      1+1 -> 0+0.  The dead list below is the record of every spelling
 *      tried BELOW the call; the lever was the statement's position, not
 *      its spelling.  Shadow: our lea for pV now schedules above the
 *      call's five constant pushes where the original has it below --
 *      2 masked regions, 5+5 raw, register-blind 0+0, +0 B.  DEAD on the
 *      shadow (2026-09-09b): `g_brIView = i` between (worse, +6 B);
 *      volatile pV (+70 B); the index via a `k = i` copy; pV in the for
 *      condition (+3 B); the read spelled `aViews[i].iCar` beside it;
 *      declaration order `y, x`; the sum through `y`.
 *
 *  (1-old) orig+0x131, the loop top, AS IT STOOD.  The original forms
 *      `pV` (`lea esi,[ebp+ecx*8]`) and reads the car index through it
 *      (`mov ecx,[esi+0x10]`) after all six constant pushes; we folded
 *      the first read into `mov ecx,[ebp+eax*8+0x10]` and formed esi
 *      after.  DEAD
 *      (all byte-identical to what is here): `aViews + i`; the byte-cast
 *      `(BrHudView *)((uint8_t *)aViews + i * 0x58)`; `pV = aViews;
 *      pV += i`; `(BrHudView *)(DAT_103c2fd0 + off) + i` (worse, +17 B);
 *      a macro for aViews (worse, +24 B); the read as
 *      `*(int32_t *)((uint8_t *)pV + 0x10)`, as a volatile read, as
 *      `aViews[i].iCar` beside the pointer in either order; the
 *      assignment embedded `(pV = &aViews[i])->iCar` or comma-joined;
 *      the three global stores comma-joined into FUN_1006ec30's third
 *      argument with or without the pointer; a dead `pV = aViews` /
 *      `pV = 0` before the loop; `const BrHudView *pV`; `pV` declared
 *      first, last, before pCars; `g_brIView = i` before the read (worse);
 *      `register int i`; `i` first/last among the ints, `k` first.
 *      `pV = &aViews[i]` BEFORE the trace call reads through esi but
 *      schedules the lea ahead of the call (4502 B, 3+2) -- so the
 *      original's definition sits after the call.
 *  (2) orig+0xaf, the first block: `add edx,eax` / `push edx` where we
 *      emit `add eax,edx` / `push eax` -- the two-address destination of
 *      `aViews->x + aViews->w`, both operands dead after.  DEAD: operand
 *      order (canonicalised); the sum through the `x` local; `aViews->w`
 *      through the `n` local; both.  The lever that closed 0x1005FF00's
 *      twin of this (name BOTH operands, later-declared symbol is the
 *      destination) is INERT here: `(n = aViews->w)` / `(x = aViews->x)
 *      + n` as statements or inside the expressions, with `n` before `x`
 *      or after, and with fresh names `wv`/`xv` in both orders -- all
 *      byte-identical to what is here.  The difference from the race
 *      site: `w` has a second use (`0x130 - w`) before the add.
 */

/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include "br_coretypes.h"   /* br_globals: its objects */
#include "br_race.h"   /* br_globals: its objects */
#include "slice1_05.h"   /* br_globals: its objects */
#include <stdio.h>
#include "slice3_41.h"
#include "br_camwide.h"
#include "br_platview.h"
/* initialised in the original source (restored: the 64-bit core keeps them
 * private to this file; no Glide relocation places them elsewhere) */
static const float kF728C = 0.6499999761581421f;
static const float kF7290 = 0.699999988079071f;
static const float kF7294 = -0.5f;
static const float kF7298 = 0.20000000298023224f;
static const float kF729C = 0.30000001192092896f;
static const float kF72A0 = 0.5f;

typedef unsigned int   uint32_t;
typedef unsigned short uint16_t;
typedef unsigned char  uint8_t;
typedef int            int32_t;

/* ------------------------------------------------------------------ */
/* Records                                                            */
/* ------------------------------------------------------------------ */
/* One view of the split screen: slice2_15.h's BrHudView, 0x58 bytes. */
typedef struct BrHudView {
    int32_t  x, y, w, h;        /* +0x00 .. +0x0C */
    int32_t  iCar;              /* +0x10  the car this view follows      */
    uint32_t aDial[16];         /* +0x14 .. +0x50                        */
    uint32_t dlOverlay;         /* +0x54                                 */
} BrHudView;

/* A camera object: eye position at +0x30, field of view at +0x40. */
typedef struct BrCamObj {
    uint8_t  pad00[0x30];
    float    pos[4];            /* +0x30 */
    float    fov;               /* +0x40 */
} BrCamObj;

#define BR_SLOT_STRIDE   0x2E0F0    /* one slot record (slice2_14.h)      */
#define BR_CAR_STRIDE    0x2B68     /* one car record (br_drawcar.h)      */

/* The slot's car-pointer table: 0x80-byte entries from +0x60, first
 * dword is the car (br_drawcar.h). */

/* ------------------------------------------------------------------ */
/* Callees                                                            */
/* ------------------------------------------------------------------ */
/* BrRecHdrLatch_10010F80: prototype in br_funcs.h */
/* BrFadeTick: prototype in br_funcs.h */
/* BrFrameBeginRec: prototype in br_funcs.h */
/* BrPodNop: prototype in br_funcs.h */
/* BrViewBuffersRebase: prototype in br_funcs.h */
/* BrGfxFillRect: prototype in br_funcs.h */
/* BrGfxClearScreen: prototype in br_funcs.h */
/* FUN_1006ec30: prototype in br_funcs.h */
/* BrDlRectCmdEmit: prototype in br_funcs.h */
/* BrCamFrustumBuild: prototype in br_funcs.h */
/* BrCamMatrixSetup: prototype in br_funcs.h */
/* FUN_1002af17: prototype in br_funcs.h */
/* FUN_1002b480: prototype in br_funcs.h */
/* BrSceneSetupFrame: prototype in br_funcs.h */
/* BrSceneAccumReset: prototype in br_funcs.h */
/* BrSpanBuildHull: prototype in br_funcs.h */
/* BrCarVisibilityUpdate: prototype in br_funcs.h */
/* BrSceneDlBuild: prototype in br_funcs.h */
/* BrCarDrawVehicle: prototype in br_funcs.h */
/* BrNop6E590: prototype in br_funcs.h */
/* FUN_10011650: prototype in br_funcs.h */
/* FUN_10011d20: prototype in br_funcs.h */
/* BrCarDrawBody: prototype in br_funcs.h */
/* FUN_100119c0: prototype in br_funcs.h */
/* BrEnvEmit: prototype in br_funcs.h */
/* BrFadeDrawSprite: prototype in br_funcs.h */
/* BrCamMatrixSetupOrtho: prototype in br_funcs.h */
/* BrDlScreenRectEmit: prototype in br_funcs.h */
/* BrNop_1002AB94: prototype in br_funcs.h */
/* BrDlBorderEmit: prototype in br_funcs.h */
/* BrFpsReadout: prototype in br_funcs.h */
/* BrTextFlag358Clear: prototype in br_funcs.h */
/* BrSet_10019270: prototype in br_funcs.h */
/* BrSetGlobal_ABB30: prototype in br_funcs.h */
/* BrStrGet: prototype in br_funcs.h */
/* BrTextDraw: prototype in br_funcs.h */
/* BrSub_10019280: prototype in br_funcs.h */
/* BrSub_10019290: prototype in br_funcs.h */
/* FUN_10014e00: prototype in br_funcs.h */
/* BrHudDrawViewMessage: prototype in br_funcs.h */
/* BrHudDraw: prototype in br_funcs.h */
/* BrNop_1002C509: prototype in br_funcs.h */
/* BrHudDrawViewCentreText: prototype in br_funcs.h */
/* BrNop_1002AB8F: prototype in br_funcs.h */
/* BrSub_1003289F: prototype in br_funcs.h */
/* BrHudTextListDraw: prototype in br_funcs.h */
/* BrTextSetColors: prototype in br_funcs.h */
/* BrFadeDrawBars: prototype in br_funcs.h */
/* FUN_1002cb49: prototype in br_funcs.h */
/* FUN_1002cee9: prototype in br_funcs.h */

/* ------------------------------------------------------------------ */
/* Globals                                                            */
/* ------------------------------------------------------------------ */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* slot +0x004: view header, car table at +0x60 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* slot +0xA08: 16 car records, 0x2B68 each     */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* slot +0x2C088: BrHudView aViews[]            */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* slot +0x2C0E4: the record header latched     */
/* 64-bit core: declared once, in br_globals.h or its struct's header */      /* frames left to clear the whole screen        */
/* 64-bit core: declared once, in br_globals.h or its struct's header */      /* view count the clear counter was armed for   */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */        /* 0x100AA044 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */         /* 0x106EC798 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */      /* the car the current view follows             */
/* 64-bit core: declared once, in br_globals.h or its struct's header */      /* its active camera                            */
/* 64-bit core: declared once, in br_globals.h or its struct's header */      /* 400.0f, the far plane                        */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */      /* 0x100B2F04 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x100B2F00 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */          /* 0x104B16A0 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */          /* 0x104B16AC */
/* 64-bit core: declared once, in br_globals.h or its struct's header */      /* mirror size selector 1/2/3               */
/* 64-bit core: declared once, in br_globals.h or its struct's header */      /* game mode                                */
/* 64-bit core: declared once, in br_globals.h or its struct's header */      /* screen height                            */
/* 64-bit core: declared once, in br_globals.h or its struct's header */      /* screen width                             */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */ /* 0x105BC760 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */      /* pause state                              */
/* 64-bit core: declared once, in br_globals.h or its struct's header */      /* pause-menu cursor                        */
/* 64-bit core: declared once, in br_globals.h or its struct's header */       /* 0x10226A48 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* "%ry" */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* "%y1" */
/* 64-bit core: declared once, in br_globals.h or its struct's header */        /* 0x10B71A6C */
/* 64-bit core: declared once, in br_globals.h or its struct's header */        /* 0x10B71A68 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* sprintf scratch                          */
/* 64-bit core: declared once, in br_globals.h or its struct's header */      /* credits page                             */
/* 64-bit core: declared once, in br_globals.h or its struct's header */ /* credits: title, 4.0f, six lines        */
/* 64-bit core: declared once, in br_globals.h or its struct's header */      /* credits page timer                       */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x1007728C */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* 0x10077290 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                 /* 0x10077294 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */  /* 0x10077298 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */  /* 0x1007729C */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                  /* 0x100772A0 */

/* WHAT IT DOES: draws one whole frame for a slot of the split screen.  For
 * every view it builds that view's camera and scene display list, draws
 * the cars in three passes (opaque, bodies, translucent), the track
 * environment and the fade sprite, then -- for a single-view race with the
 * rear-view mirror on -- repeats the scene into the mirror rectangle with
 * the car's second camera before restoring the first.  After the views it
 * lays the overlays on top: the FPS readout, the split-screen captions
 * ("wait for player" style prompts by game mode), the pause menu with its
 * highlighted entry, the attract-mode credits page, the fade bars, and
 * finally closes the frame.  Debug colour markers bracket every stage;
 * their callee is a bare `ret`. */
/* @t4-pass 0x10011FA0 1 2026-09-07 probes 103 bytes 4503 insns 1377 regions 10 rows 7 census yes  (tools/brally/crank.py) */
/* @t4-pass 0x10011FA0 2 2026-09-07 probes 103 bytes 4503 insns 1377 regions 10 rows 7 census yes  (tools/brally/crank.py) */
/* @t4-pass 0x10011FA0 3 2026-09-09 probes 35 bytes 4501 insns 1376 regions 2 rows 2 census no  (hand: hMir first) */
/* @t4-pass 0x10011FA0 4 2026-09-09 probes 11 bytes 4500 insns 1376 regions 2 rows 0 census yes  (hand: corpus MISS at +0x131/+0xaf; site-1 fresh angles -- X7 pV-before-call landed, size exact) */
/* @t4-pass 0x10011FA0 5 2026-09-09 probes 11 bytes 4500 insns 1376 regions 2 rows 0 census yes  (hand: shadow-lea and site-2 fresh angles -- zero movement) */
/* @t3 0x10011FA0 2026-09-09 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 4500/4500 insns 1376/1376 rows 0+0 regions 2 oracle UNCLASSIFIED
 * @t3-effort passes 5 zero-movement 4 5
 * Two regions, both scheduler/allocator picks between equal operands: the
 * loop-top lea for pV scheduled above the trace call's constant pushes
 * (original: below), and the +0xaf two-address add destination
 * (`add edx,eax` vs `add eax,edx`, both operands dead after).  The full
 * dead lists live in this header (sites 1, 1-old, 2 and the shadow).
 * Do not reopen before the end-grind. */
/* @implements 0x10011FA0 glide BrFrameDraw */
void BrFrameDraw(int iSlot)
{
    BrDriverCar *pCars, *pCar;
    uint8_t *pHdr;
    BrHudView *aViews, *pV;
    BrCamObj  *pCamSave;
    const char *psz;
    int        off, i, k, n, x, y;
    int        wMir, hMir, xMir, yMir;

    off    = iSlot * BR_SLOT_STRIDE;
    /* one snapshot slot (0x2E0F0 bytes in the original) per iSlot */
    (void)off;
    pCars  = g_aBrSnap[iSlot].car;
    pHdr   = (uint8_t *)g_aBrSnap[iSlot].drv;
    aViews = (BrHudView *)&g_aBrSnap[iSlot].tailA.a[0];
    plat_glide_wide();   /* port: a race frame fills the window (br_platview.h) */
    BrRecHdrLatch_10010F80((uint8_t *)&g_aBrSnap[iSlot].tailC.a[0]);
    BrFadeTick();
    BrFrameBeginRec(aViews);
    BrPodNop();
    BrViewBuffersRebase();
    BrPodNop();

    if (DAT_100a64b8 == 0 && (*(int *)((char *)&g_aBrEntRecs + 0x68)) == 0) {
        if (aViews->w < 0x130)
            BrGfxFillRect(aViews->x + aViews->w, 8, 0x130 - aViews->w, 0xe0,
                          0, 0, 0);
    } else {
        BrGfxClearScreen(0, 0, 0);
        if (DAT_100a64b8 != 0)
            DAT_100a64b8 = DAT_100a64b8 - 1;
    }
    if (g_brMode0AA8B4 != DAT_100a64bc) {
        DAT_100a64b8 = 2;
        DAT_100a64bc = g_brMode0AA8B4;
    }

    for (i = 0; i < g_brMode0AA8B4; i++) {
        /* pV BEFORE the trace call: assigned after it (any spelling, see
         * the site-1 dead list) VC5 folds the first read into
         * `[ebp+eax*8+0x10]`; assigned before, the call clobbers the
         * scratch that held 11*i, the read is re-derived through esi and
         * matches.  This is what closed site 1 (2026-09-09, probe X7):
         * 4501 -> 4500 B, regnorm 1+1 -> 0+0. */
        pV = &aViews[i];
        BrPodNop();
        (*(uint8_t * *)&g_pBr63Race) = (uint8_t *)((BrDriverCar *)pCars + pV->iCar);
        (*(BrCamObj * *)&g_BrCamera) = ((*(BrCamObj **)((*(uint8_t * *)&g_pBr63Race) + offsetof(struct BrDriverCar, pMatA))));
        (*(int *)&g_BrEnvSection) = i;
        FUN_1006ec30(0, 0, (*(BrCamObj * *)&g_BrCamera)->pos, (*(uint8_t (*)[])&g_BrEnvFlagIndices), (*(uint8_t (*)[])&g_BrEnvFlagCount),
                     DAT_106ed590, DAT_106b7ac4, DAT_106e728c, DAT_106ec780);
        BrDlRectCmdEmit(pV->x, pV->y, pV->w, pV->h, 1);
        /* port: the view's lens and culling wedge widened to fill a window
         * of any shape (br_camwide.h); the mirror below keeps the game's */
        if (g_brMode0AA8B4 > 1) {
            BrCamFrustumBuild((*(BrCamObj * *)&g_BrCamera), (*(BrCamObj * *)&g_BrCamera)->fov * kF7290,
                              DAT_100aa040 * kF728C, (float)pV->w, (float)pV->h);
            BrCamFrustumWiden();
            BrCamMatrixSetupWide((*(BrCamObj * *)&g_BrCamera), (*(BrCamObj * *)&g_BrCamera)->fov * kF7290,
                                 DAT_100aa040 * kF728C, (float)pV->w, (float)pV->h);
        } else {
            BrCamFrustumBuild((*(BrCamObj * *)&g_BrCamera), (*(BrCamObj * *)&g_BrCamera)->fov, DAT_100aa040,
                              (float)pV->w, (float)pV->h);
            BrCamFrustumWiden();
            BrCamMatrixSetupWide((*(BrCamObj * *)&g_BrCamera), (*(BrCamObj * *)&g_BrCamera)->fov, DAT_100aa040,
                                 (float)pV->w, (float)pV->h);
        }
        BrFrameFogEmit();
        BrFrameTintSetup();
        BrPodNop();
        BrSceneSetupFrame(aViews);
        BrSceneAccumReset();
        BrSpanBuildHull();
        (*(int *)&g_BrDrawRefIndex) = ((*(int *)((char *)&g_aBrEntRecs + 0x7C)) != 0) + 1;
        for (k = 0; k < (*(int *)&g_BrCarCount); k++)
            BrCarVisibilityUpdate((uint8_t *)((BrDriverCar *)pCars + k));
        BrSceneDlBuild(aViews, 0, pHdr, pCars);
        if ((*(int *)((char *)&g_aBrEntRecs + 0x80)) == 0 || (*(int *)&g_Br0B380C) == 2 || (*(int *)&g_Br0B380C) == 8) {
            BrFrameFogEmit();
            BrFrameTintSetup();
            BrPodNop();
            for (k = 0; k < (*(int *)&g_brRaceNDriver); k++) {
                pCar = (((BrDriver *)pHdr)[k].pCar);
                if (pCar != 0 && ((pCar->b29AF)) != 2)
                    BrCarDrawVehicle(pCar, 0);
            }
            BrPodNop();
            BrNop6E590();
            FUN_10011650(aViews);
        }
        BrPodNop();
        FUN_10011d20();
        BrSceneDlBuild(aViews, 1, pHdr, pCars);
        if ((*(int *)((char *)&g_aBrEntRecs + 0x80)) != 0 && (*(int *)&g_Br0B380C) != 2 && (*(int *)&g_Br0B380C) != 8) {
            BrFrameFogEmit();
            BrFrameTintSetup();
            BrPodNop();
            for (k = 0; k < (*(int *)&g_brRaceNDriver); k++) {
                pCar = (((BrDriver *)pHdr)[k].pCar);
                if (pCar != 0 && ((pCar->b29AF)) != 2)
                    BrCarDrawVehicle(pCar, 0);
            }
            BrPodNop();
            BrNop6E590();
            FUN_10011650(aViews);
        }
        BrPodNop();
        for (k = 0; k < (*(int *)&g_brRaceNDriver); k++) {
            pCar = (((BrDriver *)pHdr)[k].pCar);
            if (pCar != 0)
                BrCarDrawBody(pCar);
        }
        BrPodNop();
        for (k = 0; k < (*(int *)&g_brRaceNDriver); k++) {
            pCar = (((BrDriver *)pHdr)[k].pCar);
            if (pCar != 0 && ((pCar->b29AF)) == 2)
                BrCarDrawVehicle(pCar, 0);
        }
        BrPodNop();
        FUN_100119c0(aViews, pHdr);
        BrPodNop();
        BrEnvEmit();
        BrPodNop();
        BrFadeDrawSprite(aViews, g_4B16AC - g_4B16A0 * kF7294);
        BrCamMatrixSetupOrtho((float)pV->w, (float)pV->h);
        BrDlScreenRectEmit(pV->x, pV->y, pV->w, pV->h, 1);

        /* The rear-view mirror: single view, the car's first camera active
         * and a mirror size selected.  The scene is drawn again into a
         * strip along the top through the car's second camera. */
        if ((*(BrCamObj * *)&g_BrCamera) == (BrCamObj *)(((*(uint8_t * *)&g_pBr63Race) + offsetof(struct BrDriverCar, aSnap[2].m[0][0])))
            && g_brMode0AA8B4 == 1 && DAT_100aa018 != 0) {
            if (DAT_100aa018 == 1)
                wMir = pV->w / 4;
            else if (DAT_100aa018 == 2)
                wMir = (pV->w * 5) / 16;
            else
                wMir = (pV->w * 3) / 8 + 2;
            /* hMir FIRST.  Its `mov ebx,edi` copy sits thirty bytes ahead
             * of its `sar` in the original: the statement is here, and ebx
             * being taken from this point on is what denies the car-record
             * global a register across the `if` (proven 2026-09-09: after
             * yMir it is 4503 B / 4+3 rows and VC5 keeps DAT_106e9d88 in
             * ebx from the condition to the store and spills the loop
             * counter through a top-of-loop reload; here it is 4501 B /
             * 1+1 rows and both artefacts are gone). */
            hMir = wMir >> 2;
            xMir = ((pV->w - wMir) >> 1) + pV->x;
            yMir = pV->h / 16 + pV->y;
            pCamSave = (*(BrCamObj * *)&g_BrCamera);
            ((*(BrCamObj **)((*(uint8_t * *)&g_pBr63Race) + offsetof(struct BrDriverCar, pMatA)))) = (BrCamObj *)(((*(uint8_t * *)&g_pBr63Race) + offsetof(struct BrDriverCar, aSnap[5].m[0][0])));
            (*(BrCamObj * *)&g_BrCamera) = ((*(BrCamObj **)((*(uint8_t * *)&g_pBr63Race) + offsetof(struct BrDriverCar, pMatA))));
            BrDlRectCmdEmit(xMir, yMir, -wMir, hMir, 1);
            BrNop_1002AB94();
            BrCamFrustumBuild((*(BrCamObj * *)&g_BrCamera), (*(BrCamObj * *)&g_BrCamera)->fov,
                              DAT_100aa040 * kF7298, (float)wMir, (float)hMir);
            BrCamMatrixSetup((*(BrCamObj * *)&g_BrCamera), (*(BrCamObj * *)&g_BrCamera)->fov,
                             DAT_100aa040 * kF729C, (float)wMir, (float)hMir);
            BrFrameFogEmit();
            BrFrameTintSetup();
            BrPodNop();
            BrSceneSetupFrame(aViews);
            BrSceneAccumReset();
            BrSpanBuildHull();
            BrPodNop();
            for (k = 0; k < (*(int *)&g_BrCarCount); k++)
                BrCarVisibilityUpdate((uint8_t *)((BrDriverCar *)pCars + k));
            BrSceneDlBuild(aViews, 0, pHdr, pCars);
            if ((*(int *)((char *)&g_aBrEntRecs + 0x80)) == 0 || (*(int *)&g_Br0B380C) == 2 || (*(int *)&g_Br0B380C) == 8) {
                BrFrameFogEmit();
                BrFrameTintSetup();
                BrPodNop();
                for (k = 0; k < (*(int *)&g_brRaceNDriver); k++) {
                    pCar = (((BrDriver *)pHdr)[k].pCar);
                    if (pCar != 0 && ((pCar->b29AF)) != 2)
                        BrCarDrawVehicle(pCar, 0);
                }
                BrPodNop();
                BrNop6E590();
                FUN_10011650(aViews);
            }
            BrSceneDlBuild(aViews, 1, pHdr, pCars);
            if ((*(int *)((char *)&g_aBrEntRecs + 0x80)) != 0 && (*(int *)&g_Br0B380C) != 2 && (*(int *)&g_Br0B380C) != 8) {
                BrFrameFogEmit();
                BrFrameTintSetup();
                BrPodNop();
                for (k = 0; k < (*(int *)&g_brRaceNDriver); k++) {
                    pCar = (((BrDriver *)pHdr)[k].pCar);
                    if (pCar != 0 && ((pCar->b29AF)) != 2)
                        BrCarDrawVehicle(pCar, 0);
                }
            }
            BrPodNop();
            for (k = 0; k < (*(int *)&g_brRaceNDriver); k++) {
                pCar = (((BrDriver *)pHdr)[k].pCar);
                if (pCar != 0)
                    BrCarDrawBody(pCar);
            }
            BrPodNop();
            for (k = 0; k < (*(int *)&g_brRaceNDriver); k++) {
                pCar = (((BrDriver *)pHdr)[k].pCar);
                if (pCar != 0 && ((pCar->b29AF)) == 2)
                    BrCarDrawVehicle(pCar, 0);
            }
            BrFadeDrawSprite(aViews, g_4B16A0 + g_4B16AC);
            BrPodNop();
            ((*(BrCamObj **)((*(uint8_t * *)&g_pBr63Race) + offsetof(struct BrDriverCar, pMatA)))) = pCamSave;
            (*(BrCamObj * *)&g_BrCamera) = pCamSave;
            BrDlRectCmdEmit(pV->x, pV->y, pV->w, pV->h, 1);
            BrDlBorderEmit(xMir, yMir, wMir, hMir);
        }

        BrFpsReadout();

        /* The split-screen captions, by game mode. */
        if ((*(int *)&g_brRaceRules.mode) == 5) {
            BrTextFlag358Clear();
            BrSet_10019270();
            BrSetGlobal_ABB30(0x28);
            if (*(char *)(pCars->pEquip + 4) != 0)
                BrTextDraw(BrStrGet(0xF0), g_scrW4 / 2, (*(int *)&g_brRaceCueBase) - 0x1E);
            else
                BrTextDraw(BrStrGet(0xF1), g_scrW4 / 2, (*(int *)&g_brRaceCueBase) - 0x1E);
        } else if (DAT_100aa024 != 0 && (*(int *)&g_brRaceRules.mode) != 4) {
            if ((*(int *)&DAT_105ccb68[8]) != 0) {
                BrTextFlag358Clear();
                BrSub_10019280();
                BrSetGlobal_ABB30(0xF);
                BrTextDraw(BrStrGet(0xF2), 0x1C, 0x20);
                if ((*(int *)&g_brRaceRules.mode) == 6)
                    BrHudDrawEntrants(aViews, pCars);
                BrSub_10019290();
                BrTextDraw(BrStrGet(0xF4), g_scrW4 - 0x1C,
                           (*(int *)&g_brRaceCueBase) - 0x18);
            } else if ((*(BrCamObj * *)&g_BrCamera) != (BrCamObj *)(((*(uint8_t * *)&g_pBr63Race) + offsetof(struct BrDriverCar, aSnap[3].m[0][0])))) {
                /* The original's branch is `je` into the message arm, so
                 * the HUD arm is the FALLTHROUGH: the test is `!=` and
                 * this arm comes first in the source. */
                BrPodNop();
                BrDlScreenRectEmit(pV->x, pV->y, pV->w, pV->h, 1);
                BrHudDraw(aViews, pCars);
                BrHudDrawEntrants(aViews, pCars);
                BrPodNop();
                if ((*(BrCamObj * *)&g_BrCamera) != (BrCamObj *)(((*(uint8_t * *)&g_pBr63Race) + offsetof(struct BrDriverCar, aSnap[2].m[0][0]))))
                    BrPodNop();
            } else if (*(uint8_t *)&((struct BrDriverCar *)(*(uint8_t * *)&g_pBr63Race))->pProfile->f68 & 2) {
                BrHudDrawViewMessage(aViews);
            }
        /* Both conditions are COMPOUND and both name the same two globals:
         * that is what makes the original re-test them at the attract arm's
         * head (`test ecx,ecx / je` and `cmp eax,4 / jne`) while still
         * cross-jumping the shared BrSub_10019290/BrTextDraw tail.  Nesting
         * this under a bare `if (DAT_100aa024 != 0)` lets VC5 fold both
         * tests away; making it a separate statement keeps them but loses
         * the cross-jump (three duplicated calls). */
        } else if (DAT_100aa024 != 0 && (*(int *)&g_brRaceRules.mode) == 4
                   && g_5bc760 == 0) {
            BrTextFlag358Clear();
            BrSetGlobal_ABB30(0xF);
            BrSub_10019290();
            BrTextDraw(BrStrGet(0xF4), g_scrW4 - 0x1C,
                       (*(int *)&g_brRaceCueBase) - 0x18);
        }
        BrNop_1002C509();
        BrHudDrawViewCentreText(aViews);
    }

    BrNop_1002AB8F();
    BrSub_1003289F(0, 0, g_scrW4, (*(int *)&g_brRaceCueBase));

    /* The pause menu. */
    if ((*(int *)&g_BrX06909B4) == 2) {
        x = (aViews->w >> 1) + aViews->x;
        BrPodNop();           /* 0.33f as its bits: a 4-byte push */
        BrTextFlag358Clear();
        BrSet_10019270();
        BrSetGlobal_ABB30(0x28);
        BrTextDraw(BrStrGet(0xF5), x, ((*(int *)&g_brRaceCueBase) * 5) / 16);
        y = ((*(int *)&g_brRaceCueBase) * 5) / 11;
        BrSetGlobal_ABB30(0x14);
        BrTextDraw(BrStrGet(0xF6), x, y);
        y += 0x28;
        if (DAT_105bc8dc == 0)
            psz = BrStrGet(0xF7);
        else
            psz = BrStrGet(0xF8);
        BrTextDraw(psz, x, y);
        y += 0x14;
        if (DAT_105bc8dc == 1)
            psz = BrStrGet(0xF9);
        else
            psz = BrStrGet(0xFA);
        BrTextDraw(psz, x, y);
    } else if ((*(int *)&g_BrX06909B4) != 0) {
        x = (aViews->w >> 1) + aViews->x;
        BrPodNop();
        BrTextFlag358Clear();
        BrSet_10019270();
        BrSetGlobal_ABB30(0x28);
        BrTextDraw(BrStrGet(0xF5), x, ((*(int *)&g_brRaceCueBase) * 5) / 16);
        y = ((*(int *)&g_brRaceCueBase) * 5) / 11;
        BrSetGlobal_ABB30(0x14);
        if (DAT_105bc8dc == 0)
            psz = BrStrGet(0xFB);
        else
            psz = BrStrGet(0xFC);
        BrTextDraw(psz, x, y);
        y += 0x14;
        if ((*(int *)&g_brRaceNet) == 0) {
            if (DAT_105bc8dc == 1)
                psz = BrStrGet(0xFF);
            else
                psz = BrStrGet(0x100);
            BrTextDraw(psz, x, y);
        }
        y += 0x14;
        sprintf(DAT_10396f28, BrStrGet(0x101),
                DAT_105bc8dc == 2 ? DAT_100a6b64 : DAT_100a6b60, (*(int *)&g_brRaceB71A6C));
        BrTextDraw(DAT_10396f28, x, y);
        y += 0x14;
        sprintf(DAT_10396f28, BrStrGet(0x102),
                DAT_105bc8dc == 3 ? DAT_100a6b64 : DAT_100a6b60, (*(int *)&g_brItemIconCount));
        BrTextDraw(DAT_10396f28, x, y);
        y += 0x14;
        if ((*(int *)&g_brRaceRules.mode) == 0) {
            if (DAT_105bc8dc == 4)
                psz = BrStrGet(0x103);
            else
                psz = BrStrGet(0x104);
        } else {
            if (DAT_105bc8dc == 4)
                psz = BrStrGet(0x105);
            else
                psz = BrStrGet(0x106);
        }
        BrTextDraw(psz, x, y);
        y += 0x14;
        if (DAT_105bc8dc == 5)
            psz = BrStrGet(0x107);
        else
            psz = BrStrGet(0x108);
        BrTextDraw(psz, x, y);
    }

    /* Attract mode: the text list, or a credits page once its timer runs. */
    if ((*(int *)&g_brRaceRules.mode) == 4 && g_5bc760 == 2) {
        BrHudTextListDraw(aViews);
    } else if ((*(int *)&g_brRaceRules.mode) == 4 && g_5bc760 == 1
               && g_brRaceRules.aCard[(*(int *)&g_brRaceBeginSeqIdx)].pszTitle != 0
               && g_brRaceBeginSeqT > kF72A0) {
        BrTextFlag358Clear();
        BrSet_10019270();
        BrTextSetColors(0xff, 0xff, 0xff, 0xff, 0xff, 0xff);
        for (n = 0; n < 6; n++) {
            if (g_brRaceRules.aCard[(*(int *)&g_brRaceBeginSeqIdx)].apszLine[n] == 0)
                break;
        }
        y = (*(int *)&g_brRaceCueBase) / 2 - (n * 40) / 4 + 10;
        if (g_brRaceRules.aCard[(*(int *)&g_brRaceBeginSeqIdx)].pszTitle[0] == 0)
            y -= 5;
        for (n--; n >= 0; n--) {
            if (g_brRaceRules.aCard[(*(int *)&g_brRaceBeginSeqIdx)].apszLine[n][0] == '`') {
                BrSetGlobal_ABB30(0xF);
                BrTextDraw(g_brRaceRules.aCard[(*(int *)&g_brRaceBeginSeqIdx)].apszLine[n] + 1,
                           g_scrW4 / 2, (n * 40) / 2 + y);
            } else {
                BrSetGlobal_ABB30(0x14);
                BrTextDraw(g_brRaceRules.aCard[(*(int *)&g_brRaceBeginSeqIdx)].apszLine[n],
                           g_scrW4 / 2, (n * 40) / 2 + y);
            }
        }
        BrSetGlobal_ABB30(0xF);
        BrTextDraw(g_brRaceRules.aCard[(*(int *)&g_brRaceBeginSeqIdx)].pszTitle, g_scrW4 / 2, y - 0x14);
    }

    BrFadeDrawBars();
    if ((*(int *)((char *)&g_aBrEntRecs + 0x8C)) >= 2) {
        BrDlRectCmdEmit(0, 0, g_scrW4, (*(int *)&g_brRaceCueBase), 1);
        BrPodNop();
    }
    BrPodNop();
    BrTexAnimStep();
    BrPodNop();
    BrFrameEnd();
}

