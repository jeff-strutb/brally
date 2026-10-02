#include "slice3_39.h"   /* br_globals: its objects */
#include "br_ui.h"   /* BrUiCtl_, the canonical record */
#include "br_phase.h"   /* BrPhase_, the canonical record */
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




extern "C" {
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* BrSub10072AF0: prototype in br_funcs.h */
/* BrOpt3760: prototype in br_funcs.h */
/* FUN_1003c240: prototype in br_funcs.h */
}

int UiPage::Frame()
{
    unsigned short saved;
    int r;
    int i;

    saved = (*(unsigned short *)&((BrUiCtl_ *)(this))->wStep);
    if (!((*(unsigned int *)&((BrUiCtl_ *)(this))->flags1C) & 0x10)) {
        if (s3C() == 0) {
            if ((*(int *)&((BrUiCtl_ *)(this))->twActive) != 0)
                s30();
            s04();
            if ((*(int (**)(UiPage *))&((BrUiCtl_ *)(this))->pfn04)) {
                r = (*(int (**)(UiPage *))&((BrUiCtl_ *)(this))->pfn04)(this);
                if (r == -2)
                    return 1;
                if (r == -1)
                    return 0;
            }
            if (s20() != 0 && g_5C30 == 0) {
                if ((*(unsigned int *)&((BrUiCtl_ *)(this))->flags1C) & 0x400000) {
                    if ((*(GameCtl * *)&g_pBrAA2E80)->f2C != 0 || (*(GameCtl * *)&g_pBrAA2E80)->f30 != 0)
                        (*(unsigned short *)&((BrUiCtl_ *)(this))->w1E20C) = (*(short *)&((BrUiCtl_ *)(this))->aStepId[1]);
                }
                if ((*(unsigned int *)&((BrUiCtl_ *)(this))->flags1C) & 2) {
                    if ((*(int (**)(UiPage *))&((BrUiCtl_ *)(this))->pfn08)) {
                        if ((*(int (**)(UiPage *))&((BrUiCtl_ *)(this))->pfn08) == (int (*)(UiPage *))BrOpt3760) {
                            BrSub10072AF0(2, 0x200020);
                            g_track = 2;
                        } else if ((*(int (**)(UiPage *))&((BrUiCtl_ *)(this))->pfn08) != (int (*)(UiPage *))FUN_1003c240) {
                            BrSub10072AF0(1, 0x200020);
                            g_track = 1;
                        }
                        if ((*(int (**)(UiPage *))&((BrUiCtl_ *)(this))->pfn08)(this) == 0)
                            return 0;
                        if ((*(int (**)(UiPage *))&((BrUiCtl_ *)(this))->pfn08) == (int (*)(UiPage *))FUN_1003c240) {
                            BrSub10072AF0(1, 0x200020);
                            g_track = 1;
                        }
                        DAT_10ac6744 = 0;
                    }
                    (*(unsigned int *)&((BrUiCtl_ *)(this))->flags1C) &= ~2u;
                } else {
                    if ((*(int (**)(UiPage *))&((BrUiCtl_ *)(this))->pfn0C))
                        (*(int (**)(UiPage *))&((BrUiCtl_ *)(this))->pfn0C)(this);
                }
                if (((*(unsigned int *)&((BrUiCtl_ *)(this))->flags1C) & 0x10000) && (*(short *)&((BrUiCtl_ *)(this))->cChild) > 0) {
                    for (i = 0; i < (*(short *)&((BrUiCtl_ *)(this))->cChild); ++i) {
                        (*(unsigned int *)&((BrUiCtl_ *)((*(PageTab * *)&((BrPhase_ *)((*(UiCont * *)&((BrUiCtl_ *)(this))->pOwner)))->pCur)->a18[(*(short (*)[25])&((BrUiCtl_ *)(this))->aChild)[i]]))->flags1C) |= 0x20000;
                        (*(unsigned short *)&((BrUiCtl_ *)((*(PageTab * *)&((BrPhase_ *)((*(UiCont * *)&((BrUiCtl_ *)(this))->pOwner)))->pCur)->a18[(*(short (*)[25])&((BrUiCtl_ *)(this))->aChild)[i]]))->wStep) = saved;
                        (*(int *)&((BrUiCtl_ *)((*(PageTab * *)&((BrPhase_ *)((*(UiCont * *)&((BrUiCtl_ *)(this))->pOwner)))->pCur)->a18[(*(short (*)[25])&((BrUiCtl_ *)(this))->aChild)[i]]))->f2974) = 0;
                        (*(int *)&((BrUiCtl_ *)((*(PageTab * *)&((BrPhase_ *)((*(UiCont * *)&((BrUiCtl_ *)(this))->pOwner)))->pCur)->a18[(*(short (*)[25])&((BrUiCtl_ *)(this))->aChild)[i]]))->f2970) = 0;
                        (*(int *)&((BrUiCtl_ *)(this))->rcRight) += (*(short *)&((BrUiCtl_ *)((*(PageTab * *)&((BrPhase_ *)((*(UiCont * *)&((BrUiCtl_ *)(this))->pOwner)))->pCur)->a18[(*(short (*)[25])&((BrUiCtl_ *)(this))->aChild)[i]]))->w48);
                        (*(PageTab * *)&((BrPhase_ *)((*(UiCont * *)&((BrUiCtl_ *)(this))->pOwner)))->pCur)->a18[(*(short (*)[25])&((BrUiCtl_ *)(this))->aChild)[i]]->s0C();
                        (*(unsigned int *)&((BrUiCtl_ *)((*(PageTab * *)&((BrPhase_ *)((*(UiCont * *)&((BrUiCtl_ *)(this))->pOwner)))->pCur)->a18[(*(short (*)[25])&((BrUiCtl_ *)(this))->aChild)[i]]))->flags1C) &= ~0x20000u;
                    }
                    s08();
                    return 1;
                }
            } else {
                if ((*(unsigned int *)&((BrUiCtl_ *)(this))->flags1C) & 0x400000)
                    (*(unsigned short *)&((BrUiCtl_ *)(this))->w1E20C) = (*(short *)&((BrUiCtl_ *)(this))->aStepId[0]);
                if (!((*(unsigned int *)&((BrUiCtl_ *)(this))->flags1C) & 4) && !((*(unsigned int *)&((BrUiCtl_ *)(this))->flags1C) & 0x20000)) {
                    (*(unsigned short *)&((BrUiCtl_ *)(this))->wStep) = 0;
                    if (((*(unsigned int *)&((BrUiCtl_ *)(this))->flags1C) & 0x100000) && !((*(unsigned int *)&((BrUiCtl_ *)(this))->flags1C) & 0x10) && (*(int (**)(UiPage *))&((BrUiCtl_ *)(this))->pfn0C) != 0) {
                        (*(unsigned short *)&((BrUiCtl_ *)(this))->w1E20C) = 3;
                        (*(char *)&((BrUiCtl_ *)(this))->aText[0].f08) = 1;
                        s08();
                        return 1;
                    }
                } else {
                    if ((*(int (**)(UiPage *))&((BrUiCtl_ *)(this))->pfn0C))
                        (*(int (**)(UiPage *))&((BrUiCtl_ *)(this))->pfn0C)(this);
                }
            }
            s08();
            return 1;
        }
    }
    s08();
    return 1;
}

/* C entry points (generated by ports/64b/tools/methodfwd.py) */
extern "C" int BrUiFrame_10048180(void *self)
{
    return ((class UiPage *)self)->Frame();
}
/* end of C entry points */
