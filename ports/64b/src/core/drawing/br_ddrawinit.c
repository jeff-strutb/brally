/* br_ddrawinit.c -- drawing: the loading-screen / bitmap-table setup.
 *
 * 0x100583C0 sits between BrFontFreeAndExit (0x10058360) and the sprite-font
 * draw (0x10058380). It allocates the font destination surface if needed,
 * optionally blits images\loading.bmp, then walks the {surface, path} table
 * at 0x10AC53E8 loading every named bitmap.
 *
 * RESIDUE 54B /O2, FIRSTDIV +0x110, REGNORM 2+1. First 272 B match. Two
 * leftovers: (1) after BrBmpLoadSurface orig stores h then reloads *p then
 * addesp; recomp reloads *p, addesp, then stores h. volatile store did not
 * reorder it. (2) loop back orig `jl body` falling into `return 1`; recomp
 * `jge success; jmp body` because the fail-sprintf block sits between the
 * loop and the success epilogue. goto-fail after return 1 still laid fail
 * in that hole. Same inversion class as 0x10027A70.
 * DEAD (2026-09-06): splitting the fused `h = (p[-1] = call())` into
 * `h = call(); p[-1] = h;` did NOT move the store earlier; rewriting the
 * loop as an explicit `do { } while ((int)p < limit)` did NOT flip the back
 * edge. Both residues are compiler scheduling/layout, not statement form.
 * REGNORM 2+1, +2 B, still parked.
 * @t4-pass 0x100583C0 1 2026-09-06 probes 2 bytes 379 insns 118 regions 2 rows 3 census no */

#define _CRTIMP __declspec(dllimport)
#include "br_coretypes.h"   /* br_globals: its objects */
#include "br_uispr.h"   /* br_globals: its objects */
#include "slice2_25.h"   /* br_globals: its objects */
#include <stdio.h>

/* BrSurfNew: prototype in br_funcs.h */
/* BrBmpLoadSurface: prototype in br_funcs.h */
/* BrFontFreeAndExit: prototype in br_funcs.h */
/* BrSprFontDraw: prototype in br_funcs.h */
/* BrSurfFree: prototype in br_funcs.h */
/* BrSurfSetColourKey: prototype in br_funcs.h */

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

typedef void (__fastcall *BrVt0)(int this);

/* WHAT IT DOES: set up the bitmap table the menus draw from. Allocates the
 * off-screen font surface on first call; when the "already inited" flag is
 * set, also puts up images\loading.bmp, draws the loading sprite and tears
 * that surface down. Then walks every {surface, path} slot, loading each
 * named BMP, bumping the live-surface count and giving it a colour key of
 * 0xFF00. A failed load of a still-named slot is fatal. Returns 1. */
/* @t4-pass 0x100583C0 2 2026-09-07 probes 88 bytes 381 insns 119 regions 2 rows 3 census yes  (tools/crank.py) */
/* @t4-pass 0x100583C0 3 2026-09-07 probes 89 bytes 381 insns 119 regions 2 rows 3 census yes  (tools/crank.py) */
/* @t3 0x100583C0 2026-09-09 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 381/379 insns 119/118 rows 1+2 regions 2 oracle UNCLASSIFIED
 * @t3-effort passes 2 zero-movement 2 3
 * residue is the either-or layout fork on the bitmap-load loop's exit
 * (jl against jge+jmp, cancelled as the branch triple) -- do-while and
 * top-test respellings probed worse 2026-09-09; two crank census passes.
 * Do not reopen before the end-grind. */
/* @implements 0x100583C0 glide FUN_100583c0 */
int FUN_100583c0(void)
{
  char buf[0x100];
  int *p;
  int i;
  struct BrSurf *h;
  BrVt0 pfn;

  if (DAT_10ac5d84 == 0) {
    DAT_10ac5d84 = BrSurfNew(BrGbiRectG_A7514, BrGbiRectG_A7518);
    if (DAT_10ac5d84 == 0) {
      BrFontFreeAndExit();
      return;
    }
  }
  if (DAT_10ac5dc4 != 0) {
    (*(int *)&g_img) = BrBmpLoadSurface(s_images_loading_bmp_100ad71c, 0, 0);
    if ((*(int *)((char *)&g_img + 0x4)) /* BR_LP64_BYTE_VIEW */ != 0 && (*(int *)&g_img) == 0) {
      sprintf(buf, s_DDraw_DoInit__loading_bmp_failed_100ad6f0);
      BrFontFreeAndExit();
      return;
    }
    pfn = *(BrVt0 *)(*(int *)g_brPAA29B8 + 0x20);
    pfn(g_brPAA29B8);
    BrSprFontDraw(0, 0, 0, (int)&(*(int *)&g_aBrUiSprite[4]), (*(int *)&g_aBrUiSprite[20]));
    pfn = *(BrVt0 *)(*(int *)g_brPAA29B8 + 0x14);
    pfn(g_brPAA29B8);
    if ((*(int *)&g_img) != 0) {
      BrSurfFree((*(int *)&g_img));
      (*(int *)&g_img) = 0;
    }
  }
  p = &(*(int *)((char *)&g_img + 0x4)) /* BR_LP64_BYTE_VIEW */;
  DAT_10ac5dc4 = DAT_10ac5dc4 + 1;
  i = 0;
  for (;;) {
    if (*p != 0) {
      h = (p[-1] = BrBmpLoadSurface((char *)*p, 0, 0));
      if (*p != 0 && h == 0) {
        sprintf(buf, s_DDraw_DoInit__Bitmap__d_failed_t_100ad6c8, i);
        BrFontFreeAndExit();
        return;
      }
      *(short *)&DAT_10ac5c2c = (short)(*(short *)&DAT_10ac5c2c + 1);
      BrSurfSetColourKey(h, 0xff00);
    }
    p = p + 2;
    i = i + 1;
    if ((int)p >= 0x10ac5874) {
      break;
    }
  }
  return 1;
}

