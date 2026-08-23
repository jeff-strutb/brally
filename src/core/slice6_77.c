/* slice6_77.c -- see slice6_77.h for the identification of both functions,
 * the evidence for the storage they use, and the gotchas. */
#include "slice6_77.h"

#include "br_slots.h"    /* BrSlot */
#include "slice2_25.h"   /* g_aBrAA2538, g_brAA288C, g_brB4E1D0/D4/E0,
                          * g_aBrB4DF30 and its stride/count                 */
#include "slice3_45.h"   /* BrFfbInit (0x100791D0), g_brFfb; pulls in
                          * slice1_10.h for BrFfbShutdown (0x10079550)       */

/* ==========================================================================
 * 0x100586A0
 * ========================================================================== */

/* WHAT IT DOES: clears the player slot table and its counter at the start or
 * end of a session. Beware that the counter it clears doubles as the gate
 * that permits network sends, so clearing it here re-opens that gate --
 * behaviour of the original, not of this port. */
/* @implements 0x100586A0 d3d BrSub100586A0 */
void BrSub100586A0(void)
{
    /* Cursor on field `a` (0x10AA253C): stores are [eax-4]/[eax]/[eax+4],
     * bound 0x10AA259C. Zero is reused from edx; -1 from ecx. The original
     * compare is signed (`jl`). */
    int32_t *p;
    int32_t nZero;
    int32_t nEmpty;

    nZero = 0;
    p = (int32_t *)((char *)g_aBrAA2538 + 4);
    g_brAA288C = nZero;
    nEmpty = -1;
    do {
        p[-1] = nEmpty;
        p[0]  = nZero;
        p[1]  = nZero;
        p += 3;
#ifdef BR_MATCHING_BUILD
    } while ((int32_t)p < (int32_t)((char *)g_aBrAA2538 + 0x64));
#else
    } while (p < (int32_t *)((char *)g_aBrAA2538 + 0x64));
#endif
}

/* ==========================================================================
 * 0x100795D0
 * ========================================================================== */

/* WHAT IT DOES: re-detects the force-feedback wheel. It forces a known
 * configuration, runs the force-feedback setup and immediately tears it down
 * again -- the probe is the setup attempt itself -- and then restores the
 * settings the player had, selecting the matching device record on the way
 * back. */
/* @implements 0x100795D0 d3d BrFfbReprobe */
void BrFfbReprobe(void)
{
    int32_t nSavedMode = g_brB4E1D0;   /* esi */
    int32_t nSavedExcl = g_brB4E1E0;   /* edi */

    /* 0x100795DE..0x100795F2 -- force the known probe configuration: mode 2,
     * record 2 (0x10B4E080), exclusive. */
    g_brB4E1D0 = 2;
    g_brB4E1D4 = g_aBrB4DF30[2];
    g_brB4E1E0 = 1;

    /* 0x100795FC / 0x10079601. BrFfbInit's result is discarded by the
     * original (it does not even keep eax past the next call), so the
     * "disabled vs already up" distinction its header describes is not used
     * here. BrFfbShutdown then unwinds the nested-init count this raised. */
    (void)BrFfbInit();
    BrFfbShutdown(&g_brFfb);

    /* 0x10079606 -- the mode is restored before the selection chain, because
     * the chain decrements the register that held it. */
    g_brB4E1D0 = nSavedMode;

    /* 0x1007960C..0x1007965F -- `dec/je` three times over 1, 2, 3 with
     * everything else falling through to record 0. Written the way
     * slice2_25.c writes the same chain for 0x10043400, so the two sites read
     * as the one idiom they are. Mode 0 and any mode > 3 both select 0. */
    if (nSavedMode >= 1 && nSavedMode <= 3) {
        g_brB4E1D4 = g_aBrB4DF30[nSavedMode];
    } else {
        g_brB4E1D4 = g_aBrB4DF30[0];
    }

    g_brB4E1E0 = nSavedExcl;
}

/* ── Ghidra-matched functions ─────────────────────────── */
#ifdef BR_MATCHING_BUILD
extern int DAT_118ef178;
__declspec(dllimport) int __stdcall DisableThreadLibraryCalls(void *hModule);
int func_0x10074aec();
void halt_baddata(void);
/* WHAT IT DOES: no-op stub (CRT-region placeholder). */
/* @implements 0x10073714 glide BrNop73714 */

int BrNop73714(void)

{
  return;
}

/* WHAT IT DOES: halt on bad data (CRT-region trap). */
/* @implements 0x10073974 glide BrHalt73974 */

int BrHalt73974(void)

{
                    
  halt_baddata();
}

/* WHAT IT DOES: halt on bad data (CRT-region trap, second entry point). */
/* @implements 0x10073979 glide BrHalt73979 */

int BrHalt73979(void)

{
                    
  halt_baddata();
}

/* WHAT IT DOES: validate a matrix magic number and call the rebuild path if it matches. */
/* @implements 0x100747E0 glide BrMat3CheckMagic */

int BrMat3CheckMagic(int *param_1)

{
  if (*(int *)*param_1 == -0x1f928c9d) {
    func_0x10074aec();
  }
  return 0;
}

/* WHAT IT DOES: the DLL entry point: on DLL_PROCESS_ATTACH, unless the flag at 0x118EF178
 * is set, disable thread attach/detach notifications. Always returns TRUE. */
/* @implements 0x10074B00 glide BrDllMain */

int __stdcall BrDllMain(void *param_1,int param_2,int _pad_2)
{
  if ((param_2 == 1) && (DAT_118ef178 == 0)) {
    DisableThreadLibraryCalls(param_1);
  }
  return 1;
}

#endif /* BR_MATCHING_BUILD */
