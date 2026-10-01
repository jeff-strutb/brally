/* br_glidestate.c -- drawing: putting the 3dfx card into a drawing state.
 *
 * RESPONSIBILITY: drawing/ -- turn geometry and images into pixels.
 *
 * Filed out of slice2_17.c, an address batch and not a module.  Everything
 * here talks to Glide directly: the standard render state a frame opens
 * with, the fog table, binding one texture slot to the card, and emptying
 * the card's texture table again.
 *
 * slice2_17.c's preamble is carried over verbatim.  An include set that
 * looks redundant has already been shown elsewhere in this module to move
 * VC5's register allocation (see br_rdpmode.c), so nothing is trimmed here
 * on the grounds that it is unused.
 */
/* slice2_17.h prototypes a list pointer the original never takes. */
#define BrPtrListContains BrPtrListContains_port
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include "br_dl.h"   /* br_globals: its objects */
#include "slice1_05.h"   /* br_globals: its objects */
#include "slice2_17.h"
#undef BrPtrListContains

#include <math.h>
#include <stdio.h>
#include <string.h>


/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* guFogGenerateLinear: prototype in br_funcs.h */
/* grFogTable: prototype in br_funcs.h */

/* WHAT IT DOES: choose the fog distances for the current situation and hand
 * them to the fog table generator. Three of the four cases share the same
 * wide range and only the ordinary in-race case pulls the fog in close,
 * which is why the branch chain looks redundant. */
/* @implements 0x10023AA0 glide FUN_10023aa0 */
/* auto-filed from ghidra --refine; transforms: as-is */

void FUN_10023aa0(void)

{
  if (DAT_106ed6a8 != 0) {
    if (BrG_6C6624 != 0) {
      guFogGenerateLinear(&DAT_105d1718,-100.0f,300.0f);
    }
    else if (DAT_106ed6b0 != 0) {
      guFogGenerateLinear(&DAT_105d1718,-100.0f,300.0f);
    }
    else if (BrG_6C661C != 0) {
      guFogGenerateLinear(&DAT_105d1718,60.0f,200.0f);
    }
    else {
      guFogGenerateLinear(&DAT_105d1718,-40.0f,220.0f);
    }
  }
  grFogTable(&DAT_105d1718);
  return;
}

/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* shadow of the alpha-blend RGB source factor */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* shadow of the alpha-blend RGB dest factor   */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* shadow of the alpha-blend A source factor   */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* shadow of the alpha-blend A dest factor     */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* DL nesting depth, reset to 0 each frame     */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* previous frame's millisecond timestamp      */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* most recent frame delta (ms)                */
/* 64-bit core: declared once, in br_globals.h or its struct's header */  /* 0x100B4C28  ring length                      */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x100B4C2C  ring write index (< 0 == empty) */
/* 64-bit core: declared once, in br_globals.h or its struct's header *//* 0x10B73348  base of the inline sample ring  */
/* 64-bit core: declared once, in br_globals.h or its struct's header */  /* the status/watermark text drawn each frame  */

typedef void (*BrFlipHook)(void);
/* 64-bit core: declared once, in br_globals.h or its struct's header */      /* 0x106B7AB8  buffer-flip callback   */
/* 64-bit core: declared once, in br_globals.h or its struct's header */     /* DL write cursor                    */

/* grAlphaCombine: prototype in br_funcs.h */
/* grAlphaBlendFunction: prototype in br_funcs.h */
/* grDepthMask: prototype in br_funcs.h */
/* BrTexQueuePop: prototype in br_funcs.h */
/* BrGbiRun: prototype in br_funcs.h */
/* BrSetGlobal_ABB30: prototype in br_funcs.h */
/* BrTextDraw: prototype in br_funcs.h */
/* BrSub10075020: prototype in br_funcs.h */

/* WHAT IT DOES: shows one finished frame. It resets the card to the frame's
 * standard alpha/blend/depth state, runs the display list it was handed,
 * lays a line of status text over the top, flips the finished image to the
 * screen, and then folds this frame's duration into the running table of
 * frame times the FPS readout averages. */
/* @t4-pass 0x10023B70 1 2026-09-10 probes 40 bytes 273 insns 78 regions 2 rows 0 census yes  (tools/crank.py) */
/* @t4-pass 0x10023B70 2 2026-09-10 probes 40 bytes 273 insns 78 regions 2 rows 0 census yes  (tools/crank.py) */
/* @t3 0x10023B70 2026-09-10 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 273/273 insns 78/78 rows 0+0 regions 2 oracle UNCLASSIFIED
 * @t3-effort passes 2 zero-movement 1 2
 * residue after tools/crank.py: 40 compiles this pass, levers accepted: none;
 * every candidate and score is in build/match/crank.log.
 * Do not reopen before the end-grind. */
/* @implements 0x10023B70 glide BrFramePresent */
void BrFramePresent(BrGfxWords *pCmd)
{
    BrGfxWords aList[0x1000];
    int now, delta, count, gate;

    grAlphaCombine(3, 8, 1, 1, 0);
    DAT_105d17a8 = 4;
    DAT_105d1758 = 0;
    DAT_105ccfd4 = 4;
    DAT_105ccfe4 = 0;
    grAlphaBlendFunction(4, 0, 4, 0);
    grDepthMask(1);
    DAT_105ccfe8 = 0;
    BrTexQueuePop();
    BrGbiRun(pCmd);

    /* Build a one-item list in place: the text draw appends its commands
     * through the shared cursor, then we cap it with the end marker and run
     * the local list. */
    DAT_106e7710 = aList;
    BrSetGlobal_ABB30(0x14);
    BrTextDraw(&DAT_118ee590, 0, 0x3c);
    {
        BrGfxWords *p = DAT_106e7710++;
        p->w0 = 0xB8000000u;
        p->w1 = 0;
    }
    BrGbiRun(aList);

    (*DAT_106b7ab8)();

    now = BrSub10075020();
    delta = now - DAT_105d17dc;
    DAT_105d17dc = now;
    DAT_105d17e0 = delta;
    count = g_BrFpsCountB;
    gate = g_BrFpsGateB;
    if (gate < 0) {
        gate = 0;
        if (count > 0) {
            /* First frame: seed every slot with this delta. */
            int i;
            for (i = 0; i < count; ++i)
                (&g_BrFpsSamplesB)[i] = delta;
            gate = count;
        }
    }
    ++gate;
    g_BrFpsGateB = gate;
    if (gate >= count) {
        g_BrFpsGateB = gate = 0;
    }
    (&g_BrFpsSamplesB)[gate] = delta;
}

/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* grClipWindow: prototype in br_funcs.h */
/* grDepthBufferMode: prototype in br_funcs.h */
/* grDepthBufferFunction: prototype in br_funcs.h */
/* grDepthMask: prototype in br_funcs.h */
/* grBufferClear: prototype in br_funcs.h */
/* grCullMode: prototype in br_funcs.h */
/* grAlphaCombine: prototype in br_funcs.h */
/* grAlphaTestFunction: prototype in br_funcs.h */
/* grAlphaTestReferenceValue: prototype in br_funcs.h */
/* grTexFilterMode: prototype in br_funcs.h */
/* grConstantColorValue: prototype in br_funcs.h */
/* grColorCombine: prototype in br_funcs.h */
/* grTexCombine: prototype in br_funcs.h */

/* WHAT IT DOES: put the 3dfx card into the game's standard drawing state for
 * a frame -- scissor box to the full screen, depth buffering on and cleared,
 * culling off, and the default alpha and colour combiners. Called once at
 * the top of rendering so nothing inherits state from the previous frame. */
/* @implements 0x1001DFB0 glide FUN_1001dfb0 */
/* auto-filed from ghidra --refine; transforms: as-is */

void FUN_1001dfb0(void)

{
  int uVar1;
  int uVar2;
  int uVar3;
  int uVar4;
  
  grClipWindow(0,0,g_BrFpsScreenW,g_BrFpsScreenH);
  grDepthBufferMode(2);
  grDepthBufferFunction(7);
  grDepthMask(1);
  grBufferClear(0,0,0xffff);
  grCullMode(0);
  grAlphaCombine(3,8,1,1,0);
  grAlphaTestFunction(7);
  grAlphaTestReferenceValue(0x80);
  grTexFilterMode(0,0,0);
  if (1 < DAT_105ccbd0) {
    grTexFilterMode(1,0,0);
  }
  grConstantColorValue(0xffffffff);
  grColorCombine(1,0,0,2,0);
  if (1 < DAT_105ccbd0) {
    grTexCombine(1, 1, 0, 1, 0, 0, 0);
    grTexCombine(0, 3, 8, 3, 8, 0, 0);
  }
  else {
    grTexCombine(0, 1, 0, 1, 0, 0, 0);
  }
  return;
}

/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* grTexSource: prototype in br_funcs.h */
/* grTexClampMode: prototype in br_funcs.h */
/* grTexFilterMode: prototype in br_funcs.h */
/* grTexLodBiasValue: prototype in br_funcs.h */
/* grTexMipMapMode: prototype in br_funcs.h */

/* WHAT IT DOES: bind one texture slot to the 3dfx card: sets its source,
 * clamping, filtering and level-of-detail bias from the stored table entry.
 * Silently does nothing if the index is past the high-water mark or the slot
 * is empty. */
/* @implements 0x10028420 glide FUN_10028420 */
/* auto-filed from ghidra --refine; transforms: as-is */

void FUN_10028420(unsigned int param_1)

{
  BrTexSlot *slot;
  int tmu;

  if ((param_1 < DAT_105d17ec) && (slot = &g_aBrTexSlot[param_1], slot->bLive != 0)) {
    tmu = slot->tmu;
    grTexSource(tmu,slot->start,slot->evenOdd,&slot->info);
    grTexClampMode(tmu,slot->clampS,slot->clampT);
    grTexFilterMode(tmu,slot->minFilter,slot->magFilter);
    grTexLodBiasValue(tmu,(float)(unsigned int)slot->lodBias * _DAT_10077460);
    grTexMipMapMode(tmu,slot->mipMode,slot->lodBlend);
  }
  return;
}


/* 64-bit core: duplicate declaration removed */

/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* grTexMinAddress: prototype in br_funcs.h */

/* WHAT IT DOES: clear the whole texture table -- marks all 256 slots free
 * and resets the high-water mark, so the next frame's textures start from an
 * empty card. */
/* @implements 0x100281C0 glide FUN_100281c0 */
/* auto-filed from ghidra --refine; transforms: callconv */

void FUN_100281c0(void)

{
  int i;

  for (i = 0; i < 0x400; i++) {
    g_aBrTexSlot[i].bLive = 0;
  }
  DAT_105d17ec = 0;
  for (i = 0; i < 2; i++) {
    g_aBrTmuMem[i].next = grTexMinAddress(i);
    g_aBrTmuMem[i].hi = 0x200000;
  }
  return;
}

/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* WHAT IT DOES: make texture record `idx` the card's current texture, unless
 * it already is. Binds the slot to the card (0x10028420), switches the
 * combine units if the record's mode changed (a `switch` on the mode: VC5
 * lowers the lone `case 1` to `dec/jne`, where an `if (x==1)` spells
 * `cmp,1`), then copies the record's UV scale pair and its active tile's
 * four cached fields into the globals the triangle emitters read. */
/* @implements 0x100284E0 glide BrTex3dMakeCurrent */

void BrTex3dMakeCurrent(int param_1)

{
  int iVar1;

  if (param_1 != DAT_105e1808) {
    FUN_10028420(*(int *)(DAT_106b7aa0 + param_1 * 0x2b4));
    iVar1 = *(int *)(DAT_106b7aa0 + 4 + param_1 * 0x2b4);
    if (DAT_105e1820 != iVar1) {
      switch (iVar1) {
      case 1:
        grTexCombine(1,1,0,1,0,0,0);
        grTexCombine(0,3,8,3,8,0,0);
        break;
      default:
        grTexCombine(0,1,0,1,0,0,0);
      }
      DAT_105e1820 = *(int *)(DAT_106b7aa0 + 4 + param_1 * 0x2b4);
    }
    _DAT_118ed1a4 = *(float *)(DAT_106b7aa0 + 0x2ac + param_1 * 0x2b4);
    _DAT_118ed1a8 = *(float *)(DAT_106b7aa0 + 0x2b0 + param_1 * 0x2b4);
    _DAT_118ed198 =
         *(int *)(*(int *)(DAT_106b7aa0 + 0x5c + param_1 * 0x2b4) * 0x40 + DAT_106b7aa0 + 0x94 +
                  param_1 * 0x2b4);
    _DAT_1186c950 =
         *(int *)(*(int *)(DAT_106b7aa0 + 0x5c + param_1 * 0x2b4) * 0x40 + DAT_106b7aa0 + 0x98 +
                  param_1 * 0x2b4);
    _DAT_1186c954 =
         *(int *)(*(int *)(DAT_106b7aa0 + 0x5c + param_1 * 0x2b4) * 0x40 + DAT_106b7aa0 + 0x9c +
                  param_1 * 0x2b4);
    _DAT_118ec988 =
         *(int *)(*(int *)(DAT_106b7aa0 + 0x5c + param_1 * 0x2b4) * 0x40 + DAT_106b7aa0 + 0xa0 +
                  param_1 * 0x2b4);
    DAT_105e1808 = param_1;
  }
  return;
}

