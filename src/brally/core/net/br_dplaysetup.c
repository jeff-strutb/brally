/* br_dplaysetup.c -- net: the DirectPlay object and session lifecycle.
 *
 * Creating the DirectPlay object at boot, connecting it to the transport the
 * player picked, hosting and joining a session, the once-a-second housekeeping
 * timer, and the shutdown and final teardown -- plus the two ask-for-the-size-
 * then-fetch wrappers around DirectPlay calls that return variable-sized data
 * and the compound-address builder the connection step uses.  0x10035400..
 * 0x10036F40.  Matching build only: the port has no DirectPlay.
 *
 * Every function carries its original address.
 */
/* The original is /MD: CRT calls go through the import table (FF 15). */
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

/* ------------------------------------------------------------------ */
/* 0x10035400                                                         */
/* ------------------------------------------------------------------ */

/* Matching TU for 0x10035400: DirectPlay init (prefix of a map-split
 * function; 0x10035533 is the internal join, not a separate C function).
 * Inferred from orig bytes: two lstrcpyA via IAT-in-esi, /Oi memset of
 * 8+16 dwords, stride-0xe0 zero, nested GetModuleHandleA into
 * FUN_10009b00, FUN_10037260(FUN_1006d280(id), hr). */

int BrDPlayStartup(void *hInst, void *pCtx);   /* 0x10009B00, net/br_dplay.c */
int FUN_10032320(void *pCtx);
int FUN_10035bb0(void *pCtx);
int FUN_1006d280(int);
int FUN_10037260(int, int);
int FUN_10036e50(int **);
int FUN_10036300(int *);
int __stdcall FUN_10035ac0(int, int, int, int, int, int);

extern char DAT_10ac3e80[];
extern char DAT_10ac3f88[];
extern char DAT_10396f08[];
extern char DAT_10b71aa0[];
extern char DAT_10b71ac0[];
extern int DAT_10ac315c;
extern int DAT_10273334;
extern int DAT_10226a48;
extern int DAT_10ac5bdc;
extern int DAT_10ac4090;
extern int DAT_10ac4098;
extern int DAT_10077500;
extern int DAT_105bc72c;
extern int *g_brP277B40;             /* 0x10273328: the IDirectPlay4A interface */
extern int *DAT_10ac3068;            /* the IDirectPlayLobby interface */
extern int DAT_10ac5d2c;
extern int DAT_10ac5bf0;
extern int DAT_10ac306c;
extern int DAT_10ac408c;
extern char s_Could_not_create_DirectPlay_obje_100aa4d8[];

typedef int (__stdcall *CC_std_5)(void *, int, int, int, int);

/* WHAT IT DOES: initialise the multiplayer subsystem from cold -- clears the
 * session name and password buffers, zeroes every entry in the player table,
 * and creates the DirectPlay object. Returns zero if DirectPlay is
 * unavailable, which is how the game discovers multiplayer cannot run. */
/* @implements 0x10035400 glide BrDPlayCreate */
int BrDPlayCreate(void)
{
  int *puVar1;
  int iVar4;
  char local_400[1024];

  lstrcpyA((LPSTR)&DAT_10ac3e80, (LPCSTR)&DAT_10396f08);
  lstrcpyA((LPSTR)&DAT_10ac3f88, (LPCSTR)&DAT_10396f08);
  memset(&DAT_10b71aa0, 0, 32);
  memset(&DAT_10b71ac0, 0, 64);
  puVar1 = &DAT_10ac315c;
  do {
    *puVar1 = 0;
    puVar1 = puVar1 + 0x38;
  } while ((int)puVar1 < 0x10ac3f5c);

  iVar4 = BrDPlayStartup(GetModuleHandleA((LPCSTR)0x0), &g_brP277B40);
  if (iVar4 < 0) {
    return 0;
  }
  iVar4 = FUN_10032320(&g_brP277B40);
  if (iVar4 == -0x7788fbd2) {
    g_brP277B40 = 0;
    iVar4 = FUN_10035bb0(&g_brP277B40);
    if (iVar4 < 0) {
      sprintf(local_400, s_Could_not_create_DirectPlay_obje_100aa4d8, iVar4);
      FUN_10037260(FUN_1006d280(0x12b), iVar4);
      return 0;
    }
  } else {
    if (iVar4 < 0) {
      FUN_10037260(FUN_1006d280(0x12a), iVar4);
      exit(1);
    }
    if (DAT_10273334 != 0) {
      DAT_10226a48 = 2;
      DAT_10ac5bdc = 1;
    } else {
      DAT_10226a48 = 1;
      DAT_10ac5bdc = 0;
    }
    DAT_10ac4090 = 1;
  }
  iVar4 = DAT_10ac4090;
  DAT_10ac4098 = (int)&g_brP277B40;
  if (iVar4 == 0) {
    (*(CC_std_5 *)(*(int *)g_brP277B40 + 0x8c))(
        g_brP277B40, (int)&DAT_10077500, (int)FUN_10035ac0, DAT_105bc72c, 0);
    iVar4 = FUN_10036e50(&DAT_10ac3068);
    if (iVar4 < 0) {
      return 0;
    }
  } else {
    iVar4 = DAT_10ac5d2c;
    DAT_10ac5bf0 = 0;
    if (iVar4 != 0) {
      iVar4 = FUN_10036300(g_brP277B40);
      if (iVar4 < 0) {
        return 0;
      }
    }
    DAT_10ac306c = SetTimer((HWND)DAT_105bc72c, 1, 1000, (TIMERPROC)0x0);
    DAT_10ac408c = 1;
  }
  return 1;
}

/* ------------------------------------------------------------------ */
/* 0x100355F0                                                         */
/* ------------------------------------------------------------------ */

typedef int (*funcptr)();

int FUN_10035be0();
extern int DAT_10ac5d30;
extern int g_brAA287C;
extern int g_brAA2884;
extern int g_brAA2888;
extern HWND g_brP680584;               /* the game window */
int BrSndThreadStop();
int BrSub100586A0();

/* WHAT IT DOES: shut a multiplayer session down: stops the periodic timer,
 * stops the sound thread if it was started, tears the session down, and
 * clears the flags that say a session is live. Leaves the local car record
 * alone in the two modes where it is still needed. */
/* @implements 0x100355F0 glide BrExt_1003BF60 */
void BrExt_1003BF60(void)

{
  BrSub100586A0();
  KillTimer(g_brP680584,DAT_10ac306c);
  if (g_brAA2884 != 0) {
    BrSndThreadStop();
  }
  FUN_10035be0();
  if (((g_brAA287C != 2) && (g_brAA287C != 3)) && (DAT_10ac5d30 != 0)) {
    *(char *)(DAT_10ac5d30 + 0x2b64) = 0;
    *(unsigned int *)(DAT_10ac5d30 + 0x1c) = *(unsigned int *)(DAT_10ac5d30 + 0x1c) & 0xffffffef;
  }
  DAT_10ac408c = 0;
  g_brAA2884 = 0;
  DAT_10226a48 = 0;
  g_brAA2888 = 0;
  return;
}

/* ------------------------------------------------------------------ */
/* 0x10035660                                                         */
/* ------------------------------------------------------------------ */

int FUN_10005f50();
extern int DAT_10ac4094;
extern char s_DirectPlay_interface_final_insta_100aa514[];
int BrGlobalFreeAll();

typedef int (__stdcall *CC_std_1)(void *);

/* WHAT IT DOES: final multiplayer teardown at exit: closes the session,
 * frees every global allocation, logs the DirectPlay interface's remaining
 * reference count for leak-hunting, and releases the last interface. */
/* @implements 0x10035660 glide FUN_10035660 */
void FUN_10035660(void)

{
  char local_104 [260];
  
  FUN_10005f50(1);
  BrGlobalFreeAll();
  sprintf(local_104,s_DirectPlay_interface_final_insta_100aa514,DAT_10ac4094);
  OutputDebugStringA(local_104);
  if (DAT_10ac3068 != (int *)0x0) {
    (*(CC_std_1 *)(*(int *)(DAT_10ac3068) + 8))(DAT_10ac3068);
  }
  return;
}

/* ------------------------------------------------------------------ */
/* 0x100356B0                                                         */
/* ------------------------------------------------------------------ */

int FUN_10036300();
int FUN_10036b20();
extern HANDLE DAT_1027332c;
extern int g_brPAA29D4;
extern char s_Could_not_select_service_provide_100aa544[];
int BrComCreateInstance();
typedef int (__stdcall *CC_std_3)(void *, int, int);

/* WHAT IT DOES: connect to a chosen network transport -- tears down whatever
 * was running, asks the user's selection for its service provider, creates a
 * fresh DirectPlay object and initialises it on that provider. This is the
 * step between picking 'TCP/IP' or 'modem' and being able to see sessions. */
/* @implements 0x100356B0 glide FUN_100356b0 */
void FUN_100356b0(void)

{
  int iVar1;
  int local_408 [2];
  char local_400 [1024];
  
  local_408[0] = 0;
  local_408[1] = 0;
  KillTimer(g_brP680584,DAT_10ac306c);
  FUN_10035be0();
  iVar1 = FUN_10036b20(local_408,local_408 + 1);
  if (local_408[0] != 0) {
    iVar1 = BrComCreateInstance(&g_brP277B40);
    DAT_10ac4094 = DAT_10ac4094 + 1;
    if ((((iVar1 >= 0)) && (g_brP277B40 != (int *)0x0)) &&
       (iVar1 = (*(CC_std_3 *)(*(int *)(g_brP277B40) + 152))(g_brP277B40,local_408[0],0), (iVar1 >= 0))) {
      if ((g_brAA287C != 2) && (g_brAA287C != 3)) {
        if ((g_brPAA29D4 != 0) && (iVar1 = FUN_10036300(g_brP277B40), iVar1 < 0))
        goto LAB_100357b5;
        DAT_10ac306c = SetTimer(g_brP680584,1,1000,(TIMERPROC)0x0);
        DAT_10ac408c = 1;
      }
      if (DAT_1027332c != (HANDLE)0x0) {
        return;
      }
      DAT_1027332c = CreateEventA((LPSECURITY_ATTRIBUTES)0x0,0,0,(LPCSTR)0x0);
      if (DAT_1027332c != (HANDLE)0x0) {
        return;
      }
      iVar1 = -0x7ff8fff2;
    }
  }
LAB_100357b5: ;
  if (iVar1 != -0x7788fee8) {
    sprintf(local_400,s_Could_not_select_service_provide_100aa544,iVar1);
  }
  return;
}

/* ------------------------------------------------------------------ */
/* 0x100357E0                                                         */
/* ------------------------------------------------------------------ */

int FUN_10035c50();
int FUN_100367c0();
extern int g_brPA9D008;
extern char s_Could_not_host_session_because_o_100aa580[];
int BrNetMutexInit();
int BrSub10071550();

/* WHAT IT DOES: host a new multiplayer session -- builds the session
 * description, asks DirectPlay to create it, and on success marks the game
 * as hosting and starts the networking mutexes. On failure it formats a
 * message and returns without starting anything. */
/* @implements 0x100357E0 glide BrSub1003C150 */
void BrSub1003C150(void)

{
  int iVar2;
  int local_4cc [51];
  char local_400 [1024];
  
  if (g_brP277B40 != 0) {
    memset(local_4cc, 0, 204);
    FUN_100367c0(local_4cc);
    iVar2 = FUN_10035c50(g_brP277B40,local_4cc,g_brPA9D008);
    if (iVar2 < 0) {
      sprintf(local_400,s_Could_not_host_session_because_o_100aa580,iVar2);
      return;
    }
    DAT_10226a48 = 2;
    BrSub10071550();
    BrNetMutexInit(1);
  }
  return;
}

/* ------------------------------------------------------------------ */
/* 0x100358C0                                                         */
/* ------------------------------------------------------------------ */


/* WHAT IT DOES: start the once-a-second Windows timer that drives
 * multiplayer housekeeping, and flag that it is running. Always reports
 * success. */
/* @implements 0x100358C0 glide BrTimerStart1003C230 */
int BrTimerStart1003C230(void)

{
  FUN_100356b0();
  DAT_10ac306c = SetTimer(g_brP680584,1,1000,(TIMERPROC)0x0);
  DAT_10ac408c = 1;
  return 1;
}

/* ------------------------------------------------------------------ */
/* 0x100361A0                                                         */
/* ------------------------------------------------------------------ */

extern int DAT_10ac5898;
extern int g_brPAA29E4;
int BrSlotsFindById();
int BrSlotsResetIfBZero();
int BrSub1003C9B0();
int BrSub1003D950();


/* WHAT IT DOES: join an existing multiplayer session: leaves any current
 * one, clears the local player table, and passes the request to DirectPlay.
 * Returns the DirectPlay result, or a generic failure code when no
 * DirectPlay object exists. */
/* @implements 0x100361A0 glide FUN_100361a0 */
int FUN_100361a0(int param_1,int param_2,int param_3,int param_4)

{
  int *puVar1;
  int uVar2;
  int uVar3;
  
  uVar3 = 0x80004005;
  if (g_brPAA29E4 != 0) {
    BrSub1003C9B0();
  }
  puVar1 = &DAT_10ac5898;
  do {
    *puVar1 = 0;
    puVar1 = puVar1 + 3;
  } while ((int)puVar1 < 0x10ac58f8);
  if (g_brP277B40 != (int *)0x0) {
    uVar3 = (*(CC_std_5 *)(*(int *)(g_brP277B40) + 48))(g_brP277B40,param_1,param_2,param_3,param_4);
  }
  BrSlotsResetIfBZero();
  uVar2 = BrSlotsFindById(*(int *)(g_brPA9D008 + 8));
  BrSub1003D950(g_brPA9D008,uVar2);
  return uVar3;
}

/* ------------------------------------------------------------------ */
/* 0x10036740                                                         */
/* ------------------------------------------------------------------ */

typedef int (__stdcall *COM3)(void *this, void *buf, unsigned int *size);

/* WHAT IT DOES: call a DirectPlay method that reports how big its answer is
 * before it will give it -- ask with no buffer, take the 'buffer too small'
 * reply as the size, allocate exactly that much, and ask again. The standard
 * two-call pattern, wrapped so callers do not repeat it. */
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

/* ------------------------------------------------------------------ */
/* 0x10036F40                                                         */
/* ------------------------------------------------------------------ */

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

