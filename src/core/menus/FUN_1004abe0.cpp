/* WHAT IT DOES: build one menu page: creates the page container, adds every
 * control on it in turn, and reports failure if any of them could not be
 * made. One of a family of page builders, each laying out its own screen,
 * and this one adds a repeated row of controls in a loop rather than one at
 * a time. */
/* @implements 0x1004abe0 glide FUN_1004abe0
 * @cpp_kind free
 * @cpp_symbol ?FUN_1004abe0@@YAHPAVGameUi@@@Z
 *
 * 760 B cdecl EH-frame menu-page builder -- the smallest member of the
 * family that carries the "photo" control, so it is the one this block was
 * solved on. Three plain entries and then a fourth whose extra setup is the
 * photo block:
 *
 *   - a slot-table pointer at +0x1E210 and two ones at +0x2968/+0x296C;
 *   - TWO strided loops over a pair of parallel arrays (ints at +0x2978,
 *     shorts at +0x2A40) whose ranges are contiguous, 0..14 then 15..23 --
 *     the second loop exists only because the short changes from 0x50 to
 *     0x51. VC5 strength-reduces both into pointer walks, which is why the
 *     original opens each with two `lea`s of the STARTING element and a
 *     down-counter; that is codegen, the source is an indexed for loop;
 *   - a rect built with four `__ftol` conversions of the page's two floats,
 *     stored in the order +0x54, +0x50, +0x58, +0x5C. That order is the
 *     source's, not a schedule: writing them 50/54/58/5C rotates the block.
 *
 * The three null-checks are all the same source shape (store the slot, then
 * take the bool, then test it). They emit differently only because `ebx`
 * holds the pinned zero for the first two and is clobbered by the +0x34
 * vcall's vtable load before the third -- `cmp r,ebx` versus `test r,r` is
 * that register dying, not a source difference.
 */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#endif

class GameUi;
class BrCtl;

class PageABE0 {
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
    PageABE0();
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
    char pad18[0x50 - 0x18];        /* +0x18 */
    int   f050;                     /* +0x050 */
    int   f054;                     /* +0x054 */
    int   f058;                     /* +0x058 */
    int   f05C;                     /* +0x05C */
    char pad060[0x2968 - 0x60];     /* +0x060 */
    int   f2968;                    /* +0x2968 */
    int   f296C;                    /* +0x296C */
    char pad2970[0x2978 - 0x2970];
    int   a2978[24];                /* +0x2978 */
    char pad29D8[0x2A40 - 0x29D8];
    short w2A40[24];                /* +0x2A40 */
    char pad2A70[0x2AB4 - 0x2A70];
    short w2AB4;                    /* +0x2AB4 */
    short w2AB6[0x19];              /* +0x2AB6 */
    char pad2AE8[0x1E1F4 - 0x2AE8]; /* +0x2AE8 */
    int f1E1F4;                     /* +0x1E1F4 */
    char pad1E1F8[0x1E20C - 0x1E1F8];
    unsigned short w1E20C;          /* +0x1E20C */
    short pad1E20E;
    void *p1E210;                   /* +0x1E210 */
    BrCtl();
};

class GameUi {
public:
    char pad[0x10];
    unsigned short w10;             /* +0x10 */
    short w12;                      /* +0x12 */
    PageABE0 *a14[22];              /* +0x14 */
    int a6C[1];                     /* +0x6C */
};

typedef char chk_pageABE0[sizeof(PageABE0) == 0x348 ? 1 : -1];
typedef char chk_ctlABE0[sizeof(BrCtl) == 0x1E214 ? 1 : -1];
typedef char chk_f050ABE0[(unsigned)&((BrCtl *)0)->f050 == 0x50 ? 1 : -1];
typedef char chk_f2968ABE0[(unsigned)&((BrCtl *)0)->f2968 == 0x2968 ? 1 : -1];
typedef char chk_a2978ABE0[(unsigned)&((BrCtl *)0)->a2978 == 0x2978 ? 1 : -1];
typedef char chk_w2A40ABE0[(unsigned)&((BrCtl *)0)->w2A40 == 0x2A40 ? 1 : -1];
typedef char chk_f1e1f4ABE0[(unsigned)&((BrCtl *)0)->f1E1F4 == 0x1E1F4 ? 1 : -1];
typedef char chk_p1E210ABE0[(unsigned)&((BrCtl *)0)->p1E210 == 0x1E210 ? 1 : -1];
typedef char chk_a6cABE0[(unsigned)&((GameUi *)0)->a6C == 0x6C ? 1 : -1];

typedef int (*CtlFn)(BrCtl *);

extern "C" {
extern char  DAT_100aabd8;
extern char  DAT_10ac4ad8;
extern float DAT_10077628;
extern float DAT_10077670;
int BrSub10040600();
char *BrStrGet(int);
void FUN_100378c0(int);
}

int FUN_1004abe0(GameUi *parent)
{
    PageABE0 *cont;
    BrCtl    *p;
    char      bad;
    int       i;

    parent->w12 = 0;
    parent->a6C[parent->w10] = 1;
    cont = new PageABE0;
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
    p->s34(BrStrGet(0x43), 1, 1, &DAT_100aabd8);
    cont->w14 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, cont->f338 - DAT_10077628, cont->f33C - DAT_10077670,
           0x22001, 2, 5, 0, 0x50);
    p->p1E210 = &DAT_10ac4ad8;
    p->f2968 = 1;
    p->f296C = 1;
    for (i = 0; i < 15; i++) {
        p->a2978[i] = 0x3c;
        p->w2A40[i] = 0x50;
    }
    for (i = 15; i < 24; i++) {
        p->a2978[i] = 0x3c;
        p->w2A40[i] = 0x51;
    }
    p->pfn08 = (CtlFn)BrSub10040600;
    p->w1E20C = 0x50;
    p->f054 = (int)cont->f33C;
    p->f050 = (int)cont->f338;
    p->f058 = (int)cont->f338 + 0x80;
    p->f05C = (int)cont->f33C + 0x80;
    cont->w14 += 1;
    cont->w344 += 1;

    return 1;
}
