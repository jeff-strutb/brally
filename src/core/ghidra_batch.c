/* Ghidra-decompiled functions that match bit-exact but cannot live in their
 * named modules due to header/context conflicts.  See comments on each. */
#ifdef BR_MATCHING_BUILD

#include <windows.h>

int FUN_10035400();
void BrUiSprClip();
extern int BrSndG0B5DE8;
extern int BrSndG18290FC;
extern int BrSndPDS;
extern int DAT_10ac53e8;
extern int DAT_10ac5d84;
extern int g_aBrSndBankVoice;

/* WHAT IT DOES: get the desktop window and call into the display setup path. */
/* NOTE: context-sensitive codegen — matches here but not in br_drawcar.c. */
/* @implements 0x10009C00 glide BrDesktopSetup */

int BrDesktopSetup(void)

{
  GetDesktopWindow();
  FUN_10035400();
  return;
}

/* WHAT IT DOES: draw one clipped sprite glyph from the font sheet. */
/* NOTE: BrUiSprClip takes 7 args in the port header, 6 in the original. */
/* @implements 0x10058380 glide BrSprFontDraw */

int BrSprFontDraw(int param_1,int param_2,unsigned int param_3,int param_4,
                 int param_5)

{
  BrUiSprClip(DAT_10ac5d84,param_1,param_2,(&DAT_10ac53e8)[(param_3 & 0xffff) * 2],param_4,param_5)
  ;
  return;
}

/* WHAT IT DOES: set the pan value on a sound voice by bank index. */
/* NOTE: context-sensitive codegen — matches here but not in slice6_76.c. */
/* @implements 0x1006B5B0 glide BrSndVoiceSetPan */

int BrSndVoiceSetPan(int param_1,int param_2)

{
  if (((BrSndG0B5DE8 != 0) && (BrSndPDS != 0)) && (BrSndG18290FC != 0)) {
    if ((&g_aBrSndBankVoice)[param_1] != 0) {
      *(int *)((&g_aBrSndBankVoice)[param_1] + 0x18) = param_2;
      return 1;
    }
    return 0;
  }
  return 1;
}

#endif /* BR_MATCHING_BUILD */
