/* trackselect.c -- the track-select screen
 */
#include "tgr/common.h"
#include "tgr/car.h"

/* -- declarations -- */
int BrTrackIsPresent(int n);
extern int D_8026FF18;
extern int D_8028AE04;
extern int D_8028B940;
extern int D_80315EE0;
extern int D_8027086C;
/* -- end declarations -- */

/* WHAT IT DOES: Tell whether track row n may be chosen on the track-select
 * screen. In a Championship (mode 0): the season's current track (0x8028B940)
 * and, unless 0x80315EE0 is set, every row from 0x8028AE04 on. In mode 2:
 * rows 0x8028AE04 and 0x8028AE04 + 2 always. Otherwise: rows whose bit is set
 * in player 1's season track mask (+0xCE) and whose track is present. */
/* @implements 0x80208900 tgr BrTrackSelectable */
int BrTrackSelectable(int n)
{
  if (D_8026FF18 == 0) {
    if (D_80315EE0 != 0) {
      return n == D_8028B940;
    }
    return n == D_8028B940 || n >= D_8028AE04;
  }
  if (D_8026FF18 == 2 && (n == D_8028AE04 || n == D_8028AE04 + 2)) {
    return 1;
  }
  return (D_8031B760[0].season->xce & (1 << n)) && BrTrackIsPresent(n);
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
