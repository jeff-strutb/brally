/* br_dllentry.c -- startup: the DLL entry point and the CRT exit glue.
 *
 * Filed out of the address batch slice6_77.c.  This is the module's own
 * bring-up and take-down machinery -- DllMain, the per-DLL atexit list, and
 * the CRT-region stubs and traps that sit beside them -- rather than game
 * code, but it is in the image and it matches, so it lives here.
 */
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
