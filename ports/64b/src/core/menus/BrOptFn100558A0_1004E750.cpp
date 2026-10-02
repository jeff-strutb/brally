/* WHAT IT DOES: build one menu page: creates the page container, adds every
 * control on it in turn, and reports failure if any of them could not be
 * made. One of a family of page builders, each laying out its own screen,
 * setting the return mode before it lays out. */
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
#define _CRTIMP __declspec(dllimport)
#include "br_coretypes.h"   /* br_globals: its objects */
#include "slice3_39.h"   /* BrTextBox, the canonical record */
#include "br_phase.h"   /* BrPhase_, the canonical record */
#include "br_ui.h"   /* BrUiCtl_, the canonical record */
#include "br_race.h"   /* br_globals: its objects */
#include <string.h>

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







typedef char chk_tbl04E750[(unsigned)&((Root04E750 *)0)->pTable == 0xC0 ? 1 : -1];

typedef int (*CtlFn)(BrCtl *);

extern "C" {
/* 64-bit core: g_br0AA010 is defined once, in br_globals.c */
/* Br85TextReadBack: prototype in br_funcs.h */
/* BrHook_10044010: prototype in br_funcs.h */
/* BrMenuCap0930: prototype in br_funcs.h */
/* BrStubTrue: prototype in br_funcs.h */
/* BrSub10047360: prototype in br_funcs.h */
/* BrUiHook81_100463C0: prototype in br_funcs.h */
/* BrUiHook85_1003F0B0: prototype in br_funcs.h */
/* BrUiHook85_10042AC0: prototype in br_funcs.h */
/* BrUiHook85_10044030: prototype in br_funcs.h */
/* BrUiHook85_10044050: prototype in br_funcs.h */
/* BrUiHook85_10044070: prototype in br_funcs.h */
/* BrUiHook85_10044090: prototype in br_funcs.h */
/* BrUiHook85_100440B0: prototype in br_funcs.h */
/* BrUiText1003FC40: prototype in br_funcs.h */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* BrStrGet: prototype in br_funcs.h */
/* FUN_100378c0: prototype in br_funcs.h */
}

int BrOptFn100558A0(GameUi *parent)
{
    Page04E750 *cont;
    BrCtl     *p;
    char       bad;

    (*(short *)&((BrPhase_ *)(parent))->iPage) = 0;
    (*(int *)((char *)&g_brRaceRules + 0xC)) /* BR_LP64_BYTE_VIEW */ = 6;
    (*(int (*)[1])&((BrPhase_ *)(parent))->aFlags[0])[(*(unsigned short *)&((BrPhase_ *)(parent))->nPages)] = 1;
    cont = new Page04E750;
    (*(Page04E750 * (*)[22])&((BrPhase_ *)(parent))->aPages[0])[(*(unsigned short *)&((BrPhase_ *)(parent))->nPages)] = cont;
    bad = (cont == 0);
    if (bad)
        FUN_100378c0(4);
    (*(unsigned short *)&((BrPhase_ *)(parent))->nPages) += 1;
    (*(GameUi * *)&((BrUiPage_ *)(cont))->pOwner) = parent;
    (*(int *)&((BrUiPage_ *)(cont))->f10) = 0;
    (*(float *)&((BrUiPage_ *)(cont))->fX) = 195.0f;
    (*(float *)&((BrUiPage_ *)(cont))->fY) = 130.0f;
    p = new BrCtl;
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 0, 0, 9, 2, 5, 0, 0);
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;
    p = new BrCtl;
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, (*(float *)&((BrUiPage_ *)(cont))->fX), 10.0f, 0x100009, 2, 5, 1, -1);
    (*(unsigned short *)&((BrUiCtl_ *)(p))->w1E20C) = 3;
    p->s34((char *)(BrStrGet(0x54)), 1, 1, (char *)(&DAT_100aaca8));
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;
    p = new BrCtl;
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, (*(float *)&((BrUiPage_ *)(cont))->fX), (*(float *)&((BrUiPage_ *)(cont))->fY), 0x102001, 2, 5, 1, -1);
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn0C) = (CtlFn)BrUiHook85_10044030;
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn08) = (CtlFn)BrHook_10044010;
    (*(unsigned short *)&((BrUiCtl_ *)(p))->w1E20C) = 3;
    p->s34((char *)(BrStrGet(0x55)), 1, 1, (char *)(&(*(char *)&g_hot0)));
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;
    (*(short *)&((BrUiPage_ *)(cont))->cSel) += 1;
    p = new BrCtl;
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, (*(float *)&((BrUiPage_ *)(cont))->fX), (*(float *)&((BrUiPage_ *)(cont))->fY) - DAT_10077648, 0x102001, 2, 5, 1, -1);
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn0C) = (CtlFn)BrUiHook85_10044070;
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn08) = (CtlFn)BrUiHook85_10044050;
    (*(unsigned short *)&((BrUiCtl_ *)(p))->w1E20C) = 3;
    p->s34((char *)(BrStrGet(0x56)), 1, 1, (char *)(&(*(char *)&g_hot0)));
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;
    (*(short *)&((BrUiPage_ *)(cont))->cSel) += 1;
    p = new BrCtl;
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, (*(float *)&((BrUiPage_ *)(cont))->fX), (*(float *)&((BrUiPage_ *)(cont))->fY) - DAT_1007764c, 0x102001, 2, 5, 1, -1);
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn0C) = (CtlFn)BrUiHook85_100440B0;
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn08) = (CtlFn)BrUiHook85_10044090;
    (*(unsigned short *)&((BrUiCtl_ *)(p))->w1E20C) = 3;
    p->s34((char *)(BrStrGet(0x57)), 1, 1, (char *)(&(*(char *)&g_hot0)));
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;
    (*(short *)&((BrUiPage_ *)(cont))->cSel) += 1;
    p = new BrCtl;
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, (*(float *)&((BrUiPage_ *)(cont))->fX), (*(float *)&((BrUiPage_ *)(cont))->fY) - DAT_1007765c, 0x102001, 2, 5, 1, -1);
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn0C) = (CtlFn)BrSub10047360;
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn08) = (CtlFn)BrOpt63C0;
    (*(unsigned short *)&((BrUiCtl_ *)(p))->w1E20C) = 3;
    p->s34((char *)(BrStrGet(0xc)), 1, 1, (char *)(&(*(char *)&g_hot0)));
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;
    (*(short *)&((BrUiPage_ *)(cont))->cSel) += 1;
    p = new BrCtl;
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 95.0f, 317.0f, 9, 2, 5, 0, 0x67);
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;
    p = new BrCtl;
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 130.0f, 336.0f, 0x100009, 2, 5, 1, -1);
    (*(unsigned short *)&((BrUiCtl_ *)(p))->w1E20C) = 0x34;
    p->s34((char *)(BrStrGet(0x59)), 1, 4, (char *)(&(*(char *)&g_hot0)));
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;
    p = new BrCtl;
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 130.0f, 374.0f, 0x100009, 2, 5, 1, -1);
    (*(unsigned short *)&((BrUiCtl_ *)(p))->w1E20C) = 3;
    p->s34((char *)(BrStrGet(0x5a)), 1, 1, (char *)(&(*(char *)&g_hot0)));
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;
    p = new BrCtl;
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 155.0f, 393.0f, 9, 2, 5, 0, 0x39);
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;
    p = new BrCtl;
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 155.0f, 393.0f, 0x200001, 2, 5, 1, -1);
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn08) = (CtlFn)BrUiHook85_10042AC0;
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn04) = (CtlFn)Br85TextReadBack;
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn10) = (CtlFn)BrStubTrue;
    (*(unsigned short *)&((BrUiCtl_ *)(p))->w1E20C) = 3;
    p->s34(&(g_aBr39B720[0]), 1, 1, (char *)(&(*(char *)&g_hot0)));
    (*(class Item2B5C *)&((BrUiCtl_ *)(p))->aText[0]).s1();
    (*(int *)&((BrUiCtl_ *)(p))->rcLeft) = 0x9b;
    (*(int (*)[4])&((BrTextBox *)&((*(class Item2B5C *)&((BrUiCtl_ *)(p))->aText[0])))->left)[0] = 0x9b;
    (*(int *)&((BrUiCtl_ *)(p))->rcRight) = 0x15b;
    (*(int (*)[4])&((BrTextBox *)&((*(class Item2B5C *)&((BrUiCtl_ *)(p))->aText[0])))->left)[2] = 0x15b;
    (*(int *)&((BrUiCtl_ *)(p))->rcTop) = 0x189;
    (*(int (*)[4])&((BrTextBox *)&((*(class Item2B5C *)&((BrUiCtl_ *)(p))->aText[0])))->left)[1] = 0x189;
    (*(int *)&((BrUiCtl_ *)(p))->rcBottom) = 0x199;
    (*(int (*)[4])&((BrTextBox *)&((*(class Item2B5C *)&((BrUiCtl_ *)(p))->aText[0])))->left)[3] = 0x199;
    (*(short *)&((BrTextBox *)&((*(class Item2B5C *)&((BrUiCtl_ *)(p))->aText[0])))->f41C) = (short)((*(int (*)[4])&((BrTextBox *)&((*(class Item2B5C *)&((BrUiCtl_ *)(p))->aText[0])))->left)[2] - (*(int (*)[4])&((BrTextBox *)&((*(class Item2B5C *)&((BrUiCtl_ *)(p))->aText[0])))->left)[0]) - 0x10;
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;
    (*(short *)&((BrUiPage_ *)(cont))->cSel) += 1;
    p = new BrCtl;
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 130.0f, 412.0f, 0x100009, 2, 5, 1, -1);
    (*(unsigned short *)&((BrUiCtl_ *)(p))->w1E20C) = 3;
    p->s34((char *)(BrStrGet(0x5b)), 1, 1, (char *)(&(*(char *)&g_hot0)));
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;
    p = new BrCtl;
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 155.0f, 431.0f, 9, 2, 5, 0, 0x39);
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;
    p = new BrCtl;
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 155.0f, 431.0f, 0x200001, 2, 5, 1, -1);
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn08) = (CtlFn)BrUiHook85_10042AC0;
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn04) = (CtlFn)BrUiHook85_1003F0B0;
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn10) = (CtlFn)BrStubTrue;
    (*(unsigned short *)&((BrUiCtl_ *)(p))->w1E20C) = 3;
    p->s34(&(g_aBr39B720[0]), 1, 1, (char *)(&(*(char *)&g_hot0)));
    (*(class Item2B5C *)&((BrUiCtl_ *)(p))->aText[0]).s1();
    (*(int *)&((BrUiCtl_ *)(p))->rcLeft) = 0x9b;
    (*(int (*)[4])&((BrTextBox *)&((*(class Item2B5C *)&((BrUiCtl_ *)(p))->aText[0])))->left)[0] = 0x9b;
    (*(int *)&((BrUiCtl_ *)(p))->rcRight) = 0x15b;
    (*(int (*)[4])&((BrTextBox *)&((*(class Item2B5C *)&((BrUiCtl_ *)(p))->aText[0])))->left)[2] = 0x15b;
    (*(int *)&((BrUiCtl_ *)(p))->rcTop) = 0x1af;
    (*(int (*)[4])&((BrTextBox *)&((*(class Item2B5C *)&((BrUiCtl_ *)(p))->aText[0])))->left)[1] = 0x1af;
    (*(int *)&((BrUiCtl_ *)(p))->rcBottom) = 0x1bf;
    (*(int (*)[4])&((BrTextBox *)&((*(class Item2B5C *)&((BrUiCtl_ *)(p))->aText[0])))->left)[3] = 0x1bf;
    (*(short *)&((BrTextBox *)&((*(class Item2B5C *)&((BrUiCtl_ *)(p))->aText[0])))->f41C) = (short)((*(int (*)[4])&((BrTextBox *)&((*(class Item2B5C *)&((BrUiCtl_ *)(p))->aText[0])))->left)[2] - (*(int (*)[4])&((BrTextBox *)&((*(class Item2B5C *)&((BrUiCtl_ *)(p))->aText[0])))->left)[0]) - 0x10;
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;
    (*(short *)&((BrUiPage_ *)(cont))->cSel) += 1;
    p = new BrCtl;
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 80.0f, 46.0f, 9, 2, 5, 0, 7);
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;
    p = new BrCtl;
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 339.0f, 61.0f, 1, 2, 5, 1, 0x45);
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn04) = (CtlFn)BrMenuCap0930;
    (*(short (*)[25])&((BrUiCtl_ *)(p))->aChild)[0] = (short)((*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) + 1);
    (*(short *)&((BrUiCtl_ *)(p))->cChild) += 1;
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;
    p = new BrCtl;
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, (*(float *)&((BrUiPage_ *)(cont))->fX), 160.0f, 0x101001, 2, 5, 1, -1);
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn04) = (CtlFn)BrUiText3FC40;
    (*(unsigned short *)&((BrUiCtl_ *)(p))->w1E20C) = 3;
    p->s34((char *)(&(g_strA[0])), 1, 1, (char *)(&DAT_100aac48));
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;

    return 1;
}
