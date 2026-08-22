/* br_replayon.h -- racing: replay recording on/off, and the race clock.
 *
 * Responsibility: the rules of a race (what gets recorded, how long
 * this race has been running, the RNG seed a race starts from).
 */
#ifndef BR_REPLAYON_H
#define BR_REPLAYON_H

#include <stdint.h>

/* 0x1006AA90  turn replay recording on.  0x1006AAA0  is it on? */
void     BrSet_1006AA90(void);
uint32_t BrGet_1006AAA0(void);
/* 0x1006A990  how many players to record; 1 installs the 1-player trio. */
void     BrMode_1006A990(uint32_t nPlayers);
/* 0x1003BD40  plant the race RNG seed. */
void     BrStore_1003BD40(uint32_t seed);
/* 0x10078C10  advance the 64-bit "now" counter by a fixed slice. */
void     BrTickAdd_10078C10(void);
/* 0x100713A0  current counter minus the value stored at race start. */
int      BrDelta_100713A0(void);

extern uint32_t g_1750308;
extern uint32_t g_B502E4;
extern uint32_t g_690A20, g_B501C8, g_0B8C94;
extern uint32_t g_A9BFD0;
extern uint32_t g_18ABDE0, g_18ABDE4;
extern uint32_t g_178FEE8;

#endif
