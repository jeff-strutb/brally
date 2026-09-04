/* br_hud.c -- drawing.
 *
 * Filed out of the address batches: these functions were
 * matched first and grouped by what they are afterwards.
 * Every function carries its original address.
 */
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import
 * table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include <stdint.h>

#ifdef BR_MATCHING_BUILD

extern int DAT_100a5eb0;
extern int DAT_100a5ebc;
extern int DAT_106e7714;
extern int DAT_106e9a2c;
int BrSetGlobal_ABB30();
int BrSet_10019270();
int BrTextFlag358Clear();
int BrTextSetColors();

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
  if (DAT_100a5ebc != 0) {
    piVar2 = &DAT_100a5eb0;
    do {
      if ((*piVar2 > -0x50) && (*piVar2 < DAT_106e9a2c + 0x28)) {
        BrSetGlobal_ABB30(piVar2[1]);
        BrTextDraw((const char *)piVar2[3],DAT_106e7714 / 2,*piVar2);
      }
      iVar1 = piVar2[7];
      piVar2 = piVar2 + 4;
    } while (iVar1 != 0);
  }
  return;
}

#endif /* BR_MATCHING_BUILD */
