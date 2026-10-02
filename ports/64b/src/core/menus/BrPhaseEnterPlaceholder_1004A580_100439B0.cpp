#include "br_coretypes.h"   /* br_globals: its objects */
#include "br_phase.h"   /* BrPhase_, the canonical record */
#include "br_ui.h"   /* BrUiCtl_, the canonical record */
#include "br_race.h"   /* br_globals: its objects */
/* WHAT IT DOES: build one menu page: creates the page container, adds every
 * control on it in turn, and reports failure if any of them could not be
 * made. One of a family of page builders, each laying out its own screen,
 * and this one positions its controls from computed coordinates rather than
 * a fixed table. */
/* @implements 0x100439B0 glide BrPhaseEnterPlaceholder_1004A580
 * @cpp_kind method
 * @cpp_symbol ?BrPhaseEnterPlaceholder_1004A580@@YAHPAVGameUi@@@Z
 *
 * 3746 B cdecl EH-frame race-options menu builder: 0x100425E0 family
 * (same layouts and levers). Extras here: the cycle pages capture
 * DAT_10ac5d0c, then the photo trio: fx/fy floats filled from int
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






extern "C" {
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
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
/* BrSub10047360: prototype in br_funcs.h */
/* BrOptCycleTrack: prototype in br_funcs.h */
/* BrOptCycleAC64C: prototype in br_funcs.h */
/* BrOptCycleAC650: prototype in br_funcs.h */
/* BrOptCycleAC65C: prototype in br_funcs.h */
/* BrOptCycleAA2A08: prototype in br_funcs.h */
/* BrOpt3760: prototype in br_funcs.h */
/* FUN_1003f980: prototype in br_funcs.h */
/* BrPhaseActivate_1003ED70: prototype in br_funcs.h */
/* BrOpt3FA0: prototype in br_funcs.h */
/* BrHook_100458C0: prototype in br_funcs.h */
/* BrUiFn1003E920: prototype in br_funcs.h */
/* BrRaceIconLookup: prototype in br_funcs.h */
/* BrUiText1003F760: prototype in br_funcs.h */
/* BrUiText1003F7F0: prototype in br_funcs.h */
/* BrUiText1003F990: prototype in br_funcs.h */
/* BrMenuCap0870: prototype in br_funcs.h */
/* BrMenuCap0890: prototype in br_funcs.h */
/* BrMenuCap08B0: prototype in br_funcs.h */
/* FUN_10038da0: prototype in br_funcs.h */
}

typedef int (*CtlFn)(BrCtl *);

int BrPhaseEnterPlaceholder_1004A580(GameUi *parent)
{
    Phase32F *cont;
    BrCtl *p;
    char bad;
    float fx, fy;
    int xi, yi;

    (*(short *)&((BrPhase_ *)(parent))->iPage) = 0;
    (*(int (*)[1])&((BrPhase_ *)(parent))->aFlags[0])[(*(unsigned short *)&((BrPhase_ *)(parent))->nPages)] = 1;
    cont = new Phase32F;
    (*(Phase32F * (*)[22])&((BrPhase_ *)(parent))->aPages[0])[(*(unsigned short *)&((BrPhase_ *)(parent))->nPages)] = cont;
    bad = (cont == 0);
    if (bad)
        FUN_100378c0(4);
    (*(unsigned short *)&((BrPhase_ *)(parent))->nPages) += 1;
    (*(GameUi * *)&((BrUiPage_ *)(cont))->pOwner) = parent;
    (*(int *)&((BrUiPage_ *)(cont))->f10) = 0;
    (*(float *)&((BrUiPage_ *)(cont))->fX) = 195.0f;
    (*(float *)&((BrUiPage_ *)(cont))->fY) = 111.0f;
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
    p->s34((char *)(BrStrGet(0x13)), 1, 1, (char *)(&DAT_100aaca8));
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;
    p = new BrCtl;
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, (*(float *)&((BrUiPage_ *)(cont))->fX), (*(float *)&((BrUiPage_ *)(cont))->fY), 0x102001, 2, 5, 1, -1);
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn0C) = (CtlFn)BrSub10047360;
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn08) = (CtlFn)BrOptCycleTrack;
    (*(unsigned short *)&((BrUiCtl_ *)(p))->w1E20C) = 3;
    p->s34((char *)(BrStrGet(0x14)), 1, 1, (char *)(&(*(char *)&g_hot0)));
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;
    (*(short *)&((BrUiPage_ *)(cont))->cSel) += 1;
    p = new BrCtl;
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, (*(float *)&((BrUiPage_ *)(cont))->fX), (*(float *)&((BrUiPage_ *)(cont))->fY) - DAT_10077648, 0x102001, 2, 5, 1, -1);
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn0C) = (CtlFn)BrSub10047360;
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn08) = (CtlFn)BrOptCycleAC64C;
    (*(unsigned short *)&((BrUiCtl_ *)(p))->w1E20C) = 3;
    p->s34((char *)(BrStrGet(0x15)), 1, 1, (char *)(&(*(char *)&g_hot0)));
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;
    (*(short *)&((BrUiPage_ *)(cont))->cSel) += 1;
    p = new BrCtl;
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, (*(float *)&((BrUiPage_ *)(cont))->fX), (*(float *)&((BrUiPage_ *)(cont))->fY) - DAT_1007764c, 0x102001, 2, 5, 1, -1);
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn0C) = (CtlFn)BrSub10047360;
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn08) = (CtlFn)BrOptCycleAC650;
    (*(unsigned short *)&((BrUiCtl_ *)(p))->w1E20C) = 3;
    p->s34((char *)(BrStrGet(0x16)), 1, 1, (char *)(&(*(char *)&g_hot0)));
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;
    (*(short *)&((BrUiPage_ *)(cont))->cSel) += 1;
    p = new BrCtl;
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, (*(float *)&((BrUiPage_ *)(cont))->fX), (*(float *)&((BrUiPage_ *)(cont))->fY) - DAT_10077650, 0x102001, 2, 5, 1, -1);
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn0C) = (CtlFn)BrSub10047360;
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn08) = (CtlFn)BrOptCycleAC65C;
    (*(unsigned short *)&((BrUiCtl_ *)(p))->w1E20C) = 3;
    p->s34((char *)(BrStrGet(0x17)), 1, 1, (char *)(&(*(char *)&g_hot0)));
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;
    (*(short *)&((BrUiPage_ *)(cont))->cSel) += 1;
    p = new BrCtl;
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, (*(float *)&((BrUiPage_ *)(cont))->fX), (*(float *)&((BrUiPage_ *)(cont))->fY) - DAT_10077654, 0x102001, 2, 5, 1, -1);
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn0C) = (CtlFn)BrSub10047360;
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn08) = (CtlFn)BrOptCycleAA2A08;
    (*(unsigned short *)&((BrUiCtl_ *)(p))->w1E20C) = 3;
    p->s34((char *)(BrStrGet(0x18)), 1, 1, (char *)(&(*(char *)&g_hot0)));
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;
    (*(short *)&((BrUiPage_ *)(cont))->cSel) += 1;
    p = new BrCtl;
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, (*(float *)&((BrUiPage_ *)(cont))->fX), (*(float *)&((BrUiPage_ *)(cont))->fY) - DAT_1007765c, 0x102001, 2, 5, 1, -1);
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn0C) = (CtlFn)BrSub10047360;
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn08) = (CtlFn)BrOpt3760;
    (*(unsigned short *)&((BrUiCtl_ *)(p))->w1E20C) = 3;
    p->s34((char *)(BrStrGet(0x19)), 1, 1, (char *)(&(*(char *)&g_hot0)));
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;
    (*(short *)&((BrUiPage_ *)(cont))->cSel) += 1;
    p = new BrCtl;
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, (*(float *)&((BrUiPage_ *)(cont))->fX), (*(float *)&((BrUiPage_ *)(cont))->fY) - DAT_10077660, 0x102001, 2, 5, 1, -1);
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn0C) = (CtlFn)BrSub10047360;
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn08) = (CtlFn)BrOpt64E0;
    (*(unsigned short *)&((BrUiCtl_ *)(p))->w1E20C) = 3;
    p->s34((char *)(BrStrGet(0xc)), 1, 1, (char *)(&(*(char *)&g_hot0)));
    (*(BrCtl * *)&DAT_10ac5d0c) = p;
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;
    (*(short *)&((BrUiPage_ *)(cont))->cSel) += 1;
    fx = (float)(*(int *)&g_hot2);
    fy = (float)(*(int *)((char *)&g_hot2 + 0x4));
    if ((*(int *)((char *)&g_brRaceRules + 0xC)) /* BR_LP64_BYTE_VIEW */ == 0) {
        p = new BrCtl;
        (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
        bad = (p == 0);
        if (bad)
                FUN_100378c0(4);
        p->s38(parent, fx, fy, 0x402001, 2, 5, 1, 0x78);
        (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn0C) = (CtlFn)BrSub10047360;
        (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn08) = (CtlFn)BrPhaseActivate_1003ED70;
        yi = (int)fy;
        (*(int *)&((BrUiCtl_ *)(p))->rcTop) = yi;
        xi = (int)fx;
        fy = fy - DAT_10077664;
        (*(int *)&((BrUiCtl_ *)(p))->rcLeft) = xi;
        (*(int *)&((BrUiCtl_ *)(p))->rcRight) = xi + 0x7f;
        (*(int *)&((BrUiCtl_ *)(p))->rcBottom) = yi + 0x21;
        (*(int *)&((BrUiCtl_ *)(p))->f2968) = 0;
        (*(short *)&((BrUiCtl_ *)(p))->aStepId[1]) = 0x79;
        (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;
    }
    p = new BrCtl;
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, fx, fy, 0x402001, 2, 5, 1, 0x52);
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn0C) = (CtlFn)BrSub10047360;
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn08) = (CtlFn)BrOpt3FA0;
    yi = (int)fy;
    (*(int *)&((BrUiCtl_ *)(p))->rcTop) = yi;
    xi = (int)fx;
    fy = fy - DAT_10077664;
    (*(int *)&((BrUiCtl_ *)(p))->f2968) = 0;
    (*(int *)&((BrUiCtl_ *)(p))->rcLeft) = xi;
    (*(short *)&((BrUiCtl_ *)(p))->aStepId[1]) = 0x53;
    (*(int *)&((BrUiCtl_ *)(p))->rcRight) = xi + 0x7f;
    (*(int *)&((BrUiCtl_ *)(p))->rcBottom) = yi + 0x21;
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;
    p = new BrCtl;
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, fx, fy, 0x402001, 2, 5, 1, 0x54);
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn0C) = (CtlFn)BrSub10047360;
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn08) = (CtlFn)BrHook_100458C0;
    yi = (int)fy;
    (*(int *)&((BrUiCtl_ *)(p))->rcTop) = yi;
    (*(int *)&((BrUiCtl_ *)(p))->rcLeft) = xi;
    (*(int *)&((BrUiCtl_ *)(p))->rcRight) = xi + 0x7f;
    (*(int *)&((BrUiCtl_ *)(p))->rcBottom) = yi + 0x21;
    (*(int *)&((BrUiCtl_ *)(p))->f2968) = 0;
    (*(short *)&((BrUiCtl_ *)(p))->aStepId[1]) = 0x55;
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;
    p = new BrCtl;
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 61.0f, 244.0f, 9, 2, 5, 1, 0x36);
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;
    p = new BrCtl;
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 73.0f, 214.0f, 1, 2, 5, 1, 0x35);
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn04) = (CtlFn)BrUiFn1003E920;
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;
    p = new BrCtl;
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 339.0f, 70.0f, 1, 2, 5, 1, 0xb);
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn04) = (CtlFn)BrRaceIconLookup;
    (*(short (*)[25])&((BrUiCtl_ *)(p))->aChild)[0] = (short)((*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) + 1);
    (*(short *)&((BrUiCtl_ *)(p))->cChild) += 1;
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;
    p = new BrCtl;
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, (*(float *)&((BrUiPage_ *)(cont))->fX), 137.0f, 0x101001, 2, 5, 1, -1);
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn04) = (CtlFn)BrUiText1003F760;
    (*(unsigned short *)&((BrUiCtl_ *)(p))->w1E20C) = 3;
    p->s34((char *)(&(g_strA[0])), 1, 1, (char *)(&DAT_100aac08));
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;
    p = new BrCtl;
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 108.0f, 66.0f, 1, 2, 5, 1, 0x19);
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn04) = (CtlFn)BrMenuCap0890;
    (*(short (*)[25])&((BrUiCtl_ *)(p))->aChild)[0] = (short)((*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) + 1);
    (*(short *)&((BrUiCtl_ *)(p))->cChild) += 1;
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;
    p = new BrCtl;
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, (*(float *)&((BrUiPage_ *)(cont))->fX), 123.0f, 0x101001, 2, 5, 1, -1);
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn04) = (CtlFn)BrUiText3F7F0;
    (*(unsigned short *)&((BrUiCtl_ *)(p))->w1E20C) = 3;
    p->s34((char *)(&(g_strA[0])), 1, 1, &DAT_100aac38);
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;
    p = new BrCtl;
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 476.0f, 129.0f, 1, 2, 5, 1, 0xe);
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn04) = (CtlFn)BrMenuCap08B0;
    (*(short (*)[25])&((BrUiCtl_ *)(p))->aChild)[0] = (short)((*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) + 1);
    (*(short *)&((BrUiCtl_ *)(p))->cChild) += 1;
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;
    p = new BrCtl;
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, (*(float *)&((BrUiPage_ *)(cont))->fX), 181.0f, 0x101001, 2, 5, 1, -1);
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn04) = (CtlFn)BrUiText3F990;
    (*(unsigned short *)&((BrUiCtl_ *)(p))->w1E20C) = 3;
    p->s34((char *)(&(g_strA[0])), 1, 1, &DAT_100aac28);
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;
    p = new BrCtl;
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 505.0f, 200.0f, 1, 2, 5, 1, 0xc);
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn04) = (CtlFn)BrMenuCap0870;
    (*(short (*)[25])&((BrUiCtl_ *)(p))->aChild)[0] = (short)((*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) + 1);
    (*(short *)&((BrUiCtl_ *)(p))->cChild) += 1;
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;
    p = new BrCtl;
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, (*(float *)&((BrUiPage_ *)(cont))->fX), 262.0f, 0x101001, 2, 5, 1, -1);
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn04) = (CtlFn)BrUiText3F860;
    (*(unsigned short *)&((BrUiCtl_ *)(p))->w1E20C) = 3;
    p->s34((char *)(&(g_strA[0])), 1, 1, (char *)(&DAT_100aac18));
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;
    return 1;
}
