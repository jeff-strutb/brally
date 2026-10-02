#include "slice2_25.h"   /* br_globals: its objects */
/* WHAT IT DOES: leave this menu page: run its leave routine, destroy the
 * page object, and make its parent current again. One of a family that
 * differ only in which parent they return to and which state flags they
 * clear, and it tears down two side panels before the page itself. */
/* @implements 0x10040420 glide BrPhaseLeave_10046FD0
 * @cpp_kind free
 * @cpp_symbol ?BrPhaseLeave_10046FD0@@YAHPAVCtl40420@@@Z
 *
 * cdecl, one arg, `ret`, 119 B. Leave a phase that owns three optional
 * side panels: tear each of them down through the same +0x1C vcall and
 * null it, then the owner's own teardown. After that the usual tail --
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

class Sub2AE8_40420 {
public:
    virtual void s0(); virtual void s1(); virtual void s2();
    virtual void s3(); virtual void s4(); virtual void s5();
    virtual void s6();
    virtual void s7();          /* +0x1C teardown */
};

class Phase40420 {
public:
    virtual ~Phase40420();
};

class Ctl40420 {
public:
    char            pad000[0x2AE8];
    Sub2AE8_40420  *p2AE8;      /* +0x2AE8 */
};


extern "C" {
/* 64-bit core: g_brPhase5C5C is defined once, in br_globals.c */
/* 64-bit core: g_brRoot5C60 is defined once, in br_globals.c */
/* 64-bit core: g_brPending5CCC is defined once, in br_globals.c */
/* 64-bit core: g_brPanel5C8C is defined once, in br_globals.c */
/* 64-bit core: g_brPanel5C90 is defined once, in br_globals.c */
/* 64-bit core: g_brPanel5C94 is defined once, in br_globals.c */
}

int BrPhaseLeave_10046FD0(Ctl40420 *pCtl)
{
    if ((*(Sub2AE8_40420 * *)&g_5C8C) != 0) {
        (*(Sub2AE8_40420 * *)&g_5C8C)->s7();
        (*(Sub2AE8_40420 * *)&g_5C8C) = 0;
    }
    if ((*(Sub2AE8_40420 * *)&g_5C90) != 0) {
        (*(Sub2AE8_40420 * *)&g_5C90)->s7();
        (*(Sub2AE8_40420 * *)&g_5C90) = 0;
    }
    if ((*(Sub2AE8_40420 * *)&g_5C94) != 0) {
        (*(Sub2AE8_40420 * *)&g_5C94)->s7();
        (*(Sub2AE8_40420 * *)&g_5C94) = 0;
    }

    pCtl->p2AE8->s7();

    if ((*(Phase40420 * *)&g_brPAA29B8) != 0)
        delete (*(Phase40420 * *)&g_brPAA29B8);

    g_brPhaseAA2974 = 0;
    (*(Phase40420 * *)&g_brPAA29B8) = (Phase40420 *)((BrOptObj *)((Phase40420 *)((BrOptObj *)((*(Phase40420 * *)&g_2908)))));

    return 0;
}
