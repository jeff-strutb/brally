/* slice3_41.c -- Boss Rally (BRD3D.dll) slice 3, a later pass.
 *
 * See slice3_41.h for the packet inventory, the offsets that were recovered,
 * and the list of functions that were deliberately left out.
 *
 * Every x87 sequence in here was traced through its fxch chain; where the
 * original reads a status word twice and looks at different bits each time,
 * the C is written to reproduce that exactly (including what happens to a
 * NaN), not to look tidy.
 */
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "slice3_41.h"


/* =====================================================================
 * 1.  Driver records and the race-position sort
 * ===================================================================== */

/* The 8-byte element the original sorts.  `key` MUST come first: the
 * comparator dereferences the element pointer as a float directly. */
typedef struct BrRankPair {
    float   key;
    int32_t idx;
} BrRankPair;


/* The g_22AF18 == 0 half of 0x10066510. */
void BrRankAssign(BrDriver *pSlots, int32_t n)
{
    BrRankPair a[BR_RANK_MAX];
    int32_t    i, j, m = 0;

    for (i = 0; i < n; i++) {
        if ((pSlots[i].f68 & BR_DRIVER_SKIP) != 0)
            continue;

        /* DEVIATION: the original's pair buffer is a bare 0xA0-byte stack
         * array with no bound check, so a 21st participating slot smashes
         * the saved registers behind it.  Extra slots are dropped here
         * instead.  Everything at or below 20 participants is bit-identical. */
        if (m >= BR_RANK_MAX)
            break;

        a[m].idx = i;
        a[m].key = (pSlots[i].pCar != NULL) ? pSlots[i].pCar->fFF4
                                            : pSlots[i].f50;
        m++;
    }

    if (m != 0)
        qsort(a, (size_t)m, sizeof a[0], BrRankCmpKey);

    for (j = 0; j < m; j++) {
        BrDriver *pS = &pSlots[a[j].idx];

        /* Note `n`, not `m`: see the GOTCHA in the header. */
        if (pS->pCar != NULL)
            pS->pCar->fFF8 = n - j - 1;
        else
            pS->f54 = n - j - 1;
    }
}

/* =====================================================================
 * 2.  Variable-block save / restore
 * ===================================================================== */

/* 0x10067880 */
/* WHAT IT DOES: gathers a list of scattered game variables into one
 * contiguous block of memory -- the snapshot the replay and save-state code
 * works from. If the block turns out not to have been big enough it stops the
 * game with an error, but only after the overrun has already happened. */
/* @implements 0x10067880 d3d BrVarSave */
/* @n64 0x8022ADCC located */
void BrVarSave(const BrVarBlock *pTable, void *pDst, int32_t cbAvail)
{
    uint8_t *pOut = (uint8_t *)pDst;
    int32_t  cbUsed;

    while (pTable->pData != NULL) {
        memcpy(pOut, pTable->pData, (size_t)pTable->cb);
        pOut += pTable->cb;
        pTable++;
    }

    cbUsed = (int32_t)(pOut - (uint8_t *)pDst);
    if (cbUsed > cbAvail) {
        /* sprintf into an 80-byte stack buffer, as the original does --
         * confirmed against the N64 build (TGR USA 0x8022adcc), which
         * compiles the same source with plain sprintf + fatal. */
        char szMsg[0x50];
        sprintf(szMsg,
                "VAR SAVE OVERFLOW (%d avail, %d used)",
                (int)cbAvail, (int)cbUsed);
        BrFatal(szMsg);
    }
}

/* 0x10067900 */
/* WHAT IT DOES: puts a previously gathered snapshot back where it came from,
 * restoring every variable in the list. It trusts the buffer completely --
 * there is no length given and no check made. */
/* @implements 0x10067900 d3d BrVarLoad */
/* @n64 0x8022AE70 located */
void BrVarLoad(const BrVarBlock *pTable, const void *pSrc)
{
    const uint8_t *pIn = (const uint8_t *)pSrc;

    while (pTable->pData != NULL) {
        memcpy(pTable->pData, pIn, (size_t)pTable->cb);
        pIn += pTable->cb;
        pTable++;
    }
}


/* =====================================================================
 * 5.  Per-frame slot banks
 * ===================================================================== */

BrFrameBank g_BrPool16 = { NULL, 16, 20, 21, 0, 0 };    /* 0x100694E0 */
BrFrameBank g_BrPool32 = { NULL, 32, 20, 21, 0, 0 };    /* 0x10069530 */

BrPool *g_pBrPool64 = NULL;

void *BrFrameBankAlloc(BrFrameBank *pBank)
{
    int32_t  slot;
    uint8_t *p;

    /* Signed compare, as in the original (`jge`). */
    if (pBank->count < pBank->nUsable)
        slot = pBank->nBank * pBank->frame + pBank->count;
    else
        slot = pBank->nBank * pBank->frame + pBank->nUsable;

    p = pBank->pBase + (ptrdiff_t)slot * pBank->cbSlot;

    /* Incremented on BOTH paths, so the counter runs past the limit and
     * every late request in the frame aliases the same overflow slot. */
    pBank->count++;
    return p;
}

/* 0x100694E0 */
/* WHAT IT DOES: hands out one small scratch block that only has to last the
 * rest of the frame, from a pool that is thrown away wholesale at the end of
 * it. Once the pool is full every further request gets the same last block
 * back, so late callers quietly share one. */
/* @implements 0x100694E0 d3d BrPool16Alloc */
/* The original does not have BrFrameBankAlloc.  It hand-inlines the same body
 * into each allocator with that bank's four constants folded in -- usable
 * count, slots per frame, slot size, and the two array bases -- reading the
 * counter and the frame index as absolute globals rather than through a bank
 * pointer.  Calling a shared helper compiles to a 16-byte thunk against the
 * original's 77, so the matching build spells the body out per bank and the
 * port keeps the factored version below.
 *
 * The counter store must be written `= ++c`, not `= c + 1`.  They compute the
 * same number, but `c + 1` lets VC5 form the incremented value early
 * (`lea eax,[ecx+1]` and a store before the address arithmetic), which costs a
 * byte and changes the register picked for the index LEA; the pre-increment
 * keeps the count in its register across the address computation and emits the
 * original's trailing `inc ecx / mov [count],ecx`.  That one token is the whole
 * difference between 78 bytes and an exact 77. */
#ifdef BR_MATCHING_BUILD
extern int32_t BrG_6C65EC;      /* 0x106C65EC  frame parity, shared by all three */
extern int32_t BrG_B01C48;      /* 0x10B01C48  16-byte bank counter              */
extern uint8_t BrG_B02190[];    /* 0x10B02190  16-byte bank base                 */
extern uint8_t BrG_B022D0[];    /* 0x10B022D0  16-byte bank overflow slot        */
void *BrPool16Alloc(void)
{
    int32_t c = BrG_B01C48;
    if (c < 20) {
        uint8_t *p = &BrG_B02190[(BrG_6C65EC * 21 + c) * 16];
        BrG_B01C48 = ++c;
        return p;
    }
    BrG_B01C48 = ++c;
    return &BrG_B022D0[BrG_6C65EC * 21 * 16];
}
#else
void *BrPool16Alloc(void)
{
    return BrFrameBankAlloc(&g_BrPool16);
}
#endif

/* Glide 0x100625A0 == D3D 0x10069530 (shared.csv pair).  Tagged on the Glide
 * side -- the reference build, and the address 0x1000A110's specular pass
 * calls -- so claimcheck audits the body against it. */
/* WHAT IT DOES: hand out the next free 32-byte scratch block for this frame,
 * from a small per-frame bank that is thrown away wholesale rather than freed
 * item by item. Past the twentieth request every caller gets the SAME overflow
 * block -- the original does not fail, it just stops giving out distinct
 * memory, and callers are expected never to ask that often. BrPool16Alloc is
 * the same template with 16-byte slots. */
/* @implements 0x100625A0 glide BrPool32Alloc */
/* Same template as BrPool16Alloc, with 32-byte slots (`shl eax,5`) and its own
 * counter and two bases; see the note there for the `= ++c` requirement. */
#ifdef BR_MATCHING_BUILD
extern int32_t BrG_B01C44;      /* 0x10B01C44  32-byte bank counter       */
extern uint8_t BrG_B01C50[];    /* 0x10B01C50  32-byte bank base          */
extern uint8_t BrG_B01ED0[];    /* 0x10B01ED0  32-byte bank overflow slot */
void *BrPool32Alloc(void)
{
    int32_t c = BrG_B01C44;
    if (c < 20) {
        uint8_t *p = &BrG_B01C50[(BrG_6C65EC * 21 + c) * 32];
        BrG_B01C44 = ++c;
        return p;
    }
    BrG_B01C44 = ++c;
    return &BrG_B01ED0[BrG_6C65EC * 21 * 32];
}
#else
void *BrPool32Alloc(void)
{
    return BrFrameBankAlloc(&g_BrPool32);
}
#endif

/* Glide 0x100625F0: XOR EAX,EAX / MOV [BrG_B01C40],EAX /
 * MOV [BrG_B01C48],EAX / MOV [BrG_B01C44],EAX / RET (18 bytes, 3 relocs).
 * Order: a0, a8, a4 -- the 64-byte counter first, then 16, then 32.
 * D3D 0x10069580 clears a pool object instead; not byte-identical. */
/* WHAT IT DOES: throw away everything handed out of the three frame-scratch
 * pools, which is how they are emptied -- nothing is freed individually, the
 * counts simply go back to zero and the space is reused next frame. */
/* @implements 0x100625F0 glide BrGfx69580 */
#ifdef BR_MATCHING_BUILD
extern int32_t BrG_B01C40;      /* 0x10B24FA0  64-byte bank counter */
void BrGfx69580(void)
{
    BrG_B01C40 = 0;
    BrG_B01C48 = 0;
    BrG_B01C44 = 0;
}
#else
/* WHAT IT DOES: throws away everything handed out of the three frame-scratch
 * pools, which is how they are emptied -- nothing is freed individually, the
 * counts simply go back to zero and the space is reused. */
void BrGfx69580(void)
{
    /* DEVIATION: the original writes 0x10B01C40 directly.  That counter is
     * br_pool.h's BrPool::count, and br_pool.h exposes no global instance,
     * so it is reached through a integration-supplied pointer.  A NULL hook
     * simply skips it. */
    if (g_pBrPool64 != NULL)
        g_pBrPool64->count = 0;

    g_BrPool16.count = 0;
    g_BrPool32.count = 0;
    /* The frame index is untouched -- something else advances 0x106C65EC. */
}
#endif

/* ── Ghidra-matched functions ─────────────────────────── */
#ifdef BR_MATCHING_BUILD
void BrSndBankSetCar(int, int);
extern int DAT_100b32b0;
extern int DAT_100b32bc;
extern int DAT_100b32c0;
int FUN_1006c010();
int FUN_1006e1d0();
int FUN_1006f840();
extern int g_AC300;



/* WHAT IT DOES: copy a car position record and its trailing 3-vector. */
/* @implements 0x10062610 glide BrRacePosCopy */

int BrRacePosCopy(int param_1,int param_2)

{
  FUN_1006f840(param_2,param_1);
  *(int *)(param_1 + 0x10) = *(int *)(param_2 + 0x30);
  *(int *)(param_1 + 0x14) = *(int *)(param_2 + 0x34);
  *(int *)(param_1 + 0x18) = *(int *)(param_2 + 0x38);
  return;
}

/* WHAT IT DOES: free every entry of the driver's pointer array at +0x78 (count +0x7C),
 * then the array itself, and zero both fields. thiscall via BR_THISCALL1 (__fastcall). */
/* @implements 0x1005F530 glide BrDriverAssetsFree */

void __fastcall BrDriverAssetsFree(int param_1)

{
  int iVar1;
  
  iVar1 = 0;
  if (0 < *(int *)(param_1 + 0x7c)) {
    do {
      free(*(void **)(*(int *)(param_1 + 0x78) + iVar1 * 4));
      *(int *)(*(int *)(param_1 + 0x78) + iVar1 * 4) = 0;
      iVar1 = iVar1 + 1;
    } while (iVar1 < *(int *)(param_1 + 0x7c));
  }
  free(*(void **)(param_1 + 0x78));
  *(int *)(param_1 + 0x78) = 0;
  *(int *)(param_1 + 0x7c) = 0;
  return;
}

#endif /* BR_MATCHING_BUILD */
