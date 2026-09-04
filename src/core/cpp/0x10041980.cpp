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
extern unsigned short DAT_10ac5bc4;
extern unsigned short DAT_10ac5bc8;
extern unsigned short DAT_100aab7c;
extern int            DAT_10ac5c30;
extern char           DAT_10ac5c00;
void *memset(void *, int, unsigned int);
}

int Phase32F::Frame()
{
    int i;

    if (f04) f04();
    if (f0C) f0C();
    DAT_10ac5bc8 = 0;
    Adv();

    for (i = 0; i < f14; ++i) {
        UiPage *p = a18[i];

        if (p == 0)
            goto fail;
        if (p->pfn14) {
            if (p->pfn14(p) == 0)
                goto fail;
        }
        if (p->f1C & 0x1000) {
            p->s1();
            if (p->pfn04)
                p->pfn04(p);
            if (p->f1C & 0x10) {
                if (DAT_10ac5bc4 == DAT_10ac5bc8) {
                    DAT_10ac5bc4 = (unsigned short)(DAT_10ac5bc4
                                                    + DAT_100aab7c);
                    Adv();
                }
                DAT_10ac5bc8 = DAT_10ac5bc8 + 1;
            }
            if (!(p->f1C & 0x10))
                goto latch;
        }
        if (p->f1C & 0x800)
            goto latch;
        if (p->s3() == 0) {
            DAT_10ac5c00 = 0;
            goto fail;
        }
        if ((p->f1C & 0x6000)
            && (f340->fBC == i || (p->f1C & 0x4000))) {
            if (p->w2AB4 > 0) {
                int j;
                for (j = 0; j < p->w2AB4; ++j)
                    a18[p->w2AB6[j]]->s3();
            }
        }
        if (p->pfn18) {
            if (p->pfn18(p) == 0)
                goto fail;
        }
        if ((p->f1C & 0x20) && DAT_10ac5c30 == 0 && (p->f1C & 0x2000)) {
            UiCtx *c = f340;
            if (c->fBC != i) {
                c->fBC = (unsigned short)i;
                memset((char *)f340 + 0x6C, 0, 0x50);
                f340->f6C = 1;
            }
        }
    latch:;
    }

    if (f08) f08();
    return 1;

fail:
    return 0;
}
