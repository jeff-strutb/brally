#include "slice2_25.h"   /* br_globals: its objects */
#include "br_ui.h"   /* BrUiCtl_, the canonical record */
/* WHAT IT DOES: leave this menu page: run its leave routine, destroy the
 * page object, and make its parent current again. One of a family that
 * differ only in which parent they return to and which state flags they
 * clear, clearing four selection flags. */
/* @implements 0x1003F8A0 glide BrSub10046400
 * @cpp_kind method
 * @cpp_symbol ?BrSub10046400@@YAHPAVGameObj@@@Z
 *
 * Phase-leave family, esi-form: four zero stores make VC5 materialize the
 * zero in esi (`xor esi,esi; cmp ecx,esi` doubles as the g_cur null test,
 * `push esi` prologue, `mov [g],esi` stores). Same source shape as the
 * C7-05 form gen_phaseleave stamps. No EH.
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

int BrSub10046400(GameObj *pGame)
{
    Phase *pObj;

    (*(GameSub * *)&((BrUiCtl_ *)(pGame))->pOwner)->s7();
    pObj = (Phase *)((*(Phase * *)&g_brPAA29B8));
    if (pObj != 0)
        pObj->f00(1);
    g_5CAC = 0;
    g_brPAA29E4 = 0;
    g_pGame = 0;
    g_5BB4 = 0;
    (*(Phase * *)&g_brPAA29B8) = (Phase *)((BrOptObj *)((*(Phase * *)&g_brPAA2950)));
    return 0;
}
