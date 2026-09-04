/* br_framepool.c -- gamedata: the per-frame slot banks.
 *
 * Three banks of fixed-size scratch blocks handed out for the rest of the
 * frame and thrown away wholesale at the end of it. Past the usable count
 * every further request gets the same overflow block back -- the original
 * does not fail, it stops handing out distinct memory. Filed out of
 * slice3_41.c section 5.
 *
 * br_pool.c holds the portable reading of the 64-byte bank's allocator.
 *
 * See slice3_41.h for the recovered layouts.
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

/* ==================================================================== */
/* 6. 0x10069490 -- adapter over br_pool.c                              */
/* ==================================================================== */

BrPool g_brPool10069490;

/* WHAT IT DOES: hands out one scratch transform from the frame pool, for a
 * caller that needs somewhere to build a position-and-facing that only has to
 * survive the rest of this frame. */
/* @implements 0x10069490 d3d BrSub_10069490 */
/* Third instance of the frame-bank template that slice3_41.c's BrPool16Alloc
 * and BrPool32Alloc carry -- the original hand-inlines the allocator into each
 * bank with that bank's constants folded, rather than calling a shared helper.
 * This one is the 64-byte bank: 256 usable, 257 slots per frame, and its own
 * counter and two bases.  The `= ++c` on the counter is required for the same
 * reason as there; `c + 1` costs a byte and moves a register. */
#ifdef BR_MATCHING_BUILD
extern int32_t BrG_6C65EC;      /* 0x106C65EC  frame parity               */
extern int32_t BrG_B01C40;      /* 0x10B01C40  64-byte bank counter       */
extern uint8_t BrG_AF9BC0[];    /* 0x10AF9BC0  64-byte bank base          */
extern uint8_t BrG_AFDBC0[];    /* 0x10AFDBC0  64-byte bank overflow slot */
BrMat4 *BrSub_10069490(void)
{
    int32_t c = BrG_B01C40;
    if (c < 256) {
        uint8_t *p = &BrG_AF9BC0[(BrG_6C65EC * 257 + c) * 64];
        BrG_B01C40 = ++c;
        return (BrMat4 *)p;
    }
    BrG_B01C40 = ++c;
    return (BrMat4 *)&BrG_AFDBC0[BrG_6C65EC * 257 * 64];
}
#else
BrMat4 *BrSub_10069490(void)
{
    return (BrMat4 *)BrPoolAlloc(&g_brPool10069490);
}
#endif
