/* Auto-generated from Ghidra decompilation — 0x10036740 */
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
typedef int (__stdcall *COM3)(void *this, void *buf, unsigned int *size);

/* @implements 0x10036740 glide FUN_10036740 */
int FUN_10036740(void *param_1, void **param_2)

{
  COM3 fn;
  int hr;
  HGLOBAL hMem;
  void *pMem;
  unsigned int size;
  
  pMem = 0;
  fn = *(COM3 *)(*(int *)param_1 + 0x58);
  hr = fn(param_1, 0, &size);
  if (hr == (int)0x8877001e) {
    hMem = GlobalAlloc(0x42, size);
    pMem = GlobalLock(hMem);
    if (pMem == 0) {
      hr = (int)0x8007000e;
    }
    else {
      hr = fn(param_1, pMem, &size);
      if (hr >= 0) {
        *param_2 = pMem;
        pMem = 0;
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
