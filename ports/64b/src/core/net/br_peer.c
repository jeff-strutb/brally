/* br_peer.c -- net.
 *
 * The peer table and the worker thread that walks it: each peer record is
 * guarded by its own mutex, and the thread waits on that mutex and the
 * subsystem's quit event together.
 *
 * Filed out of the address batches: these functions were
 * matched first and grouped by what they are afterwards.
 * Every function carries its original address.
 */
/* The original is /MD: CRT calls go through the import
 * table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include "br_coretypes.h"   /* br_globals: its objects */
#include "slice1_09.h"   /* br_globals: its objects */
#include <stdint.h>

#include <windows.h>

/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* WHAT IT DOES: the networking worker thread's wait loop: blocks until
 * either the quit event or a peer's mutex is signalled, exits the thread on
 * quit, and otherwise checks each peer's state and bails out of the scan as
 * soon as one is not ready. */
/* @implements 0x1006A650 glide FUN_1006a650 */
/* auto-filed from ghidra --refine; transforms: as-is */

void FUN_1006a650(void)
{
  DWORD wr;
  BrPeerRec *pPeer;
  BrPeerRec *pAlt;
  HANDLE h1[2];
  HANDLE h2[2];
  char skip;
  int st;
  int t;
  int i;

  for (i = 0; i < 16; i++) {
    pPeer = &(*(int *)&g_aBrPeer71)[i];
    h1[0] = (HANDLE)g_hBrSndWake86;
    h1[1] = pPeer->hMutex;
    wr = WaitForMultipleObjects(2, h1, 0, 0xffffffff);
    if (wr == 0) {
      ExitThread(0);
    }
    st = pPeer->f02C & 0x3f;
    if (st < 2 || st == 3) {
      skip = 0;
    }
    else {
      skip = 1;
    }
    ReleaseMutex(pPeer->hMutex);
    if (skip) {
      return;
    }
  }

  /* each peer, and the diagonal of the history grid beside it */
  for (i = 0; ; i++) {
    pPeer = &(*(int *)&g_aBrPeer71)[i];
    pAlt = &(*(int *)&g_aBr178FEF8)[i][i];
    h1[0] = (HANDLE)g_hBrSndWake86;
    h1[1] = pPeer->hMutex;
    wr = WaitForMultipleObjects(2, h1, 0, 0xffffffff);
    if (wr == 0) {
      ExitThread(0);
    }
    st = pPeer->f02C;
    skip = ((st & 0x3f) == 3);
    ReleaseMutex(pPeer->hMutex);
    if (skip) {
      h2[0] = (HANDLE)g_hBrSndWake86;
      h2[1] = pAlt->hMutex;
      wr = WaitForMultipleObjects(2, h2, 0, 0xffffffff);
      if (wr == 0) {
        ExitThread(0);
      }
      st = pAlt->f02C;
      skip = ((st & 0x3f) != 3);
      ReleaseMutex(pAlt->hMutex);
      if (skip) {
        return;
      }
    }
    if (i + 1 >= 16) {
      t = 4;
      for (i = 0; i < 16; i++) {
        pPeer = &(*(int *)&g_aBrPeer71)[i];
        h2[0] = (HANDLE)g_hBrSndWake86;
        h2[1] = pPeer->hMutex;
        wr = WaitForMultipleObjects(2, h2, 0, 0xffffffff);
        if (wr == 0) {
          ExitThread(0);
        }
        if ((pPeer->f02C & 0x3f) == 3) {
          pPeer->f02C = t;
          DAT_117b3250 = 1;
          (*(int *)&DAT_1184c074) = (*(int *)&DAT_1184c070) + 3000;
        }
        ReleaseMutex(pPeer->hMutex);
      }
      return;
    }
  }
}



/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* FUN_1006a330: prototype in br_funcs.h */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* WHAT IT DOES: create Win32 mutexes for each network peer and its sub-channels. */
/* @implements 0x1006A4D0 glide BrNetPeerMutexInit */

int BrNetPeerMutexInit(void)

{
  int i;
  int j;

  g_178FEE8 = BrSub10075020();
  DAT_117b324c = BrDelta_100713A0();
  for (i = 0; i < 16; i++) {
    (*(int *)&g_aBrPeer71)[i].hMutex = CreateMutexA((LPSECURITY_ATTRIBUTES)0x0,0,(LPCSTR)0x0);
    for (j = 0; j < 16; j++) {
      (*(int *)&g_aBr178FEF8)[j][i].hMutex = CreateMutexA((LPSECURITY_ATTRIBUTES)0x0,0,(LPCSTR)0x0);
    }
  }
  FUN_1006a330();
  return;
}

/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* WHAT IT DOES: under each peer's mutex, clear the pending message slot whose
 * id matches while its state (low 6 bits) is still early (< 5). */
/* @implements 0x1006A3F0 glide BrNetPeerMsgCancel */

int BrNetPeerMsgCancel(int id)

{
  int i;

  for (i = 0; i < 16; i++) {
    WaitForSingleObject((*(int *)&g_aBrPeer71)[i].hMutex,0xffffffff);
    if (((*(int *)&g_aBrPeer71)[i].f004 == id) && (((*(int *)&g_aBrPeer71)[i].f02C & 0x3f) < 5)) {
      (*(int *)&g_aBrPeer71)[i].f02C = 0;
    }
    ReleaseMutex((*(int *)&g_aBrPeer71)[i].hMutex);
  }
  return;
}

/* ==========================================================================
 * 0x10071550
 * ========================================================================== */

/* WHAT IT DOES: runs two other routines in order and reports success
 * unconditionally. What the two do was not established, so the purpose is
 * unclear; the caller ignores the answer in any case.
 *
 * The original is four instructions -- two `call rel32`, `mov eax, 1`, `ret`.
 * The two callees are DIRECT calls to 0x10071560 and 0x10071630, not indirect
 * calls through pointer slots, and there is no null test on either. */
/* BrSub10071560: prototype in br_funcs.h */
/* BrSub10071630: prototype in br_funcs.h */

/* WHAT IT DOES: run the two-step networking start-up in order and report
 * success. A sequencing wrapper, nothing more. */
/* @implements 0x1006A4C0 glide BrSub10071550 */
int32_t BrSub10071550(void)
{
    BrNetPeerMutexInit();
    BrSecondTickStart();
    return 1;
}

/* ==========================================================================
 * 0x1006AAF0
 * ========================================================================== */

/* 0x11849F30: sixteen outgoing message streams, 0x214 bytes apart, one per
 * peer; the loop ends at the address just past the last (0x1184C070). */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
struct BrBitStream;
/* BrObjResetMsgHdr: prototype in br_funcs.h */

/* WHAT IT DOES: the once-a-second reset of every peer's outgoing message:
 * for each of the sixteen peers it waits for that peer's mutex (or the
 * subsystem's quit event, on which the worker thread exits), empties the
 * peer's message stream and writes the fresh header, then hands the mutex
 * back. */
/* @implements 0x1006AAF0 glide BrNetPeerMsgReset */
void BrNetPeerMsgReset(void)
{
  int i;
  HANDLE h[2];

  for (i = 0; i < 16; i++) {
    h[0] = (HANDLE)g_hBrSndWake86;
    h[1] = (*(int *)&g_aBrPeer71)[i].hMutex;
    if (WaitForMultipleObjects(2, h, 0, 0xffffffff) == 0) {
      ExitThread(0);
    }
    BrObjResetMsgHdr(&g_aBrPeerMsg[i].bs);
    ReleaseMutex((*(int *)&g_aBrPeer71)[i].hMutex);
  }
}

