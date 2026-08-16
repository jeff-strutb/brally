/* slice2_26.h -- BRD3D.dll 0x100447D0-0x100456B0, a later pass.
 *
 * All twenty-five functions in this range belong to ONE mechanism: the game's
 * phase (screen/mode) switcher. Nothing here computes anything; every routine
 * is bookkeeping over a set of file-scope globals plus calls into other
 * slices. Following the precedent set by br_pool.h / br_span.h / slice1_06.h,
 * those globals are gathered into a struct (BrPhaseCtx) and passed in as an
 * added first argument. Where the original also took an argument, that
 * argument keeps its original position (second here).
 *
 * The mechanism
 * -------------
 * A phase is a 0xC8-byte heap object. There is one singleton per phase, held
 * in its own global slot, and one "current phase" global (0x10AA2904). The
 * thirteen ACTIVATE routines all have the identical shape:
 *
 *     <per-phase prologue>
 *     if (slot != NULL) { current = slot; return 1; }      // already built
 *     p = operator new(0xC8);                              // NOT zeroed
 *     p = p ? ctor_0x10048710(p) : NULL;
 *     slot = current = p;
 *     if (p == NULL) return 0;                             // note: NOT 1
 *     p->pfn04 = <the phase's enter hook>;
 *     slot->pfn04(slot);                                   // re-read of slot
 *     current->f0C = 1;                                    // re-read
 *     current->f68 = 1;                                    // re-read again
 *     <per-phase epilogue>
 *     return 1;
 *
 * GOTCHA (load order): the original re-reads the `current` global after
 * calling the enter hook and again between the two flag stores. An enter hook
 * that re-points `current` therefore has its own f0C/f68 set, not the object
 * that was just built. That reload is reproduced here and is load-bearing.
 *
 * GOTCHA (three outcomes, not two): "already built" and "just built" both
 * return 1, but the per-phase epilogue runs ONLY on the just-built path
 * (0x10044F50, which then calls three subsystem hooks), while the tail call
 * at 0x10045460 / 0x10045520 runs on both. Allocation failure returns 0 and
 * skips everything downstream.
 *
 * The eight LEAVE routines take an entity record (stride 0x2B68, per
 * slice1_09.h) and share a prologue: call the entity sub-object's vtable slot
 * +0x1C, then notify a phase through its vtable slot +0x00 with the argument
 * 1, then repoint `current` at some other phase's slot and clear a handful of
 * globals. They all return 0.
 *
 * Naming: no semantic name could be established for any individual phase, so
 * every routine is named for its mechanism plus its address, and every global
 * is named for its address. Cross-slice callees whose purpose is unknown are
 * declared as BrExt_<ADDR>.
 *
 * Calling conventions: the original mixes three.
 *   - The phase vtable slot +0x00 and the entity sub-object vtable slots
 *     +0x18 / +0x1C are __thiscall (object in ecx, callee cleans).
 *   - The phase fields +0x04 / +0x08 hold plain __cdecl function pointers;
 *     +0x04 is called with the phase itself as its one argument, +0x08 with
 *     whatever the caller was handed.
 *   - The vtable slot +0x7C on the object at 0x10277B40 is __cdecl WITH the
 *     object pushed as the first argument.
 * DEVIATION: all of them are modelled here as ordinary C functions whose
 * first parameter is the object. That is a portability change only; the
 * argument values and their order are unchanged.
 */
#ifndef SLICE2_26_H
#define SLICE2_26_H

#include <stdint.h>

#include "br_slots.h"   /* 0x100586A0 == BrSlotsReset */
#include "br_phase.h"   /* BrPhase_, BrPhaseVtbl_ -- CANONICAL, see below */

/* ==========================================================================
 * The phase object (0xC8 bytes, allocated at 0x1004488E and eleven twins)
 *
 * THIS HEADER NO LONGER DEFINES IT. br_phase.h does, and the names below are
 * aliases onto that one model.
 *
 * What used to be here was a FIVE-field partial view {pVtbl, pfn04, pfn08,
 * f0C, f68} -- the five fields this range touches -- with the rest of the 0xC8
 * bytes left unmodelled on the stated grounds that "the rest belongs to the
 * constructor at 0x10048710". That was true and it was safe only for exactly
 * as long as 0x10048710 was NOT wired to these call sites.
 *
 * It is wired now, and the partial view could not survive it. The fifth
 * member, f68, sat immediately after f0C in the port's layout, whereas in the
 * object the constructor writes there are three fields in between (nPages,
 * iPage, aPages[20]). The constructor would have written a full BrPhase_ into
 * an allocation this header's callers then read as a BrPhase, and every field
 * from +0x0C on would have landed somewhere else -- link-clean, run-clean, and
 * silently wrong, which is the failure mode CONVENTIONS.md's "Two models of
 * one object, shifted" section describes one level up.
 *
 * Nothing this header observed was wrong. All five fields exist in BrPhase_
 * under the mapping br_phase.h records (pfn04 = pfnEnter, pfn08 = pfnHook);
 * the model simply had no way to express the other eight fields, and a
 * partial view is only safe while nobody writes the whole object.
 *
 * TWO TYPE CHANGES fall out of the merge, both widenings, both adjudicated in
 * br_phase.h's banner from the disassembly:
 *
 *   - BrPhaseVtbl has NINE slots, not one. The extra slots were always there
 *     at 0x1008F700; this range only ever called +0x00.
 *   - Slot +0x00 returns `void *`, not void. 0x10048850 is the MSVC scalar
 *     deleting destructor and ends `mov eax,esi / ret 4`. Callers in this
 *     range discard the result, which is why void survived; discarding a
 *     returned value is legal C and no call site changes.
 * ========================================================================== */

typedef BrPhase_     BrPhase;
typedef BrPhaseVtbl_ BrPhaseVtbl;

/* +0x04. Called as pfn04(self). */
typedef BrPhaseEnterFn_ BrPhaseEnterFn;
/* +0x08. Called as pfn08(x) where x is the caller's own argument -- an
 * entity record, not the phase. See 0x100450F0. */
typedef BrPhaseHookFn_  BrPhaseHookFn;

/* FIELD RENAMES, done at the use sites rather than with macros. This header's
 * `pfn04`/`pfn08` are br_phase.h's `pfnEnter`/`pfnHook`; slice2_26.c and
 * slice3_31.c now spell them that way. They are NOT `#define pfn04 pfnEnter`,
 * deliberately: slice2_25.h's BrOptObj -- a THIRD partial view of this same
 * 0xC8 object -- has its own `pfn04` member, and slice4_50.c writes it. A
 * member-renaming macro is textual and would silently rewrite that unrelated
 * store the moment the two headers met in one TU. f0C and f68 keep their
 * names; br_phase.h spells them the same.
 *
 * (slice2_25.h's BrOptObj is the remaining un-merged view of this object. It
 * is padded to 0xC8 and nothing writes the whole object through it today, so
 * it is not yet a live hazard -- but it is the same hazard, and slice6_73.h's
 * conflict note #1 already names it.) */

/* The literal the original passes to operator new. br_phase.h owns it now and
 * defines the same value; the guard stays so this header still compiles first
 * in a TU. If the two ever disagree the #ifndef would HIDE it, so it is
 * asserted rather than assumed. */
#ifndef BR_PHASE_ORIG_SIZE
#define BR_PHASE_ORIG_SIZE 0xC8
#endif
typedef char BrPhaseOrigSizeAgrees[(BR_PHASE_ORIG_SIZE == 0xC8) ? 1 : -1];

/* WAS: `sizeof(BrPhase) <= BR_PHASE_ORIG_SIZE`, and that assertion encoded the
 * bug. It held only because the struct was a five-field stub; the real object
 * is LARGER than 0xC8 on LP64 because every pointer widened. The invariant
 * that actually matters is the opposite one -- the port must never allocate
 * less than the original did -- and BR_PHASE_ALLOC_SIZE is what call sites
 * use. See br_phase.h. */
typedef char BrPhaseSizeCheck[(sizeof(BrPhase) >= BR_PHASE_ORIG_SIZE) ? 1 : -1];

/* ==========================================================================
 * The entity record (stride 0x2B68 -- the same record slice1_09.h describes)
 * ========================================================================== */

typedef struct BrEntSub     BrEntSub;
typedef struct BrEntSubVtbl BrEntSubVtbl;

struct BrEntSubVtbl {
    void *aReserved[6];                        /* +0x00..+0x17, unused here */
    void (*f18)(BrEntSub *pThis, int32_t a);   /* +0x18 __thiscall(this,int) */
    void (*f1C)(BrEntSub *pThis);              /* +0x1C __thiscall(this)     */
};

struct BrEntSub {
    const BrEntSubVtbl *pVtbl;   /* +0x00 */
};

/* Only three fields of the 0x2B68-byte record are touched, so -- exactly as
 * slice1_09.h does for the same record -- no struct is invented and the
 * fields are reached by byte offset from a void *. */
#define BR_ENTITY_OFF_FLAGS 0x001C  /* uint32; bit 0x10 is cleared on leave  */
#define BR_ENTITY_OFF_SUB   0x2AE8  /* BrEntSub *                            */
#define BR_ENTITY_OFF_F2B64 0x2B64  /* uint8; cleared by 0x10044A30 only     */

/* ==========================================================================
 * Two more foreign objects
 * ========================================================================== */

typedef struct BrHost     BrHost;      /* *(void**)0x10277B40 */
typedef struct BrHostVtbl BrHostVtbl;

struct BrHostVtbl {
    void *aReserved[31];                                  /* +0x00..+0x78 */
    /* +0x7C. __cdecl, and the object is pushed as the first argument. */
    void (*f7C)(BrHost *pSelf, void *pItem, int32_t a);
};

struct BrHost {
    const BrHostVtbl *pVtbl;   /* +0x00 */
};

/* What 0x1003D0B0 hands back through its out-parameter. Only +0x04 is
 * touched (bit 0x20 cleared). */
typedef struct BrHostItem {
    uint32_t f00;
    uint32_t f04;
} BrHostItem;

/* *(void**)0x10A9D008. Only +0x08 is read. */
typedef struct BrObjA9D008 {
    void *f00;
    void *f04;
    void *f08;
} BrObjA9D008;

/* ==========================================================================
 * The globals this range owns, gathered up
 * ========================================================================== */

typedef struct BrPhaseCtx {
    /* --- phase singletons; every one of these is a BrPhase * ------------- */
    BrPhase *pAA2904;   /* 0x10AA2904 -- the CURRENT phase                   */
    BrPhase *pAA2908;   /* 0x10AA2908 */
    BrPhase *pAA290C;   /* 0x10AA290C */
    BrPhase *pAA2914;   /* 0x10AA2914 */
    BrPhase *pAA2918;   /* 0x10AA2918 */
    BrPhase *pAA2940;   /* 0x10AA2940 */
    BrPhase *pAA2948;   /* 0x10AA2948 */
    BrPhase *pAA294C;   /* 0x10AA294C */
    BrPhase *pAA2954;   /* 0x10AA2954 */
    BrPhase *pAA295C;   /* 0x10AA295C */
    BrPhase *pAA2964;   /* 0x10AA2964 */
    BrPhase *pAA2968;   /* 0x10AA2968 */
    BrPhase *pAA297C;   /* 0x10AA297C */
    BrPhase *pAA2980;   /* 0x10AA2980 */
    BrPhase *pAA2984;   /* 0x10AA2984 */
    BrPhase *pAA2988;   /* 0x10AA2988 */
    BrPhase *pAA2990;   /* 0x10AA2990 */
    BrPhase *pAA2994;   /* 0x10AA2994 */
    BrPhase *pAA29B0;   /* 0x10AA29B0 -- receives the +0x08 hook            */
    BrPhase *pAA29B4;   /* 0x10AA29B4 -- receives the +0x08 hook            */
    BrPhase *pAA29F4;   /* 0x10AA29F4 -- its +0x08 hook is dispatched       */

    /* --- scalars --------------------------------------------------------- */
    int32_t n0AA010;    /* 0x100AA010 -- set to 6, 2, 1 or 0                 */
    int32_t n0AC304;    /* 0x100AC304 -- cleared then set around 0x10045050  */
    int32_t nA9CFFC;    /* 0x10A9CFFC */
    int32_t nA9D000;    /* 0x10A9D000 */
    int32_t nAA287C;    /* 0x10AA287C -- switched on: 0/1 vs 2/3             */
    int32_t nAA2880;    /* 0x10AA2880 */
    int32_t nAA2884;    /* 0x10AA2884 */
    int32_t nAA2888;    /* 0x10AA2888 */
    int32_t nAA2898;    /* 0x10AA2898 */
    int32_t nAA28C8;    /* 0x10AA28C8 */
    int32_t nAA28CC;    /* 0x10AA28CC */
    int32_t nAA2950;    /* 0x10AA2950 */
    int32_t nAA298C;    /* 0x10AA298C */
    int32_t nAA29AC;    /* 0x10AA29AC */
    int32_t nAA29B8;    /* 0x10AA29B8 */
    int32_t nAA29D4;    /* 0x10AA29D4 */
    int32_t nAA29E8;    /* 0x10AA29E8 */
    int32_t nACEE8C;    /* 0x10ACEE8C -- copied to nAA28CC by 0x10044E20     */
    int32_t nACEE94;    /* 0x10ACEE94 -- copied to nAA28C8 by 0x10044E20     */

    /* --- pointers to foreign objects -------------------------------------- */
    void        *pAA29D8;  /* 0x10AA29D8 -- an entity record (0x2B68 bytes)  */
    BrHost      *p277B40;  /* 0x10277B40 */
    BrObjA9D008 *pA9D008;  /* 0x10A9D008 */
    void        *p0AD300;  /* &0x100AD300 -- the argument to 0x100419D0      */
    BrSlotTable *pSlots;   /* 0x10AA2538 -- the table 0x100586A0 resets      */
} BrPhaseCtx;

/* The bit 0x10044970 and friends clear in the entity's +0x1C flags, and the
 * bit 0x100447D0 clears in the host item's +0x04. */
#define BR_ENTITY_FLAG_1C_10 0x00000010u
#define BR_HOSTITEM_FLAG_20  0x00000020u

/* ==========================================================================
 * Cross-slice callees. The integration wires these; a stand-in for each one
 * lives in port/tests/test_slice2_26.c and NOWHERE else.
 * ========================================================================== */

/* XSLICE 0x1007DFE0 */
/* operator new -- _nh_malloc(cb,1). Does NOT zero the block. */
extern void *BrOperatorNew(uint32_t cb);

/* XSLICE 0x10048710 */
/* __thiscall constructor for the phase object; returns `this`. */
extern BrPhase *BrPhaseCtor(BrPhase *pThis);

/* XSLICE 0x1003D0B0 */
/* Two arguments, the second an out-parameter. Its return value is discarded
 * by every caller in this range, so it is declared void here. */
extern void BrExt_1003D0B0(BrHost *pHost, BrHostItem **ppOut);

/* XSLICE 0x10043BF0 */ extern void BrExt_10043BF0(int32_t a);
/* XSLICE 0x10043CD0 */ extern void BrExt_10043CD0(int32_t a);
/* XSLICE 0x10043E70 */ extern void BrExt_10043E70(int32_t a);
/* XSLICE 0x100440D0 */ extern void BrExt_100440D0(int32_t a);
/* XSLICE 0x100443E0 */ extern void BrExt_100443E0(int32_t a);
/* XSLICE 0x10044280 */ extern void BrExt_10044280(int32_t a);
/* XSLICE 0x10038F30 */ extern void BrExt_10038F30(int32_t a);
/* XSLICE 0x100419D0 */ extern void BrExt_100419D0(void *p);
/* XSLICE 0x1003DB00 */ extern void BrExt_1003DB00(BrObjA9D008 *pObj, void *p);
/* XSLICE 0x1003BF60 */ extern void BrExt_1003BF60(void);
/* XSLICE 0x1003C020 */ extern void BrExt_1003C020(void);
/* XSLICE 0x1003C150 */ extern void BrExt_1003C150(void);
/* XSLICE 0x1003CDA0 */ extern void BrExt_1003CDA0(void);
/* XSLICE 0x1003DFC0 */ extern void BrExt_1003DFC0(void);
/* XSLICE 0x1003E510 */ extern void BrExt_1003E510(void);
/* XSLICE 0x1003E680 */ extern void BrExt_1003E680(void);
/* XSLICE 0x10041BD0 */ extern void BrExt_10041BD0(void);
/* XSLICE 0x1007AC00 */ extern void BrExt_1007AC00(void);
/* XSLICE 0x10045C90 */ extern void BrExt_10045C90(void *p);

/* XSLICE 0x10008B80 */
/* A stub in this build (a bare `ret`) -- see the contract. The call is kept
 * so the call graph stays faithful. */
extern void BrExt_10008B80(void);

/* The enter hooks stored into BrPhase.pfn04. */
/* XSLICE 0x10058750 */ extern void BrExt_10058750(BrPhase *pSelf);
/* XSLICE 0x10059760 */ extern void BrExt_10059760(BrPhase *pSelf);
/* XSLICE 0x10059BB0 */ extern void BrExt_10059BB0(BrPhase *pSelf);
/* XSLICE 0x1005A6E0 */ extern void BrExt_1005A6E0(BrPhase *pSelf);
/* XSLICE 0x1004A580 -- OWNED BY slice3_33 (BrExt_1004A580(BrUiBuildCtx*, BrUiPhase*)).
 * This placeholder was declared before that landed and had an incompatible
 * signature; renamed so the two headers can coexist. Call the slice3_33 form. */
extern void BrPhaseEnterPlaceholder_1004A580(BrPhase *pSelf);
/* XSLICE 0x1004B430 -- OWNED BY slice3_33 (BrExt_1004B430(BrUiBuildCtx*, BrUiPhase*)).
 * This placeholder was declared before that landed and had an incompatible
 * signature; renamed so the two headers can coexist. Call the slice3_33 form. */
extern void BrPhaseEnterPlaceholder_1004B430(BrPhase *pSelf);
/* XSLICE 0x1004BDC0 -- OWNED BY slice3_33 (BrExt_1004BDC0(BrUiBuildCtx*, BrUiPhase*)).
 * This placeholder was declared before that landed and had an incompatible
 * signature; renamed so the two headers can coexist. Call the slice3_33 form. */
extern void BrPhaseEnterPlaceholder_1004BDC0(BrPhase *pSelf);
/* XSLICE 0x1004C4A0 -- OWNED BY slice3_33 (BrExt_1004C4A0(BrUiBuildCtx*, BrUiPhase*)).
 * This placeholder was declared before that landed and had an incompatible
 * signature; renamed so the two headers can coexist. Call the slice3_33 form. */
extern void BrPhaseEnterPlaceholder_1004C4A0(BrPhase *pSelf);
/* XSLICE 0x1004D1F0 */ extern void BrExt_1004D1F0(BrPhase *pSelf);
/* XSLICE 0x1004D640 */ extern void BrExt_1004D640(BrPhase *pSelf);
/* XSLICE 0x1004DB00 */ extern void BrExt_1004DB00(BrPhase *pSelf);
/* XSLICE 0x1004DFC0 */ extern void BrExt_1004DFC0(BrPhase *pSelf);
/* XSLICE 0x1004E830 */ extern void BrExt_1004E830(BrPhase *pSelf);

/* The hooks stored into BrPhase.pfn08. */
/* XSLICE 0x10046CD0 */ extern void BrExt_10046CD0(void *pEntity);
/* XSLICE 0x10046DC0 */ extern void BrExt_10046DC0(void *pEntity);

/* ==========================================================================
 * The range itself
 * ========================================================================== */

/* --- activate ------------------------------------------------------------ */

/* 0x100447D0  The heavy one. Resets the slot table, tears three subsystems
 * down, brings one of two back up depending on nAA2884, then activates the
 * 0x10AA2954 phase (enter hook 0x10058750) and finally sets n0AA010 = 6.
 * Returns 1, or 0 if the phase object could not be allocated -- in which case
 * NONE of the tail work (n0AA010, the nAA2888 branch, the pA9D008 poke) runs.
 * Takes no argument in the original. */
int BrPhaseActivate_100447D0(BrPhaseCtx *pCtx);

/* 0x10044B90  prologue: BrExt_100419D0(p0AD300). Slot 0x10AA295C, enter hook
 * 0x10059760. */
int BrPhaseActivate_10044B90(BrPhaseCtx *pCtx);

/* 0x10044D00  prologue: nAA28C8 = nAA28CC = 0. Slot 0x10AA2964, enter hook
 * 0x10059BB0. */
int BrPhaseActivate_10044D00(BrPhaseCtx *pCtx);

/* 0x10044E20  prologue: nAA28CC = nACEE8C and nAA28C8 = nACEE94.
 * GOTCHA: the two copies cross -- the LOWER source feeds the HIGHER
 * destination. Slot 0x10AA2968, enter hook 0x1005A6E0. */
int BrPhaseActivate_10044E20(BrPhaseCtx *pCtx);

/* 0x10044F50  prologue: BrExt_100419D0(p0AD300), n0AA010 = 1, BrExt_1003E680.
 * Slot 0x10AA290C, enter hook 0x1004B430. Epilogue (just-built path ONLY):
 * BrExt_10008B80, BrExt_1003DFC0, BrExt_1003E510. */
int BrPhaseActivate_10044F50(BrPhaseCtx *pCtx);

/* 0x10045110  no prologue. Slot 0x10AA2914, enter hook 0x1004A580.
 * NOTE: 0x10045050 pushes one argument at this function, which ignores it;
 * the original is __cdecl so the mismatch is harmless. Declared here with no
 * parameter, which is what the body actually uses. */
int BrPhaseActivate_10045110(BrPhaseCtx *pCtx);

/* 0x100451E0  prologue: BrExt_100419D0(p0AD300). Slot 0x10AA2918, enter hook
 * 0x1004BDC0. */
int BrPhaseActivate_100451E0(BrPhaseCtx *pCtx);

/* 0x100452C0  slot 0x10AA297C, enter hook 0x1004C4A0. */
int BrPhaseActivate_100452C0(BrPhaseCtx *pCtx);

/* 0x10045390  slot 0x10AA2980, enter hook 0x1004D1F0. */
int BrPhaseActivate_10045390(BrPhaseCtx *pCtx);

/* 0x10045460  slot 0x10AA2990, enter hook 0x1004D640. Epilogue
 * BrExt_1007AC00 on BOTH the already-built and just-built paths, but not
 * after an allocation failure. */
int BrPhaseActivate_10045460(BrPhaseCtx *pCtx);

/* 0x10045520  slot 0x10AA2994, enter hook 0x1004DB00. Epilogue as 0x10045460. */
int BrPhaseActivate_10045520(BrPhaseCtx *pCtx);

/* 0x100455E0  slot 0x10AA2984, enter hook 0x1004DFC0. */
int BrPhaseActivate_100455E0(BrPhaseCtx *pCtx);

/* 0x100456B0  slot 0x10AA2988, enter hook 0x1004E830. */
int BrPhaseActivate_100456B0(BrPhaseCtx *pCtx);

/* --- leave --------------------------------------------------------------- */

/* 0x10044970  Leave to the 0x10AA2948 phase. When nA9D000 is set it first
 * calls the entity sub-object's +0x18 slot with 0 and BrExt_10038F30(0).
 * Clears bit 0x10 of pAA29D8's +0x1C flags BEFORE BrExt_1003BF60, sets
 * nAA2898 = 1, and -- when nAA287C is 0 or 1 and nA9D000 is clear -- calls
 * BrExt_1003C020 and RE-READS nAA287C, so a mode changed by that call is what
 * the 2/3 test below sees. Always returns 0. */
int BrPhaseLeave_10044970(BrPhaseCtx *pCtx, void *pEntity);

/* 0x10044A30  The twin of 0x10044970: leaves to 0x10AA294C, does NOT touch
 * nAA2898 and does NOT clear the 0x10 flag bit up front, and on the 2/3 path
 * additionally clears the entity byte at +0x2B64. Returns 0. */
int BrPhaseLeave_10044A30(BrPhaseCtx *pCtx, void *pEntity);

/* 0x10044AE0  Leave to 0x10AA2940, clearing pAA2948, nAA29B8, pAA29D8,
 * nAA29D4 and nAA2880, then BrExt_1003BF60. Returns 0. */
int BrPhaseLeave_10044AE0(BrPhaseCtx *pCtx, void *pEntity);

/* 0x10044B40  Leave to 0x10AA2940, clearing nAA298C and nAA29E8. Returns 0. */
int BrPhaseLeave_10044B40(BrPhaseCtx *pCtx, void *pEntity);

/* 0x10044C70  Leave to 0x10AA2908, clearing the 0x10AA295C slot. Returns 0. */
int BrPhaseLeave_10044C70(BrPhaseCtx *pCtx, void *pEntity);

/* 0x10044CB0  Leave to 0x10AA295C, clearing the 0x10AA290C slot and nAA29AC.
 * Returns 0. */
int BrPhaseLeave_10044CB0(BrPhaseCtx *pCtx, void *pEntity);

/* 0x10044DE0  Leave to 0x10AA295C, clearing the 0x10AA2964 slot. Returns 0. */
int BrPhaseLeave_10044DE0(BrPhaseCtx *pCtx, void *pEntity);

/* 0x10044F00  Leave to 0x10AA295C, clearing the 0x10AA2968 slot and setting
 * n0AA010 = 2.
 * GOTCHA: alone among the leave routines this notifies pAA2968 -- the phase
 * it is about to drop -- rather than pAA2904. Returns 0. */
int BrPhaseLeave_10044F00(BrPhaseCtx *pCtx, void *pEntity);

/* --- hook installers and the dispatcher ----------------------------------- */

/* 0x10045050  n0AC304 = 0, activate 0x10045110, n0AC304 = 1, then install
 * 0x10046CD0 as pAA29B4's +0x08 hook and set n0AA010 = 0. Returns 1
 * unconditionally -- the result of the activation is discarded. */
int BrPhaseHook_10045050(BrPhaseCtx *pCtx, void *pArg);

/* 0x10045090  BrExt_10045C90(pArg), then install 0x10046DC0 as pAA29B0's
 * +0x08 hook and set n0AA010 = 0. Returns 1. */
int BrPhaseHook_10045090(BrPhaseCtx *pCtx, void *pArg);

/* 0x100450C0  As 0x10045090 with a leading BrExt_10041BD0(). Returns 1. */
int BrPhaseHook_100450C0(BrPhaseCtx *pCtx, void *pArg);

/* 0x100450F0  Dispatch: call pAA29F4's +0x08 hook with pArg, then
 * n0AA010 = 0.
 * GOTCHA: returns 0, where its three neighbours return 1. */
int BrPhaseDispatch_100450F0(BrPhaseCtx *pCtx, void *pArg);

#endif /* SLICE2_26_H */
