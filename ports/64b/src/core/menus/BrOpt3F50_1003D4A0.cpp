#include "br_race.h"   /* br_globals: its objects */
#include "br_ui.h"   /* BrUiCtl_, the canonical record */
#include "slice2_25.h"   /* br_globals: its objects */
/* WHAT IT DOES: leave this page and return to a specific parent, setting the
 * mode that tells the parent what happened. */
/* @implements 0x1003D4A0 glide BrOpt3F50
 * @cpp_kind method
 * @cpp_symbol ?BrOpt3F50@@YAHPAVGameObj@@@Z
 *
 * Free cdecl. Slot+0x1C is a no-arg virtual thiscall
 * (`mov edx,[ecx]; call [edx+0x1C]`). Slot+0x00 with one stack arg is
 * `mov eax,[ecx]; push 1; call [eax]`: C __fastcall edx-slot colours
 * the vtbl into edx. No EH (no new).
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
#define g_mode DAT_10ac5bd4
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

int BrOpt3F50(GameObj *pGame)
{
    Phase *pObj;

    g_mode = 2;
    (*(GameSub * *)&((BrUiCtl_ *)(pGame))->pOwner)->s7();
    pObj = (Phase *)((*(Phase * *)&g_brPAA29B8));
    if (pObj != 0)
        pObj->f00(1);
    g_298C = 0;
    (*(Phase * *)&g_brPAA29B8) = (Phase *)((BrOptObj *)((*(Phase * *)&g_2948)));
    return 0;
}
