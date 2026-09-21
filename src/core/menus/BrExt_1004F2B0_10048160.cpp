/* WHAT IT DOES: build one menu page: creates the page container, adds every
 * control on it in turn, and reports failure if any of them could not be
 * made. One of a family of page builders, each laying out its own screen. */
/* @implements 0x10048160 glide BrExt_1004F2B0
 * @cpp_kind free
 * @cpp_symbol ?BrExt_1004F2B0@@YAHPAVGameUi@@@Z
 *
 * 1091 B cdecl EH-frame menu-page builder, the small sibling of
 * 0x100425E0. `new Page48160` (0x348, ctor 0x10048470) then six
 * `new BrCtl` (0x1E214, ctor 0x10040B10) blocks -- each new is its own
 * /GX EH state, null-checked into the Err(4) shell. Layouts are shared
 * with 0x100425E0 / 0x10041980 / 0x100415D0; only the page ctor and the
 * entry list differ.
 *
 * The proven levers from 0x100425E0 apply unchanged:
 *  (1) the null check is a CHAR bool computed AFTER the slot store
 *      (`store; bad = (p == 0); if (bad)`) -- that is what emits
 *      sete al / test al; an int bool folds to a plain jne.
 *  (2) simple float lvalues push raw; only the computed y offsets
 *      (`f33C - k`) become fld/fsub/fstp.
 *  (3) the entry tail is w14-inc then w344-inc, in that order.
 *
 * Transcribed from the Ghidra draft.
 */
class GameUi;
class BrCtl;

class Page48160 {
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
    Page48160();
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
    Page48160 *a14[22];              /* +0x14 */
    int a6C[1];                     /* +0x6C */
};

typedef char chk_page48160[sizeof(Page48160) == 0x348 ? 1 : -1];
typedef char chk_ctl48160[sizeof(BrCtl) == 0x1E214 ? 1 : -1];
typedef char chk_w1e20c48160[(unsigned)&((BrCtl *)0)->w1E20C == 0x1E20C ? 1 : -1];
typedef char chk_a6c48160[(unsigned)&((GameUi *)0)->a6C == 0x6C ? 1 : -1];


typedef int (*CtlFn)(BrCtl *);

extern "C" {
extern char  DAT_100aaca8;
extern char  DAT_100aabe8;
extern float DAT_10077648;
extern float DAT_1007765c;
char *BrStrGet(int);
void FUN_100378c0(int);
int BrSub10047360();
int BrPhaseActivate_10045AF0();
int BrPhaseHook_10045AA0();
int BrUiNavHook_10046C90();
}

int BrExt_1004F2B0(GameUi *parent)
{
    Page48160 *cont;
    BrCtl     *p;
    char       bad;

    parent->w12 = 0;
    parent->a6C[parent->w10] = 1;

    cont = new Page48160;
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
    p->s34(BrStrGet(9), 1, 1, &DAT_100aaca8);
    cont->w14 += 1;

    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, cont->f338, cont->f33C, 0x102001, 2, 5, 1, -1);
    p->pfn0C = (CtlFn)BrSub10047360;
    p->pfn08 = (CtlFn)BrPhaseActivate_10045AF0;
    p->w1E20C = 3;
    p->s34(BrStrGet(10), 1, 1, &DAT_100aabe8);
    cont->w14 += 1;
    cont->w344 += 1;

    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, cont->f338, cont->f33C - DAT_10077648, 0x102001, 2, 5, 1, -1);
    p->pfn0C = (CtlFn)BrSub10047360;
    p->pfn08 = (CtlFn)BrPhaseHook_10045AA0;
    p->w1E20C = 3;
    p->s34(BrStrGet(11), 1, 1, &DAT_100aabe8);
    cont->w14 += 1;
    cont->w344 += 1;

    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, cont->f338, cont->f33C - DAT_1007765c, 0x102001, 2, 5, 1, -1);
    p->pfn0C = (CtlFn)BrSub10047360;
    p->pfn08 = (CtlFn)BrUiNavHook_10046C90;
    p->w1E20C = 3;
    p->s34(BrStrGet(12), 1, 1, &DAT_100aabe8);
    cont->w14 += 1;
    cont->w344 += 1;

    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 80.0f, 46.0f, 9, 2, 5, 0, 6);
    cont->w14 += 1;

    return 1;
}
