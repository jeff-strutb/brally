/* @implements 0x10045ef0 glide BrOptFn1004CAC0
 * @cpp_kind free
 * @cpp_symbol ?BrOptFn1004CAC0@@YAHPAVGameUi@@@Z
 *
 * 1834 B cdecl EH-frame menu-page builder. Scaffolded by
 * tools/gen_menubuilder.py from the Ghidra draft; the class layouts and the
 * three family levers come from the hand-solved 0x100425E0 / 0x10048160
 * (char bool after the slot store, raw float pushes for simple lvalues,
 * w14-then-w344 tails).
 *
 * The one part Ghidra renders as noise is the selector fill loop, and it
 * carries a lever worth keeping: the bound compare is `jl`, SIGNED, so the
 * source compares the cursor as an int, not as a pointer. Written as a
 * plain `pe < end` pointer compare the whole function is byte-exact except
 * for that single `jb`. Everything else about the loop is ordinary source
 * -- an indexed walk of a 21-entry table, `flags` reset each iteration,
 * `BrStrGet` called twice (once to test, once as the argument), and the
 * selector's vtable and `this` cached by VC5 across the whole loop from the
 * +0x14 configure call that precedes it.
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

class SlotTable045EF0 {
public:
    virtual void s0();
    virtual void s1(char *pszPattern);   /* +0x04 */
    char aRecs[26000];                   /* +0x04, 0x104-byte records */
};

class Root045EF0 {
public:
    char            pad000[0xC0];
    SlotTable045EF0 *pTable;         /* +0xC0 */
    SlotTable045EF0 *pTableC4;       /* +0xC4 */
};


class Page045EF0 {
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
    Page045EF0();
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
    Page045EF0 *a14[22];              /* +0x14 */
    int a6C[1];                     /* +0x6C */
};

typedef char chk_page045EF0[sizeof(Page045EF0) == 0x348 ? 1 : -1];
typedef char chk_ctl045EF0[sizeof(BrCtl) == 0x1E214 ? 1 : -1];
typedef char chk_sel_off[(unsigned)&((BrCtl *)0)->m3838 == 0x3838 ? 1 : -1];
typedef char chk_f1e1f4[(unsigned)&((BrCtl *)0)->f1E1F4 == 0x1E1F4 ? 1 : -1];
typedef char chk_w1e20c045EF0[(unsigned)&((BrCtl *)0)->w1E20C == 0x1E20C ? 1 : -1];
typedef char chk_a6c045EF0[(unsigned)&((GameUi *)0)->a6C == 0x6C ? 1 : -1];
typedef char chk_tbl045EF0[(unsigned)&((Root045EF0 *)0)->pTable == 0xC0 ? 1 : -1];

typedef int (*CtlFn)(BrCtl *);

/* The string table the selector is filled from: 21 eight-byte records at
 * 0x100AAAD0, of which only the leading string id is read here. The
 * original's `lea`-free pointer walk (`add edi,8` against an absolute end
 * address) is strength reduction, not the source shape. */
struct KeyEnt {
    int id;
    int f04;
};

extern "C" {
int DAT_10ac5d64;
int DAT_10ac5ba8;
int DAT_100abcc0[];
int BrCfgFindConflicts(int);
extern KeyEnt g_aKeyEnt0AAAD0[21];
int DAT_10ac5b98;
BrCtl *g_AA29C8;
int BrMenuCap1870();
int BrMenuEnter();
int BrPhaseLeave_10046560();
int BrSub10047360();
int BrUiPoll1003EC80();
int BrUiText1003F8D0();
extern char  DAT_100aabe8;
extern char  DAT_100aac28;
extern char  DAT_100aac68;
extern char  DAT_100aac78;
extern char  DAT_100aaca8;
extern char  DAT_100acad8;
int FUN_10039620();
int FUN_10039990();
extern float DAT_10077658;
extern float DAT_1007765c;
char *BrStrGet(int);
void FUN_100378c0(int);
}

int BrOptFn1004CAC0(GameUi *parent)
{
    Page045EF0 *cont;
    KeyEnt    *pe;
    int        flags;
    BrCtl     *p;
    char       bad;

    parent->w12 = 0;
    parent->a6C[parent->w10] = 1;
    cont = new Page045EF0;
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
    p->s34(BrStrGet(0x26), 1, 1, &DAT_100aaca8);
    cont->w14 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, cont->f338, cont->f33C, 0x3001, 2, 5, 1, -1);
    p->pfn04 = (CtlFn)BrUiPoll1003EC80;
    p->f1E1F4 = 1;
    p->m3838.s5(0x40001, &DAT_100aac78, 5, 0, -1);
    for (pe = g_aKeyEnt0AAAD0; (int)pe < (int)(g_aKeyEnt0AAAD0 + 21); pe++) {
        flags = 0;
        if (DAT_10ac5d64 == 3
            && (pe == &g_aKeyEnt0AAAD0[0] || pe == &g_aKeyEnt0AAAD0[1])) {
            flags = 0x10;
            DAT_10ac5b98 = 2;
        }
        if (BrStrGet(pe->id) != 0)
            p->m3838.s4(BrStrGet(pe->id), flags, 1, &DAT_100aac78, 0);
    }
    cont->w14 += 1;
    cont->w344 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, cont->f338, cont->f33C - DAT_10077658, 0x102001, 2, 5, 1, -1);
    p->pfn0C = (CtlFn)BrSub10047360;
    p->pfn08 = (CtlFn)BrMenuEnter;
    p->pfn18 = (CtlFn)FUN_10039990;
    p->w1E20C = 3;
    p->s34(BrStrGet(0x27), 1, 1, &DAT_100aabe8);
    cont->w14 += 1;
    cont->w344 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, cont->f338, cont->f33C - DAT_1007765c, 0x102001, 2, 5, 1, -1);
    p->pfn0C = (CtlFn)BrSub10047360;
    p->pfn08 = (CtlFn)BrPhaseLeave_10046560;
    p->w1E20C = 3;
    p->s34(BrStrGet(0xc), 1, 1, &DAT_100aabe8);
    g_AA29C8 = p;
    cont->w14 += 1;
    cont->w344 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 80.0f, 46.0f, 9, 2, 5, 0, 9);
    cont->w14 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, cont->f338, 135.0f, 0x101001, 2, 5, 1, -1);
    p->pfn04 = (CtlFn)BrUiText1003F8D0;
    p->w1E20C = 3;
    p->s34(&DAT_100acad8, 1, 1, &DAT_100aac28);
    cont->w14 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 324.0f, 46.0f, 1, 2, 5, 1, 0x16);
    p->pfn04 = (CtlFn)BrMenuCap1870;
    p->w2AB6[0] = (short)(cont->w14 + 1);
    p->w2AB4 += 1;
    cont->w14 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, cont->f338, 155.0f, 0x100001, 2, 5, 1, -1);
    p->w1E20C = 3;
    p->s34(BrStrGet(0x28), 1, 1, &DAT_100aac68);
    cont->w14 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, cont->f338, 100.0f, 0x101001, 2, 5, 1, -1);
    p->pfn04 = (CtlFn)FUN_10039620;
    p->w1E20C = 3;
    p->s34(&DAT_100acad8, 1, 1, &DAT_100aac68);
    cont->w14 += 1;
    DAT_10ac5ba8 = BrCfgFindConflicts(DAT_100abcc0[DAT_10ac5d64]);

    return 1;
}
