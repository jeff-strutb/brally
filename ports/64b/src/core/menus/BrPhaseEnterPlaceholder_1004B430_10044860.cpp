/* WHAT IT DOES: build one menu page: creates the page container, adds every
 * control on it in turn, and reports failure if any of them could not be
 * made. One of a family of page builders, each laying out its own screen,
 * positioning its controls from a running vertical coordinate rather than a
 * fixed table. */
/* @implements 0x10044860 glide BrPhaseEnterPlaceholder_1004B430
 * @cpp_kind free
 * @cpp_symbol ?BrPhaseEnterPlaceholder_1004B430@@YAHPAVGameUi@@@Z
 *
 * 2439 B cdecl EH-frame menu-page builder. Scaffolded by
 * tools/gen_menubuilder.py from the Ghidra draft; the class layouts and the
 * three family levers come from the hand-solved 0x100425E0 / 0x10048160
 * (char bool after the slot store, raw float pushes for simple lvalues,
 * w14-then-w344 tails).
 *
 * The generator used to BAIL on this draft ("no recognisable entry point")
 * because Ghidra types the parent pointer as `float param_1`. That one
 * mis-typing produced five symptoms -- an `(int)` cast on every prologue use
 * and a `*(float *)` +0x340 store -- so the generator now undoes the typing
 * at parse time instead of matching five patterns against it.
 *
 * What Ghidra could not see, read off the original: there is ONE running
 * float `fy`, not the two temps the draft splits it into. It is zeroed at
 * the top, set to 19.0f as the LAST statement of the first
 * `DAT_100abaa4` block (the `je` skips that store, which is how the block's
 * extent is fixed), and then steps down past four controls whose second
 * float argument is `fy + cont->f33C`. A second `DAT_100abaa4` block later
 * wraps exactly two controls.
 *
 * BYTE-EXACT 2026-09-12 (4/4 pieces), two findings, neither a spelling:
 *  - `fy = 0.0f` is the FIRST statement, before `parent->w12 = 0`. Written
 *    second, the zero is emitted after ebx is pinned to 0 and becomes
 *    `mov [slot], ebx`, and fy is then homed in the dead parameter slot
 *    with the operator-new temp in the `push ecx` local. Written first it
 *    is an immediate `mov [esp+0x14], 0` into the real local and the temp
 *    takes the parameter slot, exactly as the original (1952 -> 36).
 *  - The four `fld [esp+0x10]; fadd [esi+0x33c]` sites are the compiler
 *    OPTION /Gi (incremental compilation), not source: under /O2 /GX /MD
 *    VC5 loads the member first for every spelling of a float
 *    `local + member` (24 declaration orders, temps, references, pointers,
 *    casts, volatile, inline helper, `(fy + 0.0f)` -- all member-first or
 *    worse), and under /O2 /Gi /GX /MD the same source is 0 diffs. /Gi is
 *    per-TU: 0x10004AD0 is exact only WITHOUT it (it flips an `or cl,al`).
 *    cpp_score.DEFAULT_OPTS now carries the /Gi shape; the sweep picks it.
 */
#define _CRTIMP __declspec(dllimport)
#include "br_coretypes.h"   /* br_globals: its objects */
#include "br_phase.h"   /* BrPhase_, the canonical record */
#include "br_ui.h"   /* BrUiCtl_, the canonical record */
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



class SlotTable044860 {
public:
    virtual void s0();
    virtual void s1(char *pszPattern);   /* +0x04 */
    char aRecs[26000];                   /* +0x04, 0x104-byte records */
};

class Root044860 {
public:
    char            pad000[0xC0];
    SlotTable044860 *pTable;         /* +0xC0 */
    SlotTable044860 *pTableC4;       /* +0xC4 */
};


class Page044860 {
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
    Page044860();
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
    Page044860 *a14[22];              /* +0x14 */
    int a6C[1];                     /* +0x6C */
};








typedef int (*CtlFn)(BrCtl *);

extern "C" {
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: DAT_10ac5d04 is defined once, in br_globals.c */
/* BrMenuCap0730: prototype in br_funcs.h */
/* BrMenuCap07E0: prototype in br_funcs.h */
/* BrMenuText08D0: prototype in br_funcs.h */
/* BrOptCycleAA2A00: prototype in br_funcs.h */
/* BrOptCycleBD3E0: prototype in br_funcs.h */
/* BrPhaseActivate_10045110: prototype in br_funcs.h */
/* BrPhaseTick_100474B0: prototype in br_funcs.h */
/* BrSub10047360: prototype in br_funcs.h */
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
/* FUN_1003f8f0: prototype in br_funcs.h */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* BrStrGet: prototype in br_funcs.h */
/* FUN_100378c0: prototype in br_funcs.h */
}

int BrPhaseEnterPlaceholder_1004B430(GameUi *parent)
{
    float      fy;
    Page044860 *cont;
    BrCtl     *p;
    char       bad;

    fy = 0.0f;
    (*(short *)&((BrPhase_ *)(parent))->iPage) = 0;
    (*(int (*)[1])&((BrPhase_ *)(parent))->aFlags[0])[(*(unsigned short *)&((BrPhase_ *)(parent))->nPages)] = 1;
    cont = new Page044860;
    (*(Page044860 * (*)[22])&((BrPhase_ *)(parent))->aPages[0])[(*(unsigned short *)&((BrPhase_ *)(parent))->nPages)] = cont;
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
    p->s34((char *)(BrStrGet(0x1a)), 1, 1, (char *)(&DAT_100aaca8));
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;
    if (DAT_100abaa4 != 0) {
    p = new BrCtl;
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, (*(float *)&((BrUiPage_ *)(cont))->fX), (*(float *)&((BrUiPage_ *)(cont))->fY), 0x102001, 2, 5, 1, -1);
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn0C) = (CtlFn)BrPhaseTick_100474B0;
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn08) = (CtlFn)FUN_1003c430;
    (*(unsigned short *)&((BrUiCtl_ *)(p))->w1E20C) = 3;
    p->s34((char *)(BrStrGet(0x1b)), 1, 1, (char *)(&(*(char *)&g_hot0)));
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;
    (*(short *)&((BrUiPage_ *)(cont))->cSel) += 1;
    fy = 19.0f;
    }
    p = new BrCtl;
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, (*(float *)&((BrUiPage_ *)(cont))->fX), fy + (*(float *)&((BrUiPage_ *)(cont))->fY), 0x102001, 2, 5, 1, -1);
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn0C) = (CtlFn)BrSub10047360;
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn08) = (CtlFn)BrOptCycleAA2A00;
    (*(unsigned short *)&((BrUiCtl_ *)(p))->w1E20C) = 3;
    p->s34((char *)(BrStrGet(0x1c)), 1, 1, (char *)(&(*(char *)&g_hot0)));
    fy = fy - DAT_10077648;
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;
    (*(short *)&((BrUiPage_ *)(cont))->cSel) += 1;
    p = new BrCtl;
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, (*(float *)&((BrUiPage_ *)(cont))->fX), fy + (*(float *)&((BrUiPage_ *)(cont))->fY), 0x102001, 2, 5, 1, -1);
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn0C) = (CtlFn)BrSub10047360;
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn08) = (CtlFn)BrOptCycleBD3E0;
    (*(unsigned short *)&((BrUiCtl_ *)(p))->w1E20C) = 3;
    p->s34((char *)(BrStrGet(0x1d)), 1, 1, (char *)(&(*(char *)&g_hot0)));
    fy = fy - DAT_10077650;
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;
    (*(short *)&((BrUiPage_ *)(cont))->cSel) += 1;
    p = new BrCtl;
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, (*(float *)&((BrUiPage_ *)(cont))->fX), fy + (*(float *)&((BrUiPage_ *)(cont))->fY), 0x102001, 2, 5, 1, -1);
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn0C) = (CtlFn)BrPhaseTick_100474B0;
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn08) = (CtlFn)CtlE660_fn;
    (*(unsigned short *)&((BrUiCtl_ *)(p))->w1E20C) = 3;
    p->s34((char *)(BrStrGet(0x1e)), 1, 1, (char *)(&(*(char *)&g_hot0)));
    fy = fy - DAT_10077648;
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;
    (*(short *)&((BrUiPage_ *)(cont))->cSel) += 1;
    p = new BrCtl;
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, (*(float *)&((BrUiPage_ *)(cont))->fX), fy + (*(float *)&((BrUiPage_ *)(cont))->fY), 0x102001, 2, 5, 1, -1);
    p = new BrCtl;
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, (*(float *)&((BrUiPage_ *)(cont))->fX), (*(float *)&((BrUiPage_ *)(cont))->fY) - DAT_1007765c, 0x102001, 2, 5, 1, -1);
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn0C) = (CtlFn)BrSub10047360;
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn08) = (CtlFn)BrOpt6450;
    (*(unsigned short *)&((BrUiCtl_ *)(p))->w1E20C) = 3;
    p->s34((char *)(BrStrGet(0xc)), 1, 1, (char *)(&(*(char *)&g_hot0)));
    (*(BrCtl * *)&g_hookObj) = (BrCtl *)((struct Phase *)((BrCtl *)((struct Phase *)(p))));
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;
    (*(short *)&((BrUiPage_ *)(cont))->cSel) += 1;
    p = new BrCtl;
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 73.0f, 212.0f, 1, 2, 5, 1, 0x16);
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn04) = (CtlFn)BrMenuCap07E0;
    (*(short (*)[25])&((BrUiCtl_ *)(p))->aChild)[0] = (short)((*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) + 1);
    (*(short *)&((BrUiCtl_ *)(p))->cChild) += 1;
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;
    p = new BrCtl;
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, (*(float *)&((BrUiPage_ *)(cont))->fX), 275.0f, 0x101001, 2, 5, 1, -1);
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn04) = (CtlFn)BrItemSetModeLabel_100393C0;
    (*(unsigned short *)&((BrUiCtl_ *)(p))->w1E20C) = 3;
    p->s34((char *)(&(g_strA[0])), 1, 1, (char *)(&DAT_100aac58));
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;
    if (DAT_100abaa4 != 0) {
    p = new BrCtl;
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 325.0f, 72.0f, 1, 2, 5, 1, 0x11);
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn04) = (CtlFn)BrMenuCap0730;
    (*(short (*)[25])&((BrUiCtl_ *)(p))->aChild)[0] = (short)((*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) + 1);
    (*(short *)&((BrUiCtl_ *)(p))->cChild) += 1;
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;
    p = new BrCtl;
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, (*(float *)&((BrUiPage_ *)(cont))->fX), 152.0f, 0x101001, 2, 5, 1, -1);
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn04) = (CtlFn)BrItemSetPickLabel_10038F40;
    (*(unsigned short *)&((BrUiCtl_ *)(p))->w1E20C) = 3;
    p->s34((char *)(&(g_strA[0])), 1, 1, (char *)(&DAT_100aac48));
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;
    }
    p = new BrCtl;
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 106.0f, 68.0f, 0x5001, 2, 5, 1, -1);
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn04) = (CtlFn)BrMenuText08D0;
    (*(unsigned short *)&((BrUiCtl_ *)(p))->w1E20C) = 5;
    p->s34(&DAT_100aca4c, 1, 3, (char *)(&DAT_100aabf8));
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;
    p = new BrCtl;
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 106.0f, 115.0f, 0x100001, 2, 5, 1, -1);
    (*(unsigned short *)&((BrUiCtl_ *)(p))->w1E20C) = 3;
    p->s34((char *)(BrStrGet(0x1d)), 1, 1, (char *)(&DAT_100aabf8));
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;

    return 1;
}
