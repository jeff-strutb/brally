/* br_musiccmd.c -- audio.  See br_musiccmd.h. */
#include "slice1_08.h"   /* br_globals: its objects */
#include "br_musiccmd.h"

#include <stdint.h>

/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* BrExt_10002660: prototype in br_funcs.h */
/* BrExt_100025F0: prototype in br_funcs.h */
/* BrExt_10072B30: prototype in br_funcs.h */
/* BrExt_10072A90: prototype in br_funcs.h */
/* BrExt_10002660: the original function is FUN_10002980 */
/* BrExt_100025F0: the original function is BrCdStartup */
/* @n64 0x8021CD24 located */
/* BrExt_10072B30: the original function is FUN_1006baa0 */
/* @n64 0x802607DC located */
/* BrExt_10072A90: the original function is BrSndVoiceConfigure */

/* WHAT IT DOES: send one command to the live music path: Windows CD
 * audio if that mode is on, otherwise the in-process EAR mixer. */
/* @implements 0x100025C0 d3d BrDispatch_100025C0 */
void BrDispatch_100025C0(void *p)
{
    if ((*(uint32_t *)&DAT_1007b074) == 1)
        FUN_10002980(p);
    else
        BrCdStartup(p);
}

/* WHAT IT DOES: write a value into a sound table, packing the row index
 * as 2*index. */
/* @implements 0x10072B80 d3d BrWrap_10072B80 */
/* @n64 0x80242880 located */
void BrWrap_10072B80(int a, int b, int c)
{
    FUN_1006baa0(a, b + b, c);
}

/* WHAT IT DOES: the same table write, with an extra "1" meaning in use. */
/* @implements 0x10072B10 d3d BrWrap_10072B10 */
void BrWrap_10072B10(int a, int b, int c)
{
    BrSndVoiceConfigure(a, b + b, c, 1);
}

/* WHAT IT DOES: the same table write, with the packed index forced to 1. */
/* @implements 0x10072A70 d3d BrWrap_10072A70 */
/* @n64 0x80240240 located */
void BrWrap_10072A70(int a, int b, int c)
{
    BrSndVoiceConfigure(a, 1, b, c);
}

/* -- Ghidra-matched functions --------------------------- */
#include <windows.h>
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* FUN_1006b490: prototype in br_funcs.h */
/* FUN_1006b4c0: prototype in br_funcs.h */
/* FUN_1006b790: prototype in br_funcs.h */
/* FUN_1006b950: prototype in br_funcs.h */
/* FUN_1006b970: prototype in br_funcs.h */

/* WHAT IT DOES: set the pan value on a DirectSound buffer and commit the change. */
/* @implements 0x1006B950 glide BrSndBufSetPan */

int BrSndBufSetPan(BrSndVoice *param_1,int param_2)

{
  param_1->f18 = param_2;
  return BrSndVoiceBufStart(param_1);   /* the original returns the callee's EAX */
}

/* WHAT IT DOES: set frequency and pan on a voice within a bank, checking that DirectSound is ready. */
/* @implements 0x1006BA00 glide BrSndVoiceConfigure */

int BrSndVoiceConfigure(int param_1,int param_2,int param_3,int param_4)

{
  BrSndVoice *uVar1;
  int iVar2;
  
  if ((((*(int *)&DAT_100b51e4[1036]) != 0) && ((((intptr_t)(BrSndPDS))) != 0)) && (BrSndG18290FC != 0)) {
    uVar1 = g_aBrSndRow[param_1].aSlot[param_2];
    iVar2 = BrSndVoiceSetLR(uVar1,param_3);
    if ((iVar2 != 0) && (iVar2 = BrSndBufSetPan(uVar1,param_4), iVar2 == 0)) {
      return 1;
    }
    return 0;
  }
  return 1;
}

/* WHAT IT DOES: walk the linked list of active sound buffers and stop each one. */
/* @implements 0x1006BB60 glide BrSndBufStopAll */

int BrSndBufStopAll(BrSndVoice *param_1)

{
  BrSndVoice *v;

  for (v = param_1->pNext; v != 0; v = v->pNext)
    BrSndVoiceBufStop(v);
  return 0;
}

/* WHAT IT DOES: walk the linked list of active sound buffers, stop each one, and free its GlobalAlloc memory. */
/* @implements 0x1006BB90 glide BrSndBufFreeAll */

int BrSndBufFreeAll(BrSndVoice *param_1)

{
  BrSndVoice *pMem;
  BrSndVoice *pNext;
  HGLOBAL pvVar2;

  pMem = param_1->pNext;
  param_1->pNext = 0;
  while (pMem != 0) {
    BrSndVoiceBufRelease(pMem);
    pvVar2 = GlobalHandle(pMem->pFormat);
    GlobalUnlock(pvVar2);
    pvVar2 = GlobalHandle(pMem->pFormat);
    GlobalFree(pvVar2);
    pvVar2 = GlobalHandle(pMem->pData);
    GlobalUnlock(pvVar2);
    pvVar2 = GlobalHandle(pMem->pData);
    GlobalFree(pvVar2);
    pNext = pMem->pNext;
    pvVar2 = GlobalHandle(pMem);
    GlobalUnlock(pvVar2);
    pvVar2 = GlobalHandle(pMem);
    GlobalFree(pvVar2);
    pMem = pNext;
  }
  return 0;
}

/* BrSndPlayGroup: prototype in br_funcs.h */

/* ==========================================================================
 * 2. Sound
 * ========================================================================== */

/* 0x10072AF0 */
/* WHAT IT DOES: plays a sound effect from one of the game's sound banks, in
 * the simple case where the caller does not care about the result. A second
 * name for a routine whose body lives in the sound module. */
/* @implements 0x10072AF0 d3d BrSub10072AF0 */
void BrSub10072AF0(int a, int b)
{
    /* BrSndPlayGroup(a, b, 0) == BrSndPlayEx(a, 1, b, 0). The two callers
     * discard the result. */
    (void)BrWrap_10072A70((int32_t)a, (uint32_t)b, 0);
}

