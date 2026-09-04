/* WHAT IT DOES: build one menu page: creates the page container, adds every
 * control on it in turn, and reports failure if any of them could not be
 * made. One of a family of page builders, each laying out its own screen,
 * positioning its controls from computed coordinates rather than a fixed
 * table. */
/* @implements 0x1004AEE0 glide BrExt_10052030
 * @cpp_kind method
 * @cpp_symbol ?BrExt_10052030@@YAHPAVGameUi@@@Z
 *
 * 3862 B cdecl EH-frame multiplayer/session menu builder — 0x100425E0
 * family (same layouts and levers). Photo trio is unconditional here:
 * xi lives in ebx across all three pages, xi+0x7f is spelled INLINE in
 * each (VC5's own CSE spills it across the news), yi is fresh per page, fy steps
 * down by DAT_10077664 in pages 1-2 only. Later pages use the
 * w1E20C=5/0x34 text forms s34(&DAT_10396f08, 1, 3|4, &buf).
 *
 * PARKED 2026-09-01 at 34 diffs (T3a scheduling): photo1's tail alone
 * diverges — orig emits [movs, fsub, lea, add, f50, f58, f5C, f2968,
 * fstp, w2A42]; every source permutation tried (14: fy-update in all 5
 * positions, compound/temp/inline spellings, x2/y2 temps, f58/f5C
 * swaps, /Op) canonicalizes to the same fsub-adjacent, f5C-early,
 * fstp-early schedule. Multiset is identical; photos 2-3 and all other
 * 3828 B match. Do not re-probe these spellings.
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

typedef char chk_cont[sizeof(Phase32F) == 0x348 ? 1 : -1];
typedef char chk_ctl[sizeof(BrCtl) == 0x1E214 ? 1 : -1];

extern "C" {
extern char DAT_100aaca8;
extern char DAT_100aabe8;
extern char DAT_100aabf8;
extern char DAT_100acad8;
extern char DAT_100aac18;
extern char DAT_100aac48;
extern char DAT_100aac58;
extern char DAT_100aac98;
extern char DAT_10396f08;
extern int DAT_100aabc8;
extern int DAT_100aabcc;
extern float DAT_10077648;
extern float DAT_10077658;
extern float DAT_1007765c;
extern float DAT_10077664;
extern float DAT_10077668;
char *BrStrGet(int);
void FUN_100378c0(int);
int BrSub10047360();
int BrPhaseNameClear_10047340();
int BrPhaseHook_10045050();
int BrUiHook84_10047060();
int BrUiHook84_100457E0();
int BrUiHook84_100457C0();
int BrOpt3FA0();
int BrMenuCap0730();
int BrMenuCap07E0();
int BrMenuText0B30();
int BrMenuText1300();
int BrMenuText15A0();
int FUN_100393c0();
int FUN_10038f40();
int FUN_1003aa10();
int FUN_1003a910();
}

typedef int (*CtlFn)(BrCtl *);

int BrExt_10052030(GameUi *parent)
{
    Phase32F *cont;
    BrCtl *p;
    char bad;
    float fx, fy;
    int xi, yi;

    parent->w12 = 0;
    parent->a6C[parent->w10] = 1;
    cont = new Phase32F;
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
    p->s34(BrStrGet(0x44), 1, 1, &DAT_100aaca8);
    cont->w14 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, cont->f338, cont->f33C - DAT_10077648, 0x102001, 2, 5, 1, -1);
    p->pfn0C = (CtlFn)BrSub10047360;
    p->pfn08 = (CtlFn)BrPhaseNameClear_10047340;
    p->w1E20C = 3;
    p->s34(BrStrGet(0x45), 1, 1, &DAT_100aabe8);
    cont->w14 += 1;
    cont->w344 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, cont->f338, cont->f33C - DAT_10077658, 0x102001, 2, 5, 1, -1);
    p->pfn0C = (CtlFn)BrSub10047360;
    p->pfn08 = (CtlFn)BrPhaseHook_10045050;
    p->w1E20C = 2;
    p->s34(BrStrGet(0x1e), 1, 0, &DAT_100aabe8);
    cont->w14 += 1;
    cont->w344 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, cont->f338, cont->f33C - DAT_1007765c, 0x102001, 2, 5, 1, -1);
    p->pfn0C = (CtlFn)BrSub10047360;
    p->pfn08 = (CtlFn)BrUiHook84_10047060;
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
    p->pfn08 = (CtlFn)BrUiHook84_100457E0;
    yi = (int)fy;
    p->f54 = yi;
    xi = (int)fx;
    fy = fy - DAT_10077664;
    p->f50 = xi;
    p->f58 = xi + 0x7f;
    p->f5C = yi + 0x21;
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
    p->f54 = yi;
    p->f50 = xi;
    p->f58 = xi + 0x7f;
    p->f5C = yi + 0x21;
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
    p->pfn08 = (CtlFn)BrUiHook84_100457C0;
    yi = (int)fy;
    p->f54 = yi;
    p->f50 = xi;
    p->f58 = xi + 0x7f;
    p->f5C = yi + 0x21;
    p->f2968 = 0;
    p->w2A42 = 0x55;
    cont->w14 += 1;
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
    p->pfn04 = (CtlFn)BrMenuText15A0;
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
    p->pfn04 = (CtlFn)FUN_1003aa10;
    p->w1E20C = 5;
    p->s34(&DAT_10396f08, 1, 3, &DAT_100aac18);
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
    p->s38(parent, cont->f338, cont->f33C - DAT_10077668, 0x5001, 2, 5, 1, -1);
    p->pfn04 = (CtlFn)BrMenuText0B30;
    p->w1E20C = 0x34;
    p->s34(&DAT_10396f08, 1, 4, &DAT_100aabe8);
    cont->w14 += 1;
    return 1;
}
