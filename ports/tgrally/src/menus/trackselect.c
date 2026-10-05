/* trackselect.c -- the track-select screen
 */
#include "tgr/common.h"
#include "tgr/car.h"
#include "tgr/menu.h"

/* -- declarations -- */
int BrTrackIsPresent(int n);
extern int D_8026FF18;
extern int D_8028AE04;
extern int D_8028B940;
extern int D_80315EE0;
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
  return (TGR_PTR(BrSeason *, D_8031B760[0].season)->xce & (1 << n)) && BrTrackIsPresent(n);
}

