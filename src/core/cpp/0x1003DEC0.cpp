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
    virtual ~CurPhase();
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
extern int       DAT_10ac4090;
extern CurPhase *DAT_10ac5c5c;
extern CurPhase *DAT_10ac5ca0;
extern int       DAT_10ac5ca8;
extern int       DAT_10ac5bd4;
extern int       DAT_10ac5bf0;
extern D30Obj   *DAT_10ac5d30;
void BrSub100325B0(int);
void BrSub100355F0(void);
void BrSub100356B0(void);
}

int Leave(GameObj3DEC0 *pObj)
{
    int v;

    if (DAT_10ac4090 != 0) {
        pObj->p2AE8->s6(0);
        BrSub100325B0(0);
    }
    pObj->p2AE8->s7();

    if (DAT_10ac5c5c != 0)
        delete DAT_10ac5c5c;

    DAT_10ac5ca8 = 0;
    DAT_10ac5c5c = DAT_10ac5ca0;
    if (DAT_10ac5d30 != 0)
        DAT_10ac5d30->f1C = DAT_10ac5d30->f1C & ~0x10u;

    BrSub100355F0();
    DAT_10ac5bf0 = 1;

    v = DAT_10ac5bd4;
    if (v == 0 || v == 1) {
        if (DAT_10ac4090 == 0) {
            BrSub100356B0();
            v = DAT_10ac5bd4;
        }
    }
    if (v == 2 || v == 3) {
        if (DAT_10ac5d30 != 0)
            DAT_10ac5d30->f1C = DAT_10ac5d30->f1C & ~0x10u;
    }
    return 0;
}
