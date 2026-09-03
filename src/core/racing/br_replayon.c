/* br_replayon.c -- racing.  See br_replayon.h. */
#include "br_replayon.h"
#include "br_objlife.h"

#ifdef BR_MATCHING_BUILD
extern uint32_t g_1750308, g_B502E4;
extern uint32_t g_690A20, g_B501C8, g_0B8C94;
extern uint32_t g_A9BFD0, g_18ABDE0, g_18ABDE4, g_178FEE8;
void BrExt_10024460(void);
void BrExt_1002A640(void);
int  BrExt_10075020(void);
void BrExt_10024460(void) {}
void BrExt_1002A640(void) {}
int  BrExt_10075020(void) { return 0; }
#else
uint32_t g_1750308, g_B502E4;
uint32_t g_690A20, g_B501C8, g_0B8C94;
uint32_t g_A9BFD0, g_18ABDE0, g_18ABDE4, g_178FEE8;
void BrExt_10024460(void);
void BrExt_1002A640(void);
int  BrExt_10075020(void);
#endif

/* WHAT IT DOES: turn replay recording on. */
/* @implements 0x1006AA90 d3d BrSet_1006AA90 */
/* @n64 0x8021C6B8 located */
void BrSet_1006AA90(void)
{
    g_1750308 = 1;
}

/* WHAT IT DOES: is replay recording on? */
/* @d3donly 0x1006AAA0 BrGet_1006AAA0 -- exists in BRGlide only as folded/duplicated stubs; no unique twin locatable by bytes */
uint32_t BrGet_1006AAA0(void)
{
    return g_1750308;
}

/* WHAT IT DOES: remember how many players the replay should record.  If
 * the answer is one, install the three helpers that set that player up,
 * tear them down, and fix their state. */
void BrMode_1006A990(uint32_t n)
{
    g_B502E4 = n;
    if (--n == 0) {
        g_690A20 = (uint32_t)(uintptr_t)&BrInstall_1001BAE0;
        g_B501C8 = (uint32_t)(uintptr_t)&BrExt_10024460;
        g_0B8C94 = (uint32_t)(uintptr_t)&BrExt_1002A640;
    }
}

/* WHAT IT DOES: plant the seed the game's random-number generator uses. */
/* @implements 0x1003BD40 d3d BrStore_1003BD40 */
void BrStore_1003BD40(uint32_t v)
{
    g_A9BFD0 = v;
}

/* WHAT IT DOES: advance the engine's 64-bit "now" counter by a fixed
 * slice (1,562,500 ticks). */
void BrTickAdd_10078C10(void)
{
    uint32_t lo = g_18ABDE0;
    uint32_t hi = g_18ABDE4;

    lo += 0x17D784u;
    hi += (lo < 0x17D784u) ? 1u : 0u;
    g_18ABDE0 = lo;
    g_18ABDE4 = hi;
}

/* WHAT IT DOES: how long has this been running?  Current counter minus
 * the value stored when the run started. */
/* @implements 0x1006A310 glide BrDelta_100713A0 */
int BrDelta_100713A0(void)
{
    return BrExt_10075020() - (int)g_178FEE8;
}

/* ── Ghidra-matched functions ─────────────────────────── */
#ifdef BR_MATCHING_BUILD
int FUN_1002f282();
extern int g_BrReplayOn;
extern int g_a220B20;

/* WHAT IT DOES: return whether replay playback is active. */
/* @implements 0x10063A50 glide BrReplayIsOn */

int BrReplayIsOn(void)

{
  return g_BrReplayOn;
}

/* WHAT IT DOES: set the app mode to 5 (return to menu) and call the mode-change handler. */
/* @implements 0x1006A070 glide BrSetMode5 */

int BrSetMode5(void)

{
  g_a220B20 = 5;
  FUN_1002f282();
  return;
}

#endif /* BR_MATCHING_BUILD */
