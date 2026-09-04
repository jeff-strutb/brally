/* br_replayon.c -- racing.  See br_replayon.h. */
#include "br_replayon.h"
#include "br_objlife.h"

#ifdef BR_MATCHING_BUILD
extern uint32_t g_1750308, g_B502E4;
extern uint32_t g_690A20, g_B501C8, g_0B8C94;
extern uint32_t g_A9BFD0, g_18ABDE0, g_18ABDE4, g_178FEE8;
extern int64_t  g_18ABDE0_64;    /* the same storage as g_18ABDE0/g_18ABDE4 */
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
int64_t g_18ABDE0_64;
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
 * slice (1,562,500 ticks).  It is the game's whole notion of time: a
 * deterministic fake clock, not a reading of the machine's, so the n-th
 * call always yields the same value.  The low half doubles as the return
 * value, which is why callers can subtract two of these and rely on the
 * 32-bit wrap. */
/* Three source facts, each worth one instruction:
 *  1. The counter is ONE 64-bit variable, not two dwords.  `+= K` on it is
 *     what emits `add eax,K / adc edx,0`; a lo/hi pair with an explicit
 *     carry test gives `sbb`/`setb` instead.
 *  2. It is read into a LOCAL first.  Updating the global in place makes
 *     VC5 do lo (load, add, store) then hi (load, adc, store) in one
 *     register; the original loads BOTH halves before either store, which
 *     is what a local copy expresses.  Two bytes, since the second load
 *     then cannot use the 5-byte accumulator form.
 *  3. It RETURNS the whole 64-bit value.  That is the only thing that
 *     pins the pair to edx:eax -- returning nothing (or just the low
 *     half) leaves VC5 free to pick ecx for the high word, which it does. */
/* @implements 0x10071F00 glide BrTickAdd_10078C10 */
int64_t BrTickAdd_10078C10(void)
{
    int64_t t = g_18ABDE0_64;

    t += 0x17D784;
    g_18ABDE0_64 = t;
    return t;
}

/* WHAT IT DOES: how long has this been running?  Current counter minus
 * the value stored when the run started. */
/* @implements 0x1006A310 glide BrDelta_100713A0 */
/* @n64 0x80242954 located */
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

extern int DAT_10b73668;

/* WHAT IT DOES: return a pointer to the primary replay data buffer. */
/* @implements 0x10063B40 glide BrReplayGetBuf */

char * BrReplayGetBuf(void)

{
  return &DAT_10b73668;
}

extern int DAT_10cf3668;

/* WHAT IT DOES: return a pointer to the secondary replay data buffer. */
/* @implements 0x10063DA0 glide BrReplayGetBuf2 */

char * BrReplayGetBuf2(void)

{
  return &DAT_10cf3668;
}

extern unsigned int DAT_10b7364c;

/* WHAT IT DOES: store how many 0x18-byte replay records fit in a byte count
 * (the inverse of BrReplayGetSize's count * 0x18). */
/* @implements 0x10063DB0 glide BrReplayCountFromBytes */

unsigned int BrReplayCountFromBytes(unsigned int cb)

{
  /* mov eax,edx; shr eax,4 -- the quotient is copied into EAX because the
   * assignment's value is also RETURNED. */
  return DAT_10b7364c = cb / 0x18;
}

#endif /* BR_MATCHING_BUILD */
