/* br_dllentry.c -- startup: the DLL entry point and the CRT exit glue.
 *
 * Filed out of the address batch slice6_77.c.  This is the module's own
 * bring-up and take-down machinery -- DllMain, the per-DLL atexit list and
 * the array ctor/dtor helpers -- rather than game code, but it is in the
 * image and it matches, so it lives here.
 *
 * Four "functions" that used to be tagged here (0x10073714, 0x10073719,
 * 0x10073974, 0x10073979) were bytes of the 256-record data table at
 * 0x10072AE0..0x10073AE0 that happen to decode as `ret` / `call rel32`.  They
 * matched only because the image gate filled the call's displacement from
 * the original; they are fenced as data_table in config/fenced.csv now.
 */
#include <excpt.h>   /* GetExceptionInformation, for the array unwinders */

/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared by the platform headers */
/* func_0x10074aec: prototype in br_funcs.h */
/* WHAT IT DOES: the exception filter the array unwinder runs if a destructor
 * throws while an exception is already unwinding: a C++ exception (code
 * 0xE06D7363, "msc") there means terminate() (0x10074AEC); anything else
 * returns EXCEPTION_CONTINUE_SEARCH.  The CRT's __ArrayUnwindFilter. */
/* @implements 0x100747E0 glide BrEhArrayUnwindFilter */
int BrEhArrayUnwindFilter(int *param_1)
{
  if (*(int *)*param_1 == -0x1f928c9d) {
    abort();
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

/* The per-DLL CRT exit-handler glue every /MD DLL carries: an _onexit that
 * routes to the module's own table via __dllonexit (a LOCAL thunk at
 * 0x10074AE0, so the call is E8) or to MSVCRT's _onexit (FF 15 import). */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
typedef int (__cdecl *BrOnExitFn)(void);
__declspec(dllimport) BrOnExitFn __cdecl _onexit(BrOnExitFn pfn);
/* __dllonexit: prototype in br_funcs.h */

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
  return dllonexit(pfn,&DAT_118ef180,&DAT_118ef17c);
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



typedef void (__fastcall *PDtor)(void *);
/* BrEhArrayUnwind: prototype in br_funcs.h */

/* WHAT IT DOES: destroy an array of objects, BACK to front, calling each
 * element's destructor. If one throws, the compiler's cleanup helper
 * finishes destroying the rest. Compiler-generated array teardown, not game
 * code. */
/* @implements 0x100746C0 glide BrEhVecDtor */
void __stdcall
BrEhVecDtor(void *ptr, unsigned size, int count, PDtor dtor)
{
    int success = 0;

    ptr = (char *)ptr + size * count;
    __try {
        while (--count >= 0) {
            ptr = (char *)ptr - size;
            dtor(ptr);
        }
        success = 1;
    } __finally {
        if (!success)
            BrEhArrayUnwind(ptr, size, count, dtor);
    }
}


typedef void (__fastcall *PDtor)(void *);

/* WHAT IT DOES: destroy the remaining elements of an array while an
 * exception is already unwinding, swallowing any further exception --
 * because throwing during cleanup would terminate. The partner of the array
 * destructor above. */
/* @implements 0x10074770 glide BrEhArrayUnwind */
void __stdcall
BrEhArrayUnwind(void *ptr, unsigned size, int count, PDtor dtor)
{
    __try {
        for (;;) {
            --count;
            if (count < 0)
                break;
            dtor(ptr = (char *)ptr - size);
        }
    } __except (BrEhArrayUnwindFilter((int *)GetExceptionInformation())) {
    }
}


typedef void (__fastcall *PDtor)(void *);
typedef void (__fastcall *PCtor)(void *);
/* BrEhArrayUnwind: prototype in br_funcs.h */

/* WHAT IT DOES: construct an array of objects front to back and, if any
 * constructor throws, destroy the ones already built before letting the
 * exception out. Compiler-generated array construction. */
/* @implements 0x10074800 glide BrEhVecCtor */
void __stdcall
BrEhVecCtor(void *ptr, unsigned size, int count, PCtor ctor, PDtor dtor)
{
    int success = 0;
    int i;

    __try {
        for (i = 0; i < count; i++) {
            ctor(ptr);
            ptr = (char *)ptr + size;
        }
        success = 1;
    } __finally {
        if (!success)
            BrEhArrayUnwind(ptr, size, i, dtor);
    }
}
