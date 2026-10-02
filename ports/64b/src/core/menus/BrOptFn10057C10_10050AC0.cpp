#include "br_coretypes.h"   /* br_globals: its objects */
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







typedef char chk_tbl050AC0[(unsigned)&((Root050AC0 *)0)->pTable == 0xC0 ? 1 : -1];

typedef int (*CtlFn)(BrCtl *);

extern "C" {
/* 64-bit core: DAT_10ac4090 is defined once, in br_globals.c */
/* 64-bit core: g_brAA2884 is defined once, in br_globals.c */
/* 64-bit core: DAT_10226a48 is defined once, in br_globals.c */
/* 64-bit core: DAT_10ac5d10 is defined once, in br_globals.c */
/* BrMenuCap0730: prototype in br_funcs.h */
/* BrMenuCap07E0: prototype in br_funcs.h */
/* BrMenuText08D0: prototype in br_funcs.h */
/* BrOpt37D0: prototype in br_funcs.h */
/* BrOptCycleAA2A00: prototype in br_funcs.h */
/* BrOptCycleAA2A18: prototype in br_funcs.h */
/* BrOptCycleBD3E0: prototype in br_funcs.h */
/* BrOptOpen2954: prototype in br_funcs.h */
/* BrPhaseTick_100474B0: prototype in br_funcs.h */
/* BrSub10047360: prototype in br_funcs.h */
/* BrUiHook87_1003F5E0: prototype in br_funcs.h */
/* BrUiHook87_1003F680: prototype in br_funcs.h */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* FUN_10038f40: prototype in br_funcs.h */
/* FUN_100393c0: prototype in br_funcs.h */
/* FUN_1003c430: prototype in br_funcs.h */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* BrStrGet: prototype in br_funcs.h */
/* FUN_100378c0: prototype in br_funcs.h */
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
    p->s34((char *)(BrStrGet(100)), 1, 1, (char *)(&DAT_100aaca8));
    cont->w14 += 1;
    if ((*(int *)&g_brRaceNet) == 2) {
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, cont->f338, cont->f33C, 0x102001, 2, 5, 1, -1);
    p->pfn0C = (CtlFn)BrPhaseTick_100474B0;
    p->pfn08 = (CtlFn)FUN_1003c430;
    p->w1E20C = 3;
    p->s34((char *)(BrStrGet(0x1b)), 1, 1, (char *)(&(*(char *)&g_hot0)));
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
    p->s34((char *)(BrStrGet(0x1c)), 1, 1, (char *)(&(*(char *)&g_hot0)));
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
    p->s34((char *)(BrStrGet(0x1d)), 1, 1, (char *)(&(*(char *)&g_hot0)));
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
    p->s34((char *)(BrStrGet(0x65)), 1, 1, (char *)(&(*(char *)&g_hot0)));
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
    p->pfn08 = (CtlFn)Ctl3DC20_fn;
    p->pfn18 = (CtlFn)BrOpt37D0;
    p->w1E20C = 3;
    if (g_host != 0)
        p->s34((char *)(BrStrGet(0x66)), 1, 1, (char *)(&(*(char *)&g_hot0)));
    else
        p->s34((char *)(BrStrGet(0x1E)), 1, 1, (char *)(&(*(char *)&g_hot0)));
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
    if (g_guardB != 0)
        p->s34((char *)(BrStrGet(0x67)), 1, 1, (char *)(&(*(char *)&g_hot0)));
    else
        p->s34((char *)(BrStrGet(0x0C)), 1, 1, (char *)(&(*(char *)&g_hot0)));
    (*(BrCtl * *)&g_29B8) = (BrCtl *)((struct Phase *)((BrCtl *)((struct Phase *)(p))));
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
    p->pfn04 = (CtlFn)BrItemSetModeLabel_100393C0;
    p->w1E20C = 3;
    p->s34((char *)(&(g_strA[0])), 1, 1, (char *)(&DAT_100aac58));
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
    p->pfn04 = (CtlFn)BrItemSetPickLabel_10038F40;
    p->w1E20C = 3;
    p->s34((char *)(&(g_strA[0])), 1, 1, (char *)(&DAT_100aac48));
    cont->w14 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 106.0f, 68.0f, 0x5001, 2, 5, 1, -1);
    p->pfn04 = (CtlFn)BrMenuText08D0;
    p->w1E20C = 5;
    p->s34(&DAT_100aca4c, 1, 3, (char *)(&DAT_100aabf8));
    cont->w14 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 106.0f, 115.0f, 0x100001, 2, 5, 1, -1);
    p->w1E20C = 3;
    p->s34((char *)(BrStrGet(0x1d)), 1, 1, (char *)(&DAT_100aabf8));
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
