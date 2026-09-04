/* br_replay.c -- racing: the replay ring, the parts that are byte-exact.
 *
 * Clearing a recording, rewinding playback, putting a car where the
 * recording says it was, and the recording's size in bytes. Filed out of
 * slice3_42.c; the ring itself and the session globals stay with the rest of
 * the replay code there, and br_replayon.h owns the on/off half.
 *
 * See slice3_42.h for the recovered layouts and the GOTCHAs.
 */
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include "slice3_42.h"

/* Byte access into an untyped car record, the slice2_17.h convention. */
#define BR_CAR_I32(p, off) (*(int32_t *)(void *)((unsigned char *)(p) + (off)))

/* 0x1006AA50 */
/* WHAT IT DOES: throws away any recording in progress and switches recording
 * off, so the next race starts with an empty replay. How many cars it clears
 * depends on the game mode -- one in the two single-car modes, eight
 * otherwise. */
/* @implements 0x1006AA50 d3d BrReplayReset */
void BrReplayReset(void)
{
    int n;
    int i;

    /* Open-coded: two callers keep BrReplayActiveCount from inlining. */
    n = 8;
    if (g_BrX0AA010 == 2 || g_BrX0AA010 == 4)
        n = 1;

    /* The `test ecx,ecx / jle` guard is dead (n is 1 or 8) but is kept as the
     * loop bound so the shape matches. */
    for (i = 0; i < n; ++i)
        g_BrReplayCount[i] = 0;

    g_BrReplayOn = 0;
}

/* 0x1006ABB0 */
/* WHAT IT DOES: sends the replay back to the start -- every car's playback
 * position returns to its first recorded frame. The recording itself is
 * untouched. */
/* @implements 0x1006ABB0 d3d BrReplayRewind */
void BrReplayRewind(void)
{
    int i;
    for (i = 0; i < BR_REPLAY_PLAYERS; ++i)
        g_BrReplayCursor[i] = 0;
}

/* 0x1006ACF0 */
/* WHAT IT DOES: the same as the above, for a car that already knows its own
 * number -- it looks the number up on the car rather than being told it. */
/* @implements 0x1006ACF0 d3d BrReplayApplyCar */
void BrReplayApplyCar(void *pCar)
{
    BrReplayApply(pCar, BR_CAR_I32(pCar, BR_S42_CAR_OFF_INDEX));
}

#ifdef BR_MATCHING_BUILD

/* WHAT IT DOES: return the byte size of the current replay (frame count * 0x18). */
/* @implements 0x10063B50 glide BrReplayGetSize */
/* @n64 0x80226070 located */

int BrReplayGetSize(void)

{
  return g_BrReplayCount[0] * 0x18;
}

#endif /* BR_MATCHING_BUILD */
