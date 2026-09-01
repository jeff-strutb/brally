/* @implements 0x10041D10 glide BrPhaseTick_100488C0
 * @cpp_kind method
 * @cpp_symbol ?Tick@Phase32T@@QAEHXZ
 *
 * 160 B thiscall, no stack args. Early-out on flag 0x10; a 120-frame
 * cadence gate (idiv, memory inc); CD-track capture minus 2; then the
 * current-object swap around a slot-3 vcall, and a guarded slot-5
 * self-vcall. All state loose globals.
 */
struct BrTickPair { int i0; int i4; };

class PhaseInner {
public:
    virtual void s0();
    virtual void s1();
    virtual void s2();
    virtual void s3();          /* +0x0C */
    char pad[0x38];
    float f3C;                  /* +0x3C */
    float f40;                  /* +0x40 */
};

class PhaseHolder2 {
public:
    char pad[0x334];
    PhaseInner *p334;           /* +0x334 */
};

class PhaseHolder {
public:
    char pad[0x14];
    PhaseHolder2 *f14;          /* +0x14 */
};

class Phase32T {
public:
    virtual void v0();
    virtual void v1();
    virtual void v2();
    virtual void v3();
    virtual void v4();
    virtual void v5();          /* +0x14 */
    int f04;
    int f08;                    /* +0x08 */
    int Tick();
};

extern "C" {
extern int DAT_1007b074;
extern int DAT_10ac5da4;
extern int DAT_10ac5d8c;
extern int DAT_10ac5bcc;
extern PhaseHolder *DAT_10ac5c5c;
extern PhaseHolder *DAT_10ac5c60;
extern BrTickPair  *DAT_10ac61e0;
int BrCdTrackGet(void);
}

int Phase32T::Tick()
{
    if (f08 & 0x10)
        return 0;

    int go = 0;
    if (DAT_1007b074 == 2) {
        go = 1;
    } else {
        if (DAT_10ac5da4 % 0x78 == 0)
            go = 1;
        DAT_10ac5da4 = DAT_10ac5da4 + 1;
    }
    if (go)
        DAT_10ac5d8c = BrCdTrackGet() - 2;

    PhaseHolder *pSaved = DAT_10ac5c5c;
    PhaseHolder *pCur   = DAT_10ac5c60;
    DAT_10ac5c5c = pCur;
    PhaseInner *p = pCur->f14->p334;
    p->f3C = (float)DAT_10ac61e0->i0;
    p->f40 = (float)DAT_10ac61e0->i4;
    p->s3();
    DAT_10ac5c5c = pSaved;
    if (DAT_10ac5bcc == 0)
        v5();
    return 1;
}
