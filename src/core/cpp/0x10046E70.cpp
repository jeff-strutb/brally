/* WHAT IT DOES: build one menu page: creates the page container, adds every
 * control on it in turn, and reports failure if any of them could not be
 * made. One of a family of page builders, each laying out its own screen. */
/* @implements 0x10046E70 glide BrExt_1004DFC0
 * @cpp_kind free
 * @cpp_symbol ?BrExt_1004DFC0@@YAHPAVGameUi@@@Z
 *
 * 2114 B cdecl EH-frame menu builder, same family and recipe as
 * 0x10048160 / 0x100458D0 / 0x100451F0 / 0x10048F10. Twelve `new BrCtl`
 * entries around one dropdown row that is the only thing in this family
 * needing the ASM rather than the Ghidra draft -- the draft invents a
 * bare `ftol()` call and loses which value feeds which store.
 *
 * What that row actually does:
 *   - clamp the saved index to 0..11 and hand it to the selector's
 *     +0x14 configure vcall;
 *   - fill the list from the twelve name pointers at 0x100B81D0, walked
 *     as POINTERS against the end address (the original compares
 *     `edi < 0x100B8200`), through the selector's +0x10 add-item slot;
 *   - then set +0x1E1E8 three ways off the SAME index, re-read from the
 *     global after the loop: the low bound below 0, the high bound above
 *     11, and otherwise `lo - (hi - lo) * (float)n * k`; and finally
 *     truncate that into +0x1E1C8 with +0x1E1D0 = it + 0x10.
 *
 * Family levers unchanged: CHAR bool after the slot store, raw float
 * pushes for simple lvalues, w14 then w344 tails.
 *
 * Two shapes in that row were recovered from the asm and both matter:
 *  - the CLAMP is one load then an in-place fix (`k = g; if (k >= 0) {
 *    if (k > 11) k = 11; } else k = 0;`). Re-reading the global in each
 *    arm, or naming a separate result, moves it out of the eax
 *    accumulator form and costs 26 diffs.
 *  - the loop's bound test is SIGNED on the pointer (`jl`, i.e. the
 *    original compares it as an int), not the unsigned `jb` a plain
 *    pointer comparison gives.
 *
 * PARKED at 1177 diffs. The first 675 bytes -- the page prologue, three
 * entries, the clamp and the configure vcall -- are byte-exact; from
 * there it is ONE constant-register fork carried forward. The original
 * pushes the -1 arguments as immediates (`6a ff`) and keeps the
 * selector's vptr in edi across the configure call and the add-item slot
 * read; ours promotes -1 into edi instead, so the vptr has to go to edx
 * and then spill to a stack slot, and every later entry inherits the
 * rotation. Same class as the pinned-zero register in 0x10054E20 -- a
 * constant VC5 chose to keep in a register where the original did not.
 * DO NOT RE-PROBE the clamp or the arm order; those are settled above.
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

typedef char chk_sel3838[sizeof(Sel3838) == 0x18 ? 1 : -1];


class Page46E70 {
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
    Page46E70();
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
    char pad10[4];                  /* +0x10 */
    int (*pfn14)(BrCtl *);          /* +0x14 */
    char pad18[0x2AB4 - 0x18];      /* +0x18 */
    short w2AB4;                    /* +0x2AB4 */
    short w2AB6[0x19];              /* +0x2AB6 */
    char pad2AE8[0x3838 - 0x2AE8];  /* +0x2AE8 */
    Sel3838 m3838;                  /* +0x3838 */
    char pad3850[0x1E1C8 - 0x3850]; /* +0x3850 */
    int   f1E1C8;                   /* +0x1E1C8 */
    char  pad1E1CC[0x1E1D0 - 0x1E1CC];
    int   f1E1D0;                   /* +0x1E1D0 */
    char  pad1E1D4[0x1E1E8 - 0x1E1D4];
    float f1E1E8;                   /* +0x1E1E8 */
    char  pad1E1EC[0x1E1F4 - 0x1E1EC];
    int   f1E1F4;                   /* +0x1E1F4 */
    char  pad1E1F8[0x1E200 - 0x1E1F8];
    float f1E200;                   /* +0x1E200 */
    float f1E204;                   /* +0x1E204 */
    char  pad1E208[0x1E20C - 0x1E208];
    unsigned short w1E20C;          /* +0x1E20C */
    char pad1E20E[6];               /* +0x1E20E */
    BrCtl();
};

class GameUi {
public:
    char pad[0x10];
    unsigned short w10;             /* +0x10 */
    short w12;                      /* +0x12 */
    Page46E70 *a14[22];              /* +0x14 */
    int a6C[1];                     /* +0x6C */
};

typedef char chk_page46E70[sizeof(Page46E70) == 0x348 ? 1 : -1];
typedef char chk_ctl46E70[sizeof(BrCtl) == 0x1E214 ? 1 : -1];
typedef char chk_sel_off[(unsigned)&((BrCtl *)0)->m3838 == 0x3838 ? 1 : -1];
typedef char chk_f1e1f4[(unsigned)&((BrCtl *)0)->f1E1F4 == 0x1E1F4 ? 1 : -1];
typedef char chk_w1e20c46E70[(unsigned)&((BrCtl *)0)->w1E20C == 0x1E20C ? 1 : -1];
typedef char chk_a6c46E70[(unsigned)&((GameUi *)0)->a6C == 0x6C ? 1 : -1];

typedef int (*CtlFn)(BrCtl *);

extern "C" {
extern char  DAT_100aabe8;
extern char  DAT_100aac08;
extern char  DAT_100aac78;
extern char  DAT_100aaca8;
extern char  DAT_100aace8;
extern float DAT_10077654;
extern float DAT_10077658;
extern float DAT_1007765c;
extern float DAT_1007766c;
int    g_brSel5D8C;             /* 0x10AC5D8C */
BrCtl *g_AA29C8;
char  *g_aBrNames0B81D0[12];    /* 0x100B81D0 .. 0x100B8200 */
char *BrStrGet(int);
void FUN_100378c0(int);
int BrSub10047360();
int BrPhaseLeave_100466C0();
int BrUiHook85_1003E950();
int BrUiHook85_1003E9E0();
int BrUiHook85_1003EA40();
int BrUiHook85_1003EE20();
int FUN_100476C0();
int FUN_1003c240();
int FUN_1003c2b0();
int FUN_10037fa0();
}

int BrExt_1004DFC0(GameUi *parent)
{
    Page46E70 *cont;
    BrCtl     *p;
    char       bad;

    parent->w12 = 0;
    parent->a6C[parent->w10] = 1;

    cont = new Page46E70;
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
    p->s34(BrStrGet(0x21), 1, 1, &DAT_100aaca8);
    cont->w14 += 1;

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
    p->s38(parent, cont->f338, cont->f33C, 0x3001, 2, 5, 1, -1);
    p->pfn04 = (CtlFn)BrUiHook85_1003EE20;
    p->f1E1F4 = 1;
    {
        int k = g_brSel5D8C;

        if (k >= 0) {
            if (k > 11)
                k = 11;
        } else {
            k = 0;
        }

        p->m3838.s5(0x40001, &DAT_100aace8, 4, k, -1);
    }
    p->m3838.pfn04 = (int (*)(void))FUN_100476C0;
    {
        char **pp = g_aBrNames0B81D0;

        do {
            p->m3838.s4(*pp, 0, 1, &DAT_100aac78, 1);
            pp++;
        } while ((int)pp < (int)&g_aBrNames0B81D0[12]);
    }
    if (g_brSel5D8C < 0)
        p->f1E1E8 = p->f1E200;
    else if (g_brSel5D8C > 11)
        p->f1E1E8 = p->f1E204;
    else
        p->f1E1E8 = p->f1E200
                  - (p->f1E204 - p->f1E200) * g_brSel5D8C * DAT_1007766c;
    p->f1E1C8 = (int)p->f1E1E8;
    p->f1E1D0 = p->f1E1C8 + 0x10;
    cont->w14 += 1;
    cont->w344 += 1;

    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, cont->f338, cont->f33C - DAT_10077654, 0x102001, 2, 5, 1, -1);
    p->pfn0C = (CtlFn)BrSub10047360;
    p->pfn08 = (CtlFn)FUN_1003c240;
    p->w1E20C = 3;
    p->s34(BrStrGet(0x2E), 1, 1, &DAT_100aabe8);
    cont->w14 += 1;
    cont->w344 += 1;

    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, cont->f338, cont->f33C - DAT_10077658, 0x102001, 2, 5, 1, -1);
    p->pfn0C = (CtlFn)BrSub10047360;
    p->pfn08 = (CtlFn)FUN_1003c2b0;
    p->w1E20C = 3;
    p->s34(BrStrGet(0x2F), 1, 1, &DAT_100aabe8);
    cont->w14 += 1;
    cont->w344 += 1;

    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, cont->f338, cont->f33C - DAT_1007765c, 0x102001, 2, 5, 1, -1);
    p->pfn0C = (CtlFn)BrSub10047360;
    p->pfn08 = (CtlFn)BrPhaseLeave_100466C0;
    p->w1E20C = 3;
    p->s34(BrStrGet(0x0C), 1, 1, &DAT_100aabe8);
    g_AA29C8 = p;
    cont->w14 += 1;
    cont->w344 += 1;

    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 76.0f, 211.0f, 1, 2, 5, 1, 0x68);
    p->pfn04 = (CtlFn)BrUiHook85_1003E950;
    cont->w14 += 1;

    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 75.0f, 267.0f, 9, 2, 5, 1, 0x6A);
    cont->w14 += 1;

    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 79.0f, 256.0f, 1, 2, 5, 1, 0x6B);
    p->pfn04 = (CtlFn)BrUiHook85_1003EA40;
    cont->w14 += 1;

    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 339.0f, 90.0f, 0x102001, 2, 5, 1, -1);
    p->pfn04 = (CtlFn)FUN_10037fa0;
    p->w1E20C = 3;
    p->s34(BrStrGet(0x2E), 1, 1, &DAT_100aac08);
    cont->w14 += 1;

    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 339.0f, 128.0f, 0x102001, 2, 5, 1, -1);
    p->pfn04 = (CtlFn)BrUiHook85_1003E9E0;
    p->w1E20C = 3;
    p->s34(BrStrGet(0x2F), 1, 1, &DAT_100aac08);
    cont->w14 += 1;

    return 1;
}
