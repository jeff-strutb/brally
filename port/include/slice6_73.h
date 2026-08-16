/* slice6_73.h -- BRD3D.dll, packet 73 (slice 6).
 *
 * WHAT THIS PACKET IS
 * -------------------
 * Twenty-three addresses that some already-ported module calls through an
 * `extern` it declared itself.  Fourteen are implemented here; nine are not,
 * and the reasons are listed below so nobody re-derives them.
 *
 * The centre of gravity is the SIX MENU-SCREEN BUILDERS.  slice5_60.h,
 * slice3_32.c, slice4_51.c and slice5_61.h each declined this family in turn,
 * every time for the same reason: no declarable type could hold the fields
 * their bodies touch.  br_phase.h now exists and is canonical for the
 * 0xC8-byte phase, so the family is finally writable with the ONE-ARGUMENT
 * signature its callers use.  That is what this file does.
 *
 * ==========================================================================
 * IN THIS FILE (14 of the 23)
 * ==========================================================================
 *   0x10048710  BrOptObjCtor        the phase constructor itself
 *   0x100558A0  BrOptFn100558A0  }
 *   0x1004D640  BrExt_1004D640   }
 *   0x1004DFC0  BrExt_1004DFC0   }  the six menu-screen builders
 *   0x1004F2B0  BrExt_1004F2B0   }
 *   0x10050060  BrExt_10050060   }
 *   0x10054B50  BrExt_10054B50   }
 *   0x10041A00  BrExt_10041A00      name commit  (0x10AA29CC records)
 *   0x100424D0  BrExt_100424D0      name restore (0x10AA29D0 records)
 *   0x1003E680  BrSub1003E680 / BrExt_1003E680   global reset
 *   0x1003D030  BrSub1003D030       16-byte join blob
 *   0x10071550  BrSub10071550       two calls, returns 1
 *   0x1006F720  BrCollGridCellAcquire   collision-grid cell load
 *   0x10031140  BrSub_10031140      ADAPTER ONLY -- see below
 *
 * ==========================================================================
 * NOT IN THIS FILE, AND WHY
 * ==========================================================================
 * WIN32 / COM -- the contract bars Win32 types and calling conventions from
 * portable code, and there is nothing else in these three bodies:
 *   0x1003C020  BrExt_1003C020 / BrSub1003C020
 *               KillTimer, SetTimer, CreateEventA, IDirectPlay4 slot +0x98.
 *               (slice5_60.h declined it for exactly this reason.)
 *   0x1003D210  BrFn1003D210 / BrSub1003D210
 *               GlobalAlloc/GlobalLock/GlobalUnlock/GlobalHandle/GlobalFree,
 *               PostMessageA, lstrlenA, lstrcpyA.
 *   0x1003CDA0  BrSub1003CDA0 / BrExt_1003CDA0
 *               Global* again plus a COM vtable slot +0x7C.
 *
 * ALREADY IMPLEMENTED ELSEWHERE -- these are NAME gaps, not code gaps.  The
 * contract forbids a second decompilation; what is missing is an adapter,
 * and an adapter cannot be written here because the existing bodies are
 * typed against the partial phase models this file deliberately does not
 * include (see CONFLICTS):
 *   0x10043330  BrExt_10043330   == slice2_25.c  BrOptOpen2970
 *   0x10043CD0  BrExt_10043CD0   == slice2_25.c  BrOptOpen2940
 *   0x10044280  BrExt_10044280   == slice2_25.c  BrOptOpen2950A
 *   0x10043BF0  BrExt_10043BF0   == slice4_50.c  BrSub10043BF0
 *                (and its listing in the packet ENDS MID-FLOW at 0x10043C25:
 *                 the extent says 59 bytes but the body branches to
 *                 0x10043CB2.  One of config/functions.csv's 37 bad extents.)
 *   0x10044970  BrOptFn10044970  == slice2_26.c  BrPhaseLeave_10044970
 *
 * 0x100290A0  BrSub_100290A0 -- NOT PORTABLE UNDER ITS DECLARED TYPE.
 *   The body uses its THIRD argument as an integer record index
 *   (`edx = 87*n; rec = *(char**)0x1057543C + 8*edx`, a 696-byte stride) and
 *   forwards rec[+0x20] and rec[+0x24] to 0x10028720.  slice2_15.h declares
 *   it `void BrSub_100290A0(void *, void *, void *)` and slice2_15.c passes
 *   `g_weather.apTable[i]` as that third argument.  Implementing it would
 *   mean reinterpreting a host pointer as a 32-bit table index, which is the
 *   LP64 hazard the contract rules out.  The declaration has to become an
 *   integer before this can be written.
 *
 * ==========================================================================
 * CONFLICTS FOUND (reported, deliberately not silently "resolved")
 * ==========================================================================
 * 1. 0x10048710 -- SIGNATURE CONFLICT, and it is the load-bearing one.
 *      slice2_25.h:450   BrOptObj *BrOptObjCtor(BrOptObj *pThis)
 *      here              BrPhase_ *BrOptObjCtor(BrPhase_ *pThis)
 *    `BrOptObj` is slice2_25.h's five-field partial view of the SAME 0xC8
 *    object br_phase.h models in full.  The constructor writes +0x10, +0x12,
 *    +0x64, +0xBC, +0xC0, +0xC4 and twenty dwords at +0x6C -- none of which
 *    BrOptObj has.  Compiling slice2_25.c against this definition would link
 *    and be wrong at every one of those stores, so the two headers are NOT
 *    included together and integration must retype slice2_25.h's slot
 *    globals to BrPhase_ *.  This is the merge br_phase.h was created for.
 *
 * 2. The six builders' declared parameter type.
 *      slice3_31.h:246/249/253  void BrExt_1004F2B0(BrPhase *pSelf)   ...
 *      slice2_26.h:289/291      void BrExt_1004D640(BrPhase *pSelf)   ...
 *      slice2_25.h:467          void BrOptFn100558A0(BrOptObj *pThis)
 *    Same object, three partial models.  Taken here as `BrPhase_ *`.  The
 *    ARGUMENT COUNT and the return type match the callers exactly, which is
 *    the part slice3_33.h could not manage (it had to add a context
 *    parameter); the module's globals are file-scope here instead.
 *
 * 3. RESOLVED.  The 0x348 page and the 0x1E214 control had six models
 *    between them (slice2_23.h, slice3_32.h, slice3_33.h, slice6_71.h,
 *    slice6_72.h and the ones that used to be in this file).  br_ui.h is the
 *    adjudicated merge and OWNS `struct BrUiPage_` -- the tag br_phase.h
 *    forward-declares and this header used to complete.  Both definitions are
 *    gone from here and br_ui.h is included instead, which is what makes this
 *    header and slice6_72.h able to share a translation unit at last.
 *
 *    What moved, for anyone chasing an old field name:
 *      f50/f54/f58/f5C -> rcLeft/rcTop/rcRight/rcBottom
 *      f1C/f24/f28     -> flags1C/flags24/flags28
 *      f3C/f40         -> x/y          f48/f4A -> w48/w4A
 *      f2A40/f2A42     -> aStepId[0]/aStepId[1]         (ADJ-3, ADJ-4)
 *      f2AB4/f2AB6     -> cChild/aChild[0]              (ADJ-5)
 *      f2AE8           -> pOwner
 *      f2B5C           -> aText[0]                      (ADJ-1, ADJ-2)
 *      f3838           -> list  (slice3_39.h BrTextList) (ADJ-6)
 *      f1E1F4/f1E1E8   -> list.f1A99C[8]/[5]
 *      f1E200/f1E204   -> list.f1A99C[11]/[12]
 *      f1E1C8/f1E1D0   -> list.f1A990/f1A998  -- br_ui.h ADJ-6 records these
 *                         two as the ONE thing the canonical control cannot
 *                         express, because they land in a region slice3_39.h
 *                         did not model.  They are named in slice3_39.h now,
 *                         in their rightful owner, rather than being added
 *                         back to the control as a private extension.
 *      f1E20C          -> w1E20C
 *
 * 4. 0x10031140 -- ALREADY slice1_05.c's BrMat4Translate (slice5_61.h says so
 *    too).  `BrSub_10031140` here is a three-line ADAPTER, not a second body.
 *    It exists because slice2_15.h declares the first two coordinates
 *    `int32_t` (the original copies all three as raw dwords, and slice2_15.h
 *    types the camera's +0x30/+0x34 the same way), so the adapter has to
 *    reinterpret two bit patterns as floats before delegating.
 *
 * 5. 0x117554A0 -- slice2_11.h declares `const uint16_t *g_pBrCollGridCount`.
 *    0x1006F720 WRITES it.  The const is cast away at that single store; see
 *    the DEVIATION in slice6_73.c.  slice2_11.h should drop the const.
 *
 * ==========================================================================
 * FACTS WORTH NOT RE-DERIVING
 * ==========================================================================
 * - The row constants at 0x1008F680.. are NEGATIVE, and every use is
 *   `fsub`, so each one ADDS its magnitude to the row cursor:
 *     0x1008F680 -19   0x1008F684 -38   0x1008F688 -57   0x1008F68C -76
 *     0x1008F690 -95   0x1008F694 -114  0x1008F698 -133  0x1008F69C -33
 *     0x1008F6A4 -0.09090909f (== -1/11 in float)
 *   Read out of orig/BRD3D.dll, not assumed.  slice3_33.h found the same.
 * - Every f38 call site in all six builders passes 2 and 5 as its fourth and
 *   fifth arguments.  Every one.
 * - The 0x6594 block the phase constructor allocates twice is slice1_06.h's
 *   BrNameList (vtable + 100 slots of 0x104): 4 + 100*0x104 == 0x6594.
 * - 0x1006F720's three callees ALREADY have names and are reused, not
 *   re-coined: 0x10002DE0 is slice1_01.h's BrGrid64Sample, 0x10002EF0 is its
 *   BrU16CursorNext, and 0x10074250 is slice1_09.h's BrVec3Normalise.  The
 *   dword 0x10002DE0 returns is the cell's CSR row -- first triangle index in
 *   the low half, count in the high half -- which is exactly the
 *   {pos, remaining} cursor 0x10002EF0 consumes.  slice1_08.h recorded this
 *   pair as the blocker for 0x1006F720; it is not one.
 */
#ifndef SLICE6_73_H
#define SLICE6_73_H

#include <stddef.h>
#include <stdint.h>

#include "br_phase.h"    /* BrPhase_, BrUiPage_ (COMPLETED by br_ui.h)     */
#include "br_ui.h"       /* struct BrUiPage_, BrUiCtl_, BrUiCtlVtbl_,
                          * BrUiCtlHookFn_, BrUiPageHookFn_ -- canonical   */
#include "br_crt.h"      /* BrOperatorNew (0x1007DFE0), BrFtolTrunc        */
#include "slice1_06.h"   /* BrErrHost/BrErrShow (0x1003E260),
                          * BrNameList/BrNameListInit (0x1005CB90)         */
#include "slice5_61.h"   /* g_br0AB3F4, g_brPAA29D0, g_aBrA9D078,
                          * BrSub1003E510 (0x1003E510); pulls slice1_05.h
                          * for BrMat4 / BrMat4Translate                   */
#include "slice2_11.h"   /* BrCollPlane, g_pBrCollGrid, g_pBrCollGridCount */
#include "slice1_01.h"   /* BrGrid64Sample (0x10002DE0),
                          * BrU16Cursor/BrU16CursorNext (0x10002EF0)       */
#include "slice1_09.h"   /* BrVec3Normalise (0x10074250)                   */
#include "slice3_39.h"   /* BrTextBox / BrTextBoxVtbl -- the 0x438 element
                          * at control +0x2B5C is that object, see below   */

/* ==========================================================================
 * 1. The page and the control -- BOTH OWNED BY br_ui.h
 *
 * This header used to define `struct BrUiPage_`, `BrUiCtl_`, `BrUiCtlVtbl_`
 * and a three-field `BrUiCtlSub_` stub for the object at control +0x3838.
 * All four are gone; br_ui.h supplies the first three and slice3_39.h's
 * BrTextList is the fourth in full (br_ui.h ADJ-6 -- it is 0x1A9D4 bytes, not
 * three fields, and 0x3838 + 0x1A9D4 lands exactly on +0x1E20C).
 *
 * The identification of the item block at +0x2B5C as slice3_39.h's BrTextBox,
 * which this header made and which br_ui.h's ADJ-1/ADJ-2 confirm from the
 * constructor and from vtable slot +0x34, survives unchanged: br_ui.h spells
 * it `BrTextBox aText[3]` and the old `f2B5C` is `aText[0]`.
 *
 * The NO-ALIASING note this header carried is kept, because it is the reason
 * the merge is safe: there is ONE object at +0x2B5C and +0x2F78 / +0x2F80 /
 * +0x2F84 / +0x2F88 / +0x2F8C are ITS fields (f41C / left / f428 / right /
 * f430), not the control's.  Two host fields for one original address drift
 * apart after the first write.  Use pCtl->aText[0].left and friends.
 * ========================================================================== */

/* The text room the element constructor's `rep stosd` establishes: 0x100
 * dwords from element +0x09.  slice3_39.h's BR_TEXTBOX_MAX is the same
 * number, reused rather than restated. */
#define BR73_ITEM_TEXT_ROOM  BR_TEXTBOX_MAX

/* This packet's own names for br_ui.h's numbers.  Defined in terms of them,
 * so the two cannot drift into a second opinion. */
#define BR73_CTL_ORIG_SIZE   BR_UI_CTL_ORIG_SIZE
#define BR73_PAGE_ORIG_SIZE  BR_UI_PAGE_ORIG_SIZE
#define BR73_PAGE_CTL_MAX    BR_UI_PAGE_CTL_MAX

/* GOTCHA (0x10048470), kept: the two bytes at page +0x016 are the ONLY part
 * of the object the constructor does not write, and `operator new` does not
 * zero, so they are indeterminate after construction.  br_ui.h names them
 * `w16` and says the same.  slice3_32.h found it independently. */

/* The port must never allocate less than the original did.  br_ui.h's
 * BR_UI_PAGE_ALLOC_SIZE / BR_UI_CTL_ALLOC_SIZE are this macro applied to the
 * two objects; it stays because this packet also allocates a BrNameList. */
#define BR73_ALLOC(type, cbOrig) \
    ((uint32_t)(sizeof(type) > (size_t)(cbOrig) ? sizeof(type) : (size_t)(cbOrig)))

/* ==========================================================================
 * 3. Cross-slice callees
 * ========================================================================== */

/* XSLICE 0x10048470 -- __thiscall page constructor; returns `this`.
 * Same name slice3_32.h gives it (slice3_33.h calls it BrUiScreenCtor). */
extern BrUiPage_ *BrUiPageCtor_10048470(BrUiPage_ *pThis);

/* XSLICE 0x100476C0 -- __thiscall control constructor; returns `this`.
 * Same name slice3_33.h gives it. */
extern BrUiCtl_ *BrUiCtlCtor(BrUiCtl_ *pThis);

/* XSLICE 0x10074030 -- string-table lookup by id; NULL for a bad id.
 * BrStrGet is the name slice2_23.h, slice2_25.h and slice3_33.h all use. */
extern const char *BrStrGet(int id);

/* XSLICE 0x1039B720 -- the shared edit buffer.  slice2_25.h declares it with
 * an explicit bound; an unspecified bound is compatible with that. */
extern char g_aBr39B720[];

/* 0x10AA28D8 -- slice2_25.h's name for it, reused rather than duplicated in
 * the context below: slice5_61.c writes the same address from the sibling of
 * BrExt_10041A00, and two storages for one global would be a live bug. */
extern int32_t g_brAA28D8;

/* The vtable slot 0x10050060 calls on the phase's +0xC0 name list: a
 * __thiscall taking one pattern string ("RallySeason*.BRF").  slice1_06.h
 * models BrNameList but not its vtable, which the original hardcodes at
 * 0x1008F788 and which BrNameListInit takes as a parameter. */
typedef struct BrNameListVtbl_ {
    void *f00;
    void (*f04)(BrNameList *pThis, const char *pszPattern);
} BrNameListVtbl_;

/* ==========================================================================
 * 4. The module's globals
 *
 * Gathered into one file-scope object, the precedent of slice2_26.h /
 * slice3_31.h / br_pool.h.  The builders keep the ONE-ARGUMENT signature
 * their callers declare, so nothing can be injected through a parameter.
 * Every field records the original address, and where another header already
 * models the same address as a field of its own struct that is named too, so
 * integration can alias rather than duplicate.
 * ========================================================================== */

/* The 48 addresses this packet stores into a control's / page's function
 * slots.  They are never called here, only installed. */
typedef struct BrUi73Hooks {
    BrUiCtlHookFn_ p1003E7A0;
    BrUiCtlHookFn_ p1003E950;
    BrUiCtlHookFn_ p1003E980;
    BrUiCtlHookFn_ p1003E9E0;
    BrUiCtlHookFn_ p1003EA40;
    BrUiCtlHookFn_ p1003EB10;
    BrUiCtlHookFn_ p1003ECB0;
    BrUiCtlHookFn_ p1003ED10;
    BrUiCtlHookFn_ p1003EE20;
    BrUiCtlHookFn_ p1003F050;
    BrUiCtlHookFn_ p1003F0B0;
    BrUiCtlHookFn_ p1003FC40;
    BrUiCtlHookFn_ p10040930;
    /* the two that land in PAGE slots (+0x04 / +0x08), which br_ui.h's ADJ-9
     * shows take no arguments at all -- not the control's hook type. */
    BrUiPageHookFn_ p100409F0;
    BrUiPageHookFn_ p10040A20;
    BrUiCtlHookFn_ p10040A50;
    BrUiCtlHookFn_ p10040AC0;
    BrUiCtlHookFn_ p10041300;
    BrUiCtlHookFn_ p100413B0;
    BrUiCtlHookFn_ p10041670;
    BrUiCtlHookFn_ p10041710;
    BrUiCtlHookFn_ p100417B0;
    BrTextListCbFn p10041DF0;
    BrTextListCbFn p10042020;
    BrUiCtlHookFn_ p10042AC0;
    BrUiCtlHookFn_ p10042AF0;
    BrUiCtlHookFn_ p10042CF0;
    BrUiCtlHookFn_ p10042D60;
    BrUiCtlHookFn_ p10043FA0;
    BrUiCtlHookFn_ p10044010;
    BrUiCtlHookFn_ p10044030;
    BrUiCtlHookFn_ p10044050;
    BrUiCtlHookFn_ p10044070;
    BrUiCtlHookFn_ p10044090;
    BrUiCtlHookFn_ p100440B0;
    BrUiCtlHookFn_ p100450F0;
    BrUiCtlHookFn_ p10045880;
    BrUiCtlHookFn_ p100458A0;
    BrUiCtlHookFn_ p10045AA0;
    BrUiCtlHookFn_ p10045AF0;
    BrUiCtlHookFn_ p100418D0;
    BrUiCtlHookFn_ p10046620;
    BrUiCtlHookFn_ p100466C0;
    BrUiCtlHookFn_ p100463C0;
    BrUiCtlHookFn_ p10046C90;
    BrUiCtlHookFn_ p10046EB0;
    BrUiCtlHookFn_ p100470E0;
    BrUiCtlHookFn_ p10047210;
    BrUiCtlHookFn_ p10047290;
    BrUiCtlHookFn_ p10047360;
    /* the three that land in the embedded LIST (+0x383C / +0x384C), not in a
     * control slot -- slice3_39.h owns their type. */
    BrTextListCbFn p1004E810;
} BrUi73Hooks;

/* The .rdata style / text blocks; the original pushes their ADDRESSES. */
typedef struct BrUi73Styles {
    const void *p0AB438;
    const void *p0AB448;
    const void *p0AB458;
    const void *p0AB468;
    const void *p0AB478;
    const void *p0AB488;
    const void *p0AB4A8;
    const void *p0AB4D8;
    const void *p0AB4F8;
    const void *p0AB508;
    const void *p0AB528;
    const void *p0AB548;
    const void *p0AD300;   /* the one-space string, used as TEXT           */
    const char *p0AD348;   /* "RallySeason*.BRF"                           */
} BrUi73Styles;

typedef struct BrUi73Ctx {
    /* --- injected callees -------------------------------------------- */
    const BrErrHost *pErrHost;      /* 0x1003E260 -- slice1_06.h injects it */
    const BrUi73Hooks  *pHooks;
    BrUi73Styles        aStyles;

    /* the vtable the phase constructor hands to 0x1005CB90 (0x1008F788),
     * a parameter in slice1_06.h's BrNameListInit rather than a literal. */
    const void *pNameListVtbl;      /* 0x1008F788 */

    /* the phase's own vtable, which the original stores as the literal
     * 0x1008F700.  br_phase.h types it; its nine slots live in other packets,
     * so it is wired rather than hardcoded. */
    const BrPhaseVtbl_ *pPhaseVtbl; /* 0x1008F700 */

    /* 0x1003E1D0 -- the paired scratch buffers slice1_06.h models.  NULL
     * means "not wired"; the original always has one. */
    BrPairBuf *pPairBuf;            /* 0x1003E1D0 operand */

    /* 0x10071560 / 0x10071630, called by 0x10071550.  No existing header
     * names either, so they are reached through pointers rather than by
     * coining two more global names. */
    void (*pfn10071560)(void);
    void (*pfn10071630)(void);

    /* `*(int32_t *)((char *)pArg->pSub + 0x70) = 0`, the first thing both
     * 0x10041A00 and 0x100424D0 do.  slice2_25.h models the object as
     * BrGameObj/BrGameSub and slice5_61.c reaches it that way, but
     * slice2_25.h cannot be included here (CONFLICT 1), so the store is
     * routed through a hook rather than dropped or guessed. */
    void (*pfnClearSub70)(void *pArg);

    /* --- plain globals ------------------------------------------------ */
    int32_t   n0AA010;      /* 0x100AA010  = slice3_33.h BrUiBuildCtx::n0AA010 */
    int32_t   n0AC648;      /* 0x100AC648 */
    int32_t   n0AC64C;      /* 0x100AC64C */
    int32_t   n0AC650;      /* 0x100AC650 */
    int32_t   n0AC654;      /* 0x100AC654 */
    int32_t   n0AC658;      /* 0x100AC658 */
    int32_t   nAA2848;      /* 0x10AA2848 */
    int32_t   nAA2880;      /* 0x10AA2880  index into the join-blob array   */
    int32_t   nAA289C;      /* 0x10AA289C */
    int32_t   nAA28A0;      /* 0x10AA28A0 */
    int32_t   nAA28A4;      /* 0x10AA28A4 */
    int32_t   nAA28AC;      /* 0x10AA28AC */
    int32_t   nAA28B0;      /* 0x10AA28B0 */
    int32_t   nAA28B4;      /* 0x10AA28B4 */
    uint8_t   bAA28B8;      /* 0x10AA28B8  a BYTE, not a dword             */
    int32_t   nAA28BC;      /* 0x10AA28BC */
    int32_t   nAA28C0;      /* 0x10AA28C0 */
    int32_t   nAA28C4;      /* 0x10AA28C4 */
    int32_t   nAA28C8;      /* 0x10AA28C8 */
    int32_t   nAA28D0;      /* 0x10AA28D0 */
    int32_t   nAA28EC;      /* 0x10AA28EC */
    int32_t   nAA26E8;      /* 0x10AA26E8 */
    int32_t   nA9D068;      /* 0x10A9D068 */
    int32_t   nA9D06C;      /* 0x10A9D06C */
    int32_t   nAA2A00;      /* 0x10AA2A00 */
    int32_t   nAA2A04;      /* 0x10AA2A04 */
    int32_t   nAA2A08;      /* 0x10AA2A08 */
    int32_t   nAA2A10;      /* 0x10AA2A10 */
    int32_t   nAA2A14;      /* 0x10AA2A14 */
    int32_t   nAA2A34;      /* 0x10AA2A34  = slice2_23.h BrUiGlobals::gAA2A34 */
    uint16_t  wAA27E0;      /* 0x10AA27E0  <- 0x0102                        */

    /* 0x100AB428 / 0x100AB42C are read with `fild`, i.e. they are INTS.
     * slice3_33.h found them 0 and 380; 0x10054B50 uses them as the x and y
     * of its three rectangle controls. */
    int32_t   n0AB428;      /* 0x100AB428  = slice3_33.h BrUiBuildCtx::nAB428 */
    int32_t   n0AB42C;      /* 0x100AB42C  = slice3_33.h BrUiBuildCtx::nAB42C */

    /* --- record arrays ------------------------------------------------- */
    /* 0x10AA29CC -- the twin of slice5_61.h's g_brPAA29D0 (0x10AA29D0).
     * SAME 0x438 geometry, SAME +0x35 name and +0x44C flag; 0x10041A00 uses
     * THIS one and 0x100424D0 uses the OTHER one.  The asymmetry is the
     * original's. */
    unsigned char *pAA29CC;

    /* 0x10AA29D4 + 0x1DE48 + 8*i -- the original indexes an array of 8-byte
     * records and dereferences the first dword of each as a pointer.
     * DEVIATION (LP64): modelled as a plain array of host pointers, so the
     * stride is sizeof(void *) here and 8 in the original. */
    void *const *apJoinBlob;

    /* 0x100B89C8..0x100B89F4 -- twelve car-name pointers. */
    const char *apCarName[12];

    /* --- objects ------------------------------------------------------- */
    BrPhase_ *pAA2908;      /* 0x10AA2908  = slice2_26.h BrPhaseCtx::pAA2908 */
    BrUiCtl_ *pAA29C0;      /* 0x10AA29C0 */
    BrUiCtl_ *pAA29C8;      /* 0x10AA29C8  = slice3_33.h BrUiBuildCtx::pAA29C8 */
    BrUiCtl_ *pAA29F0;      /* 0x10AA29F0 */
    BrUiCtl_ *pAA29F4;      /* 0x10AA29F4 */

    /* 0x10AA26F0 / 0x10A9DBD8 / 0x10220B20 -- the three blocks 0x1003E680
     * clears (0x53, 0x53 and 0x46 dwords).  NULL means "not wired"; the
     * original's storage is static. */
    int32_t  *aAA26F0;
    int32_t  *aA9DBD8;
    int32_t  *a220B20;

    /* 0x10AA2518 and 0x10A9D618 -- the two "%d" scratch strings. */
    char     *szAA2518;
    char     *szA9D618;
    size_t    cbScratch;    /* capacity of both, for the port's snprintf */
} BrUi73Ctx;

/* The single instance.  Defined by slice6_73.c, zero-initialised; a caller
 * (or a test) wires the pointers before the first build. */
extern BrUi73Ctx g_br73;

/* ==========================================================================
 * 5. The packet
 * ========================================================================== */

/* 0x10048710 (310 bytes) -- the phase constructor.  __thiscall; returns this.
 *
 * Zeroes +0x08, +0x0C, +0x10, +0x12, +0x64 and +0xBC, sets +0x68 to 1 and the
 * vtable to 0x1008F700, allocates TWO 0x6594-byte BrNameLists into +0xC0 and
 * +0xC4 (reporting error index 6 for each failure), fills all 100 slots of
 * both with sprintf(BrStrGet(0xBE), i), then zeroes the twenty dwords at
 * +0x6C.
 *
 * GOTCHA: +0x04 (pfnEnter) is NEVER written.  `operator new` does not zero,
 * so it is garbage until a caller installs a hook -- which is exactly what
 * slice2_26.c records from the other end.
 *
 * GOTCHA: +0x0C is zeroed here and set to 1 by each of the installers, so a
 * just-constructed phase and an installed one differ in that one field.
 *
 * GOTCHA: on either allocation failure the original reports error index 6 and
 * carries on with a NULL list, then dereferences it in the fill loop.  The
 * port skips the fill for a NULL list (DEVIATION, memory safety). */
BrPhase_ *BrOptObjCtor(BrPhase_ *pThis);

/* The six builders.  All return 1 in the original; every caller discards it,
 * and all six are declared `void` by their callers, so `void` is kept.
 *
 * GOTCHAS common to all six:
 *  - The page is stored into aPages[nPages] and aFlags[nPages] is set BEFORE
 *    the allocation, using the count as it was on entry; the array store
 *    re-reads the count afterwards.  Both indices are the same in practice.
 *  - iPage (+0x12) is zeroed once, at the top, before anything else.
 *  - A control is stored into apCtl[cCtl] BEFORE the NULL check, and cCtl is
 *    bumped at the END of the block, so a failed allocation leaves a NULL in
 *    the array and still advances the cursor.
 *  - cSel (+0x344) is bumped only for rows that carry a +0x08 hook or a list
 *    sub-object -- never for the plain labels, the root control or the title.
 */

/* 0x10050060 (2433 bytes)  The season/track screen.  TWO pages.
 * GOTCHA: the second page's aFlags entry is set to ZERO, not 1 -- the only
 * place in the packet that does.  And iPage is NOT re-zeroed for it. */
void BrExt_10050060(BrPhase_ *pSelf);

/* 0x1004D640 (1210 bytes)  Six controls; installs 0x10AA29F0 and twice
 * 0x10AA29C8. */
void BrExt_1004D640(BrPhase_ *pSelf);

/* 0x1004DFC0 (2114 bytes)  The car screen.
 * GOTCHA: 0x10AA2A34 is read THREE times and clamped differently each time:
 * to [0,11] for the list cursor, then re-tested for the < 0 / > 11 / in-range
 * interpolation of f1E200..f1E204 by n/11. */
void BrExt_1004DFC0(BrPhase_ *pSelf);

/* 0x1004F2B0 (1091 bytes)  Six controls. */
void BrExt_1004F2B0(BrPhase_ *pSelf);

/* 0x10054B50 (3394 bytes)  The widest one: 21 controls, three of which build
 * an integer rectangle out of the same floats they just passed to f38.
 * GOTCHA: the rectangle's x comes from __ftol(0x100AB428) and its y from
 * __ftol of the row cursor -- BUT only the FIRST of the three calls __ftol
 * on the x; the other two reuse the ebx it left behind, so all three share a
 * left edge even though only the first computed one. */
void BrExt_10054B50(BrPhase_ *pSelf);

/* 0x100558A0 (2877 bytes)  17 controls; sets 0x100AA010 to 6 first. */
void BrOptFn100558A0(BrPhase_ *pSelf);

/* 0x10041A00 (179 bytes)  Commit the edited name into record g_br0AB3F4 of
 * the 0x10AA29CC array.  The twin of slice5_61.c's BrExt_10042410, which
 * does the same to the 0x10AA29D0 array.  Always returns 1.
 *
 * GOTCHA: the flag at +0x44C is written with "the flag WAS zero" and then
 * RE-READ from memory, so 0x10AA28D8 is always 0 or 1.
 * GOTCHA: +0x44C is 0x14 bytes past the end of record n under the stride the
 * index math implies (0x438).  Reproduced, not "corrected". */
int32_t BrExt_10041A00(void *pArg);

/* 0x100424D0 (144 bytes)  The restore half: put the saved name back into
 * record g_br0AB3F4 of the 0x10AA29D0 array and reload the edit buffer.
 * Always returns 1.
 *
 * GOTCHA: it tests the ADDRESS 0x10A9D078 against zero -- a literal, so the
 * branch is dead.  Kept as an always-true condition.
 * GOTCHA: it reads the 0x10AA29CC flag that 0x10041A00 published (via
 * 0x10AA28D8) but writes into the 0x10AA29D0 array. */
int32_t BrExt_100424D0(void *pArg);

/* 0x1003E680 (287 bytes)  The "new session" global reset.  Two wanted names,
 * one body, `void (void)` both ways (slice2_25.h BrSub1003E680,
 * slice2_26.h BrExt_1003E680).
 *
 * GOTCHA: it prints "%d" twice -- once with the literal 1 into 0x10AA2518,
 * once with 0x10AA28A4 + 1 into 0x10A9D618 -- and 0x10AA28A4 was zeroed four
 * instructions earlier, so the second is always "1" as well.
 * GOTCHA: 0x10AA289C is cleared TWICE, once before 0x1003E1D0 and once
 * after, with nothing in between that could change it. */
void BrSub1003E680(void);
void BrExt_1003E680(void);

/* 0x1003D030 (55 bytes)  Copy the 16-byte join blob for the current session.
 * Returns 0 on every path, including both "nothing to copy" exits -- the
 * caller (slice4_50.c) treats it as an HRESULT and therefore always sees
 * success. */
int32_t BrSub1003D030(void *pBlob);

/* 0x10071550 (16 bytes)  Two calls, then `mov eax,1`.  The caller declares
 * it `void` (slice4_50.h), so the 1 is dropped. */
void BrSub10071550(void);

/* 0x10031140 -- ADAPTER.  See CONFLICT 4.  Delegates to slice1_05.c's
 * BrMat4Translate after reinterpreting the two int32_t bit patterns. */
void BrSub_10031140(BrMat4 *pM, int32_t a, int32_t b, float c);

/* ==========================================================================
 * 6. The collision grid (0x1006F720)
 * ========================================================================== */

/* The four resident cells.  slice2_11.h already owns the plane storage
 * (0x11750338) and the per-cell count (0x117554A0); these three are the rest
 * of the cache.
 *
 * CONFLICT 6: slice3_42.h ALSO models this whole block, as
 * `BrFxRecord g_BrFx1750338[600]` (600 == 4 * 150, the same 32-byte records)
 * plus `g_BrX17554A0/A4`, `g_BrX17554C8/CC`, `g_BrX17554D0/D4`,
 * `g_BrX17554D8/DC` and `g_BrX17554E0/E4` as bare int32 pairs.  The two views
 * are consistent -- the pairs are exactly the u16 key array and the u32
 * timestamp array this file names -- but ONE OF THE TWO MUST OWN THE STORAGE.
 * They must not both be linked into one image as they stand. */
#define BR73_COLL_CELLS 4

extern uint32_t g_aBrCollGridStamp[BR73_COLL_CELLS];  /* 0x117554C8..0x117554D7 */
extern int16_t  g_aBrCollGridKey[BR73_COLL_CELLS];    /* 0x117554D8..0x117554DF */
extern uint32_t g_brCollGridClock;                    /* 0x117554E0 */

/* The two table bases 0x10002DE0 and 0x10002EF0 took as globals in the
 * original and slice1_01.c takes as parameters.  Declared here because
 * 0x1006F720 is the caller that has to supply them and no header names them
 * yet; slice1_01.h records the addresses in prose only. */
extern const uint16_t *g_pBrGrid64;     /* 0x106C7C6C -- BrGrid64Sample's grid */
extern const uint16_t *g_pBrTriTable;   /* 0x106C7C68 -- BrU16CursorNext's table */

/* The three per-triangle tables 0x1006F720 reads.
 *   0x106C7C54  three u16 vertex indices per triangle, 8-byte stride
 *   0x106C7C5C  the vertex array (12-byte BrVec3)
 *   0x106C7CDC  one surface byte per triangle, masked with 7 */
extern const uint16_t *g_pBrCollTriIdx;    /* 0x106C7C54, 4 u16 per record */
extern BrVec3         *g_pBrCollVerts;     /* 0x106C7C5C */
extern const uint8_t  *g_pBrCollTriFlags;  /* 0x106C7CDC */

#endif /* SLICE6_73_H */
