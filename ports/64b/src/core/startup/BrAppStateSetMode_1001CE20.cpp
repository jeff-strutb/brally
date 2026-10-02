#include "br_race.h"   /* br_globals: its objects */
#include "br_phase.h"   /* BrPhase_, the canonical record */
#include "slice2_25.h"   /* br_globals: its objects */
#include "slice3_42.h"   /* br_globals: its objects */
/* WHAT IT DOES: choose the screen resolution for the current situation,
 * writing it into the four places that cache width and height and recording
 * which video mode was picked. */
/* @implements 0x1001CE20 glide BrAppStateSetMode
 * @cpp_kind method
 * @cpp_symbol ?BrAppStateSetMode@@YAHXZ
 *
 * Dense-class row, strong on the C++ screen (both vcall arms: EAX and
 * EDX scratch). Entry forks on (current-phase, active) pair; the
 * demo-globals arm sets the state batch; otherwise dispatch v4()/v3()
 * on the phase by its f0C flag, then the timed phase-leave (f68=0,
 * v6(0)) once the 90000-tick window expires.
 */
#define _CRTIMP __declspec(dllimport)

class Phase {
public:
    virtual void v0();
    virtual void v1();
    virtual void v2();
    virtual void v3();
    virtual void v4();
    virtual void v5();
    virtual void v6(int);
    int f04;
    int f08;
    int f0C;
    int pad[0x16];
    int f68;
};




extern "C" {
/* 64-bit core: g_cur is defined once, in br_globals.c */
/* 64-bit core: g_active is defined once, in br_globals.c */
/* 64-bit core: g_vidMode is defined once, in br_globals.c */
/* 64-bit core: g_demoFlag is defined once, in br_globals.c */
/* 64-bit core: g_time is defined once, in br_globals.c */
/* 64-bit core: g_mode is defined once, in br_globals.c */
/* 64-bit core: g_b3014 is defined once, in br_globals.c */
/* 64-bit core: g_b71530 is defined once, in br_globals.c */
/* 64-bit core: g_b71534 is defined once, in br_globals.c */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: g_bc0 is defined once, in br_globals.c */
/* 64-bit core: g_5bc760 is defined once, in br_globals.c */
/* Fn6C460: prototype in br_funcs.h */
/* Fn6C290: prototype in br_funcs.h */
/* Fn56260: prototype in br_funcs.h */
/* Fn6E280: prototype in br_funcs.h */
}

int BrAppStateSetMode(void)
{
    int w, h;

    if ((*(Phase * *)&g_brPAA29B8) == 0 && (*(int *)&g_AC300) == 0) {
        w = 0x280;
        h = 0x1e0;
        (*(int *)&DAT_105ccb68[19]) = w;
        (*(int *)&DAT_105ccb68[18]) = h;
        BrGbiRectG_A7514 = w;
        g_scrW4 = w;
        BrGbiRectG_A7518 = h;
        (*(int *)&g_brRaceCueBase) = h;
        BrSndBankFree();
        (*(int *)&DAT_105ccb68[21]) = 3;
        return 1;
    }
    if ((*(Phase * *)&g_brPAA29B8) == 0 && (*(int *)&g_AC300) != 0) {
        BrSfxBankLoad(0);
        BrUiBootPreLoopGate();
        (*(int *)&BrGlNavLast6748) = BrSub10075020();
    }
    if (g_demoFlag != 0) {
        (*(int *)&g_brRaceRules.mode) = 1;
        (*(int *)&g_Br0B380C) = 2;
        g_226e7c = 5;
        g_226e80 = 0;
        (*(int *)&g_BrCtrlCfg.active) = 0;
        (*(int * *)&g_BrPadModeBytes) = (*(int (*)[])&g_BrCtrlCfg);
        g_7b320 = 1;
        g_7b328 = 1;
        g_7b32c = 2;
        g_7b324 = 1;
        (*(int *)&g_AC300) = 0;
        (*(int *)&DAT_105ccb68[21]) = 3;
        return 1;
    }
    if ((*(int *)&((BrPhase_ *)((*(Phase * *)&g_brPAA29B8)))->f0C) == 0)
        (*(Phase * *)&g_brPAA29B8)->v4();
    else
        (*(Phase * *)&g_brPAA29B8)->v3();
    if ((*(Phase * *)&g_brPAA29B8) != 0 && (*(int *)&g_AC300) != 0) {
        if (g_bc0 != 0) {
            if ((*(int *)&BrGlNavLast6748) + 0x15f90 < BrSub10075020()) {
                (*(int *)&((BrPhase_ *)((*(Phase * *)&g_brPAA29B8)))->f68) = 0;
                (*(Phase * *)&g_brPAA29B8)->v6(0);
                (*(int *)&g_brRaceRules.mode) = 4;
                g_5bc760 = 0;
                return 1;
            }
        } else {
            (*(int *)&BrGlNavLast6748) = BrSub10075020();
        }
    }
    return 1;
}
