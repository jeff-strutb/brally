#include "slice2_25.h"   /* br_globals: its objects */
/* WHAT IT DOES: leave this menu page: run its leave routine, destroy the
 * page object, and make its parent current again. One of a family that
 * differ only in which parent they return to and which state flags they
 * clear, and this one also tears a live session down first if one is
 * running. */
/* @implements 0x1003DF80 glide BrOptFn10044A30
 * @cpp_kind method
 * @cpp_symbol ?Leave@Opt3DF80@@YAHPAVGameObj3DF80@@@Z
 *
 * 170 B cdecl(pObj), returns 0. Phase-leave: guarded slot-6(0) vcall +
 * exit helper, slot-7 vcall, delete of the current phase object, phase
 * swap, a mode-gated helper pair, and the 0x10-flag clear on the D30
 * object. All state loose globals.
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
    virtual ~CurPhase();
};

class D30Obj {
public:
    char pad[0x1C];
    unsigned int f1C;           /* +0x1C */
    char pad20[0x2B44];
    unsigned char b2B64;        /* +0x2B64 */
};

class GameObj3DF80 {
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
/* BrSub100325B0: prototype in br_funcs.h */
/* BrSub100355F0: prototype in br_funcs.h */
/* BrSub100356B0: prototype in br_funcs.h */
}

int Leave(GameObj3DF80 *pObj)
{
    int v;

    if (g_guardB != 0) {
        pObj->p2AE8->s6(0);
        BrExt_10038F30(0);
    }
    pObj->p2AE8->s7();

    if ((*(CurPhase * *)&g_brPAA29B8) != 0)
        delete (*(CurPhase * *)&g_brPAA29B8);

    g_brPAA2950 = 0;
    (*(CurPhase * *)&g_brPAA29B8) = (CurPhase *)((BrOptObj *)((*(CurPhase * *)&g_294C)));
    BrExt_1003BF60();

    v = DAT_10ac5bd4;
    if (v == 0 || v == 1) {
        if (g_guardB == 0) {
            FUN_100356b0();
            v = DAT_10ac5bd4;
        }
    }
    if (v == 2 || v == 3) {
        D30Obj *d = (*(D30Obj * *)&g_brPAA29D8);
        if (d != 0) {
            d->f1C = d->f1C & ~0x10u;
            (*(D30Obj * *)&g_brPAA29D8)->b2B64 = 0;
        }
    }
    return 0;
}
