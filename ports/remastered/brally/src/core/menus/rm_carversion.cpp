/* rm_carversion.cpp: Car Setup's VERSION row in Boss Rally's own menus, as Top
 * Gear Rally Remastered (ports/remastered) has them when Boss Rally's menus are
 * the app's.
 *
 * Japan's cartridge of Top Gear Rally replaced three cars with its own:
 * Type-PJ for Type-RS, Type-TR for Type-M3, Type-MN for the Helmet.  Boss
 * Rally has all six, Type-RS, M3 and the Helmet its cars 3, 6 and 9 and
 * Type-PJ, TR and MN its 13, 14 and 15 (and so again in the upper sixteen of
 * its thirty-two).  Remastered's Top Gear Rally menus offer each pair as one
 * car whose up and down choose the cartridge's version (tgrally/src/menus/
 * rmregion.c); here the CAR row steps the cars with each pair as one, the
 * pair's USA entry standing for it (br_optcycle.c, BrOptCycleTrack), and a
 * VERSION row under CAR says USA or JAPAN for a pair (a dash for any other
 * car) and turns to the other on Return or a click.  Each version is offered
 * as Boss Rally's own unlocks offer it (BrOptAvailB); a pair is on the CAR row
 * when either version is.  The version last chosen is kept for the next pair
 * the CAR row comes to.
 *
 * The row goes in both Car Setup pages, one player's (BrPhaseEnterPlaceholder_
 * 1004A580) and the network game's (FUN_10051600), under CAR; the rows below
 * it move down one. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "br_coretypes.h"
#include "br_phase.h"
#include "br_ui.h"
#include "slice2_25.h"

class GameUi;

class RmVPage {                         /* the page container (Phase32F's layout) */
public:
    virtual void v0();
    int (*pfnA)(void);
    int (*pfnB)(void);
    int (*pfnC)(void);
    int f10;
    unsigned short w14;
    unsigned short w16;
    void *a18[199];
    void *f334;
    float f338;
    float f33C;
    GameUi *f340;
    short w344;
    short w346;
};

class RmVCtl {                          /* a control (BrCtl's vtable) */
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
    virtual void s34(char *, int, int, char *);
    virtual void s38(GameUi *, float, float, unsigned int, int, int, int, int);
    virtual void s3C();
};

typedef int (*CtlFn)(RmVCtl *);

extern "C" void rm_br_car_announce(int v);   /* br_optcycle.c: a car chosen, told to the other players */

/* the other version of car v's pair: 3 and 13, 6 and 14, 9 and 15 (and 16 on) */
static const unsigned char k_other[16] = { 0, 1, 2, 13, 4, 5, 14, 7, 8, 15, 10, 11, 12, 3, 6, 9 };

static int paired(int v) { return k_other[v & 15] != (v & 15); }
static int japans(int v) { return (v & 15) >= 13; }
static int other(int v) { return (v & 16) | k_other[v & 15]; }

#define NPAGE 4                         /* (the two Car Setup pages: each built once, kept
                                           until the menus shut down) */
static int s_japan;                     /* the version last chosen: Japan's */
static RmVCtl *s_row[NPAGE];            /* each page's VERSION row */
static char s_text[NPAGE][32];          /* its words */

/* the CAR row: car v is a step of it (a pair by its USA entry, when either
   version is unlocked) */
extern "C" int rm_br_car_offered(int v)
{
    if (!paired(v))
        return BrOptAvailB((unsigned)v);
    if (japans(v))
        return 0;
    return BrOptAvailB((unsigned)v) || BrOptAvailB((unsigned)other(v));
}

/* the CAR row's entry for car v: a pair's USA one */
extern "C" int rm_br_car_class(int v)
{
    return paired(v) && japans(v) ? other(v) : v;
}

/* (RM_LOG: the car chosen, a check's) */
static void note(const char *how, int v)
{
    if (getenv("RM_LOG"))
        fprintf(stderr, "remastered: car setup %s: car %d (code %d)\n", how, v, (int)g_aBrAC420[v]);
}

/* the car the CAR row has come to, v: of a pair, the version last chosen when
   it is unlocked, else the other */
extern "C" int rm_br_car_version(int v)
{
    int want;

    if (!paired(v)) {
        note("car", v);
        return v;
    }
    want = japans(v) == s_japan ? v : other(v);
    if (!BrOptAvailB((unsigned)want))
        want = BrOptAvailB((unsigned)other(want)) ? other(want) : v;
    note("car", want);
    return want;
}

/* the rows say the car in force (one car for both pages) */
extern "C" void rm_br_version_relabel(void)
{
    int v = (*(int32_t *)&g_brSel0ABDF4), i;

    for (i = 0; i < NPAGE; i++) {
        if (s_row[i]) {
            snprintf(s_text[i], sizeof s_text[i], "VERSION  %s", !paired(v) ? "-" : japans(v) ? "JAPAN" : "USA");
            s_row[i]->s34(s_text[i], 1, 1, (char *)(&(*(char *)&g_hot0)));
        }
    }
}

/* Return or a click: the pair's other version, when it is unlocked */
static int act_version(RmVCtl *)
{
    int v = (*(int32_t *)&g_brSel0ABDF4), o = other(v);

    if (paired(v) && BrOptAvailB((unsigned)o)) {
        (*(int32_t *)&g_brSel0ABDF4) = o;
        (*(int32_t *)&g_226e7c) = g_aBrAC420[o];
        s_japan = japans(o);
        rm_br_car_announce(o);
        note("version", o);
    }
    rm_br_version_relabel();
    return 1;
}

/* the row, on page `page` under its CAR row (one row step, 19, below it) */
extern "C" void rm_br_version_row(GameUi *parent, void *page)
{
    RmVPage *cont = (RmVPage *)page;
    RmVCtl *p = (RmVCtl *)br_new_obj(sizeof(BrUiCtl_), (void *(*)(void *))BrMenuObjCtor_10040B10);
    int v = (*(int32_t *)&g_brSel0ABDF4), i;

    cont->a18[cont->w14] = p;
    if (p == 0)
        FUN_100378c0(4);
    p->s38(parent, cont->f338, cont->f33C + 19.0f, 0x102001, 2, 5, 1, -1);
    (*(int (**)(RmVCtl *))&((BrUiCtl_ *)(p))->pfn0C) = (CtlFn)BrSub10047360;
    (*(int (**)(RmVCtl *))&((BrUiCtl_ *)(p))->pfn08) = act_version;
    (*(unsigned short *)&((BrUiCtl_ *)(p))->w1E20C) = 3;
    if (paired(v))
        s_japan = japans(v);
    for (i = 0; i < NPAGE - 1 && s_row[i]; i++) {
    }
    s_row[i] = p;                       /* (more pages than there are: the last slot reused) */
    rm_br_version_relabel();
    cont->w14 += 1;
    cont->w344 += 1;
}

/* the menus shut down (a race): the pages go with them */
extern "C" void rm_br_version_release(void)
{
    memset(s_row, 0, sizeof s_row);
}
