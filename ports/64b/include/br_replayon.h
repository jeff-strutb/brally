/* br_replayon.h -- racing: replay recording on/off, and the race clock.
 *
 * Responsibility: the rules of a race (what gets recorded, how long
 * this race has been running, the RNG seed a race starts from).
 */
#ifndef BR_REPLAYON_H
#define BR_REPLAYON_H
#ifdef __cplusplus
extern "C" {  /* BR_CLINK_BEGIN: every original function has C linkage */
#endif

#include <stdint.h>

/* 0x1006AA90  turn replay recording on.  0x1006AAA0  is it on? */
/* BrSet_1006AA90: prototype in br_funcs.h */
uint32_t BrGet_1006AAA0(void);
/* 0x1006A990  how many players to record; 1 installs the 1-player trio. */
void     BrMode_1006A990(uint32_t nPlayers);
/* 0x1003BD40  plant the race RNG seed. */
/* BrStore_1003BD40: prototype in br_funcs.h */
/* 0x10078C10 (glide 0x10071F00)  advance the 64-bit "now" counter by a
 * fixed slice and return it.  The counter is returned WHOLE, in edx:eax --
 * that 64-bit return is what pins the original's register pair; callers
 * that want a millisecond-ish number just take the low half. */
/* BrTickAdd_10078C10: prototype in br_funcs.h */
/* 0x100713A0  current counter minus the value stored at race start. */
/* BrDelta_100713A0: prototype in br_funcs.h */

/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
extern uint32_t g_18ABDE0, g_18ABDE4;
/* 64-bit core: declared once, in br_globals.h or its struct's header */

#ifdef __cplusplus
}  /* BR_CLINK_END */
#endif
#endif
