#include "br_race.h"   /* br_globals: its objects */
#include "br_phase.h"   /* BrPhase_, the canonical record */
#include "br_ui.h"   /* BrUiCtl_, the canonical record */
/* WHAT IT DOES: start the credits rolling: sets the mode that plays them and
 * closes the page that launched them. */
/* @implements 0x1003AED0 glide BrUiCreditsAction_1003AED0
 * @cpp_kind method
 * @cpp_symbol ?BrUiCreditsAction_1003AED0@@YAHPAVGameObj@@@Z
 *
 * Guarded-vcall family variant: cdecl helper on a global's address, mode
 * stores forked on a global test (zero materialized in edx and reused for
 * the stores and the vcall arg), then the member zero-store + slot-6
 * vcall. No EH.
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
    char padA[0x64];
    int f68;
};

class GameObj {
public:
    char pad[0x2AE8];
    GameSub *pSub;
};



/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* FnAF30: prototype in br_funcs.h */

int BrUiCreditsAction_1003AED0(GameObj *pGame)
{
    BrExt_100419D0(g_strA);
    (*(int *)((char *)&g_brRaceRules + 0xC)) /* BR_LP64_BYTE_VIEW */ = 4;
    if (g_5D98 != 0) {
        g_5bc760 = 2;
        g_5BF4 = 0;
    } else {
        g_5bc760 = 1;
    }
    (*(int *)&((BrPhase_ *)((*(GameSub * *)&((BrUiCtl_ *)(pGame))->pOwner)))->f68) = 0;
    (*(GameSub * *)&((BrUiCtl_ *)(pGame))->pOwner)->s6(0);
    return 0;
}
