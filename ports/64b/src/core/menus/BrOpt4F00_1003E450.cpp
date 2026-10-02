#include "br_race.h"   /* br_globals: its objects */
#include "br_ui.h"   /* BrUiCtl_, the canonical record */
#include "slice2_25.h"   /* br_globals: its objects */
/* WHAT IT DOES: leave this menu page: run its leave routine, destroy the
 * page object, and make its parent current again. One of a family that
 * differ only in which parent they return to and which state flags they
 * clear, setting a return mode the parent reads. */
/* @implements 0x1003E450 glide BrOpt4F00
 * @cpp_kind method
 * @cpp_symbol ?BrOpt4F00@@YAHPAVGameObj@@@Z
 *
 * Phase-leave family member (see tools/gen_phaseleave.py), hand-filed:
 * its current-phase slot is g_5CC0, not the family's g_cur, and the
 * tail sets a mode global to 2 instead of a zero store.
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
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
#define g_mode (*(int *)((char *)&g_brRaceRules + 0xC)) /* BR_LP64_BYTE_VIEW */

int BrOpt4F00(GameObj *pGame)
{
    Phase *pObj;

    (*(GameSub * *)&((BrUiCtl_ *)(pGame))->pOwner)->s7();
    pObj = g_5CC0;
    if (pObj != 0)
        pObj->f00(1);
    g_5CC0 = 0;
    (*(Phase * *)&g_brPAA29B8) = (Phase *)((BrOptObj *)(g_5CB4));
    g_mode = 2;
    return 0;
}
