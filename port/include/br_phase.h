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
 * ADJUDICATED: the array holds 20 entries, not 22.
 *
 * slice3_33.h models the same memory as `apScreen[22]` at +0x14 followed by
 * `aF6C[22]` at +0x6C. That is internally consistent, so it cannot be dismissed
 * on shape alone -- but it makes +0x64 and +0x68 into screen POINTERS, and
 * slice2_26 independently establishes +0x68 as an int32 the phase-activate
 * routines set to 1 on the just-built path. A pointer slot is not written with
 * a literal 1 by three separate activate routines.
 *
 * So: 20 entries, +0x64 is the current-page pointer, +0x68 is that flag. An
 * over-read of two entries is the more likely error, and it is the reading that
 * two independent sources (the destructor at 0x10048870 and slice2_26's
 * activate paths) agree on.
 *
 * CONFIRMED INDEPENDENTLY, after the fact: the constructor at 0x10048710 ends
 * with `mov ecx,0x14 / xor eax,eax / lea edi,[ebx+0x6c] / rep stosd` -- TWENTY
 * dwords at +0x6C, landing on 0x6C + 20*4 == 0xBC, which is the very next
 * field (fBC, the word the same constructor zeroes at 0x1004874A). The 20 was
 * adjudicated above from the destructor and slice2_26's activate paths before
 * the constructor was read; the constructor agrees to the byte. 22 entries
 * would have run to 0xC4 and clobbered fBC, fC0 and fC4 -- and fC0/fC4 are the
 * two BrNameLists this same constructor allocates eight instructions earlier.
 *
 * ---------------------------------------------------------------------------
 * ADJUDICATED: slice2_26.h's `BrPhase` IS this object, and is now an alias.
 *
 * slice2_26.h declared a FIVE-field partial view {pVtbl, pfn04, pfn08, f0C,
 * f68} of this same allocation, and slice3_31.c calls through it. That was
 * survivable only while the real constructor was unwired, because the fifth
 * member `f68` sits where THIS model has nPages/iPage/aPages[0]: a constructor
 * writing a BrPhase_ into an object its caller reads as BrPhase puts every
 * field from +0x0C on somewhere else. It is the same hazard CONVENTIONS.md
 * records for BrUiScreen vs BrUiPage_, one level up.
 *
 * slice2_26.h now typedefs `BrPhase` to `BrPhase_` and `BrPhaseVtbl` to
 * `BrPhaseVtbl_` rather than defining rival structs. Nothing about the five
 * fields it named was wrong -- they are pVtbl, pfnEnter, pfnHook, f0C and f68
 * here, under the names br_phase.h already mapped -- it simply had no way to
 * express the other eight, and a partial view is only safe while nobody
 * writes the whole object.
 *
 * WRONG IF: +0x68 is not the flag slice2_26 says it is. Three activate
 * routines store a literal 1 there and the constructor stores a literal 1
 * there (0x10048743). Both models agree about that field; they disagree only
 * about what lies between +0x0C and +0x68, and only this one has an answer.
 *
 * ---------------------------------------------------------------------------
 * ADJUDICATED: f00 returns a pointer, and the vtable really is NINE slots.
 *
 * slice2_26.h typed slot +0x00 `void (*)(BrPhase *, int32_t)`. It returns a
 * value: 0x10048850 is the standard MSVC scalar deleting destructor --
 *
 *     push esi / mov esi,ecx / call 0x10048870      ; the real destructor
 *     test byte ptr [esp+8],1 / je  ...
 *     push esi / call 0x1007DE40 / add esp,4        ; operator delete
 *     mov eax,esi / pop esi / ret 4                 ; returns THIS
 *
 * `mov eax,esi` before `ret 4` is a returned pointer, and `ret 4` confirms
 * __thiscall with the one int32 flags argument. Every observed caller
 * discards the result, which is why a void model survived; `void *` is the
 * safe superset of the two readings, exactly as br_ui.h's ADJ-8 took the
 * int32 return over the void one for the control hooks.
 *
 * Nine slots, not one and not twelve. Read out of the image at 0x1008F700:
 *
 *     +0x00 0x10048850   +0x0C 0x100489A0   +0x18 0x10048B20
 *     +0x04 0x100488B0   +0x10 0x1005AE70   +0x1C 0x10048AA0
 *     +0x08 0x100488C0   +0x14 0x10048960   +0x20 0x1005AFA0
 *     +0x24 0x00000000  <-- NULL: the table ends here
 *     +0x28 0x1005B0A0   +0x2C 0x1005B0D0   <-- BrTextBox's, a different class
 *
 * slice3_31.h reached slot +0x1C through a `BrPhaseVtblExt` overlay whose
 * first member was the one-slot BrPhaseVtbl followed by six reserved pointers.
 * That overlay was correct arithmetic over the one-slot model and becomes a
 * WILD READ the moment BrPhaseVtbl is the nine-slot table -- its `f1C` would
 * land at +0x1C past the END of the real vtable. It is deleted, not adjusted:
 * slot +0x1C is a member of this struct and is reached directly.
 *
 * WRONG IF: 0x1008F700+0x24 is a slot that legitimately holds NULL rather than
 * the end of the table. Nothing calls it, so nothing depends on the answer;
 * the two entries after it belong to BrTextBox (0x1005B0A0 / 0x1005B0D0 are
 * the methods br_ui.h's ADJ-2 derives BrTextBox's own offsets from).
 *
 * ---------------------------------------------------------------------------
 * REMAINING WORK, and this header does NOT fix it: the screen/control objects
 * themselves (`BrUiScreen`, `BrUiCtl` in slice3_33.h) are still a separate
 * conflict. Their `f34`/`f38` slots take a phase pointer typed to slice3_33's
 * model, and several build hooks are absent from `BrUiBuildHooks`. Four screen
 * builders (0x1004D1F0, 0x1004DB00, 0x10053CF0, 0x10058750) remain blocked on
 * ONE merged build-context, not on this header.
 * ---------------------------------------------------------------------------
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
/* slice2_26.h defines the same constant with the same value; whichever header
 * a TU sees first wins, and an assertion below catches any disagreement rather
 * than letting the guard hide it. */
#ifndef BR_PHASE_ORIG_SIZE
#define BR_PHASE_ORIG_SIZE  0xC8u
#endif

typedef struct BrPhase_     BrPhase_;
typedef struct BrUiPage_    BrUiPage_;

/* ---------------------------------------------------------------------------
 * The two non-vtable code slots, +0x04 and +0x08.
 *
 * Both were `void *` here until the call sites were read, because THIS header
 * only ever saw them stored. slice2_26.c and slice3_31.c call them, and the
 * shapes come from those calls -- not from symmetry:
 *
 *   +0x04  `p->pfn04(p)`                 -- one argument, the phase ITSELF.
 *          Every installer writes it immediately after construction
 *          (0x10048710 does NOT initialise +0x04; see the GOTCHA below).
 *
 *   +0x08  `pCtx->pAA29F4->pfn08(pArg)`  -- one argument, and it is the
 *          CALLER's own argument, an entity record. Not the phase. That
 *          asymmetry is the original's (0x100450F0) and is preserved.
 *
 * DEVIATION (portability, slice2_26.h's, restated): both are __cdecl in the
 * original and are modelled as ordinary C function pointers here. Argument
 * values and order are unchanged.
 * ------------------------------------------------------------------------- */
typedef void (*BrPhaseEnterFn_)(BrPhase_ *pSelf);

/* RETURN VALUE (corrected -- this used to be `void`, and the value was lost).
 *
 * The +0x08 slot is the ACTION hook, and its result is TESTED. 0x10048180
 * dispatches it and branches on eax:
 *
 *     10048280  ff5608   call dword ptr [esi + 8]
 *     10048286  85c0     test eax, eax
 *     10048288  7508     jne  0x10048292
 *
 * -- a zero return makes 0x10048180 return 0 immediately, skipping the
 * `[0x10AA33E4] = 0` store, the `flags &= ~2` clear, the child loop and the
 * vtable +0x08 draw. Every one of the forty routines slice3_31.c installs
 * here ends in `xor eax, eax`, so all of them take that early exit; a `void`
 * host type could not express it. See slice3_31.h for the per-address table.
 *
 * The width is int32_t because the one reader is a 32-bit `test eax, eax`. */
typedef int32_t (*BrPhaseHookFn_)(void *pEntity);

/* vtable at 0x1008F700. Slots verified against the ctor/dtor pair; unused slots
 * are void* rather than given plausible-but-unverified signatures. */
typedef struct BrPhaseVtbl_ {
    void   *(*f00)(BrPhase_ *pThis, int32_t nFlags);  /* 0x10048850 */
    int32_t (*f04)(BrPhase_ *pThis);                  /* 0x100488B0 */
    int32_t (*f08)(BrPhase_ *pThis);                  /* 0x100488C0 */
    int32_t (*f0C)(BrPhase_ *pThis);                  /* 0x100489A0 */
    void    *f10;                                     /* 0x1005AE70 -- a real
                                                       * function; no call site
                                                       * seen, so no signature
                                                       * is read into it.     */
    void    (*f14)(BrPhase_ *pThis);                  /* 0x10048960 */
    void    (*f18)(BrPhase_ *pThis, void *pArg);      /* 0x10048B20 */
    void    (*f1C)(BrPhase_ *pThis);                  /* 0x10048AA0 */
    void    (*f20)(BrPhase_ *pThis);                  /* 0x1005AFA0 */
} BrPhaseVtbl_;

struct BrPhase_ {
    const BrPhaseVtbl_ *pVtbl;               /* +0x00  = 0x1008F700           */
    /* +0x04. NOT written by the constructor, and operator new does not zero,
     * so it is GARBAGE until an installer stores a hook -- which is exactly
     * what slice2_26.c does one line after calling the constructor. */
    BrPhaseEnterFn_     pfnEnter;            /* +0x04  slice2_26 pfn04        */
    BrPhaseHookFn_      pfnHook;             /* +0x08  slice2_26 pfn08, = 0   */
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

/* The arithmetic that PINS the count, in br_ui.h's style: the constructor's
 * `rep stosd` of 0x14 dwords at +0x6C ends exactly on +0xBC, the next field it
 * writes. Host-independent, because it is a statement about the ORIGINAL's
 * offsets. If someone "corrects" the count back to 22, this fails and points
 * at the adjudication in the banner. */
BR_PH_ASSERT(aflags_fill_ends_on_fBC,
             0x6Cu + (unsigned)BR_PHASE_PAGES * 4u == 0xBCu);

/* Allocate with this, never with the 0xC8 literal. */
#define BR_PHASE_ALLOC_SIZE \
    (sizeof(BrPhase_) > BR_PHASE_ORIG_SIZE ? sizeof(BrPhase_) : BR_PHASE_ORIG_SIZE)

#endif /* BR_PHASE_H */
