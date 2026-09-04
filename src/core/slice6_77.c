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

/* WHAT IT DOES: no-op stub (CRT-region placeholder). */
/* @implements 0x10073719 glide BrNop73719 */

int BrNop73719(void)

{
  return;
}

/* The per-DLL CRT exit-handler glue every /MD DLL carries: an _onexit that
 * routes to the module's own table via __dllonexit (a LOCAL thunk at
 * 0x10074AE0, so the call is E8) or to MSVCRT's _onexit (FF 15 import). */
extern int DAT_118ef17c;
extern int DAT_118ef180;
typedef int (__cdecl *BrOnExitFn)(void);
__declspec(dllimport) BrOnExitFn __cdecl _onexit(BrOnExitFn pfn);
BrOnExitFn __cdecl __dllonexit(BrOnExitFn pfn, int *ppEnd, int *ppStart);

/* WHAT IT DOES: register a function to run at exit. Picks between the
 * executable's exit list and the DLL's own, depending on whether this module
 * has its own list -- the CRT's atexit machinery, not game code. BrCrtAtExit
 * is the thin wrapper over it. */
/* @implements 0x100745B0 glide BrCrtOnExit */

BrOnExitFn BrCrtOnExit(BrOnExitFn pfn)

{
  if (DAT_118ef180 == -1) {
    return _onexit(pfn);
  }
  return __dllonexit(pfn,&DAT_118ef180,&DAT_118ef17c);
}

/* WHAT IT DOES: the CRT's atexit -- 0x100745B0 wrapped, returning 0 or -1. */
/* @implements 0x100745E0 glide BrCrtAtExit */

int BrCrtAtExit(BrOnExitFn pfn)

{
  BrOnExitFn p;

  p = BrCrtOnExit(pfn);
  /* neg;sbb;neg;dec -- the ternary's branchless codegen, not setne. */
  return (p != 0) ? 0 : -1;
}

#endif /* BR_MATCHING_BUILD */
