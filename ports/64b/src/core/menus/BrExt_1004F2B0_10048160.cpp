#include "br_coretypes.h"   /* br_globals: its objects */
#include "br_phase.h"   /* BrPhase_, the canonical record */
#include "br_ui.h"   /* BrUiCtl_, the canonical record */
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







typedef int (*CtlFn)(BrCtl *);

extern "C" {
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* BrStrGet: prototype in br_funcs.h */
/* FUN_100378c0: prototype in br_funcs.h */
/* BrSub10047360: prototype in br_funcs.h */
/* BrPhaseActivate_10045AF0: prototype in br_funcs.h */
/* BrPhaseHook_10045AA0: prototype in br_funcs.h */
/* BrUiNavHook_10046C90: prototype in br_funcs.h */
}

int BrExt_1004F2B0(GameUi *parent)
{
    Page48160 *cont;
    BrCtl     *p;
    char       bad;

    (*(short *)&((BrPhase_ *)(parent))->iPage) = 0;
    (*(int (*)[1])&((BrPhase_ *)(parent))->aFlags[0])[(*(unsigned short *)&((BrPhase_ *)(parent))->nPages)] = 1;

    cont = ((Page48160 *)br_new_obj(sizeof(Page48160), (void *(*)(void *))BrUiPageCtor_10048470));
    (*(Page48160 * (*)[22])&((BrPhase_ *)(parent))->aPages[0])[(*(unsigned short *)&((BrPhase_ *)(parent))->nPages)] = cont;
    bad = (cont == 0);
    if (bad)
        FUN_100378c0(4);
    (*(unsigned short *)&((BrPhase_ *)(parent))->nPages) += 1;
    (*(GameUi * *)&((BrUiPage_ *)(cont))->pOwner) = parent;
    (*(int *)&((BrUiPage_ *)(cont))->f10) = 0;
    (*(float *)&((BrUiPage_ *)(cont))->fX) = 195.0f;
    (*(float *)&((BrUiPage_ *)(cont))->fY) = 130.0f;

    p = ((BrCtl *)br_new_obj(sizeof(BrCtl), (void *(*)(void *))BrMenuObjCtor_10040B10));
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 0, 0, 9, 2, 5, 0, 0);
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;

    p = ((BrCtl *)br_new_obj(sizeof(BrCtl), (void *(*)(void *))BrMenuObjCtor_10040B10));
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, (*(float *)&((BrUiPage_ *)(cont))->fX), 10.0f, 0x100009, 2, 5, 1, -1);
    (*(unsigned short *)&((BrUiCtl_ *)(p))->w1E20C) = 3;
    p->s34((char *)(BrStrGet(9)), 1, 1, (char *)(&DAT_100aaca8));
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;

    p = ((BrCtl *)br_new_obj(sizeof(BrCtl), (void *(*)(void *))BrMenuObjCtor_10040B10));
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, (*(float *)&((BrUiPage_ *)(cont))->fX), (*(float *)&((BrUiPage_ *)(cont))->fY), 0x102001, 2, 5, 1, -1);
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn0C) = (CtlFn)BrSub10047360;
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn08) = (CtlFn)CtlEF90_fn;
    (*(unsigned short *)&((BrUiCtl_ *)(p))->w1E20C) = 3;
    p->s34((char *)(BrStrGet(10)), 1, 1, (char *)(&(*(char *)&g_hot0)));
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;
    (*(short *)&((BrUiPage_ *)(cont))->cSel) += 1;

    p = ((BrCtl *)br_new_obj(sizeof(BrCtl), (void *(*)(void *))BrMenuObjCtor_10040B10));
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, (*(float *)&((BrUiPage_ *)(cont))->fX), (*(float *)&((BrUiPage_ *)(cont))->fY) - DAT_10077648, 0x102001, 2, 5, 1, -1);
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn0C) = (CtlFn)BrSub10047360;
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn08) = (CtlFn)BrPhaseHook_10045AA0;
    (*(unsigned short *)&((BrUiCtl_ *)(p))->w1E20C) = 3;
    p->s34((char *)(BrStrGet(11)), 1, 1, (char *)(&(*(char *)&g_hot0)));
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;
    (*(short *)&((BrUiPage_ *)(cont))->cSel) += 1;

    p = ((BrCtl *)br_new_obj(sizeof(BrCtl), (void *(*)(void *))BrMenuObjCtor_10040B10));
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, (*(float *)&((BrUiPage_ *)(cont))->fX), (*(float *)&((BrUiPage_ *)(cont))->fY) - DAT_1007765c, 0x102001, 2, 5, 1, -1);
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn0C) = (CtlFn)BrSub10047360;
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn08) = (CtlFn)BrUiNavHook_10046C90;
    (*(unsigned short *)&((BrUiCtl_ *)(p))->w1E20C) = 3;
    p->s34((char *)(BrStrGet(12)), 1, 1, (char *)(&(*(char *)&g_hot0)));
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;
    (*(short *)&((BrUiPage_ *)(cont))->cSel) += 1;

    p = ((BrCtl *)br_new_obj(sizeof(BrCtl), (void *(*)(void *))BrMenuObjCtor_10040B10));
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 80.0f, 46.0f, 9, 2, 5, 0, 6);
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;

    return 1;
}
