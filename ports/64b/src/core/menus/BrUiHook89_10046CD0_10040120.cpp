#include "slice2_25.h"   /* br_globals: its objects */
/* WHAT IT DOES: leave this menu page: run its leave routine, destroy the
 * page object, and make its parent current again. One of a family that
 * differ only in which parent they return to and which state flags they
 * clear. */
/* @implements 0x10040120 glide BrUiHook89_10046CD0
 * @cpp_kind method
 * @cpp_symbol ?Hook@Ui89@@YAHPAVGameObj89@@@Z
 *
 * 66 B cdecl(pObj), returns 0. Slot-7 vcall, delete of the current phase,
 * zero two flags, phase swap from the 5C88 slot.
 */
class Sub2AE8b {
public:
    virtual void s0(); virtual void s1(); virtual void s2();
    virtual void s3(); virtual void s4(); virtual void s5();
    virtual void s6(int);
    virtual void s7();          /* +0x1C */
};

class CurPhase89 {
public:
    virtual ~CurPhase89();
};

class GameObj89 {
public:
    char pad[0x2AE8];
    Sub2AE8b *p2AE8;
};

extern "C" {
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
}

int Hook(GameObj89 *pObj)
{
    pObj->p2AE8->s7();

    if ((*(CurPhase89 * *)&g_brPAA29B8) != 0)
        delete (*(CurPhase89 * *)&g_brPAA29B8);

    g_5C6C = 0;
    DAT_10ac5d0c = 0;
    (*(CurPhase89 * *)&g_brPAA29B8) = (CurPhase89 *)((BrOptObj *)((*(CurPhase89 * *)&g_5C88)));
    return 0;
}
