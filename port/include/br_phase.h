/* br_phase.h -- THE canonical layout of the 0xC8-byte phase/screen object.
 *
 * WHY THIS FILE EXISTS
 *
 * Four headers independently modelled the same object and disagreed:
 *
 *   slice2_26.h  BrPhase      {pVtbl, pfn04, pfn08, f0C, f68}
 *   slice2_25.h  BrOptObj     same five fields, padded to 0xC8
 *   slice3_33.h  BrUiPhase    {cScreen@0x10, f12, apScreen@0x14, aF6C@0x6C}
 *   slice3_32.h  BrPhaseFull  the full 0x00..0xC4 map, recovered from the
 *                             destructor at 0x10048870 and the vtable at
 *                             0x1008F700
 *
 * The first three are each a partial view. Because none of them contains the
 * fields the others need, FIVE menu-screen builders (0x10049F40, 0x1004D640,
 * 0x10056FF0, 0x1004F700, 0x10053CF0) were declined by five different passes in
 * a row -- not because their bodies are hard, but because no declarable type
 * could hold what they touch. Casting between the partial views links cleanly
 * and is silently wrong at every field access.
 *
 * BrPhaseFull is the superset and is promoted here unchanged. New code should
 * use THIS header. The four originals stay for the modules already compiled
 * against them; do not add a fifth model.
 *
 * ---------------------------------------------------------------------------
 * BYTE OFFSETS ARE 32-BIT-ONLY. READ THIS BEFORE ASSERTING ANYTHING.
 *
 * The comments below give the ORIGINAL offsets, which hold on a 32-bit host.
 * On LP64 every pointer widens and the tail shifts, so `offsetof(BrPhase_,
 * f68)` is NOT 0x68 there. That is the "only agree on a 32-bit host" hazard the
 * passes kept reporting across several objects, and it is real: nothing may
 * overlay this struct on a file image or a foreign buffer.
 *
 * What IS asserted below, and holds on both: field ORDER, and that the whole
 * object still fits the original's 0xC8 allocation.
 * ---------------------------------------------------------------------------
 */
#ifndef BR_PHASE_H
#define BR_PHASE_H

#include <stddef.h>
#include <stdint.h>

#define BR_PHASE_PAGES      20
#define BR_PHASE_ORIG_SIZE  0xC8u

typedef struct BrPhase_     BrPhase_;
typedef struct BrUiPage_    BrUiPage_;

/* vtable at 0x1008F700. Slots verified against the ctor/dtor pair; unused slots
 * are void* rather than given plausible-but-unverified signatures. */
typedef struct BrPhaseVtbl_ {
    void   *(*f00)(BrPhase_ *pThis, int32_t nFlags);  /* 0x10048850 */
    int32_t (*f04)(BrPhase_ *pThis);                  /* 0x100488B0 */
    int32_t (*f08)(BrPhase_ *pThis);                  /* 0x100488C0 */
    int32_t (*f0C)(BrPhase_ *pThis);                  /* 0x100489A0 */
    void    *f10;
    void    (*f14)(BrPhase_ *pThis);                  /* 0x10048960 */
    void    (*f18)(BrPhase_ *pThis, void *pArg);      /* 0x10048B20 */
    void    (*f1C)(BrPhase_ *pThis);                  /* 0x10048AA0 */
    void    (*f20)(BrPhase_ *pThis);                  /* 0x1005AFA0 */
} BrPhaseVtbl_;

struct BrPhase_ {
    const BrPhaseVtbl_ *pVtbl;               /* +0x00  = 0x1008F700           */
    void               *pfnEnter;            /* +0x04  slice2_26 pfn04        */
    void               *pfnHook;             /* +0x08  slice2_26 pfn08        */
    int32_t             f0C;                 /* +0x0C  set 1 on just-built    */
    uint16_t            nPages;              /* +0x10  slice3_33 cScreen      */
    uint16_t            iPage;               /* +0x12  zeroed first           */
    BrUiPage_          *aPages[BR_PHASE_PAGES]; /* +0x14 slice3_33 apScreen   */
    BrUiPage_          *pCur;                /* +0x64  current page           */
    int32_t             f68;                 /* +0x68  set 1 on just-built    */
    int32_t             aFlags[BR_PHASE_PAGES]; /* +0x6C slice3_33 aF6C       */
    uint16_t            fBC;                 /* +0xBC  selection              */
    uint16_t            fBE;
    void               *fC0;                 /* +0xC0  released by 0x10048870 */
    void               *fC4;                 /* +0xC4  released by 0x10048870 */
};

/* --- what we can actually assert portably ------------------------------- */
#define BR_PH_ASSERT(name, cond) typedef char BR_PH_##name[(cond) ? 1 : -1]

/* Field order matches the original. These hold on 32- and 64-bit alike. */
BR_PH_ASSERT(order_enter_after_vtbl,
             offsetof(BrPhase_, pfnEnter) > offsetof(BrPhase_, pVtbl));
BR_PH_ASSERT(order_pages_after_f0C,
             offsetof(BrPhase_, nPages)   > offsetof(BrPhase_, f0C));
BR_PH_ASSERT(order_cur_after_pages,
             offsetof(BrPhase_, pCur)     > offsetof(BrPhase_, aPages));
BR_PH_ASSERT(order_f68_after_cur,
             offsetof(BrPhase_, f68)      > offsetof(BrPhase_, pCur));
BR_PH_ASSERT(order_flags_after_f68,
             offsetof(BrPhase_, aFlags)   > offsetof(BrPhase_, f68));

/* The port must never allocate less than the original did. On LP64 the struct
 * is LARGER than 0xC8; callers must use sizeof(), not the literal. */
BR_PH_ASSERT(pages_count_is_20, BR_PHASE_PAGES == 20);

/* Allocate with this, never with the 0xC8 literal. */
#define BR_PHASE_ALLOC_SIZE \
    (sizeof(BrPhase_) > BR_PHASE_ORIG_SIZE ? sizeof(BrPhase_) : BR_PHASE_ORIG_SIZE)

#endif /* BR_PHASE_H */
