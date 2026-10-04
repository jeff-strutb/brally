#include "slice2_25.h"   /* br_globals: its objects */
#include "br_ui.h"   /* BrUiCtl_, the canonical record */
/* WHAT IT DOES: leave this menu page: run its leave routine, destroy the
 * page object, and make its parent current again. One of a family that
 * differ only in which parent they return to and which state flags they
 * clear, and it clears the current-page pointer before restoring the parent,
 * so nothing runs against a half-torn-down page. */
/* @implements 0x100403B0 glide BrPhaseLeave_10046F60
 * @cpp_kind method
 * @cpp_symbol ?BrPhaseLeave_10046F60@@YAHPAVGameObj@@@Z
 *
 * Phase-leave family: after the prefix, a SECOND guarded slot-0 vcall on
 * g_5C84 (also released to 0), then the g_cur swap from g_5C60. No EH.
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


/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
#define g_5C84 (*(Phase * *)&g_brPhaseAA292C)
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

int BrPhaseLeave_10046F60(GameObj *pGame)
{
    Phase *pObj;
    Phase *pOld;

    (*(GameSub * *)&((BrUiCtl_ *)(pGame))->pOwner)->s7();
    pObj = (Phase *)((*(Phase * *)&g_brPAA29B8));
    if (pObj != 0)
        pObj->f00(1);
    pOld = (Phase *)(g_5C84);
    (*(Phase * *)&g_brPAA29B8) = 0;
    g_brPhaseAA2974 = 0;
    if (pOld != 0) {
        pOld->f00(1);
        g_5C84 = 0;
    }
    (*(Phase * *)&g_brPAA29B8) = (Phase *)((BrOptObj *)(g_2908));
    return 0;
}
