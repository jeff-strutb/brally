/* trackselect.c -- the track-select screen
 */
#include "tgr/common.h"

/* -- declarations -- */
int BrTrackIsPresent();
extern int D_8026FF18;
extern int D_8028AE04;
extern int D_8028B940;
extern int D_80315EE0;
extern int D_8031C5BC;
extern int D_8027086C;
/* -- end declarations -- */

/* WHAT IT DOES: Tell whether a track row may be chosen on the track-select
 * screen: in a Championship only the current track, or any track once the
 * season allows it; in Time Attack only the unlocked pairs; otherwise none. */
/* @t4-pass 0x80208900 1 2026-09-26 compiles 17 best 27 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x80208900 2 2026-09-26 compiles 17 best 27 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x80208900 3 2026-09-26 compiles 17 best 27 moved 0  (n64/tools/n64permute.py) */
/* @implements 0x80208900 tgr BrTrackSelectable */
int BrTrackSelectable(unsigned int param_1)
{
  int iVar1;
  int bVar2;
  
  if (D_8026FF18 == 0) {
    if (D_80315EE0 == 0) {
      if (param_1 == D_8028B940) {
        bVar2 = 1;
      }
      else {
        bVar2 = (int)D_8028AE04 <= (int)param_1;
      }
    }
    else {
      bVar2 = param_1 == D_8028B940;
    }
  }
  else if ((D_8026FF18 == 2) && ((param_1 == D_8028AE04 || (param_1 == D_8028AE04 + 2)))) {
    bVar2 = 1;
  }
  else {
    bVar2 = 0;
    if (((unsigned int)*(unsigned short *)(D_8031C5BC + 0xce) & 1 << (param_1 & 0x1f)) != 0) {
      iVar1 = BrTrackIsPresent();
      bVar2 = iVar1 != 0;
    }
  }
  return bVar2;
}

/* WHAT IT DOES: Tell whether track number n exists in this build's track
 * table (below the track count and with a record present). */
/* @t4-pass 0x8021E180 1 2026-09-26 compiles 14 best 14 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x8021E180 2 2026-09-26 compiles 13 best 14 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x8021E180 3 2026-09-26 compiles 9 best 14 moved 0  (n64/tools/n64permute.py) */
/* @implements 0x8021E180 tgr BrTrackIsPresent */
int BrTrackIsPresent(int param_1)
{
  int bVar1;
  
  bVar1 = 0;
  if (param_1 < D_8028AE04) {
    bVar1 = *(int *)(&D_8027086C + param_1 * 0x17c) != 0;
  }
  return bVar1;
}
