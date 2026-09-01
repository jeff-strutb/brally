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
extern int       DAT_10ac4090;
extern CurPhase *DAT_10ac5c5c;
extern CurPhase *DAT_10ac5ca4;
extern int       DAT_10ac5ca8;
extern int       DAT_10ac5bd4;
extern D30Obj   *DAT_10ac5d30;
void BrSub100325B0(int);
void BrSub100355F0(void);
void BrSub100356B0(void);
}

int Leave(GameObj3DF80 *pObj)
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
    DAT_10ac5c5c = DAT_10ac5ca4;
    BrSub100355F0();

    v = DAT_10ac5bd4;
    if (v == 0 || v == 1) {
        if (DAT_10ac4090 == 0) {
            BrSub100356B0();
            v = DAT_10ac5bd4;
        }
    }
    if (v == 2 || v == 3) {
        D30Obj *d = DAT_10ac5d30;
        if (d != 0) {
            d->f1C = d->f1C & ~0x10u;
            DAT_10ac5d30->b2B64 = 0;
        }
    }
    return 0;
}
