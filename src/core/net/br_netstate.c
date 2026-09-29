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
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
/* The header declares the port's (state, slot, ...) forms of these; the
 * originals take the slot (or key) alone and reach the state as globals. */
#define BrNetSlotName     BrNetSlotName_hdr
#define BrNetSlotGetF004  BrNetSlotGetF004_hdr
#define BrNetSlotGetF02C  BrNetSlotGetF02C_hdr
#define BrNetSlotSetF02C  BrNetSlotSetF02C_hdr
#define BrNetDropMatching BrNetDropMatching_hdr
#include "slice1_02.h"
#undef BrNetSlotName
#undef BrNetSlotGetF004
#undef BrNetSlotGetF02C
#undef BrNetSlotSetF02C
#undef BrNetDropMatching

#include <string.h>

/* 0x10005960 */
/* WHAT IT DOES: wipes the multiplayer state back to empty at the start of a
 * session: clears every player slot's samples and status under that slot's
 * own lock, and resets the shared counters, queues and timers. A few fields
 * are deliberately stepped over and left as they were, and the disarmed
 * timers are set to -1 rather than zero. */
/* @implements 0x10005960 d3d BrNetReset */
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

/* 0x10004DC0 / d3d 0x10004A50 */
/* WHAT IT DOES: writes a player slot's status word under that slot's lock. */
/* @implements 0x10004DC0 glide BrNetSlotSetF02C */
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
    char  pad030[0x570 - 0x30];
    char  name[0x978 - 0x570];    /* +0x570 = 0x1021d3c8, the player name */
} BrNetSlot978;

typedef char br_assert_slot978[(sizeof(BrNetSlot978) == 0x978) ? 1 : -1];

extern BrNetSlot978 slots[];      /* 0x1021ce58, stride 0x978 */

void BrNetSlotSetF02C(int param_1, int param_2)
{
    WaitForSingleObject((void *)slots[param_1].hMutex, 0xffffffff);
    slots[param_1].f02C = param_2;
    ReleaseMutex((void *)slots[param_1].hMutex);
}

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

/* ââ Ghidra-matched functions âââââââââââââââââââââââââââ */
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
void BrTimeUpdate(void);   /* 0x1006E360 */

/* WHAT IT DOES: create Win32 mutexes for the net/multiplayer subsystem and reset the network layer. */
/* @implements 0x10005E80 glide BrNetMutexInit */

int BrNetMutexInit(void)

{
  HANDLE pvVar1;
  int *puVar2;
  
  puVar2 = &DAT_1021ce58;
  do {
    pvVar1 = CreateMutexA((LPSECURITY_ATTRIBUTES)0x0,0,(LPCSTR)0x0);
    *puVar2 = (int)pvVar1;
    puVar2 = puVar2 + 0x25e;
  } while ((int)puVar2 < 0x102265d8);
  DAT_10226a54 = (int)CreateMutexA((LPSECURITY_ATTRIBUTES)0x0,0,(LPCSTR)0x0);
  DAT_10226a58 = (int)CreateMutexA((LPSECURITY_ATTRIBUTES)0x0,0,(LPCSTR)0x0);
  DAT_10226a5c = (int)CreateMutexA((LPSECURITY_ATTRIBUTES)0x0,0,(LPCSTR)0x0);
  g_h1022AF30 = (int)CreateMutexA((LPSECURITY_ATTRIBUTES)0x0,0,(LPCSTR)0x0);
  DAT_1021ce40 = 0;
  DAT_1021c908 = 0;
  BrTimeUpdate();
  DAT_10226a64 = (int)CreateMutexA((LPSECURITY_ATTRIBUTES)0x0,0,(LPCSTR)0x0);
  g_brH221324 = CreateMutexA((LPSECURITY_ATTRIBUTES)0x0,0,(LPCSTR)0x0);
  g_brH22AF04 = CreateMutexA((LPSECURITY_ATTRIBUTES)0x0,0,(LPCSTR)0x0);
  g_brH220DDC = (int)CreateMutexA((LPSECURITY_ATTRIBUTES)0x0,0,(LPCSTR)0x0);
  DAT_1021ce4c = (int)CreateMutexA((LPSECURITY_ATTRIBUTES)0x0,0,(LPCSTR)0x0);
  DAT_1021c81c = (int)CreateMutexA((LPSECURITY_ATTRIBUTES)0x0,0,(LPCSTR)0x0);
  ((int (*)())BrNetReset)();
  return 1;
}



/* ------------------------------------------------------------------ */
/* 0x10005330 / 0x10005400 -- the two periodic slot broadcasts          */
/* ------------------------------------------------------------------ */

int BrNetSlotGetF02C(int);                       /* 0x10004D80 */
int BrNetSend4AD0(void *dest, int a1, int a2, unsigned char r,
                  unsigned char g, unsigned char b, int a6,
                  char *text, unsigned char a8, unsigned char a9);  /* 0x10004AD0 */
extern int DAT_10226a48;             /* g_brRaceNet                        */
extern int DAT_10226a44;
extern int DAT_105ccb88;             /* g_brFlag6909E0                     */
extern int DAT_10af21b0;
extern int DAT_100bcbe8;             /* g_br0BD3E0                         */
extern int DAT_1007b264;             /* g_br094294, the local player slot  */
extern int DAT_10226e7c;             /* g_br22B34C                         */
extern int DAT_10273330;             /* g_br277B48                         */
extern unsigned char DAT_10af3bb4;   /* g_brAD0854: the player colour r,g,b */
extern unsigned char DAT_10af3bb5;
extern unsigned char DAT_10af3bb6;
extern char DAT_10b71648[];          /* g_brPB4E2E8, the player name       */
extern char DAT_10273328[];          /* g_brP277B40, the send target       */
/* An int-typed view of the sender for the two flag-byte arguments: with the
 * real prototype's unsigned char parameter VC5 folds the masks below
 * (`& 0x7f | 0x40` into `& 0x3f | 0x40`), which the original does not.  The
 * cast of the function designator still compiles to a direct call. */
typedef int (*BrNetSend4AD0Int)(void *dest, int a1, int a2, unsigned char r,
                                unsigned char g, unsigned char b, int a6,
                                char *text, int a8, int a9);

/* WHAT IT DOES: one tick of the network "still here" beacon. Under the tick
 * mutex, advance the counter when it is running and, every 27th tick, set
 * the resend flag and wrap it. Then, if the counter is running, a net race
 * is up, the deactivate flag is clear and the local frame count has not yet
 * reached the limit, broadcast the local player's slot, colour and name with
 * bit 7 set and bit 6 cleared in the slot's flag byte. */
/* Byte-exact 2026-09-28.  The flag byte is `(x & ~0x40) | 0x80` passed
 * through the int-typed view of the sender (BrNetSend4AD0Int above): with the
 * prototype's unsigned char parameter VC5 folds the two masks into
 * `& 0x3f | 0x80`, and with `& 0xbf` it widens the operation; the original
 * keeps `and al,0xbf; or al,0x80` separate, which only this pair gives.
 * (The old 1-byte residue note listed every other spelling as inert.) */
/* @implements 0x10005330 glide BrNetBeaconTick */
void BrNetBeaconTick(void)
{
  int n;

  WaitForSingleObject((void *)DAT_10226a34, INFINITE);
  if (DAT_10226624 != 0) {
    DAT_10226624++;
    if (DAT_10226624 >= 27) {
      DAT_10226a50 = 1;
      DAT_10226624 = 0;
    }
  }
  n = DAT_10226624;
  ReleaseMutex((void *)DAT_10226a34);
  if (n != 0 && DAT_10226a48 != 0 && DAT_10226a44 != 0 && DAT_105ccb88 == 0
      && DAT_10af21b0 < DAT_100bcbe8) {
    ((BrNetSend4AD0Int)BrNetSend4AD0)(DAT_10273328, DAT_1007b264, DAT_10226e7c, DAT_10af3bb4,
                  DAT_10af3bb5, DAT_10af3bb6, DAT_10273330, DAT_10b71648,
                  (BrNetSlotGetF02C(DAT_1007b264) & ~0x40) | 0x80, 0);
  }
}


/* WHAT IT DOES: the other periodic slot broadcast.  Under the slot mutex
 * (0x1021C90C), advance its counter when it is running and, every 100th
 * tick, raise the resend flag and wrap it; then, if the counter is running,
 * send the local player's slot, colour and name with bit 6 set and bit 7
 * cleared in the slot's flag byte.  Reset by BrNetReset with the beacon.
 * Byte-exact: the counter is ticked on the global itself and read into n
 * only after the wrap (the original's eax web copied to esi once at the
 * join); the flag-byte call sits inside the send's argument list, so the
 * trailing 0 is pushed before it, as the original does. */
/* @implements 0x10005400 glide BrNetSlotBroadcastTick */
void BrNetSlotBroadcastTick(void)
{
    int n;

    WaitForSingleObject((void *)DAT_1021c90c, 0xffffffff);
    if (DAT_1021ce44 != 0) {
        DAT_1021ce44++;
        if (DAT_1021ce44 >= 0x64) {
            DAT_105ccb80 = 1;
            DAT_1021ce44 = 0;
        }
    }
    n = DAT_1021ce44;
    ReleaseMutex((void *)DAT_1021c90c);
    if (n != 0) {
        ((BrNetSend4AD0Int)BrNetSend4AD0)(DAT_10273328, DAT_1007b264, DAT_10226e7c,
                      DAT_10af3bb4, DAT_10af3bb5, DAT_10af3bb6,
                      DAT_10273330, DAT_10b71648,
                      (BrNetSlotGetF02C(DAT_1007b264) & ~0x80) | 0x40, 0);
    }
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


/* Forward declarations for unknown functions/globals */
extern int DAT_1021ce58;
extern char DAT_1021ce5c;

/* WHAT IT DOES: read a different field out of a network player slot, under
 * the same per-slot mutex. */
/* @implements 0x10006060 glide BrNetSlotGetF004 */
int BrNetSlotGetF004(int param_1)

{
  int uVar1;
  
  WaitForSingleObject((HANDLE)(&DAT_1021ce58)[param_1 * 0x25e],0xffffffff);
  uVar1 = *(int *)(&DAT_1021ce5c + param_1 * 0x978);
  ReleaseMutex((HANDLE)(&DAT_1021ce58)[param_1 * 0x25e]);
  return uVar1;
}

/* The original binary is /MD: CRT calls resolve through the import table. */
#define _CRTIMP __declspec(dllimport)
#include <windows.h>

extern int DAT_10226a5c;    /* the stack's mutex handle */
extern int DAT_1021c904;    /* top index; negative = empty */
extern int DAT_1021c8c0[];  /* the stack itself */

/* One shared unlock/return tail in source; VC5 duplicates it into both
 * arms (each gets its own ReleaseMutex + ret). */
/* WHAT IT DOES: pop the next free network slot index off a shared stack,
 * returning -1 when none are left. Locked, because the receive thread also
 * takes slots. */
/* @implements 0x100060B0 glide BrNetStackPop */
int BrNetStackPop(void)
{
    int v;
    int n;

    WaitForSingleObject((HANDLE)DAT_10226a5c, 0xffffffff);
    n = DAT_1021c904;
    if (n >= 0) {
        v = DAT_1021c8c0[n];
        DAT_1021c904 = n - 1;
    } else {
        v = -1;
    }
    ReleaseMutex((HANDLE)DAT_10226a5c);
    return v;
}

/* The original binary is /MD: CRT calls resolve through the import table. */
#define _CRTIMP __declspec(dllimport)
#include <windows.h>

extern int DAT_1021ce58;    /* slot[0].hMutex; slot stride 0x978 (0x25E ints) */

/* WHAT IT DOES: read one network slot's state word plus three of its status
 * bytes in a single locked operation, so the caller sees a consistent
 * snapshot rather than four separately-locked reads that could disagree. */
/* @implements 0x10006150 glide BrNetSlotGetF030 */
int BrNetSlotGetF030(int i, unsigned char *pb34, unsigned char *pb35,
                     unsigned char *pb36)
{
    int v;
    int off = i * 0x978;

    WaitForSingleObject(*(HANDLE *)((char *)&DAT_1021ce58 + off),
                        0xffffffff);
    v     = *(int *)((char *)&DAT_1021ce58 + off + 0x30);
    *pb34 = *(unsigned char *)((char *)&DAT_1021ce58 + off + 0x34);
    *pb35 = *(unsigned char *)((char *)&DAT_1021ce58 + off + 0x35);
    *pb36 = *(unsigned char *)((char *)&DAT_1021ce58 + off + 0x36);
    ReleaseMutex(*(HANDLE *)((char *)&DAT_1021ce58 + off));
    return v;
}

/* Forward declarations for unknown functions/globals */
extern int DAT_1021ce00;
extern int DAT_10226a58;

/* WHAT IT DOES: read one entry from a shared network table under the table's
 * own mutex. */
/* @implements 0x100061B0 glide BrNetGetA102212D0 */
int BrNetGetA102212D0(int param_1)

{
  int uVar1;
  
  WaitForSingleObject((void *)DAT_10226a58,0xffffffff);
  uVar1 = (&DAT_1021ce00)[param_1];
  ReleaseMutex((void *)DAT_10226a58);
  return uVar1;
}

/* Forward declarations for unknown functions/globals */
extern int DAT_1021ce58;
extern char DAT_1021d3c8;
extern char DAT_10226628[];

/* WHAT IT DOES: copy a network player's name out of its slot into one SHARED
 * static buffer and return that buffer. Not re-entrant and not safe to hold:
 * the next caller overwrites it. That is the original's design. */
/* @implements 0x100061E0 glide BrNetSlotName */
char *BrNetSlotName(int param_1)

{
  WaitForSingleObject((HANDLE)(&DAT_1021ce58)[param_1 * 0x25e],0xffffffff);
  strcpy(DAT_10226628, &DAT_1021d3c8 + param_1 * 0x978);
  ReleaseMutex((HANDLE)(&DAT_1021ce58)[param_1 * 0x25e]);
  return DAT_10226628;
}

/* Forward declarations for unknown functions/globals */
extern int DAT_1021ce58;
extern char DAT_1021d3c8;

/* WHAT IT DOES: write a network player's name into its slot, under that
 * slot's mutex. The read side is BrNetSlotName. */
/* @implements 0x10006250 glide BrNetSlotSetName */
void BrNetSlotSetName(int param_1,char *param_2)

{
  WaitForSingleObject((HANDLE)(&DAT_1021ce58)[param_1 * 0x25e],0xffffffff);
  strcpy(&DAT_1021d3c8 + param_1 * 0x978, param_2);
  ReleaseMutex((HANDLE)(&DAT_1021ce58)[param_1 * 0x25e]);
  return;
}

/* The original binary is /MD: CRT calls resolve through the import table. */
#define _CRTIMP __declspec(dllimport)
#include <windows.h>

extern int DAT_1021ce58;    /* slot[0].hMutex; slot stride 0x978 (0x25E ints) */
extern int BrNetSlotGetF02C(int i);     /* 0x10004D80, re-enters the mutex */

/* WHAT IT DOES: read a slot's flag word and turn it into a small non-
 * negative number: keep the low six bits, subtract four, and clamp at zero.
 * Used where the caller wants a count or level rather than the raw flags. */
/* @implements 0x100062B0 glide BrNetSlotGetF02CBiased */
int BrNetSlotGetF02CBiased(int i)
{
    int v;

    WaitForSingleObject((HANDLE)(&DAT_1021ce58)[i * 0x25e], 0xffffffff);
    v = (BrNetSlotGetF02C(i) & 0x3f) - 4;
    ReleaseMutex((HANDLE)(&DAT_1021ce58)[i * 0x25e]);
    return (v > 0) ? v : 0;
}

/* Forward declarations for unknown functions/globals */
extern int DAT_1021ce58;
extern int DAT_1021d7cc;

/* WHAT IT DOES: read another per-slot counter under the slot's mutex,
 * clamped so a negative stored value reads as zero. */
/* @implements 0x10006300 glide BrNetSlotGetF974 */
int BrNetSlotGetF974(int param_1)

{
  int iVar1;
  
  WaitForSingleObject((HANDLE)(&DAT_1021ce58)[param_1 * 0x25e],0xffffffff);
  iVar1 = (&DAT_1021d7cc)[param_1 * 0x25e];
  ReleaseMutex((HANDLE)(&DAT_1021ce58)[param_1 * 0x25e]);
  if (iVar1 < 0) {
    iVar1 = 0;
  }
  return iVar1;
}

/* The original binary is /MD: CRT calls resolve through the import table. */
#define _CRTIMP __declspec(dllimport)
#include <stdio.h>

__declspec(dllimport) unsigned long __stdcall WaitForSingleObject(void *, unsigned long);
__declspec(dllimport) int __stdcall ReleaseMutex(void *);



extern BrNetSlot978 slots[16];       /* 0x1021CE58 */
extern int          g_a10221288[];   /* 0x1021CDB8 */
extern int          g_i10221318;     /* 0x1021CE48 */

int  BrNetSlotGetF004(int slot);
int  BrNetSlotGetF02C(int slot);
void BrNetSlotSetF02C(int slot, int value);
void BrNetAnnounce(const char *psz);

/* WHAT IT DOES: drop every network player matching a key -- returns their
 * slot to the free stack, clears their flags, and announces that they left
 * the game. This is what runs when a player disconnects or is kicked. */
/* @implements 0x10006350 glide BrNetDropMatching */
void BrNetDropMatching(int key)
{
    char szMsg[0x400];
    int  i;

    for (i = 0; i < 16; ++i) {
        if (BrNetSlotGetF004(i) != key)
            continue;
        /* `test al,0x3f` -- any of the low six flag bits. */
        if ((BrNetSlotGetF02C(i) & 0x3F) == 0)
            continue;

        WaitForSingleObject((void *)g_h1022AF30, 0xffffffff);
        g_i10221318 = g_i10221318 + 1;
        g_a10221288[g_i10221318] = i;
        ReleaseMutex((void *)g_h1022AF30);

        BrNetSlotSetF02C(i, 0);

        sprintf(szMsg, "%%15%s left the game.", slots[i].name);
        BrNetAnnounce(szMsg);
    }
}

/* Forward declarations for unknown functions/globals */
extern int DAT_1021c900;
extern int DAT_1021ce4c;

/* WHAT IT DOES: raise a shared network flag under its mutex. Paired with
 * BrNetClearF10220DD0. */
/* @implements 0x10006400 glide BrNetSetF10220DD0 */
void BrNetSetF10220DD0(void)

{
  WaitForSingleObject((void *)DAT_1021ce4c,0xffffffff);
  DAT_1021c900 = 1;
  ReleaseMutex((void *)DAT_1021ce4c);
  return;
}

/* Forward declarations for unknown functions/globals */
extern int DAT_1021c900;
extern int DAT_1021ce4c;

/* WHAT IT DOES: clear the flag BrNetSetF10220DD0 raises, under the same
 * mutex. */
/* @implements 0x10006430 glide BrNetClearF10220DD0 */
void BrNetClearF10220DD0(void)

{
  WaitForSingleObject((void *)DAT_1021ce4c,0xffffffff);
  DAT_1021c900 = 0;
  ReleaseMutex((void *)DAT_1021ce4c);
  return;
}

/* Forward declarations for unknown functions/globals */
extern int DAT_1021c81c;
extern int DAT_10226a30;
extern int DAT_10226a44;
int BrTicks30FromMs();

/* WHAT IT DOES: check whether a network timeout has expired and, if so, set
 * the flag that tells the rest of the game to give up waiting. Called from
 * the polling loop. */
/* @implements 0x100064D0 glide BrNetCheckDeadline */
void BrNetCheckDeadline(void)

{
  unsigned int uVar1;
  
  WaitForSingleObject((void *)DAT_1021c81c,0xffffffff);
  uVar1 = BrTicks30FromMs();
  if (uVar1 >= (unsigned int)DAT_10226a30) {
    DAT_10226a44 = 1;
  }
  ReleaseMutex((void *)DAT_1021c81c);
  return;
}

/* ------------------------------------------------------------------ */
/* 0x10004D80 -- slot field accessor                                  */
/* ------------------------------------------------------------------ */

/* Forward declarations for unknown functions/globals */
extern int DAT_1021ce58;
extern int DAT_1021ce84;

/* WHAT IT DOES: read one field out of a network player slot under that
 * slot's mutex. The slots are shared with the receive thread, so every read
 * is locked. */
/* @implements 0x10004D80 glide BrNetSlotGetF02C */
int BrNetSlotGetF02C(int param_1)

{
  int uVar1;
  
  WaitForSingleObject((HANDLE)(&DAT_1021ce58)[param_1 * 0x25e],0xffffffff);
  uVar1 = (&DAT_1021ce84)[param_1 * 0x25e];
  ReleaseMutex((HANDLE)(&DAT_1021ce58)[param_1 * 0x25e]);
  return uVar1;
}


typedef struct {
    int key;         /* what the ack quotes back */
    int tSent;       /* local time the ping went out */
} BrPingSlot;

extern BrPingSlot g_aBrPing[8];                /* 0x102265E0 .. 0x10226620 */
extern int        g_brPingHead;                /* 0x10226A68 */
extern unsigned   g_brPingBestRtt;             /* 0x10226A6C */
extern int        g_brClockOffset;             /* 0x1021C908 */

void BrTimeUpdate(void);                       /* 0x1006E360 */
int  BrGetTimerState(void);                    /* 0x1006E350 */

/* WHAT IT DOES: the net clock-sync ring.  An incoming ack that beat (or
 * tied) the best round-trip so far is matched against the eight outstanding
 * pings by key, and the matching ping sets the local-to-remote clock offset
 * from the remote's timestamp plus half the round-trip.  Then the new ping
 * (key, send time) is written into the ring at the head, which wraps at 8. */
/* @implements 0x10003810 glide BrNetPingSync */
void BrNetPingSync(int key, int tSent, int ackKey, unsigned rtt)
{
    BrPingSlot *p;

    if (rtt <= g_brPingBestRtt && rtt != 0) {
        for (p = g_aBrPing; (int)p < (int)&g_aBrPing[8]; p++) {
            if (p->key == ackKey) {
                int tRemote = (rtt >> 1) - p->tSent + tSent + p->key;
                BrTimeUpdate();
                g_brClockOffset = BrGetTimerState() - tRemote;
                g_brPingBestRtt = rtt;
            }
        }
    }
    g_aBrPing[g_brPingHead].key   = key;
    g_aBrPing[g_brPingHead].tSent = tSent;
    g_brPingHead++;
    if (g_brPingHead >= 8)
        g_brPingHead = 0;
}
