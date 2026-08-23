/* br_musiccmd.c -- audio.  See br_musiccmd.h. */
#include "br_musiccmd.h"

#include <stdint.h>

#ifdef BR_MATCHING_BUILD
extern uint32_t g_0940A4;
void BrExt_10002660(void *);
void BrExt_100025F0(void *);
void BrExt_10072B30(void *, int, int);
void BrExt_10072A90(void *, int, int, int);
void BrExt_10002660(void *p) { (void)p; }
void BrExt_100025F0(void *p) { (void)p; }
void BrExt_10072B30(void *a, int b, int c) { (void)a; (void)b; (void)c; }
void BrExt_10072A90(void *a, int b, int c, int d)
{ (void)a; (void)b; (void)c; (void)d; }
#else
uint32_t g_0940A4;
void BrExt_10002660(void *p);
void BrExt_100025F0(void *p);
void BrExt_10072B30(void *a, int b, int c);
void BrExt_10072A90(void *a, int b, int c, int d);
#endif

/* WHAT IT DOES: send one command to the live music path: Windows CD
 * audio if that mode is on, otherwise the in-process EAR mixer. */
/* @implements 0x100025C0 d3d BrDispatch_100025C0 */
void BrDispatch_100025C0(void *p)
{
    if (g_0940A4 == 1)
        BrExt_10002660(p);
    else
        BrExt_100025F0(p);
}

/* WHAT IT DOES: write a value into a sound table, packing the row index
 * as 2*index. */
/* @implements 0x10072B80 d3d BrWrap_10072B80 */
void BrWrap_10072B80(void *a, int b, int c)
{
    BrExt_10072B30(a, b + b, c);
}

/* WHAT IT DOES: the same table write, with an extra "1" meaning in use. */
/* @implements 0x10072B10 d3d BrWrap_10072B10 */
void BrWrap_10072B10(void *a, int b, int c)
{
    BrExt_10072A90(a, b + b, c, 1);
}

/* WHAT IT DOES: the same table write, with the packed index forced to 1. */
/* @implements 0x10072A70 d3d BrWrap_10072A70 */
void BrWrap_10072A70(void *a, int b, int c)
{
    BrExt_10072A90(a, 1, b, c);
}

/* ── Ghidra-matched functions ─────────────────────────── */
#ifdef BR_MATCHING_BUILD
#include <windows.h>
extern int BrSndG0B5DE8;
extern int BrSndG18290FC;
extern int BrSndPDS;
extern int DAT_100b55f8;
int FUN_1006b490();
int FUN_1006b4c0();
int FUN_1006b790();
int FUN_1006b950();
int FUN_1006b970();

/* WHAT IT DOES: set the pan value on a DirectSound buffer and commit the change. */
/* @implements 0x1006B950 glide BrSndBufSetPan */

int BrSndBufSetPan(int param_1,int param_2)

{
  *(int *)(param_1 + 0x18) = param_2;
  FUN_1006b970(param_1);
  return;
}

/* WHAT IT DOES: set frequency and pan on a voice within a bank, checking that DirectSound is ready. */
/* @implements 0x1006BA00 glide BrSndVoiceConfigure */

int BrSndVoiceConfigure(int param_1,int param_2,int param_3,int param_4)

{
  int uVar1;
  int iVar2;
  
  if (((BrSndG0B5DE8 != 0) && (BrSndPDS != 0)) && (BrSndG18290FC != 0)) {
    uVar1 = (&DAT_100b55f8)[param_2 + param_1 * 0x12];
    iVar2 = FUN_1006b790(uVar1,param_3);
    if ((iVar2 != 0) && (iVar2 = FUN_1006b950(uVar1,param_4), iVar2 == 0)) {
      return 1;
    }
    return 0;
  }
  return 1;
}

/* WHAT IT DOES: walk the linked list of active sound buffers and stop each one. */
/* @implements 0x1006BB60 glide BrSndBufStopAll */

int BrSndBufStopAll(int param_1)

{
  int iVar1;
  int *piVar2;
  
  piVar2 = (int *)(param_1 + 0x1a8);
  iVar1 = *(int *)(param_1 + 0x1a8);
  while (iVar1 != 0) {
    FUN_1006b4c0(iVar1);
    piVar2 = (int *)(*piVar2 + 0x1a8);
    iVar1 = *piVar2;
  }
  return 0;
}

/* WHAT IT DOES: walk the linked list of active sound buffers, stop each one, and free its GlobalAlloc memory. */
/* @implements 0x1006BB90 glide BrSndBufFreeAll */

int BrSndBufFreeAll(int param_1)

{
  int *pMem;
  int *puVar1;
  HGLOBAL pvVar2;
  
  pMem = *(int **)(param_1 + 0x1a8);
  *(int *)(param_1 + 0x1a8) = 0;
  while (pMem != (int *)0x0) {
    FUN_1006b490(pMem);
    pvVar2 = GlobalHandle((LPCVOID)pMem[2]);
    GlobalUnlock(pvVar2);
    pvVar2 = GlobalHandle((LPCVOID)pMem[2]);
    GlobalFree(pvVar2);
    pvVar2 = GlobalHandle((LPCVOID)*pMem);
    GlobalUnlock(pvVar2);
    pvVar2 = GlobalHandle((LPCVOID)*pMem);
    GlobalFree(pvVar2);
    puVar1 = (int *)pMem[0x6a];
    pvVar2 = GlobalHandle(pMem);
    GlobalUnlock(pvVar2);
    pvVar2 = GlobalHandle(pMem);
    GlobalFree(pvVar2);
    pMem = puVar1;
  }
  return 0;
}

#endif /* BR_MATCHING_BUILD */
