/* br_cd.c -- audio.
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

int FUN_10002c50();
extern int g_brCdEnabled;
extern int g_brCdPlaying;
extern int g_brCdTrackCur;
extern int g_brCdTrackFirst;

/* WHAT IT DOES: play the previous CD track, clamping to the first track. */
/* @implements 0x10002C70 glide BrCdTrackPrev */

int BrCdTrackPrev(void)

{
  int iVar1;
  
  if ((g_brCdEnabled != 0) && (g_brCdPlaying != 0)) {
    iVar1 = FUN_10002c50();
    g_brCdTrackCur = iVar1 + -1;
    if (iVar1 + -1 < g_brCdTrackFirst) {
      g_brCdTrackCur = g_brCdTrackFirst;
    }
    BrCdTrackPlay(g_brCdTrackCur);
  }
  return 1;
}

#endif /* BR_MATCHING_BUILD */
