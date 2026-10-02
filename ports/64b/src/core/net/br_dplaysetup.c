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
#include "br_coretypes.h"   /* br_globals: its objects */
#include "br_slots.h"   /* br_globals: its objects */
#include "slice2_25.h"   /* br_globals: its objects */
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

/* 64-bit core: declared once, by its definition's header */   /* 0x10009B00, net/br_dplay.c */
/* FUN_10032320: prototype in br_funcs.h */
/* FUN_10035bb0: prototype in br_funcs.h */
/* FUN_1006d280: prototype in br_funcs.h */
/* FUN_10037260: prototype in br_funcs.h */
/* BrDpCreateIface: prototype in br_funcs.h */
/* FUN_10036300: prototype in br_funcs.h */
/* BrNetSessionStore: prototype in br_funcs.h */

/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */             /* 0x10273328: the IDirectPlay4A interface */
/* 64-bit core: declared once, in br_globals.h or its struct's header */            /* the IDirectPlayLobby interface */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

typedef int (__stdcall *CC_std_5)(void *, void *, void *, void *, intptr_t);

/* WHAT IT DOES: initialise the multiplayer subsystem from cold -- clears the
 * session name and password buffers, zeroes every entry in the player table,
 * and creates the DirectPlay object. Returns zero if DirectPlay is
 * unavailable, which is how the game discovers multiplayer cannot run. */
/* @implements 0x10035400 glide BrDPlayCreate */
int BrDPlayCreate(void)
{
  int i;
  BrObj29D4 *iVar4;
  char local_400[1024];

  lstrcpyA((LPSTR)&g_aBrA9CDF0, (LPCSTR)&g_aBr39B720);
  lstrcpyA((LPSTR)&DAT_10ac3f88, (LPCSTR)&g_aBr39B720);
  memset(&DAT_10b71aa0, 0, 32);
  memset(&DAT_10b71ac0, 0, 64);
  for (i = 0; i < 16; i++) {
    g_aBrNetSession[i].pPayload = 0;
  }

  iVar4 = BrDPlayStartup(GetModuleHandleA((LPCSTR)0x0), &(*(int * *)&g_brP277B40));
  if (iVar4 < 0) {
    return 0;
  }
  iVar4 = BrDpLobbyConnect(&(*(int * *)&g_brP277B40));
  if (iVar4 == -0x7788fbd2) {
    (*(int * *)&g_brP277B40) = 0;
    iVar4 = BrComCreateInstance(&(*(int * *)&g_brP277B40));
    if (iVar4 < 0) {
      sprintf(local_400, s_Could_not_create_DirectPlay_obje_100aa4d8, iVar4);
      BrNetErrMsgBox(BrStrGet(0x12b), iVar4);
      return 0;
    }
  } else {
    if (iVar4 < 0) {
      BrNetErrMsgBox(BrStrGet(0x12a), iVar4);
      exit(1);
    }
    if (DAT_10273334 != 0) {
      (*(int *)&g_brRaceNet) = 2;
      g_host = 1;
    } else {
      (*(int *)&g_brRaceNet) = 1;
      g_host = 0;
    }
    g_guardB = 1;
  }
  iVar4 = g_guardB;
  g_brPA9D008 = (BrOptUi *)&g_BrDPlayCtx;
  if (iVar4 == 0) {
    (*(CC_std_5 *)&((void **)*(void ***)((*(int * *)&g_brP277B40)))[35])(
        (*(int * *)&g_brP277B40), (void *)&DAT_10077500, (void *)BrNetSessionStore, g_brOwner5BC72C, 0);
    iVar4 = BrDpCreateIface(&DAT_10ac3068);
    if (iVar4 < 0) {
      return 0;
    }
  } else {
    iVar4 = g_brPAA29D4;
    DAT_10ac5bf0 = 0;
    if (iVar4 != 0) {
      iVar4 = BrNetEnumSessionsStart((*(int * *)&g_brP277B40));
      if (iVar4 < 0) {
        return 0;
      }
    }
    DAT_10ac306c = SetTimer((HWND)g_brOwner5BC72C, 1, 1000, (TIMERPROC)0x0);
    g_guardA = 1;
  }
  return 1;
}

/* ------------------------------------------------------------------ */
/* 0x100355F0                                                         */
/* ------------------------------------------------------------------ */

typedef int (*funcptr)();

/* FUN_10035be0: prototype in br_funcs.h */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */               /* the game window */
/* BrSndThreadStop: prototype in br_funcs.h */
/* 64-bit core: declared once, by its definition's header */

/* WHAT IT DOES: shut a multiplayer session down: stops the periodic timer,
 * stops the sound thread if it was started, tears the session down, and
 * clears the flags that say a session is live. Leaves the local car record
 * alone in the two modes where it is still needed. */
/* @implements 0x100355F0 glide BrExt_1003BF60 */
void BrExt_1003BF60(void)

{
  BrSub100586A0();
  KillTimer(g_brOwner5BC72C,DAT_10ac306c);
  if (g_host != 0) {
    BrSndThreadStop();
  }
  BrDpShutdown();
  if (((DAT_10ac5bd4 != 2) && (DAT_10ac5bd4 != 3)) && (g_brPAA29D8 != 0)) {
    *(char *)(g_brPAA29D8 + 0x2b64) = 0;
    *(unsigned int *)(g_brPAA29D8 + 0x1c) = *(unsigned int *)(g_brPAA29D8 + 0x1c) & 0xffffffef;
  }
  g_guardA = 0;
  g_host = 0;
  (*(int *)&g_brRaceNet) = 0;
  g_inited = 0;
  return;
}

/* ------------------------------------------------------------------ */
/* 0x10035660                                                         */
/* ------------------------------------------------------------------ */

/* FUN_10005f50: prototype in br_funcs.h */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* BrGlobalFreeAll: prototype in br_funcs.h */

typedef int (__stdcall *CC_std_1)(void *);

/* WHAT IT DOES: final multiplayer teardown at exit: closes the session,
 * frees every global allocation, logs the DirectPlay interface's remaining
 * reference count for leak-hunting, and releases the last interface. */
/* @implements 0x10035660 glide FUN_10035660 */
void FUN_10035660(void)

{
  char local_104 [260];
  
  BrNetShutdown();
  BrGlobalFreeAll();
  sprintf(local_104,s_DirectPlay_interface_final_insta_100aa514,DAT_10ac4094);
  OutputDebugStringA(local_104);
  if (DAT_10ac3068 != (int *)0x0) {
    (*(CC_std_1 *)&((void **)*(void ***)((DAT_10ac3068)))[2])(DAT_10ac3068);
  }
  return;
}

/* ------------------------------------------------------------------ */
/* 0x100356B0                                                         */
/* ------------------------------------------------------------------ */

/* FUN_10036300: prototype in br_funcs.h */
/* FUN_10036b20: prototype in br_funcs.h */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* BrComCreateInstance: prototype in br_funcs.h */
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
  KillTimer(g_brOwner5BC72C,DAT_10ac306c);
  BrDpShutdown();
  iVar1 = BrDpAddressBuild(local_408,local_408 + 1);
  if (local_408[0] != 0) {
    iVar1 = BrComCreateInstance(&(*(int * *)&g_brP277B40));
    DAT_10ac4094 = DAT_10ac4094 + 1;
    if ((((iVar1 >= 0)) && ((*(int * *)&g_brP277B40) != (int *)0x0)) &&
       (iVar1 = (*(CC_std_3 *)&((void **)*(void ***)(((*(int * *)&g_brP277B40))))[38])((*(int * *)&g_brP277B40),local_408[0],0), (iVar1 >= 0))) {
      if ((DAT_10ac5bd4 != 2) && (DAT_10ac5bd4 != 3)) {
        if ((g_brPAA29D4 != 0) && (iVar1 = BrNetEnumSessionsStart((*(int * *)&g_brP277B40)), iVar1 < 0))
        goto LAB_100357b5;
        DAT_10ac306c = SetTimer(g_brOwner5BC72C,1,1000,(TIMERPROC)0x0);
        g_guardA = 1;
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

/* FUN_10035c50: prototype in br_funcs.h */
/* FUN_100367c0: prototype in br_funcs.h */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* BrNetMutexInit: prototype in br_funcs.h */
/* BrSub10071550: prototype in br_funcs.h */

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
  
  if ((*(int * *)&g_brP277B40) != 0) {
    memset(local_4cc, 0, 204);
    FUN_100367c0(local_4cc);
    iVar2 = BrNetSessionHost((*(int * *)&g_brP277B40),local_4cc,g_brPA9D008);
    if (iVar2 < 0) {
      sprintf(local_400,s_Could_not_host_session_because_o_100aa580,iVar2);
      return;
    }
    (*(int *)&g_brRaceNet) = 2;
    BrSub10071550();
    BrNetMutexInit();
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
  DAT_10ac306c = SetTimer(g_brOwner5BC72C,1,1000,(TIMERPROC)0x0);
  g_guardA = 1;
  return 1;
}

/* ------------------------------------------------------------------ */
/* 0x100361A0                                                         */
/* ------------------------------------------------------------------ */

/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* BrSlotsFindById: prototype in br_funcs.h */
/* 64-bit core: declared once, by its definition's header */
/* BrSub1003C9B0: prototype in br_funcs.h */
/* BrSub1003D950: prototype in br_funcs.h */


/* WHAT IT DOES: join an existing multiplayer session: leaves any current
 * one, clears the local player table, and passes the request to DirectPlay.
 * Returns the DirectPlay result, or a generic failure code when no
 * DirectPlay object exists. */
/* @implements 0x100361A0 glide FUN_100361a0 */
int FUN_100361a0(void *param_1, void *param_2, void *param_3, int param_4)   /* IDirectPlay::EnumPlayers (slot 12) */

{
  int i;
  int uVar2;
  int uVar3;
  
  uVar3 = 0x80004005;
  if (g_brPAA29E4 != 0) {
    BrSub1003C9B0();
  }
  for (i = 0; i < BR_SLOT_COUNT; i++) {
    g_aBrAA2538[i].b = 0;
  }
  if ((*(int * *)&g_brP277B40) != (int *)0x0) {
    uVar3 = (*(CC_std_5 *)&((void **)*(void ***)(((*(int * *)&g_brP277B40))))[12])((*(int * *)&g_brP277B40),param_1,param_2,param_3,param_4);
  }
  BrSlotsResetIfBZero();
  uVar2 = BrSlotsFindById(g_brPA9D008->f08);
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
/* 0x10036B20                                                         */
/* ------------------------------------------------------------------ */

/* FUN_10036650: prototype in br_funcs.h */

/* A 16-byte GUID as four ints; the copy is a structure assignment, which is
 * what makes the compiler pair the loads and stores (and form one destination
 * pointer when the element index is variable). */
/* BrDpGuid: br_coretypes.h */

/* One DPCOMPOUNDADDRESSELEMENT: data-type GUID, byte count, data pointer. */
typedef struct { BrDpGuid g; int cb; char *pv; } BrDpAddrElem;

/* CreateCompoundAddress lives at +0x38 in the IDirectPlayLobby vtable. */
typedef int (__stdcall *BrDpCreateAddr5)(void *, BrDpAddrElem *, int,
                                         void *, unsigned int *);

/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* modem provider GUID  */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* serial provider GUID */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* tcp/ip provider GUID */
/* 64-bit core: declared once, in br_globals.h or its struct's header */          /* DPAID_ServiceProvider */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* WHAT IT DOES: builds a DirectPlay compound address for the connection the
 * player selected, then asks the lobby interface (vtable +0x38,
 * CreateCompoundAddress) for its packed size, allocates a movable global
 * buffer, and calls it again to fill the buffer. The element list always
 * starts with the service-provider GUID; a modem connection appends the
 * modem name (if set) and the phone number, a serial connection fills in
 * defaulted device/baud strings and appends both, tcp/ip appends the host
 * address (or points at the queried blob when the GUIDs differ). On success
 * returns 0 and hands back the buffer and its size; on failure unlocks and
 * frees the buffer and returns the error code. */
/* T2 RESIDUE (805 orig / 805 recomp B, insns 232/231, raw 14+15, regnorm 2+3):
 * size-exact; byte-exact through branch 1 and branch 2 (offsets align to
 * +0x235). Two coupled defects remain, both in the certified either-or
 * layout class (see 0x10036810 BrComGetAlloc, T3 2026-09-09):
 *  - branch 3 (the tcp/ip arm): the original hoists the memcmp's cmpsb above
 *    the element GUID/cb stores and delays the jne 14 insns (flags held
 *    across the movs, GUID store order pair-swapped 1,0,3,2); ours keeps
 *    source order (stores, then cmpsb+jne adjacent). Ternary, temp-variable,
 *    duplicated-arm and member-wise-copy spellings all fail to reproduce the
 *    hoist (probes w3, w4, w6): the temp materializes an sbb pair instead.
 *  - the tail: the original lays out [err][jmp cleanup][2nd-call][cleanup]
 *    [success]; ours emits [err][cleanup][2nd-call][success] with a
 *    backward jl, eliding the jmp (one instruction fewer, same bytes).
 *    goto-shaped and arm-swapped restructures compile to identical bytes
 *    (probes w2, w5): the layout is not source-reachable from here.
 * Proven levers already in this file: null pointers spelled bare 0 (any
 * (void*)0x0 cast flips the whole tail layout AND the strlen guards, +11 B);
 * pObj/vt COM locals per the byte-exact sibling 0x10036F40; strlen guards
 * through a materialized unsigned temp; single count variable incremented in
 * place; one result variable across both vtable calls.
 * @t4-pass 0x10036B20 1 2026-09-09 probes 6 bytes 805 insns 231 regions 1 rows 5 census no  (fn.py variants: null spellings, goto tail, duplicated arms, memcmp temp, inverted pMem arms, member-wise GUID copy) */
/* @t4-pass 0x10036B20 2 2026-09-09 probes 10 bytes 805 insns 231 regions 3 rows 3 census no  (statement orders, commutes, ++, nested calls, pMem goto: 8 identical, 2 regressions) */
/* @t4-pass 0x10036B20 3 2026-09-09 probes 10 bytes 805 insns 231 regions 3 rows 3 census yes  (polarity, arm swap, &aElem[0], decl/init orders: 9 identical, 1 regression; histograms equal except the layout triple jge 1/0, jl 1/2, jmp 4/3) */
/* @t3 0x10036B20 2026-09-09 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 805/805 insns 231/232 rows 2+1 regions 3 oracle UNCLASSIFIED
 * @t3-effort passes 2 zero-movement 2 3
 * Residue: the tail's either-or layout fork and the branch-3 cmpsb hoist --
 * both in the certified layout class; dead lists in the T2 RESIDUE note
 * above.  Do not reopen before the end-grind. */
/* @implements 0x10036B20 glide BrDpAddressBuild */
int BrDpAddressBuild(int *param_1, unsigned int *param_2)

{
  int iVar2;
  int iVar3;
  unsigned int uVar5;
  void *pvVar4;
  void *pMem;
  int iVar6;
  char *pcVar7;
  void *pObj;
  int *vt;
  unsigned int local_50;
  char *local_4c;
  BrDpAddrElem aElem [3];

  pMem = 0;
  local_50 = 0;
  iVar2 = BrSub1003CFC0(&local_4c);
  if (0 <= iVar2) {
    if (memcmp(local_4c, DAT_10078898, 0x10) == 0) {
      aElem[0].g = DAT_10078978;
      iVar6 = 1;
      aElem[0].cb = 0x10;
      aElem[0].pv = DAT_10078898;
      uVar5 = strlen(DAT_10ac3f88);
      if (1 < uVar5) {
        aElem[1].g = DAT_100789b8;
        aElem[1].cb = lstrlenA(DAT_10ac3f88) + 1;
        aElem[1].pv = DAT_10ac3f88;
        iVar6 = 2;
      }
      pcVar7 = g_aBrA9CDF0;
      if (pcVar7 != 0) {
        aElem[iVar6].g = DAT_10078998;
        iVar3 = lstrlenA(g_aBrA9CDF0);
        aElem[iVar6].cb = iVar3 + 1;
        aElem[iVar6].pv = g_aBrA9CDF0;
        iVar6 = iVar6 + 1;
      }
    }
    else if (memcmp(local_4c, DAT_10078878, 0x10) == 0) {
      aElem[0].g = DAT_10078978;
      aElem[0].cb = 0x10;
      aElem[0].pv = DAT_10078878;
      uVar5 = strlen(DAT_10b71ac0);
      if (uVar5 <= 1) {
        lstrcpyA(DAT_10b71ac0, g_aBr39B720);
      }
      aElem[1].g = DAT_100789d8;
      aElem[1].cb = lstrlenA(DAT_10b71ac0) + 1;
      aElem[1].pv = DAT_10b71ac0;
      uVar5 = strlen(DAT_10b71aa0);
      if (uVar5 <= 1) {
        lstrcpyA(DAT_10b71aa0, g_aBr39B720);
      }
      aElem[2].g = DAT_100789f8;
      aElem[2].cb = lstrlenA(DAT_10b71aa0) + 1;
      aElem[2].pv = DAT_10b71aa0;
      iVar6 = 3;
    }
    else {
      aElem[0].g = DAT_10078978;
      aElem[0].cb = 0x10;
      if (memcmp(local_4c, DAT_10078868, 0x10) == 0) {
        aElem[0].pv = DAT_10078868;
      }
      else {
        aElem[0].pv = (char *)&local_4c;
      }
      iVar6 = 1;
    }
    pObj = DAT_10ac3068;
    vt = *(int **)pObj;
    iVar2 = (*(BrDpCreateAddr5 *)((char *)vt + 0x38))
                (pObj, aElem, iVar6, 0, &local_50);
    if (iVar2 == (int)0x8877001e) {
      pvVar4 = GlobalAlloc(0x42, local_50);
      pMem = GlobalLock(pvVar4);
      if (pMem == 0) {
        iVar2 = (int)0x8007000e;
      }
      else {
        pObj = DAT_10ac3068;
        vt = *(int **)pObj;
        iVar2 = (*(BrDpCreateAddr5 *)((char *)vt + 0x38))
                    (pObj, aElem, iVar6, pMem, &local_50);
        if (0 <= iVar2) {
          *param_1 = (int)pMem;
          *param_2 = local_50;
          return 0;
        }
      }
    }
  }
  if (pMem != 0) {
    pvVar4 = GlobalHandle(pMem);
    GlobalUnlock(pvVar4);
    pvVar4 = GlobalHandle(pMem);
    GlobalFree(pvVar4);
  }
  return iVar2;
}

/* ------------------------------------------------------------------ */
/* 0x10036F40                                                         */
/* ------------------------------------------------------------------ */

/* BrSub1003D850: prototype in br_funcs.h */

typedef int (__stdcall *COM4)(void *this, int a, void *b, int *c);
typedef int (__stdcall *COM5)(void *this, void *cb, void *buf, int n, int cookie);

/* WHAT IT DOES: the same ask-then-allocate-then-ask-again dance for a
 * different DirectPlay method -- one that takes an extra argument and
 * returns a variable-sized result. */
/* @implements 0x10036F40 glide FUN_10036f40 */
int FUN_10036f40(HWND param_1, void *param_2)

{
  int hr;
  HGLOBAL hMem;
  void *pMem;
  int size;
  void *pObj;
  int *vt;
  
  pObj = (*(int * *)&g_brP277B40);
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
      pObj = (*(int * *)&g_brP277B40);
      vt = *(int **)pObj;
      hr = (*(COM4 *)((char *)vt + 0x48))(pObj, 0, pMem, &size);
      if (hr >= 0) {
        hr = (*(COM5 *)&((void **)*(void ***)(param_2))[5])(param_2, (void *)BrSub1003D850, pMem, size, param_1);
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

