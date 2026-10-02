/* WHAT IT DOES: build one menu page: creates the page container, adds every
 * control on it in turn, and reports failure if any of them could not be
 * made. One of a family of page builders, each laying out its own screen;
 * this one latches the pending selection into the current one on entry,
 * carries a selector filled with the named-and-numbered slot labels of the
 * current profile and a three-photo strip positioned from two globals. */
/* @t3 0x1004CBA0 2026-09-13 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 3671/3671 insns 1091/1091 rows 3+3 regions 1 oracle UNCLASSIFIED
 * @t3-effort passes 2 zero-movement 1 2
 * Residue: photo1's ten-instruction Pentium-pairing schedule (identical
 * multiset; the 3+3 rows are the EH frame's fs:[0] reloc form), the wall
 * certified on 0x1004AEE0 / 0x1004BE00 / 0x1004DA00 / 0x100498A0. Dossier
 * below; dead list and schedule census in 0x1004AEE0.cpp.
 * Do not reopen before the end-grind. */
/* @implements 0x1004CBA0 glide FUN_1004cba0
 * @cpp_kind free
 * @cpp_symbol ?FUN_1004cba0@@YAHPAVGameUi@@@Z
 *
 * 3671 B cdecl EH-frame menu-page builder, 0x100425E0 family (same class
 * layouts and the three family levers). Skeleton from
 * tools/gen_menubuilder.py --partial; the hand blocks read from the asm:
 *   - the selector list is filled from the slot count of the current
 *     profile (`DAT_100b301c[g_brSel5C10].n`): label = string 0x37 +
 *     DAT_100acad8 + the upper-cased decimal index (strcpy/strcat are the
 *     inlined intrinsics), added through the selector's +0x10 slot, the
 *     for-loop's `i + 1` CSE'd with its own increment;
 *   - the photo trio (fx/fy from DAT_100aabc8/cc, xi in edi across all
 *     three pages, fy stepped by DAT_10077664 on pages 1-2), spelled as
 *     in 0x1004AEE0.cpp / 0x100498A0.cpp.
 *
 * Residue (34 diffs, 3637 of 3671 B exact): photo1's ten-instruction tail,
 * the same Pentium-pairing schedule of an identical instruction multiset
 * certified on 0x1004AEE0 / 0x1004BE00 / 0x1004DA00 / 0x100498A0 (dossier
 * and dead list in 0x1004AEE0.cpp). The original issues [fld fy][yi
 * reload + xi copy][fsub]; ours issues the fsub right after the fld.
 *
 * @t4-pass 0x1004CBA0 1 2026-09-13 probes 122 bytes 3671 insns 1091 regions 1 rows 6 census no  (generated: all 120 orders of {fy, f50, f58, f5C, f2968} after the xi cast, plus a late temp-float step and f5C-before-fy; best 32 = f50 before fy, none 0)
 * @t4-pass 0x1004CBA0 2 2026-09-13 probes 22 bytes 3671 insns 1091 regions 1 rows 6 census yes  (22 compiler options incl. /Gi /Op /G3 /G4 /G5 /Ow /Ob1 /Ob2 /Ox /O1 /Oa /Os /Ot /Oy- /Za /Gf /Gy /Gr /Ge, all 34 or worse; corpus query MISS at +0x59f len 12; residue byte-identical to 0x1004AEE0's certified census)
 */
#define _CRTIMP __declspec(dllimport)
#include "br_coretypes.h"   /* br_globals: its objects */
#include "br_phase.h"   /* BrPhase_, the canonical record */
#include "br_ui.h"   /* BrUiCtl_, the canonical record */
#include "slice2_24.h"   /* br_globals: its objects */
#include <string.h>
#include <stdlib.h>

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



struct Sel301C { int n; int pad[5]; };   /* 0x18-byte profile records at 0x100B301C */

class SlotTable04CBA0 {
public:
    virtual void s0();
    virtual void s1(char *pszPattern);   /* +0x04 */
    char aRecs[26000];                   /* +0x04, 0x104-byte records */
};

class Root04CBA0 {
public:
    char            pad000[0xC0];
    SlotTable04CBA0 *pTable;         /* +0xC0 */
    SlotTable04CBA0 *pTableC4;       /* +0xC4 */
};


class Page04CBA0 {
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
    Page04CBA0();
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
    int  f50;                       /* +0x050 */
    int  f54;
    int  f58;
    int  f5C;
    char pad60[0x2968 - 0x60];      /* +0x060 */
    int  f2968;                     /* +0x2968 */
    char pad296C[0x2A42 - 0x296C];
    short w2A42;                    /* +0x2A42 */
    char pad2A44[0x2AB4 - 0x2A44];
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
    Page04CBA0 *a14[22];              /* +0x14 */
    int a6C[1];                     /* +0x6C */
};







typedef char chk_tbl04CBA0[(unsigned)&((Root04CBA0 *)0)->pTable == 0xC0 ? 1 : -1];

typedef int (*CtlFn)(BrCtl *);

extern "C" {
/* 64-bit core: g_brAA28A4 is defined once, in br_globals.c */
/* 64-bit core: g_iAA28AC is defined once, in br_globals.c */
/* BrHook_10045840: prototype in br_funcs.h */
/* BrHook_10045860: prototype in br_funcs.h */
/* BrMenuCap0730: prototype in br_funcs.h */
/* BrMenuCap07E0: prototype in br_funcs.h */
/* 64-bit core: declared once, by its definition's header */
/* BrMenuLatchPending: prototype in br_funcs.h */
/* 64-bit core: declared once, by its definition's header */
/* BrMenuText1300: prototype in br_funcs.h */
/* BrOpt3FA0: prototype in br_funcs.h */
/* BrPhaseActivate_100460A0: prototype in br_funcs.h */
/* BrSub10047360: prototype in br_funcs.h */
/* BrUiNum1003EA90: prototype in br_funcs.h */
/* BrUiPoll1003EB60: prototype in br_funcs.h */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* FUN_10038f40: prototype in br_funcs.h */
/* FUN_100393c0: prototype in br_funcs.h */
/* FUN_10039f60: prototype in br_funcs.h */
/* FUN_1003a910: prototype in br_funcs.h */
/* FUN_100404f0: prototype in br_funcs.h */
/* br23_num_common: prototype in br_funcs.h */
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
/* BrHook_10045860: prototype in br_funcs.h */
/* BrHook_10045840: prototype in br_funcs.h */
/* BrStrGet: prototype in br_funcs.h */
/* FUN_100378c0: prototype in br_funcs.h */
}

int FUN_1004cba0(GameUi *parent)
{
    Page04CBA0 *cont;
    BrCtl     *p;
    char       bad;
    float      fx, fy;
    int        xi, yi;
    int        i;
    char       szNum[32];
    char       szItem[32];

    (*(short *)&((BrPhase_ *)(parent))->iPage) = 0;
    g_brIdx5C04 = g_brIdx5BFC;
    (*(int (*)[1])&((BrPhase_ *)(parent))->aFlags[0])[(*(unsigned short *)&((BrPhase_ *)(parent))->nPages)] = 1;
    cont = new Page04CBA0;
    (*(Page04CBA0 * (*)[22])&((BrPhase_ *)(parent))->aPages[0])[(*(unsigned short *)&((BrPhase_ *)(parent))->nPages)] = cont;
    bad = (cont == 0);
    if (bad)
        FUN_100378c0(4);
    (*(unsigned short *)&((BrPhase_ *)(parent))->nPages) += 1;
    (*(GameUi * *)&((BrUiPage_ *)(cont))->pOwner) = parent;
    (*(int (**)(void))&((BrUiPage_ *)(cont))->pfn04) = (int (*)(void))BrMenuLatchPending;
    (*(int (**)(void))&((BrUiPage_ *)(cont))->pfn08) = (int (*)(void))FUN_10039f60;
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
    p->s34((char *)(BrStrGet(0x4e)), 1, 1, (char *)(&DAT_100aaca8));
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;
    p = new BrCtl;
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, (*(float *)&((BrUiPage_ *)(cont))->fX), (*(float *)&((BrUiPage_ *)(cont))->fY), 0x3001, 2, 5, 1, -1);
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn04) = (CtlFn)BrUiPoll1003EB60;
    (*(int *)&((BrUiCtl_ *)(p))->list.f1A99C[8]) = 1;
    (*(class Sel3838 *)&((BrUiCtl_ *)(p))->list).s5(0x200000, &DAT_100aac78, 5, 0, -1);
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn14) = (CtlFn)BrMenuSetAA28A8;
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn18) = (CtlFn)BrMenuClearAA28A8;
    for (i = 0; i < g_brStages[(*(char *)&DAT_10ac5c10)].f04; i++) {
        strcpy(szItem, BrStrGet(0x37));
        strcat(szItem, &(g_strA[0]));
        strcat(szItem, _strupr(_itoa(i + 1, szNum, 10)));
        (*(class Sel3838 *)&((BrUiCtl_ *)(p))->list).s4(szItem, 1, 1, &(*(char *)&g_hot0), 1);
    }
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;
    (*(short *)&((BrUiPage_ *)(cont))->cSel) += 1;
    p = new BrCtl;
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, (*(float *)&((BrUiPage_ *)(cont))->fX), (*(float *)&((BrUiPage_ *)(cont))->fY) - DAT_10077658, 0x102001, 2, 5, 1, -1);
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn0C) = (CtlFn)BrSub10047360;
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn08) = (CtlFn)CtlF540_fn;
    (*(unsigned short *)&((BrUiCtl_ *)(p))->w1E20C) = 3;
    p->s34((char *)(BrStrGet(0x1e)), 1, 1, (char *)(&(*(char *)&g_hot0)));
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;
    (*(short *)&((BrUiPage_ *)(cont))->cSel) += 1;
    p = new BrCtl;
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, (*(float *)&((BrUiPage_ *)(cont))->fX), (*(float *)&((BrUiPage_ *)(cont))->fY) - DAT_1007765c, 0x102001, 2, 5, 1, -1);
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn0C) = (CtlFn)BrSub10047360;
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn08) = (CtlFn)BrOpt70A0;
    (*(unsigned short *)&((BrUiCtl_ *)(p))->w1E20C) = 3;
    p->s34((char *)(BrStrGet(0xc)), 1, 1, (char *)(&(*(char *)&g_hot0)));
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;
    (*(short *)&((BrUiPage_ *)(cont))->cSel) += 1;
    fx = (float)(*(int *)&g_hot2);
    fy = (float)(*(int *)((char *)&g_hot2 + 0x4));
    p = new BrCtl;
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, fx, fy, 0x402001, 2, 5, 1, 0x78);
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn0C) = (CtlFn)BrSub10047360;
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn08) = (CtlFn)BrHook_10045860;
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
    p = new BrCtl;
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, fx, fy, 0x402001, 2, 5, 1, 0x52);
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn0C) = (CtlFn)BrSub10047360;
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn08) = (CtlFn)BrOpt3FA0;
    yi = (int)fy;
    fy = fy - DAT_10077664;
    (*(int *)&((BrUiCtl_ *)(p))->rcTop) = yi;
    (*(int *)&((BrUiCtl_ *)(p))->rcLeft) = xi;
    (*(int *)&((BrUiCtl_ *)(p))->rcRight) = xi + 0x7f;
    (*(int *)&((BrUiCtl_ *)(p))->rcBottom) = yi + 0x21;
    (*(int *)&((BrUiCtl_ *)(p))->f2968) = 0;
    (*(short *)&((BrUiCtl_ *)(p))->aStepId[1]) = 0x53;
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;
    p = new BrCtl;
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, fx, fy, 0x402001, 2, 5, 1, 0x54);
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn0C) = (CtlFn)BrSub10047360;
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn08) = (CtlFn)BrHook_10045840;
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
    p->s38(parent, 106.0f, 85.0f, 0x100001, 2, 5, 1, -1);
    (*(unsigned short *)&((BrUiCtl_ *)(p))->w1E20C) = 3;
    p->s34((char *)(BrStrGet(0x38)), 1, 1, (char *)(&DAT_100aabf8));
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;
    p = new BrCtl;
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 440.0f, 66.0f, 0x5001, 2, 5, 1, -1);
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn04) = (CtlFn)BrMenuText1300;
    (*(unsigned short *)&((BrUiCtl_ *)(p))->w1E20C) = 0x34;
    p->s34((char *)(&(g_aBr39B720[0])), 1, 4, (char *)(&DAT_100aabf8));
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;
    p = new BrCtl;
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 106.0f, 123.0f, 0x100001, 2, 5, 1, -1);
    (*(unsigned short *)&((BrUiCtl_ *)(p))->w1E20C) = 3;
    p->s34((char *)(BrStrGet(0x36)), 1, 1, (char *)(&DAT_100aabf8));
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;
    p = new BrCtl;
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 440.0f, 104.0f, 0x5001, 2, 5, 1, -1);
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn04) = (CtlFn)BrItemSetTitle_1003A910;
    (*(unsigned short *)&((BrUiCtl_ *)(p))->w1E20C) = 0x34;
    p->s34((char *)(&(g_aBr39B720[0])), 1, 4, (char *)(&DAT_100aabf8));
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;
    p = new BrCtl;
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 106.0f, 185.0f, 0x100001, 2, 5, 1, -1);
    (*(unsigned short *)&((BrUiCtl_ *)(p))->w1E20C) = 3;
    p->s34((char *)(BrStrGet(0x4c)), 1, 1, (char *)(&DAT_100aac98));
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;
    p = new BrCtl;
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 440.0f, 141.0f, 0x5001, 2, 5, 1, -1);
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn04) = (CtlFn)BrItemSetNumByte_10037EF0;
    (*(unsigned short *)&((BrUiCtl_ *)(p))->w1E20C) = 5;
    p->s34((char *)(&(g_aBr39B720[0])), 1, 3, (char *)(&DAT_100aac98));
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;
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
    p = new BrCtl;
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, (*(float *)&((BrUiPage_ *)(cont))->fX), 258.0f, 0x100009, 2, 5, 1, -1);
    (*(unsigned short *)&((BrUiCtl_ *)(p))->w1E20C) = 3;
    p->s34((char *)(BrStrGet(0x40)), 1, 1, (char *)(&DAT_100aac18));
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;
    p = new BrCtl;
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 450.0f, 211.0f, 0x5001, 2, 5, 1, -1);
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn04) = (CtlFn)BrItemSetNumWord_100380B0;
    (*(unsigned short *)&((BrUiCtl_ *)(p))->w1E20C) = 5;
    p->s34((char *)(&(g_aBr39B720[0])), 1, 3, (char *)(&DAT_100aac18));
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;

    return 1;
}
