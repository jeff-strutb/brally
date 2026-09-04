/* WHAT IT DOES: build one menu page: creates the page container, adds every
 * control on it in turn, and reports failure if any of them could not be
 * made. One of a family of page builders, each laying out its own screen --
 * this is the front end's ROOT page, the first thing the player sees. */
/* @implements 0x100425E0 glide BrUiRootEnter_100425E0
 * @cpp_kind method
 * @cpp_symbol ?BrUiRootEnter_100425E0@@YAHPAVGameUi@@@Z
 *
 * 2659 B cdecl EH-frame root-menu builder. `new Phase32F` (0x348, ctor
 * 0x100418C0) then thirteen `new BrCtl` (0x1E214, ctor 0x10040B10) page
 * blocks — each new is its own /GX EH state (incrementing trylevels,
 * new-result spilled to the unwind temp), null-checked into the Err(4)
 * shell. s38 (+0x38) is the 8-arg init vcall (floats push raw for
 * simple lvalues, fld/fsub/fstp for the computed y offsets); s34
 * (+0x34) takes (BrStrGet(n), 1, 1, &str). Menu entries hook pfn08/
 * pfn0C and thread w2AB6[0] = w14+1 sublinks. THREE PROVEN LEVERS:
 * (1) the null check is a CHAR bool computed AFTER the slot store
 * (`store; bad = (p==0); if (bad)`) — that's what emits sete al/test al
 * (an int bool folds to a plain jne); (2) the sublink temp is the
 * INLINE expression `(short)(cont->w14 + 1)` (a named short temp
 * allocates to ax, the original uses dx); (3) tail order is
 * w2AB6-store FIRST, w2AB4-inc second — the scheduler sinks the store.
 * Transcribed from the Ghidra draft; container/page layouts shared
 * with 0x10041980 / 0x100415D0 / 0x10040D10.
 */
class GameUi;
class BrCtl;

class Phase32F {
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
    Phase32F *a14[22];              /* +0x14 */
    int a6C[1];                     /* +0x6C */
};

typedef char chk_cont[sizeof(Phase32F) == 0x348 ? 1 : -1];
typedef char chk_ctl[sizeof(BrCtl) == 0x1E214 ? 1 : -1];
typedef char chk_w1e20c[(unsigned)&((BrCtl *)0)->w1E20C == 0x1E20C ? 1 : -1];
typedef char chk_a6c[(unsigned)&((GameUi *)0)->a6C == 0x6C ? 1 : -1];

extern "C" {
extern char DAT_100aaca8;
extern char DAT_100aabe8;
extern char DAT_100acad8;
extern char DAT_100aacf8;
extern int DAT_10ac4c58;
extern float DAT_10077648;
extern float DAT_1007764c;
extern float DAT_10077650;
extern float DAT_10077654;
extern float DAT_10077658;
extern float DAT_1007765c;
char *BrStrGet(int);
void FUN_100378c0(int);
int BrSub10047360();
int BrSub10043BF0();
int BrMenuSub10044B90();
int BrPhaseActivate_10044F50();
int BrPhaseActivate_100451E0();
int BrPhaseActivate_10045900();
int BrPhaseActivate_10046170();
int BrPhaseTick_100474B0();
int BrPhaseTick_100475F0();
int BrUiCreditsAction_1003AED0();
}

typedef int (*CtlFn)(BrCtl *);

int BrUiRootEnter_100425E0(GameUi *parent)
{
    Phase32F *cont;
    BrCtl *p;
    char bad;

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
    cont->f33C = 125.0f;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 0, 0, 9, 2, 5, 0, 0);
    cont->w14 += 1;
    p = new BrCtl;
    cont->f334 = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 0, 0, 9, 2, 5, 0, 1);
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, cont->f338, 10.0f, 0x100009, 2, 5, 1, -1);
    p->w1E20C = 3;
    p->s34(BrStrGet(1), 1, 1, &DAT_100aaca8);
    cont->w14 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, cont->f338, cont->f33C, 0x102001, 2, 5, 1, -1);
    p->pfn0C = (CtlFn)BrSub10047360;
    p->pfn08 = (CtlFn)BrPhaseActivate_10045900;
    p->w1E20C = 3;
    p->s34(BrStrGet(2), 1, 1, &DAT_100aabe8);
    p->w2AB6[0] = (short)(cont->w14 + 1);
    p->w2AB4 += 1;
    cont->w14 += 1;
    cont->w344 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 80.0f, 46.0f, 0x809, 2, 5, 0, 6);
    cont->w14 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, cont->f338, cont->f33C - DAT_10077648, 0x102001, 2, 5, 1, -1);
    p->pfn0C = (CtlFn)BrPhaseTick_100474B0;
    p->pfn08 = (CtlFn)BrSub10043BF0;
    p->w1E20C = 3;
    p->s34(BrStrGet(3), 1, 1, &DAT_100aabe8);
    p->w2AB6[0] = (short)(cont->w14 + 1);
    p->w2AB4 += 1;
    cont->w14 += 1;
    cont->w344 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 80.0f, 46.0f, 0x809, 2, 5, 0, 7);
    cont->w14 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, cont->f338, cont->f33C - DAT_1007764c, 0x102001, 2, 5, 1, -1);
    p->pfn0C = (CtlFn)BrPhaseTick_100474B0;
    p->pfn08 = (CtlFn)BrMenuSub10044B90;
    p->w1E20C = 3;
    p->s34(BrStrGet(4), 1, 1, &DAT_100aabe8);
    p->w2AB6[0] = (short)(cont->w14 + 1);
    p->w2AB4 += 1;
    cont->w14 += 1;
    cont->w344 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 80.0f, 46.0f, 0x809, 2, 5, 0, 8);
    cont->w14 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, cont->f338, cont->f33C - DAT_10077650, 0x102001, 2, 5, 1, -1);
    p->pfn0C = (CtlFn)BrSub10047360;
    p->pfn08 = (CtlFn)BrPhaseActivate_10044F50;
    p->w1E20C = 3;
    p->s34(BrStrGet(5), 1, 1, &DAT_100aabe8);
    p->w2AB6[0] = (short)(cont->w14 + 1);
    p->w2AB4 += 1;
    cont->w14 += 1;
    cont->w344 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 80.0f, 46.0f, 0x809, 2, 5, 0, 10);
    cont->w14 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, cont->f338, cont->f33C - DAT_10077654, 0x102001, 2, 5, 1, -1);
    p->pfn0C = (CtlFn)BrPhaseTick_100474B0;
    p->pfn08 = (CtlFn)BrPhaseActivate_100451E0;
    p->w1E20C = 3;
    p->s34(BrStrGet(6), 1, 1, &DAT_100aabe8);
    p->w2AB6[0] = (short)(cont->w14 + 1);
    p->w2AB4 += 1;
    cont->w14 += 1;
    cont->w344 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 80.0f, 46.0f, 0x809, 2, 5, 0, 9);
    cont->w14 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, cont->f338, cont->f33C - DAT_10077658, 0x102001, 2, 5, 1, -1);
    p->pfn0C = (CtlFn)BrPhaseTick_100475F0;
    p->pfn08 = (CtlFn)BrUiCreditsAction_1003AED0;
    p->w1E20C = 3;
    p->s34(BrStrGet(7), 1, 1, &DAT_100aabe8);
    p->w2AB6[0] = (short)(cont->w14 + 1);
    p->w2AB4 += 1;
    cont->w14 += 1;
    cont->w344 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, cont->f338, cont->f33C - DAT_1007765c, 0x102001, 2, 5, 1, -1);
    p->pfn0C = (CtlFn)BrPhaseTick_100474B0;
    p->pfn08 = (CtlFn)BrPhaseActivate_10046170;
    p->w1E20C = 3;
    p->s34(BrStrGet(8), 1, 1, &DAT_100aabe8);
    cont->w14 += 1;
    cont->w344 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, cont->f338, 29.0f, 0x100009, 2, 5, 1, -1);
    p->w1E20C = 3;
    p->s34(&DAT_100acad8, 1, 1, &DAT_100aacf8);
    DAT_10ac4c58 = cont->w14;
    cont->w14 += 1;
    return 1;
}
