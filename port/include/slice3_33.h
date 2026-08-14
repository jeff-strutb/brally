/* slice3_33.h -- BRD3D.dll 0x1004A580-0x1004D1E9, agent 33.
 *
 * WHAT THIS RANGE ACTUALLY IS
 * ---------------------------
 * The packet was flagged as "probably x87-heavy physics or geometry". It is
 * not. All five functions are MENU SCREEN BUILDERS -- the "enter hooks" that
 * slice2_26.h already identified by address (BrExt_1004A580, BrExt_1004B430,
 * BrExt_1004BDC0, BrExt_1004C4A0; 0x1004CAC0 is named as the enter hook of
 * 0x10045390 in slice2_25.h). Each one is a straight line of
 *
 *     p = operator new(0x1E214);  p = p ? ctor(p) : NULL;
 *     screen->apCtl[screen->cCtl] = p;
 *     p->pVtbl->f38(p, phase, x, y, flags, 2, 5, a6, a7);   // place
 *     p->pVtbl->f34(p, text, a2, a3, style);                // label
 *     screen->cCtl++;
 *
 * repeated ten to twenty times. There is no algorithm here; the value is in
 * the exact coordinates, ids, flags and ordering, and in the handful of
 * places where the pattern breaks (see GOTCHAS below).
 *
 * x87: there is NOT ONE `fxch` in the whole 11,333-byte packet, and every
 * float instruction is `fld m32 / fadd m32 / fsub m32 / fstp m32 / fild m32`
 * with a memory operand, so no operand order is ambiguous. `fsub m32` is the
 * non-reversed form: st(0) = st(0) - m32. All seven .rdata constants used as
 * subtrahends are NEGATIVE, so every `fsub` is an ADDITION of a row offset:
 *
 *     0x1008F680 = -19    0x1008F684 = -38    0x1008F688 = -57
 *     0x1008F68C = -76    0x1008F690 = -95    0x1008F694 = -114
 *     0x1008F698 = -133   0x1008F69C = -33
 *
 * (read out of orig/BRD3D.dll .rdata with tools/pe.py, not assumed).
 *
 * OBJECT LAYOUTS
 * --------------
 * Three heap objects appear. Following the precedent of slice2_26.h, the
 * structs below use natural C layout and carry the original's 32-bit byte
 * offsets as comments; they are NOT laid out to match a 32-bit image.
 *
 *   phase   0xC8 bytes   built elsewhere (ctor 0x10048710)
 *   screen  0x348 bytes  operator new + ctor 0x10048470
 *   control 0x1E214 bytes operator new + ctor 0x100476C0
 *
 * CONFLICT THE COORDINATOR MUST RESOLVE
 * -------------------------------------
 * slice2_26.h declares these same five addresses as
 * `void BrExt_1004A580(BrPhase *)` and models BrPhase with five fields
 * (pVtbl, pfn04, pfn08, f0C, f68). That model cannot be used here: this range
 * reads the phase's screen count at +0x10, a screen-pointer array at +0x14
 * and a parallel int array at +0x6C, and slice2_26.h's `f68` occupies +0x10
 * in its layout. BrUiPhase below is the layout the field offsets actually
 * imply. The two headers must not be included in the same translation unit
 * until one of the two is folded into the other; slice2_26.h's extern block
 * for these five addresses is a placeholder and this file is the owner.
 *
 * NAMING
 * ------
 * 0x10074030 already carries THREE names in port/include (BrStrGet in
 * slice2_23.h and slice2_25.h, BrStringById in slice2_24.h, BrHandleLookup in
 * br_bits.h). BrStrGet is used here because it has two of the three votes and
 * the exact signature `const char *(int)` of slice2_25.h.
 *
 * Everything this range only ever STORES as a function pointer, or calls with
 * a signature some other slice already claimed under an incompatible shape
 * (0x10045110 = BrPhaseActivate_10045110, 0x100400E0 = BrUiText100400E0,
 * 0x10040330 = BrCfgFindConflicts, ...), is reached through BrUiBuildHooks /
 * BrUiBuildCtx rather than through a fresh extern, so no address gets a
 * fourth name and no declaration conflicts with an existing one.
 */
#ifndef SLICE3_33_H
#define SLICE3_33_H

#include <stddef.h>
#include <stdint.h>

#include "slice1_06.h"   /* BrErrHost / BrErrShow -- 0x1003E260 */

typedef struct BrUiPhase       BrUiPhase;
typedef struct BrUiScreen      BrUiScreen;
typedef struct BrUiCtl         BrUiCtl;
typedef struct BrUiCtlVtbl     BrUiCtlVtbl;
typedef struct BrUiCtlSub      BrUiCtlSub;
typedef struct BrUiCtlSubVtbl  BrUiCtlSubVtbl;

/* ==========================================================================
 * The control (0x1E214 bytes)
 * ========================================================================== */

/* The original's +0x04 / +0x08 / +0x0C / +0x18 slots hold plain __cdecl
 * pointers. This range only ever STORES them, never calls them, so the exact
 * parameter list is not established here -- hence one positional argument. */
typedef void (*BrUiCtlFn)(void *pArg);

struct BrUiCtlVtbl {
    void *aReserved[13];   /* +0x00..+0x30 -- untouched in this range */

    /* +0x34 __thiscall. Sets the control's text. Four arguments; when the
     * text comes from the string table the call site is
     * `BrStrGet(id)` and only ONE of the four pushes is cleaned by the
     * caller, which is how the shape was pinned down. */
    void (*f34)(BrUiCtl *pThis, const void *pText,
                int32_t a2, int32_t a3, const void *pStyle);

    /* +0x38 __thiscall. Places the control. a4 is 2 and a5 is 5 at EVERY
     * one of the ~60 call sites in this packet; a6/a7 vary. */
    void (*f38)(BrUiCtl *pThis, BrUiPhase *pOwner, float x, float y,
                int32_t flags, int32_t a4, int32_t a5,
                int32_t a6, int32_t a7);
};

struct BrUiCtlSubVtbl {
    void *aReserved[4];    /* +0x00..+0x0C */
    /* +0x10 __thiscall -- append one row of text. */
    void (*f10)(BrUiCtlSub *pThis, const void *pText, int32_t a2,
                int32_t a3, const void *pStyle, int32_t a5);
    /* +0x14 __thiscall -- configure the list. */
    void (*f14)(BrUiCtlSub *pThis, int32_t a1, const void *pStyle,
                int32_t a3, int32_t a4, int32_t a5);
};

/* The sub-object embedded at control +0x3838. The original reads its vtable
 * with `mov eax,[edi+0x3838]` and takes its address with
 * `lea ecx,[edi+0x3838]`, so +0x3838 is the vtable slot of an object that
 * begins there. */
struct BrUiCtlSub {
    const BrUiCtlSubVtbl *pVtbl;   /* +0x3838 */
};

struct BrUiCtl {
    const BrUiCtlVtbl *pVtbl;   /* +0x0000 */
    BrUiCtlFn  pfn04;           /* +0x0004 */
    BrUiCtlFn  pfn08;           /* +0x0008 */
    BrUiCtlFn  pfn0C;           /* +0x000C */
    BrUiCtlFn  pfn18;           /* +0x0018 */
    int32_t    f50;             /* +0x0050 } the four make a rectangle:      */
    int32_t    f54;             /* +0x0054 } f50/f54 = truncated x/y,        */
    int32_t    f58;             /* +0x0058 } f58 = f50+0x7F, f5C = f54+0x21  */
    int32_t    f5C;             /* +0x005C }                                 */
    int32_t    f2968;           /* +0x2968 -- cleared alongside the rect     */
    uint16_t   f2A42;           /* +0x2A42 */
    uint16_t   f2AB4;           /* +0x2AB4 -- incremented, never read here   */
    uint16_t   f2AB6;           /* +0x2AB6 -- receives cCtl + 1              */
    BrUiCtlSub f3838;           /* +0x3838 */
    int32_t    f1E1F4;          /* +0x1E1F4 */
    uint16_t   f1E20C;          /* +0x1E20C -- 3 almost everywhere, 5 once   */
};

#define BR_UI_CTL_ORIG_SIZE    0x1E214u

/* The literal each `operator new` is given in the original.
 *
 * DEVIATION (memory safety): the port asks for whichever is larger, the
 * original literal or the host's sizeof. On a 32-bit host the two structs
 * below fit inside the original allocations, so this is a no-op there; on a
 * 64-bit host BrUiScreen's 200-entry pointer array does not fit in 0x348
 * bytes and the original literal would under-allocate. */
#define BR_ALLOC_SIZE(type, cbOrig) \
    ((uint32_t)(sizeof(type) > (size_t)(cbOrig) ? sizeof(type) : (size_t)(cbOrig)))

/* ==========================================================================
 * The screen (0x348 bytes)
 * ========================================================================== */

/* (0x338 - 0x18) / 4 -- the pointer array runs up to the first float. */
#define BR_UI_SCREEN_CTL_MAX   200

struct BrUiScreen {
    int32_t    f10;                            /* +0x010 -- zeroed at build */
    uint16_t   cCtl;                           /* +0x014 */
    BrUiCtl   *apCtl[BR_UI_SCREEN_CTL_MAX];    /* +0x018 */
    float      fX;                             /* +0x338 -- 195.0 always    */
    float      fY;                             /* +0x33C -- 111.0 or 130.0  */
    BrUiPhase *pOwner;                         /* +0x340 */
    uint16_t   cSel;                           /* +0x344 -- selectable count*/
};

#define BR_UI_SCREEN_ORIG_SIZE 0x348u

/* ==========================================================================
 * The phase (0xC8 bytes) -- only the four fields this range touches
 * ========================================================================== */

/* (0x6C - 0x14) / 4. The parallel int array at +0x6C has room for 23 but is
 * indexed by the same counter, so 22 is the real bound for both. */
#define BR_UI_PHASE_SCREEN_MAX 22

struct BrUiPhase {
    uint16_t    cScreen;                            /* +0x10 */
    uint16_t    f12;                                /* +0x12 -- zeroed first */
    BrUiScreen *apScreen[BR_UI_PHASE_SCREEN_MAX];   /* +0x14 */
    int32_t     aF6C[BR_UI_PHASE_SCREEN_MAX];       /* +0x6C -- set to 1     */
};

#define BR_UI_PHASE_ORIG_SIZE  0xC8u

/* ==========================================================================
 * The 0x100AB330 table
 * ========================================================================== */

/* 21 records of 8 bytes spanning 0x100AB330..0x100AB3D8. 0x1004CAC0 reads the
 * FIRST dword of each record (a string id); slice2_23.h reaches the SECOND
 * dword of the same records through the address 0x100AB334 and indexes it
 * `aAB334[2*i]`, and its `g0AB3D8` is record 20's second dword. Same table,
 * two views. */
#define BR_UI_AB330_COUNT 21

typedef struct BrUiStrEnt {
    int32_t idText;   /* +0x00 -- 0x100AB330 + 8*i */
    int32_t f04;      /* +0x04 -- 0x100AB334 + 8*i, not read here */
} BrUiStrEnt;

/* ==========================================================================
 * Cross-slice callees
 * ========================================================================== */

/* XSLICE 0x1007DFE0 */
/* operator new -- _nh_malloc(cb,1). Does NOT zero the block. Declared exactly
 * as slice2_26.h declares it so the two agree. */
extern void *BrOperatorNew(uint32_t cb);

/* XSLICE 0x10048470 */
/* __thiscall constructor for the screen object; returns `this`. */
extern BrUiScreen *BrUiScreenCtor(BrUiScreen *pThis);

/* XSLICE 0x100476C0 */
/* __thiscall constructor for the control object; returns `this`. */
extern BrUiCtl *BrUiCtlCtor(BrUiCtl *pThis);

/* XSLICE 0x10074030 */
/* String-table lookup by id; NULL for an out-of-range id. Same name and
 * signature as slice2_25.h. */
extern const char *BrStrGet(int id);

/* ==========================================================================
 * Everything reached indirectly (see NAMING above)
 * ========================================================================== */

/* The 48 addresses this range stores into a control's +0x04 / +0x08 / +0x0C /
 * +0x18 slots. They are never called here, only installed. */
typedef struct BrUiBuildHooks {
    BrUiCtlFn p1003E920;
    BrUiCtlFn p1003EC80;
    BrUiCtlFn p1003F720;
    BrUiCtlFn p1003F760;
    BrUiCtlFn p1003F7F0;
    BrUiCtlFn p1003F860;
    BrUiCtlFn p1003F8D0;
    BrUiCtlFn p1003F990;
    BrUiCtlFn p1003FA00;
    BrUiCtlFn p1003FE80;
    BrUiCtlFn p1003FFD0;
    BrUiCtlFn p100400E0;
    BrUiCtlFn p10040450;
    BrUiCtlFn p10040680;
    BrUiCtlFn p100406C0;
    BrUiCtlFn p10040730;
    BrUiCtlFn p100407E0;
    BrUiCtlFn p10040870;
    BrUiCtlFn p10040890;
    BrUiCtlFn p100408B0;
    BrUiCtlFn p100408D0;
    BrUiCtlFn p10041870;
    BrUiCtlFn p10042B30;
    BrUiCtlFn p10042C80;
    BrUiCtlFn p10042DC0;
    BrUiCtlFn p10042E20;
    BrUiCtlFn p10042E80;
    BrUiCtlFn p10042EE0;
    BrUiCtlFn p100430B0;
    BrUiCtlFn p10043180;
    BrUiCtlFn p10043400;
    BrUiCtlFn p100434C0;
    BrUiCtlFn p10043760;
    BrUiCtlFn p10043FA0;
    BrUiCtlFn p10045110;   /* = BrPhaseActivate_10045110 (slice2_26.h)  */
    BrUiCtlFn p100452C0;   /* = BrPhaseActivate_100452C0 (slice2_26.h)  */
    BrUiCtlFn p10045390;   /* = BrPhaseActivate_10045390 (slice2_26.h)  */
    BrUiCtlFn p100455E0;   /* = BrPhaseActivate_100455E0 (slice2_26.h)  */
    BrUiCtlFn p100456B0;   /* = BrPhaseActivate_100456B0 (slice2_26.h)  */
    BrUiCtlFn p100458C0;
    BrUiCtlFn p100458E0;
    BrUiCtlFn p10046450;
    BrUiCtlFn p100464E0;
    BrUiCtlFn p10046520;
    BrUiCtlFn p10046560;
    BrUiCtlFn p100465A0;
    BrUiCtlFn p10047360;
    BrUiCtlFn p100474B0;
} BrUiBuildHooks;

/* The globals this range reads or writes, gathered as slice2_26.h gathers
 * its own. Named for their addresses; no semantic name was established. */
typedef struct BrUiBuildCtx {
    /* --- read --------------------------------------------------------- */
    int32_t  n0AA010;   /* 0x100AA010 -- 0x1004A580 skips one control if !=0 */
    int32_t  n0AC304;   /* 0x100AC304 -- gates two blocks of 0x1004B430      */
    int32_t  nAA2A0C;   /* 0x10AA2A0C */
    int32_t  nAB428;    /* 0x100AB428 -- read with fild, i.e. an INT (== 0)  */
    int32_t  nAB42C;    /* 0x100AB42C -- read with fild, i.e. an INT (== 380)*/

    /* --- written ------------------------------------------------------ */
    int32_t  nAA2840;   /* 0x10AA2840 <- 2 */
    int32_t  nAA2850;   /* 0x10AA2850 <- BrCfgFindConflicts result           */
    BrUiCtl *pAA29AC;   /* 0x10AA29AC */
    BrUiCtl *pAA29B4;   /* 0x10AA29B4 */
    BrUiCtl *pAA29C8;   /* 0x10AA29C8 -- written by THREE of the five        */

    /* --- tables ------------------------------------------------------- */
    const BrUiStrEnt *aAB330;   /* 0x100AB330, BR_UI_AB330_COUNT records */
    const int32_t    *aAC520;   /* 0x100AC520, indexed by nAA2A0C        */

    /* --- style / text blocks; the original pushes their ADDRESSES ------ */
    const void *p0AB448;
    const void *p0AB458;
    const void *p0AB468;
    const void *p0AB478;
    const void *p0AB488;
    const void *p0AB498;
    const void *p0AB4A8;
    const void *p0AB4B8;
    const void *p0AB4C8;
    const void *p0AB4D8;
    const void *p0AB508;
    const void *p0AD274;
    const void *p0AD300;

    /* --- injected callees --------------------------------------------- */
    const BrUiBuildHooks *pHooks;

    /* DEVIATION: 0x1003E260 takes only an index in the original;
     * slice1_06.h's port injects its host. Carried here so the call sites
     * stay one-for-one with the original. */
    const BrErrHost *pErrHost;

    /* DEVIATION: 0x10040330 takes only `kind` in the original;
     * slice2_23.h's port (BrCfgFindConflicts) injects two more arguments.
     * Reached through a pointer so no fourth name is created for it. */
    int32_t (*pfn10040330)(int32_t kind);
} BrUiBuildCtx;

/* ==========================================================================
 * The range itself
 * ==========================================================================
 *
 * All five return 1 unconditionally in the original (the value is discarded
 * by slice2_26.h's caller, which types the slot `void (*)(BrPhase *)`).
 *
 * GOTCHAS, in rough order of how much they would cost to rediscover:
 *
 *  1. 0x1004B430 builds one control and then does NOT increment cCtl. The
 *     next control therefore OVERWRITES it in apCtl. Preserved.
 *  2. 0x1004B430's row cursor is a stack local initialised to 0.0f and set
 *     to 19.0f only inside the `n0AC304` block, so when n0AC304 is zero the
 *     first four rows all land on the screen's own fY. The steps are then
 *     +19, +57, +19 -- NOT a uniform 19.
 *  3. 0x1004A580 walks the row constants -19,-38,-57,-76 then jumps to
 *     -114: the -95 slot (0x1008F690) is SKIPPED. Only 0x1004CAC0 uses -95.
 *  4. In 0x1004A580 the third rect control reuses the PREVIOUS control's
 *     truncated x and its right edge (both kept in registers/locals) and
 *     does not advance the y cursor, so two controls share a top edge.
 *  5. The rect is built with __ftol (truncate toward zero) on the same
 *     floats that were passed to f38, and the y cursor is advanced by 33
 *     (0x1008F69C) in float while the rect uses +0x21 in int.
 *  6. cSel (+0x344) is bumped only for the rows that carry a +0x08 hook,
 *     never for the plain labels, the rect controls, or the title.
 *  7. 0x1004CAC0's list loop calls BrStrGet TWICE per record -- once to test
 *     for NULL, once for the value -- and re-reads nAA2A0C every iteration.
 *     Its `ebx` argument is 0x10 only for records 0 and 1 and only when
 *     nAA2A0C == 3, and that same condition also writes nAA2840 = 2, once
 *     per qualifying record.
 *  8. f38's fourth and fifth arguments are the literals 2 and 5 at every
 *     single call site.
 */

void BrExt_1004A580(BrUiBuildCtx *pCtx, BrUiPhase *pPhase);
void BrExt_1004B430(BrUiBuildCtx *pCtx, BrUiPhase *pPhase);
void BrExt_1004BDC0(BrUiBuildCtx *pCtx, BrUiPhase *pPhase);
void BrExt_1004C4A0(BrUiBuildCtx *pCtx, BrUiPhase *pPhase);
void BrExt_1004CAC0(BrUiBuildCtx *pCtx, BrUiPhase *pPhase);

#endif /* SLICE3_33_H */
