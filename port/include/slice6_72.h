/* slice6_72.h -- BRD3D.dll, packet 72: seventeen functions every other module
 * already CALLS and none defines.
 *
 * WHAT IS IN HERE
 * ---------------
 * Six of the seventeen are menu-screen builders, the sixth..eleventh members
 * of the family slice3_33.c ports as BrExt_1004A580 .. BrExt_1004CAC0 and
 * slice4_52.c as BrOptFn10051990.  Five of those six were declined by an
 * earlier pass (see slice5_61.h "NOT TRACTABLE") purely because no declarable
 * type could hold the 0xC8 phase object.  br_phase.h now supplies it, so they
 * are ported here.
 *
 * The other eleven are small: two render-state / config publishers, a bit
 * shuffle, a vtable re-seat, a DirectInput read, two display-list emitters, a
 * Direct3D rectangle filler, a mutex factory, a phase opener (a THUNK -- see
 * below) and a DirectPlay session update.
 *
 * TYPES -- WHAT IS REUSED AND WHAT IS NOT
 * ---------------------------------------
 * REUSED unchanged:
 *   br_phase.h    BrPhase_        the 0xC8 phase/screen object.  This header
 *                                 is canonical; the builders take BrPhase_ *.
 *   slice1_06.h   BrErrHost/BrErrShow    0x1003E260.
 *   slice3_39.h   BrTextBox, BrTextBoxVtbl, g_pBrTextBoxVtbl, BrOperatorNew,
 *                 and its own declaration of BrDikGetDeviceState, which this
 *                 module DEFINES.
 *
 * MIGRATED AWAY (this header no longer models either object):
 *   br_ui.h       struct BrUiPage_  the 0x348 "screen", and BrUiCtl_ /
 *                 BrUiCtlVtbl_ the 0x1E214 control.  This header used to
 *                 COMPLETE br_phase.h's forward declaration of BrUiPage_
 *                 itself, and to carry a private control called BrUi72Ctl
 *                 with the item block nested and the +0x3838 sub-object
 *                 spelled BrUi72Sub.  br_ui.h is the merge of all six of
 *                 those models and is the rightful owner of the tag, so the
 *                 definitions are gone and it is included instead.
 *
 *                 The mapping the bodies now use:
 *                   f50/f54/f58/f5C -> rcLeft/rcTop/rcRight/rcBottom
 *                   f2A42           -> aStepId[1]        (ADJ-4)
 *                   f2AB4/f2AB6     -> cChild/aChild[0]  (ADJ-5)
 *                   item            -> aText[0]          (ADJ-1, ADJ-2)
 *                   f3838           -> list              (ADJ-6)
 *                   f383C/f384C     -> list.f04/list.f14
 *                   f1E1F4          -> list.f1A99C[8]
 *                   f1E20C          -> w1E20C
 *
 *                 Consequence, which is the whole point: this header and
 *                 slice6_73.h now agree about the page BY CONSTRUCTION
 *                 rather than by hand, so port/tests/test_pagemodel.c --
 *                 which existed only to catch them drifting apart -- is
 *                 deleted, exactly as its own comment instructed.
 *
 * As in slice2_26.h / slice3_33.h, every struct below uses natural C layout
 * and carries the original's 32-bit byte offsets as comments.  Nothing here
 * is overlaid on a file image or a foreign buffer, and every allocation asks
 * for max(sizeof, the original literal) -- the 0xC8 / 0x348 / 0x1E214
 * literals all under-allocate on LP64.
 *
 * SIGNATURE NOTES (the callers, verbatim)
 * ---------------------------------------
 *   slice2_25.h  void BrOptFn10056A10(BrOptObj *)   void BrOptFn10057C10(BrOptObj *)
 *   slice3_31.h  void BrExt_10052030(BrPhase *)
 *   slice2_26.h  void BrExt_10059760/1005A6E0/1004E830(BrPhase *)
 * BrOptObj, slice3_31.h's BrPhase and slice2_26.h's BrPhase are three partial
 * views of the one 0xC8 allocation (br_phase.h documents the pile-up).  The
 * six definitions below take br_phase.h's BrPhase_ * instead.  That is the
 * same pointer at ABI level, so the link closes; it is recorded here because
 * a translation unit that includes BOTH this header and one of those three
 * will see conflicting prototypes until they are merged.
 *
 * Everything else matches its caller's declaration exactly.
 *
 * THE THUNK
 * ---------
 *   0x100440D0 BrExt_100440D0 is the SAME BODY as slice2_25.c's
 *   BrOptOpen294C (slice5_61.h already recorded the equality).  It is
 *   forwarded, not re-transcribed.
 *
 * INJECTION
 * ---------
 * The original reaches ~90 globals and ~60 code addresses absolutely.  They
 * are gathered into ONE file-scope context, the precedent of br_pool.h /
 * slice2_23.h / slice2_26.h / slice3_33.h.  A context POINTER rather than an
 * extra argument, because every caller's declaration above is one-argument;
 * slice4_52.h's g_pBrUi51990Ctx sets that precedent.
 */
#ifndef SLICE6_72_H
#define SLICE6_72_H

#include <stddef.h>
#include <stdint.h>

#include "br_phase.h"    /* BrPhase_, BrUiPage_, BR_PHASE_PAGES            */
#include "br_ui.h"       /* BrUiPage_ (COMPLETED there), BrUiCtl_,
                          * BrUiCtlVtbl_, BrUiCtlHookFn_                   */
#include "slice1_06.h"   /* BrErrHost / BrErrShow -- 0x1003E260            */
#include "slice3_39.h"   /* BrTextBox / BrOperatorNew / BrDikGetDeviceState */

/* ==========================================================================
 * 0. Allocation sizes
 *
 * DEVIATION (memory safety): the port asks for whichever is larger, the
 * original literal or the host's sizeof.  On a 32-bit host these are no-ops.
 * ========================================================================== */
#define BR72_ALLOC(type, cbOrig) \
    ((uint32_t)(sizeof(type) > (size_t)(cbOrig) ? sizeof(type) : (size_t)(cbOrig)))

/* The original literals and the element count, kept under this packet's own
 * names so its .c and its test keep reading -- but they are now br_ui.h's
 * numbers, not a second opinion about them.  If br_ui.h ever changes one, the
 * change lands here too instead of the two silently disagreeing. */
#define BR72_PAGE_ORIG_SIZE  BR_UI_PAGE_ORIG_SIZE
#define BR72_CTL_ORIG_SIZE   BR_UI_CTL_ORIG_SIZE
#define BR72_PAGE_CTL_MAX    BR_UI_PAGE_CTL_MAX

/* ==========================================================================
 * 1. The page and the control -- BOTH OWNED BY br_ui.h
 *
 * There is nothing to define here any more.  `struct BrUiPage_`, `BrUiCtl_`,
 * `BrUiCtlVtbl_`, the item block at +0x2B5C and the sub-object at +0x3838 all
 * come from br_ui.h (and, for the last two, from slice3_39.h's BrTextBox and
 * BrTextList, which br_ui.h embeds rather than re-modelling).
 *
 * WHAT THIS PACKET USED TO SAY THAT br_ui.h DOES NOT, so nobody re-derives it:
 *
 *   BrUi72Item.w2F78  was a uint16 at item +0x41C.  br_ui.h routes it to
 *                     BrTextBox::f41C, which slice3_39.h derived from the
 *                     box's OWN constructor as an int16 width limit.  Same
 *                     address, same 16 bits; the .c casts.
 *   BrUi72Sub         modelled the +0x3838 object as a three-field stub.  It
 *                     is slice3_39.h's whole 0x1A9D4-byte BrTextList (ADJ-6);
 *                     +0x383C and +0x384C are that object's f04 and f14.
 *
 * NOTHING this packet writes lacks a home in the merged model.  The two
 * control offsets br_ui.h cannot express (+0x1E1C8 and +0x1E1D0) belong to
 * slice6_73's builders, not to this one.
 * ========================================================================== */

/* ==========================================================================
 * 3. The .GRF name list 0x1005A6E0 enumerates
 * ========================================================================== */

typedef struct BrGrfList     BrGrfList;
typedef struct BrGrfListVtbl BrGrfListVtbl;

struct BrGrfListVtbl {
    void *f00;
    void (*f04)(BrGrfList *pThis, const char *pszMask);   /* +0x04 thiscall */
};

#define BR72_GRF_STRIDE  0x104              /* == MAX_PATH */
#define BR72_GRF_COUNT   100                /* 0x6590 / 0x104 exactly */

struct BrGrfList {
    const BrGrfListVtbl *pVtbl;                       /* +0x0000 */
    char                 aName[BR72_GRF_COUNT][BR72_GRF_STRIDE]; /* +0x0004 */
};

/* ==========================================================================
 * 4. Direct3D -- what 0x1001BE90 touches
 * ========================================================================== */

/* The vertex is eight dwords: the `rep movsd` count is 8 and the primitive
 * is drawn with dwVertexTypeDesc == 3 (D3DVT_TLVERTEX).  That fixes it. */
typedef struct BrD3DTLVertex {
    float    x, y, z, rhw;
    uint32_t diffuse, specular;
    float    tu, tv;
} BrD3DTLVertex;

typedef struct BrD3DDev     BrD3DDev;
typedef struct BrD3DDevVtbl BrD3DDevVtbl;

/* Only the three slots the function reaches are typed.  +0x5C, +0x74 and
 * +0x78 are IDirect3DDevice3::SetRenderState / DrawPrimitive /
 * DrawIndexedPrimitive; the argument lists below are the original's. */
struct BrD3DDevVtbl {
    void *aReserved00[23];                            /* +0x00 .. +0x58 */
    int32_t (*SetRenderState)(BrD3DDev *pThis, int32_t state, int32_t value);
    void    *aReserved60[5];                          /* +0x60 .. +0x70 */
    int32_t (*DrawPrimitive)(BrD3DDev *pThis, int32_t type, int32_t vtype,
                             const void *pVerts, int32_t cVerts,
                             int32_t flags);
    int32_t (*DrawIndexedPrimitive)(BrD3DDev *pThis, int32_t type,
                                    int32_t vtype, const void *pVerts,
                                    int32_t cVerts, const void *pIndices,
                                    int32_t cIndices, int32_t flags);
};

struct BrD3DDev { const BrD3DDevVtbl *pVtbl; };

/* The eleven cached render states.  desired[] is 0x10277378 + 4i, applied[]
 * is 0x102773F8 + 4i, and bit i of the dword at 0x10277370 says "differs".
 * The ids are the ones the apply loop pushes, in index order. */
#define BR72_RS_COUNT 11
extern const int32_t g_aBr72RsId[BR72_RS_COUNT];

/* The non-Direct3D drawing path 0x1001BE90 takes when 0x104C1694 is not the
 * magic 0x504340.  Natural layout, original offsets in the comments. */
typedef struct BrGfxSurf {
    int32_t f00;
    float   f04, f08, f0C;   /* +0x04/+0x08/+0x0C -- R/G/B scaled by 1/255 */
    int32_t f4C;             /* +0x4C */
    int32_t f50;             /* +0x50 -- handed to the target's vtable +0x20 */
    int32_t f58;             /* +0x58 */
} BrGfxSurf;

typedef struct BrGfxTarget     BrGfxTarget;
typedef struct BrGfxTargetVtbl BrGfxTargetVtbl;

struct BrGfxTargetVtbl {
    void *aReserved00[8];                                 /* +0x00 .. +0x1C */
    void (*f20)(BrGfxTarget *pThis, int32_t a);           /* +0x20 */
    void *aReserved24[3];                                 /* +0x24 .. +0x2C */
    void (*f30)(BrGfxTarget *pThis, int32_t a1,
                const int32_t *pRect, int32_t a3);        /* +0x30 */
};

struct BrGfxTarget { const BrGfxTargetVtbl *pVtbl; };

/* *0x1027736C.  Only +0x08 and +0x64 are read. */
typedef struct BrGfxCtx {
    BrGfxSurf   *pSurf;      /* +0x08 */
    BrGfxTarget *pTarget;    /* +0x64 */
} BrGfxCtx;

/* ==========================================================================
 * 5. DirectInput -- what 0x100771B0 touches
 * ========================================================================== */

typedef struct BrDInputDev     BrDInputDev;
typedef struct BrDInputDevVtbl BrDInputDevVtbl;

/* IDirectInputDeviceA: slot 7 (+0x1C) is Acquire, slot 9 (+0x24) is
 * GetDeviceState(cbData, lpvData). */
struct BrDInputDevVtbl {
    void   *aReserved00[7];                               /* +0x00 .. +0x18 */
    int32_t (*Acquire)(BrDInputDev *pThis);               /* +0x1C */
    void   *f20;                                          /* +0x20 */
    int32_t (*GetDeviceState)(BrDInputDev *pThis, uint32_t cb, void *pv);
};

struct BrDInputDev { const BrDInputDevVtbl *pVtbl; };

#define BR72_DIERR_NOTACQUIRED  ((int32_t)0x8007001E)

/* ==========================================================================
 * 6. DirectPlay -- what 0x1003CDA0 touches
 * ==========================================================================
 *
 * The four fields the function writes are DPSESSIONDESC2's dwUser1..dwUser4
 * at +0x40..+0x4C of the 32-bit layout.  DEVIATION: the port does NOT overlay
 * that layout on the block DirectPlay hands back -- the injected builder
 * below owns the type, so no foreign buffer is ever punned.  Everything else
 * about the block is opaque to this module. */
typedef struct BrDPSessionUser {
    int32_t dwUser1;   /* +0x40 <- 0x100B380C */
    int32_t dwUser2;   /* +0x44 <- 0x1022B350 */
    int32_t dwUser3;   /* +0x48 <- 0x10AA2A18 */
    int32_t dwUser4;   /* +0x4C <- 0x100AC658 */
} BrDPSessionUser;

#define BR72_DPERR_NOINTERFACE  ((int32_t)0x88770082)

/* ==========================================================================
 * 7. The hooks -- code addresses this packet only ever INSTALLS
 * ==========================================================================
 *
 * None is called here.  Names are the addresses; those that also appear in
 * slice3_33.h's BrUiBuildHooks are marked, so integration can merge the two
 * lists without inventing a second name for any of them. */
typedef struct BrUi72Hooks {
    BrUiCtlHookFn_ p1003E7A0;
    BrUiCtlHookFn_ p1003EC30;
    BrUiCtlHookFn_ p1003EF90;
    BrUiCtlHookFn_ p1003F020;
    BrUiCtlHookFn_ p1003F5E0;
    BrUiCtlHookFn_ p1003F680;
    BrUiCtlHookFn_ p1003FA00;   /* also in slice3_33.h */
    BrUiCtlHookFn_ p1003FCB0;
    BrUiCtlHookFn_ p1003FD30;
    BrUiCtlHookFn_ p1003FDA0;
    BrUiCtlHookFn_ p1003FE10;
    BrUiCtlHookFn_ p1003FE80;   /* also in slice3_33.h */
    BrUiCtlHookFn_ p10040730;   /* also in slice3_33.h */
    BrUiCtlHookFn_ p100407E0;   /* also in slice3_33.h */
    BrUiCtlHookFn_ p100408D0;   /* also in slice3_33.h */
    BrUiCtlHookFn_ p10040950;
    BrUiCtlHookFn_ p10040990;
    BrUiCtlHookFn_ p100409B0;
    BrUiCtlHookFn_ p100409D0;
    BrUiCtlHookFn_ p10040B30;
    BrUiCtlHookFn_ p10041040;
    BrUiCtlHookFn_ p10041180;
    BrUiCtlHookFn_ p10041300;
    BrUiCtlHookFn_ p100413B0;
    BrUiCtlHookFn_ p100414B0;
    BrUiCtlHookFn_ p100415A0;
    BrTextListCbFn p10042560;
    /* the two that land in the LIST (+0x383C / +0x384C), not in a control
     * slot -- slice3_39.h owns their type. */
    BrTextListCbFn p10042740;
    BrUiCtlHookFn_ p10042AC0;
    BrUiCtlHookFn_ p10042EE0;   /* also in slice3_33.h */
    BrUiCtlHookFn_ p100430B0;   /* also in slice3_33.h */
    BrUiCtlHookFn_ p10043180;   /* also in slice3_33.h */
    BrUiCtlHookFn_ p10043590;
    BrUiCtlHookFn_ p100435F0;
    BrUiCtlHookFn_ p10043650;
    BrUiCtlHookFn_ p100436B0;
    BrUiCtlHookFn_ p100437D0;
    BrUiCtlHookFn_ p10043F50;
    BrUiCtlHookFn_ p10043FA0;   /* also in slice3_33.h */
    BrUiCtlHookFn_ p10044600;
    BrUiCtlHookFn_ p100446D0;
    BrUiCtlHookFn_ p10044B40;
    BrUiCtlHookFn_ p10044C70;
    BrUiCtlHookFn_ p10044D00;
    BrUiCtlHookFn_ p10044F00;
    BrUiCtlHookFn_ p10045050;
    BrUiCtlHookFn_ p10046260;
    BrUiCtlHookFn_ p10046710;
    BrUiCtlHookFn_ p10047060;
    BrUiCtlHookFn_ p10047250;
    BrUiCtlHookFn_ p10047340;
    BrUiCtlHookFn_ p10047360;   /* also in slice3_33.h */
    BrUiCtlHookFn_ p100474B0;   /* also in slice3_33.h */
    BrUiCtlHookFn_ p100457C0;
    BrUiCtlHookFn_ p100457E0;
} BrUi72Hooks;

/* ==========================================================================
 * 7a. The display-list command 0x1003407D writes
 * ==========================================================================
 *
 * slice2_15.h models the same 8-byte allocation unit as
 * `BrGfxCmd { uint32_t w0, w1; }`.  This packet needs one more thing from it:
 * 0x1003407D stores a POINTER into the second dword, which does not fit on
 * LP64.
 *
 * DEVIATION: the port carries that pointer in a parallel field and leaves w1
 * zero for the one command that needs it.  Everything else about the unit is
 * slice2_15.h's; integration should extend BrGfxCmd rather than keep two.
 */
typedef struct Br72GfxCmd {
    uint32_t w0;
    uint32_t w1;    /* the original's second dword when it is a scalar */
    void    *p1;    /* ... and when it is a pointer -- see the DEVIATION */
} Br72GfxCmd;

/* ==========================================================================
 * 8. The one context
 * ========================================================================== */

/* The twelve-dword block at 0x10B4E710 that 0x1003E3A0 publishes.  Read as
 * a whole; the field names are the source addresses. */
typedef struct Br72Config {
    int32_t nB4E710, nB4E714, nB4E718, nB4E71C;
    int32_t nB4E720, nB4E724, nB4E728, nB4E72C;
    int32_t nB4E730, nB4E734, nB4E738, nB4E73C;
} Br72Config;

#define BR72_MUTEX_BANK   16      /* (0x1022AAA8-0x10221328)/0x978 exactly  */
#define BR72_MUTEX_EXTRA  10      /* the four then six singletons after it  */
#define BR72_AC520_MAX    8       /* 0x100AC520, indexed by 0x10AA2A0C      */
#define BR72_A9CDF0_ROOM  0x210   /* 0x10A9CDF0, the strcpy destination     */

typedef struct Br72Env {
    /* --- injected callees ------------------------------------------------ */
    const BrErrHost   *pErrHost;        /* 0x1003E260, see slice1_06.h       */
    const BrUi72Hooks *pHooks;

    /* The two thiscall constructors, 0x10048470 and 0x100476C0.  Both are now
     * typed over br_ui.h's objects, so br_uivt.h's BrUiPageCtor_10048470 and
     * br_uictl.h's BrUiCtlCtor go straight into these slots -- no adapter and
     * no cast.  They stay POINTERS because slice3_33.h still declares the same
     * two addresses over its own BrUiScreen / BrUiCtl, and a module compiled
     * against that header must not pick up a conflicting prototype from this
     * one. */
    BrUiPage_ *(*pfnPageCtor)(BrUiPage_ *pThis);    /* 0x10048470 */
    BrUiCtl_  *(*pfnCtlCtor)(BrUiCtl_ *pThis);       /* 0x100476C0 */

    void (*pfn100795D0)(void);    /* 0x100795D0 -- force-feedback re-probe   */
    void (*pfn1003E2C0)(void);    /* 0x1003E2C0                              */
    void (*pfn10075100)(void);    /* 0x10075100                              */
    void (*pfn10005960)(void);    /* 0x10005960                              */
    void (*pfn1001C620)(BrGfxSurf *pThis);          /* 0x1001C620 thiscall   */
    void (*pfn1001C640)(void);                      /* 0x1001C640            */
    void (*pfn100307A0)(const float *pM, void *pDst);/* 0x100307A0, cdecl(2) */
    void *(*pfn10069490)(void);                     /* 0x10069490            */

    /* KERNEL32.  DEVIATION: the original calls CreateMutexA(NULL,FALSE,NULL),
     * GlobalHandle, GlobalUnlock and GlobalFree directly; no Win32 name may
     * appear in portable code, so the host supplies them. */
    void *(*pfnCreateMutex)(void);
    void *(*pfnGlobalHandle)(void *pv);
    int32_t (*pfnGlobalUnlock)(void *hMem);
    int32_t (*pfnGlobalFree)(void *hMem);

    /* DirectPlay.  0x1003D0B0 in the original; it allocates and fills the
     * session descriptor and returns an HRESULT. */
    int32_t (*pfn1003D0B0)(void *pDPlay, BrDPSessionUser **ppDesc);
    /* IDirectPlay4::SetSessionDesc, the object's vtable +0x7C. */
    int32_t (*pfnDPSetSessionDesc)(void *pDPlay, BrDPSessionUser *pDesc,
                                   uint32_t flags);

    /* --- foreign objects ------------------------------------------------- */
    BrPhase_    *pAA2908;      /* 0x10AA2908 -- the GLOBAL phase            */
    BrD3DDev    *pDev277368;   /* 0x10277368                                */
    BrGfxCtx    *pCtx27736C;   /* *0x1027736C                               */
    BrDInputDev *pDik18ABDD0;  /* 0x118ABDD0                                */
    void        *pDPlay277B40; /* 0x10277B40                                */

    /* --- style / text blocks; the original pushes their ADDRESSES --------- */
    const void *p0AB438, *p0AB448, *p0AB458, *p0AB478, *p0AB488;
    const void *p0AB4A8, *p0AB4B8, *p0AB4C8, *p0AB4D8, *p0AB4F8;
    const void *p0AB508, *p0AD274, *p0AD300;
    const void *p39B720;       /* 0x1039B720 -- writable scratch text        */
    const char *pszTimeAttackMask;  /* 0x100AD35C "TimeAttack*.GRF"          */

    /* --- scalars READ ----------------------------------------------------- */
    int32_t  nAB428;      /* 0x100AB428 -- fild-ed, i.e. an INT             */
    int32_t  nAB42C;      /* 0x100AB42C -- fild-ed, i.e. an INT             */
    int32_t  n18ABDBC;    /* 0x118ABDBC -- force-feedback present           */
    int32_t  n22AF18;     /* 0x1022AF18                                     */
    int32_t  nAA2884;     /* 0x10AA2884                                     */
    int32_t  nA9D000;     /* 0x10A9D000                                     */
    float    f575514;     /* 0x10575514 -- a float, see BrSub_1002B2A0      */
    int32_t  n575530;     /* 0x10575530                                     */
    int32_t  n4C1694;     /* 0x104C1694 -- compared to 0x504340             */
    /* 0x104C5158 / 0x104C515C are compared as raw bit patterns against two
     * magic numbers; the other four are genuine floats. */
    uint32_t n4C5158, n4C515C;
    float    f4C5154, f4C5160, f4C1690, f4C0BA8;
    int32_t  n0B380C;     /* 0x100B380C -- DPSESSIONDESC2 dwUser1           */
    int32_t  n22B350;     /* 0x1022B350 -- DPSESSIONDESC2 dwUser2           */
    uint8_t  b4BBF00, b4BC194, b4C5150, b4C15CC;  /* the byte colour path   */
    int32_t  n4C5164, n4C516C, n4C5170, n4C01A0;  /* the clamp limits       */
    int32_t  n4C5188, n4C518C;                    /* index buffer + count   */
    const void *p4BC1A0, *p4C4D50;                /* vertex / index arrays  */
    int32_t  n0A81C4;     /* 0x100A81C4 -- screen height                    */
    int32_t  n6C6618;     /* 0x106C6618                                     */
    uint16_t w6C067C;     /* 0x106C067C                                     */

    /* --- scalars WRITTEN --------------------------------------------------- */
    int32_t  n0AB3F4;     /* 0x100AB3F4 <- -1 by 0x1005A6E0                 */
    int32_t  nAA28E8;     /* 0x10AA28E8 <- 0  by 0x1005A6E0                 */
    int32_t  nAA2A44;     /* 0x10AA2A44 -- 0x10044540's "last published"    */
    int32_t  n0AB3E8;     /* 0x100AB3E8 }  0x10044540's two outputs         */
    int32_t  n0AC654;     /* 0x100AC654 }                                   */
    uint16_t w0AB3E4;     /* 0x100AB3E4 -- OR-ed, WORD wide                 */
    int32_t  n0AB3EC;     /* 0x100AB3EC -- OR-ed                            */
    int32_t  n0AC648, n0AC64C, n0AC650, n0AC658, n0AC65C;
    int32_t  nAA2A00, nAA2A08, nAA2A0C, nAA2A10, nAA2A14, nAA2A18;
    int32_t  nAA2A1C, nAA2A20, nAA2A24, nAA2A28;
    int32_t  nB4E1D0;     /* 0x10B4E1D0 */
    void    *pB4E1D4;     /* 0x10B4E1D4 */
    int32_t  nB4E1D8, nB4E1DC, nB4E1E0, nB4E7A0;  /* read, tested for zero  */
    void    *pB4DF30, *pB4DFD8, *pB4E080, *pB4E128;  /* the four candidates */
    const char *pszB4E1E4;             /* 0x10B4E1E4 -- strcpy source       */
    char     szA9CDF0[BR72_A9CDF0_ROOM];  /* 0x10A9CDF0 -- and destination  */
    Br72Config cfgB4E710;              /* 0x10B4E710 .. 0x10B4E73C          */
    const int32_t *aAC520;             /* 0x100AC520, indexed by nAA2A0C    */
    int32_t  cAC520;

    /* --- pointer globals the builders publish ------------------------------ */
    BrUiCtl_  *pAA29B8;   /* 0x10AA29B8 */
    BrUiCtl_  *pAA29C4;   /* 0x10AA29C4 */
    BrUiCtl_  *pAA29C8;   /* 0x10AA29C8 */
    BrUiCtl_  *pAA29E8;   /* 0x10AA29E8 */
    void      *pfn0A79EC; /* 0x100A79EC -- a code slot, one of two values   */
    void      *pfn1001C690;  /* the two candidates 0x1001BE90 chooses from  */
    void      *pfn1001BC90;

    /* --- the render-state cache 0x1001BE90 drives -------------------------- */
    uint32_t nDirty277370;              /* 0x10277370 */
    int32_t  aWant277378[BR72_RS_COUNT];/* 0x10277378 + 4i */
    int32_t  aHave2773F8[BR72_RS_COUNT];/* 0x102773F8 + 4i */
    int32_t  n4BBE28;                   /* 0x104BBE28 */
    int32_t  n4C16A0;                   /* 0x104C16A0 */

    /* --- the display list 0x1003407D emits into --------------------------- */
    Br72GfxCmd *pDlCursor; /* 0x106C0680 -- advances 8 bytes per command    */
    void     *p6C32D0;     /* 0x106C32D0                                    */
    float     aMtx6C29A8[16];  /* 0x106C29A8 -- the matrix built in place   */

    /* --- the mutex bank 0x10005B10 fills ---------------------------------- */
    void *aMutexBank[BR72_MUTEX_BANK];   /* 0x10221328, stride 0x978        */
    void *aMutexExtra[BR72_MUTEX_EXTRA]; /* see the .c for the address of   */
                                         /* each slot, in creation order    */
    int32_t n221310;      /* 0x10221310 <- 0 */
    int32_t n220DD8;      /* 0x10220DD8 <- 0 */
} Br72Env;

extern Br72Env *g_pBr72Env;

/* ==========================================================================
 * 9. Cross-slice callees
 * ========================================================================== */

/* XSLICE 0x10074030 -- string-table lookup by id; NULL for a bad id.  Same
 * name and signature as slice2_25.h / slice3_33.h. */
extern const char *BrStrGet(int id);

/* XSLICE 0x1007C8A0 -- __ftol.  Same name and signature as slice2_21.h /
 * slice3_32.h. */
extern int32_t BrFtolTrunc(float f);

/* XSLICE 0x100440D0 is the same body; see THE THUNK above. */
struct BrGameObj;
extern int BrOptOpen294C(struct BrGameObj *pUnused);

/* ==========================================================================
 * 10. The packet
 * ========================================================================== */

/* 0x10044540.  Publishes the pair (0x100AB3E8, 0x100AC654) for the current
 * 0x10AA2A18, and does nothing at all when it has not changed.
 * GOTCHA: the out-of-range arm sets 0x100AC654 BEFORE 0x100AB3E8 while every
 * table arm sets 0x100AB3E8 first.  Same values as case 0. */
void BrSub10044540(void);

/* 0x1003E3A0.  Selects one of four option blocks by aAC520[nAA2A0C], derives
 * four "is zero" flags, copies a string, then republishes twelve config
 * dwords.
 * GOTCHA: nAA2A0C is overwritten from the config block and then forced to 2
 * when the config said 1 -- so 1 is not representable.
 * GOTCHA: 0x100AB3E4 is OR-ed with the LOW WORD of a dword. */
void BrSub1003E3A0(void);

/* 0x10035FC0 (thiscall).  Two dwords in, two out: p[0] = a & ~b and
 * p[1] = a & b, where a = p[0] and b = p[1] on entry.  An edge/level split. */
void BrEnt35FC0(void *pThis);

/* 0x1005B0C0 (thiscall).  Re-seats the text widget's vtable to 0x1008F728
 * and nothing else -- the destructor body of a class with no members to
 * release. */
void BrTextBoxDtor(BrTextBox *pBox);

/* 0x100771B0.  Fills a 256-byte DirectInput keyboard buffer.  Returns 1 when
 * there is no device (a POSITIVE value, so callers testing >= 0 proceed with
 * a stale buffer -- that is the original).
 * GOTCHA: on DIERR_NOTACQUIRED it re-acquires and retries; if the re-acquire
 * itself fails, ITS hresult is what comes back, not the original error. */
int32_t BrDikGetDeviceState(uint8_t *pState);

/* 0x1002B2A0.  1 when 0x10575514 is not >= 0.0f, or 0x10575530 is set.
 * GOTCHA: `fcomp` + `test ah,1` puts C0 -- which is also set for UNORDERED --
 * on the "return 1" side, so a NaN there returns 1. */
int BrSub_1002B2A0(void);

/* 0x1003407D.  Builds an orthographic-ish 4x4 at 0x106C29A8 from 2.0f/a and
 * 2.0f/b, emits one 0xBC00000E command, hands the matrix to 0x100307A0 and
 * emits one 0x01030040 command pointing at the result.
 * GOTCHA: a == 0 or b == 0 divides by zero in x87 (masked -> infinity). */
void BrSub_1003407D(float a, float b);

/* 0x1001BE90.  Fills the clamped rectangle (x1,y1)-(x2,y2) with the current
 * colour, y flipped about 0x100A81C4.
 * GOTCHA: the two arms are chosen by an exact-equality test against the magic
 * 0x504340 in 0x104C1694, and the eleven cached render states are only
 * re-published on the Direct3D arm. */
void BrSub_1001BE90(int32_t x1, int32_t y1, int32_t x2, int32_t y2);

/* 0x10005B10.  Creates 26 unnamed mutexes and clears two flags.
 * GOTCHA: the declared argument is never read -- the original takes none. */
void BrSub10005B10(int32_t a);

/* 0x100440D0.  A THUNK: the body is slice2_25.c's BrOptOpen294C. */
void BrExt_100440D0(int32_t a);

/* 0x1003CDA0.  Re-publishes the DirectPlay session descriptor.  The original
 * returns an HRESULT; the caller's declaration discards it.
 * GOTCHA: the descriptor is freed on BOTH paths, success and failure. */
void BrExt_1003CDA0(void);

/* --- the six menu-screen builders ---------------------------------------- *
 * All six return 1 in the original; every caller types the slot
 * `void (*)(<the phase>)`.  See the per-function GOTCHA comments in the .c --
 * the value of these is entirely in the coordinates, ids, flags and ordering.
 */
void BrOptFn10056A10(BrPhase_ *pPhase);
void BrOptFn10057C10(BrPhase_ *pPhase);
void BrExt_10052030(BrPhase_ *pPhase);
void BrExt_10059760(BrPhase_ *pPhase);
void BrExt_1005A6E0(BrPhase_ *pPhase);
void BrExt_1004E830(BrPhase_ *pPhase);

#endif /* SLICE6_72_H */
