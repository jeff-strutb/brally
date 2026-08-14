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
 *   slice3_33.h   BrUiScreen      the 0x348 page (+0x10/+0x14/+0x18/+0x338/
 *                                 +0x33C/+0x340/+0x344)
 *                 BrUiCtl         the 0x1E214 control, and BrUiCtlVtbl's
 *                                 f34 (set text) / f38 (place)
 *                 BrUiCtlSub      the sub-object at control +0x3838, and its
 *                                 f10 (append row) / f14 (configure)
 *                 BrOperatorNew, BrUiScreenCtor, BrUiCtlCtor, BrStrGet
 *   slice1_06.h   BrErrHost / BrErrShow          (0x1003E260)
 *                 BrOptCaps / BrOptAvailA        (0x1003F2B0's real body)
 *   slice3_32.h   BrScrItemVtbl   the vtable of the item object embedded at
 *                                 control +0x2B5C (slice2_23.h calls the same
 *                                 thing BrUiWidgetVtbl). Only slot +0x04 is
 *                                 called here.
 *   br_crt.h      BrFtolTrunc                    (0x1007C8A0)
 *
 * BrUiCtlX below is NOT a fifth control model. It is a STRICT EXTENSION:
 * `BrUiCtl base` is its first member, so the two share an initial sequence
 * and the object handed to BrUiCtlCtor is the very same object. Three of the
 * four builders here reach fields slice3_33.h's five builders never touched
 * (+0x10, +0x1E210, +0x296C, the step arrays at +0x2978/+0x2A40, the item
 * block at +0x2B5C, +0x383C); those fields -- and only those -- are added.
 *
 * SIGNATURE CONFLICTS (reported, deliberately not "resolved") -- see the
 * PARAMETER TYPE block further down.
 *
 * BYTE OFFSETS ARE 32-BIT-ONLY. Every offset in a comment is the original's;
 * on LP64 the structs are larger and every allocation goes through
 * BR_ALLOC_SIZE / BR71_CTLX_ALLOC, never through a size literal.
 */
#ifndef SLICE6_71_H
#define SLICE6_71_H

#include <stddef.h>
#include <stdint.h>

#include "br_phase.h"    /* BrPhase_ -- canonical 0xC8 phase object          */
#include "br_crt.h"      /* BrFtolTrunc -- 0x1007C8A0                        */
#include "slice3_33.h"   /* BrUiScreen / BrUiCtl / BrUiCtlSub / the ctors    */
#include "slice3_32.h"   /* BrScrItemVtbl only -- the control's item vtable  */

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
 * 1. BrUiCtlX -- BrUiCtl plus the fields these four builders reach
 * ========================================================================== */

/* The step table at control +0x2978 (int32 durations) / +0x2A40 (uint16
 * codes). 0x10051D30 fills entries 0..14 then 15..23 with different codes,
 * and +0x29B4 == +0x2978 + 15*4 and +0x2A5E == +0x2A40 + 15*2 exactly, which
 * is what shows the two loops are one array of 24 and not two arrays. */
#define BR71_STEP_COUNT 24

/* The item text buffer at control +0x2B65 runs up to the int16 at +0x2F66
 * (slice3_32.h's BR_SCR_UI_ITEMW40A), so it is 0x401 bytes. */
#define BR71_ITEM_TEXT 0x401

/* The item object embedded at control +0x2B5C. Only its vtable, its text and
 * the five fields at +0x2F78..+0x2F8C are used here; the offsets in the
 * comments are relative to the CONTROL, matching the original's operands. */
typedef struct BrUiCtlItemX {
    const BrScrItemVtbl *pVtbl;              /* ctl +0x2B5C, item +0x000 */
    char                 szText[BR71_ITEM_TEXT]; /* ctl +0x2B65, item +0x009 */
    uint16_t             w2F78;              /* ctl +0x2F78, item +0x41C */
    int32_t              f2F80;              /* ctl +0x2F80, item +0x424 */
    int32_t              f2F84;              /* ctl +0x2F84, item +0x428 */
    int32_t              f2F88;              /* ctl +0x2F88, item +0x42C */
    int32_t              f2F8C;              /* ctl +0x2F8C, item +0x430 */
} BrUiCtlItemX;

/* STRICT EXTENSION of slice3_33.h's BrUiCtl -- see the header comment. */
typedef struct BrUiCtlX {
    BrUiCtl      base;                   /* every field slice3_33.h models */

    void        *f10;                    /* +0x0010  a __cdecl hook slot   */
    void        *p1E210;                 /* +0x1E210 -> 0x10A9DA50         */
    int32_t      f296C;                  /* +0x296C                        */
    int32_t      aStepMs[BR71_STEP_COUNT];  /* +0x2978, stride 4           */
    uint16_t     aStepId[BR71_STEP_COUNT];  /* +0x2A40, stride 2           */
    BrUiCtlItemX item;                   /* +0x2B5C                        */
    void        *f383C;                  /* +0x383C, i.e. sub-object +0x04 */
} BrUiCtlX;

/* What the port allocates for one control. Never the 0x1E214 literal: on a
 * 64-bit host BrUiCtlX is larger, and the original's size would truncate the
 * object. (Same DEVIATION slice3_33.h's BR_ALLOC_SIZE documents.) */
#define BR71_CTLX_ALLOC  BR_ALLOC_SIZE(BrUiCtlX, BR_UI_CTL_ORIG_SIZE)

/* ==========================================================================
 * 2. The hook slots these builders INSTALL but never call
 *
 * Same treatment as slice3_33.h's BrUiBuildHooks, and for the same reason:
 * several of these addresses already carry a name with an incompatible shape
 * elsewhere in port/include, so reaching them through a table creates no new
 * name and contradicts no existing declaration.
 * ========================================================================== */
typedef struct BrS71Hooks {
    /* stored into control +0x04 */
    BrUiCtlFn p1003EAE0;
    BrUiCtlFn p1003F210;
    BrUiCtlFn p1003F720;      /* unused by these four; kept for symmetry */
    BrUiCtlFn p10040A50;
    BrUiCtlFn p10040AC0;
    BrUiCtlFn p10041300;
    BrUiCtlFn p10041890;
    /* stored into control +0x08 */
    BrUiCtlFn p100443E0;
    BrUiCtlFn p100444C0;
    BrUiCtlFn p10042B00;
    BrUiCtlFn p10045090;
    BrUiCtlFn p100450C0;
    BrUiCtlFn p10046E10;
    BrUiCtlFn p10046F60;
    BrUiCtlFn p10046FC0;
    BrUiCtlFn p100471B0;
    /* stored into control +0x0C */
    BrUiCtlFn p10047360;
    /* stored into control +0x10 */
    BrUiCtlFn p1003F280;
    /* stored into control +0x383C */
    BrUiCtlFn p10042170;
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

    /* --- 0x10038F30 ---------------------------------------------------- */
    BrPhase_ *pAA2904;     /* 0x10AA2904 -- the current phase               */
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
    BrUiCtlX *pAA29B0;     /* 0x10AA29B0 <- 0x10049F40's third control      */
    BrUiCtlX *pAA29BC;     /* 0x10AA29BC <- 0x100575F0's sixth control      */
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
