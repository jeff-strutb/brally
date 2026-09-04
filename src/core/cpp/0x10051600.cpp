/* WHAT IT DOES: build one menu page: creates the page container, adds every
 * control on it in turn, and reports failure if any of them could not be
 * made. One of a family of page builders, each laying out its own screen,
 * and it saves the settings and runs a teardown step before laying out --
 * this page is entered on the way out of the options screens. */
/* @implements 0x10051600 glide FUN_10051600
 * @cpp_kind free
 * @cpp_symbol ?FUN_10051600@@YAHPAVGameUi@@@Z
 *
 * 4109 B cdecl EH-frame menu-page builder. Emitted by
 * tools/gen_menubuilder.py from the Ghidra draft; the class
 * layouts and the three family levers come from the hand-solved
 * 0x100425E0 / 0x10048160 (char bool after the slot store, raw
 * float pushes for simple lvalues, w14-then-w344 tails).
 */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#include <string.h>
#endif

class GameUi;
class BrCtl;
class Sel3838;
class Item2B5C;

class Item2B5C {
public:
    virtual void  s0();
    virtual void  s1();         /* +0x04 relayout */
    virtual void  s2();
    virtual void  s3();
    virtual void  s4();
    virtual void  s5();
    virtual void  s6();
    virtual void  s7();
    virtual void  s8();
    virtual void  s9();
    virtual float s10();
    virtual void  s11();

    int   f004;
    char  b008;
    char  szName[0x401];        /* +0x009 */
    short w40A;
    short w40C;
    short pad40E;
    int   f410;
    int   f414;
    int   f418;
    short w41C;                 /* +0x41C */
    short pad41E;
    int   f420;
    int   a424[4];              /* +0x424 */
    int   f434;
};

typedef char chk_item2B5C[sizeof(Item2B5C) == 0x438 ? 1 : -1];

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

class SlotTable051600 {
public:
    virtual void s0();
    virtual void s1(char *pszPattern);   /* +0x04 */
    char aRecs[26000];                   /* +0x04, 0x104-byte records */
};

class Root051600 {
public:
    char            pad000[0xC0];
    SlotTable051600 *pTable;         /* +0xC0 */
    SlotTable051600 *pTableC4;       /* +0xC4 */
};


class Page051600 {
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
    Page051600();
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
    char pad1C[0x50 - 0x1C];        /* +0x1C */
    int  f050;                      /* +0x050 */
    int  f054;
    int  f058;
    int  f05C;
    char pad60[0x2AB4 - 0x60];      /* +0x060 */
    short w2AB4;                    /* +0x2AB4 */
    short w2AB6[0x19];              /* +0x2AB6 */
    char pad2AE8[0x2B5C - 0x2AE8];  /* +0x2AE8 */
    Item2B5C m2B5C;                 /* +0x2B5C -- the 0x438 item record */
    char pad2F94[0x3838 - 0x2F94];  /* +0x2F94 */
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
    Page051600 *a14[22];              /* +0x14 */
    int a6C[1];                     /* +0x6C */
};

typedef char chk_page051600[sizeof(Page051600) == 0x348 ? 1 : -1];
typedef char chk_ctl051600[sizeof(BrCtl) == 0x1E214 ? 1 : -1];
typedef char chk_sel_off[(unsigned)&((BrCtl *)0)->m3838 == 0x3838 ? 1 : -1];
typedef char chk_f1e1f4[(unsigned)&((BrCtl *)0)->f1E1F4 == 0x1E1F4 ? 1 : -1];
typedef char chk_w1e20c051600[(unsigned)&((BrCtl *)0)->w1E20C == 0x1E20C ? 1 : -1];
typedef char chk_a6c051600[(unsigned)&((GameUi *)0)->a6C == 0x6C ? 1 : -1];
typedef char chk_tbl051600[(unsigned)&((Root051600 *)0)->pTable == 0xC0 ? 1 : -1];

typedef int (*CtlFn)(BrCtl *);

extern "C" {
int g_br10226A48;
int g_brAA2884;
int DAT_10ac5bb4;
int g_br0AA010;
int g_brAA28D8;
BrCtl *DAT_10ac5d38;
BrCtl *g_brPAA29E4;
int BrMenuCap0870();
int BrMenuCap0890();
int BrMenuCap08B0();
int BrOpt37B0();
int BrOpt3810();
int BrOpt3A00();
int BrOptCycleAA2A08();
int BrOptCycleAC64C();
int BrOptCycleAC650();
int BrOptCycleAC65C();
int BrOptCycleTrack();
int BrOptSave();
int BrRaceIconLookup();
int BrStubTrue();
int BrSub1003E510();
int BrSub10046400();
int BrSub10047360();
int BrUiFn1003E920();
int BrUiFn1003F110();
int BrUiFn1003F170();
int BrUiPoll1003EBC0();
int BrUiPoll1003EBE0();
int BrUiText1003F760();
int BrUiText1003F7F0();
int BrUiText1003F990();
extern char  DAT_100aab98;
extern char  DAT_100aaba8;
extern char  DAT_100aabd8;
extern char  DAT_100aabe8;
extern char  DAT_100aac08;
extern char  DAT_100aac18;
extern char  DAT_100aac28;
extern char  DAT_100aac38;
extern char  DAT_100acad8;
extern char  DAT_10ac40a8;
int FUN_10038da0();
extern char  g_aBr39B720;
extern float DAT_10077648;
extern float DAT_1007764c;
extern float DAT_10077650;
extern float DAT_10077654;
extern float DAT_10077658;
extern float DAT_1007765c;
extern float DAT_10077660;
char *BrStrGet(int);
void FUN_100378c0(int);
}

int FUN_10051600(GameUi *parent)
{
    Page051600 *cont;
    BrCtl     *p;
    char       bad;

    parent->w12 = 0;
    g_br0AA010 = 6;
    BrOptSave();
    BrSub1003E510();
    parent->a6C[parent->w10] = 1;
    cont = new Page051600;
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
    p->s38(parent, 0, 320.0f, 9, 2, 5, 0, 0x5F);
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
    if (g_br10226A48 == 2) {
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, cont->f338, cont->f33C - DAT_10077658, 0x102001, 2, 5, 1, -1);
    p->pfn0C = (CtlFn)BrSub10047360;
    p->pfn08 = (CtlFn)BrOpt37B0;
    p->w1E20C = 3;
    p->s34(BrStrGet(0x68), 1, 1, &DAT_100aabe8);
    cont->w14 += 1;
    cont->w344 += 1;
    }

    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, cont->f338, cont->f33C - DAT_1007765c, 0x102001, 2, 5, 1, -1);
    p->pfn0C = (CtlFn)BrSub10047360;
    p->pfn08 = (CtlFn)BrOpt3A00;
    p->pfn18 = (CtlFn)BrOpt3810;
    p->w1E20C = 3;
    if (g_brAA2884 != 0)
        p->s34(BrStrGet(0x19), 1, 1, &DAT_100aabe8);
    else
        p->s34(BrStrGet(0x69), 1, 1, &DAT_100aabe8);
    cont->w14 += 1;
    cont->w344 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, cont->f338, cont->f33C - DAT_10077660, 0x102001, 2, 5, 1, -1);
    p->pfn0C = (CtlFn)BrSub10047360;
    p->pfn08 = (CtlFn)BrSub10046400;
    p->w1E20C = 3;
    p->s34(BrStrGet(0xc), 1, 1, &DAT_100aabe8);
    cont->w14 += 1;
    cont->w344 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 32.0f, 330.0f, 0x3001, 2, 5, 1, -1);
    p->pfn04 = (CtlFn)BrUiPoll1003EBC0;
    p->f1E1F4 = 1;
    p->m3838.s5(0x1840001, &DAT_100aaba8, 5, 0, -1);
    DAT_10ac5d38 = p;
    cont->w14 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 32.0f, 330.0f, 0x200001, 2, 5, 1, -1);
    p->pfn08 = (CtlFn)BrStubTrue;
    p->pfn04 = (CtlFn)BrUiFn1003F110;
    p->pfn10 = (CtlFn)BrUiFn1003F170;
    p->w1E20C = 3;
    p->s34(&g_aBr39B720, 0, 1, &DAT_100aabe8);
    p->f050 = 0x20;
    p->m2B5C.a424[0] = 0x20;
    p->f058 = 0x19f;
    p->m2B5C.a424[2] = 0x19f;
    p->f054 = 0x14a;
    p->m2B5C.a424[1] = 0x14a;
    p->f05C = 0x15a;
    p->m2B5C.a424[3] = 0x15a;
    p->m2B5C.w41C = (short)(p->m2B5C.a424[2] - p->m2B5C.a424[0]) - 0x10;
    g_brAA28D8 = 0;
    DAT_10ac5bb4 = 1;
    p->m2B5C.f420 = 1;
    cont->w14 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 484.0f, 2.0f, 0x1001, 2, 5, 1, -1);
    p->pfn04 = (CtlFn)BrUiPoll1003EBE0;
    p->f1E1F4 = 1;
    if (g_br10226A48 == 2)
        p->m3838.s5(0x2040001, &DAT_100aab98, 4, 0, -1);
    else
        p->m3838.s5(0x3040001, &DAT_100aab98, 4, 0, -1);
    g_brPAA29E4 = p;
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
    p->s38(parent, 339.0f, 70.0f, 1, 2, 5, 1, 0xB);
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
    p->s38(parent, 476.0f, 129.0f, 1, 2, 5, 1, 0xE);
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
    p->s38(parent, 505.0f, 200.0f, 1, 2, 5, 1, 0xC);
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
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 320.0f, 10.0f, 0x100001, 2, 5, 1, -1);
    p->w1E20C = 0x34;
    p->s34(&DAT_100acad8, 1, 4, &DAT_100aabd8);
    strcpy(p->m2B5C.szName, &DAT_10ac40a8);
    p->m2B5C.s1();
    cont->w14 += 1;

    return 1;
}
