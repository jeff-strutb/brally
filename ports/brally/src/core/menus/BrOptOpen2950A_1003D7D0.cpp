/* BrOptOpen2950A_1003D7D0.cpp -- menus, one C++ TU: 0x1003D620 CtlD620 (a
 * page opener) and, after it, 0x1003D7D0 BrOptOpen2950A (join a network game
 * and open its lobby). */
#define _CRTIMP __declspec(dllimport)
#include "slice2_25.h"   /* br_globals: its objects */
#include "br_ui.h"   /* BrUiCtl_, the canonical record */
#include "br_phase.h"   /* BrPhase_, the canonical record */
#include <string.h>

/* WHAT IT DOES: open this menu page: create its object the first time it is
 * asked for, run its enter routine and make it the current page. One of a
 * family of near-identical page openers -- each owns its own page slot, and
 * the page object is created ONCE and reused for the rest of the run. */
/* @implements 0x1003D620 glide CtlD620
 * @cpp_kind method
 * @cpp_symbol ?Activate@CtlD620@@QAEHXZ
 *
 * Shared-return activate: `return 1` sits after the if/else so `mov eax,1`
 * stays out of the flag stores (they remain `c7` immediates). Ctor
 * DECLARED, no dtor: unwind is operator delete (maxState=1).
 *
 * 0xC8 Phase: +0 vtbl, +4 pfnEnter, +0xC f0C, +0x68 f68.
 */
class Phase;

/* Phase: br_coretypes.h */

class Phase {
public:
    void *vtbl;
    PhaseEnterFn pfnEnter;
    void *pfnHook;
    int f0C;
    char _pad[0x58];
    int f68;
    char _rest[0x5C];
    Phase();
};






/* 64-bit core: declared once, in br_globals.h or its struct's header */
#define g_slot g_294C
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* EnterFn was a stand-in; the original calls BrOptFn100575F0 (?BrOptFn100575F0@@YAHPAVGameUi@@@Z).  Declared under
 * its real symbol so the relocation resolves by name. */
class GameUi;
/* BrOptFn100575F0: prototype in br_funcs.h */
#define EnterFn ((void (*)(Phase *))BrOptFn100575F0)

class CtlD620 {
public:
    int Activate();
};

int CtlD620::Activate()
{
    Phase *p;

    p = g_slot;
    if (p == 0) {
        p = ((Phase *)br_new_obj(sizeof(BrPhase_), (void *(*)(void *))BrOptObjCtor));
        g_slot = p;
        (*(Phase * *)&g_brPAA29B8) = (Phase *)((BrOptObj *)(p));
        if (p == 0)
            return 0;
        (*(PhaseEnterFn *)&((BrPhase_ *)(p))->pfnEnter) = EnterFn;
        (*(PhaseEnterFn *)&((BrPhase_ *)(g_slot))->pfnEnter)(g_slot);
        (*(int *)&((BrPhase_ *)((*(Phase * *)&g_brPAA29B8)))->f0C) = 1;
        (*(int *)&((BrPhase_ *)((*(Phase * *)&g_brPAA29B8)))->f68) = 1;
    } else {
        (*(Phase * *)&g_brPAA29B8) = (Phase *)((BrOptObj *)(p));
    }
    return 1;
}


class NameList55 {
public:
    int  hdr;
    char asz[100][0x104];
    NameList55();
};

class OptObj41B60 {
public:
    virtual void v0();

    int       (*pfnOpen)(OptObj41B60 *);  /* +0x04 */
    void       *f008;
    int         f00C;
    short       w010;
    short       w012;
    char        pad014[0x50];
    void       *f064;
    int         f068;
    int         a06C[20];
    short       w0BC;
    char        pad0BE[2];
    NameList55 *pC0;
    NameList55 *pC4;

    OptObj41B60();
};



struct PhaseCtx5D10 {
    int   f000;
    int   f004;
    int (*pfnTick)(void *);       /* +0x08 */
};

struct Screen5D2C {
    char           pad[0x1E164];
    unsigned short w1E164;
};

extern "C" {
/* 64-bit core: g_brAA2878 is defined once, in br_globals.c */
/* 64-bit core: g_brAA287C is defined once, in br_globals.c */
/* 64-bit core: g_brAA2884 is defined once, in br_globals.c */
/* 64-bit core: g_brAA2898 is defined once, in br_globals.c */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: g_brPAA2950 is defined once, in br_globals.c */
/* 64-bit core: g_brPAA29B8 is defined once, in br_globals.c */
/* 64-bit core: g_brPAA29D4 is defined once, in br_globals.c */
/* 64-bit core: g_brPAA29D8 is defined once, in br_globals.c */
/* 64-bit core: g_brPAA2A18 is defined once, in br_globals.c */
/* 64-bit core: declared once, by its definition's header */          /* 0x100358F0 */
/* 64-bit core: declared once, by its definition's header */          /* 0x10035870 */
/* BrOptFn10057C10: prototype in br_funcs.h */
/* 64-bit core: declared once, by its definition's header */          /* 0x1003DEC0 */
}

/* WHAT IT DOES: joins somebody else's network game and opens the lobby
 * screen for it. It refuses to go ahead when the player has not typed a
 * long enough name in one of the modes, and falls back to setting the
 * connection up first if there is nothing to join yet.  The lobby's menu
 * object is created on first use (its open hook installed and run), and
 * the phase's tick hook is pointed at the lobby stepper. */
/* @implements 0x1003D7D0 glide BrOptOpen2950A
 * @cpp_kind free
 * @cpp_symbol ?BrOptOpen2950A@@YAHPAX@Z
 *
 * cdecl, one (unused) arg, EH frame for the single `new OptObj41B60`
 * (unwind state 0 while its constructor runs; the raw pointer is spilled
 * to [esp+4] for the funclet).  The strlen is the repne-scasb intrinsic,
 * which is why edi is the one callee-saved register.  The failed-new
 * return hands back the null pointer in eax without a fresh zero.
 *
 * Byte-exact 2026-09-27.  Two source facts carry it:
 *   - forward `goto done` to ONE `return 1` after the open block, the
 *     2/3-mode arm ending in a single `goto done` after its if/else;
 *   - a function compiled ahead of it in the TU.  Alone in its TU, VC5 /O2
 *     tail-duplicates that epilogue at every unconditional jump (383-400 B);
 *     with any predecessor the original's single epilogue comes out.  The
 *     original's neighbour 0x1003D620 CtlD620 is filed above it here.
 * Earlier probes, kept so they are not re-run: early `return 1`s (400),
 * `goto done` at every site with the open block behind `goto open` (399),
 * a success flag tested once (334-340, VC5 keeps the `test/je`), `||`/`?:`
 * conditions and an inlined bool predicate (339-356, `neg/sbb`), /O1 /Os
 * /Ob0 /Ob2 /Ox /Gy /Gi-chain and the /O2 components one by one.
 */
int BrOptOpen2950A(void *pUnused)
{
    OptObj41B60 *p;

    g_host = 0;
    DAT_10ac5bf0 = 0;

    if (DAT_10ac5bd0 == 0) {
        if (DAT_10ac5bd4 == 2 || DAT_10ac5bd4 == 3) {
            DAT_10ac5bf0 = 1;
            if (DAT_10ac5bd4 == 2 && strlen(g_aBrA9CDF0) < 7)
                goto done;
            if ((*(void * *)&g_brPAA29D8) != 0 && (*(unsigned short *)&((BrUiCtl_ *)((*(Screen5D2C * *)&g_brPAA29D4)))->list.count) > 0) {
                if (BrSub1003C260() != 0)
                    goto open;
            } else {
                BrTimerStart();
            }
            goto done;
        } else {
            if (BrSub1003C260() == 0)
                goto done;
        }
    }
open:
    if ((*(OptObj41B60 * *)&g_brPAA2950) == 0) {
        p = ((OptObj41B60 *)br_new_obj(sizeof(BrPhase_), (void *(*)(void *))BrOptObjCtor));
        (*(OptObj41B60 * *)&g_brPAA2950) = (OptObj41B60 *)((struct Phase *)((OptObj41B60 *)((struct Phase *)(p))));
        (*(OptObj41B60 * *)&g_brPAA29B8) = (OptObj41B60 *)((BrOptObj *)(p));
        if (p == 0)
            return 0;
        (*(int (**)(OptObj41B60 *))&((BrPhase_ *)(p))->pfnEnter) = (int (*)(OptObj41B60 *))BrOptFn10057C10;
        (*(int (**)(OptObj41B60 *))&((BrPhase_ *)((*(OptObj41B60 * *)&g_brPAA2950)))->pfnEnter)((*(OptObj41B60 * *)&g_brPAA2950));
        (*(int *)&((BrPhase_ *)((*(OptObj41B60 * *)&g_brPAA29B8)))->f0C) = 1;
        (*(int *)&((BrPhase_ *)((*(OptObj41B60 * *)&g_brPAA29B8)))->f68) = 1;
    } else {
        (*(OptObj41B60 * *)&g_brPAA29B8) = (OptObj41B60 *)((BrOptObj *)((*(OptObj41B60 * *)&g_brPAA2950)));
    }
    (*(int (**)(void *))&((BrPhase_ *)((*(PhaseCtx5D10 * *)&g_29B8)))->pfnHook) = BrPhaseLeave_10044970;
done:
    return 1;
}

/* C entry points (generated by ports/brally/tools/methodfwd.py) */
extern "C" int CtlD620_fn(void *self)
{
    return ((class CtlD620 *)self)->Activate();
}
/* end of C entry points */
