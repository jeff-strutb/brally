#include "br_coretypes.h"   /* br_globals: its objects */
#include "slice3_39.h"   /* BrTextList, the canonical record */
#include "br_phase.h"   /* BrPhase_, the canonical record */
#include "br_ui.h"   /* br_globals: its objects */
/* WHAT IT DOES: build one menu page: creates the page container, adds every
 * control on it in turn, and reports failure if any of them could not be
 * made. One of a family of page builders, each laying out its own screen,
 * and it loads the season data file before laying out, so the page can show
 * what is in it. */
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
class Item2B5C;

class Item2B5C {
public:
    virtual void  s0();
    virtual void  s1();         /* +0x04 relayout */
    virtual void  s2();
    virtual void  s3();
    virtual void  s4();
    virtual void  s5();
    virtual void  s6();
    virtual void  s7();
    virtual void  s8();
    virtual void  s9();
    virtual float s10();
    virtual void  s11();

    int   f004;
    char  b008;
    char  szName[0x401];        /* +0x009 */
    short w40A;
    short w40C;
    short pad40E;
    int   f410;
    int   f414;
    int   f418;
    short w41C;                 /* +0x41C */
    short pad41E;
    int   f420;
    int   a424[4];              /* +0x424 */
    int   f434;
};



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
    SlotTable48F10 *pTableC4;       /* +0xC4 */
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
    int (*pfn10)(BrCtl *);          /* +0x10 */
    int (*pfn14)(BrCtl *);          /* +0x14 */
    int (*pfn18)(BrCtl *);          /* +0x18 */
    char pad1C[0x50 - 0x1C];        /* +0x1C */
    int  f050;                      /* +0x050 */
    int  f054;
    int  f058;
    int  f05C;
    char pad60[0x2AB4 - 0x60];      /* +0x060 */
    short w2AB4;                    /* +0x2AB4 */
    short w2AB6[0x19];              /* +0x2AB6 */
    char pad2AE8[0x2B5C - 0x2AE8];  /* +0x2AE8 */
    Item2B5C m2B5C;                 /* +0x2B5C -- the 0x438 item record */
    char pad2F94[0x3838 - 0x2F94];  /* +0x2F94 */
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
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: g_brGate5BA0 is defined once, in br_globals.c */
/* 64-bit core: g_brRoot5C60 is defined once, in br_globals.c */
/* 64-bit core: g_brCtl5D18 is defined once, in br_globals.c */
/* 64-bit core: g_AA29F4 is defined once, in br_globals.c */
/* 64-bit core: g_i0AB3F4 is defined once, in br_globals.c */
/* BrStrGet: prototype in br_funcs.h */
/* FUN_100378c0: prototype in br_funcs.h */
/* BrSub10047360: prototype in br_funcs.h */
/* BrPhaseDispatch_100450F0: prototype in br_funcs.h */
/* BrPhaseEdit_10047210: prototype in br_funcs.h */
/* BrUiHook81_10046EB0: prototype in br_funcs.h */
/* BrUiHook85_1003E7A0: prototype in br_funcs.h */
/* BrUiHook85_1003EB10: prototype in br_funcs.h */
/* BrMenuFlags18D0: prototype in br_funcs.h */
/* BrMenuText0A50: prototype in br_funcs.h */
/* BrMenuText0AC0: prototype in br_funcs.h */
/* BrMenuText1300: prototype in br_funcs.h */
/* FUN_1003b350: prototype in br_funcs.h */
/* FUN_1003b580: prototype in br_funcs.h */
}

int BrExt_10050060(GameUi *parent)
{
    Page48F10 *cont;
    BrCtl     *p;
    char       bad;

    g_brGate5BA0 = 1;
    (*(SlotTable48F10 * *)&((BrPhase_ *)((*(Root48F10 * *)&g_2908)))->fC0)->s1((char *)(&(s_RallySeason_BRF[0])));
    g_brGate5BA0 = 0;

    (*(short *)&((BrPhase_ *)(parent))->iPage) = 0;
    g_AB94 = -1;
    (*(int (*)[1])&((BrPhase_ *)(parent))->aFlags[0])[(*(unsigned short *)&((BrPhase_ *)(parent))->nPages)] = 1;

    cont = ((Page48F10 *)br_new_obj(sizeof(BrUiPage_), (void *(*)(void *))BrUiPageCtor_10048470));
    (*(Page48F10 * (*)[22])&((BrPhase_ *)(parent))->aPages[0])[(*(unsigned short *)&((BrPhase_ *)(parent))->nPages)] = cont;
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
    p->s34((char *)(BrStrGet(0x39)), 1, 1, (char *)(&DAT_100aaca8));
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;

    p = ((BrCtl *)br_new_obj(sizeof(BrUiCtl_), (void *(*)(void *))BrMenuObjCtor_10040B10));
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, (*(float *)&((BrUiPage_ *)(cont))->fX), (*(float *)&((BrUiPage_ *)(cont))->fY), 0x3001, 2, 5, 1, -1);
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn04) = (CtlFn)BrUiHook85_1003EB10;
    (*(int *)&((BrUiCtl_ *)(p))->list.f1A99C[8]) = 1;
    (*(class Sel3838 *)&((BrUiCtl_ *)(p))->list).s5(0x40001, &DAT_100aac78, 5, 0, -1);
    (*(int (**)(void))&((BrTextList *)&((*(class Sel3838 *)&((BrUiCtl_ *)(p))->list)))->f04) = (int (*)(void))BrSaveProbeRallySeason;
    ((BrTextList *)&((*(class Sel3838 *)&((BrUiCtl_ *)(p))->list)))->f14 = (BrTextListCbFn)BrSaveNameCommitRallySeason;
    {
        int off = 0;

        do {
            char *psz = &(*(SlotTable48F10 * *)&((BrPhase_ *)((*(Root48F10 * *)&g_2908)))->fC0)->aRecs[off];

            if (psz != 0)
                (*(class Sel3838 *)&((BrUiCtl_ *)(p))->list).s4(psz, 0, 1, &DAT_100aac78, 0);
            off += 0x104;
        } while (off < 26000);
    }
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;
    (*(short *)&((BrUiPage_ *)(cont))->cSel) += 1;

    p = ((BrCtl *)br_new_obj(sizeof(BrUiCtl_), (void *(*)(void *))BrMenuObjCtor_10040B10));
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, (*(float *)&((BrUiPage_ *)(cont))->fX), (*(float *)&((BrUiPage_ *)(cont))->fY) - DAT_10077658, 0x103011, 2, 5, 1, -1);
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn0C) = (CtlFn)BrSub10047360;
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn08) = (CtlFn)BrPhaseDispatch_100450F0;
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn04) = (CtlFn)BrMenuFlags18D0;
    (*(unsigned short *)&((BrUiCtl_ *)(p))->w1E20C) = 2;
    p->s34((char *)(BrStrGet(0x1E)), 1, 0, (char *)(&(*(char *)&g_hot0)));
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;
    (*(short *)&((BrUiPage_ *)(cont))->cSel) += 1;

    p = ((BrCtl *)br_new_obj(sizeof(BrUiCtl_), (void *(*)(void *))BrMenuObjCtor_10040B10));
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, (*(float *)&((BrUiPage_ *)(cont))->fX), (*(float *)&((BrUiPage_ *)(cont))->fY) - DAT_1007765c, 0x102001, 2, 5, 1, -1);
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn0C) = (CtlFn)BrSub10047360;
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn08) = (CtlFn)BrUiHook81_10046EB0;
    (*(unsigned short *)&((BrUiCtl_ *)(p))->w1E20C) = 3;
    p->s34((char *)(BrStrGet(0x0C)), 1, 1, (char *)(&(*(char *)&g_hot0)));
    (*(BrCtl * *)&g_brUipAA29F4) = (BrCtl *)((BrUiCtl_ *)((BrCtl *)((BrUiCtl_ *)(p))));
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;
    (*(short *)&((BrUiPage_ *)(cont))->cSel) += 1;

    p = ((BrCtl *)br_new_obj(sizeof(BrUiCtl_), (void *(*)(void *))BrMenuObjCtor_10040B10));
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 80.0f, 46.0f, 9, 2, 5, 0, 6);
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;

    p = ((BrCtl *)br_new_obj(sizeof(BrUiCtl_), (void *(*)(void *))BrMenuObjCtor_10040B10));
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 330.0f, 153.0f, 0x100009, 2, 5, 1, -1);
    (*(unsigned short *)&((BrUiCtl_ *)(p))->w1E20C) = 3;
    p->s34((char *)(BrStrGet(0x36)), 1, 1, (char *)(&DAT_100aac08));
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;

    p = ((BrCtl *)br_new_obj(sizeof(BrUiCtl_), (void *(*)(void *))BrMenuObjCtor_10040B10));
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 330.0f, 97.0f, 0x5001, 2, 5, 1, -1);
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn04) = (CtlFn)BrMenuText0A50;
    (*(unsigned short *)&((BrUiCtl_ *)(p))->w1E20C) = 5;
    p->s34(&(g_aBr39B720[0]), 1, 3, (char *)(&DAT_100aac08));
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;

    p = ((BrCtl *)br_new_obj(sizeof(BrUiCtl_), (void *(*)(void *))BrMenuObjCtor_10040B10));
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 440.0f, 181.0f, 0x100009, 2, 5, 1, -1);
    (*(unsigned short *)&((BrUiCtl_ *)(p))->w1E20C) = 3;
    p->s34((char *)(BrStrGet(0x37)), 1, 1, &DAT_100aac28);
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;

    p = ((BrCtl *)br_new_obj(sizeof(BrUiCtl_), (void *(*)(void *))BrMenuObjCtor_10040B10));
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 440.0f, 129.0f, 0x5001, 2, 5, 1, -1);
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn04) = (CtlFn)BrMenuText0AC0;
    (*(unsigned short *)&((BrUiCtl_ *)(p))->w1E20C) = 5;
    p->s34(&(g_aBr39B720[0]), 1, 3, &DAT_100aac28);
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;

    p = ((BrCtl *)br_new_obj(sizeof(BrUiCtl_), (void *(*)(void *))BrMenuObjCtor_10040B10));
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 440.0f, 243.0f, 0x100009, 2, 5, 1, -1);
    (*(unsigned short *)&((BrUiCtl_ *)(p))->w1E20C) = 3;
    p->s34((char *)(BrStrGet(0x38)), 1, 1, (char *)(&DAT_100aac18));
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;

    p = ((BrCtl *)br_new_obj(sizeof(BrUiCtl_), (void *(*)(void *))BrMenuObjCtor_10040B10));
    (*(BrCtl * (*)[199])&((BrUiPage_ *)(cont))->apCtl[0])[(*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl)] = p;
    bad = (p == 0);
    if (bad)
        FUN_100378c0(4);
    p->s38(parent, 440.0f, 224.0f, 0x5001, 2, 5, 1, -1);
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn04) = (CtlFn)BrMenuText1300;
    (*(unsigned short *)&((BrUiCtl_ *)(p))->w1E20C) = 0x34;
    p->s34(&(g_aBr39B720[0]), 1, 4, (char *)(&DAT_100aac18));
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;

    (*(int (*)[1])&((BrPhase_ *)(parent))->aFlags[0])[(*(unsigned short *)&((BrPhase_ *)(parent))->nPages)] = 0;

    cont = ((Page48F10 *)br_new_obj(sizeof(BrUiPage_), (void *(*)(void *))BrUiPageCtor_10048470));
    (*(Page48F10 * (*)[22])&((BrPhase_ *)(parent))->aPages[0])[(*(unsigned short *)&((BrPhase_ *)(parent))->nPages)] = cont;
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
    p->s38(parent, 0, 232.0f, 0x100009, 2, 5, 1, -1);
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn0C) = (CtlFn)BrSub10047360;
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn04) = (CtlFn)BrPhaseEdit_10047210;
    (*(int (**)(BrCtl *))&((BrUiCtl_ *)(p))->pfn14) = (CtlFn)BrUiHook85_1003E7A0;
    (*(unsigned short *)&((BrUiCtl_ *)(p))->w1E20C) = 3;
    p->s34((char *)(BrStrGet(0x3A)), 1, 1, &DAT_100aabd8);
    (*(BrCtl * *)&DAT_10ac5d18) = p;
    (*(unsigned short *)&((BrUiPage_ *)(cont))->cCtl) += 1;

    return 1;
}
