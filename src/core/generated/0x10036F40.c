/* Auto-generated from Ghidra decompilation — 0x10036F40 */
#ifdef BR_MATCHING_BUILD

/* The original binary is /MD: CRT calls resolve through the import table. */
#define _CRTIMP __declspec(dllimport)
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <mmsystem.h>

#ifndef true
#define true 1
#define false 0
#endif
#ifndef NAN
unsigned long _ghidra_nan_bits = 0x7FC00000;
#define NAN (*(float*)&_ghidra_nan_bits)
#endif

typedef int (*funcptr)();

/* Forward declarations for unknown functions/globals */
extern int *g_brP277B40;
int BrSub1003D850();



typedef int (__stdcall *COM4)(void *this, int a, void *b, int *c);
typedef int (__stdcall *COM5)(void *this, void *cb, void *buf, int n, int cookie);

/* WHAT IT DOES: the same ask-then-allocate-then-ask-again dance for a
 * different DirectPlay method -- one that takes an extra argument and
 * returns a variable-sized result. */
/* @implements 0x10036F40 glide FUN_10036f40 */
int FUN_10036f40(int param_1, void *param_2)

{
  int hr;
  HGLOBAL hMem;
  void *pMem;
  int size;
  void *pObj;
  int *vt;
  
  pObj = g_brP277B40;
  vt = *(int **)pObj;
  pMem = 0;
  size = 0;
  hr = (*(COM4 *)((char *)vt + 0x48))(pObj, 0, 0, &size);
  if (hr == (int)0x8877001e) {
    hMem = GlobalAlloc(0x42, (unsigned int)size);
    pMem = GlobalLock(hMem);
    if (pMem == 0) {
      hr = (int)0x8007000e;
    }
    else {
      pObj = g_brP277B40;
      vt = *(int **)pObj;
      hr = (*(COM4 *)((char *)vt + 0x48))(pObj, 0, pMem, &size);
      if (hr >= 0) {
        hr = (*(COM5 *)(*(int *)param_2 + 0x14))(param_2, (void *)BrSub1003D850, pMem, size, param_1);
      }
    }
  }
  if (pMem != 0) {
    hMem = GlobalHandle(pMem);
    GlobalUnlock(hMem);
    hMem = GlobalHandle(pMem);
    GlobalFree(hMem);
  }
  return hr;
}


#endif /* BR_MATCHING_BUILD */
