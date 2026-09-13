/* WHAT IT DOES: build one menu page: creates the page container, adds every
 * control on it in turn, and reports failure if any of them could not be
 * made. One of a family of page builders, each laying out its own screen,
 * positioning its controls from computed coordinates rather than a fixed
 * table. */
/* @t3 0x1004DA00 2026-09-12 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 3394/3394 insns 1002/1002 rows 3+3 regions 1 oracle UNCLASSIFIED
 * @t3-effort passes 2 zero-movement 1 2
 * Residue: photo1's ten-instruction Pentium-pairing schedule (identical
 * multiset; the 3+3 rows are the EH frame's fs:[0] reloc form). Dossier
 * below; dead list and schedule census in 0x1004AEE0.cpp.
 * Do not reopen before the end-grind (CLAUDE.md rule 12). */
/* @implements 0x1004da00 glide BrExt_10054B50
 * @cpp_kind free
 * @cpp_symbol ?BrExt_10054B50@@YAHPAVGameUi@@@Z
 *
 * 3394 B cdecl EH-frame menu-page builder. Emitted by
 * tools/gen_menubuilder.py from the Ghidra draft; the class
 * layouts and the three family levers come from the hand-solved
 * 0x100425E0 / 0x10048160 (char bool after the slot store, raw
 * float pushes for simple lvalues, w14-then-w344 tails).
 *
 * Residue: 34 diffs, ALL in photo1's ten-instruction tail -- the third
 * function with byte-identical residue there (0x1004AEE0, 0x1004BE00,
 * this one). The original issues [fld fy][yi reload + xi copy][fsub][lea +
 * add][f50 + f58][f5C + f2968][fstp][w2A42 + inc]; ours issues the fsub
 * right after the fld (the xi copy lands at its first use, so the fy
 * statement is IR-first) and the lea waits a cycle after the copy (AGI),
 * so f5C overtakes f58. Identical instruction multiset. The full dead list
 * and the pairing-schedule census (2026-09-12) are in 0x1004AEE0.cpp.
 *
 * @t4-pass 0x1004DA00 1 2026-09-12 probes 240 bytes 3394 insns 1002 regions 1 rows 6 census no  (generated: all 240 orders of {xi, fy, f50, f58, f5C, f2968}; best 32 = f50 before fy, none 0)
 * @t4-pass 0x1004DA00 2 2026-09-12 probes 27 bytes 3394 insns 1002 regions 1 rows 6 census yes  (27 compiler options incl. /Gi /Op /G5 /Ow /Ob2, all 34 or worse; corpus query MISS at +0x3d0 len 8; residue byte-identical to 0x1004AEE0's certified census)
 *
 * DEAD PROBE (2026-09-03, this file): moving `fy -= K` to sit between
 * `f2968` and `w2A42` -- the one slot the 0x1004AEE0 dead list left
 * ambiguous -- makes it WORSE, 34 -> 59. Every placement is now covered.
 * Do not re-probe the fy-update position.
 *
 * DEAD PROBE 2: naming the two derived ints (`x2 = xi + 0x7f; y2 = yi +
 * 0x21;`) BEFORE the three stores, which is literally the original's
 * instruction order (lea, add, store 50/58/5C), also stays at 34. VC5 folds
 * the temps back into the stores and re-schedules identically. The window
 * gates FIVE functions -- 0x1004AEE0, 0x1004BE00, 0x1004DA00, 0x100498A0,
 * 0x1004CBA0, 18,395 B -- so it is worth a fresh IDEA, but not another
 * permutation of this statement list.
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

class SlotTable04DA00 {
public:
    virtual void s0();
    virtual void s1(char *pszPattern);   /* +0x04 */
    char aRecs[26000];                   /* +0x04, 0x104-byte records */
};

class Root04DA00 {
public:
    char            pad000[0xC0];
    SlotTable04DA00 *pTable;         /* +0xC0 */
    SlotTable04DA00 *pTableC4;       /* +0xC4 */
};


class Page04DA00 {
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
    Page04DA00();
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
    Page04DA00 *a14[22];              /* +0x14 */
    int a6C[1];                     /* +0x6C */
};

typedef char chk_page04DA00[sizeof(Page04DA00) == 0x348 ? 1 : -1];
typedef char chk_ctl04DA00[sizeof(BrCtl) == 0x1E214 ? 1 : -1];
typedef char chk_sel_off[(unsigned)&((BrCtl *)0)->m3838 == 0x3838 ? 1 : -1];
typedef char chk_f1e1f4[(unsigned)&((BrCtl *)0)->f1E1F4 == 0x1E1F4 ? 1 : -1];
typedef char chk_w1e20c04DA00[(unsigned)&((BrCtl *)0)->w1E20C == 0x1E20C ? 1 : -1];
typedef char chk_a6c04DA00[(unsigned)&((GameUi *)0)->a6C == 0x6C ? 1 : -1];
typedef char chk_tbl04DA00[(unsigned)&((Root04DA00 *)0)->pTable == 0xC0 ? 1 : -1];

typedef int (*CtlFn)(BrCtl *);

extern "C" {
extern int DAT_100aabc8;
extern int DAT_100aabcc;
extern float DAT_10077664;
int BrMenuLatchPending();
int BrMenuText1300();
int BrMenuText1670();
int BrMenuText1710();
int BrMenuText17B0();
int BrOpt3FA0();
int BrPhaseLeave_10047290();
int BrSub10047360();
int BrUiHook81_10045880();
int BrUiHook81_100458A0();
int BrUiHook81_100470E0();
extern char  DAT_100aabe8;
extern char  DAT_100aabf8;
extern char  DAT_100aac08;
extern char  DAT_100aac18;
extern char  DAT_100aac98;
extern char  DAT_100aaca8;
extern char  DAT_10396f08;
int FUN_10039f60();
int FUN_1003a910();
extern float DAT_10077658;
extern float DAT_1007765c;
char *BrStrGet(int);
void FUN_100378c0(int);
}

int BrExt_10054B50(GameUi *parent)
{
    Page04DA00 *cont;
    BrCtl     *p;
    char       bad;
    float      fx, fy;
    int        xi, yi;

    parent->w12 = 0;
    parent->a6C[parent->w10] = 1;
    cont = new Page04DA00;
    parent->a14[parent->w10] = cont;
    bad = (cont == 0);
    if (bad)
        FUN_100378c0(4);
    parent->w10 += 1;
    cont->f340 = parent;
    cont->pfnA = (int (*)(void))BrMenuLatchPending;
    cont->pfnB = (int (*)(void))FUN_10039f60;
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
    p->s34(BrStrGet(0x4f), 1, 1, &DAT_100aaca8);
    cont->w14 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, cont->f338, cont->f33C - DAT_10077658, 0x102001, 2, 5, 1, -1);
    p->pfn0C = (CtlFn)BrSub10047360;
    p->pfn08 = (CtlFn)BrPhaseLeave_10047290;
    p->w1E20C = 3;
    p->s34(BrStrGet(0x50), 1, 1, &DAT_100aabe8);
    cont->w14 += 1;
    cont->w344 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, cont->f338, cont->f33C - DAT_1007765c, 0x102001, 2, 5, 1, -1);
    p->pfn0C = (CtlFn)BrSub10047360;
    p->pfn08 = (CtlFn)BrUiHook81_100470E0;
    p->w1E20C = 3;
    p->s34(BrStrGet(0xc), 1, 1, &DAT_100aabe8);
    cont->w14 += 1;
    cont->w344 += 1;
    fx = (float)DAT_100aabc8;
    fy = (float)DAT_100aabcc;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, fx, fy, 0x402001, 2, 5, 1, 0x78);
    p->pfn0C = (CtlFn)BrSub10047360;
    p->pfn08 = (CtlFn)BrUiHook81_100458A0;
    yi = (int)fy;
    p->f054 = yi;
    xi = (int)fx;
    fy = fy - DAT_10077664;
    p->f050 = xi;
    p->f058 = xi + 0x7f;
    p->f05C = yi + 0x21;
    p->f2968 = 0;
    p->w2A42 = 0x79;
    cont->w14 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, fx, fy, 0x402001, 2, 5, 1, 0x52);
    p->pfn0C = (CtlFn)BrSub10047360;
    p->pfn08 = (CtlFn)BrOpt3FA0;
    yi = (int)fy;
    fy = fy - DAT_10077664;
    p->f054 = yi;
    p->f050 = xi;
    p->f058 = xi + 0x7f;
    p->f05C = yi + 0x21;
    p->f2968 = 0;
    p->w2A42 = 0x53;
    cont->w14 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, fx, fy, 0x402001, 2, 5, 1, 0x54);
    p->pfn0C = (CtlFn)BrSub10047360;
    p->pfn08 = (CtlFn)BrUiHook81_10045880;
    yi = (int)fy;
    p->f054 = yi;
    p->f050 = xi;
    p->f058 = xi + 0x7f;
    p->f05C = yi + 0x21;
    p->f2968 = 0;
    p->w2A42 = 0x55;
    cont->w14 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 106.0f, 85.0f, 0x100001, 2, 5, 1, -1);
    p->w1E20C = 3;
    p->s34(BrStrGet(0x38), 1, 1, &DAT_100aabf8);
    cont->w14 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 440.0f, 66.0f, 0x5001, 2, 5, 1, -1);
    p->pfn04 = (CtlFn)BrMenuText1300;
    p->w1E20C = 0x34;
    p->s34(&DAT_10396f08, 1, 4, &DAT_100aabf8);
    cont->w14 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 106.0f, 123.0f, 0x100001, 2, 5, 1, -1);
    p->w1E20C = 3;
    p->s34(BrStrGet(0x36), 1, 1, &DAT_100aabf8);
    cont->w14 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 440.0f, 104.0f, 0x5001, 2, 5, 1, -1);
    p->pfn04 = (CtlFn)FUN_1003a910;
    p->w1E20C = 0x34;
    p->s34(&DAT_10396f08, 1, 4, &DAT_100aabf8);
    cont->w14 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 0, 70.0f, 0x100009, 2, 5, 1, -1);
    p->w1E20C = 3;
    p->s34(BrStrGet(0x52), 1, 1, &DAT_100aac08);
    cont->w14 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 330.0f, 97.0f, 0x5001, 2, 5, 1, -1);
    p->pfn04 = (CtlFn)BrMenuText1670;
    p->w1E20C = 5;
    p->s34(&DAT_10396f08, 1, 3, &DAT_100aac08);
    cont->w14 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 0, 150.0f, 0x100009, 2, 5, 1, -1);
    p->w1E20C = 3;
    p->s34(BrStrGet(0x53), 1, 1, &DAT_100aac08);
    cont->w14 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 450.0f, 125.0f, 0x100009, 2, 5, 1, -1);
    p->w1E20C = 3;
    p->s34(BrStrGet(0x40), 1, 1, &DAT_100aac98);
    cont->w14 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 450.0f, 185.0f, 0x100009, 2, 5, 1, -1);
    p->w1E20C = 3;
    p->s34(BrStrGet(0x46), 1, 1, &DAT_100aac98);
    cont->w14 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 450.0f, 141.0f, 0x5001, 2, 5, 1, -1);
    p->pfn04 = (CtlFn)BrMenuText17B0;
    p->w1E20C = 5;
    p->s34(&DAT_10396f08, 1, 3, &DAT_100aac98);
    cont->w14 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, cont->f338, 203.0f, 0x100009, 2, 5, 1, -1);
    p->w1E20C = 3;
    p->s34(BrStrGet(0x40), 1, 1, &DAT_100aac18);
    cont->w14 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, cont->f338, 265.0f, 0x100009, 2, 5, 1, -1);
    p->w1E20C = 3;
    p->s34(BrStrGet(0x41), 1, 1, &DAT_100aac18);
    cont->w14 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 450.0f, 217.0f, 0x5001, 2, 5, 1, -1);
    p->pfn04 = (CtlFn)BrMenuText1710;
    p->w1E20C = 5;
    p->s34(&DAT_10396f08, 1, 3, &DAT_100aac18);
    cont->w14 += 1;

    return 1;
}
