#include "slice2_25.h"   /* br_globals: its objects */
#include "br_ui.h"   /* BrUiCtl_, the canonical record */
#include "slice3_42.h"   /* br_globals: its objects */
/* WHAT IT DOES: leave this menu page: run its leave routine, destroy the
 * page object, and make its parent current again. One of a family that
 * differ only in which parent they return to and which state flags they
 * clear, and it refreshes the navigation bar afterwards. */
/* @implements 0x1003FB10 glide BrPhaseLeave_100466C0
 * @cpp_kind method
 * @cpp_symbol ?BrPhaseLeave_100466C0@@YAHPAVGameObj@@@Z
 *
 * Phase-leave family: one zero store (C7-05 form), then the plain helper
 * and the static-object thiscall after the g_cur swap. No EH.
 */
#define _CRTIMP __declspec(dllimport)

class GameSub {
public:
    virtual void s0();
    virtual void s1();
    virtual void s2();
    virtual void s3();
    virtual void s4();
    virtual void s5();
    virtual void s6(int);
    virtual void s7();
};

class GameObj {
public:
    char pad[0x2AE8];
    GameSub *pSub;
};

class Phase {
public:
    virtual void *f00(int);
};

class Nav {
public:
    void m(void *);
};

typedef char chk_sub[(unsigned)&((GameObj *)0)->pSub == 0x2AE8 ? 1 : -1];

/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* Fn7920: prototype in br_funcs.h */

int BrPhaseLeave_100466C0(GameObj *pGame)
{
    Phase *pObj;

    (*(GameSub * *)&((BrUiCtl_ *)(pGame))->pOwner)->s7();
    pObj = (Phase *)((*(Phase * *)&g_brPAA29B8));
    if (pObj != 0)
        pObj->f00(1);
    g_5CDC = 0;
    (*(Phase * *)&g_brPAA29B8) = (Phase *)((BrOptObj *)(g_5C70));
    BrOptSave();
    (*(Nav *)&g_BrCtrlCfg).m(&g_navArg);
    return 0;
}
