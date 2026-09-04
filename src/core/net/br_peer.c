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

#endif /* BR_MATCHING_BUILD */
