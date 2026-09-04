/* br_netstate.c -- net.
 *
 * The multiplayer session's shared state: the mutex-guarded player-slot
 * table, the reset that empties it at the start of a session, the mutexes
 * themselves, and the stale-packet filter that decides which incoming car
 * update is worth applying.
 *
 * Filed out of the address batches: these functions were
 * matched first and grouped by what they are afterwards.
 * Every function carries its original address.
 */
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include "slice1_02.h"

#include <string.h>

/* 0x10005960 */
/* WHAT IT DOES: wipes the multiplayer state back to empty at the start of a
 * session: clears every player slot's samples and status under that slot's
 * own lock, and resets the shared counters, queues and timers. A few fields
 * are deliberately stepped over and left as they were, and the disarmed
 * timers are set to -1 rather than zero. */
/* @implements 0x10005960 d3d BrNetReset */
#ifdef BR_MATCHING_BUILD
/* The original takes no arguments and reaches every field as a loose global:
 * the slot array runs from 0x1021CE58, and the shared state is scattered from
 * 0x1021C81C to 0x105CCB80.  The mutex is the raw Win32 pair
 * WaitForSingleObject(h, INFINITE) / ReleaseMutex(h) through the import table,
 * not the port's BrNetMutexLock/Unlock wrappers.  pNet is the header's
 * signature and is unused here. */
__declspec(dllimport) unsigned long __stdcall WaitForSingleObject(void *, unsigned long);
__declspec(dllimport) int __stdcall ReleaseMutex(void *);

extern int DAT_1021ce64;   /* slot[0] + 0x00C -- the walk pointer */
extern int DAT_102265e4;   /* one past the last slot; also the pair array */
extern int DAT_10226624;
extern int DAT_10226a54;
extern int DAT_10226a28;
extern int DAT_10226a38;
extern unsigned char DAT_1021c9b0;
extern int DAT_10226a58;
extern int DAT_1021ce00;
extern int DAT_10226a5c;
extern int DAT_1021c904;
extern int DAT_10226a60;
extern int DAT_1021ce48;
extern int DAT_1021ce54;
extern int DAT_102265d8;
extern int DAT_10226a34;
extern int DAT_1021c90c;
extern int DAT_1021ce44;
extern int DAT_1021ce4c;
extern int DAT_1021c900;
extern int DAT_1021c81c;
extern int DAT_10226a30;
extern int DAT_1021c908;
extern int DAT_10226a6c;
extern int DAT_10226a50;
extern int DAT_105ccb80;

int BrNetReset(BrNetState *pNet)
{
    int *p;
    int *q;

    (void)pNet;

    /* q walks &slot->f00C (slot + 0x0C), one 0x978-byte record per turn; p is
     * the record base.  Two things are load-bearing here:
     *   - q, not p, is the loop variable.  VC5 substitutes the loop pointer
     *     with the first derived address it has to materialise, so a p-based
     *     loop moves the induction register to &slot->f038 and costs a `lea`
     *     at the bottom test.  With q primary the test is `cmp esi, END`.
     *   - the ReleaseMutex handle is read through q, not p.  Read through the
     *     same pointer as the +0x558 stores, VC5 proves non-aliasing and
     *     hoists `mov ecx,[esi-0xc]; push ecx` above them; through the other
     *     pointer it cannot, and the load stays after the stores like orig. */
    q = &DAT_1021ce64;
    do {
        p = q - 3;
        WaitForSingleObject((void *)p[0], 0xffffffff);
        p[2] = 0;                    /* +0x008 */
        memset(q, 0, 32);            /* +0x00C..+0x02B  (rep stosd, 8 dwords) */
        p[11] = 0;                   /* +0x02C */
        memset(q + 11, 0, 32);       /* +0x038..+0x057 */
        p[0x156] = 0;                /* +0x558 */
        p[0x157] = 0;                /* +0x55C */
        p[0x158] = -1;               /* +0x560 */
        p[0x15a] = 0;                /* +0x568 */
        p[0x15b] = 0;                /* +0x56C */
        p[0x159] = 0;                /* +0x564 */
        p[0x25d] = 0;                /* +0x974 */
        ReleaseMutex((void *)q[-3]);
        q += 0x25e;
    } while ((int)q < (int)&DAT_102265e4);

    WaitForSingleObject((void *)DAT_10226a54, 0xffffffff);
    DAT_10226a28 = -1;
    DAT_10226a38 = 0;
    DAT_1021c9b0 = 0;
    ReleaseMutex((void *)DAT_10226a54);

    WaitForSingleObject((void *)DAT_10226a58, 0xffffffff);
    memset(&DAT_1021ce00, 0, 64);
    ReleaseMutex((void *)DAT_10226a58);

    WaitForSingleObject((void *)DAT_10226a5c, 0xffffffff);
    DAT_1021c904 = -1;
    ReleaseMutex((void *)DAT_10226a5c);

    WaitForSingleObject((void *)DAT_10226a60, 0xffffffff);
    DAT_1021ce48 = -1;
    ReleaseMutex((void *)DAT_10226a60);

    WaitForSingleObject((void *)DAT_1021ce54, 0xffffffff);
    DAT_102265d8 = 0;
    ReleaseMutex((void *)DAT_1021ce54);

    WaitForSingleObject((void *)DAT_10226a34, 0xffffffff);
    DAT_10226624 = 0;
    ReleaseMutex((void *)DAT_10226a34);

    WaitForSingleObject((void *)DAT_1021c90c, 0xffffffff);
    DAT_1021ce44 = 0;
    ReleaseMutex((void *)DAT_1021c90c);

    WaitForSingleObject((void *)DAT_1021ce4c, 0xffffffff);
    DAT_1021c900 = 0;
    ReleaseMutex((void *)DAT_1021ce4c);

    WaitForSingleObject((void *)DAT_1021c81c, 0xffffffff);
    DAT_10226a30 = -1;
    ReleaseMutex((void *)DAT_1021c81c);

    DAT_1021c908 = 0;
    DAT_10226a6c = -1;

    p = &DAT_102265e4;
    do {
        p[-1] = 0;
        *p = 0;
        p += 2;
    } while ((int)p < (int)&DAT_10226624);

    DAT_10226a50 = 0;
    DAT_105ccb80 = 0;
    return 1;
}
#else
int BrNetReset(BrNetState *pNet)
{
    int i;

    for (i = 0; i < BR_NET_SLOTS; ++i) {
        BrNetSlot *p = &pNet->aSlots[i];

        BrNetMutexLock(p->hMutex);

        /* The original clears +0x08, then +0x0C..+0x28, then +0x2C, then
         * +0x38..+0x54. +0x04, +0x30 and +0x34 are stepped over on purpose. */
        p->f008 = 0;
        memset(p->f00C, 0, sizeof p->f00C);
        p->f02C = 0;
        memset(p->f038, 0, sizeof p->f038);

        p->f558 = 0;
        p->f55C = 0;
        p->f560 = -1;
        p->f568 = 0;
        p->f56C = 0;
        p->f564 = 0;
        p->f974 = 0;

        BrNetMutexUnlock(p->hMutex);
    }

    BrNetMutexLock(pNet->h1022AF24);
    pNet->f1022AEF8 = -1;
    pNet->f1022AF08 = 0;
    pNet->f10220E80 = 0;
    BrNetMutexUnlock(pNet->h1022AF24);

    BrNetMutexLock(pNet->h1022AF28);
    memset(pNet->a102212D0, 0, sizeof pNet->a102212D0);
    BrNetMutexUnlock(pNet->h1022AF28);

    BrNetMutexLock(pNet->h1022AF2C);
    pNet->f10220DD4 = -1;
    BrNetMutexUnlock(pNet->h1022AF2C);

    BrNetMutexLock(pNet->h1022AF30);
    pNet->f10221318 = -1;
    BrNetMutexUnlock(pNet->h1022AF30);

    BrNetMutexLock(pNet->h10221324);
    pNet->f1022AAA8 = 0;
    BrNetMutexUnlock(pNet->h10221324);

    BrNetMutexLock(pNet->h1022AF04);
    pNet->f1022AAF4 = 0;
    BrNetMutexUnlock(pNet->h1022AF04);

    BrNetMutexLock(pNet->h10220DDC);
    pNet->f10221314 = 0;
    BrNetMutexUnlock(pNet->h10220DDC);

    BrNetMutexLock(pNet->h1022131C);
    pNet->f10220DD0 = 0;
    BrNetMutexUnlock(pNet->h1022131C);

    BrNetMutexLock(pNet->h10220CEC);
    pNet->f1022AF00 = -1;
    BrNetMutexUnlock(pNet->h10220CEC);

    /* Tail: no lock is taken for any of these. */
    pNet->f10220DD8 = 0;
    pNet->f1022AF3C = -1;
    memset(pNet->a1022AAB0, 0, sizeof pNet->a1022AAB0);
    pNet->f1022AF20 = 0;
    pNet->f106909D8 = 0;

    return 1;
}
#endif

/* 0x10004DC0 / d3d 0x10004A50 */
/* WHAT IT DOES: writes a player slot's status word under that slot's lock. */
/* @implements 0x10004DC0 glide BrNetSlotSetF02C */
#ifdef BR_MATCHING_BUILD
/* Orig is two-arg cdecl (index at [esp+4], value at [esp+8]), not the port's
 * (pNet, slot, value).  Mutex lock is the raw import, same shape as BrNetReset:
 * WaitForSingleObject(h, INFINITE) / ReleaseMutex(h), not BrNetMutexLock.
 * Three indexings of the global so the ReleaseMutex handle reloads; a cached
 * pointer would materialise the base into esi and encode [esi]/[esi+0x2c]
 * instead of orig's [esi+0x1021ce58]/[esi+0x1021ce84]. */
__declspec(dllimport) unsigned long __stdcall WaitForSingleObject(void *, unsigned long);
__declspec(dllimport) int __stdcall ReleaseMutex(void *);

typedef struct BrNetSlot978 {
    void *hMutex;                 /* +0x000 = 0x1021ce58 */
    char  pad004[0x28];
    int   f02C;                   /* +0x02C = 0x1021ce84 */
    char  rest[0x978 - 0x30];
} BrNetSlot978;

typedef char br_assert_slot978[(sizeof(BrNetSlot978) == 0x978) ? 1 : -1];

extern BrNetSlot978 slots[];      /* 0x1021ce58, stride 0x978 */

void BrNetSlotSetF02C(int param_1, int param_2)
{
    WaitForSingleObject((void *)slots[param_1].hMutex, 0xffffffff);
    slots[param_1].f02C = param_2;
    ReleaseMutex((void *)slots[param_1].hMutex);
}
#else
void BrNetSlotSetF02C(BrNetState *pNet, int32_t slot, int32_t value)
{
    BrNetSlot *p = &pNet->aSlots[slot];

    BrNetMutexLock(p->hMutex);
    p->f02C = value;
    BrNetMutexUnlock(p->hMutex);
}
#endif

#ifdef BR_MATCHING_BUILD
__declspec(dllimport) unsigned long __stdcall WaitForSingleObject(void *, unsigned long);
__declspec(dllimport) int __stdcall ReleaseMutex(void *);

extern void    *g_brH221324;
extern int32_t  g_br22AAA8;
extern void    *g_brH22AF04;
extern int32_t  g_br22AAF4;

/* WHAT IT DOES: under the mutex, turns on the broadcast-enable flag. */
/* @implements 0x10004BB0 d3d BrNetLockSet22AAA8 */
int BrNetLockSet22AAA8(void)
{
    WaitForSingleObject(g_brH221324, (unsigned long)-1);
    g_br22AAA8 = 1;
    ReleaseMutex(g_brH221324);
    return 1;
}

/* WHAT IT DOES: seeds the keepalive counter if it is sitting at zero. */
/* @implements 0x10004BE0 d3d BrNetLockSetIfZero22AAF4 */
int BrNetLockSetIfZero22AAF4(void)
{
    WaitForSingleObject(g_brH22AF04, (unsigned long)-1);
    if (g_br22AAF4 == 0)
        g_br22AAF4 = 1;
    ReleaseMutex(g_brH22AF04);
    return 1;
}
#else
int BrNetLockSet22AAA8(BrNetState *pNet)
{
    BrNetMutexLock(pNet->h10221324);
    pNet->f1022AAA8 = 1;
    BrNetMutexUnlock(pNet->h10221324);
    return 1;
}

int BrNetLockSetIfZero22AAF4(BrNetState *pNet)
{
    BrNetMutexLock(pNet->h1022AF04);
    if (pNet->f1022AAF4 == 0)
        pNet->f1022AAF4 = 1;
    BrNetMutexUnlock(pNet->h1022AF04);
    return 1;
}
#endif

/* ── Ghidra-matched functions ─────────────────────────── */
#ifdef BR_MATCHING_BUILD
#include <windows.h>
extern int DAT_1021c81c;
extern int DAT_1021c908;
extern int DAT_1021ce40;
extern int DAT_1021ce4c;
extern int DAT_1021ce58;
extern int DAT_10226a54;
extern int DAT_10226a58;
extern int DAT_10226a5c;
extern int DAT_10226a64;
extern int g_brH220DDC;
extern int g_h1022AF30;
int BrNetReset();

/* WHAT IT DOES: create Win32 mutexes for the net/multiplayer subsystem and reset the network layer. */
/* @implements 0x10005E80 glide BrNetMutexInit */

int BrNetMutexInit(void)

{
  HANDLE pvVar1;
  int *puVar2;
  
  puVar2 = &DAT_1021ce58;
  do {
    pvVar1 = CreateMutexA((LPSECURITY_ATTRIBUTES)0x0,0,(LPCSTR)0x0);
    *puVar2 = pvVar1;
    puVar2 = puVar2 + 0x25e;
  } while ((int)puVar2 < 0x102265d8);
  DAT_10226a54 = CreateMutexA((LPSECURITY_ATTRIBUTES)0x0,0,(LPCSTR)0x0);
  DAT_10226a58 = CreateMutexA((LPSECURITY_ATTRIBUTES)0x0,0,(LPCSTR)0x0);
  DAT_10226a5c = CreateMutexA((LPSECURITY_ATTRIBUTES)0x0,0,(LPCSTR)0x0);
  g_h1022AF30 = CreateMutexA((LPSECURITY_ATTRIBUTES)0x0,0,(LPCSTR)0x0);
  DAT_1021ce40 = 0;
  DAT_1021c908 = 0;
  BrTimeUpdate();
  DAT_10226a64 = CreateMutexA((LPSECURITY_ATTRIBUTES)0x0,0,(LPCSTR)0x0);
  g_brH221324 = CreateMutexA((LPSECURITY_ATTRIBUTES)0x0,0,(LPCSTR)0x0);
  g_brH22AF04 = CreateMutexA((LPSECURITY_ATTRIBUTES)0x0,0,(LPCSTR)0x0);
  g_brH220DDC = CreateMutexA((LPSECURITY_ATTRIBUTES)0x0,0,(LPCSTR)0x0);
  DAT_1021ce4c = CreateMutexA((LPSECURITY_ATTRIBUTES)0x0,0,(LPCSTR)0x0);
  DAT_1021c81c = CreateMutexA((LPSECURITY_ATTRIBUTES)0x0,0,(LPCSTR)0x0);
  ((int (*)())BrNetReset)();
  return 1;
}


extern int DAT_1007b268;
extern float DAT_1021c820;
extern float DAT_1021c990;
extern float DAT_1021c994;
extern float DAT_1021c998;
extern float DAT_1021c99c;
extern float DAT_1021c9a0;
extern float DAT_1021c9a4;
extern float DAT_1021c9a8;
extern int DAT_10226a70;
extern float _DAT_100770ac;
extern float _DAT_1021c898;
int FUN_10004fd0(float *);
int FUN_100051c0(float *, float *);
int FUN_10005330(void);
int BrNetSendFlush(void);

/* WHAT IT DOES: decide whether an incoming car-state update is fresh enough
 * to accept: a new enough one is copied into the shared record and applied,
 * an older one is counted as a miss and only allowed to nudge a couple of
 * values before being dropped. The network's stale-packet filter. */
/* @implements 0x100054A0 glide FUN_100054a0 */
/* auto-filed from ghidra --refine; transforms: as-is */

int FUN_100054a0(float *param_1)
{
  int uVar1;

  if ((param_1[0x1e] >= _DAT_100770ac) && (_DAT_1021c898 < _DAT_100770ac)) {
    memcpy(&DAT_1021c820, param_1, 0xa0);
    uVar1 = FUN_10004fd0(param_1);
    return uVar1;
  }
  DAT_10226a70 = DAT_10226a70 + 1;
  if (DAT_10226a70 < 3) {
    if (DAT_1021c990 < param_1[0x20]) {
      DAT_1021c990 = param_1[0x20];
    }
    if (DAT_1021c994 < param_1[0x21]) {
      DAT_1021c994 = param_1[0x21];
    }
    if (DAT_1021c998 < param_1[0x22]) {
      DAT_1021c998 = param_1[0x22];
    }
    if (DAT_1021c99c < param_1[0x23]) {
      DAT_1021c99c = param_1[0x23];
    }
    if (DAT_1021c9a0 < param_1[0x24]) {
      DAT_1021c9a0 = param_1[0x24];
    }
    if (DAT_1021c9a4 < param_1[0x25]) {
      DAT_1021c9a4 = param_1[0x25];
    }
    if (DAT_1021c9a8 < param_1[0x26]) {
      DAT_1021c9a8 = param_1[0x26];
    }
    return 1;
  }
  if (param_1[0x20] < DAT_1021c990) {
    param_1[0x20] = DAT_1021c990;
  }
  if (param_1[0x21] < DAT_1021c994) {
    param_1[0x21] = DAT_1021c994;
  }
  if (param_1[0x22] < DAT_1021c998) {
    param_1[0x22] = DAT_1021c998;
  }
  if (param_1[0x23] < DAT_1021c99c) {
    param_1[0x23] = DAT_1021c99c;
  }
  if (param_1[0x24] < DAT_1021c9a0) {
    param_1[0x24] = DAT_1021c9a0;
  }
  if (param_1[0x25] < DAT_1021c9a4) {
    param_1[0x25] = DAT_1021c9a4;
  }
  if (param_1[0x26] < DAT_1021c9a8) {
    param_1[0x26] = DAT_1021c9a8;
  }
  DAT_1021c9a8 = 0.0f;
  DAT_1007b268 = DAT_1007b268 + 1;
  DAT_1021c9a4 = 0.0f;
  DAT_1021c9a0 = 0.0f;
  DAT_1021c99c = 0.0f;
  DAT_1021c998 = 0.0f;
  DAT_1021c994 = 0.0f;
  DAT_1021c990 = 0.0f;
  DAT_10226a70 = 0;
  if (DAT_1007b268 % 4 == 0) {
    memcpy(&DAT_1021c820, param_1, 0xa0);
    uVar1 = FUN_10004fd0(param_1);
    return uVar1;
  }
  uVar1 = FUN_100051c0(param_1, &DAT_1021c820);
  BrNetSendFlush();
  FUN_10005330();
  return uVar1;
}

#endif /* BR_MATCHING_BUILD */
