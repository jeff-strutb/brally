/* WHAT IT DOES: run one frame of a menu page: gives its pre-hook first
 * refusal (which can end the page early with a sentinel), then updates the
 * page's sub-mode and its controls. The per-frame step of the front end,
 * once per page. */
/* @implements 0x100415D0 glide BrUiFrame_10048180
 * @cpp_kind method
 * @cpp_symbol ?Frame@UiPage@@QAEHXZ
 *
 * 738 B thiscall on the page object itself. Vtbl CSEd into edi and
 * spilled ([esp+0x14]); w128 saved as a word local ([esp+0x10]). Guarded
 * s3C gate, pre-hook fnptr with -1/-2 sentinel returns, then an s20/mode
 * split: the live branch does the pSub-mode helper swaps keyed on the
 * pfn08 address, the paused branch latches w128=0 and the 3-latch focus
 * handoff. Tail loop restamps the w2AB6 sublist pages (full-chain
 * re-reads each statement; VC5 CSEs only across the f58 store). Every
 * f1C test is a direct re-read (CSE folds adjacent ones); the outer
 * early-exit s08 re-reads the vtbl because edi is not yet live there.
 */
class PageTab;
class UiCont;

class UiPage {
public:
    virtual void s00();
    virtual void s04();             /* +0x04 */
    virtual void s08();             /* +0x08 */
    virtual void s0C();             /* +0x0C */
    virtual void s10();
    virtual void s14();
    virtual void s18();
    virtual void s1C();
    virtual int  s20();             /* +0x20 */
    virtual void s24();
    virtual void s28();
    virtual void s2C();
    virtual void s30();             /* +0x30 */
    virtual void s34();
    virtual void s38();
    virtual int  s3C();             /* +0x3C */
    int (*pfn04)(UiPage *);         /* +0x04 */
    int (*pfn08)(UiPage *);         /* +0x08 */
    int (*pfn0C)(UiPage *);         /* +0x0C */
    char pad10[0xC];                /* +0x10 */
    unsigned int f1C;               /* +0x1C */
    char pad20[0x28];               /* +0x20 */
    short w48;                      /* +0x48 */
    char pad4A[0xE];                /* +0x4A */
    int f58;                        /* +0x58 */
    char pad5C[0xCC];               /* +0x5C */
    unsigned short w128;            /* +0x128 */
    char pad12A[0x2846];            /* +0x12A */
    int f2970;                      /* +0x2970 */
    int f2974;                      /* +0x2974 */
    char pad2978[0xC8];             /* +0x2978 */
    short w2A40;                    /* +0x2A40 */
    short w2A42;                    /* +0x2A42 */
    char pad2A44[0x70];             /* +0x2A44 */
    short w2AB4;                    /* +0x2AB4 */
    short w2AB6[0x19];              /* +0x2AB6 */
    UiCont *f2AE8;                  /* +0x2AE8 */
    char pad2AEC[0x78];             /* +0x2AEC */
    char b2B64;                     /* +0x2B64 */
    char pad2B65[0xCB3];            /* +0x2B65 */
    int f3818;                      /* +0x3818 */
    char pad381C[0x1A9F0];          /* +0x381C */
    unsigned short w1E20C;          /* +0x1E20C */
    int Frame();
};

class PageTab {
public:
    char pad[0x18];
    UiPage *a18[1];                 /* +0x18 */
};

class UiCont {
public:
    char pad[0x64];
    PageTab *f64;                   /* +0x64 */
};

class GameCtl {
public:
    char pad[0x2C];
    int f2C;                        /* +0x2C */
    int f30;                        /* +0x30 */
};

typedef char chk_sub[(unsigned)&((UiPage *)0)->f2AE8 == 0x2AE8 ? 1 : -1];
typedef char chk_w[(unsigned)&((UiPage *)0)->w1E20C == 0x1E20C ? 1 : -1];

extern "C" {
extern int DAT_10ac5c30;
extern int g_brAA2854;
extern int g_brAA33E4;
extern GameCtl *g_pBrAA2E80;
void BrSub10072AF0(int, int);
int BrOpt3760();
int FUN_1003c240();
}

int UiPage::Frame()
{
    unsigned short saved;
    int r;
    int i;

    saved = w128;
    if (!(f1C & 0x10)) {
        if (s3C() == 0) {
            if (f3818 != 0)
                s30();
            s04();
            if (pfn04) {
                r = pfn04(this);
                if (r == -2)
                    return 1;
                if (r == -1)
                    return 0;
            }
            if (s20() != 0 && DAT_10ac5c30 == 0) {
                if (f1C & 0x400000) {
                    if (g_pBrAA2E80->f2C != 0 || g_pBrAA2E80->f30 != 0)
                        w1E20C = w2A42;
                }
                if (f1C & 2) {
                    if (pfn08) {
                        if (pfn08 == (int (*)(UiPage *))BrOpt3760) {
                            BrSub10072AF0(2, 0x200020);
                            g_brAA2854 = 2;
                        } else if (pfn08 != (int (*)(UiPage *))FUN_1003c240) {
                            BrSub10072AF0(1, 0x200020);
                            g_brAA2854 = 1;
                        }
                        if (pfn08(this) == 0)
                            return 0;
                        if (pfn08 == (int (*)(UiPage *))FUN_1003c240) {
                            BrSub10072AF0(1, 0x200020);
                            g_brAA2854 = 1;
                        }
                        g_brAA33E4 = 0;
                    }
                    f1C &= ~2u;
                } else {
                    if (pfn0C)
                        pfn0C(this);
                }
                if ((f1C & 0x10000) && w2AB4 > 0) {
                    for (i = 0; i < w2AB4; ++i) {
                        f2AE8->f64->a18[w2AB6[i]]->f1C |= 0x20000;
                        f2AE8->f64->a18[w2AB6[i]]->w128 = saved;
                        f2AE8->f64->a18[w2AB6[i]]->f2974 = 0;
                        f2AE8->f64->a18[w2AB6[i]]->f2970 = 0;
                        f58 += f2AE8->f64->a18[w2AB6[i]]->w48;
                        f2AE8->f64->a18[w2AB6[i]]->s0C();
                        f2AE8->f64->a18[w2AB6[i]]->f1C &= ~0x20000u;
                    }
                    s08();
                    return 1;
                }
            } else {
                if (f1C & 0x400000)
                    w1E20C = w2A40;
                if (!(f1C & 4) && !(f1C & 0x20000)) {
                    w128 = 0;
                    if ((f1C & 0x100000) && !(f1C & 0x10) && pfn0C != 0) {
                        w1E20C = 3;
                        b2B64 = 1;
                        s08();
                        return 1;
                    }
                } else {
                    if (pfn0C)
                        pfn0C(this);
                }
            }
            s08();
            return 1;
        }
    }
    s08();
    return 1;
}
