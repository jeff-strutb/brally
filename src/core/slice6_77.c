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
#ifdef BR_MATCHING_BUILD
/* The restore chain is a real switch: each arm stores its record ADDRESS
 * as an immediate and restores the exclusive flag itself (arms in memory
 * order default,3,2,1; case 1 restores the flag before the pointer). */
extern void BrExt_10079550(void);   /* glide 0x10072840, 0 args */

void BrFfbReprobe(void)
{
    int32_t nSavedMode = g_brB4E1D0;   /* esi */
    int32_t nSavedExcl = g_brB4E1E0;   /* edi */

    g_brB4E1D0 = 2;
    g_brB4E1D4 = g_aBrB4DF30[2];
    g_brB4E1E0 = 1;

    (void)BrFfbInit();
    BrExt_10079550();

    g_brB4E1D0 = nSavedMode;

    switch (nSavedMode) {
    default:
        g_brB4E1D4 = g_aBrB4DF30[0];
        g_brB4E1E0 = nSavedExcl;
        return;
    case 3:
        g_brB4E1D4 = g_aBrB4DF30[3];
        g_brB4E1E0 = nSavedExcl;
        return;
    case 2:
        g_brB4E1D4 = g_aBrB4DF30[2];
        g_brB4E1E0 = nSavedExcl;
        return;
    case 1:
        g_brB4E1E0 = nSavedExcl;
        g_brB4E1D4 = g_aBrB4DF30[1];
        return;
    }
}
#else
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
#endif

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


int FUN_1006c290();
extern int DAT_100b2f04;
extern int DAT_100b32b0;
extern int DAT_100b32bc;
extern int DAT_100b32c0;
extern int DAT_10af3bb0;
extern int DAT_10b71338;
extern int DAT_10b713e0;
extern int DAT_10b71488;
extern int DAT_10b71530;
extern int DAT_10b71534;
extern int g_BrCtrlCfg;
int BrFfbInit();
int BrSfxSrcPlaySilent();
int BrSndBankClear();
int BrSndBankSetCar();

/* WHAT IT DOES: bring force feedback up if the settings ask for it, and
 * record the outcome back into the setting so a failure downgrades it rather
 * than retrying every time. */
/* @implements 0x10061310 glide FUN_10061310 */
/* auto-filed from ghidra --refine; transforms: as-is */

void FUN_10061310(void)

{
  int *piVar1;
  int iVar2;
  int *puVar3;
  int v;
  
  if ((DAT_10b71530 == 1) || (DAT_10b71530 == 2)) {
    v = BrFfbInit();
    DAT_10b71530 = v;
    switch (v) {
    case 1:
      DAT_10b71534 = (int)&DAT_10b71338;
      break;
    case 2:
      DAT_10b71534 = (int)&DAT_10b713e0;
      break;
    case 3:
      DAT_10b71534 = (int)&DAT_10b71488;
      break;
    default:
      DAT_10b71534 = (int)&g_BrCtrlCfg;
      break;
    }
  }
  iVar2 = 0;
  piVar1 = &DAT_100b32b0;
  do {
    *piVar1 = iVar2;
    piVar1 = piVar1 + 6;
    iVar2 = iVar2 + 1;
  } while ((int)piVar1 < 0x100b3508);
  BrSndBankClear();
  iVar2 = 0;
  if (DAT_100b2f04 > 0) {
    puVar3 = &DAT_10af3bb0;
    do {
      BrSndBankSetCar(iVar2,*puVar3);
      iVar2 = iVar2 + 1;
      puVar3 = puVar3 + 0xada;
    } while (iVar2 < DAT_100b2f04);
  }
  FUN_1006c290(1);
  BrSfxSrcPlaySilent(0,DAT_100b32b0,DAT_100b32bc,DAT_100b32c0);
  if (DAT_100b2f04 > 1) {
    BrSfxSrcPlaySilent(2,DAT_100b32b0,DAT_100b32bc,DAT_100b32c0);
  }
  if (DAT_100b2f04 > 2) {
    BrSfxSrcPlaySilent(4,DAT_100b32b0,DAT_100b32bc,DAT_100b32c0);
  }
  return;
}


int FUN_1006b790(int, int);
extern int DAT_100b55f8[];
extern int BrSndG0B5DE8;
extern int BrSndG18290FC;
extern int BrSndPDS;

/* WHAT IT DOES: the same bank-and-slot lookup, setting a sound's panning
 * rather than its volume. */
/* @implements 0x1006BAA0 glide FUN_1006baa0 */
/* auto-filed from ghidra --refine; transforms: as-is */

int FUN_1006baa0(int param_1,int param_2,int param_3)

{
  if (((BrSndG0B5DE8 != 0) && (BrSndPDS != 0)) && (BrSndG18290FC != 0)) {
    return FUN_1006b790(DAT_100b55f8[param_2 + param_1 * 0x12], param_3) != 0;
  }
  return 1;
}


extern int DAT_100b55f8[];
extern int BrSndG0B5DE8;
extern int BrSndG18290FC;
extern int BrSndPDS;
int BrSndBufSetVolume(int, int);
int FUN_1006b6e0(int, int, int);

/* WHAT IT DOES: set a sound's volume from a float, for the banks whose slots
 * hold a PAIR of handles -- it doubles the slot index and truncates the level
 * to an integer before handing both to FUN_1006b6e0 below. The doubling is
 * the same stride FUN_1006bb10 uses; the single-handle callers reach
 * FUN_1006b6e0 directly. */
/* @implements 0x1006B6C0 glide BrSndSetVolumePairF */

int BrSndSetVolumePairF(int param_1,int param_2,float param_3)

{
  return FUN_1006b6e0(param_1,param_2 * 2,(int)param_3);
}

/* WHAT IT DOES: set the volume of one sound in a two-dimensional bank-and-
 * slot table. Reports success without doing anything when the sound system
 * is not running, so callers need no guard of their own -- the same shape as
 * its two siblings below. */
/* @implements 0x1006B6E0 glide FUN_1006b6e0 */
/* auto-filed from ghidra --refine; transforms: as-is */

int FUN_1006b6e0(int param_1,int param_2,int param_3)

{
  if (((BrSndG0B5DE8 != 0) && (BrSndPDS != 0)) && (BrSndG18290FC != 0)) {
    return BrSndBufSetVolume(DAT_100b55f8[param_2 + param_1 * 0x12], param_3) != 0;
  }
  return 1;
}


int FUN_1006b4c0(int);
extern int DAT_100b55f8[];
extern int BrSndG0B5DE8;
extern int BrSndG18290FC;
extern int BrSndPDS;

/* WHAT IT DOES: the same bank-and-slot lookup, stopping a sound. GOTCHA: its
 * table stride is DIFFERENT from its two siblings -- the second index is
 * doubled -- so this addresses a pair of handles per slot where they address
 * one. */
/* @implements 0x1006BB10 glide FUN_1006bb10 */
/* auto-filed from ghidra --refine; transforms: as-is */

int FUN_1006bb10(int param_1,int param_2)

{
  int iVar1;

  if ((((BrSndG0B5DE8 != 0) && (BrSndPDS != 0)) && (BrSndG18290FC != 0)) &&
     (iVar1 = DAT_100b55f8[param_1 * 0x12 + param_2 * 2], iVar1 != 0)) {
    return FUN_1006b4c0(iVar1) == 0;
  }
  return 1;
}

#endif /* BR_MATCHING_BUILD */
