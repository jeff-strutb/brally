/* slice6_71.h -- packet 71: seven addresses that every other module already
 * calls and nobody defined.
 *
 * WHAT IS HERE
 * ------------
 *   0x1003F2B0  BrSub1003F2B0     option-availability predicate (ADAPTER --
 *                                 the body already lives in slice1_06.c)
 *   0x1003BF60  BrSub1003BF60     network/session teardown
 *   0x10038F30  BrExt_10038F30    the process shutdown sequence
 *   0x10049F40  BrExt_10049F40  } four menu-screen builders of the family
 *   0x10051D30  BrOptFn10051D30 } slice3_33.h decompiled five times over
 *   0x1004F700  BrExt_1004F700  }
 *   0x100575F0  BrOptFn100575F0 }
 *
 * NOT HERE (see the pass report):
 *   0x10029470, 0x1003C740, 0x10052F50, 0x10059BB0.
 *
 * TYPES -- WHAT IS REUSED AND FROM WHERE
 * --------------------------------------
 * Nothing below re-models an object some other header already owns.
 *
 *   br_phase.h    BrPhase_        the 0xC8 phase/screen object. CANONICAL.
 *                                 Used for every `this` in the four builders
 *                                 and for 0x10038F30's 0x10AA2904 slot.
 *   br_ui.h       BrUiPage_       the 0x348 page. CANONICAL.
 *                 BrUiCtl_        the 0x1E214 control. CANONICAL, and the
 *                                 whole of it -- including the step arrays,
 *                                 the three text boxes at +0x2B5C and the
 *                                 embedded BrTextList at +0x3838.
 *                 BrUiCtlVtbl_    f34 (set text) / f38 (place)
 *                 BrUiCtlHookFn_  the +0x04..+0x18 hook slots
 *   slice3_39.h   BrTextBox / BrTextList, reached THROUGH br_ui.h -- the
 *                 control's aText[] and list members are those objects.
 *   slice1_06.h   BrErrHost / BrErrShow          (0x1003E260)
 *                 BrOptCaps / BrOptAvailA        (0x1003F2B0's real body)
 *   br_crt.h      BrOperatorNew (0x1007DFE0), BrFtolTrunc (0x1007C8A0)
 *
 * MIGRATED OFF slice3_33.h (2026 pass). This header used to model the page as
 * slice3_33.h's `BrUiScreen` -- which begins at +0x10 and has no pVtbl /
 * pfn04 / pfn08 -- and the control as `BrUiCtlX`, a private extension of
 * slice3_33.h's `BrUiCtl`. Both are gone. The measured cost of keeping them
 * was that port/host/brally.c, which reads pages through a model that begins
 * at +0x00, read cCtl three fields off the end of what this module wrote and
 * reported 9, 10, 12, 7 and 10 controls across five runs of one binary for a
 * builder whose disassembly says 4. See CONVENTIONS.md, "Two models of one
 * object, shifted".
 *
 * Every field BrUiCtlX added is named by br_ui.h and is used by that name now:
 *
 *     BrUiCtlX::f10        -> BrUiCtl_::pfn10          (+0x0010)
 *     BrUiCtlX::p1E210     -> BrUiCtl_::p1E210         (+0x1E210)
 *     BrUiCtlX::f296C      -> BrUiCtl_::f296C          (+0x296C)
 *     BrUiCtlX::aStepMs[]  -> BrUiCtl_::aStepMs[]      (+0x2978, FIFTY, ADJ-3)
 *     BrUiCtlX::aStepId[]  -> BrUiCtl_::aStepId[]      (+0x2A40, FIFTY)
 *     BrUiCtlX::item       -> BrUiCtl_::aText[0]       (+0x2B5C, ADJ-1/ADJ-2)
 *     BrUiCtlX::f383C      -> BrUiCtl_::list.f04       (+0x383C, ADJ-6)
 *
 * and every field the OLD base carried moved with it: f50/f54/f58/f5C are
 * rcLeft/rcTop/rcRight/rcBottom, f1E20C is w1E20C, f1E1F4 is list.f1A99C[8],
 * and the sub-object at +0x3838 is the BrTextList itself rather than a
 * one-field stand-in.
 *
 * BR71_STEP_COUNT is deleted rather than corrected: it said 24, which was the
 * highest index 0x10051D30 happens to write, and the constructor's
 * `mov ecx,0x32 / lea edi,[esi+0x2978] / rep stosd` says 50. Use
 * BR_UI_CTL_STEPS.
 *
 * SIGNATURE CONFLICTS (reported, deliberately not "resolved") -- see the
 * PARAMETER TYPE block further down.
 *
 * BYTE OFFSETS ARE 32-BIT-ONLY. Every offset in a comment is the original's;
 * on LP64 the structs are larger and every allocation goes through
 * BR_UI_PAGE_ALLOC_SIZE / BR_UI_CTL_ALLOC_SIZE, never through a size literal.
 */
#ifndef SLICE6_71_H
#define SLICE6_71_H

#include <stddef.h>
#include <stdint.h>

#include "br_phase.h"    /* BrPhase_ -- canonical 0xC8 phase object          */
#include "br_phasecur.h" /* BR_PHASE_CUR -- 0x10AA2904, the ONE current phase */
#include "br_crt.h"      /* BrOperatorNew, BrFtolTrunc -- 0x1007C8A0         */
#include "br_ui.h"       /* BrUiPage_ / BrUiCtl_ -- CANONICAL. Pulls
                          * slice3_39.h for BrTextBox / BrTextList.          */
#include "slice1_06.h"   /* BrErrHost / BrErrShow, BrOptCaps / BrOptAvailA   */

/* ==========================================================================
 * PARAMETER TYPES -- the conflict this file inherits
 *
 * The four builders are called as
 *
 *   slice3_31.h:244  void BrExt_10049F40 (BrPhase   *pSelf);
 *   slice3_31.h:247  void BrExt_1004F700 (BrPhase   *pSelf);
 *   slice2_25.h:466  void BrOptFn10051D30(BrOptObj  *pThis);
 *   slice2_25.h:470  void BrOptFn100575F0(BrOptObj  *pThis);
 *
 * `BrPhase` (slice2_26.h) and `BrOptObj` (slice2_25.h) are the SAME original
 * class as br_phase.h's BrPhase_, but both model only five fields and neither
 * has +0x10 (page count), +0x14 (page array) or +0x6C (parallel flag array),
 * which is all these four bodies touch. br_phase.h exists precisely to end
 * that, and is what is used here. ARITY, RETURN TYPE AND NAME MATCH THE
 * CALLERS EXACTLY; only the pointee type differs, and it differs by being the
 * correct one. Integration must retype the three declaring headers, not cast.
 *
 * 0x1003BF60 additionally carries TWO wanted names: BrSub1003BF60
 * (slice2_25.h:424) and BrExt_1003BF60 (slice2_26.h:255), identical `void
 * (void)` shape. The packet asked for the first; the second is provided as a
 * thin alias so both link, and integration should delete one.
 * ========================================================================== */

/* ==========================================================================
 * 1. The list slots 0x1004F700 calls
 *
 * br_ui.h's ADJ-6 established that control +0x3838 is slice3_39.h's
 * BrTextList in full, so the sub-object this module used to model as a lone
 * vtable slot (slice3_33.h's BrUiCtlSub) is that object and its `f10` /
 * `f14` are BrTextListVtbl's. slice3_39.h types both slots, so nothing is
 * re-declared here; the two call sites are
 *
 *     1004F94F  push 0x100ab538 / push 0x40001 / ... ff 50 14   -- five args
 *     1004F99F  push 1 / push 0x100ab4d8 / ... ff 50 10         -- five args
 *
 * ==========================================================================
 * 2. The hook slots these builders INSTALL but never call
 *
 * Same treatment as slice3_33.h's BrUiBuildHooks, and for the same reason:
 * several of these addresses already carry a name with an incompatible shape
 * elsewhere in port/include, so reaching them through a table creates no new
 * name and contradicts no existing declaration.
 *
 * The slots take br_ui.h's BrUiCtlHookFn_ (`int32_t (*)(BrUiCtl_ *)`, ADJ-8)
 * rather than slice3_33.h's shapeless `void (*)(void *)`: 0x10048530 pushes
 * the control and USES the result of +0x14, and the same push/call/add-4
 * shape appears on +0x04.
 * ========================================================================== */
typedef struct BrS71Hooks {
    /* stored into control +0x04 */
    BrUiCtlHookFn_ p1003EAE0;
    BrUiCtlHookFn_ p1003F210;
    BrUiCtlHookFn_ p1003F720;   /* unused by these four; kept for symmetry */
    BrUiCtlHookFn_ p10040A50;
    BrUiCtlHookFn_ p10040AC0;
    BrUiCtlHookFn_ p10041300;
    BrUiCtlHookFn_ p10041890;
    /* stored into control +0x08 */
    BrUiCtlHookFn_ p100443E0;
    BrUiCtlHookFn_ p100444C0;
    BrUiCtlHookFn_ p10042B00;
    BrUiCtlHookFn_ p10045090;
    BrUiCtlHookFn_ p100450C0;
    BrUiCtlHookFn_ p10046E10;
    BrUiCtlHookFn_ p10046F60;
    BrUiCtlHookFn_ p10046FC0;
    BrUiCtlHookFn_ p100471B0;
    /* stored into control +0x0C */
    BrUiCtlHookFn_ p10047360;
    /* stored into control +0x10 */
    BrUiCtlHookFn_ p1003F280;
    /* stored into control +0x383C, i.e. the embedded list's +0x04. That slot
     * is slice3_39.h's BrTextList::f04 and its type is BrTextListCbFn --
     * a DIFFERENT shape from the control hooks above, and deliberately so:
     * nothing in the port calls it, and 0x1004F96F stores a code address
     * there, not a number. */
    BrTextListCbFn p10042170;
} BrS71Hooks;

/* ==========================================================================
 * 3. The callees and platform services 0x1003BF60 / 0x10038F30 need
 *
 * Both are pure call sequences over addresses that live outside this packet.
 * The original takes no context argument, and the wanted signatures are
 * `void (void)` and `void (int32_t)`, so the table is reached through a
 * file-scope pointer rather than an added parameter.
 * ========================================================================== */
typedef struct BrS71Env {
    /* --- 0x1003BF60 ---------------------------------------------------- */
    void (*pfn100586A0)(void);      /* resets the 0x10AA2538 slot table     */
    void (*pfnKillTimer)(void *hWnd, uint32_t idEvent);  /* USER32          */
    void (*pfn10072270)(void);
    void (*pfn1003C550)(void);

    /* --- 0x10038F30 ---------------------------------------------------- */
    void (*pfn1002C4A0)(void);
    void (*pfn10016990)(void);
    void (*pfn10079550)(void);
    void (*pfn10078BC0)(void);
    void (*pfn10078DB0)(void);
    void (*pfn10073730)(void);
    void (*pfn10005BE0)(int32_t a);
    void (*pfn1003BFD0)(void);
    void (*pfn10002CF0)(void);
    void (*pfn10008B80)(void);      /* a STUB in this build -- kept anyway  */
    void (*pfn10061620)(void);
    void (*pfn10008970)(void *pThis);   /* __thiscall, ecx = 0x10A99780     */
    void (*pfn1002AEA0)(void);
    void (*pfn10074050)(void);
    void (*pfnCoUninitialize)(void);
    void (*pfnExit)(int32_t code);  /* 0x1007CC00                           */

    /* --- 0x1004F700 ---------------------------------------------------- */
    /* 0x1007CE90 / 0x1007CD50: "does this file open?". The original keeps
     * the FILE* only to close it again. */
    void *(*pfnFopen)(const char *pszPath, const char *pszMode);
    void  (*pfnFclose)(void *pFile);
} BrS71Env;

/* ==========================================================================
 * 4. The globals this range reads or writes
 *
 * Gathered exactly as slice2_26.h gathers BrPhaseCtx and slice3_32.h gathers
 * BrScrGlobals: named for their addresses, because none of them has an
 * establishable semantic name here. Where another header already owns the
 * same address, the field is the alias point for integration -- storage must
 * not be defined twice.
 * ========================================================================== */

/* 0x10A9D018 -- an 80-byte buffer (0x1003C740 fills it with `rep movsd`,
 * ecx = 0x14, from the joined session's name). 0x100575F0 reads it as a C
 * string. slice2_23.h / slice5_61.h / slice5_63.h also model this address. */
#define BR71_A9D018_SIZE 80

typedef struct BrS71Globals {
    /* --- 0x1003BF60 ---------------------------------------------------- */
    void    *hWnd680584;   /* 0x10680584 -- the window KillTimer is given   */
    uint32_t nA9BFDC;      /* 0x10A9BFDC -- the timer id                    */
    int32_t  nAA2884;      /* 0x10AA2884 */
    int32_t  nAA287C;      /* 0x10AA287C -- 2 or 3 skip the entity reset    */
    int32_t  nA9CFFC;      /* 0x10A9CFFC */
    int32_t  n22AF18;      /* 0x1022AF18 -- also read by 0x10038F30         */
    int32_t  nAA2888;      /* 0x10AA2888 */

    /* 0x10AA29D8 -- an entity record (stride 0x2B68). Only the byte at
     * +0x2B64 and the flags at +0x1C are touched, exactly as slice2_26.h
     * says of the same object, so only those two are modelled. */
    uint8_t *pAA29D8_b2B64;
    int32_t *pAA29D8_f1C;

    /* --- 0x10038F30 ----------------------------------------------------
     * 0x10AA2904, the current phase, is NOT a member here.  It is the same
     * dword as br_uinav.h's BrUiNav::pAA2904, and a second copy of it meant
     * 0x10038F30 tested a phase nothing else ever set.  Use BR_PHASE_CUR. */
    int32_t   n0AC300;     /* 0x100AC300 */
    int32_t   n0940A4;     /* 0x100940A4 */
    void    (*pfnB501CC)(void);    /* 0x10B501CC -- called if non-null      */
    void    (*pfn18AA0D0)(void);   /* 0x118AA0D0 -- called if non-null      */
    void    (*pfn690A28)(void);    /* 0x10690A28 -- called if non-null      */
    void     *pA99780;     /* 0x10A99780 -- the `this` of 0x10008970        */

    /* --- the builders --------------------------------------------------- */
    BrPhase_ *pAA2908;     /* 0x10AA2908 -- the phase whose +0xC0 holds the
                            *  file-list object 0x1004F700 rescans          */
    int32_t   n0AB3F4;     /* 0x100AB3F4 <- -1 */
    int32_t   nAA2848;     /* 0x10AA2848 <- 1 around the rescan, then 0     */
    BrUiCtl_ *pAA29B0;     /* 0x10AA29B0 <- 0x10049F40's third control      */
    BrUiCtl_ *pAA29BC;     /* 0x10AA29BC <- 0x100575F0's sixth control      */
    void     *pA9DA50;     /* 0x10A9DA50 -- stored into control +0x1E210    */
    char     *pA9D018;     /* 0x10A9D018 -- BR71_A9D018_SIZE bytes          */

    /* style / text blocks; the original pushes their ADDRESSES ------------ */
    const void *p0AB438;
    const void *p0AB448;
    const void *p0AB468;
    const void *p0AB478;
    const void *p0AB488;
    const void *p0AB4D8;
    const void *p0AB508;
    const void *p0AB538;

    /* 0x1039B720 -- past the end of the DLL's initialised data, so at load
     * time this is an empty string (slice1_06.h says the same of it). The
     * original pushes it as a literal text pointer, not through BrStrGet. */
    const char *p39B720;

    /* string literals the original embeds */
    const char *pszRallySeasonBrf;  /* 0x100AD348 "RallySeason*.BRF" */
    const char *pszAutoSaveBrf;     /* 0x100AD310 "AutoSave.brf"     */
    const char *pszFopenMode;       /* 0x100AD1F0                    */

    /* --- injected, per the precedent of slice3_33.h ---------------------- */
    const BrErrHost  *pErrHost;    /* 0x1003E260's host   */
    const BrS71Hooks *pHooks;
    const BrOptCaps  *pOptCaps;    /* 0x1003F2B0's state  */
} BrS71Globals;

/* Storage lives in slice6_71.c. The wanted signatures leave no room for a
 * context argument, so this is the seam integration wires up. */
extern BrS71Globals    g_brS71;
extern const BrS71Env *g_brS71Env;

/* ==========================================================================
 * 4b. Cross-slice callees
 *
 * These three used to arrive through slice3_33.h, whose page/control models
 * this module no longer uses. They are re-declared here over the CANONICAL
 * types rather than reached by a cast: the name, arity and return type are
 * unchanged, only the pointee is, and it is the merged one. This is the same
 * move slice6_73.h and port/host/br_wire72.c already make for the same two
 * constructors -- see br_ui.h's opening note.
 * ========================================================================== */

/* XSLICE 0x10048470 -- __thiscall page constructor; returns `this`.
 * slice3_33.h calls it BrUiScreenCtor over a model that begins at +0x10 and
 * therefore cannot write the vtable, +0x04/+0x08/+0x0C or +0x346. This is the
 * name slice3_32.h, slice6_73.h and br_uivt.h all use for the full object. */
extern BrUiPage_ *BrUiPageCtor_10048470(BrUiPage_ *pThis);

/* XSLICE 0x100476C0 -- __thiscall control constructor; returns `this`. */
extern BrUiCtl_ *BrUiCtlCtor(BrUiCtl_ *pThis);

/* XSLICE 0x10074030 -- string-table lookup by id; NULL for a bad id.
 * BrStrGet is the name slice2_23.h, slice2_25.h, slice3_33.h and slice6_73.h
 * all use, with this exact signature. */
extern const char *BrStrGet(int id);

/* The walk 0x1004F700 makes over the file-list object at phase +0xC0.
 * 0x6590 == 100 * 0x104 - 4, so `for (k = 0; k < 0x6590; k += 0x104)` runs
 * exactly 100 times -- the same 100 x 0x104 name array slice1_06.h models as
 * BR_NAMELIST_COUNT / BR_NAMELIST_STRIDE. */
#define BR71_LIST_STRIDE 0x104
#define BR71_LIST_BYTES  0x6590
#define BR71_LIST_ROWS   100

/* The vtable slot the file-list object at phase +0xC0 exposes. 0x1004F700
 * calls `[[phase+0xC0]] + 4` with one string, and 0x10059BB0 (not ported)
 * calls the same slot on phase +0xC4. */
typedef struct BrS71FileList  BrS71FileList;
typedef struct BrS71FileListVtbl {
    void *f00;
    void (*f04)(BrS71FileList *pThis, const char *pszPattern);
} BrS71FileListVtbl;
struct BrS71FileList { const BrS71FileListVtbl *pVtbl; };

/* ==========================================================================
 * 5. The range itself
 * ========================================================================== */

/* 0x1003F2B0. ADAPTER, not a decompilation: slice1_06.c already carries this
 * exact body as BrOptAvailA(const BrOptCaps *, uint32_t). Per the contract's
 * "reuse, never coin a fifth name" rule this entry only supplies the state
 * pointer and keeps the callers' `int (int)` shape.
 *
 * GOTCHA preserved by BrOptAvailA and visible through here: index 12 is a
 * reserved sentinel that returns 0 before the force-available flag is even
 * consulted, and the result is the MASKED BIT, not a normalised 0/1. */
extern int BrSub1003F2B0(int index);

/* 0x1003BF60. Session teardown. */
extern void BrSub1003BF60(void);
/* The second wanted name for the same address -- see the CONFLICT block. */
extern void BrExt_1003BF60(void);

/* 0x10038F30. The shutdown sequence; `a` is handed straight to exit(). */
extern void BrExt_10038F30(int32_t a);

/* The four screen builders. All four return 1 in the original and all four
 * are declared void by their callers; the value is dropped. */
extern void BrExt_10049F40 (BrPhase_ *pSelf);
extern void BrOptFn10051D30(BrPhase_ *pThis);
extern void BrExt_1004F700 (BrPhase_ *pSelf);
extern void BrOptFn100575F0(BrPhase_ *pThis);

#endif /* SLICE6_71_H */
