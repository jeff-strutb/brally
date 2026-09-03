/* @implements 0x10043050 glide BrUiQuitEnter_10043050
 * @cpp_kind free
 * @cpp_symbol ?BrUiQuitEnter_10043050@@YAHPAVGameUi@@@Z
 *
 * 794 B cdecl EH-frame menu-page builder. Emitted by
 * tools/gen_menubuilder.py from the Ghidra draft; the class
 * layouts and the three family levers come from the hand-solved
 * 0x100425E0 / 0x10048160 (char bool after the slot store, raw
 * float pushes for simple lvalues, w14-then-w344 tails).
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

class SlotTable043050 {
public:
    virtual void s0();
    virtual void s1(char *pszPattern);   /* +0x04 */
    char aRecs[26000];                   /* +0x04, 0x104-byte records */
};

class Root043050 {
public:
    char            pad000[0xC0];
    SlotTable043050 *pTable;         /* +0xC0 */
};


class Page043050 {
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
    Page043050();
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
    char pad18[0x2AB4 - 0x18];      /* +0x18 */
    short w2AB4;                    /* +0x2AB4 */
    short w2AB6[0x19];              /* +0x2AB6 */
    char pad2AE8[0x3838 - 0x2AE8];  /* +0x2AE8 */
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
    Page043050 *a14[22];              /* +0x14 */
    int a6C[1];                     /* +0x6C */
};

typedef char chk_page043050[sizeof(Page043050) == 0x348 ? 1 : -1];
typedef char chk_ctl043050[sizeof(BrCtl) == 0x1E214 ? 1 : -1];
typedef char chk_sel_off[(unsigned)&((BrCtl *)0)->m3838 == 0x3838 ? 1 : -1];
typedef char chk_f1e1f4[(unsigned)&((BrCtl *)0)->f1E1F4 == 0x1E1F4 ? 1 : -1];
typedef char chk_w1e20c043050[(unsigned)&((BrCtl *)0)->w1E20C == 0x1E20C ? 1 : -1];
typedef char chk_a6c043050[(unsigned)&((GameUi *)0)->a6C == 0x6C ? 1 : -1];
typedef char chk_tbl043050[(unsigned)&((Root043050 *)0)->pTable == 0xC0 ? 1 : -1];

typedef int (*CtlFn)(BrCtl *);

extern "C" {
int BrSub10047360();
extern char  DAT_100aabe8;
extern char  DAT_100aaca8;
int FUN_1003cc60();
int FUN_1003f940();
extern float DAT_1007765c;
char *BrStrGet(int);
void FUN_100378c0(int);
}

int BrUiQuitEnter_10043050(GameUi *parent)
{
    Page043050 *cont;
    BrCtl     *p;
    char       bad;

    parent->w12 = 0;
    parent->a6C[parent->w10] = 1;
    cont = new Page043050;
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
    p->s34(BrStrGet(0xe), 1, 1, &DAT_100aaca8);
    cont->w14 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, cont->f338, cont->f33C, 0x102001, 2, 5, 1, -1);
    p->pfn0C = (CtlFn)BrSub10047360;
    p->pfn08 = (CtlFn)FUN_1003cc60;
    p->w1E20C = 3;
    p->s34(BrStrGet(0xf), 1, 1, &DAT_100aabe8);
    cont->w14 += 1;
    cont->w344 += 1;
    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, cont->f338, cont->f33C - DAT_1007765c, 0x102001, 2, 5, 1, -1);
    p->pfn0C = (CtlFn)BrSub10047360;
    p->pfn08 = (CtlFn)FUN_1003f940;
    p->w1E20C = 3;
    p->s34(BrStrGet(0xc), 1, 1, &DAT_100aabe8);
    cont->w14 += 1;
    cont->w344 += 1;

    return 1;
}
