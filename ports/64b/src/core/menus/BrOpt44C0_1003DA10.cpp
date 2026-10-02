#include "br_race.h"   /* br_globals: its objects */
#include "slice2_25.h"   /* br_globals: its objects */
/* WHAT IT DOES: leave this page, return to the parent, and clear a flag bit
 * on a related object so the option it guarded becomes available again. */
/* @implements 0x1003DA10 glide BrOpt44C0
 * @cpp_kind method
 * @cpp_symbol ?BrOpt44C0@@YAHPAVGameObj@@@Z
 *
 * Same thiscall pair as 0x1003D4A0, then hoist the flag-obj pointer
 * before the two NULL stores so orig's `test eax` sits above them.
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

/* FlagObj: br_coretypes.h */

typedef char chk_sub[(unsigned)&((GameObj *)0)->pSub == 0x2AE8 ? 1 : -1];
/* FlagObj: br_coretypes.h */

/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
#define g_mode DAT_10ac5bd4
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* FnBF60: prototype in br_funcs.h */
/* FnC020: prototype in br_funcs.h */

int BrOpt44C0(GameObj *pGame)
{
    Phase *pObj;
    FlagObj *pFlag;

    pGame->pSub->s7();
    pObj = (Phase *)((*(Phase * *)&g_brPAA29B8));
    if (pObj != 0)
        pObj->f00(1);
    pFlag = (*(FlagObj * *)&g_brPAA29D8);
    g_294C = 0;
    g_29B8 = 0;
    (*(Phase * *)&g_brPAA29B8) = (Phase *)((BrOptObj *)((*(Phase * *)&g_2948)));
    if (pFlag != 0)
        pFlag->f1C &= ~0x10;
    if ((g_mode == 0 || g_mode == 1) && g_guardB == 0) {
        BrExt_1003BF60();
        DAT_10ac5bf0 = 1;
        FUN_100356b0();
    }
    return 0;
}
