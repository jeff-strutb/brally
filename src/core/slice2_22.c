/* slice2_22.c -- 0x1003BD50..0x1003DBC0 (DirectPlay module), portable part.
 * See slice2_22.h for the identification notes and every GOTCHA. */

#include <stdlib.h>
#include <string.h>

#include "slice2_22.h"

#include "slice1_03.h"   /* BrComObj, BrComCallLocked68 (= 0x1000C4D0) */
#include "slice1_06.h"   /* BrOptCaps,  BrOptAvailB      (= 0x1003F320) */

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
/* @implements 0x1000C4D0 d3d BrDPlayRawSend */
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

/* ==========================================================================
 * 1. 0x1003BD50
 * ========================================================================== */

uint32_t BrDPlayRandStep(uint32_t *pSeed)
{
    uint32_t s = *pSeed;

    /* shl/lea chain == *16807, then `and eax, 0x07FFFFFF`. The intermediate
     * 32-bit wrap is invisible: 2^32 is a multiple of 2^27. */
    s = (s * 16807u) & 0x07FFFFFFu;

    *pSeed = s;
    return s;
}

/* ==========================================================================
 * 2. 0x1003C430 / 0x1003CFC0 / 0x1003CFE0 -- the service-provider table
 * ========================================================================== */

int BrDPlaySpClassify(const uint8_t *pGuid,
                      const uint8_t aKnown[BR_DPLAY_SP_KNOWN][16])
{
    /* Compare order is the original's, and so is the index it assigns.
     * aKnown[] is in ADDRESS order (0x10090890, A0, B0, C0); note that
     * entries 2 and 3 do not map to rows 2 and 3. */
    if (memcmp(pGuid, aKnown[3], 16) == 0) return 2;   /* 0x100908C0 */
    if (memcmp(pGuid, aKnown[1], 16) == 0) return 1;   /* 0x100908A0 */
    if (memcmp(pGuid, aKnown[0], 16) == 0) return 0;   /* 0x10090890 */
    if (memcmp(pGuid, aKnown[2], 16) == 0) return 3;   /* 0x100908B0 */
    return -1;
}

int BrDPlaySpEnumConn(BrDPlaySp *aTable, const uint8_t *pGuid,
                      const uint8_t aKnown[BR_DPLAY_SP_KNOWN][16],
                      const char *pszName,
                      const void *pConn, uint32_t cbConn)
{
    BrDPlaySp *pRow;
    void      *pBlock;
    int        idx = BrDPlaySpClassify(pGuid, aKnown);

    if (idx < 0)
        return 1;               /* no match: nothing written, keep enumerating */

    pRow = &aTable[idx];

    /* DEVIATION: the original `rep movsd`s a flat 0xC8 bytes out of the
     * NUL-terminated short name, reading past its end. Bounded here. */
    {
        size_t n = 0;
        if (pszName != NULL) {
            n = strlen(pszName);
            if (n > BR_DPLAY_SP_NAMELEN) n = BR_DPLAY_SP_NAMELEN;
            memcpy(pRow->aName, pszName, n);
        }
        memset(pRow->aName + n, 0, BR_DPLAY_SP_NAMELEN - n);
    }

    memcpy(pRow->aGuid, pGuid, 16);

    /* DEVIATION: GlobalAlloc(GMEM_MOVEABLE|GMEM_ZEROINIT) + GlobalLock. */
    pBlock = (cbConn != 0) ? calloc(1, cbConn) : calloc(1, 1);
    pRow->pConn = pBlock;       /* stored before the null test, as in the
                                 * original -- so a failed alloc leaves a null
                                 * pointer next to a STALE cbConn */
    if (pBlock == NULL)
        return 1;

    if (cbConn != 0 && pConn != NULL)
        memcpy(pBlock, pConn, cbConn);
    pRow->cbConn = cbConn;
    return 1;
}

void BrDPlaySpFreeAll(BrDPlaySp *aTable)
{
    int i;
    for (i = 0; i < BR_DPLAY_SP_MAX; ++i) {
        if (aTable[i].pConn != NULL) {
            free(aTable[i].pConn);          /* DEVIATION: was GlobalUnlock +
                                             * GlobalFree */
            aTable[i].pConn = NULL;
        }
        /* cbConn deliberately left alone -- the original does not clear it. */
    }
}

int BrDPlaySpSelectedGuid(BrDPlaySp *aTable, uint32_t idxSel,
                          uint8_t **ppGuid)
{
    *ppGuid = aTable[idxSel].aGuid;   /* no bounds check in the original */
    return 0;
}

/* ==========================================================================
 * 3. 0x1003C9F0 / 0x1003CA70 / 0x1003CB24 -- the 8-slot mark-and-sweep
 * ========================================================================== */

void BrDPlaySlotTouch(BrSlotTable *pTable, int32_t id)
{
    int i;
    int idxFree = -1;           /* the original's `or bl,0xff`, signed byte */

    for (i = 0; i < BR_SLOT_COUNT; ++i) {
        if (pTable->aSlots[i].id == id) {
            pTable->aSlots[i].b = 1;
            return;
        }
        if (pTable->aSlots[i].id == BR_SLOT_EMPTY && idxFree < 0)
            idxFree = i;        /* FIRST free row only */
    }

    if (idxFree < 0)
        return;                 /* table full: silently does nothing */

    pTable->aSlots[idxFree].id = id;
    pTable->aSlots[idxFree].a  = (id == 1) ? 1 : 0;   /* id 1 is reserved */
    pTable->aSlots[idxFree].b  = 1;
}

void BrDPlaySlotsPurge(BrSlotTable *pTable)
{
    int i;
    for (i = 0; i < BR_SLOT_COUNT; ++i) {
        if (pTable->aSlots[i].b == 0) {
            pTable->aSlots[i].id = BR_SLOT_EMPTY;
            pTable->aSlots[i].a  = 0;
            pTable->aSlots[i].b  = 0;
        }
    }
}

void BrDPlaySlotsClearMarks(BrSlotTable *pTable)
{
    int i;
    for (i = 0; i < BR_SLOT_COUNT; ++i)
        pTable->aSlots[i].b = 0;
}

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
    return BrDPlayRawSend(pLink->pIface, pLink->f08, aPayload,
                          (uint32_t)sizeof aPayload);
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

    return BrDPlayRawSend(pLink->pIface, pLink->f08, aPayload,
                          (uint32_t)sizeof aPayload);
}

/* ==========================================================================
 * 5. 0x1003CE80 tail fragment
 * ========================================================================== */

void BrDPlayAdvanceAvail(const void *pCaps, int32_t *pIdx)
{
    const BrOptCaps *p = (const BrOptCaps *)pCaps;
    int32_t start = *pIdx;
    int32_t n;

    if (BrOptAvailB(p, (uint32_t)start) != 0)
        return;                                 /* start is already usable */

    for (;;) {
        n = *pIdx + 1;
        if (n > 0x1F)                           /* `cmp 0x1F / jle`: 0..31 */
            n = 0;
        *pIdx = n;
        if (n == start)
            return;                             /* wrapped: give up, index
                                                 * is left at `start` */
        if (BrOptAvailB(p, (uint32_t)n) != 0)
            return;
    }
}

/* ==========================================================================
 * 6. 0x1003D850 -- IDirectPlayLobby::EnumAddress callback
 * ========================================================================== */

#ifdef BR_MATCHING_BUILD
/* DPAID_Modem at 0x100909E0. */
extern unsigned char g_0909E0[16];

/* dllimport so the IAT pointer is hoisted into esi and called through it,
 * matching `mov esi, [0x118AE45C] / call esi`. */
__declspec(dllimport) int __stdcall lstrlenA(const char *lpString);

/* WHAT IT DOES: DirectPlay EnumAddress callback. When the current address
 * chunk is the modem-name list, walk that packed (double-NUL-terminated)
 * list of names and throw them away. Always tells the enumerator to keep
 * going. arg1 (chunk size) and arg3 (context) are unused; they stay in the
 * signature so the callee pops 16 bytes (`ret 0x10`). */
/* @implements 0x1003D850 d3d BrSub1003D850 */
int __stdcall BrSub1003D850(const void *pGuid, unsigned dwDataSize,
                            char *pList, void *pContext)
{
    char *p = pList;

    if (memcmp(pGuid, g_0909E0, 16) == 0) {
        if (lstrlenA(p) != 0) {
            do {
                p += lstrlenA(p) + 1;
            } while (lstrlenA(p) != 0);
        }
    }
    return 1;
}
#endif

/* ── Ghidra-matched functions ─────────────────────────── */
#ifdef BR_MATCHING_BUILD
int FUN_10037130();
extern int g_brPA9D008;

/* WHAT IT DOES: send DPlay message 0x60000006 to the local player recorded at +8 of the
 * network object, if one is live. */
/* @implements 0x10037180 glide BrDPlayMsg6SendSelf */

void BrDPlayMsg6SendSelf(void)

{
  if ((g_brPA9D008 != 0) && (*(int *)(g_brPA9D008 + 8) != 0)) {
    FUN_10037130(g_brPA9D008,*(int *)(g_brPA9D008 + 8));
  }
  return;
}

#endif /* BR_MATCHING_BUILD */
