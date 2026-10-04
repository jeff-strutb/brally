#include "br_ui.h"   /* BrUiPage_, the canonical record */
/* WHAT IT DOES: run one frame of a page: its two optional callbacks, the
 * selection wrap, then every child page in turn. */
/* @implements 0x10041980 glide BrUiPageFrame_10048530
 * @cpp_kind method
 * @cpp_symbol ?Frame@Phase32F@@QAEHXZ
 *
 * 468 B thiscall, no stack args. Pre/post hook fnptr calls, a counter
 * reset + Adv() thiscall, then the page loop: gate fnptr, flag-driven
 * slot-1/slot-3 vcalls, the paced-counter block, a short-indexed sublist
 * of slot-3 vcalls (i's register repurposed as the walk pointer), and the
 * f340 focus handoff (memset 0x50 + first-flag). Every flag test re-reads
 * f1C (CSE folds the adjacent ones); f340 is re-read per site.
 */
class UiPage {
public:
    virtual void s0();
    virtual void s1();              /* +0x04 */
    virtual void s2();
    virtual int  s3();              /* +0x0C */
    int (*pfn04)(UiPage *);         /* +0x04 */
    char pad08[0xC];                /* +0x08 */
    int (*pfn14)(UiPage *);         /* +0x14 */
    int (*pfn18)(UiPage *);        /* +0x18 */
    unsigned int f1C;               /* +0x1C */
    char pad20[0x2A94];             /* +0x20 */
    short w2AB4;                    /* +0x2AB4 */
    short w2AB6[1];                 /* +0x2AB6 */
};

class UiCtx {
public:
    char pad[0x6C];
    int  f6C;                       /* +0x6C */
    char pad70[0x4C];               /* +0x70 */
    unsigned short fBC;             /* +0xBC */
};

class Phase32F {
public:
    virtual void v0();
    void (*f04)(void);              /* +0x04 */
    void (*f08)(void);              /* +0x08 */
    void (*f0C)(void);              /* +0x0C */
    char pad10[4];                  /* +0x10 */
    unsigned short f14;             /* +0x14 */
    unsigned short f16;             /* +0x16 */
    UiPage *a18[202];               /* +0x18 */
    UiCtx *f340;                    /* +0x340 */
    void Adv();                     /* 0x10041940 */
    int  Frame();
};

extern "C" {
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
#include <string.h>
}

int Phase32F::Frame()
{
    int i;

    if ((*(void (**)(void))&((BrUiPage_ *)(this))->pfn04)) (*(void (**)(void))&((BrUiPage_ *)(this))->pfn04)();
    if ((*(void (**)(void))&((BrUiPage_ *)(this))->pfn0C)) (*(void (**)(void))&((BrUiPage_ *)(this))->pfn0C)();
    (*(unsigned short *)&g_wAA2870) = 0;
    Adv();

    for (i = 0; i < (*(unsigned short *)&((BrUiPage_ *)(this))->cCtl); ++i) {
        UiPage *p = (*(UiPage * (*)[202])&((BrUiPage_ *)(this))->apCtl[0])[i];

        if (p == 0)
            goto fail;
        if ((*(int (**)(UiPage *))&((BrUiCtl_ *)(p))->pfn14)) {
            if ((*(int (**)(UiPage *))&((BrUiCtl_ *)(p))->pfn14)(p) == 0)
                goto fail;
        }
        if ((*(unsigned int *)&((BrUiCtl_ *)(p))->flags1C) & 0x1000) {
            p->s1();
            if ((*(int (**)(UiPage *))&((BrUiCtl_ *)(p))->pfn04))
                (*(int (**)(UiPage *))&((BrUiCtl_ *)(p))->pfn04)(p);
            if ((*(unsigned int *)&((BrUiCtl_ *)(p))->flags1C) & 0x10) {
                if ((*(unsigned short *)&BrGlNavCur5BC4) == (*(unsigned short *)&g_wAA2870)) {
                    (*(unsigned short *)&BrGlNavCur5BC4) = (unsigned short)((*(unsigned short *)&BrGlNavCur5BC4)
                                                    + (*(unsigned short *)&BrGlNavStepAB7C));
                    Adv();
                }
                (*(unsigned short *)&g_wAA2870) = (*(unsigned short *)&g_wAA2870) + 1;
            }
            if (!((*(unsigned int *)&((BrUiCtl_ *)(p))->flags1C) & 0x10))
                goto latch;
        }
        if ((*(unsigned int *)&((BrUiCtl_ *)(p))->flags1C) & 0x800)
            goto latch;
        if (p->s3() == 0) {
            DAT_10ac5c00 = 0;
            goto fail;
        }
        if (((*(unsigned int *)&((BrUiCtl_ *)(p))->flags1C) & 0x6000)
            && ((*(UiCtx * *)&((BrUiPage_ *)(this))->pOwner)->fBC == i || ((*(unsigned int *)&((BrUiCtl_ *)(p))->flags1C) & 0x4000))) {
            if ((*(short *)&((BrUiCtl_ *)(p))->cChild) > 0) {
                int j;
                for (j = 0; j < (*(short *)&((BrUiCtl_ *)(p))->cChild); ++j)
                    (*(UiPage * (*)[202])&((BrUiPage_ *)(this))->apCtl[0])[(*(short (*)[1])&((BrUiCtl_ *)(p))->aChild[0])[j]]->s3();
            }
        }
        if ((*(int (**)(UiPage *))&((BrUiCtl_ *)(p))->pfn18)) {
            if ((*(int (**)(UiPage *))&((BrUiCtl_ *)(p))->pfn18)(p) == 0)
                goto fail;
        }
        if (((*(unsigned int *)&((BrUiCtl_ *)(p))->flags1C) & 0x20) && g_5C30 == 0 && ((*(unsigned int *)&((BrUiCtl_ *)(p))->flags1C) & 0x2000)) {
            UiCtx *c = (*(UiCtx * *)&((BrUiPage_ *)(this))->pOwner);
            if (c->fBC != i) {
                c->fBC = (unsigned short)i;
                memset((char *)(*(UiCtx * *)&((BrUiPage_ *)(this))->pOwner) + 0x6C, 0, 0x50);
                (*(UiCtx * *)&((BrUiPage_ *)(this))->pOwner)->f6C = 1;
            }
        }
    latch:;
    }

    if ((*(void (**)(void))&((BrUiPage_ *)(this))->pfn08)) (*(void (**)(void))&((BrUiPage_ *)(this))->pfn08)();
    return 1;

fail:
    return 0;
}

/* C entry points (generated by ports/brally/tools/methodfwd.py) */
extern "C" int BrUiPageFrame_10048530(void *self)
{
    return ((class Phase32F *)self)->Frame();
}
/* end of C entry points */
