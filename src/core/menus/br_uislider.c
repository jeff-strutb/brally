/* br_uislider.c -- menus: the two front-end slider rows. Stepping a selection
 * index up or down and latching the byte that step maps to.
 *
 * BrUiVolumeApply, the commit that pushes those bytes at the audio code, was
 * handed over from audio/ and lives here with the sliders it commits.
 *
 * Filed out of slice3_40.c, whose preamble it keeps verbatim below so the
 * compiler's view of these bodies is unchanged.  The original banner follows.
 *
 * slice3_40.c -- Boss Rally (BRD3D.dll) slice 3, a later pass.
 *
 * See slice3_40.h for the packet contents, the offset maps and every
 * GOTCHA.  This file carries only the code and the line-level notes.
 *
 * x87 NOTE (applies throughout).  The original is MSVC x87 code and the CRT
 * leaves the precision control at 53 bits, so an intermediate the original
 * never spills carries double precision, not float.  Wherever that is
 * observable the intermediate is a `double` here and the rounding to float
 * happens exactly where the original has an `fstp dword`.  Where the
 * original stores every intermediate, plain float is used.
 */

#include <string.h>

#ifdef BR_MATCHING_BUILD
/* Header prototype is cdecl; the original is thiscall.  Rename the
 * prototype so the thiscall definition is not a C2373 redefinition. */
#define BrCarInitTables BrCarInitTables_cdecl_hdr
#define BrCarClear29C8  BrCarClear29C8_cdecl_hdr
#define BrZeroRegions   BrZeroRegions_cdecl_hdr
#endif
#include "slice3_40.h"
#ifdef BR_MATCHING_BUILD
#undef BrCarInitTables
#undef BrCarClear29C8
#undef BrZeroRegions
void BrZeroRegions(void);
#endif

#include "br_match.h"    /* BR_THISCALL1 */

#ifdef BR_MATCHING_BUILD
extern unsigned char DAT_100ad770;
extern unsigned char DAT_100ad798;
extern unsigned char DAT_100bb2e0;
extern unsigned char DAT_100bb2e8;

/* WHAT IT DOES: step selection index A up (clamped at 9) and latch its byte from the
 * 4-stride table at 0x100AD770 into 0x100BB2E0. */
/* @implements 0x10059DC0 glide BrUiSelAInc */

void BrUiSelAInc(void)

{
  if (g_brB4E70C < 9) {
    g_brB4E70C = g_brB4E70C + 1;
  }
  DAT_100bb2e0 = (&DAT_100ad770)[g_brB4E70C * 4];
  return;
}

/* WHAT IT DOES: step selection index A down (clamped at 0) and latch its byte. */
/* @implements 0x10059DE0 glide BrUiSelADec */

void BrUiSelADec(void)

{
  if (0 < g_brB4E70C) {
    g_brB4E70C = g_brB4E70C + -1;
  }
  DAT_100bb2e0 = (&DAT_100ad770)[g_brB4E70C * 4];
  return;
}

/* WHAT IT DOES: step selection index B up (clamped at 9) and latch its byte from the
 * 4-stride table at 0x100AD798 into 0x100BB2E8. */
/* @implements 0x10059E30 glide BrUiSelBInc */

void BrUiSelBInc(void)

{
  if (g_brB4E708 < 9) {
    g_brB4E708 = g_brB4E708 + 1;
  }
  DAT_100bb2e8 = (&DAT_100ad798)[g_brB4E708 * 4];
  return;
}

/* WHAT IT DOES: step selection index B down (clamped at 0) and latch its byte. */
/* @implements 0x10059E50 glide BrUiSelBDec */

void BrUiSelBDec(void)

{
  if (0 < g_brB4E708) {
    g_brB4E708 = g_brB4E708 + -1;
  }
  DAT_100bb2e8 = (&DAT_100ad798)[g_brB4E708 * 4];
  return;
}
/* Declared here with a BYTE parameter, which is not how slice1_01.c defines
 * it. The call below pushes eax with the index's upper three bytes still in
 * it -- VC5 only leaves a stack argument dirty when the callee's parameter is
 * a byte type. The definition at 0x10002D30 reads the whole slot and hands it
 * straight on, so both spellings agree on the only byte that is ever read. */
extern int BrCdVolumeSet(unsigned char v);

/* WHAT IT DOES: push both volume selections where the audio code reads them --
 * selection A becomes the music volume (and is applied to the CD/mixer right
 * away), selection B becomes the sound-effect master volume, which the SFX
 * code picks up from the global on its own. This is the "commit" for the two
 * sliders BrUiSelAInc/Dec and BrUiSelBInc/Dec move. */
/* @implements 0x10059E00 glide BrUiVolumeApply */

void BrUiVolumeApply(void)

{
  DAT_100bb2e0 = (&DAT_100ad770)[g_brB4E70C * 4];
  BrCdVolumeSet(DAT_100bb2e0);
  DAT_100bb2e8 = (&DAT_100ad798)[g_brB4E708 * 4];
  return;
}


#endif /* BR_MATCHING_BUILD */
