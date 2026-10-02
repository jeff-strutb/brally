#include "slice2_25.h"   /* br_globals: its objects */
#include "br_ui.h"   /* BrUiCtl_, the canonical record */
/* WHAT IT DOES: leave this menu page: run its leave routine, destroy the
 * page object, and make its parent current again. One of a family that
 * differ only in which parent they return to and which state flags they
 * clear, clearing five selection indices on the way out. */
/* @implements 0x1003E030 glide BrPhaseLeave_10044AE0
 * @cpp_kind method
 * @cpp_symbol ?BrPhaseLeave_10044AE0@@YAHPAVGameObj@@@Z
 *
 * Phase-leave family, esi-form (five zero stores through esi), with one
 * trailing cdecl helper call after the g_cur swap. No EH.
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

typedef char chk_sub[(unsigned)&((GameObj *)0)->pSub == 0x2AE8 ? 1 : -1];

/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* Fn55F0: prototype in br_funcs.h */

int BrPhaseLeave_10044AE0(GameObj *pGame)
{
    Phase *pObj;

    (*(GameSub * *)&((BrUiCtl_ *)(pGame))->pOwner)->s7();
    pObj = (Phase *)((*(Phase * *)&g_brPAA29B8));
    if (pObj != 0)
        pObj->f00(1);
    g_2948 = 0;
    g_29B8 = 0;
    g_brPAA29D8 = 0;
    g_brPAA29D4 = 0;
    g_5BD8 = 0;
    (*(Phase * *)&g_brPAA29B8) = (Phase *)((BrOptObj *)(g_5C98));
    BrExt_1003BF60();
    return 0;
}
