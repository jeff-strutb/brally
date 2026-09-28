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

#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
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
#ifdef BR_MATCHING_BUILD
/* Orig is 6-arg: EnterCS, stdcall Send at vtbl+0x68 with this pushed,
 * LeaveCS.  BrComCallLocked68 is a helper CALL (33 B vs 64 B). */
extern int DAT_10273310;
__declspec(dllimport) void __stdcall EnterCriticalSection(void *);
__declspec(dllimport) void __stdcall LeaveCriticalSection(void *);
typedef int (__stdcall *BrDpSend6)(void *, uint32_t, uint32_t, uint32_t,
                                   void *, uint32_t);
static int BrDPlayRawSend(void *pIface, uint32_t idFrom, uint32_t idTo,
                          uint32_t flags, void *pData, uint32_t cbData)
{
    BrDpSend6 send;
    int r;
    EnterCriticalSection(&DAT_10273310);
    send = *(BrDpSend6 *)(*(unsigned char **)pIface + 0x68);
    r = send(pIface, idFrom, idTo, flags, pData, cbData);
    LeaveCriticalSection(&DAT_10273310);
    return r;
}
#else
static int BrDPlayRawSend(void *pIface, uint32_t idFrom,
                          const void *pData, uint32_t cbData)
{
    return BrComCallLocked68((BrComObj *)pIface,
                             BR_ARG(idFrom),
                             BR_ARG(0),          /* DPID_ALLPLAYERS      */
                             BR_ARG(1),          /* DPSEND_GUARANTEED    */
                             (void *)(uintptr_t)(const void *)pData,
                             BR_ARG(cbData));
}
#endif

/* ==========================================================================
 * 4. 0x1003D950..0x1003DB50 -- the senders
 * ========================================================================== */

int BrDPlaySendPair(const BrDPlayLink *pLink, int32_t fGate,
                    uint32_t tag, uint32_t value)
{
    uint32_t aPayload[2];

    if (pLink == NULL)          return 0;
    if (pLink->pIface == NULL)  return 0;
    if (fGate != 0)             return 0;

    aPayload[0] = tag;
    aPayload[1] = value;
#ifdef BR_MATCHING_BUILD
    return BrDPlayRawSend(pLink->pIface, pLink->f08, 0, 1, aPayload,
                          (uint32_t)sizeof aPayload);
#else
    return BrDPlayRawSend(pLink->pIface, pLink->f08, aPayload,
                          (uint32_t)sizeof aPayload);
#endif
}

int BrDPlaySendTag2(const BrDPlayLink *pLink, int32_t fGate, uint32_t value)
{
    return BrDPlaySendPair(pLink, fGate, BR_DPLAY_TAG2, value);
}

int BrDPlaySendTag5(const BrDPlayLink *pLink, int32_t fGate, uint32_t value)
{
    return BrDPlaySendPair(pLink, fGate, BR_DPLAY_TAG5, value);
}

int BrDPlaySendTag4(const BrDPlayLink *pLink, int32_t fGate, uint32_t value)
{
    return BrDPlaySendPair(pLink, fGate, BR_DPLAY_TAG4, value);
}

int BrDPlaySendTag3(const BrDPlayLink *pLink, int32_t fGate)
{
    /* DEVIATION: the original leaves the second dword uninitialised. */
    return BrDPlaySendPair(pLink, fGate, BR_DPLAY_TAG3, 0u);
}

/* @n64 0x8026C654 located */
int BrDPlaySendTag6(const BrDPlayLink *pLink, uint32_t value)
{
    /* No gate test -- that is the whole difference from the four above. */
    return BrDPlaySendPair(pLink, 0, BR_DPLAY_TAG6, value);
}

int BrDPlaySendTag7(const BrDPlayLink *pLink, uint32_t value)
{
    return BrDPlaySendPair(pLink, 0, BR_DPLAY_TAG7, value);
}

void BrDPlaySendTag6Self(const BrDPlayLink *pLink)
{
    if (pLink == NULL)     return;
    if (pLink->f08 == 0)   return;
    (void)BrDPlaySendTag6(pLink, pLink->f08);
}

int BrDPlaySendTag8(const BrDPlayLink *pLink, uint32_t a, uint32_t b)
{
    uint32_t aPayload[3];

    if (pLink == NULL)          return 0;
    if (pLink->pIface == NULL)  return 0;
    /* no 0x10AA288C gate here */

    aPayload[0] = BR_DPLAY_TAG8;
    aPayload[1] = a;
    aPayload[2] = b;

    if (pLink->f0C != 0)
        BrAppMsg107((void *)(uintptr_t)(const void *)pLink, aPayload,
                    (uint32_t)sizeof aPayload, pLink->f08, 1);

#ifdef BR_MATCHING_BUILD
    return BrDPlayRawSend(pLink->pIface, pLink->f08, 0, 1, aPayload,
                          (uint32_t)sizeof aPayload);
#else
    return BrDPlayRawSend(pLink->pIface, pLink->f08, aPayload,
                          (uint32_t)sizeof aPayload);
#endif
}

/* ==========================================================================
 * 5. 0x10004A40 / 0x10005140 -- the counted-state senders
 *
 * Both send a counted state object's payload (BrStateGetField10 / BrCounted-
 * Total, settings/br_state.c) to the other players.  When the link has its
 * local-echo flag (+0x0C) set the payload is also handed to the local message
 * path (0x1002F790) first.
 * ========================================================================== */
#ifdef BR_MATCHING_BUILD
extern int DAT_10ac5bec;                               /* 0x10AC5BEC: net lock */
int __fastcall BrCountedTotal(void *pObj);             /* 0x1006D180 */
int __fastcall BrStateGetField10(void *pObj);          /* 0x1006D190 */
int FUN_1002f790(int *p, int a, int b, int c, int d);  /* 0x1002F790 */

/* WHAT IT DOES: if the net-lock flag is set, just return the counted total;
 * otherwise echo the payload locally when the link asks for it and return,
 * or send it with DirectPlay (flags 1, guaranteed) and return the total on
 * success, -1 on failure. */
/* @implements 0x10004A40 glide BrCountedNetSend */
int BrCountedNetSend(int *param_1, void *param_2)
{
    int iVar2;

    if (DAT_10ac5bec != 0) {
        return BrCountedTotal(param_2);
    }
    if (param_1[3] != 0) {
        FUN_1002f790(param_1,
                     BrStateGetField10(param_2),
                     BrCountedTotal(param_2),
                     1, 1);
        return BrCountedTotal(param_2);
    }
    iVar2 = BrDPlayRawSend((void *)param_1[0], param_1[2], 1, 0,
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
int BrNetTrySend(int *param_1, void *param_2)
{
    int n;

    if (DAT_10ac5bec != 0)
        return BrCountedTotal(param_2);
    if (param_1[3] != 0)
        FUN_1002f790(param_1,
                     BrStateGetField10(param_2),
                     BrCountedTotal(param_2),
                     1, 1);
    n = BrDPlayRawSend((void *)param_1[0], param_1[2], 0, 0,
                       (void *)BrStateGetField10(param_2),
                       BrCountedTotal(param_2));
    if (n == 0)
        return BrCountedTotal(param_2);
    return -1;
}
#endif
