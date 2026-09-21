/* WHAT IT DOES: build one menu page: creates the page container, adds every
 * control on it in turn, and reports failure if any of them could not be
 * made. One of a family of page builders, each laying out its own screen. */
/* @implements 0x100476E0 glide BrExt_1004E830
 * @cpp_kind method
 * @cpp_symbol ?BrExt_1004E830@@YAHPAVGameUi@@@Z
 *
 * 2679 B cdecl EH-frame options-menu builder — sibling of 0x100425E0
 * (same Phase32F/BrCtl layouts, same three levers: char bool after the
 * slot store, inline (short)(w14+1) sublink, w2AB6-store-before-
 * w2AB4-inc tail). New here: the force-feedback page's flags arg is the
 * neg/sbb ternary `(DAT_118eeed4 ? 0xfffffff0 : 0) + 0x102011`, its
 * w1E20C/s34 pair is a plain if/else on the same global (VC5 tail-
 * merges after the differing arg), caption pages hook pfn04, and the
 * hook page pointer is captured in g_AA29C8.
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

extern "C" {
extern char DAT_100aaca8;
extern char DAT_100aabe8;
extern char DAT_100acad8;
extern char DAT_100aac18;
extern char DAT_100aac28;
extern char DAT_100aac58;
extern char DAT_100aac68;
extern int DAT_118eeed4;
extern BrCtl *g_AA29C8;
extern float DAT_10077648;
extern float DAT_1007764c;
extern float DAT_10077650;
extern float DAT_1007765c;
char *BrStrGet(int);
void FUN_100378c0(int);
void BrFfbReprobe(void);
int BrSub10047360();
int FUN_1003cae0();
int FUN_1003cb40();
int FUN_1003cba0();
int FUN_100392e0();
int BrUiOptHook_100436B0();
int BrUiHook84_10046710();
int BrUiText1003FCB0();
int BrUiText1003FD30();
int BrUiText1003FE10();
int BrMenuCap0950();
int BrMenuCap0990();
int BrMenuCap09B0();
int BrMenuCap09D0();
}

typedef int (*CtlFn)(BrCtl *);

int BrExt_1004E830(GameUi *parent)
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
    p->s34(BrStrGet(0x22), 1, 1, &DAT_100aaca8);
    cont->w14 += 1;
    BrFfbReprobe();
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, cont->f338, cont->f33C, (DAT_118eeed4 ? 0xfffffff0 : 0) + 0x102011, 2, 5, 1, -1);
    p->pfn0C = (CtlFn)BrSub10047360;
    p->pfn08 = (CtlFn)FUN_1003cae0;
    if (DAT_118eeed4 != 0) {
        p->w1E20C = 3;
        p->s34(BrStrGet(0x30), 1, 1, &DAT_100aabe8);
    } else {
        p->w1E20C = 2;
        p->s34(BrStrGet(0x30), 1, 0, &DAT_100aabe8);
    }
    cont->w14 += 1;
    cont->w344 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, cont->f338, cont->f33C - DAT_10077648, 0x102001, 2, 5, 1, -1);
    p->pfn0C = (CtlFn)BrSub10047360;
    p->pfn08 = (CtlFn)FUN_1003cb40;
    p->w1E20C = 3;
    p->s34(BrStrGet(0x31), 1, 1, &DAT_100aabe8);
    cont->w14 += 1;
    cont->w344 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, cont->f338, cont->f33C - DAT_1007764c, 0x102001, 2, 5, 1, -1);
    p->pfn0C = (CtlFn)BrSub10047360;
    p->pfn08 = (CtlFn)BrUiOptHook_100436B0;
    p->w1E20C = 3;
    p->s34(BrStrGet(0x32), 1, 1, &DAT_100aabe8);
    cont->w14 += 1;
    cont->w344 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, cont->f338, cont->f33C - DAT_10077650, 0x102001, 2, 5, 1, -1);
    p->pfn0C = (CtlFn)BrSub10047360;
    p->pfn08 = (CtlFn)FUN_1003cba0;
    p->w1E20C = 3;
    p->s34(BrStrGet(0x33), 1, 1, &DAT_100aabe8);
    cont->w14 += 1;
    cont->w344 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, cont->f338, cont->f33C - DAT_1007765c, 0x102001, 2, 5, 1, -1);
    p->pfn0C = (CtlFn)BrSub10047360;
    p->pfn08 = (CtlFn)BrUiHook84_10046710;
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
    p->s38(parent, 336.0f, 48.0f, 1, 2, 5, 1, 0x61);
    p->pfn04 = (CtlFn)BrMenuCap0950;
    p->w2AB6[0] = (short)(cont->w14 + 1);
    p->w2AB4 += 1;
    cont->w14 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, cont->f338, 166.0f, 0x101001, 2, 5, 1, -1);
    p->pfn04 = (CtlFn)BrUiText1003FCB0;
    p->w1E20C = 3;
    p->s34(&DAT_100acad8, 1, 1, &DAT_100aac68);
    cont->w14 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 67.0f, 199.0f, 1, 2, 5, 1, 0x8c);
    p->pfn04 = (CtlFn)BrMenuCap0990;
    p->w2AB6[0] = (short)(cont->w14 + 1);
    p->w2AB4 += 1;
    cont->w14 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, cont->f338, 279.0f, 0x101001, 2, 5, 1, -1);
    p->pfn04 = (CtlFn)BrUiText1003FD30;
    p->w1E20C = 3;
    p->s34(&DAT_100acad8, 1, 1, &DAT_100aac58);
    cont->w14 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 440.0f, 114.0f, 1, 2, 5, 1, 0x65);
    p->pfn04 = (CtlFn)BrMenuCap09D0;
    p->w2AB6[0] = (short)(cont->w14 + 1);
    p->w2AB4 += 1;
    cont->w14 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, cont->f338, 196.0f, 0x101001, 2, 5, 1, -1);
    p->pfn04 = (CtlFn)BrUiText1003FE10;
    p->w1E20C = 3;
    p->s34(&DAT_100acad8, 1, 1, &DAT_100aac28);
    cont->w14 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 484.0f, 214.0f, 1, 2, 5, 1, 99);
    p->pfn04 = (CtlFn)BrMenuCap09B0;
    p->w2AB6[0] = (short)(cont->w14 + 1);
    p->w2AB4 += 1;
    cont->w14 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, cont->f338, 273.0f, 0x101001, 2, 5, 1, -1);
    p->pfn04 = (CtlFn)FUN_100392e0;
    p->w1E20C = 3;
    p->s34(&DAT_100acad8, 1, 1, &DAT_100aac18);
    cont->w14 += 1;
    return 1;
}
