/* @implements 0x10048F10 glide BrExt_10050060
 * @cpp_kind free
 * @cpp_symbol ?BrExt_10050060@@YAHPAVGameUi@@@Z
 *
 * 2433 B cdecl EH-frame menu builder, the largest of this family so far
 * and the only one that builds TWO pages. Same recipe as 0x10048160 /
 * 0x100458D0 / 0x100451F0, plus three things they do not have:
 *
 *  - a prologue that flips the 0x10AC5BA0 gate around a vcall on the save
 *    table (+0xC0 of the root object) to load "RallySeason*.BRF", and
 *    resets the slot index to -1;
 *  - a dropdown row: the +0x3838 selector sub-object is configured
 *    through its own vtable (+0x14 setup, +0x10 add-item) and then fed
 *    every save-slot name by walking the table's 0x104-byte records --
 *    the record ADDRESS is null-tested even though it cannot be null,
 *    the same computed-address idiom the item-label functions use;
 *  - a second page, opened by clearing the parent's flag slot and
 *    running the whole page prologue again.
 *
 * The family levers are unchanged: the null check is a CHAR bool computed
 * AFTER the slot store, simple float lvalues push raw while computed y
 * offsets become fld/fsub/fstp, and the entry tail is w14 then w344.
 *
 * Transcribed from the Ghidra draft (which loses the parameter to
 * `unaff_retaddr`; it is the usual GameUi *).
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

class SlotTable48F10 {
public:
    virtual void s0();
    virtual void s1(char *pszPattern);   /* +0x04 */
    char aRecs[26000];                   /* +0x04, 0x104-byte records */
};

class Root48F10 {
public:
    char            pad000[0xC0];
    SlotTable48F10 *pTable;         /* +0xC0 */
};


class Page48F10 {
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
    Page48F10();
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
    Page48F10 *a14[22];              /* +0x14 */
    int a6C[1];                     /* +0x6C */
};

typedef char chk_page48F10[sizeof(Page48F10) == 0x348 ? 1 : -1];
typedef char chk_ctl48F10[sizeof(BrCtl) == 0x1E214 ? 1 : -1];
typedef char chk_sel_off[(unsigned)&((BrCtl *)0)->m3838 == 0x3838 ? 1 : -1];
typedef char chk_f1e1f4[(unsigned)&((BrCtl *)0)->f1E1F4 == 0x1E1F4 ? 1 : -1];
typedef char chk_w1e20c48F10[(unsigned)&((BrCtl *)0)->w1E20C == 0x1E20C ? 1 : -1];
typedef char chk_a6c48F10[(unsigned)&((GameUi *)0)->a6C == 0x6C ? 1 : -1];
typedef char chk_tbl48F10[(unsigned)&((Root48F10 *)0)->pTable == 0xC0 ? 1 : -1];

typedef int (*CtlFn)(BrCtl *);

extern "C" {
extern char  DAT_100aabd8;
extern char  DAT_100aabe8;
extern char  DAT_100aac08;
extern char  DAT_100aac18;
extern char  DAT_100aac28;
extern char  DAT_100aac78;
extern char  DAT_100aaca8;
extern char  DAT_10396f08;
extern char  s_RallySeason_BRF_100acb20;
extern float DAT_10077658;
extern float DAT_1007765c;
int        g_brGate5BA0;        /* 0x10AC5BA0 */
Root48F10 *g_brRoot5C60;        /* 0x10AC5C60 */
BrCtl     *g_brCtl5D18;         /* 0x10AC5D18 */
BrCtl     *g_AA29F4;
int        g_i0AB3F4;
char *BrStrGet(int);
void FUN_100378c0(int);
int BrSub10047360();
int BrPhaseDispatch_100450F0();
int BrPhaseEdit_10047210();
int BrUiHook81_10046EB0();
int BrUiHook85_1003E7A0();
int BrUiHook85_1003EB10();
int BrMenuFlags18D0();
int BrMenuText0A50();
int BrMenuText0AC0();
int BrMenuText1300();
int FUN_1003b350();
int FUN_1003b580();
}

int BrExt_10050060(GameUi *parent)
{
    Page48F10 *cont;
    BrCtl     *p;
    char       bad;

    g_brGate5BA0 = 1;
    g_brRoot5C60->pTable->s1(&s_RallySeason_BRF_100acb20);
    g_brGate5BA0 = 0;

    parent->w12 = 0;
    g_i0AB3F4 = -1;
    parent->a6C[parent->w10] = 1;

    cont = new Page48F10;
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
    p->s34(BrStrGet(0x39), 1, 1, &DAT_100aaca8);
    cont->w14 += 1;

    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, cont->f338, cont->f33C, 0x3001, 2, 5, 1, -1);
    p->pfn04 = (CtlFn)BrUiHook85_1003EB10;
    p->f1E1F4 = 1;
    p->m3838.s5(0x40001, &DAT_100aac78, 5, 0, -1);
    p->m3838.pfn04 = (int (*)(void))FUN_1003b580;
    p->m3838.f14 = (int)FUN_1003b350;
    {
        int off = 0;

        do {
            char *psz = &g_brRoot5C60->pTable->aRecs[off];

            if (psz != 0)
                p->m3838.s4(psz, 0, 1, &DAT_100aac78, 0);
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
    p->s38(parent, cont->f338, cont->f33C - DAT_10077658, 0x103011, 2, 5, 1, -1);
    p->pfn0C = (CtlFn)BrSub10047360;
    p->pfn08 = (CtlFn)BrPhaseDispatch_100450F0;
    p->pfn04 = (CtlFn)BrMenuFlags18D0;
    p->w1E20C = 2;
    p->s34(BrStrGet(0x1E), 1, 0, &DAT_100aabe8);
    cont->w14 += 1;
    cont->w344 += 1;

    p = new BrCtl;
    cont->a18[cont->w14] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, cont->f338, cont->f33C - DAT_1007765c, 0x102001, 2, 5, 1, -1);
    p->pfn0C = (CtlFn)BrSub10047360;
    p->pfn08 = (CtlFn)BrUiHook81_10046EB0;
    p->w1E20C = 3;
    p->s34(BrStrGet(0x0C), 1, 1, &DAT_100aabe8);
    g_AA29F4 = p;
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

    parent->a6C[parent->w10] = 0;

    cont = new Page48F10;
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
    p->s38(parent, 0, 232.0f, 0x100009, 2, 5, 1, -1);
    p->pfn0C = (CtlFn)BrSub10047360;
    p->pfn04 = (CtlFn)BrPhaseEdit_10047210;
    p->pfn14 = (CtlFn)BrUiHook85_1003E7A0;
    p->w1E20C = 3;
    p->s34(BrStrGet(0x3A), 1, 1, &DAT_100aabd8);
    g_brCtl5D18 = p;
    cont->w14 += 1;

    return 1;
}
