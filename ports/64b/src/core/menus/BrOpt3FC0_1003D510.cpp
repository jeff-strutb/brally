#include "slice2_25.h"   /* br_globals: its objects */
#include "br_ui.h"   /* BrUiCtl_, the canonical record */
/* WHAT IT DOES: leave this page and return to its parent, clearing two state
 * flags on the way out. */
/* @implements 0x1003D510 glide BrOpt3FC0
 * @cpp_kind method
 * @cpp_symbol ?BrOpt3FC0@@YAHPAVGameObj@@@Z
 *
 * Twin of 0x1003D4A0: same slot+0x1C / slot+0x00 thiscall pair, then two
 * zero-stores and a pointer copy. No named temp on the copy (ecx form).
 * No EH.
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

int BrOpt3FC0(GameObj *pGame)
{
    Phase *pObj;

    (*(GameSub * *)&((BrUiCtl_ *)(pGame))->pOwner)->s7();
    pObj = (Phase *)((*(Phase * *)&g_brPAA29B8));
    if (pObj != 0)
        pObj->f00(1);
    g_5CB0 = 0;
    DAT_10ac5d00 = 0;
    (*(Phase * *)&g_brPAA29B8) = (Phase *)((BrOptObj *)(g_2908));
    return 0;
}
