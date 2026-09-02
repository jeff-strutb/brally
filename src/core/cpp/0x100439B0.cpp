/* @implements 0x100439B0 glide BrPhaseEnterPlaceholder_1004A580
 * @cpp_kind method
 * @cpp_symbol ?BrPhaseEnterPlaceholder_1004A580@@YAHPAVGameUi@@@Z
 *
 * 3746 B cdecl EH-frame race-options menu builder — 0x100425E0 family
 * (same layouts and levers). Extras here: the cycle pages capture
 * DAT_10ac5d0c, then the photo trio — fx/fy floats filled from int
 * globals (fild), a page conditional on g_br0AA010, per-page ftol
 * coordinate stores into f50..f5C, xi shared across the two
 * unconditional pages in ebx, fy walked down by DAT_10077664, and the
 * caption code stored in w2A42. Per-block statement order follows the
 * original (it differs block to block).
 */
class GameUi;
class BrCtl;

class Phase32F {
public:
    virtual void v0();
    int (*pfnA)(void);
    int (*pfnB)(void);
    int (*pfnC)(void);
    int f10;                        /* +0x10 */
    unsigned short w14;             /* +0x14 */
    unsigned short w16;             /* +0x16 */
    BrCtl *a18[199];                /* +0x18 */
    BrCtl *f334;                    /* +0x334 */
    float f338;                     /* +0x338 */
    float f33C;                     /* +0x33C */
    GameUi *f340;                   /* +0x340 */
    short w344;                     /* +0x344 */
    short w346;                     /* +0x346 */
    Phase32F();
};

class BrCtl {
public:
    virtual void s00();
    virtual void s04();
    virtual void s08();
    virtual void s0C();
    virtual void s10();
    virtual void s14();
    virtual void s18();
    virtual void s1C();
    virtual void s20();
    virtual void s24();
    virtual void s28();
    virtual void s2C();
    virtual void s30();
    virtual void s34(char *, int, int, char *);                          /* +0x34 */
    virtual void s38(GameUi *, float, float, unsigned int, int, int, int, int); /* +0x38 */
    virtual void s3C();
    int (*pfn04)(BrCtl *);          /* +0x04 */
    int (*pfn08)(BrCtl *);          /* +0x08 */
    int (*pfn0C)(BrCtl *);          /* +0x0C */
    char pad10[0x40];               /* +0x10 */
    int f50;                        /* +0x50 */
    int f54;                        /* +0x54 */
    int f58;                        /* +0x58 */
    int f5C;                        /* +0x5C */
    char pad60[0x2908];             /* +0x60 */
    int f2968;                      /* +0x2968 */
    char pad296C[0xD6];             /* +0x296C */
    short w2A42;                    /* +0x2A42 */
    char pad2A44[0x70];             /* +0x2A44 */
    short w2AB4;                    /* +0x2AB4 */
    short w2AB6[0x19];              /* +0x2AB6 */
    char pad2AE8[0x1B724];          /* +0x2AE8 */
    unsigned short w1E20C;          /* +0x1E20C */
    char pad1E20E[6];               /* +0x1E20E */
    BrCtl();
};

class GameUi {
public:
    char pad[0x10];
    unsigned short w10;             /* +0x10 */
    short w12;                      /* +0x12 */
    Phase32F *a14[22];              /* +0x14 */
    int a6C[1];                     /* +0x6C */
};

typedef char chk_cont[sizeof(Phase32F) == 0x348 ? 1 : -1];
typedef char chk_ctl[sizeof(BrCtl) == 0x1E214 ? 1 : -1];
typedef char chk_2968[(unsigned)&((BrCtl *)0)->f2968 == 0x2968 ? 1 : -1];
typedef char chk_2a42[(unsigned)&((BrCtl *)0)->w2A42 == 0x2A42 ? 1 : -1];

extern "C" {
extern char DAT_100aaca8;
extern char DAT_100aabe8;
extern char DAT_100acad8;
extern char DAT_100aac08;
extern char DAT_100aac18;
extern char DAT_100aac28;
extern char DAT_100aac38;
extern int DAT_100aabc8;
extern int DAT_100aabcc;
extern BrCtl *DAT_10ac5d0c;
extern int g_br0AA010;
extern float DAT_10077648;
extern float DAT_1007764c;
extern float DAT_10077650;
extern float DAT_10077654;
extern float DAT_1007765c;
extern float DAT_10077660;
extern float DAT_10077664;
char *BrStrGet(int);
void FUN_100378c0(int);
int BrSub10047360();
int BrOptCycleTrack();
int BrOptCycleAC64C();
int BrOptCycleAC650();
int BrOptCycleAC65C();
int BrOptCycleAA2A08();
int BrOpt3760();
int FUN_1003f980();
int BrPhaseActivate_1003ED70();
int BrOpt3FA0();
int BrHook_100458C0();
int BrUiFn1003E920();
int BrRaceIconLookup();
int BrUiText1003F760();
int BrUiText1003F7F0();
int BrUiText1003F990();
int BrMenuCap0870();
int BrMenuCap0890();
int BrMenuCap08B0();
int FUN_10038da0();
}

typedef int (*CtlFn)(BrCtl *);

int BrPhaseEnterPlaceholder_1004A580(GameUi *parent)
{
    Phase32F *cont;
    BrCtl *p;
    char bad;
    float fx, fy;
    int xi, yi;

    parent->w12 = 0;
    parent->a6C[parent->w10] = 1;
    cont = new Phase32F;
    parent->a14[parent->w10] = cont;
    bad = (cont == 0);
    if (bad)
        FUN_100378c0(4);
    parent->w10 += 1;
    cont->f340 = parent;
    cont->f10 = 0;
    cont->f338 = 195.0f;
    cont->f33C = 111.0f;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 0, 0, 9, 2, 5, 0, 0);
    cont->w14 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, cont->f338, 10.0f, 0x100009, 2, 5, 1, -1);
    p->w1E20C = 3;
    p->s34(BrStrGet(0x13), 1, 1, &DAT_100aaca8);
    cont->w14 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, cont->f338, cont->f33C, 0x102001, 2, 5, 1, -1);
    p->pfn0C = (CtlFn)BrSub10047360;
    p->pfn08 = (CtlFn)BrOptCycleTrack;
    p->w1E20C = 3;
    p->s34(BrStrGet(0x14), 1, 1, &DAT_100aabe8);
    cont->w14 += 1;
    cont->w344 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, cont->f338, cont->f33C - DAT_10077648, 0x102001, 2, 5, 1, -1);
    p->pfn0C = (CtlFn)BrSub10047360;
    p->pfn08 = (CtlFn)BrOptCycleAC64C;
    p->w1E20C = 3;
    p->s34(BrStrGet(0x15), 1, 1, &DAT_100aabe8);
    cont->w14 += 1;
    cont->w344 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, cont->f338, cont->f33C - DAT_1007764c, 0x102001, 2, 5, 1, -1);
    p->pfn0C = (CtlFn)BrSub10047360;
    p->pfn08 = (CtlFn)BrOptCycleAC650;
    p->w1E20C = 3;
    p->s34(BrStrGet(0x16), 1, 1, &DAT_100aabe8);
    cont->w14 += 1;
    cont->w344 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, cont->f338, cont->f33C - DAT_10077650, 0x102001, 2, 5, 1, -1);
    p->pfn0C = (CtlFn)BrSub10047360;
    p->pfn08 = (CtlFn)BrOptCycleAC65C;
    p->w1E20C = 3;
    p->s34(BrStrGet(0x17), 1, 1, &DAT_100aabe8);
    cont->w14 += 1;
    cont->w344 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, cont->f338, cont->f33C - DAT_10077654, 0x102001, 2, 5, 1, -1);
    p->pfn0C = (CtlFn)BrSub10047360;
    p->pfn08 = (CtlFn)BrOptCycleAA2A08;
    p->w1E20C = 3;
    p->s34(BrStrGet(0x18), 1, 1, &DAT_100aabe8);
    cont->w14 += 1;
    cont->w344 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, cont->f338, cont->f33C - DAT_1007765c, 0x102001, 2, 5, 1, -1);
    p->pfn0C = (CtlFn)BrSub10047360;
    p->pfn08 = (CtlFn)BrOpt3760;
    p->w1E20C = 3;
    p->s34(BrStrGet(0x19), 1, 1, &DAT_100aabe8);
    cont->w14 += 1;
    cont->w344 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, cont->f338, cont->f33C - DAT_10077660, 0x102001, 2, 5, 1, -1);
    p->pfn0C = (CtlFn)BrSub10047360;
    p->pfn08 = (CtlFn)FUN_1003f980;
    p->w1E20C = 3;
    p->s34(BrStrGet(0xc), 1, 1, &DAT_100aabe8);
    DAT_10ac5d0c = p;
    cont->w14 += 1;
    cont->w344 += 1;
    fx = (float)DAT_100aabc8;
    fy = (float)DAT_100aabcc;
    if (g_br0AA010 == 0) {
        p = new BrCtl;
        cont->a18[cont->w14] = p;
        bad = (p == 0);
        if (bad)
                FUN_100378c0(4);
        p->s38(parent, fx, fy, 0x402001, 2, 5, 1, 0x78);
        p->pfn0C = (CtlFn)BrSub10047360;
        p->pfn08 = (CtlFn)BrPhaseActivate_1003ED70;
        yi = (int)fy;
        p->f54 = yi;
        xi = (int)fx;
        fy = fy - DAT_10077664;
        p->f50 = xi;
        p->f58 = xi + 0x7f;
        p->f5C = yi + 0x21;
        p->f2968 = 0;
        p->w2A42 = 0x79;
        cont->w14 += 1;
    }
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, fx, fy, 0x402001, 2, 5, 1, 0x52);
    p->pfn0C = (CtlFn)BrSub10047360;
    p->pfn08 = (CtlFn)BrOpt3FA0;
    yi = (int)fy;
    p->f54 = yi;
    xi = (int)fx;
    fy = fy - DAT_10077664;
    p->f2968 = 0;
    p->f50 = xi;
    p->w2A42 = 0x53;
    p->f58 = xi + 0x7f;
    p->f5C = yi + 0x21;
    cont->w14 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, fx, fy, 0x402001, 2, 5, 1, 0x54);
    p->pfn0C = (CtlFn)BrSub10047360;
    p->pfn08 = (CtlFn)BrHook_100458C0;
    yi = (int)fy;
    p->f54 = yi;
    p->f50 = xi;
    p->f58 = xi + 0x7f;
    p->f5C = yi + 0x21;
    p->f2968 = 0;
    p->w2A42 = 0x55;
    cont->w14 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 61.0f, 244.0f, 9, 2, 5, 1, 0x36);
    cont->w14 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 73.0f, 214.0f, 1, 2, 5, 1, 0x35);
    p->pfn04 = (CtlFn)BrUiFn1003E920;
    cont->w14 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 339.0f, 70.0f, 1, 2, 5, 1, 0xb);
    p->pfn04 = (CtlFn)BrRaceIconLookup;
    p->w2AB6[0] = (short)(cont->w14 + 1);
    p->w2AB4 += 1;
    cont->w14 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, cont->f338, 137.0f, 0x101001, 2, 5, 1, -1);
    p->pfn04 = (CtlFn)BrUiText1003F760;
    p->w1E20C = 3;
    p->s34(&DAT_100acad8, 1, 1, &DAT_100aac08);
    cont->w14 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 108.0f, 66.0f, 1, 2, 5, 1, 0x19);
    p->pfn04 = (CtlFn)BrMenuCap0890;
    p->w2AB6[0] = (short)(cont->w14 + 1);
    p->w2AB4 += 1;
    cont->w14 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, cont->f338, 123.0f, 0x101001, 2, 5, 1, -1);
    p->pfn04 = (CtlFn)BrUiText1003F7F0;
    p->w1E20C = 3;
    p->s34(&DAT_100acad8, 1, 1, &DAT_100aac38);
    cont->w14 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 476.0f, 129.0f, 1, 2, 5, 1, 0xe);
    p->pfn04 = (CtlFn)BrMenuCap08B0;
    p->w2AB6[0] = (short)(cont->w14 + 1);
    p->w2AB4 += 1;
    cont->w14 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, cont->f338, 181.0f, 0x101001, 2, 5, 1, -1);
    p->pfn04 = (CtlFn)BrUiText1003F990;
    p->w1E20C = 3;
    p->s34(&DAT_100acad8, 1, 1, &DAT_100aac28);
    cont->w14 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 505.0f, 200.0f, 1, 2, 5, 1, 0xc);
    p->pfn04 = (CtlFn)BrMenuCap0870;
    p->w2AB6[0] = (short)(cont->w14 + 1);
    p->w2AB4 += 1;
    cont->w14 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, cont->f338, 262.0f, 0x101001, 2, 5, 1, -1);
    p->pfn04 = (CtlFn)FUN_10038da0;
    p->w1E20C = 3;
    p->s34(&DAT_100acad8, 1, 1, &DAT_100aac18);
    cont->w14 += 1;
    return 1;
}
