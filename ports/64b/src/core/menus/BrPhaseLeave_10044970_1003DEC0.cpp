#include "slice2_25.h"   /* br_globals: its objects */
#include "br_ui.h"   /* BrUiCtl_, the canonical record */
/* WHAT IT DOES: leave this menu page: run its leave routine, destroy the
 * page object, and make its parent current again. One of a family that
 * differ only in which parent they return to and which state flags they
 * clear, and this one also tears a live session down first if one is
 * running. */
/* @implements 0x1003DEC0 glide BrPhaseLeave_10044970
 * @cpp_kind method
 * @cpp_symbol ?Leave@Opt3DEC0@@YAHPAVGameObj3DEC0@@@Z
 *
 * 178 B cdecl(pObj), returns 0. Sibling of 0x1003DF80: guarded slot-6(0)
 * vcall + exit helper, slot-7 vcall, delete of the current phase object,
 * phase swap from the 5CA0 slot, the 0x10-flag clear on the D30 object,
 * the frame helper, a 1 latch, and a mode-gated helper pair with a second
 * 0x10-flag clear. The ~0x10 constant is used twice, so VC5 CSEs it into
 * esi (reusing pObj's register once pObj is dead) and both clears become
 * `and [eax+0x1c], esi`.
 */
class Sub2AE8 {
public:
    virtual void s0(); virtual void s1(); virtual void s2();
    virtual void s3(); virtual void s4(); virtual void s5();
    virtual void s6(int);       /* +0x18 */
    virtual void s7();          /* +0x1C */
};

class CurPhase {
public:
    virtual void *ScalarDtor_(unsigned);   /* slot 0: the scalar deleting destructor */
};

class D30Obj {
public:
    char pad[0x1C];
    unsigned int f1C;           /* +0x1C */
};

class GameObj3DEC0 {
public:
    char pad[0x2AE8];
    Sub2AE8 *p2AE8;             /* +0x2AE8 */
};

extern "C" {
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* BrSub100325B0: prototype in br_funcs.h */
/* BrSub100355F0: prototype in br_funcs.h */
/* BrSub100356B0: prototype in br_funcs.h */
}

extern "C" int BrPhaseLeave_10044970(void *pObj_)
{
    GameObj3DEC0 *pObj = (GameObj3DEC0 *)pObj_;
    int v;

    if (g_guardB != 0) {
        (*(Sub2AE8 * *)&((BrUiCtl_ *)(pObj))->pOwner)->s6(0);
        BrExt_10038F30(0);
    }
    (*(Sub2AE8 * *)&((BrUiCtl_ *)(pObj))->pOwner)->s7();

    if ((*(CurPhase * *)&g_brPAA29B8) != 0)
        br_vdelete((*(CurPhase * *)&g_brPAA29B8));

    g_brPAA2950 = 0;
    (*(CurPhase * *)&g_brPAA29B8) = (CurPhase *)((BrOptObj *)((*(CurPhase * *)&g_2948)));
    if ((*(D30Obj * *)&g_brPAA29D8) != 0)
        (*(unsigned int *)&((BrUiCtl_ *)((*(D30Obj * *)&g_brPAA29D8)))->flags1C) = (*(unsigned int *)&((BrUiCtl_ *)((*(D30Obj * *)&g_brPAA29D8)))->flags1C) & ~0x10u;

    BrExt_1003BF60();
    DAT_10ac5bf0 = 1;

    v = DAT_10ac5bd4;
    if (v == 0 || v == 1) {
        if (g_guardB == 0) {
            FUN_100356b0();
            v = DAT_10ac5bd4;
        }
    }
    if (v == 2 || v == 3) {
        if ((*(D30Obj * *)&g_brPAA29D8) != 0)
            (*(unsigned int *)&((BrUiCtl_ *)((*(D30Obj * *)&g_brPAA29D8)))->flags1C) = (*(unsigned int *)&((BrUiCtl_ *)((*(D30Obj * *)&g_brPAA29D8)))->flags1C) & ~0x10u;
    }
    return 0;
}
