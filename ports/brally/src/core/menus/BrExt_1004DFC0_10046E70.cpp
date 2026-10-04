#include "br_coretypes.h"   /* br_globals: its objects */
#include "slice3_39.h"   /* BrTextList, the canonical record */
#include "br_phase.h"   /* BrPhase_, the canonical record */
#include "br_ui.h"   /* br_globals: its objects */
/* WHAT IT DOES: build one menu page: creates the page container, adds every
 * control on it in turn, and reports failure if any of them could not be
 * made. One of a family of page builders, each laying out its own screen. */
/* @implements 0x10046E70 glide BrExt_1004DFC0
 * @cpp_kind free
 * @cpp_symbol ?BrExt_1004DFC0@@YAHPAVGameUi@@@Z
 *
 * 2114 B cdecl EH-frame menu builder, same family and recipe as
 * 0x10048160 / 0x100458D0 / 0x100451F0 / 0x10048F10. Twelve `new BrCtl`
 * entries around one dropdown row that is the only thing in this family
 * needing the ASM rather than the Ghidra draft -- the draft invents a
 * bare `ftol()` call and loses which value feeds which store.
 *
 * What that row actually does:
 *   - clamp the saved index to 0..11 and hand it to the selector's
 *     +0x14 configure vcall;
 *   - fill the list from the twelve name pointers at 0x100B81D0, walked
 *     as POINTERS against the end address (the original compares
 *     `edi < 0x100B8200`), through the selector's +0x10 add-item slot;
 *   - then set +0x1E1E8 three ways off the SAME index, re-read from the
 *     global after the loop: the low bound below 0, the high bound above
 *     11, and otherwise `lo - (hi - lo) * (float)n * k`; and finally
 *     truncate that into +0x1E1C8 with +0x1E1D0 = it + 0x10.
 *
 * Family levers unchanged: CHAR bool after the slot store, raw float
 * pushes for simple lvalues, w14 then w344 tails.
 *
 * Two shapes in that row were recovered from the asm and both matter:
 *  - the CLAMP is one load then an in-place fix (`k = g; if (k >= 0) {
 *    if (k > 11) k = 11; } else k = 0;`). Re-reading the global in each
 *    arm, or naming a separate result, moves it out of the eax
 *    accumulator form and costs 26 diffs.
 *  - the loop's bound test is SIGNED on the pointer (`jl`, i.e. the
 *    original compares it as an int), not the unsigned `jb` a plain
 *    pointer comparison gives.
 *
 * BYTE-EXACT 2026-09-12. What was parked as "a -1 constant-register
 * fork" was two source shapes, neither of them the constant:
 *  - The selector's vtable is a NAMED LOCAL (`vt`) read once, the
 *    configure call goes through it as a pointer-to-member, and the
 *    add-item slot is read into `add` BEFORE the pfn04 store and the
 *    walker init. With a plain virtual call VC5 hoists the slot read
 *    below `pp = names`, so the vptr temp overlaps the walker and cannot
 *    share edi -- it goes to edx and spills. Dropping the configure -1 to
 *    0 (a diagnostic) changed nothing, which is how the -1 theory died.
 *  - The third arm's product is explicitly grouped
 *    `((hi - lo) * n) * k`; flat `(hi - lo) * n * k` is reassociated by
 *    VC5 into `(hi - lo) * k` first (two fxch), the paren pins the tree.
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


typedef void (Sel3838::*CfgPmf)(int, void *, int, int, int);
typedef void (Sel3838::*AddPmf)(char *, int, int, void *, int);


class Page46E70 {
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
    Page46E70();
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
    char pad3850[0x1E1C8 - 0x3850]; /* +0x3850 */
    int   f1E1C8;                   /* +0x1E1C8 */
    char  pad1E1CC[0x1E1D0 - 0x1E1CC];
    int   f1E1D0;                   /* +0x1E1D0 */
    char  pad1E1D4[0x1E1E8 - 0x1E1D4];
    float f1E1E8;                   /* +0x1E1E8 */
    char  pad1E1EC[0x1E1F4 - 0x1E1EC];
    int   f1E1F4;                   /* +0x1E1F4 */
    char  pad1E1F8[0x1E200 - 0x1E1F8];
    float f1E200;                   /* +0x1E200 */
    float f1E204;                   /* +0x1E204 */
    char  pad1E208[0x1E20C - 0x1E208];
    unsigned short w1E20C;          /* +0x1E20C */
    char pad1E20E[6];               /* +0x1E20E */
    BrCtl();
};

class GameUi {
public:
    char pad[0x10];
    unsigned short w10;             /* +0x10 */
    short w12;                      /* +0x12 */
    Page46E70 *a14[22];              /* +0x14 */
    int a6C[1];                     /* +0x6C */
};








typedef int (*CtlFn)(BrCtl *);

extern "C" {
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: g_brSel5D8C is defined once, in br_globals.c */
/* 64-bit core: g_AA29C8 is defined once, in br_globals.c */
/* 64-bit core: g_aBrNames0B81D0 is defined once, in br_globals.c */
/* BrStrGet: prototype in br_funcs.h */
/* FUN_100378c0: prototype in br_funcs.h */
/* BrSub10047360: prototype in br_funcs.h */
/* BrPhaseLeave_100466C0: prototype in br_funcs.h */
/* BrUiHook85_1003E950: prototype in br_funcs.h */
/* BrUiHook85_1003E9E0: prototype in br_funcs.h */
/* BrUiHook85_1003EA40: prototype in br_funcs.h */
/* BrUiHook85_1003EE20: prototype in br_funcs.h */
/* FUN_100476C0: prototype in br_funcs.h */
/* FUN_1003c240: prototype in br_funcs.h */
/* FUN_1003c2b0: prototype in br_funcs.h */
/* FUN_10037fa0: prototype in br_funcs.h */
}

int BrExt_1004DFC0(GameUi *parent)
{
    Page46E70 *cont;
    BrCtl     *p;
    char       bad;
    char      *vt;
    AddPmf     add;

    (*(short *)&((BrPhase_ *)(parent))->iPage) = 0;
    (*(int (*)[1])&((BrPhase_ *)(parent))->aFlags[0])[(*(unsigned short *)&((BrPhase_ *)(parent))->nPages)] = 1;

    cont = ((Page46E70 *)br_new_obj(sizeof(BrUiPage_), (void *(*)(void *))BrUiPageCtor_10048470));
    (*(Page46E70 * (*)[22])&((BrPhase_ *)(parent))->aPages[0])[(*(unsigned short *)&((BrPhase_ *)(parent))->nPages)] = cont;
    bad = (cont == 0);
    if (bad)
        FUN_100378c0(4);
    (*(unsigned short *)&((BrPhase_ *)(parent))->nPages) += 1;
    (*(GameUi * *)&((BrUiPage_ *)(cont))->pOwner) = parent;
    (*(int *)&((BrUiPage_ *)(cont))->f10) = 0;
    (*(float *)&((BrUiPage_ *)(cont))->fX) = 195.0f;
    (*(float *)&((BrUiPage_ *)(cont))->fY) = 130.0f;

    p = ((BrCtl *)br_new_obj(sizeof(BrUiCtl_), (void *(*)(void *))BrMenuObjCtor_10040B10));
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 0, 0, 9, 2, 5, 0, 0);
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;

    p = ((BrCtl *)br_new_obj(sizeof(BrUiCtl_), (void *(*)(void *))BrMenuObjCtor_10040B10));
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, (*(float *)&((BrUiPage_ *)(cont))->fX), 10.0f, 0x100009, 2, 5, 1, -1);
    (*(unsigned short *)&((BrUiCtl_ *)(p))->w1E20C) = 3;
    p->s34((char *)(BrStrGet(0x21)), 1, 1, (char *)(&DAT_100aaca8));
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;

    p = ((BrCtl *)br_new_obj(sizeof(BrUiCtl_), (void *(*)(void *))BrMenuObjCtor_10040B10));
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 80.0f, 46.0f, 9, 2, 5, 0, 9);
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;

    p = ((BrCtl *)br_new_obj(sizeof(BrUiCtl_), (void *(*)(void *))BrMenuObjCtor_10040B10));
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, (*(float *)&((BrUiPage_ *)(cont))->fX), (*(float *)&((BrUiPage_ *)(cont))->fY), 0x3001, 2, 5, 1, -1);
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn04) = (CtlFn)BrUiHook85_1003EE20;
    (*(int *)&((BrUiCtl_ *)(p))->list.f1A99C[8]) = 1;
    {
        int k = g_5D8C;

        if (k >= 0) {
            if (k > 11)
                k = 11;
        } else {
            k = 0;
        }

        /* The original fetches the list's vtable once and calls its +0x14
         * slot (configure), keeping the +0x10 slot (add a row) for the loop. */
        ((BrUiCtl_ *)(p))->list.pVtbl->f14(&((BrUiCtl_ *)(p))->list, 0x40001, &DAT_100aace8, 4, k, -1);
    }
    (*(int (**)(void))&((BrTextList *)&((*(class Sel3838 *)&((BrUiCtl_ *)(p))->list)))->f04) = (int (*)(void))BrUiHook85_1004E810;
    {
        char **pp = g_aBrNames0B81D0;

        do {
            ((BrUiCtl_ *)(p))->list.pVtbl->f10(&((BrUiCtl_ *)(p))->list, *pp, 0, 1, &DAT_100aac78, 1);
            pp++;
        } while ((uintptr_t)pp < (uintptr_t)&g_aBrNames0B81D0[12]);
    }
    if (g_5D8C < 0)
        (*(float *)&((BrUiCtl_ *)(p))->list.f1A99C[5]) = (*(float *)&((BrUiCtl_ *)(p))->list.f1A99C[11]);
    else if (g_5D8C > 11)
        (*(float *)&((BrUiCtl_ *)(p))->list.f1A99C[5]) = (*(float *)&((BrUiCtl_ *)(p))->list.f1A99C[12]);
    else
        (*(float *)&((BrUiCtl_ *)(p))->list.f1A99C[5]) = (*(float *)&((BrUiCtl_ *)(p))->list.f1A99C[11])
                  - (((*(float *)&((BrUiCtl_ *)(p))->list.f1A99C[12]) - (*(float *)&((BrUiCtl_ *)(p))->list.f1A99C[11])) * g_5D8C) * DAT_1007766c;
    (*(int *)&((BrUiCtl_ *)(p))->list.f1A990) = (int)(*(float *)&((BrUiCtl_ *)(p))->list.f1A99C[5]);
    (*(int *)&((BrUiCtl_ *)(p))->list.f1A998) = (*(int *)&((BrUiCtl_ *)(p))->list.f1A990) + 0x10;
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;
    (*(short *)&((BrUiPage_ *)(cont))->cSel) += 1;

    p = ((BrCtl *)br_new_obj(sizeof(BrUiCtl_), (void *(*)(void *))BrMenuObjCtor_10040B10));
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, (*(float *)&((BrUiPage_ *)(cont))->fX), (*(float *)&((BrUiPage_ *)(cont))->fY) - DAT_10077654, 0x102001, 2, 5, 1, -1);
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn0C) = (CtlFn)BrSub10047360;
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn08) = (CtlFn)FUN_1003c240;
    (*(unsigned short *)&((BrUiCtl_ *)(p))->w1E20C) = 3;
    p->s34((char *)(BrStrGet(0x2E)), 1, 1, (char *)(&(*(char *)&g_hot0)));
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;
    (*(short *)&((BrUiPage_ *)(cont))->cSel) += 1;

    p = ((BrCtl *)br_new_obj(sizeof(BrUiCtl_), (void *(*)(void *))BrMenuObjCtor_10040B10));
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, (*(float *)&((BrUiPage_ *)(cont))->fX), (*(float *)&((BrUiPage_ *)(cont))->fY) - DAT_10077658, 0x102001, 2, 5, 1, -1);
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn0C) = (CtlFn)BrSub10047360;
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn08) = (CtlFn)FUN_1003c2b0;
    (*(unsigned short *)&((BrUiCtl_ *)(p))->w1E20C) = 3;
    p->s34((char *)(BrStrGet(0x2F)), 1, 1, (char *)(&(*(char *)&g_hot0)));
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;
    (*(short *)&((BrUiPage_ *)(cont))->cSel) += 1;

    p = ((BrCtl *)br_new_obj(sizeof(BrUiCtl_), (void *(*)(void *))BrMenuObjCtor_10040B10));
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, (*(float *)&((BrUiPage_ *)(cont))->fX), (*(float *)&((BrUiPage_ *)(cont))->fY) - DAT_1007765c, 0x102001, 2, 5, 1, -1);
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn0C) = (CtlFn)BrSub10047360;
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn08) = (CtlFn)BrPhaseLeave_100466C0;
    (*(unsigned short *)&((BrUiCtl_ *)(p))->w1E20C) = 3;
    p->s34((char *)(BrStrGet(0x0C)), 1, 1, (char *)(&(*(char *)&g_hot0)));
    (*(BrCtl * *)&g_brUipAA29C8) = (BrCtl *)((BrUiCtl_ *)((BrCtl *)((BrUiCtl_ *)(p))));
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;
    (*(short *)&((BrUiPage_ *)(cont))->cSel) += 1;

    p = ((BrCtl *)br_new_obj(sizeof(BrUiCtl_), (void *(*)(void *))BrMenuObjCtor_10040B10));
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 76.0f, 211.0f, 1, 2, 5, 1, 0x68);
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn04) = (CtlFn)BrUiHook85_1003E950;
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;

    p = ((BrCtl *)br_new_obj(sizeof(BrUiCtl_), (void *(*)(void *))BrMenuObjCtor_10040B10));
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 75.0f, 267.0f, 9, 2, 5, 1, 0x6A);
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;

    p = ((BrCtl *)br_new_obj(sizeof(BrUiCtl_), (void *(*)(void *))BrMenuObjCtor_10040B10));
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 79.0f, 256.0f, 1, 2, 5, 1, 0x6B);
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn04) = (CtlFn)BrUiHook85_1003EA40;
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;

    p = ((BrCtl *)br_new_obj(sizeof(BrUiCtl_), (void *(*)(void *))BrMenuObjCtor_10040B10));
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 339.0f, 90.0f, 0x102001, 2, 5, 1, -1);
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn04) = (CtlFn)BrItemDrawIconRow;
    (*(unsigned short *)&((BrUiCtl_ *)(p))->w1E20C) = 3;
    p->s34((char *)(BrStrGet(0x2E)), 1, 1, (char *)(&DAT_100aac08));
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;

    p = ((BrCtl *)br_new_obj(sizeof(BrUiCtl_), (void *(*)(void *))BrMenuObjCtor_10040B10));
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 339.0f, 128.0f, 0x102001, 2, 5, 1, -1);
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn04) = (CtlFn)BrUiHook85_1003E9E0;
    (*(unsigned short *)&((BrUiCtl_ *)(p))->w1E20C) = 3;
    p->s34((char *)(BrStrGet(0x2F)), 1, 1, (char *)(&DAT_100aac08));
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;

    return 1;
}
