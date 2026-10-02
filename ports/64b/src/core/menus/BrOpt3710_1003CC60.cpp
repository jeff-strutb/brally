#include "slice2_25.h"   /* br_globals: its objects */
#include "br_ui.h"   /* BrUiCtl_, the canonical record */
#include "slice3_42.h"   /* br_globals: its objects */
/* WHAT IT DOES: leave this page, refresh the navigation bar and re-enter the
 * parent page. */
/* @implements 0x1003CC60 glide BrOpt3710
 * @cpp_kind method
 * @cpp_symbol ?BrOpt3710@@YAHPAVGameObj@@@Z
 *
 * Guarded-vcall family variant: plain helper, then a non-virtual
 * thiscall on a STATIC object (`push offset g_arg; mov ecx,offset g_nav;
 * call m`: only a real C++ member call on a global object reaches the
 * ecx-immediate shape), the slot-6 vcall, and two cdecl helpers with
 * call-site argument reads. No EH.
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

class Nav {
public:
    void m(void *);
};

typedef char chk_sub[(unsigned)&((GameObj *)0)->pSub == 0x2AE8 ? 1 : -1];

/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* Fn7920: prototype in br_funcs.h */
/* Fn9A40: prototype in br_funcs.h */
/* Fn25B0: prototype in br_funcs.h */

int BrOpt3710(GameObj *pGame)
{
    BrOptSave();
    (*(Nav *)&g_BrCtrlCfg).m(&g_navArg);
    (*(GameSub * *)&((BrUiCtl_ *)(pGame))->pOwner)->s6(0);
    BrDPlayShutdown((struct BrDPlayCtx *)(g_brPA9D008));
    BrExt_10038F30(0);
    return 1;
}
