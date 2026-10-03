/* br_dplaysend.c -- net.
 *
 * The outgoing DirectPlay senders: the locked Send wrapper every one of them
 * goes through, the tagged two-word payload they all build, and the named
 * wrappers around it.
 *
 * Filed out of the address batches: these functions were
 * matched first and grouped by what they are afterwards.
 * Every function carries its original address.
 */

/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include <stdlib.h>
#include <string.h>

#include "slice2_22.h"

#include "slice1_03.h"   /* BrComObj, BrComCallLocked68 (= 0x1000C4D0) */

/* XSLICE 0x10003580 */
extern void BrAppMsg107(void *pv1, const void *pData, uint32_t cbData,
                        uint32_t idFrom, int32_t a5);

/* Casting integers through void* for BrComCallLocked68's opaque parameters.
 * uintptr_t keeps that lossless on a 64-bit host. */
#include <stdint.h>
#define BR_ARG(v) ((void *)(uintptr_t)(uint32_t)(v))

/* 0x1000C4D0 is IDirectPlay4A::Send under a critical section; slice1_03 owns
 * it. This wrapper exists only so the six senders read like the original. */
/* WHAT IT DOES: sends a block of data to every other player in the network
 * game, through the networking library, taking the lock around the call. The
 * send is marked as guaranteed delivery. */
/* @implements 0x10009A00 glide BrDPlayRawSend */
/* @implements 0x1000C4D0 d3d BrDPlayRawSend */
/* Orig is 6-arg: EnterCS, stdcall Send at vtbl+0x68 with this pushed,
 * LeaveCS.  BrComCallLocked68 is a helper CALL (33 B vs 64 B). */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: EnterCriticalSection is declared by the platform headers */
/* 64-bit core: LeaveCriticalSection is declared by the platform headers */
typedef int (__stdcall *BrDpSend6)(void *, uint32_t, uint32_t, uint32_t,
                                   void *, uint32_t);
int BrDPlayRawSend(void *pIface, uint32_t idFrom, uint32_t idTo,
                          uint32_t flags, void *pData, uint32_t cbData)
{
    BrDpSend6 send;
    int r;
    EnterCriticalSection(&(g_BrDPlayCrit[0]));
    send = *(BrDpSend6 *)(&(*(void ***)pIface)[0x68 / 4]);
    r = send(pIface, idFrom, idTo, flags, pData, cbData);
    LeaveCriticalSection(&(g_BrDPlayCrit[0]));
    return r;
}

/* ==========================================================================
 * 4. 0x1003D950..0x1003DB50 -- the senders
 * ========================================================================== */

/* (port-only BrDPlaySendPair removed) */


/* (port-only BrDPlaySendTag2 removed) */


/* (port-only BrDPlaySendTag5 removed) */


/* (port-only BrDPlaySendTag4 removed) */


/* (port-only BrDPlaySendTag3 removed) */


/* @n64 0x8026C654 located */
/* (port-only BrDPlaySendTag6 removed) */


/* (port-only BrDPlaySendTag7 removed) */


/* (port-only BrDPlaySendTag6Self removed) */


/* (port-only BrDPlaySendTag8 removed) */


/* ==========================================================================
 * 5. 0x10004A40 / 0x10005140 -- the counted-state senders
 *
 * Both send a counted state object's payload (BrStateGetField10 / BrCounted-
 * Total, settings/br_state.c) to the other players.  When the link has its
 * local-echo flag (+0x0C) set the payload is also handed to the local message
 * path (0x1002F790) first.
 * ========================================================================== */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                               /* 0x10AC5BEC: net lock */
/* BrCountedTotal: prototype in br_funcs.h */
/* BrStateGetField10: prototype in br_funcs.h */
/* FUN_1002f790: prototype in br_funcs.h */

/* WHAT IT DOES: if the net-lock flag is set, just return the counted total;
 * otherwise echo the payload locally when the link asks for it and return,
 * or send it with DirectPlay (flags 1, guaranteed) and return the total on
 * success, -1 on failure. */
/* @implements 0x10004A40 glide BrCountedNetSend */
int BrCountedNetSend(BrDPlayCtx *param_1, void *param_2)
{
    int iVar2;

    if (DAT_10ac5bec != 0) {
        return BrCountedTotal(param_2);
    }
    if (param_1->f0C != 0) {
        FUN_1002f790((int *)param_1,
                     BrStateGetField10(param_2),
                     BrCountedTotal(param_2),
                     1, 1);
        return BrCountedTotal(param_2);
    }
    iVar2 = BrDPlayRawSend((void *)param_1->pDP, param_1->idPlayer, 1, 0,
                           (void *)BrStateGetField10(param_2),
                           BrCountedTotal(param_2));
    if (iVar2 == 0) {
        return BrCountedTotal(param_2);
    }
    return -1;
}

/* WHAT IT DOES: the same send with flags 0 (not guaranteed): returns the
 * counted total when the net lock is set or the send succeeds, -1 when it
 * fails; the local echo does not short-circuit here. */
/* @implements 0x10005140 glide BrNetTrySend */
int BrNetTrySend(BrDPlayCtx *param_1, void *param_2)
{
    int n;

    if (DAT_10ac5bec != 0)
        return BrCountedTotal(param_2);
    if (param_1->f0C != 0)
        FUN_1002f790((int *)param_1,
                     BrStateGetField10(param_2),
                     BrCountedTotal(param_2),
                     1, 1);
    n = BrDPlayRawSend((void *)param_1->pDP, param_1->idPlayer, 0, 0,
                       (void *)BrStateGetField10(param_2),
                       BrCountedTotal(param_2));
    if (n == 0)
        return BrCountedTotal(param_2);
    return -1;
}
