/* @implements 0x1004a840 glide BrUi51990ScreenNew
 * @cpp_kind free
 * @cpp_symbol ?BrUi51990ScreenNew@@YAHPAVGameUi@@@Z
 *
 * 923 B cdecl EH-frame menu-page builder. Emitted by
 * tools/gen_menubuilder.py from the Ghidra draft; the class
 * layouts and the three family levers come from the hand-solved
 * 0x100425E0 / 0x10048160 (char bool after the slot store, raw
 * float pushes for simple lvalues, w14-then-w344 tails).
 */
class GameUi;
class BrCtl;

class Page04A840 {
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
    Page04A840();
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
    Page04A840 *a14[22];              /* +0x14 */
    int a6C[1];                     /* +0x6C */
};

typedef char chk_page04A840[sizeof(Page04A840) == 0x348 ? 1 : -1];
typedef char chk_ctl04A840[sizeof(BrCtl) == 0x1E214 ? 1 : -1];
typedef char chk_w1e20c04A840[(unsigned)&((BrCtl *)0)->w1E20C == 0x1E20C ? 1 : -1];
typedef char chk_a6c04A840[(unsigned)&((GameUi *)0)->a6C == 0x6C ? 1 : -1];


typedef int (*CtlFn)(BrCtl *);

extern "C" {
int BrMenuSetTrackLetter();
int BrPhaseGuard_100471F0();
int BrPhaseLeave_10047120();
int BrRacePosIconSet();
int BrSub10047360();
extern char  DAT_100aabd8;
char *BrStrGet(int);
void FUN_100378c0(int);
}

int BrUi51990ScreenNew(GameUi *parent)
{
    Page04A840 *cont;
    BrCtl     *p;
    char       bad;

    parent->w12 = 0;
    parent->a6C[parent->w10] = 1;
    cont = new Page04A840;
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
    p->s38(parent, 0, 29.0f, 9, 2, 5, 0, 0x4E);
    cont->w14 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 13.0f, 7.0f, 9, 2, 5, 0, 0x4F);
    cont->w14 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 16.0f, 153.0f, 9, 2, 5, 1, 0x47);
    p->pfn04 = (CtlFn)BrRacePosIconSet;
    cont->w14 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 392.0f, 181.0f, 9, 2, 5, 1, 0x48);
    p->pfn04 = (CtlFn)BrMenuSetTrackLetter;
    cont->w14 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, cont->f338, 460.0f, 0x102001, 2, 5, 0, -1);
    p->pfn0C = (CtlFn)BrSub10047360;
    p->pfn08 = (CtlFn)BrPhaseLeave_10047120;
    p->pfn04 = (CtlFn)BrPhaseGuard_100471F0;
    p->w1E20C = 2;
    p->s34(BrStrGet(0x42), 1, 0, &DAT_100aabd8);
    cont->w14 += 1;
    cont->w344 += 1;

    return 1;
}
