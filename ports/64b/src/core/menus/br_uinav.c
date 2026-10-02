/* br_uinav.c -- the menu navigation chain over br_ui.h's struct model.
 *
 * See br_uinav.h for what this module is, which addresses it duplicates, and
 * why a second transcription exists at all. The short version: seven of these
 * eight bodies also exist in port/src/slice3_32.c over that module's
 * byte-image objects, and a byte-offset body cannot address a struct whose
 * fields moved under LP64.
 *
 * Where a body here and slice3_32.c's disagree, slice3_32.c is wrong or this
 * one is -- they were transcribed from the same listings and every GOTCHA
 * comment slice3_32.c carries is repeated here so a future diff of the two is
 * a diff of behaviour and not of documentation.
 */
/* Header is cdecl (nav, ctl). Original is thiscall with this = ctl. */
#define BrUiNavCtlHit_10047A60 BrUiNavCtlHit_10047A60_hdr
#include "br_coretypes.h"   /* br_globals: its objects */
#include "slice3_39.h"   /* br_globals: its objects */
#include "br_uinav.h"
#undef BrUiNavCtlHit_10047A60
#include <stddef.h>

BrUiNav *g_pBrUiNav;

/* ==========================================================================
 * 0x100484F0 -- clamp the selection cursor against one page
 * ========================================================================== */

/* (port-only BrUiNavPageSelect_100484F0 removed) */


/* ==========================================================================
 * 0x100480A0 -- control vtable +0x04, the step timer
 * ========================================================================== */

/* (port-only BrUiNavCtlTick_100480A0 removed) */


/* ==========================================================================
 * 0x10047A10 -- control vtable +0x10
 * ========================================================================== */

/* (port-only BrUiNavCtlStepCode_10047A10 removed) */


/* ==========================================================================
 * 0x10048010 -- control vtable +0x08
 * ========================================================================== */

/* (port-only BrUiNavCtlEnter_10048010 removed) */


/* ==========================================================================
 * 0x10048060 -- control vtable +0x3C
 * ========================================================================== */

/* (port-only BrUiNavCtlOther_10048060 removed) */


/* ==========================================================================
 * 0x10047A60 -- control vtable +0x20.
 *
 * The one function in this module that is a FIRST transcription. It is also
 * the one that decides everything: which control carries the CURRENT bit
 * (+0x20), which carries the ACTIVATE bit (+0x02), and -- for every ordinary
 * menu item -- when the selection cursor advances.
 * ========================================================================== */

/* The hit test the original writes out three times over, once per rect, and
 * again on the control's own rectangle. Written once here; the four copies
 * are instruction-identical apart from their operands.
 *
 * NOTE the asymmetry, which is the original's: the LEFT and TOP comparisons
 * exclude equality on the far side (`jg` -> outside when edge > p), the RIGHT
 * comparison excludes when edge < p, and the BOTTOM one INCLUDES equality
 * (`jge` -> inside). So the box is closed on all four edges. */
/* (port-only BrNavPtIn removed) */


/* (port-only BrNavPtInStyle removed) */


/* WHAT IT DOES: test whether the mouse cursor is inside one menu control's
 * hot rectangle and, if so, make that control the active one. The hit test
 * behind mouse navigation of the front end. */
/* @t4-pass 0x10040EB0 1 2026-09-27 probes 26 bytes 568 insns 184 regions 3 rows 11 census yes  (hand: retranscribed from the listing 24+28 -> 2+9; then tail/nesting shapes, C++ member function, flag sets -- census = the C++ and flag mechanism runs) */
/* @implements 0x10047A60 d3d BrUiNavCtlHit_10047A60 */
/* Orig is thiscall (this = pCtl in ecx, `mov esi, ecx`) and reads the
 * cursor / hot-rects / ordinal / activity flags as standalone globals,
 * not through a BrUiNav *. BrIsAnyActive is 0-arg cdecl; page-select is
 * thiscall on pOwner->pCur.
 * Retranscribed from the listing 2026-09-27 (was 24+28 register-blind): the
 * "is this the selected row" test compares hot0.top + 19*sel against
 * hot0.top + 19*ord -- two row Y positions, the top term NOT cancelled --
 * and every flag update is a plain read-modify-write of flags1C (VC5 emits
 * the `or al,K` / `and al,K` forms itself; the old `*(unsigned char *)&a`
 * puns homed `a` and cost six byte spills).  The early `flags & 8` return
 * shares the rect-miss path's `return 0` tail, as in the original.
 * Block layout (2026-09-27): the body is nested under `if (!(flags & 8))`
 * so that exit and the rect-miss path (placed last, `flags &= ~0x22`) share
 * the final `return 0`; the in-rect-or-current arm returns 1 explicitly for
 * the activity test (the original's `mov eax,1` before the pops) and by a
 * shared, tail-duplicated `return 1` after the flag block.  The same
 * one-shared-return idiom is the predecessor's (0x10040E60).
 * The last register choice fell to reading the control-rect y through
 * g_navCursor itself (VC5 then loads y into eax instead of reusing the
 * dying pointer register); byte-exact 2026-09-27. */
/* BrHitRect: br_coretypes.h */
/* BrObj2C: br_coretypes.h */
/* 64-bit core: declared once, in br_globals.h or its struct's header */     /* 0x10AC5DD8 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */       /* 0x10AC5BC4 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */       /* 0x10AC5BC8 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */       /* 0x100AAB7C */
/* 64-bit core: declared once, in br_globals.h or its struct's header */       /* 0x10AC5BA4 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */          /* 0x100AABE8 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */          /* 0x100AABB8 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */          /* 0x100AABC8 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */          /* 0x10AC6730 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x10AC5BB4 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */          /* 0x10AC5E50 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */          /* 0x10AC6050 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */       /* 0x10AC61E0 */
/* BrIsAnyActiveGlide: prototype in br_funcs.h */
/* BrUiNavPageSelectGlide: prototype in br_funcs.h */

int BR_THISCALL1 BrUiNavCtlHit_10047A60(BrUiCtl_ *pCtl)
{
    int32_t *pCur;
    int fHot;
    int fCurrent = 0;

    if (!(pCtl->flags1C & 8)) {
        if (pCtl->flags1C & 0x10) {
            if (BrGlNavCur5BC4 == g_wAA2870) {
                BrGlNavCur5BC4 += (*(uint16_t *)&BrGlNavStepAB7C);
                BrUiPageSelect_100484F0(pCtl->pOwner->pCur);
            }
            ++g_wAA2870;
            return 0;
        }

        if ((pCtl->flags1C & 0x80000) && BrInputAnyActive() == 0) {
            pCtl->flags1C &= 0xFFF7FFFD;
        }

        pCur = (*(int32_t * *)&BrGlNavThis5DD8);
        if (g_hot0.l <= pCur[0] && g_hot0.r >= pCur[0] &&
            g_hot0.t <= pCur[1] && g_hot0.b >= pCur[1]) {
            fHot = 1;
            fCurrent = 0;
        } else if (g_hot1.l <= pCur[0] && g_hot1.r >= pCur[0] &&
                   g_hot1.t <= pCur[1] && g_hot1.b >= pCur[1]) {
            fHot = 1;
            fCurrent = 0;
        } else if (g_hot2.l <= pCur[0] && g_hot2.r >= pCur[0] &&
                   g_hot2.t <= pCur[1] && g_hot2.b >= pCur[1]) {
            fHot = 1;
            fCurrent = 0;
        } else {
            fHot = 0;
            if (g_hot0.t + (int16_t)BrGlNavCur5BC4 * 19 == g_hot0.t + (int16_t)g_wAA2870 * 19)
                fCurrent = 1;
        }
        ++g_wAA2870;
        g_nAA284C = fHot;

        /* y through the cursor global, not the local (see above). */
        if ((pCtl->rcLeft <= pCur[0] && pCtl->rcRight >= pCur[0] &&
             pCtl->rcTop <= (*(int32_t * *)&BrGlNavThis5DD8)[1] && pCtl->rcBottom >= (*(int32_t * *)&BrGlNavThis5DD8)[1]) ||
            fCurrent != 0) {
            if (fCurrent && (g_act0 != 0 || g_act1 != 0 || g_act2 != 0 || g_act3 != 0))
                return 1;

            if (pCtl->flags1C & 0x40000) {
                if ((*(BrObj2C * *)&g_pBrAA2E80)->f2C != 0 || (*(BrObj2C * *)&g_pBrAA2E80)->f30 != 0)
                    pCtl->flags1C |= 0x80002;
                else
                    pCtl->flags1C &= ~2;
            } else if (((*(int32_t *)&g_5BB4) == 0 && ((g_BrDikEdge[28]) != 0 || (g_BrDikEdge[156]) != 0)) ||
                       BrInputAnyActive() != 0) {
                pCtl->flags1C |= 2;
            } else {
                pCtl->flags1C &= ~2;
            }
            pCtl->flags1C |= 0x20;
            return 1;
        }
        pCtl->flags1C &= ~0x22;
    }
    return 0;
}

/* ==========================================================================
 * 0x10048180 -- control vtable +0x0C, one frame of one control.
 *
 * This is where a control's HOOKS are finally called: +0x04 unconditionally,
 * then +0x08 when the ACTIVATE bit is set (and the bit is cleared afterwards),
 * or +0x0C when it is not, and +0x18 near the end.
 * ========================================================================== */

/* The child lookup the original performs six times over: the control's phase,
 * that phase's current page, then the page control named by the int16 at
 * +0x2AB6 + 2*i. */
/* (port-only BrNavChild removed) */


/* (port-only BrUiNavCtlFrame_10048180 removed) */


/* ==========================================================================
 * 0x10048530 -- page vtable +0x04, one frame of one page
 * ========================================================================== */

/* (port-only BrUiNavPageFrame_10048530 removed) */


/* ==========================================================================
 * 0x100489A0 -- phase vtable +0x0C, one frame of one phase.
 *
 * The top of the chain. slice3_32.c has this body over BrPhaseFull; see the
 * DUPLICATE OWNERSHIP table in br_uinav.h and the banner beside
 * BrPhaseRun_100489A0 in slice3_32.c. Every global it touches is reached
 * through pNav->pG or through the pAA2904 / pAA2908 members whose single-
 * population rule br_uinav.h states, so no storage is duplicated.
 * ========================================================================== */

/* The two-call teardown both failure exits run. slice3_32.c's is
 * BrScrPhaseBail; the two differ only in the type of `this`.
 *
 * The vtable is passed in rather than re-read because the SECOND exit uses the
 * pointer loaded before the tick call, which is the original's `edi`. */
/* (port-only BrNavPhaseBail removed) */


/* (port-only BrUiNavPhaseRun_100489A0 removed) */


/* ==========================================================================
 * 0x10048AA0 -- phase vtable +0x1C
 * ========================================================================== */

/* (port-only BrUiNavPhaseRelease_10048AA0 removed) */


/* ==========================================================================
 * The two control hooks that move between screens
 * ========================================================================== */

/* XSLICE 0x1007DFE0 -- operator new; does NOT zero. */
/* BrOperatorNew: prototype in br_funcs.h */
/* XSLICE 0x1004F700 -- the enter hook 0x10045AF0 installs. A ported builder. */
/* BrExt_1004F700: prototype in br_funcs.h */

/* (port-only BrUiNavHook_10045AF0 removed) */


/* declared only (the Mac port keeps its own body in ports/macos/patch/); Glide match is src/core/cpp/0x100400E0.cpp */
/* BrUiNavHook_10046C90: prototype in br_funcs.h */

/* ==========================================================================
 * Vtable adapters and the input seam
 * ========================================================================== */

/* (port-only NavV_f04 removed) */

/* (port-only NavV_f08 removed) */

/* (port-only NavV_f0C removed) */

/* (port-only NavV_f10 removed) */

/* (port-only NavV_f20 removed) */

/* (port-only NavV_f3C removed) */

/* (port-only NavV_page04 removed) */

/* (port-only NavV_phase0C removed) */

/* (port-only NavV_phase1C removed) */


/* (port-only BrUiNavInstallCtlVtbl removed) */


/* @n64 0x8020AD50 located */
/* (port-only BrUiNavInstallPageVtbl removed) */


/* (port-only BrUiNavInstallPhaseVtbl removed) */


/* 0x100603A0's two edges, and only those two. The step and the cursor are
 * written together because that function writes them together.
 *
 * NOT TAGGED. This is a FRAGMENT -- 48 bytes of a 939-byte function -- and
 * config/shared.csv maps d3d 0x100603A0 to Glide 0x10059410, which is a true
 * twin (939 bytes in both binaries) whose real transcription is BrGlNavPoll
 * below. Tagged @implements until 2026-09-03, which put two names on one
 * address and scored a 48-byte fragment against the whole function. Fragments
 * and thunks must not carry @implements -- docs/VC5-IDIOMS.md. */
/* (port-only BrUiNavMove removed) */


/* (port-only BrUiNavSetStep removed) */


/* (port-only BrUiNavSetActivate removed) */


/* @n64 0x8026B9F0 located */
/* (port-only BrUiNavSelection removed) */


/* ====================================================================== *
 * 0x10059410 -- the GLIDE mouse poller (939 B).  thiscall on the nav
 * record (this in ecx, one never-read stack arg, callee-pops), reached
 * through the fastcall shim.  DirectInput device at this+0x50: Acquire /
 * GetDeviceState(0x10) with the DIERR_INPUTLOST (0x8007001E) reacquire
 * loop, the device pointer re-read from the record at every use.  Then
 * axis accumulate+clamp, button bits & 0x80, the timeout/page-flip
 * bookkeeping, per-button edge machines, and the common tail hook
 * (g_..5DD8 = this; 0x10059060).
 * ====================================================================== */

typedef struct BrDInVtbl_ {
    int (__stdcall *f00[7])(void *);
    int (__stdcall *pfnAcquire)(void *);                      /* +0x1C */
    int (__stdcall *f20)(void *);
    int (__stdcall *pfnGetDeviceState)(void *, int, void *);  /* +0x24 */
} BrDInVtbl_;
typedef struct BrDInDev_ { BrDInVtbl_ *pVtbl; } BrDInDev_;

typedef struct BrGlNavRec {
    int32_t   x, y, z;            /* +0x00 +0x04 +0x08 */
    int32_t   xPrev, yPrev, zPrev;/* +0x0C +0x10 +0x14 */
    uint8_t   pad18[0xC];         /* +0x18 */
    uint8_t   ab[4];              /* +0x24 button bits (&0x80) */
    uint8_t   abPrev[4];          /* +0x28 */
    int32_t   aHeld[4];           /* +0x2C */
    int32_t   aClick[4];          /* +0x3C */
    int32_t   iIdle4C;            /* +0x4C */
    BrDInDev_ *pDev;              /* +0x50 */
} BrGlNavRec;

/* 64-bit core: declared once, in br_globals.h or its struct's header */     /* 0x10AC5B9C  poll disable          */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* 0x10AC670C  last poll time        */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x10AC6710  idle accumulator      */
/* 64-bit core: declared once, in br_globals.h or its struct's header */       /* 0x10AC6708  timeout latch         */
/* 64-bit core: declared once, in br_globals.h or its struct's header */        /* 0x10AC6718                        */
/* 64-bit core: declared once, in br_globals.h or its struct's header */        /* 0x10AC6714                        */
/* 64-bit core: declared once, in br_globals.h or its struct's header */     /* 0x10AC5F3C                        */
/* 64-bit core: declared once, in br_globals.h or its struct's header */     /* 0x10AC5F40                        */
/* 64-bit core: declared once, in br_globals.h or its struct's header */     /* 0x10AC66B0                        */
/* 64-bit core: declared once, in br_globals.h or its struct's header */     /* 0x10AC66B8                        */
/* 64-bit core: declared once, in br_globals.h or its struct's header */       /* 0x10AC66F8                        */
/* 64-bit core: declared once, in br_globals.h or its struct's header */       /* 0x10AC66FC                        */
/* 64-bit core: declared once, in br_globals.h or its struct's header */       /* 0x10AC6700                        */
/* 64-bit core: declared once, in br_globals.h or its struct's header */       /* 0x10AC6704                        */
/* 64-bit core: declared once, in br_globals.h or its struct's header */       /* 0x10AC610C                        */
/* 64-bit core: declared once, in br_globals.h or its struct's header */       /* 0x10AC6114                        */
/* SIGNED, and that is load-bearing: the two backward-step stores put -1 in
 * here, and VC5 materialises the constant differently depending on the
 * global's signedness. Declared `uint16_t` it emits `mov edx, 0xffff`
 * (5 bytes, the value already narrowed); declared `int16_t` it emits
 * `or edx, 0xffffffff` (3 bytes, a full-width -1 that the `mov word` store
 * then truncates) -- which is what the original does. Neither an explicit
 * `(uint16_t)` cast, a shared `int32_t step = -1` local, nor a literal -1
 * at each store moves it; only the DESTINATION's signedness does. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* 0x100AAB7C                        */
/* 64-bit core: declared once, in br_globals.h or its struct's header */     /* 0x10AC5BC4                        */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* 0x10AC6748  last-activity time    */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* 0x10AC6720                        */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* 0x10AC6724                        */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* 0x10AC6728                        */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* 0x10AC672C                        */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* 0x10AC5DD8                        */

/* BrGlNavTimeNow: prototype in br_funcs.h */
/* BrGlNavKeyLeft: prototype in br_funcs.h */
/* BrGlNavKeyRight: prototype in br_funcs.h */
/* BrGlNavTail: prototype in br_funcs.h */

/* TAG LIVE WITH DIFFS (drawcar convention): 3 regions / ~10 insns of
 * coupled coloring remain -- the -1 materialization (`or edx,-1` vs
 * folded 0xffff; a shared int local still folds), the eax/ecx flag-home
 * swap, the cl/al 0x80 swap, and one fresh `xor eax,eax`.  Everything
 * structural is byte-shape exact: rotated-while reacquire loop with
 * duplicated condition, dword+byte button loads with batch masks, edge
 * machines, tail hook.  Permuter bait (first_live/recompute class).
 *
 * Measured 2026-09-03: 939 vs 945 bytes, 298 vs 297 instructions, REGNORM
 * 1+2 -- recomp EXTRA is one `mov R,I`, MISSING is one `xor R,R` and one
 * `or R,I`.  The whole size gap is the two bytes at +0x158, where the
 * original has `or edx,0xffffffff` and we get `mov edx,0xffff`: VC5 proves
 * that only `dx` is ever read (both uses are `mov word ptr [g],dx`) and
 * NARROWS the constant, where the original keeps a full 32-bit -1.  FOUR
 * MORE DEAD SPELLINGS, all byte-identical to what is here, do not re-run:
 * dropping the `(uint16_t)` cast on the store, declaring `step` as
 * `int16_t`, declaring it `uint32_t 0xFFFFFFFFu`, and double-casting
 * `(uint16_t)(int16_t)step`.  The value has no 32-bit use in the ORIGINAL
 * either, so this is not reachable by giving it one. */
/* WHAT IT DOES: poll the menu's navigation input once per frame -- reads the
 * stick and buttons, moves the highlight, and fires the selected control's
 * action. The front end's input step. */
/* @t4-pass 0x10059410 1 2026-09-07 probes 150 bytes 943 insns 297 regions 3 rows 1 census yes  (tools/crank.py) */
/* @t4-pass 0x10059410 2 2026-09-07 probes 150 bytes 943 insns 297 regions 3 rows 1 census yes  (tools/crank.py) */
/* @t3 0x10059410 2026-09-09 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 943/939 insns 297/298 rows 1+0 regions 3 oracle UNCLASSIFIED
 * @t3-effort passes 2 zero-movement 1 2
 * Residue: regions 1-2 are a whole-body eax<->ecx/al<->cl transposition; the
 * lone row is the original rematerialising a fresh zero (xor eax,eax) for the
 * Edge672x store run while ours reuses the live edi zero -- chained
 * assignment, a named zero and both chain orders are value-numbered to the
 * same code (probed 2026-09-09, 3 variants, byte-identical), the same
 * mechanism as br_tex3d's specMem note.  Crank passes 1-2 are the ledger.
 * Do not reopen before the end-grind. */
/* @implements 0x10059410 glide BrGlNavPoll */
/* RESIDUE, measured 2026-09-03: 943 B / 297 insns against 939 / 298, and the
 * register-blind gap is 0+1 -- ONE missing `xor R,R`.  divergence.py (key 8)
 * finds exactly THREE regions and all three are the same cause: a localised
 * EAX <-> ECX rotation across 0x153..0x2B9.
 *
 *   +0x153  orig `mov eax,[BrGlNavT6708]`   ours `mov ecx,[...]`   (+1 B:
 *           the A1 accumulator form is one byte shorter than 8B 0D)
 *   +0x19F  orig `mov ecx,ebx` / `mov ecx,[BrGlNavF6700]` for `f`;
 *           ours puts `f` in eax                                   (+1 B)
 *   +0x29F  orig emits a FRESH `xor eax,eax` and stores eax into the four
 *           BrGlNavEdge globals (A3 form, 5 B each); ours reuses the pinned
 *           zero already in edi (89 3D form, 6 B each)             (+4-2 B)
 *
 * So the whole +4 is the accumulator encoding, and the missing `xor` is the
 * EFFECT of the rotation, not its cause: the original had eax free at 0x29F
 * because it kept `f` and the T6708 load in ecx.  Every store in that block
 * is already a literal 0 in the source.
 *
 * DEAD PROBE: wrapping the `f` block differently (extra scope, a dummy
 * static to defeat the block merge) leaves it at 943/297/0+1.  A fix has to
 * change which value VC5 puts in eax at 0x153, not the zero stores. */
void __fastcall BrGlNavPoll(BrGlNavRec *pNav, int _unused)
{
    uint8_t aState[0x10];
    int32_t t;
    BrDInDev_ *pDev;
    (void)_unused;

    if (BrGlNavOff5B9C != 0)
        return;

    t = BrSub10075020();
    (g_BrAA3398[6]) += t - (g_BrAA3398[5]);
    (g_BrAA3398[5]) = t;
    if ((g_BrAA3398[6]) > 0x78)
        (g_BrAA3398[4]) = 1;

    if (pNav->pDev == 0)
        return;

    pDev = pNav->pDev;
    pDev->pVtbl->pfnAcquire(pDev);
    pDev = pNav->pDev;
    while (pDev->pVtbl->pfnGetDeviceState(pDev, 0x10, aState) ==
           (int32_t)0x8007001E) {
        pDev = pNav->pDev;
        if (pDev->pVtbl->pfnAcquire(pDev) < 0)
            break;
        pDev = pNav->pDev;
        pDev->pVtbl->pfnAcquire(pDev);
        pDev = pNav->pDev;
    }

    pNav->x += *(int32_t *)&aState[0];
    pNav->y += *(int32_t *)&aState[4];
    pNav->z += *(int32_t *)&aState[8];
    if (pNav->x < 0)
        pNav->x = 0;
    else if (pNav->x >= BrGlNavMaxX)
        pNav->x = BrGlNavMaxX;
    if (pNav->y < 0)
        pNav->y = 0;
    else if (pNav->y >= BrGlNavMaxY)
        pNav->y = BrGlNavMaxY;

    {
        /* Buttons 0/1 come out of ONE dword read (cl/ch extraction);
         * 2/3 are plain byte loads; the any-pressed test reads the same
         * masked locals the stores used. */
        uint32_t bw = *(uint32_t *)&aState[0xC];
        uint8_t  b2 = aState[0xE];
        uint8_t  b3 = aState[0xF];
        uint8_t  b0 = (uint8_t)bw;
        uint8_t  b1 = ((uint8_t *)&bw)[1];
        b0 &= 0x80;
        b1 &= 0x80;
        b2 &= 0x80;
        b3 &= 0x80;
        pNav->ab[0] = b0;
        pNav->ab[1] = b1;
        pNav->ab[2] = b2;
        pNav->ab[3] = b3;
        if (b0 != 0 || b1 != 0 || b2 != 0 || b3 != 0)
            pNav->iIdle4C = 1;
    }

    if ((g_BrDikEdge[87]) != 0)
        BrCdTrackPrev();
    if ((g_BrDikEdge[88]) != 0)
        BrCdTrackNext();

    if ((g_BrAA3398[4]) != 0) {
        if ((g_BrDikState[200]) & 0x80) {
            (g_BrAA3398[0]) = 1;
            BrGlNavStepAB7C = -1;
            (g_BrAA3398[6]) = 0;
        }
        if ((g_BrDikState[208]) & 0x80) {
            (g_BrAA3398[1]) = 1;
            BrGlNavStepAB7C = 1;
            (g_BrAA3398[6]) = 0;
        }
    }
    {
        int32_t f;
        if ((g_BrDikEdge[203]) != 0) {
            f = 1;
            BrGlNavStepAB7C = -1;
            (g_BrAA3398[6]) = 0;
        } else {
            f = (g_BrAA3398[2]);
        }
        if ((g_BrDikEdge[205]) != 0) {
            (g_BrAA3398[3]) = 1;
            BrGlNavStepAB7C = 1;
            (g_BrAA3398[6]) = 0;
        }
        if (f != 0) {
            pNav->ab[0] = 1;
            pNav->iIdle4C = 0;
            (g_BrAA3398[4]) = 0;
        }
        if ((g_BrAA3398[3]) != 0) {
            pNav->ab[1] = 1;
            pNav->iIdle4C = 0;
            (g_BrAA3398[4]) = 0;
        }
    }
    if ((g_BrAA3398[4]) != 0) {
        if ((g_BrAA3398[0]) != 0) {
            --BrGlNavCur5BC4;
            (g_BrAA3398[4]) = 0;
        }
        if ((g_BrAA3398[1]) != 0) {
            ++BrGlNavCur5BC4;
            (g_BrAA3398[4]) = 0;
        }
    }
    (g_BrAA3398[3]) = 0;
    (g_BrAA3398[2]) = 0;
    (g_BrAA3398[1]) = 0;
    (g_BrAA3398[0]) = 0;

    if (pNav->x != pNav->xPrev || pNav->y != pNav->yPrev ||
        pNav->ab[0] != pNav->abPrev[0] || pNav->ab[1] != pNav->abPrev[1] ||
        pNav->ab[2] != pNav->abPrev[2] || pNav->ab[3] != pNav->abPrev[3])
        BrGlNavLast6748 = BrSub10075020();

    pNav->xPrev = pNav->x;
    pNav->yPrev = pNav->y;
    pNav->abPrev[0] = pNav->ab[0];
    pNav->abPrev[1] = pNav->ab[1];
    pNav->abPrev[2] = pNav->ab[2];
    pNav->abPrev[3] = pNav->ab[3];

    BrGlNavEdge6720 = 0;
    BrGlNavEdge6724 = 0;
    BrGlNavEdge6728 = 0;
    BrGlNavEdge672C = 0;

    if (pNav->ab[0] != 0 && pNav->aHeld[0] == 0) {
        pNav->aHeld[0] = 1;
    } else if (pNav->ab[0] == 0 && pNav->aHeld[0] != 0) {
        pNav->aHeld[0] = 0;
        pNav->aClick[0] = 1;
        pNav->iIdle4C = 0;
        BrGlNavEdge6720 = 1;
    } else {
        pNav->aClick[0] = 0;
    }
    if (pNav->ab[1] != 0 && pNav->aHeld[1] == 0) {
        pNav->aHeld[1] = 1;
    } else if (pNav->ab[1] == 0 && pNav->aHeld[1] != 0) {
        pNav->aHeld[1] = 0;
        pNav->aClick[1] = 1;
        pNav->iIdle4C = 0;
        BrGlNavEdge6724 = 1;
    } else {
        pNav->aClick[1] = 0;
    }
    if (pNav->ab[2] != 0 && pNav->aHeld[2] == 0) {
        pNav->aHeld[2] = 1;
    } else if (pNav->ab[2] == 0 && pNav->aHeld[2] != 0) {
        pNav->aHeld[2] = 0;
        pNav->aClick[2] = 1;
        pNav->iIdle4C = 0;
        BrGlNavEdge6728 = 1;
    } else {
        pNav->aClick[2] = 0;
    }
    if (pNav->ab[3] != 0 && pNav->aHeld[3] == 0) {
        pNav->aHeld[3] = 1;
    } else if (pNav->ab[3] == 0 && pNav->aHeld[3] != 0) {
        pNav->aHeld[3] = 0;
        pNav->aClick[3] = 1;
        pNav->iIdle4C = 0;
        BrGlNavEdge672C = 1;
    } else {
        pNav->aClick[3] = 0;
    }

    BrGlNavThis5DD8 = pNav;
    BrInputLatchUpdate();
}

