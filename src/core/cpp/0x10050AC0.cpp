/* WHAT IT DOES: build one menu page: creates the page container, adds every
 * control on it in turn, and reports failure if any of them could not be
 * made. One of a family of page builders, each laying out its own screen. */
/* @implements 0x10050ac0 glide BrOptFn10057C10
 * @cpp_kind free
 * @cpp_symbol ?BrOptFn10057C10@@YAHPAVGameUi@@@Z
 *
 * 2701 B cdecl EH-frame menu-page builder. Emitted by
 * tools/gen_menubuilder.py from the Ghidra draft; the class
 * layouts and the three family levers come from the hand-solved
 * 0x100425E0 / 0x10048160 (char bool after the slot store, raw
 * float pushes for simple lvalues, w14-then-w344 tails).
 */
class GameUi;
class BrCtl;
class Sel3838;

class Sel3838 {
public:
    virtual void s0();
    virtual void s1();
    virtual void s2();
    virtual void s3();
    virtual void s4(char *psz, int, int, void *, int);   /* +0x10 add item */
    virtual void s5(int, void *, int, int, int);         /* +0x14 configure */

    int (*pfn04)(void);             /* +0x04 */
    char pad08[0x14 - 8];
    int   f14;                      /* +0x14 */
};

typedef char chk_sel3838[sizeof(Sel3838) == 0x18 ? 1 : -1];

class SlotTable050AC0 {
public:
    virtual void s0();
    virtual void s1(char *pszPattern);   /* +0x04 */
    char aRecs[26000];                   /* +0x04, 0x104-byte records */
};

class Root050AC0 {
public:
    char            pad000[0xC0];
    SlotTable050AC0 *pTable;         /* +0xC0 */
    SlotTable050AC0 *pTableC4;       /* +0xC4 */
};


class Page050AC0 {
public:
    virtual void v0();
    int (*pfnA)(void);              /* +0x04 */
    int (*pfnB)(void);              /* +0x08 */
    int (*pfnC)(void);              /* +0x0C */
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
    Page050AC0();
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
    int (*pfn10)(BrCtl *);          /* +0x10 */
    int (*pfn14)(BrCtl *);          /* +0x14 */
    int (*pfn18)(BrCtl *);          /* +0x18 */
    char pad1C[0x2AB4 - 0x1C];      /* +0x1C */
    short w2AB4;                    /* +0x2AB4 */
    short w2AB6[0x19];              /* +0x2AB6 */
    char pad2AE8[0x3838 - 0x2AE8];  /* +0x2AE8 */
    Sel3838 m3838;                  /* +0x3838 */
    char pad3850[0x1E1F4 - 0x3850]; /* +0x3850 */
    int f1E1F4;                     /* +0x1E1F4 */
    char pad1E1F8[0x1E20C - 0x1E1F8];
    unsigned short w1E20C;          /* +0x1E20C */
    char pad1E20E[6];               /* +0x1E20E */
    BrCtl();
};

class GameUi {
public:
    char pad[0x10];
    unsigned short w10;             /* +0x10 */
    short w12;                      /* +0x12 */
    Page050AC0 *a14[22];              /* +0x14 */
    int a6C[1];                     /* +0x6C */
};

typedef char chk_page050AC0[sizeof(Page050AC0) == 0x348 ? 1 : -1];
typedef char chk_ctl050AC0[sizeof(BrCtl) == 0x1E214 ? 1 : -1];
typedef char chk_sel_off[(unsigned)&((BrCtl *)0)->m3838 == 0x3838 ? 1 : -1];
typedef char chk_f1e1f4[(unsigned)&((BrCtl *)0)->f1E1F4 == 0x1E1F4 ? 1 : -1];
typedef char chk_w1e20c050AC0[(unsigned)&((BrCtl *)0)->w1E20C == 0x1E20C ? 1 : -1];
typedef char chk_a6c050AC0[(unsigned)&((GameUi *)0)->a6C == 0x6C ? 1 : -1];
typedef char chk_tbl050AC0[(unsigned)&((Root050AC0 *)0)->pTable == 0xC0 ? 1 : -1];

typedef int (*CtlFn)(BrCtl *);

extern "C" {
int DAT_10ac4090;
int g_brAA2884;
int DAT_10226a48;
BrCtl *DAT_10ac5d10;
int BrMenuCap0730();
int BrMenuCap07E0();
int BrMenuText08D0();
int BrOpt37D0();
int BrOptCycleAA2A00();
int BrOptCycleAA2A18();
int BrOptCycleBD3E0();
int BrOptOpen2954();
int BrPhaseTick_100474B0();
int BrSub10047360();
int BrUiHook87_1003F5E0();
int BrUiHook87_1003F680();
extern char  DAT_100aabe8;
extern char  DAT_100aabf8;
extern char  DAT_100aac48;
extern char  DAT_100aac58;
extern char  DAT_100aaca8;
extern char  DAT_100aca4c;
extern char  DAT_100acad8;
int FUN_10038f40();
int FUN_100393c0();
int FUN_1003c430();
extern float DAT_10077648;
extern float DAT_1007764c;
extern float DAT_10077650;
extern float DAT_10077658;
extern float DAT_1007765c;
char *BrStrGet(int);
void FUN_100378c0(int);
}

int BrOptFn10057C10(GameUi *parent)
{
    Page050AC0 *cont;
    BrCtl     *p;
    char       bad;

    parent->w12 = 0;
    parent->a6C[parent->w10] = 1;
    cont = new Page050AC0;
    parent->a14[parent->w10] = cont;
    bad = (cont == 0);
    if (bad)
        FUN_100378c0(4);
    parent->w10 += 1;
    cont->f340 = parent;
    cont->f10 = 0;
    cont->f338 = 195.0f;
    cont->f33C = 130.0f;
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
    p->s34(BrStrGet(100), 1, 1, &DAT_100aaca8);
    cont->w14 += 1;
    if (DAT_10226a48 == 2) {
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, cont->f338, cont->f33C, 0x102001, 2, 5, 1, -1);
    p->pfn0C = (CtlFn)BrPhaseTick_100474B0;
    p->pfn08 = (CtlFn)FUN_1003c430;
    p->w1E20C = 3;
    p->s34(BrStrGet(0x1b), 1, 1, &DAT_100aabe8);
    cont->w14 += 1;
    cont->w344 += 1;

    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, cont->f338, cont->f33C - DAT_10077648, 0x102001, 2, 5, 1, -1);
    p->pfn0C = (CtlFn)BrSub10047360;
    p->pfn08 = (CtlFn)BrOptCycleAA2A00;
    p->w1E20C = 3;
    p->s34(BrStrGet(0x1c), 1, 1, &DAT_100aabe8);
    cont->w14 += 1;
    cont->w344 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, cont->f338, cont->f33C - DAT_1007764c, 0x102001, 2, 5, 1, -1);
    p->pfn0C = (CtlFn)BrSub10047360;
    p->pfn08 = (CtlFn)BrOptCycleBD3E0;
    p->w1E20C = 3;
    p->s34(BrStrGet(0x1d), 1, 1, &DAT_100aabe8);
    cont->w14 += 1;
    cont->w344 += 1;

    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, cont->f338, cont->f33C - DAT_10077650, 0x102001, 2, 5, 1, -1);
    p->pfn0C = (CtlFn)BrSub10047360;
    p->pfn08 = (CtlFn)BrOptCycleAA2A18;
    p->w1E20C = 3;
    p->s34(BrStrGet(0x65), 1, 1, &DAT_100aabe8);
    cont->w14 += 1;
    cont->w344 += 1;
    }

    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, cont->f338, cont->f33C - DAT_10077658, 0x102001, 2, 5, 1, -1);
    p->pfn0C = (CtlFn)BrSub10047360;
    p->pfn08 = (CtlFn)BrOptOpen2954;
    p->pfn18 = (CtlFn)BrOpt37D0;
    p->w1E20C = 3;
    if (g_brAA2884 != 0)
        p->s34(BrStrGet(0x66), 1, 1, &DAT_100aabe8);
    else
        p->s34(BrStrGet(0x1E), 1, 1, &DAT_100aabe8);
    cont->w14 += 1;
    cont->w344 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, cont->f338, cont->f33C - DAT_1007765c, 0x102001, 2, 5, 1, -1);
    p->pfn0C = (CtlFn)BrSub10047360;
    p->w1E20C = 3;
    if (DAT_10ac4090 != 0)
        p->s34(BrStrGet(0x67), 1, 1, &DAT_100aabe8);
    else
        p->s34(BrStrGet(0x0C), 1, 1, &DAT_100aabe8);
    DAT_10ac5d10 = p;
    cont->w14 += 1;
    cont->w344 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 73.0f, 212.0f, 1, 2, 5, 1, 0x16);
    p->pfn04 = (CtlFn)BrMenuCap07E0;
    p->w2AB6[0] = (short)(cont->w14 + 1);
    p->w2AB4 += 1;
    cont->w14 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, cont->f338, 275.0f, 0x101001, 2, 5, 1, -1);
    p->pfn04 = (CtlFn)FUN_100393c0;
    p->w1E20C = 3;
    p->s34(&DAT_100acad8, 1, 1, &DAT_100aac58);
    cont->w14 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 325.0f, 72.0f, 1, 2, 5, 1, 0x11);
    p->pfn04 = (CtlFn)BrMenuCap0730;
    p->w2AB6[0] = (short)(cont->w14 + 1);
    p->w2AB4 += 1;
    cont->w14 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, cont->f338, 152.0f, 0x101001, 2, 5, 1, -1);
    p->pfn04 = (CtlFn)FUN_10038f40;
    p->w1E20C = 3;
    p->s34(&DAT_100acad8, 1, 1, &DAT_100aac48);
    cont->w14 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 106.0f, 68.0f, 0x5001, 2, 5, 1, -1);
    p->pfn04 = (CtlFn)BrMenuText08D0;
    p->w1E20C = 5;
    p->s34(&DAT_100aca4c, 1, 3, &DAT_100aabf8);
    cont->w14 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 106.0f, 115.0f, 0x100001, 2, 5, 1, -1);
    p->w1E20C = 3;
    p->s34(BrStrGet(0x1d), 1, 1, &DAT_100aabf8);
    cont->w14 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 437.0f, 141.0f, 1, 2, 5, 1, 0x56);
    p->pfn04 = (CtlFn)BrUiHook87_1003F5E0;
    cont->w14 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 476.0f, 224.0f, 1, 2, 5, 1, -1);
    p->pfn04 = (CtlFn)BrUiHook87_1003F680;
    cont->w14 += 1;

    return 1;
}
