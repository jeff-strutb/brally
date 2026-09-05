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
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import
 * table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include <stdint.h>

#ifdef BR_MATCHING_BUILD
#include <windows.h>

extern int DAT_117b3250;
extern int DAT_11849e60;
extern int DAT_1184c070;
extern int DAT_1184c074;
extern int g_aBr178FEF8;
extern int g_aBrPeer71;

/* WHAT IT DOES: the networking worker thread's wait loop: blocks until
 * either the quit event or a peer's mutex is signalled, exits the thread on
 * quit, and otherwise checks each peer's state and bails out of the scan as
 * soon as one is not ready. */
/* @implements 0x1006A650 glide FUN_1006a650 */
/* auto-filed from ghidra --refine; transforms: as-is */

void FUN_1006a650(void)
{
  DWORD wr;
  int *pPeer;
  int *pAlt;
  HANDLE h1[2];
  HANDLE h2[2];
  char skip;
  int st;
  int t;

  pPeer = &g_aBrPeer71;
  do {
    h1[0] = (HANDLE)DAT_11849e60;
    h1[1] = (HANDLE)*pPeer;
    wr = WaitForMultipleObjects(2, h1, 0, 0xffffffff);
    if (wr == 0) {
      ExitThread(0);
    }
    st = pPeer[0xb] & 0x3f;
    if (st < 2 || st == 3) {
      skip = 0;
    }
    else {
      skip = 1;
    }
    ReleaseMutex((HANDLE)*pPeer);
    if (skip) {
      return;
    }
    pPeer = pPeer + 0x25b;
  } while ((int)pPeer < 0x117b3248);

  pAlt = &g_aBr178FEF8;
  pPeer = &g_aBrPeer71;
  for (;;) {
    h1[0] = (HANDLE)DAT_11849e60;
    h1[1] = (HANDLE)*pPeer;
    wr = WaitForMultipleObjects(2, h1, 0, 0xffffffff);
    if (wr == 0) {
      ExitThread(0);
    }
    st = pPeer[0xb];
    skip = ((st & 0x3f) == 3);
    ReleaseMutex((HANDLE)*pPeer);
    if (skip) {
      h2[0] = (HANDLE)DAT_11849e60;
      h2[1] = (HANDLE)*pAlt;
      wr = WaitForMultipleObjects(2, h2, 0, 0xffffffff);
      if (wr == 0) {
        ExitThread(0);
      }
      st = pAlt[0xb];
      skip = ((st & 0x3f) != 3);
      ReleaseMutex((HANDLE)*pAlt);
      if (skip) {
        return;
      }
    }
    pPeer = pPeer + 0x25b;
    pAlt = pAlt + 0x280b;
    if ((int)pPeer >= 0x117b3248) {
      pPeer = &g_aBrPeer71;
      t = 4;
      do {
        h2[0] = (HANDLE)DAT_11849e60;
        h2[1] = (HANDLE)*pPeer;
        wr = WaitForMultipleObjects(2, h2, 0, 0xffffffff);
        if (wr == 0) {
          ExitThread(0);
        }
        if ((pPeer[0xb] & 0x3f) == 3) {
          pPeer[0xb] = t;
          DAT_117b3250 = 1;
          DAT_1184c074 = DAT_1184c070 + 3000;
        }
        ReleaseMutex((HANDLE)*pPeer);
        pPeer = pPeer + 0x25b;
      } while ((int)pPeer < 0x117b3248);
      return;
    }
  }
}



extern int DAT_117b324c;
int FUN_1006a330();
extern int g_178FEE8;

/* WHAT IT DOES: create Win32 mutexes for each network peer and its sub-channels. */
/* @implements 0x1006A4D0 glide BrNetPeerMutexInit */

int BrNetPeerMutexInit(void)

{
  HANDLE pvVar1;
  int iVar2;
  int *puVar3;
  int iVar4;
  
  g_178FEE8 = BrSub10075020();
  DAT_117b324c = BrDelta_100713A0();
  iVar2 = 0;
  do {
    pvVar1 = CreateMutexA((LPSECURITY_ATTRIBUTES)0x0,0,(LPCSTR)0x0);
    *(HANDLE *)((int)&g_aBrPeer71 + iVar2) = pvVar1;
    puVar3 = (int *)((int)&g_aBr178FEF8 + iVar2);
    iVar4 = 0x10;
    do {
      pvVar1 = CreateMutexA((LPSECURITY_ATTRIBUTES)0x0,0,(LPCSTR)0x0);
      *puVar3 = pvVar1;
      puVar3 = puVar3 + 0x25b0;
      iVar4 = iVar4 + -1;
    } while (iVar4 != 0);
    iVar2 = iVar2 + 0x96c;
  } while (iVar2 < 0x96c0);
  FUN_1006a330();
  return;
}

extern int DAT_117a9bb4;

/* WHAT IT DOES: under each peer's mutex, clear the pending message slot whose
 * id matches while its state (low 6 bits) is still early (< 5). */
/* @implements 0x1006A3F0 glide BrNetPeerMsgCancel */

int BrNetPeerMsgCancel(int id)

{
  int *puVar1;

  puVar1 = &DAT_117a9bb4;
  do {
    WaitForSingleObject((HANDLE)puVar1[-0xb],0xffffffff);
    /* mov ecx,[esi]; and ecx,0x3f; cmp cl,5; jge -- a plain INT compare;
     * the and proves the high bits zero, so VC5 narrows just the cmp. */
    if ((puVar1[-10] == id) && ((*puVar1 & 0x3f) < 5)) {
      *puVar1 = 0;
    }
    ReleaseMutex((HANDLE)puVar1[-0xb]);
    puVar1 = puVar1 + 0x25b;
  } while ((int)puVar1 < 0x117b3274);
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
extern void BrSub10071560(void);
extern void BrSub10071630(void);

/* WHAT IT DOES: run the two-step networking start-up in order and report
 * success. A sequencing wrapper, nothing more. */
/* @implements 0x1006A4C0 glide BrSub10071550 */
int32_t BrSub10071550(void)
{
    BrSub10071560();
    BrSub10071630();
    return 1;
}

/* ==========================================================================
 * 0x1006AAF0
 * ========================================================================== */

/* 0x11849F30: sixteen outgoing message streams, 0x214 bytes apart, one per
 * peer; the loop ends at the address just past the last (0x1184C070). */
extern int g_1826BD0;
struct BrBitStream;
void BrObjResetMsgHdr(struct BrBitStream *pBs);    /* 0x1006AB60 */

/* WHAT IT DOES: the once-a-second reset of every peer's outgoing message:
 * for each of the sixteen peers it waits for that peer's mutex (or the
 * subsystem's quit event, on which the worker thread exits), empties the
 * peer's message stream and writes the fresh header, then hands the mutex
 * back.
 *
 * The wait result is tested INLINE, not through a named local.  With a
 * `DWORD wr` the two hoisted import pointers come out swapped (ebx/ebp,
 * 2 diff bytes, everything else identical): the dead-after-test local is
 * still an allocation candidate and shifts the tie-break between them.
 * FUN_1006a650 above keeps its `wr` because that is what matches THERE;
 * the polarity is per-function -- see docs/VC5-IDIOMS.md. */
/* @implements 0x1006AAF0 glide BrNetPeerMsgReset */
void BrNetPeerMsgReset(void)
{
  int *pPeer;
  unsigned char *pMsg;
  HANDLE h[2];

  pPeer = &g_aBrPeer71;
  pMsg = (unsigned char *)&g_1826BD0;
  do {
    h[0] = (HANDLE)DAT_11849e60;
    h[1] = (HANDLE)*pPeer;
    if (WaitForMultipleObjects(2, h, 0, 0xffffffff) == 0) {
      ExitThread(0);
    }
    BrObjResetMsgHdr((struct BrBitStream *)pMsg);
    ReleaseMutex((HANDLE)*pPeer);
    pMsg = pMsg + 0x214;
    pPeer = pPeer + 0x25b;
  } while ((int)pMsg < 0x1184c070);
}

#endif /* BR_MATCHING_BUILD */
