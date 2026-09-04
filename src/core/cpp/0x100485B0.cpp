/* WHAT IT DOES: build one menu page: creates the page container, adds every
 * control on it in turn, and reports failure if any of them could not be
 * made. One of a family of page builders, each laying out its own screen,
 * and it loads the season data file and checks whether the save file exists
 * before laying out, so the page can show what is there. */
/* @implements 0x100485b0 glide BrExt_1004F700
 * @cpp_kind free
 * @cpp_symbol ?BrExt_1004F700@@YAHPAVGameUi@@@Z
 *
 * 2389 B cdecl EH-frame menu-page builder. Scaffolded by
 * tools/gen_menubuilder.py from the Ghidra draft; the class layouts and the
 * three family levers come from the hand-solved 0x100425E0 / 0x10048160
 * (char bool after the slot store, raw float pushes for simple lvalues,
 * w14-then-w344 tails).
 *
 * The one hand-read part is the "is there a save file" probe. Ghidra splits
 * its flag into two temps because VC5 stores the 1 before the `fopen` call
 * and reuses the returned NULL as the 0; it is ONE source variable, read
 * twice afterwards -- once as the ternary that picks the control's flag
 * word (`neg/sbb/and -16/add` is `exists ? 0x102001 : 0x102011`, a
 * branchless ternary, not two calls), and once as a two-arm `if` that sets
 * a different +0x1E20C and a different third argument. Write both arms out
 * in full: the original's single shared `BrStrGet(0x35)` and `push 1` are
 * VC5 tail-merging them, not the source sharing them.
 */
#ifdef BR_MATCHING_BUILD
#include <stdio.h>
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

class SlotTable0485B0 {
public:
    virtual void s0();
    virtual void s1(char *pszPattern);   /* +0x04 */
    char aRecs[26000];                   /* +0x04, 0x104-byte records */
};

class Root0485B0 {
public:
    char            pad000[0xC0];
    SlotTable0485B0 *pTable;         /* +0xC0 */
    SlotTable0485B0 *pTableC4;       /* +0xC4 */
};


class Page0485B0 {
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
    Page0485B0();
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
    Page0485B0 *a14[22];              /* +0x14 */
    int a6C[1];                     /* +0x6C */
};

typedef char chk_page0485B0[sizeof(Page0485B0) == 0x348 ? 1 : -1];
typedef char chk_ctl0485B0[sizeof(BrCtl) == 0x1E214 ? 1 : -1];
typedef char chk_sel_off[(unsigned)&((BrCtl *)0)->m3838 == 0x3838 ? 1 : -1];
typedef char chk_f1e1f4[(unsigned)&((BrCtl *)0)->f1E1F4 == 0x1E1F4 ? 1 : -1];
typedef char chk_w1e20c0485B0[(unsigned)&((BrCtl *)0)->w1E20C == 0x1E20C ? 1 : -1];
typedef char chk_a6c0485B0[(unsigned)&((GameUi *)0)->a6C == 0x6C ? 1 : -1];
typedef char chk_tbl0485B0[(unsigned)&((Root0485B0 *)0)->pTable == 0xC0 ? 1 : -1];

typedef int (*CtlFn)(BrCtl *);

extern "C" {
int DAT_10ac5ba0;
int g_i0AB3F4;
Root0485B0 *g_brRoot5C60;   /* 0x10AC5C60 */
extern float DAT_10077658;
extern char  s_RallySeason_BRF[];
extern char  s_AutoSave_brf[];
extern char  DAT_100ac9c8;
int BrMenuFlags1890();
int BrMenuText0A50();
int BrMenuText0AC0();
int BrMenuText1300();
int BrPhaseHook_10045090();
int BrPhaseHook_100450C0();
int BrPhaseLeaveNamed_10046E10();
int BrSub10047360();
int BrUiPoll1003EAE0();
extern char  DAT_100aabe8;
extern char  DAT_100aac08;
extern char  DAT_100aac18;
extern char  DAT_100aac28;
extern char  DAT_100aac78;
extern char  DAT_100aaca8;
extern char  DAT_100aacd8;
extern char  DAT_10396f08;
int FUN_1003b6d0();
extern float DAT_10077654;
extern float DAT_1007765c;
char *BrStrGet(int);
void FUN_100378c0(int);
}

int BrExt_1004F700(GameUi *parent)
{
    Page0485B0 *cont;
    BrCtl     *p;
    char       bad;

    FILE      *fp;
    int        exists;

    parent->w12 = 0;
    g_i0AB3F4 = 0xffffffff;
    DAT_10ac5ba0 = 1;
    g_brRoot5C60->pTable->s1(s_RallySeason_BRF);
    DAT_10ac5ba0 = 0;
    parent->a6C[parent->w10] = 1;
    cont = new Page0485B0;
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
    p->s34(BrStrGet(0x34), 1, 1, &DAT_100aaca8);
    cont->w14 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, cont->f338, cont->f33C, 0x3001, 2, 5, 1, -1);
    p->pfn04 = (CtlFn)BrUiPoll1003EAE0;
    p->f1E1F4 = 1;
    p->m3838.s5(0x40001, &DAT_100aacd8, 4, 0, -1);
    p->m3838.pfn04 = (int (*)(void))FUN_1003b6d0;
    {
        int off = 0;

        do {
            char *psz = &g_brRoot5C60->pTable->aRecs[off];

            if (psz != 0)
                p->m3838.s4(psz, 0, 1, &DAT_100aac78, 1);
            off += 0x104;
        } while (off < 26000);
    }
    cont->w14 += 1;
    cont->w344 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, cont->f338, cont->f33C - DAT_10077654, 0x103011, 2, 5, 1, -1);
    p->pfn0C = (CtlFn)BrSub10047360;
    p->pfn08 = (CtlFn)BrPhaseHook_10045090;
    p->pfn04 = (CtlFn)BrMenuFlags1890;
    p->w1E20C = 2;
    p->s34(BrStrGet(0x1e), 1, 0, &DAT_100aabe8);
    cont->w14 += 1;
    cont->w344 += 1;
    exists = 1;
    fp = fopen(s_AutoSave_brf, &DAT_100ac9c8);
    if (fp == 0)
        exists = 0;
    else
        fclose(fp);
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, cont->f338, cont->f33C - DAT_10077658,
           exists ? 0x102001 : 0x102011, 2, 5, 1, -1);
    p->pfn0C = (CtlFn)BrSub10047360;
    p->pfn08 = (CtlFn)BrPhaseHook_100450C0;
    if (exists) {
        p->w1E20C = 3;
        p->s34(BrStrGet(0x35), 1, 1, &DAT_100aabe8);
    } else {
        p->w1E20C = 2;
        p->s34(BrStrGet(0x35), 1, 0, &DAT_100aabe8);
    }
    cont->w14 += 1;
    cont->w344 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, cont->f338, cont->f33C - DAT_1007765c, 0x102001, 2, 5, 1, -1);
    p->pfn0C = (CtlFn)BrSub10047360;
    p->pfn08 = (CtlFn)BrPhaseLeaveNamed_10046E10;
    p->w1E20C = 3;
    p->s34(BrStrGet(0xc), 1, 1, &DAT_100aabe8);
    cont->w14 += 1;
    cont->w344 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 80.0f, 46.0f, 9, 2, 5, 0, 6);
    cont->w14 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 330.0f, 153.0f, 0x100009, 2, 5, 1, -1);
    p->w1E20C = 3;
    p->s34(BrStrGet(0x36), 1, 1, &DAT_100aac08);
    cont->w14 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 330.0f, 97.0f, 0x5001, 2, 5, 1, -1);
    p->pfn04 = (CtlFn)BrMenuText0A50;
    p->w1E20C = 5;
    p->s34(&DAT_10396f08, 1, 3, &DAT_100aac08);
    cont->w14 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 440.0f, 181.0f, 0x100009, 2, 5, 1, -1);
    p->w1E20C = 3;
    p->s34(BrStrGet(0x37), 1, 1, &DAT_100aac28);
    cont->w14 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 440.0f, 129.0f, 0x5001, 2, 5, 1, -1);
    p->pfn04 = (CtlFn)BrMenuText0AC0;
    p->w1E20C = 5;
    p->s34(&DAT_10396f08, 1, 3, &DAT_100aac28);
    cont->w14 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 440.0f, 243.0f, 0x100009, 2, 5, 1, -1);
    p->w1E20C = 3;
    p->s34(BrStrGet(0x38), 1, 1, &DAT_100aac18);
    cont->w14 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 440.0f, 224.0f, 0x5001, 2, 5, 1, -1);
    p->pfn04 = (CtlFn)BrMenuText1300;
    p->w1E20C = 0x34;
    p->s34(&DAT_10396f08, 1, 4, &DAT_100aac18);
    cont->w14 += 1;

    return 1;
}
