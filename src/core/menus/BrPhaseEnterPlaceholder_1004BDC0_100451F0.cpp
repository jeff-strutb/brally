/* WHAT IT DOES: build one menu page: creates the page container, adds every
 * control on it in turn, and reports failure if any of them could not be
 * made. One of a family of page builders, each laying out its own screen. */
/* @implements 0x100451F0 glide BrPhaseEnterPlaceholder_1004BDC0
 * @cpp_kind free
 * @cpp_symbol ?BrPhaseEnterPlaceholder_1004BDC0@@YAHPAVGameUi@@@Z
 *
 * 1749 B cdecl EH-frame menu-page builder, same family as 0x10048160 and
 * 0x100425E0: `new Page451F0` (0x348, ctor 0x10048470) then ten
 * `new BrCtl` (0x1E214, ctor 0x10040B10) blocks, each its own /GX EH
 * state and null-checked into the Err(4) shell.
 *
 * The family's levers, unchanged:
 *  (1) the null check is a CHAR bool computed AFTER the slot store.
 *  (2) simple float lvalues push raw; only the computed y offsets
 *      (`f33C - k`) become fld/fsub/fstp.
 *  (3) the sublink is the INLINE expression `(short)(cont->w14 + 1)` --
 *      a named short temp allocates to ax where the original uses dx --
 *      and the tail is w2AB6-store FIRST, w2AB4-inc second.
 *
 * Two entries here that 0x10048160 does not have: the caption row hooks
 * pfn04 and threads a sublink, and the last row passes a static string
 * straight to s34 instead of a catalogue lookup.
 *
 * Transcribed from the Ghidra draft.
 */
class GameUi;
class BrCtl;

class Page451F0 {
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
    Page451F0();
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
    char pad10[0x2AA4];             /* +0x10 */
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
    Page451F0 *a14[22];              /* +0x14 */
    int a6C[1];                     /* +0x6C */
};

typedef char chk_page451F0[sizeof(Page451F0) == 0x348 ? 1 : -1];
typedef char chk_ctl451F0[sizeof(BrCtl) == 0x1E214 ? 1 : -1];
typedef char chk_w1e20c451F0[(unsigned)&((BrCtl *)0)->w1E20C == 0x1E20C ? 1 : -1];
typedef char chk_a6c451F0[(unsigned)&((GameUi *)0)->a6C == 0x6C ? 1 : -1];



typedef int (*CtlFn)(BrCtl *);

extern "C" {
extern char  DAT_100aaca8;
extern char  DAT_100aabe8;
extern char  DAT_100aac68;
extern char  DAT_100acad8;
extern float DAT_10077648;
extern float DAT_1007764c;
extern float DAT_10077650;
extern float DAT_1007765c;
BrCtl *g_AA29C8;
char *BrStrGet(int);
void FUN_100378c0(int);
int BrSub10047360();
int BrPhaseActivate_100452C0();
int BrPhaseActivate_10045390();
int BrPhaseActivate_100455E0();
int BrPhaseActivate_100456B0();
int FUN_1003f9c0();
int BrMenuCap1870();
int BrUiText1003FFD0();
}

int BrPhaseEnterPlaceholder_1004BDC0(GameUi *parent)
{
    Page451F0 *cont;
    BrCtl     *p;
    char       bad;

    parent->w12 = 0;
    parent->a6C[parent->w10] = 1;

    cont = new Page451F0;
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
    p->s34(BrStrGet(6), 1, 1, &DAT_100aaca8);
    cont->w14 += 1;

    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, cont->f338, cont->f33C, 0x102001, 2, 5, 1, -1);
    p->pfn0C = (CtlFn)BrSub10047360;
    p->pfn08 = (CtlFn)BrPhaseActivate_100452C0;
    p->w1E20C = 3;
    p->s34(BrStrGet(0x1F), 1, 1, &DAT_100aabe8);
    cont->w14 += 1;
    cont->w344 += 1;

    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, cont->f338, cont->f33C - DAT_10077648, 0x102001, 2, 5, 1, -1);
    p->pfn0C = (CtlFn)BrSub10047360;
    p->pfn08 = (CtlFn)BrPhaseActivate_10045390;
    p->w1E20C = 3;
    p->s34(BrStrGet(0x20), 1, 1, &DAT_100aabe8);
    cont->w14 += 1;
    cont->w344 += 1;

    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, cont->f338, cont->f33C - DAT_1007764c, 0x102001, 2, 5, 1, -1);
    p->pfn0C = (CtlFn)BrSub10047360;
    p->pfn08 = (CtlFn)BrPhaseActivate_100455E0;
    p->w1E20C = 3;
    p->s34(BrStrGet(0x21), 1, 1, &DAT_100aabe8);
    cont->w14 += 1;
    cont->w344 += 1;

    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, cont->f338, cont->f33C - DAT_10077650, 0x102001, 2, 5, 1, -1);
    p->pfn0C = (CtlFn)BrSub10047360;
    p->pfn08 = (CtlFn)BrPhaseActivate_100456B0;
    p->w1E20C = 3;
    p->s34(BrStrGet(0x22), 1, 1, &DAT_100aabe8);
    cont->w14 += 1;
    cont->w344 += 1;

    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, cont->f338, cont->f33C - DAT_1007765c, 0x102001, 2, 5, 1, -1);
    p->pfn0C = (CtlFn)BrSub10047360;
    p->pfn08 = (CtlFn)FUN_1003f9c0;
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
    p->s38(parent, 80.0f, 46.0f, 9, 2, 5, 0, 9);
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
    p->s38(parent, cont->f338, 155.0f, 0x101001, 2, 5, 1, -1);
    p->pfn04 = (CtlFn)BrUiText1003FFD0;
    p->w1E20C = 3;
    p->s34(&DAT_100acad8, 1, 1, &DAT_100aac68);
    cont->w14 += 1;

    return 1;
}
