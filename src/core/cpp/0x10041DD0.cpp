/* @implements 0x10041DD0 glide BrPhaseRun_100489A0
 * @cpp_kind method
 * @cpp_symbol ?Run@Phase32R@@QAEHXZ
 *
 * 249 B thiscall, no stack args. Two identical shutdown blocks (helper,
 * config save on the global object, slot-6 vcall) guarding f68; the
 * current-object swap around a thiscall step; DIK poll; a same-object
 * flag; then the gate loop over two parallel member arrays (+0x14
 * pointers, +0x6C flags) with per-entry slot-1 vcalls.
 */
class SubObj {
public:
    virtual void s0();
    virtual int  s1();          /* +0x04 */
};

class StepObj {
public:
    void Step();                /* 0x100592D0, non-virtual thiscall */
};

class CfgObj {
public:
    int Save(const char *psz);  /* 0x100634B0, thiscall + 1 stack arg */
};

class Phase32R {
public:
    virtual void v0();
    virtual void v1();          /* +0x04 */
    virtual void v2();          /* +0x08 */
    virtual void v3();
    virtual void v4();
    virtual void v5();
    virtual void v6(int);      /* +0x18 */
    char pad04[0xC];            /* +0x04 */
    unsigned short f10;         /* +0x10 */
    unsigned short f12;         /* +0x12 */
    SubObj *a14[20];            /* +0x14 */
    SubObj *f64;                /* +0x64 */
    int f68;                    /* +0x68 */
    int a6C[20];                /* +0x6C */
    int Run();
};

class Holder;

extern "C" {
extern Holder *DAT_10ac5c58;
extern Holder *DAT_10ac5c5c;
extern Holder *DAT_10ac5c60;
extern int     DAT_10ac5bc0;
extern char    g_BrCfgPath[];   /* 0x10B72F48 */
void BrSub10037920(void);
int  BrDikPollAndEdge(void);
}
extern CfgObj g_BrCfgObj;       /* 0x10B71290 */

int Phase32R::Run()
{
    int i;

    if (f68 == 0) {
        BrSub10037920();
        g_BrCfgObj.Save(g_BrCfgPath);
        f12 = 0;
        v6(0);
        return 0;
    }

    v1();

    {
        Holder *saved = DAT_10ac5c5c;
        DAT_10ac5c5c = DAT_10ac5c60;
        ((StepObj *)DAT_10ac5c58)->Step();
        DAT_10ac5c5c = saved;
    }
    BrDikPollAndEdge();
    DAT_10ac5bc0 = (DAT_10ac5c5c == DAT_10ac5c60);

    f12 = 0;
    for (i = 0; i < f10; ++i) {
        f64 = a14[i];
        if (f64 == 0)
            return 0;
        f12 = (unsigned short)i;
        if (a6C[i] != 0) {
            if (f64->s1() == 0)
                return 0;
        }
    }

    v2();
    if (f68 == 0) {
        BrSub10037920();
        g_BrCfgObj.Save(g_BrCfgPath);
        f12 = 0;
        v6(0);
        return 0;
    }
    return 1;
}
