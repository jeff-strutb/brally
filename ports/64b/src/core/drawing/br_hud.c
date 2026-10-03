/* br_hud.c -- drawing.
 *
 * Filed out of the address batches: these functions were
 * matched first and grouped by what they are afterwards.
 * Every function carries its original address.
 */
/* The original is /MD: CRT calls go through the import
 * table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include "br_racebegin.h"   /* br_globals: its objects */
#include <stdint.h>


/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* BrSetGlobal_ABB30: prototype in br_funcs.h */
/* BrSet_10019270: prototype in br_funcs.h */
/* BrTextFlag358Clear: prototype in br_funcs.h */
/* BrTextSetColors: prototype in br_funcs.h */

/* WHAT IT DOES: draw the on-screen text list at 0x100A5EB0 (16-byte records:
 * y, color-arg, ?, text): white colors, viewport from the caller's rect,
 * then centre-draw each record whose y sits inside (-0x50, height+0x28). */
/* @implements 0x10013140 glide BrHudTextListDraw */

void BrHudTextListDraw(int *param_1)

{
  int iVar1;
  int *piVar2;

  BrTextSetColors(0xff,0xff,0xff,0xff,0xff,0xff);
  BrTextFlag358Clear();
  BrSet_10019270();
  BrSub_1003289F(*param_1,param_1[1],param_1[2],param_1[3]);
  if (g_aBrRaceCue[0].text != 0) {
    const BrRaceCue *pc = g_aBrRaceCue;
    do {
      if ((pc->start > -0x50) && (pc->start < (*(int *)&g_brRaceCueBase) + 0x28)) {
        BrSetGlobal_ABB30(pc->len);
        BrTextDraw(pc->text,g_scrW4 / 2,pc->start);
      }
      pc++;
    } while (pc->text != 0);
  }
  return;
}

