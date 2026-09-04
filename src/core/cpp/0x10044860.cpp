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
 * PARKED at 1952 diffs, but the structure is exact: slot-blind, the ONLY
 * instruction-level differences in 2439 bytes are four `fld`/`fadd` operand
 * orders. The original loads the LOCAL and adds the member
 * (`fld [esp+0x10]; fadd [esi+0x33c]`); VC5 gives us the reverse for every
 * spelling. DEAD PROBES, all leaving 1952 and the same operand order --
 * VC5 canonicalises commutative float addition, so do NOT re-spell it:
 *   - `fy + cont->f33C` and `cont->f33C + fy` (byte-identical output);
 *   - a named `ay = fy + cont->f33C;` temp passed to the call;
 *   - declaring `fy` first among the locals.
 * The knock-on is one stack-slot swap: the original keeps `fy` in the
 * dedicated `push ecx` local and spills the operator-new temp into the dead
 * parameter slot; ours does the opposite, and that 4-byte store-form
 * difference is what cascades into the raw diff count.
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

typedef char chk_page044860[sizeof(Page044860) == 0x348 ? 1 : -1];
typedef char chk_ctl044860[sizeof(BrCtl) == 0x1E214 ? 1 : -1];
typedef char chk_sel_off[(unsigned)&((BrCtl *)0)->m3838 == 0x3838 ? 1 : -1];
typedef char chk_f1e1f4[(unsigned)&((BrCtl *)0)->f1E1F4 == 0x1E1F4 ? 1 : -1];
typedef char chk_w1e20c044860[(unsigned)&((BrCtl *)0)->w1E20C == 0x1E20C ? 1 : -1];
typedef char chk_a6c044860[(unsigned)&((GameUi *)0)->a6C == 0x6C ? 1 : -1];
typedef char chk_tbl044860[(unsigned)&((Root044860 *)0)->pTable == 0xC0 ? 1 : -1];

typedef int (*CtlFn)(BrCtl *);

extern "C" {
extern int DAT_100abaa4;
extern float DAT_10077648;
extern float DAT_10077650;
BrCtl *DAT_10ac5d04;
int BrMenuCap0730();
int BrMenuCap07E0();
int BrMenuText08D0();
int BrOptCycleAA2A00();
int BrOptCycleBD3E0();
int BrPhaseActivate_10045110();
int BrPhaseTick_100474B0();
int BrSub10047360();
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
int FUN_1003f8f0();
extern float DAT_1007765c;
char *BrStrGet(int);
void FUN_100378c0(int);
}

int BrPhaseEnterPlaceholder_1004B430(GameUi *parent)
{
    float      fy;
    Page044860 *cont;
    BrCtl     *p;
    char       bad;

    parent->w12 = 0;
    fy = 0.0f;
    parent->a6C[parent->w10] = 1;
    cont = new Page044860;
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
    p->s34(BrStrGet(0x1a), 1, 1, &DAT_100aaca8);
    cont->w14 += 1;
    if (DAT_100abaa4 != 0) {
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
    fy = 19.0f;
    }
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, cont->f338, fy + cont->f33C, 0x102001, 2, 5, 1, -1);
    p->pfn0C = (CtlFn)BrSub10047360;
    p->pfn08 = (CtlFn)BrOptCycleAA2A00;
    p->w1E20C = 3;
    p->s34(BrStrGet(0x1c), 1, 1, &DAT_100aabe8);
    fy = fy - DAT_10077648;
    cont->w14 += 1;
    cont->w344 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, cont->f338, fy + cont->f33C, 0x102001, 2, 5, 1, -1);
    p->pfn0C = (CtlFn)BrSub10047360;
    p->pfn08 = (CtlFn)BrOptCycleBD3E0;
    p->w1E20C = 3;
    p->s34(BrStrGet(0x1d), 1, 1, &DAT_100aabe8);
    fy = fy - DAT_10077650;
    cont->w14 += 1;
    cont->w344 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, cont->f338, fy + cont->f33C, 0x102001, 2, 5, 1, -1);
    p->pfn0C = (CtlFn)BrPhaseTick_100474B0;
    p->pfn08 = (CtlFn)BrPhaseActivate_10045110;
    p->w1E20C = 3;
    p->s34(BrStrGet(0x1e), 1, 1, &DAT_100aabe8);
    fy = fy - DAT_10077648;
    cont->w14 += 1;
    cont->w344 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, cont->f338, fy + cont->f33C, 0x102001, 2, 5, 1, -1);
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, cont->f338, cont->f33C - DAT_1007765c, 0x102001, 2, 5, 1, -1);
    p->pfn0C = (CtlFn)BrSub10047360;
    p->pfn08 = (CtlFn)FUN_1003f8f0;
    p->w1E20C = 3;
    p->s34(BrStrGet(0xc), 1, 1, &DAT_100aabe8);
    DAT_10ac5d04 = p;
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
    if (DAT_100abaa4 != 0) {
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
    }
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

    return 1;
}
