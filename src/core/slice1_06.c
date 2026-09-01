/* slice1_06.c -- BRD3D.dll 0x10037030-0x1005D440, a later pass. See slice1_06.h.
 *
 * Constants quoted below were read straight out of orig/BRD3D.dll rather than
 * guessed: 0x1008F62C is 0.0f, the table at 0x100AC660 is nine 8-byte
 * records, and 0x1008F788 is a vtable whose first slot is 0x1005CBF0.
 *
 * CORRECTED: this banner used to say "0x1007DFE0 is calloc(n,1) (it tail-calls
 * 0x1007D370 with a second argument of 1)". IT IS NOT. 0x1007DFE0 is
 * `operator new` == `_nh_malloc(size, 1)`, and the literal 1 is nhFlag, not
 * calloc's element count:
 *
 *   0x1007D370(size, nhFlag) reads nhFlag into edi and uses it for NOTHING
 *   but `test edi,edi / je fail` around `call 0x10082ED0` (_callnewh) and a
 *   retry loop. The allocation is `push esi / call 0x1007D3C0` -- ONE
 *   argument -- and 0x1007D3C0 ends in `HeapAlloc(heap, 0, size)` with flags
 *   ZERO, not HEAP_ZERO_MEMORY.
 *
 * Nothing on that path zeroes. CONVENTIONS.md has always said so ("0x1007DFE0
 * is operator new (_nh_malloc(size,1)) and does not zero"); this file
 * contradicted it, BrUiAssetPathsInit below called calloc on the strength of
 * it, and test_slice1_06.c asserted the resulting zero tail as if it were the
 * original's behaviour. Found while transcribing the Glide twin of the same
 * function (Glide 0x10056260 -> port/src/drawing/br_uiimg.c), where the
 * allocator is the MSVCRT import thunk 0x10074572 -> ??2@YAPAXI@Z and the
 * absence of zeroing is not in doubt.
 */

#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
/* The original BrOptSave takes no arguments (loose globals in, packed
 * array out); hide the header's port prototype behind a rename so the
 * matching twin can define the real symbol -- the slice5_63.c caller keeps
 * the port signature (cdecl, extra args harmless at run time). */
#define BrOptSave   BrOptSave_hdr
#define BrOptAvailB BrOptAvailB_hdr
#include "slice1_06.h"
#undef BrOptSave
#undef BrOptAvailB
#else
#include "slice1_06.h"
#endif

#include <stdlib.h>
#include <string.h>

/* Layout facts the original's arithmetic depends on. */
typedef char br06_assert_pendlist[
    (offsetof(BrPendList, count) == BR_PENDLIST_MAX * sizeof(void *)) ? 1 : -1];
typedef char br06_assert_devrec[
    (sizeof(BrDevRec) == BR_DEVREC_STRIDE) ? 1 : -1];
typedef char br06_assert_namelist[
    (BR_NAMELIST_COUNT * BR_NAMELIST_STRIDE == 0x1964 * 4) ? 1 : -1];

/* ==========================================================================
 * 0x10037030
 * ========================================================================== */

/* WHAT IT DOES: puts one more item on a fixed-length waiting list. When the
 * list is already full the item is simply thrown away and a "dropped" tally is
 * bumped -- but the length still counts up, so once it overflows it never
 * agrees with what is actually stored again. What the items are is not
 * established here. */
#ifdef BR_MATCHING_BUILD
/* Original layout from the context pointer at 0x106C7C3C: items at +4,
 * count at +0x7C. BrPendList in the header starts at the items, so matching
 * uses a wrapper with the leading dword the original addresses through. */
typedef struct BrPendCtx {
    uint32_t unused0;
    void    *apItems[BR_PENDLIST_MAX];
    int32_t  count;
} BrPendCtx;

typedef char br06_assert_pendctx[
    (offsetof(BrPendCtx, count) == 0x7C) ? 1 : -1];

BrPendCtx *g_brPendCtx;     /* 0x106C7C3C */
uint32_t   g_brPendDropped; /* 0x106C7C40 */

/* WHAT IT DOES: queue a piece of work to run later.  If the queue is
 * already full the item is dropped, but the counter still moves on. */
/* @implements 0x10037030 d3d BrPendListAdd */
void BrPendListAdd(BrPendList *pList, void *pItem, uint32_t *pcDropped)
{
    BrPendCtx *p = g_brPendCtx;
    int32_t n = p->count;

    if (n < BR_PENDLIST_MAX) {
        p->apItems[n] = pList;
        /* Reload 0x106C7C3C before incrementing -- the original does.
         * RESIDUE (0+0 regnorm, T3a): pure eax/ecx rotation from the first
         * instruction -- the original loads the ctx into ecx (6-byte form)
         * and the count into eax; every spelling probed (direct derefs, CSE
         * count, cached p) loads ctx into eax.  30 masked diff bytes. */
        g_brPendCtx->count++;
    } else {
        g_brPendDropped++;
        p->count++;
    }
}
#else
/* WHAT IT DOES: queue a piece of work to run later.  If the queue is
 * already full the item is dropped, but the counter still moves on. */
/* port-only variant of BrPendListAdd (matching build uses the #ifdef branch above) */
void BrPendListAdd(BrPendList *pList, void *pItem, uint32_t *pcDropped)
{
    if (pList->count < BR_PENDLIST_MAX) {
        pList->apItems[pList->count] = pItem;
        /* The original re-loads the context pointer from 0x106C7C3C here
         * before incrementing -- irrelevant unless the callee moved it, and
         * there is no callee. */
        pList->count++;
        return;
    }

    /* Over capacity: drop the item, bump the global at 0x106C7C40, and STILL
     * increment the counter. */
    if (pcDropped != NULL) {          /* DEVIATION: the original's counter is
                                       * a fixed global and is never NULL. */
        (*pcDropped)++;
    }
    pList->count++;
}
#endif

/* ==========================================================================
 * 0x10037070
 * ========================================================================== */

/* WHAT IT DOES: answers whether any of a fixed set of records -- reached
 * through an index table rather than in order -- is both in use, of one
 * particular kind, and already holding the value being asked about. It reads
 * like a "is this already taken?" test for input-device assignments, but
 * nothing in this packet confirms that, so treat the purpose as unconfirmed. */
BrDevCtx *g_brP6EECCC;                      /* 0x106EECCC */

/* @implements 0x10037070 d3d BrDevRecMatch */
int BrDevRecMatch(uint32_t value)
{
    int32_t i;

    for (i = 0; i < BR_DEVREC_SLOTS; i++) {
        const BrDevRec *pRec = &g_brP6EECCC->pRecs[g_brP6EECCC->abIndex[i]];

        if (pRec->f04 == 0u) {
            continue;
        }
        if ((pRec->f20 & BR_DEVREC_TYPE_MASK) != BR_DEVREC_TYPE_MATCH) {
            continue;
        }
        if (pRec->f04 == value) {
            return 1;
        }
    }
    return 0;
}

/* ==========================================================================
 * 0x10037930
 * ========================================================================== */

/* WHAT IT DOES: looks a key up in a small table and hands back the pair of
 * values filed against it, reporting whether it found anything. The key is
 * shifted by the table's own offset first, and the search runs from the end
 * backwards, so where a key appears twice the later entry wins. */
BrKeyEnt g_aBrKeyEnts[BR_KEYTABLE_MAX];  /* 0x106EEF0C */
int32_t  g_brKeyCount;                   /* 0x10AC0808 */
uint32_t g_brKeyBias;                    /* 0x10AC080C */

/* @implements 0x10037930 d3d BrKeyTableFind */
int BrKeyTableFind(uint32_t key, uint32_t *pA, uint32_t *pB)
{
    uint32_t want = key + g_brKeyBias;
    int32_t  i    = g_brKeyCount - 1;

    /* RESIDUE (1+1 regnorm, T3a): the key loads into a different register
     * from the first instruction (FIRSTDIV +0x1), which turns the bias
     * `add edx,eax` into a lea; destructive `key +=` spelling identical.
     * Same first-load coloring class as BrPendListAdd. */

    /* `dec eax / test eax,eax / jl` -- count == 0 leaves i == -1 and the
     * whole loop is skipped. */
    while (i >= 0) {
        if (want == g_aBrKeyEnts[i].key) {
            *pA = g_aBrKeyEnts[i].a;
            *pB = g_aBrKeyEnts[i].b;
            return 1;
        }
        i--;
    }
    return 0;
}

/* ==========================================================================
 * 0x1003B940
 * ========================================================================== */

/* The threshold is the float at 0x1008F62C, which is 0.0f. The original
 * compares with `fcomp` and then tests C0 alone, so "not less than" is the
 * accepting condition and an unordered compare (NaN) sets C0 and rejects. */
#define BR06_TRI_EPS 0.0f

/* @implements 0x1003B940 d3d BrTriContainsPoint */
int BrTriContainsPoint(const BrVec3 *pPt, const BrVec3 *pA, const BrVec3 *pB,
                       const BrVec3 *pC, const BrVec3 *pRef)
{
    /* SIX distinct Vec3 locals (frame 0x48), not three reused ones: a
     * fresh edge per test, a shared n, and two point-deltas.  Reusing
     * edge/toPt shrinks the frame to 0x24 and shifts every slot. */
    BrVec3 n, toPtB, edge1, edge2, edge3, toPtA;

    /* Edge A->B, paired with pPt - pB. */
    BrVec3Sub(&edge1, pB, pA);
    BrVec3Sub(&toPtB, pPt, pB);
    BrVec3Cross(&n, &edge1, &toPtB);
    if (!(BrVec3Dot(&n, pRef) >= BR06_TRI_EPS)) {
        return 0;
    }

    /* Edge B->C, paired with the same pPt - pB kept live. */
    BrVec3Sub(&edge2, pC, pB);
    BrVec3Cross(&n, &edge2, &toPtB);
    if (!(BrVec3Dot(&n, pRef) >= BR06_TRI_EPS)) {
        return 0;
    }

    /* Edge C->A, paired with pPt - pA. */
    BrVec3Sub(&edge3, pA, pC);
    BrVec3Sub(&toPtA, pPt, pA);
    BrVec3Cross(&n, &edge3, &toPtA);
    if (!(BrVec3Dot(&n, pRef) >= BR06_TRI_EPS)) {
        return 0;
    }

    return 1;
}

/* ==========================================================================
 * 0x1003D180
 * ========================================================================== */

/* WHAT IT DOES: fetches a piece of information from a system object whose size
 * is not known in advance -- it asks once with no buffer to be told how big the
 * answer is, allocates that much, then asks again to have it filled in, and
 * hands the block to the caller. On any failure it releases the block and
 * reports the error instead. */
#ifdef BR_MATCHING_BUILD
/* COM methods are stdcall; the header's BrComGetFn is cdecl. A local
 * stdcall typedef is what removes the `add esp, 10h` the cdecl form emits
 * after each call. __declspec(dllimport) is what emits `call dword ptr
 * [IAT]` rather than a direct `call` thunk. */
typedef int32_t (__stdcall *BrComGetFnStd)(void *pThis, void *pParam,
                                           void *pvBuf, uint32_t *pcb);

__declspec(dllimport) void *__stdcall GlobalAlloc(unsigned uFlags,
                                                  unsigned dwBytes);
__declspec(dllimport) void *__stdcall GlobalLock(void *hMem);
__declspec(dllimport) void *__stdcall GlobalHandle(void *pMem);
__declspec(dllimport) int   __stdcall GlobalUnlock(void *hMem);
__declspec(dllimport) void *__stdcall GlobalFree(void *hMem);

/* WHAT IT DOES: fetch a blob whose size is not known in advance -- ask
 * once with no buffer, allocate that much, ask again, hand the block
 * back.  On failure the block is released and the error is reported. */
/* @implements 0x1003D180 d3d BrComGetAlloc */
int32_t BrComGetAlloc(BrDPlayObj *pObj, void *pParam, void **ppvOut)
{
    BrComGetFnStd pfn = (BrComGetFnStd)pObj->pVtbl->pfnGet;
    uint32_t      cb;
    void         *pv = NULL;
    int32_t       hr;

    /* cb is uninitialised: the original reuses pObj's stack slot as the size
     * out-param and never stores 0 into it. */
    /* Layout matches the source order -- [call1][alloc][OOM, jmp cleanup]
     * [call2, jge success][cleanup][success].  The OOM arm ends in an
     * explicit `goto cleanup` and the second call is the FALLTHROUGH
     * continuation, not an else arm (an if/else makes VC5 move the call2
     * block out-of-line past the cleanup); the success block lives after
     * the cleanup via its own label. */
    /* do-while(0) with break, NOT structured if-nesting: the loop body
     * stays contiguous, which is what recovers the original's size and its
     * OOM-arm `jmp` over the second call.
     * RESIDUE (1+1 regnorm, 53 masked B, T3a-layout): the original lays the
     * OOM arm INLINE (test;jne over it, `mov esi,OOM; jmp cleanup`) with
     * the second call after; every probed spelling (nested if/else, three
     * goto flattenings, else-arm, mixed break/goto) makes VC5 thread the
     * jump and move one arm out-of-line -- one je-vs-jne with the OOM arm
     * at the end. */
    do {
        hr = pfn(pObj, pParam, NULL, &cb);
        if (hr != BR_COM_E_BUFFERTOOSMALL)
            break;
        /* GMEM_MOVEABLE|GMEM_ZEROINIT == 0x42. */
        pv = GlobalLock(GlobalAlloc(0x42u, cb));
        if (pv == NULL) {
            hr = BR_COM_E_OUTOFMEMORY;
            goto cleanup;
        }
        hr = pfn(pObj, pParam, pv, &cb);
        if (hr >= 0)
            goto success;
    } while (0);

cleanup:

    if (pv != NULL) {
        GlobalUnlock(GlobalHandle(pv));
        GlobalFree(GlobalHandle(pv));
    }
    return hr;

success:
    *ppvOut = pv;
    return 0;       /* the original discards hr here */
}
#else
/* WHAT IT DOES: the same two-step fetch, using calloc instead of a
 * Windows global heap block. */
/* port-only variant of BrComGetAlloc (matching build uses the #ifdef branch above) */
int32_t BrComGetAlloc(BrDPlayObj *pObj, void *pParam, void **ppvOut)
{
    BrComGetFn pfn = pObj->pVtbl->pfnGet;
    uint32_t   cb  = 0;
    void      *pv  = NULL;
    int32_t    hr;

    hr = pfn(pObj, pParam, NULL, &cb);
    if (hr == BR_COM_E_BUFFERTOOSMALL) {
        /* DEVIATION: GlobalAlloc(GMEM_MOVEABLE|GMEM_ZEROINIT, cb) followed by
         * GlobalLock -> calloc; GlobalUnlock(GlobalHandle(p)) +
         * GlobalFree(GlobalHandle(p)) -> free. The original's caller
         * therefore receives a locked global handle's base pointer, not a CRT
         * allocation; anything that later passes it back to GlobalFree has to
         * be adjusted alongside this.
         *
         * Corner case, not reproduced: GlobalAlloc(GMEM_MOVEABLE, 0) yields a
         * handle to a discarded object and the following GlobalLock returns
         * NULL, so the original turns a zero-size result into
         * E_OUTOFMEMORY. calloc has no such rule; the substitution below
         * succeeds instead. No caller in this range asks for zero. */
        pv = calloc(cb ? cb : 1u, 1u);
        if (pv == NULL) {
            hr = BR_COM_E_OUTOFMEMORY;
        } else {
            hr = pfn(pObj, pParam, pv, &cb);
            if (hr >= 0) {
                *ppvOut = pv;
                return 0;   /* the original discards hr here */
            }
        }
    }

    if (pv != NULL) {
        free(pv);
    }
    return hr;
}
#endif

/* ==========================================================================
 * 0x1003E1D0
 * ========================================================================== */

/* WHAT IT DOES: wipes a pair of scratch buffers back to zeros, pointing each
 * at its own built-in storage first if it has not been given anywhere else to
 * live. What the buffers hold is not established here. */
#ifdef BR_MATCHING_BUILD
uint32_t *g_brPairA;                         /* 0x10ACED34 */
uint32_t *g_brPairB;                         /* 0x10AD189C */
uint32_t  g_brPairStaticA[BR_PAIRBUF_DWORDS]; /* 0x10AF9890 */
uint32_t  g_brPairStaticB[BR_PAIRBUF_DWORDS]; /* 0x10AF99DC */

/* WHAT IT DOES: wipe a pair of scratch buffers back to zeros, pointing
 * each at its own built-in storage first if it has nowhere else to live. */
/* @implements 0x1003E1D0 d3d BrPairBufReset */
int BrPairBufReset(BrPairBuf *pBuf)
{
    uint32_t *p;

    p = g_brPairA;
    if (p == NULL) {
        p = g_brPairStaticA;
        g_brPairA = p;
    }
    memset(p, 0, BR_PAIRBUF_DWORDS * sizeof(uint32_t));

    p = g_brPairB;
    if (p == NULL) {
        p = g_brPairStaticB;
        g_brPairB = p;
    }
    memset(p, 0, BR_PAIRBUF_DWORDS * sizeof(uint32_t));

    return 1;
}
#else
/* WHAT IT DOES: wipe a pair of scratch buffers back to zeros, pointing
 * each at its own built-in storage first if it has nowhere else to live. */
/* port-only variant of BrPairBufReset (matching build uses the #ifdef branch above) */
int BrPairBufReset(BrPairBuf *pBuf)
{
    if (pBuf->pA == NULL) {
        pBuf->pA = pBuf->aStaticA;
    }
    memset(pBuf->pA, 0, BR_PAIRBUF_DWORDS * sizeof(uint32_t));

    if (pBuf->pB == NULL) {
        pBuf->pB = pBuf->aStaticB;
    }
    memset(pBuf->pB, 0, BR_PAIRBUF_DWORDS * sizeof(uint32_t));

    return 1;
}
#endif

/* ==========================================================================
 * 0x1003E260
 * ========================================================================== */

/* Read from 0x100AC660. Record 8's fFatal is 0xFFFFFFFF, not 1 -- the
 * original only tests it against zero, so any non-zero value is "fatal". */
const BrErrEnt g_aBrErrTable[BR_ERR_COUNT] = {
    { 1,          162u },
    { 1,          163u },
    { 0,          164u },
    { 0,          165u },
    { 1,          166u },
    { 0,          167u },
    { 1,          168u },
    { 1,          169u },
    { (int32_t)0xFFFFFFFF, 256u }
};

void BrErrShow(const BrErrHost *pHost, int32_t idx)
{
    const char *pszText;
    const char *pszCaption;

    /* The original only rejects idx > 8. DEVIATION: the lower bound is added
     * here, because a negative index reads outside g_aBrErrTable. */
    if (idx > (BR_ERR_COUNT - 1) || idx < 0) {
        return;
    }

    pszText    = pHost->pfnLookup(pHost->pUser, g_aBrErrTable[idx].idText);
    pszCaption = pHost->pfnLookup(pHost->pUser, BR_ERR_CAPTION_ID);

    /* The `inc edi` in the original. DEVIATION: guarded, because the lookup
     * returns NULL for an out-of-range handle and the original would hand
     * MessageBoxA the pointer value 1. */
    if (pszText != NULL) {
        pszText++;
    }

    pHost->pfnMessage(pHost->pUser, pszText, pszCaption, 0u);

    if (g_aBrErrTable[idx].fFatal != 0) {
        pHost->pfnExit(pHost->pUser, 1);
    }
}

/* ==========================================================================
 * 0x1003E310
 * ========================================================================== */

/* WHAT IT DOES: takes a snapshot of twelve of the game's option settings into
 * one block, so they can be put back later. The values come from two separate
 * places and are interleaved in a fixed order that is neither array's order --
 * the shuffle is the whole content of the function. */
/* @implements 0x1003E310 d3d BrOptSave */
#ifdef BR_MATCHING_BUILD
/* Twelve loose globals into the packed scratch array, in this exact source
 * order -- the three-ahead load/store interleave is the scheduler's. */
extern int32_t g_br0AC648, g_br0AC64C, g_br0AC650, g_br0AC654, g_br0AC658,
               g_br0AC65C;                    /* slice2_25.c */
extern int32_t g_brAA2A00, g_brAA2A08, g_brAA2A0C, g_brAA2A18;
extern int32_t g_brAA2A10, g_brAA2A14;       /* slice6_70.c */
extern int32_t g_aBrB4E710[BR_OPT_SCRATCH_COUNT];   /* slice5_63.c */

void BrOptSave(void)
{
    g_aBrB4E710[0]  = g_br0AC648;
    g_aBrB4E710[1]  = g_brAA2A00;
    g_aBrB4E710[2]  = g_brAA2A08;
    g_aBrB4E710[3]  = g_br0AC64C;
    g_aBrB4E710[4]  = g_br0AC650;
    g_aBrB4E710[5]  = g_br0AC654;
    g_aBrB4E710[6]  = g_brAA2A0C;
    g_aBrB4E710[7]  = g_br0AC658;
    g_aBrB4E710[8]  = g_brAA2A10;
    g_aBrB4E710[9]  = g_brAA2A14;
    g_aBrB4E710[10] = g_br0AC65C;
    g_aBrB4E710[11] = g_brAA2A18;
}
#else
void BrOptSave(BrOptScratch *pDst, const BrOptState *pSrc)
{
    pDst->a[0]  = pSrc->aCfg[0];   /* 0x100AC648 -> 0x10B4E710 */
    pDst->a[1]  = pSrc->aSel[0];   /* 0x10AA2A00 -> 0x10B4E714 */
    pDst->a[2]  = pSrc->aSel[2];   /* 0x10AA2A08 -> 0x10B4E718 */
    pDst->a[3]  = pSrc->aCfg[1];   /* 0x100AC64C -> 0x10B4E71C */
    pDst->a[4]  = pSrc->aCfg[2];   /* 0x100AC650 -> 0x10B4E720 */
    pDst->a[5]  = pSrc->aCfg[3];   /* 0x100AC654 -> 0x10B4E724 */
    pDst->a[6]  = pSrc->aSel[3];   /* 0x10AA2A0C -> 0x10B4E728 */
    pDst->a[7]  = pSrc->aCfg[4];   /* 0x100AC658 -> 0x10B4E72C */
    pDst->a[8]  = pSrc->aSel[4];   /* 0x10AA2A10 -> 0x10B4E730 */
    pDst->a[9]  = pSrc->aSel[5];   /* 0x10AA2A14 -> 0x10B4E734 */
    pDst->a[10] = pSrc->aCfg[5];   /* 0x100AC65C -> 0x10B4E738 */
    pDst->a[11] = pSrc->aSel[6];   /* 0x10AA2A18 -> 0x10B4E73C */
}
#endif

/* ==========================================================================
 * 0x1003F2B0
 * ========================================================================== */

/* DEVIATION: `shl eax,cl` masks the count to 5 bits on x86; `1u << n` with
 * n >= 32 is undefined in C. The mask is applied explicitly so the C matches
 * the hardware. */
#define BR06_BIT(n) (1u << ((unsigned)(n) & 31u))

int32_t BrOptAvailA(const BrOptCaps *pCaps, uint32_t n)
{
    if (n == 12u) {
        return 0;
    }
    if (pCaps->fForceAvailA != 0) {
        return 1;
    }

    if (pCaps->mode == 0) {
        if (pCaps->fAlt != 0) {
            /* zero-extended word load from 0x10AA27E2, the HIGH half */
            return (int32_t)(BR06_BIT(n) & ((pCaps->maskPair >> 16) & 0xFFFFu));
        }
        return (int32_t)(BR06_BIT(n) & pCaps->maskA);
    }

    if (pCaps->fLowAlwaysB != 0 && (n == 14u || n == 13u)) {
        return 1;
    }
    return (int32_t)(BR06_BIT(n) & pCaps->maskAMode);
}

/* ==========================================================================
 * 0x1003F320
 * ========================================================================== */

/* WHAT IT DOES: decides whether one particular option is offered to the player
 * or shown unavailable, by testing its number against a bitmask of what the
 * machine and the current mode support. Which mask applies depends on the mode,
 * and there are several special cases -- some modes fold a high number back
 * into the low range, some declare everything below sixteen always available,
 * and one number is remapped to a different one entirely. Its sibling above
 * answers the same question for the other family of options. */
/* @implements 0x1003F320 d3d BrOptAvailB */
#ifdef BR_MATCHING_BUILD
/* One argument; every input is a loose global (fAlt and maskPair are each
 * loaded ONCE and live in registers across the whole function).  Raw
 * `1u << idx` (x86 masks the count in hardware; BR06_BIT's explicit &31
 * emits four real ANDs).
 * RESIDUE (1+1 regnorm, T3a-encoding): the FIRST `idx -= 16` emits
 * add ecx,-0x10 where the original has sub ecx,0x10 -- the same
 * context-dependent add/sub fork proven on BrHudDraw and BrOptCycleTrack;
 * `idx = idx - 16` identical.  The
 * mode-0 arm splits on fAlt FIRST and duplicates the idx-fixup and
 * fLowAlways test into both sub-arms -- the shared-logic form is 45 bytes
 * short. */
extern int32_t g_br6EE1DC_fRebaseB;      /* 0x10AC5C4C */
extern int32_t g_br6EE184_fAlt;          /* 0x10AC5BF4 */
extern int32_t g_br6EE0C8_maskPair;      /* 0x10AC5B38 */
extern int32_t g_br0A9360_mode;          /* 0x100A9360 */
extern int32_t g_br6EE1D8_fLowAlways;    /* 0x10AC5C48 */
extern int32_t g_br6EDE80_maskB;         /* 0x10AC58F0 */
extern int32_t g_brAAB88_maskB6;         /* 0x100AAB88 */
extern int32_t g_brAF3CE4_nAlwaysB;      /* 0x10AF3CE4 */
extern int16_t g_brAAB84_maskBDef;       /* 0x100AAB84 */

int32_t BrOptAvailB(uint32_t n)
{
    int32_t idx = (int32_t)n;

    if (g_br6EE1DC_fRebaseB != 0 && idx > 15)
        idx -= 16;
    if (g_br6EE184_fAlt != 0 && idx > 15
        && (g_br6EE0C8_maskPair & 0x8000) != 0)
        idx -= 16;

    if (g_br0A9360_mode == 0) {
        if (g_br6EE184_fAlt != 0) {
            if (idx == 15)
                idx = 11;
            if (g_br6EE1D8_fLowAlways != 0 && idx <= 15)
                return 1;
            /* `and esi,0xFFFF` -- the LOW half of the same dword */
            return (int32_t)((1u << idx)
                             & ((uint32_t)g_br6EE0C8_maskPair & 0xFFFFu));
        }
        if (idx == 15)
            idx = 11;
        if (g_br6EE1D8_fLowAlways != 0 && idx <= 15)
            return 1;
        return (int32_t)((1u << idx) & (uint32_t)g_br6EDE80_maskB);
    }

    if (g_br0A9360_mode == 6) {
        if (idx == 15)
            idx = 7;            /* 7 here, 11 everywhere else */
        if (g_br6EE1D8_fLowAlways != 0 && idx <= 15)
            return 1;
        return (int32_t)((1u << idx) & (uint32_t)g_brAAB88_maskB6);
    }

    if (g_br0A9360_mode == 2 && idx == g_brAF3CE4_nAlwaysB)
        return 1;

    if (idx == 15)
        idx = 11;
    if (g_br6EE1D8_fLowAlways != 0 && idx <= 15)
        return 1;
    /* movsx: SIGN-extended, unlike the zero-extended masks above. */
    return (int32_t)((1u << idx) & (uint32_t)(int32_t)g_brAAB84_maskBDef);
}
#else
int32_t BrOptAvailB(const BrOptCaps *pCaps, uint32_t n)
{
    int32_t idx = (int32_t)n;

    /* Both `jle`/`jg` in the original are signed. */
    if (pCaps->fRebaseB != 0 && idx > 15) {
        idx -= 16;
    }
    if (pCaps->fAlt != 0 && idx > 15 && (pCaps->maskPair & 0x8000u) != 0u) {
        idx -= 16;
    }

    if (pCaps->mode == 0) {
        if (idx == 15) {
            idx = 11;
        }
        if (pCaps->fLowAlways != 0 && idx <= 15) {
            return 1;
        }
        if (pCaps->fAlt != 0) {
            /* `and esi,0xFFFF` -- the LOW half of the same dword */
            return (int32_t)(BR06_BIT(idx) & (pCaps->maskPair & 0xFFFFu));
        }
        return (int32_t)(BR06_BIT(idx) & pCaps->maskB);
    }

    if (pCaps->mode == 6) {
        if (idx == 15) {
            idx = 7;            /* 7 here, 11 everywhere else */
        }
        if (pCaps->fLowAlways != 0 && idx <= 15) {
            return 1;
        }
        return (int32_t)(BR06_BIT(idx) & pCaps->maskBMode6);
    }

    if (pCaps->mode == 2 && idx == pCaps->nAlwaysB) {
        return 1;
    }

    if (idx == 15) {
        idx = 11;
    }
    if (pCaps->fLowAlways != 0 && idx <= 15) {
        return 1;
    }
    /* `movsx ecx, word ptr [0x100AB3E4]` -- SIGN-extended, unlike the
     * zero-extended masks above. */
    return (int32_t)(BR06_BIT(idx) & (uint32_t)(int32_t)pCaps->maskBDefault);
}
#endif

/* ==========================================================================
 * 0x1005CB90
 * ========================================================================== */

/* WHAT IT DOES: sets up a list of a hundred name slots and writes the same
 * starting text into every one of them, so an unused slot reads as something
 * rather than as blank. */
/* @implements 0x1005CB90 d3d BrNameListInit */
BrNameList *BrNameListInit(BrNameList *pThis, const void *pVtbl,
                           const char *pszFill)
{
    int i;

    pThis->pVtbl = pVtbl;
    memset(pThis->asz, 0, sizeof(pThis->asz));

    /* The original re-reads the source string (and re-runs strlen on it) on
     * every one of the 100 iterations. */
    for (i = 0; i < BR_NAMELIST_COUNT; i++) {
        size_t cb = strlen(pszFill) + 1u;

        /* DEVIATION: the original copies strlen+1 bytes with no bound. A
         * source longer than 0x103 characters overruns into the next slot.
         * Truncated here. */
        if (cb > BR_NAMELIST_STRIDE) {
            cb = BR_NAMELIST_STRIDE;
        }
        memcpy(pThis->asz[i], pszFill, cb);
        pThis->asz[i][BR_NAMELIST_STRIDE - 1] = '\0';
    }

    return pThis;
}

/* ==========================================================================
 * 0x1005D440 (partial)
 * ==========================================================================
 * What is NOT here: after the 145 paths the original fills two more fixed
 * buffers with the save-file paths below, sets 0xB4 dwords at 0x10A9D778 to
 * -1 and 0x64 dwords at 0x10A9E1D0 to 0, zeroes the first dword of each
 * 12-byte record in 0x10A9D780..0x10A9DA50, then constructs two objects
 * (0xC8 and 0x400 bytes, via 0x10048710 and 0x10008B70) whose types are
 * defined outside this range, reporting error index 1 through 0x1003E260 if
 * the second fails. That tail is left out: none of those types are
 * recoverable from this packet.
 */

const char *const g_pszBrRallySeasonDat = "c:\\RallySeason.dat";
const char *const g_pszBrRallyGhostDat  = "c:\\RallyGhost.dat";

const char *const g_apszBrUiAssets[BR_UIASSET_COUNT] = {
    /*   0  ptr @ 0x10A9E3D0 */ "images\\work1a.bmp",
    /*   1  ptr @ 0x10A9E444 */ "images\\cursor.bmp",
    /*   2  ptr @ 0x10A9E4B8 */ "images\\type_gry.bmp",
    /*   3  ptr @ 0x10A9E52C */ "images\\type_wit.bmp",
    /*   4  ptr @ 0x10A9E5A0 */ "images\\type_mid.bmp",
    /*   5  ptr @ 0x10A9E614 */ "images\\bignums.bmp",
    /*   6  ptr @ 0x10A9E688 */ "images\\champ.bmp",
    /*   7  ptr @ 0x10A9E6FC */ "images\\mp.bmp",
    /*   8  ptr @ 0x10A9E770 */ "images\\ta.bmp",
    /*   9  ptr @ 0x10A9E7E4 */ "images\\op.bmp",
    /*  10  ptr @ 0x10A9E858 */ "images\\qr.bmp",
    /*  11  ptr @ 0x10A9E8CC */ "images\\carmt.bmp",
    /*  12  ptr @ 0x10A9E940 */ "images\\mshft.bmp",
    /*  13  ptr @ 0x10A9E9B4 */ "images\\softshok.bmp",
    /*  14  ptr @ 0x10A9EA28 */ "images\\medshok.bmp",
    /*  15  ptr @ 0x10A9EA9C */ "images\\hardshok.bmp",
    /*  16  ptr @ 0x10A9EB10 */ "images\\desrttrk.bmp",
    /*  17  ptr @ 0x10A9EB84 */ "images\\coasttrk.bmp",
    /*  18  ptr @ 0x10A9EBF8 */ "images\\trakc.bmp",
    /*  19  ptr @ 0x10A9EC6C */ "images\\fog.bmp",
    /*  20  ptr @ 0x10A9ECE0 */ "images\\nite.bmp",
    /*  21  ptr @ 0x10A9ED54 */ "images\\rain.bmp",
    /*  22  ptr @ 0x10A9EDC8 */ "images\\snow.bmp",
    /*  23  ptr @ 0x10A9EE3C */ "images\\sunweth.bmp",
    /*  24  ptr @ 0x10A9EEB0 */ "images\\drytire.bmp",
    /*  25  ptr @ 0x10A9EF24 */ "images\\medtire.bmp",
    /*  26  ptr @ 0x10A9EF98 */ "images\\wettire.bmp",
    /*  27  ptr @ 0x10A9F00C */ "images\\trakd.bmp",
    /*  28  ptr @ 0x10A9F080 */ "images\\trake.bmp",
    /*  29  ptr @ 0x10A9F0F4 */ "images\\ashft.bmp",
    /*  30  ptr @ 0x10A9F168 */ "images\\carTR.bmp",
    /*  31  ptr @ 0x10A9F1DC */ "images\\carCE.bmp",
    /*  32  ptr @ 0x10A9F250 */ "images\\carCU.bmp",
    /*  33  ptr @ 0x10A9F2C4 */ "images\\carES.bmp",
    /*  34  ptr @ 0x10A9F338 */ "images\\carFH.bmp",
    /*  35  ptr @ 0x10A9F3AC */ "images\\carIP.bmp",
    /*  36  ptr @ 0x10A9F420 */ "images\\carLD.bmp",
    /*  37  ptr @ 0x10A9F494 */ "images\\carM3.bmp",
    /*  38  ptr @ 0x10A9F508 */ "images\\carMN.bmp",
    /*  39  ptr @ 0x10A9F57C */ "images\\carNS.bmp",
    /*  40  ptr @ 0x10A9F5F0 */ "images\\carPJ.bmp",
    /*  41  ptr @ 0x10A9F664 */ "images\\carPS.bmp",
    /*  42  ptr @ 0x10A9F6D8 */ "images\\carRS.bmp",
    /*  43  ptr @ 0x10A9F74C */ "images\\carSP.bmp",
    /*  44  ptr @ 0x10A9F7C0 */ "images\\carBB.bmp",
    /*  45  ptr @ 0x10A9F834 */ "images\\arrowdd.bmp",
    /*  46  ptr @ 0x10A9F8A8 */ "images\\arrowdu.bmp",
    /*  47  ptr @ 0x10A9F91C */ "images\\arrowud.bmp",
    /*  48  ptr @ 0x10A9F990 */ "images\\arrowuu.bmp",
    /*  49  ptr @ 0x10A9FA04 */ "images\\joystk.bmp",
    /*  50  ptr @ 0x10A9FA78 */ "images\\keybd.bmp",
    /*  51  ptr @ 0x10A9FAEC */ "images\\steerinp.bmp",
    /*  52  ptr @ 0x10A9FB60 */ "images\\type_yel.bmp",
    /*  53  ptr @ 0x10A9FBD4 */ "images\\steerarr.bmp",
    /*  54  ptr @ 0x10A9FC48 */ "images\\steeradj.bmp",
    /*  55  ptr @ 0x10A9FCBC */ "images\\spkr.bmp",
    /*  56  ptr @ 0x10A9FD30 */ "images\\monitr.bmp",
    /*  57  ptr @ 0x10A9FDA4 */ "images\\namebar.bmp",
    /*  58  ptr @ 0x10A9FE18 */ "images\\slidebox.bmp",
    /*  59  ptr @ 0x10A9FE8C */ "images\\boxtile2.bmp",
    /*  60  ptr @ 0x10A9FF00 */ "images\\rboxend.bmp",
    /*  61  ptr @ 0x10A9FF74 */ "images\\lboxend.bmp",
    /*  62  ptr @ 0x10A9FFE8 */ "images\\trakc_.bmp",
    /*  63  ptr @ 0x10AA005C */ "images\\trakd_.bmp",
    /*  64  ptr @ 0x10AA00D0 */ "images\\trake_.bmp",
    /*  65  ptr @ 0x10AA0144 */ "images\\desrttr_.bmp",
    /*  66  ptr @ 0x10AA01B8 */ "images\\coasttr_.bmp",
    /*  67  ptr @ 0x10AA022C */ "images\\mpmodem.bmp",
    /*  68  ptr @ 0x10AA02A0 */ "images\\mptcpip.bmp",
    /*  69  ptr @ 0x10AA0314 */ "images\\mpipx.bmp",
    /*  70  ptr @ 0x10AA0388 */ "images\\mpserial.bmp",
    /*  71  ptr @ 0x10AA03FC */ "images\\seasn2a.bmp",
    /*  72  ptr @ 0x10AA0470 */ "images\\seasn2b.bmp",
    /*  73  ptr @ 0x10AA04E4 */ "images\\seasn3a.bmp",
    /*  74  ptr @ 0x10AA0558 */ "images\\seasn3b.bmp",
    /*  75  ptr @ 0x10AA05CC */ "images\\seasn4a.bmp",
    /*  76  ptr @ 0x10AA0640 */ "images\\seasn4b.bmp",
    /*  77  ptr @ 0x10AA06B4 */ "images\\seasn5a.bmp",
    /*  78  ptr @ 0x10AA0728 */ "images\\bgdim.bmp",
    /*  79  ptr @ 0x10AA079C */ "images\\congrat.bmp",
    /*  80  ptr @ 0x10AA0810 */ "images\\noadv1.bmp",
    /*  81  ptr @ 0x10AA0884 */ "images\\noadv2.bmp",
    /*  82  ptr @ 0x10AA08F8 */ "images\\but-main.bmp",
    /*  83  ptr @ 0x10AA096C */ "images\\but-maind.bmp",
    /*  84  ptr @ 0x10AA09E0 */ "images\\but-op.bmp",
    /*  85  ptr @ 0x10AA0A54 */ "images\\but-opd.bmp",
    /*  86  ptr @ 0x10AA0AC8 */ "images\\cars1a.bmp",
    /*  87  ptr @ 0x10AA0B3C */ "images\\cars2a.bmp",
    /*  88  ptr @ 0x10AA0BB0 */ "images\\cars2b.bmp",
    /*  89  ptr @ 0x10AA0C24 */ "images\\cars3a.bmp",
    /*  90  ptr @ 0x10AA0C98 */ "images\\cars3b.bmp",
    /*  91  ptr @ 0x10AA0D0C */ "images\\cars4a.bmp",
    /*  92  ptr @ 0x10AA0D80 */ "images\\cars4b.bmp",
    /*  93  ptr @ 0x10AA0DF4 */ "images\\cars5a.bmp",
    /*  94  ptr @ 0x10AA0E68 */ "images\\cars5b.bmp",
    /*  95  ptr @ 0x10AA0EDC */ "images\\chatbar2.bmp",
    /*  96  ptr @ 0x10AA0F50 */ "images\\mousinpt.bmp",
    /*  97  ptr @ 0x10AA0FC4 */ "images\\ffstick.bmp",
    /*  98  ptr @ 0x10AA1038 */ "images\\carwnoshad2.bmp",
    /*  99  ptr @ 0x10AA10AC */ "images\\carwshad2.bmp",
    /* 100  ptr @ 0x10AA1120 */ "images\\specoff.bmp",
    /* 101  ptr @ 0x10AA1194 */ "images\\specon.bmp",
    /* 102  ptr @ 0x10AA1208 */ "images\\noffstik.bmp",
    /* 103  ptr @ 0x10AA127C */ "images\\listbox.bmp",
    /* 104  ptr @ 0x10AA12F0 */ "images\\engsound.bmp",
    /* 105  ptr @ 0x10AA1364 */ "images\\music.bmp",
    /* 106  ptr @ 0x10AA13D8 */ "images\\soundtik.bmp",
    /* 107  ptr @ 0x10AA144C */ "images\\soundptr.bmp",
    /* 108  ptr @ 0x10AA14C0 */ "images\\trrwd.bmp",
    /* 109  ptr @ 0x10AA1534 */ "images\\pjrwd.bmp",
    /* 110  ptr @ 0x10AA15A8 */ "images\\mnrwd.bmp",
    /* 111  ptr @ 0x10AA161C */ "images\\mirrwd.bmp",
    /* 112  ptr @ 0x10AA1690 */ "images\\bbrwd.bmp",
    /* 113  ptr @ 0x10AA1704 */ "images\\curwd.bmp",
    /* 114  ptr @ 0x10AA1778 */ "images\\fbrwd.bmp",
    /* 115  ptr @ 0x10AA17EC */ "images\\mtrwd.bmp",
    /* 116  ptr @ 0x10AA1860 */ "images\\sndlevl2.bmp",
    /* 117  ptr @ 0x10AA18D4 */ "images\\sndlevl3.bmp",
    /* 118  ptr @ 0x10AA1948 */ "images\\trakraceb.bmp",
    /* 119  ptr @ 0x10AA19BC */ "images\\trakracel.bmp",
    /* 120  ptr @ 0x10AA1A30 */ "images\\but-sav.bmp",
    /* 121  ptr @ 0x10AA1AA4 */ "images\\but-savd.bmp",
    /* 122  ptr @ 0x10AA1B18 */ "images\\z-carMT.bmp",
    /* 123  ptr @ 0x10AA1B8C */ "images\\z-carTR.bmp",
    /* 124  ptr @ 0x10AA1C00 */ "images\\z-carCE.bmp",
    /* 125  ptr @ 0x10AA1C74 */ "images\\z-carCU.bmp",
    /* 126  ptr @ 0x10AA1CE8 */ "images\\z-carES.bmp",
    /* 127  ptr @ 0x10AA1D5C */ "images\\z-carFH.bmp",
    /* 128  ptr @ 0x10AA1DD0 */ "images\\z-carIP.bmp",
    /* 129  ptr @ 0x10AA1E44 */ "images\\z-carLD.bmp",
    /* 130  ptr @ 0x10AA1EB8 */ "images\\z-carM3.bmp",
    /* 131  ptr @ 0x10AA1F2C */ "images\\z-carMN.bmp",
    /* 132  ptr @ 0x10AA1FA0 */ "images\\z-carNS.bmp",
    /* 133  ptr @ 0x10AA2014 */ "images\\z-carPJ.bmp",
    /* 134  ptr @ 0x10AA2088 */ "images\\z-carPS.bmp",
    /* 135  ptr @ 0x10AA20FC */ "images\\z-carRS.bmp",
    /* 136  ptr @ 0x10AA2170 */ "images\\z-carSP.bmp",
    /* 137  ptr @ 0x10AA21E4 */ "images\\z-carBB.bmp",
    /* 138  ptr @ 0x10AA2258 */ "images\\lightr.bmp",
    /* 139  ptr @ 0x10AA22CC */ "images\\lightg.bmp",
    /* 140  ptr @ 0x10AA2340 */ "images\\tire2on.bmp",
    /* 141  ptr @ 0x10AA23B4 */ "images\\tire2off.bmp",
    /* 142  ptr @ 0x10AA2428 */ "images\\listbox2.bmp",
    /* 143  ptr @ 0x10AA249C */ "images\\trakQ.bmp",
    /* 144  ptr @ 0x10AA2510 */ "images\\trakQ_.bmp",
};

int BrUiAssetPathsInit(char *apszOut[BR_UIASSET_COUNT])
{
    int i;

    for (i = 0; i < BR_UIASSET_COUNT; i++) {
        apszOut[i] = NULL;
    }

    for (i = 0; i < BR_UIASSET_COUNT; i++) {
        /* malloc, NOT calloc: 0x1007DFE0 is operator new and does not zero,
         * so everything past the NUL is whatever the heap held. See the
         * correction in this file's banner. */
        char *p = (char *)malloc(BR_UIASSET_PATH_MAX);

        /* DEVIATION: the original stores the allocation and copies into it
         * without checking for NULL. */
        if (p == NULL) {
            BrUiAssetPathsFree(apszOut);
            return -1;
        }
        /* Every path is far shorter than 0x104; strcpy is what the original
         * inlines, and the length is a compile-time property of the table. */
        strcpy(p, g_apszBrUiAssets[i]);
        apszOut[i] = p;
    }
    return 0;
}

void BrUiAssetPathsFree(char *apszOut[BR_UIASSET_COUNT])
{
    int i;

    for (i = 0; i < BR_UIASSET_COUNT; i++) {
        free(apszOut[i]);
        apszOut[i] = NULL;
    }
}

/* ==========================================================================
 * 0x1005CB40
 * ========================================================================== */

#ifdef BR_MATCHING_BUILD
/* Original is thiscall: `this` in ecx, one stack argument, `ret 4`.  VC5 C
 * has no __thiscall keyword; __fastcall puts the first REGISTER-ELIGIBLE
 * argument in ecx, and a struct is never register-eligible, so a 4-byte
 * struct in second position is forced onto the stack.  Same split as
 * thiscall.  Both virtual calls below use the same trick:
 *   vt+8    thiscall(this, arg)         ecx still holds this from entry
 *   vt+0x1C thiscall(this, &scratch)    lea ecx, [scratch] / push ecx /
 *                                       mov ecx, this
 * scratch is the one stack slot (`push ecx` at entry), seeded -1, and is
 * what the function returns. */

typedef struct { uint32_t v; } BrSub1005CB40Arg;
typedef struct { uint32_t *p; } BrSub1005CB40Ref;

typedef struct BrSub1005CB40Vtbl {
    void *f00;
    void *f04;
    void (__fastcall *f08)(void *pThis, BrSub1005CB40Arg a);
    void *f0C;
    void *f10;
    void *f14;
    void *f18;
    void (__fastcall *f1C)(void *pThis, BrSub1005CB40Ref a);
} BrSub1005CB40Vtbl;

typedef struct BrSub1005CB40Obj {
    const BrSub1005CB40Vtbl *pVtbl;
} BrSub1005CB40Obj;

int32_t  g_AA28D8;   /* 0x10AA28D8 */
int32_t  g_AA2858;   /* 0x10AA2858 */
uint16_t g_AA2870;   /* 0x10AA2870 */

/* WHAT IT DOES: always forwards the incoming value through vtable slot +8,
 * then -- only when 0x10AA28D8 and 0x10AA2858 are both clear -- asks slot
 * +0x1C to write an answer over a dword that starts at -1. A 16-bit counter
 * at 0x10AA2870 is incremented either way, and the dword is returned. What
 * the two slots do with the value is not established here. */
/* @implements 0x1005CB40 d3d BrSub1005CB40 */
uint32_t __fastcall BrSub1005CB40(BrSub1005CB40Obj *pThis, BrSub1005CB40Arg arg)
{
    uint32_t scratch;
    const BrSub1005CB40Vtbl *pVtbl;
    BrSub1005CB40Ref out;

    scratch = 0xFFFFFFFFu;
    pVtbl = pThis->pVtbl;
    pVtbl->f08((void *)pThis, arg);
    if (g_AA28D8 == 0) {
        if (g_AA2858 == 0) {
            out.p = &scratch;
            pVtbl->f1C((void *)pThis, out);
        }
    }
    ++g_AA2870;
    return scratch;
}
#endif

/* ── Ghidra-matched functions ─────────────────────────── */
#ifdef BR_MATCHING_BUILD
int operator_delete();
typedef int (*funcptr)();
#include <windows.h>
extern int DAT_10ac5a48;
extern int DAT_10ac5a4c;
extern funcptr PTR_FUN_10077750;

/* WHAT IT DOES: set the HUD race-position icon from the current standings and AI difficulty. */
/* @implements 0x10038980 glide BrRacePosIconSet */

int BrRacePosIconSet(int param_1)

{
  if (0 < DAT_10ac5a48) {
    switch(DAT_10ac5a48) {
    case 1:
      *(short *)(param_1 + 0x1e20c) = 0x73;
      break;
    case 2:
      *(short *)(param_1 + 0x1e20c) = 0x72;
      break;
    case 3:
      *(short *)(param_1 + 0x1e20c) = 0x71;
      break;
    case 4:
      *(short *)(param_1 + 0x1e20c) = 0x70;
      break;
    case 5:
      *(short *)(param_1 + 0x1e20c) = 0x6f;
      break;
    default:
      *(short *)(param_1 + 0x1e20c) = 0xffff;
    }
  }
  if (DAT_10ac5a48 == 0) {
    switch(DAT_10ac5a4c & 0xff) {
    case 1:
      *(short *)(param_1 + 0x1e20c) = 0x47;
      return 1;
    case 2:
      *(short *)(param_1 + 0x1e20c) = 0x49;
      return 1;
    case 3:
      *(short *)(param_1 + 0x1e20c) = 0x4b;
      return 1;
    case 4:
    case 5:
    case 6:
      *(short *)(param_1 + 0x1e20c) = 0x4d;
      return 1;
    default:
      *(short *)(param_1 + 0x1e20c) = 0xffff;
    }
  }
  return 1;
}

/* WHAT IT DOES: vtable constructor: install the function-pointer table at PTR_FUN_10077750 (fastcall). */
/* @implements 0x10055A30 glide BrVtInit55A30 */

int __fastcall BrVtInit55A30(int *param_1)

{
  *param_1 = &PTR_FUN_10077750;
  return;
}

/* WHAT IT DOES: C++ scalar deleting destructor for the 0x10077750-vtable object: run the
 * destructor body, then operator delete if bit 0 of the flags is set. thiscall, spelled
 * as __fastcall with an unused EDX slot (BR_THISCALL1 idiom). */
/* @implements 0x10055A10 glide BrVt55A10DeleteDtor */

void * __fastcall BrVt55A10DeleteDtor(void *param_1,int _edx_unused,unsigned char param_2)
{
  BrVtInit55A30((int *)param_1);
  if ((param_2 & 1) != 0) {
    operator_delete(param_1);
  }
  return param_1;
}

#endif /* BR_MATCHING_BUILD */
