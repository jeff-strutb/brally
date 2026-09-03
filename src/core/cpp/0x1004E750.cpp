/* @implements 0x1004e750 glide BrOptFn100558A0
 * @cpp_kind free
 * @cpp_symbol ?BrOptFn100558A0@@YAHPAVGameUi@@@Z
 *
 * 2877 B cdecl EH-frame menu-page builder. Emitted by
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

class SlotTable04E750 {
public:
    virtual void s0();
    virtual void s1(char *pszPattern);   /* +0x04 */
    char aRecs[26000];                   /* +0x04, 0x104-byte records */
};

class Root04E750 {
public:
    char            pad000[0xC0];
    SlotTable04E750 *pTable;         /* +0xC0 */
    SlotTable04E750 *pTableC4;       /* +0xC4 */
};


class Page04E750 {
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
    Page04E750();
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
    Page04E750 *a14[22];              /* +0x14 */
    int a6C[1];                     /* +0x6C */
};

typedef char chk_page04E750[sizeof(Page04E750) == 0x348 ? 1 : -1];
typedef char chk_ctl04E750[sizeof(BrCtl) == 0x1E214 ? 1 : -1];
typedef char chk_sel_off[(unsigned)&((BrCtl *)0)->m3838 == 0x3838 ? 1 : -1];
typedef char chk_f1e1f4[(unsigned)&((BrCtl *)0)->f1E1F4 == 0x1E1F4 ? 1 : -1];
typedef char chk_w1e20c04E750[(unsigned)&((BrCtl *)0)->w1E20C == 0x1E20C ? 1 : -1];
typedef char chk_a6c04E750[(unsigned)&((GameUi *)0)->a6C == 0x6C ? 1 : -1];
typedef char chk_tbl04E750[(unsigned)&((Root04E750 *)0)->pTable == 0xC0 ? 1 : -1];

typedef int (*CtlFn)(BrCtl *);

extern "C" {
int g_br0AA010;
int Br85TextReadBack();
int BrHook_10044010();
int BrMenuCap0930();
int BrStubTrue();
int BrSub10047360();
int BrUiHook81_100463C0();
int BrUiHook85_1003F0B0();
int BrUiHook85_10042AC0();
int BrUiHook85_10044030();
int BrUiHook85_10044050();
int BrUiHook85_10044070();
int BrUiHook85_10044090();
int BrUiHook85_100440B0();
int BrUiText1003FC40();
extern char  DAT_100aabe8;
extern char  DAT_100aac48;
extern char  DAT_100aaca8;
extern char  DAT_100acad8;
extern char  DAT_10396f08;
extern float DAT_10077648;
extern float DAT_1007764c;
extern float DAT_1007765c;
char *BrStrGet(int);
void FUN_100378c0(int);
}

int BrOptFn100558A0(GameUi *parent)
{
    Page04E750 *cont;
    BrCtl     *p;
    char       bad;

    parent->w12 = 0;
    g_br0AA010 = 6;
    parent->a6C[parent->w10] = 1;
    cont = new Page04E750;
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
    p->s34(BrStrGet(0x54), 1, 1, &DAT_100aaca8);
    cont->w14 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, cont->f338, cont->f33C, 0x102001, 2, 5, 1, -1);
    p->pfn0C = (CtlFn)BrUiHook85_10044030;
    p->pfn08 = (CtlFn)BrHook_10044010;
    p->w1E20C = 3;
    p->s34(BrStrGet(0x55), 1, 1, &DAT_100aabe8);
    cont->w14 += 1;
    cont->w344 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, cont->f338, cont->f33C - DAT_10077648, 0x102001, 2, 5, 1, -1);
    p->pfn0C = (CtlFn)BrUiHook85_10044070;
    p->pfn08 = (CtlFn)BrUiHook85_10044050;
    p->w1E20C = 3;
    p->s34(BrStrGet(0x56), 1, 1, &DAT_100aabe8);
    cont->w14 += 1;
    cont->w344 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, cont->f338, cont->f33C - DAT_1007764c, 0x102001, 2, 5, 1, -1);
    p->pfn0C = (CtlFn)BrUiHook85_100440B0;
    p->pfn08 = (CtlFn)BrUiHook85_10044090;
    p->w1E20C = 3;
    p->s34(BrStrGet(0x57), 1, 1, &DAT_100aabe8);
    cont->w14 += 1;
    cont->w344 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, cont->f338, cont->f33C - DAT_1007765c, 0x102001, 2, 5, 1, -1);
    p->pfn0C = (CtlFn)BrSub10047360;
    p->pfn08 = (CtlFn)BrUiHook81_100463C0;
    p->w1E20C = 3;
    p->s34(BrStrGet(0xc), 1, 1, &DAT_100aabe8);
    cont->w14 += 1;
    cont->w344 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 95.0f, 317.0f, 9, 2, 5, 0, 0x67);
    cont->w14 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 130.0f, 336.0f, 0x100009, 2, 5, 1, -1);
    p->w1E20C = 0x34;
    p->s34(BrStrGet(0x59), 1, 4, &DAT_100aabe8);
    cont->w14 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 130.0f, 374.0f, 0x100009, 2, 5, 1, -1);
    p->w1E20C = 3;
    p->s34(BrStrGet(0x5a), 1, 1, &DAT_100aabe8);
    cont->w14 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 155.0f, 393.0f, 9, 2, 5, 0, 0x39);
    cont->w14 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 155.0f, 393.0f, 0x200001, 2, 5, 1, -1);
    p->pfn08 = (CtlFn)BrUiHook85_10042AC0;
    p->pfn04 = (CtlFn)Br85TextReadBack;
    p->pfn10 = (CtlFn)BrStubTrue;
    p->w1E20C = 3;
    p->s34(&DAT_10396f08, 1, 1, &DAT_100aabe8);
    p->m2B5C.s1();
    p->f050 = 0x9b;
    p->m2B5C.a424[0] = 0x9b;
    p->f058 = 0x15b;
    p->m2B5C.a424[2] = 0x15b;
    p->f054 = 0x189;
    p->m2B5C.a424[1] = 0x189;
    p->f05C = 0x199;
    p->m2B5C.a424[3] = 0x199;
    p->m2B5C.w41C = (short)(p->m2B5C.a424[2] - p->m2B5C.a424[0]) - 0x10;
    cont->w14 += 1;
    cont->w344 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 130.0f, 412.0f, 0x100009, 2, 5, 1, -1);
    p->w1E20C = 3;
    p->s34(BrStrGet(0x5b), 1, 1, &DAT_100aabe8);
    cont->w14 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 155.0f, 431.0f, 9, 2, 5, 0, 0x39);
    cont->w14 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 155.0f, 431.0f, 0x200001, 2, 5, 1, -1);
    p->pfn08 = (CtlFn)BrUiHook85_10042AC0;
    p->pfn04 = (CtlFn)BrUiHook85_1003F0B0;
    p->pfn10 = (CtlFn)BrStubTrue;
    p->w1E20C = 3;
    p->s34(&DAT_10396f08, 1, 1, &DAT_100aabe8);
    p->m2B5C.s1();
    p->f050 = 0x9b;
    p->m2B5C.a424[0] = 0x9b;
    p->f058 = 0x15b;
    p->m2B5C.a424[2] = 0x15b;
    p->f054 = 0x1af;
    p->m2B5C.a424[1] = 0x1af;
    p->f05C = 0x1bf;
    p->m2B5C.a424[3] = 0x1bf;
    p->m2B5C.w41C = (short)(p->m2B5C.a424[2] - p->m2B5C.a424[0]) - 0x10;
    cont->w14 += 1;
    cont->w344 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 80.0f, 46.0f, 9, 2, 5, 0, 7);
    cont->w14 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 339.0f, 61.0f, 1, 2, 5, 1, 0x45);
    p->pfn04 = (CtlFn)BrMenuCap0930;
    p->w2AB6[0] = (short)(cont->w14 + 1);
    p->w2AB4 += 1;
    cont->w14 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, cont->f338, 160.0f, 0x101001, 2, 5, 1, -1);
    p->pfn04 = (CtlFn)BrUiText1003FC40;
    p->w1E20C = 3;
    p->s34(&DAT_100acad8, 1, 1, &DAT_100aac48);
    cont->w14 += 1;

    return 1;
}
