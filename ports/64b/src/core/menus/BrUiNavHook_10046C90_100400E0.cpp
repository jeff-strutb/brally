#include "slice2_25.h"   /* br_globals: its objects */
/* WHAT IT DOES: leave this menu page: run its leave routine, destroy the
 * page object, and make its parent current again. One of a family that
 * differ only in which parent they return to and which state flags they
 * clear, returning all the way to the root page rather than one level up. */
/* @implements 0x100400E0 glide BrUiNavHook_10046C90
 * @cpp_kind free
 * @cpp_symbol ?BrUiNavHook_10046C90@@YAHPAVCtl400E0@@@Z
 *
 * cdecl, one arg, `ret`, 56 B. The "back out to the root" control hook:
 * run the owner's +0x1C teardown vcall, delete whatever phase is current
 * afterwards, clear the pending marker and make the root phase current.
 * Returns 0, which is what stops the caller's frame from continuing.
 *
 * The port body in br_uinav.c reaches its globals through a
 * BrUiNav * globals pointer; the original addresses them directly. Same
 * split as the slice2_23.c / slice3_32.c siblings.
 *
 * The original loads the ROOT phase BEFORE clearing the pending marker and
 * only then stores it as current -- but do NOT introduce a temp to force
 * that. VC5 hoists the load above the unrelated store on its own; naming
 * the value costs 10 diffs by moving it out of eax (the accumulator forms
 * `a1`/`a3`) into ecx. The port body's temp, added to "preserve" the
 * observed order, is exactly what breaks the match here.
 */
#define _CRTIMP __declspec(dllimport)

class Sub2AE8_400E0 {
public:
    virtual void s0(); virtual void s1(); virtual void s2();
    virtual void s3(); virtual void s4(); virtual void s5();
    virtual void s6();
    virtual void s7();          /* +0x1C teardown */
};

class Phase400E0 {
public:
    virtual ~Phase400E0();
};

class Ctl400E0 {
public:
    char            pad000[0x2AE8];
    Sub2AE8_400E0  *p2AE8;      /* +0x2AE8 */
};


extern "C" {
/* 64-bit core: g_brPhase5C5C is defined once, in br_globals.c */
/* 64-bit core: g_brRoot5C60 is defined once, in br_globals.c */
/* 64-bit core: g_brPending5C74 is defined once, in br_globals.c */
}

int BrUiNavHook_10046C90(Ctl400E0 *pCtl)
{
    pCtl->p2AE8->s7();

    if ((*(Phase400E0 * *)&g_brPAA29B8) != 0)
        delete (*(Phase400E0 * *)&g_brPAA29B8);

    g_5C74 = 0;
    (*(Phase400E0 * *)&g_brPAA29B8) = (Phase400E0 *)((BrOptObj *)((Phase400E0 *)((BrOptObj *)((*(Phase400E0 * *)&g_2908)))));

    return 0;
}
