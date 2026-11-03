/* rm_menuopts.cpp: the Remaster Options in Boss Rally's own menus, as Top Gear
 * Rally Remastered (ports/remastered) has them when Boss Rally's menus are
 * the app's.
 *
 * Options gets a Remaster Options row before Back (the hub's builder,
 * BrPhaseEnterPlaceholder_1004BDC0); it opens this page, built as the game's
 * own options pages are (Game Options, BrExt_1004E830): the wrench in the top
 * left lobe, the title, a row an option in the middle lobe and Back below,
 * and here, in the top right lobe as Options shows the keyboard, while the
 * Menus row is the one chosen the box of the game whose menus it says (Boss
 * Rally's, or Top Gear Rally's USA or Japanese box for the one that handed
 * over), else the box of the game whose soundtrack plays: pictures of the
 * boxes Top Gear Rally's options screen turns (tools/menuicons.py,
 * rm_menuicons.c).
 * While the View Distance row is the one chosen the lobe shows Race Setup's
 * own picture of Coastline instead, as that choice sees it: whole at
 * Unlimited, its far end (the lighthouse and the loop behind it) gone at
 * Original (coast_icon).
 * Each row says its option and the choice in force; Return or a click on it
 * takes the next choice (Music: Boss Rally, TGR USA, TGR PAL/Japan), at once,
 * both games told (the app's options.c, rm_option_set).  Menus chooses
 * between these menus and Top Gear Rally's, the one that handed over (USA or
 * PAL/Japan), taken only when the player leaves the page: Back returns to
 * Options, as Game Options' does (BrOpt6710), and with Top Gear Rally's
 * chosen the game's menus then wait for races again (race_handoff.c,
 * br_menus_leave) and that Top Gear Rally's come back in front, on its main
 * menu.
 *
 * The page's phase lives in a slot of its own; the menus' shutdown frees it
 * with the game's (rm_menuopts_release, BrPhaseShutdown_10048B20). */
#include <stdio.h>
#include <string.h>
#include "br_coretypes.h"
#include "br_phase.h"
#include "br_ui.h"
#include "slice2_25.h"
#include "br_surf.h"
#include "br_uispr.h"

class GameUi;
class RmCtl;

class RmPage {                          /* the page container (Page451F0's layout) */
public:
    virtual void v0();
    int (*pfnA)(void);
    int (*pfnB)(void);
    int (*pfnC)(void);
    int f10;
    unsigned short w14;
    unsigned short w16;
    RmCtl *a18[199];
    RmCtl *f334;
    float f338;
    float f33C;
    GameUi *f340;
    short w344;
    short w346;
};

class RmCtl {                           /* a control (BrCtl's vtable) */
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

class RmOwnerSub {                      /* the control's owner, as BrOpt6710 calls it */
public:
    virtual void s0();
    virtual void s1();
    virtual void s2();
    virtual void s3();
    virtual void s4();
    virtual void s5();
    virtual void s6(int);
    virtual void s7();
};

class RmPhase {
public:
    virtual void *f00(int);
};

typedef int (*CtlFn)(RmCtl *);

/* the app's (ports/remastered/platform/options.h) */
extern "C" int  rm_option(int which);
extern "C" void rm_option_set(int which, int value);
extern "C" void br_menus_leave(void);   /* race_handoff.c */
enum { RM_OPT_WIDESCREEN, RM_OPT_MUSIC, RM_OPT_FPS, RM_OPT_MENUS, RM_OPT_VIEWDIST };
enum { RM_MUSIC_PC, RM_MUSIC_N64, RM_MUSIC_N64_PAL, RM_MUSIC_COUNT };
enum { RM_MENUS_TGR, RM_MENUS_BR, RM_MENUS_TGR_PAL };
extern "C" int mux_tgr_active_game(void);   /* the app's (mux.h): the Top Gear Rally last in use */
enum { MUX_TGR, MUX_BR, MUX_TGRP };
enum { RM_VIEW_ORIGINAL, RM_VIEW_UNLIMITED };

#define SPR_WRENCH  9                   /* the options pages' emblem (op.bmp) */
#define SPR_MUSIC   RM_SPR_BASE         /* the soundtrack's game's box (rm_menuicons.c), or with the
                                           Menus row chosen the box of the menus it says */

/* The page's own pictures, past the game's sprite table (its 145): a
 * control's sprite number from RM_SPR_BASE up draws one of these
 * (BrUiDrawCode_10047930: rm_sprite_draw), colour-keyed as the game's are,
 * centred where the control sits */
#define RM_SPR_BASE 1000

enum { ROW_WIDE, ROW_MUSIC, ROW_FPS, ROW_VIEW, ROW_MENUS, NROW };
static RmCtl *s_row[NROW];
extern "C" const unsigned short rm_menuicon_key;
extern "C" const int rm_usaboxicon_w, rm_usaboxicon_h, rm_jpnboxicon_w, rm_jpnboxicon_h;
extern "C" const int rm_brboxicon_w, rm_brboxicon_h;
extern "C" const unsigned short rm_usaboxicon[], rm_jpnboxicon[], rm_brboxicon[];
extern "C" int rm_menu_ox(void);        /* rm_menuwide.c */
enum { BOX_USA, BOX_JPN, BOX_BR, NBOX };
static BrSurf *s_icon[NBOX];
static int s_menus_tgr;                 /* Menus says Top Gear Rally's: taken on leaving the page */

#define SPR_COAST 17                    /* Race Setup's picture of Coastline */
#define COAST_CUT 60                    /* Original's: its columns left of this gone */
static BrSurf *s_coastcut;

/* View Distance's picture: the game's at Unlimited; at Original a copy of it
 * with the end the lighthouse stands at (its left) the lobe's (the key) */
static BrSurf *coast_icon(int cut)
{
    BrSurf *src = (BrSurf *)g_img[g_aBrUiSprite[SPR_COAST].iImage].surf;
    int x, y;
    if (!cut || src == 0)
        return src;
    if (s_coastcut == 0 && (s_coastcut = BrSurfNew(src->cx, src->cy)) != 0) {
        memcpy(s_coastcut->pPix, src->pPix, (size_t)src->cx * src->cy * 2);
        s_coastcut->key = src->key;
        for (y = 0; y < src->cy; y++)
            for (x = 0; x < COAST_CUT && x < src->cx; x++)
                s_coastcut->pPix[y * src->cx + x] = src->key;
    }
    return s_coastcut;
}

/* a game's box, made a surface the first time */
static BrSurf *icon(int k)
{
    static const int *const k_w[NBOX] = { &rm_usaboxicon_w, &rm_jpnboxicon_w, &rm_brboxicon_w };
    static const int *const k_h[NBOX] = { &rm_usaboxicon_h, &rm_jpnboxicon_h, &rm_brboxicon_h };
    static const unsigned short *const k_px[NBOX] = { rm_usaboxicon, rm_jpnboxicon, rm_brboxicon };
    int w = *k_w[k], h = *k_h[k];
    if (s_icon[k] == 0 && (s_icon[k] = BrSurfNew(w, h)) != 0) {
        memcpy(s_icon[k]->pPix, k_px[k], (size_t)w * h * 2);
        s_icon[k]->key = rm_menuicon_key;
    }
    return s_icon[k];
}

extern "C" int rm_sprite_draw(int i, int x, int y)
{
    int rect[4];
    BrSurf *s = 0;
    if (i != SPR_MUSIC)
        return 1;
    /* the Menus row chosen: the box of the game whose menus these are; else
       of the game whose music plays */
    if (s_row[ROW_MENUS] && (*(unsigned int *)&((BrUiCtl_ *)(s_row[ROW_MENUS]))->flags1C & 0x20))
        s = icon(!s_menus_tgr ? BOX_BR : mux_tgr_active_game() == MUX_TGRP ? BOX_JPN : BOX_USA);
    /* the View Distance row chosen: Coastline, as that choice sees it */
    if (s_row[ROW_VIEW] && (*(unsigned int *)&((BrUiCtl_ *)(s_row[ROW_VIEW]))->flags1C & 0x20))
        s = coast_icon(rm_option(RM_OPT_VIEWDIST) == RM_VIEW_ORIGINAL);
    if (s == 0) {
        int m = rm_option(RM_OPT_MUSIC);
        if ((s = icon(m == RM_MUSIC_PC ? BOX_BR : m == RM_MUSIC_N64 ? BOX_USA : BOX_JPN)) == 0)
            return 1;
    }
    rect[0] = 0;
    rect[1] = 0;
    rect[2] = s->cx;
    rect[3] = s->cy;
    BrUiSprBlit((BrSurf *)DAT_10ac5d84, x - s->cx / 2 + rm_menu_ox(), y - s->cy / 2, s, rect, 1);
    return 1;
}

static void *s_slot;                    /* this page's phase */

static char s_text[NROW][40];

static const char *row_text(int r)
{
    switch (r) {
    case ROW_WIDE:
        snprintf(s_text[r], sizeof s_text[r], "WIDESCREEN  %s", rm_option(RM_OPT_WIDESCREEN) ? "ON" : "OFF");
        break;
    case ROW_MUSIC:
        snprintf(s_text[r], sizeof s_text[r], "MUSIC  %s",
                 rm_option(RM_OPT_MUSIC) == RM_MUSIC_PC ? "BOSS RALLY"
                 : rm_option(RM_OPT_MUSIC) == RM_MUSIC_N64 ? "TGR USA" : "TGR PAL/JAPAN");
        break;
    case ROW_FPS:
        snprintf(s_text[r], sizeof s_text[r], "SHOW FPS  %s", rm_option(RM_OPT_FPS) ? "ON" : "OFF");
        break;
    case ROW_VIEW:
        snprintf(s_text[r], sizeof s_text[r], "VIEW DISTANCE  %s",
                 rm_option(RM_OPT_VIEWDIST) == RM_VIEW_UNLIMITED ? "UNLIMITED" : "ORIGINAL");
        break;
    default:
        snprintf(s_text[r], sizeof s_text[r], "MENUS  %s",
                 !s_menus_tgr ? "BOSS RALLY" : mux_tgr_active_game() == MUX_TGRP ? "TGR PAL/JAPAN" : "TGR USA");
        break;
    }
    return s_text[r];
}

static void relabel(int r)
{
    if (s_row[r])
        s_row[r]->s34((char *)row_text(r), 1, 1, (char *)(&(*(char *)&g_hot0)));
}

static int act_wide(RmCtl *)
{
    rm_option_set(RM_OPT_WIDESCREEN, !rm_option(RM_OPT_WIDESCREEN));
    relabel(ROW_WIDE);
    return 1;
}

static int act_music(RmCtl *)
{
    rm_option_set(RM_OPT_MUSIC, (rm_option(RM_OPT_MUSIC) + 1) % RM_MUSIC_COUNT);   /* the next of the three */
    relabel(ROW_MUSIC);
    return 1;
}

static int act_fps(RmCtl *)
{
    rm_option_set(RM_OPT_FPS, !rm_option(RM_OPT_FPS));
    relabel(ROW_FPS);
    return 1;
}

static int act_view(RmCtl *)
{
    rm_option_set(RM_OPT_VIEWDIST, rm_option(RM_OPT_VIEWDIST) == RM_VIEW_UNLIMITED ? RM_VIEW_ORIGINAL : RM_VIEW_UNLIMITED);
    relabel(ROW_VIEW);
    return 1;
}

static int act_menus(RmCtl *)
{
    s_menus_tgr = !s_menus_tgr;        /* shown now, taken on leaving the page */
    relabel(ROW_MENUS);
    return 1;
}

/* Back: to Options, as Game Options' Back (BrOpt6710); with Top Gear Rally's
 * menus chosen, those, the Top Gear Rally that left */
static int act_back(RmCtl *pCtl)
{
    RmPhase *ph;
    if (s_menus_tgr) {
        s_menus_tgr = 0;
        rm_option_set(RM_OPT_MENUS, mux_tgr_active_game() == MUX_TGRP ? RM_MENUS_TGR_PAL : RM_MENUS_TGR);
        br_menus_leave();
    }
    (*(RmOwnerSub **)&((BrUiCtl_ *)(pCtl))->pOwner)->s7();
    ph = (*(RmPhase **)&g_brPAA29B8);
    if (ph != 0)
        ph->f00(1);
    s_slot = 0;
    memset(s_row, 0, sizeof s_row);
    (*(BrPhase_ **)&g_brPAA29B8) = (BrPhase_ *)g_5C70;
    return 0;
}

/* a new control on the page, placed */
static RmCtl *add(GameUi *parent, RmPage *cont, float x, float y, unsigned int flags, int a6, int spr)
{
    RmCtl *p = (RmCtl *)br_new_obj(sizeof(BrUiCtl_), (void *(*)(void *))BrMenuObjCtor_10040B10);
    cont->a18[cont->w14] = p;
    if (p == 0)
        FUN_100378c0(4);
    p->s38(parent, x, y, flags, 2, 5, a6, spr);
    return p;
}

/* a row: chosen with the keys or the pointer, its action on Return or a click */
static RmCtl *row(GameUi *parent, RmPage *cont, float dy, CtlFn fn, const char *text)
{
    RmCtl *p = add(parent, cont, cont->f338, cont->f33C - dy, 0x102001, 1, -1);
    (*(int (**)(RmCtl *))&((BrUiCtl_ *)(p))->pfn0C) = (CtlFn)BrSub10047360;
    (*(int (**)(RmCtl *))&((BrUiCtl_ *)(p))->pfn08) = fn;
    (*(unsigned short *)&((BrUiCtl_ *)(p))->w1E20C) = 3;
    p->s34((char *)text, 1, 1, (char *)(&(*(char *)&g_hot0)));
    cont->w14 += 1;
    cont->w344 += 1;
    return p;
}

static const float k_dy[] = { 0.0f, -19.0f, -38.0f, -57.0f, -76.0f, -95.0f, -114.0f };  /* DAT_10077648.. */

static int enter(GameUi *parent)
{
    RmPage *cont;
    RmCtl *p;
    BrPhase_ *ph = (BrPhase_ *)parent;

    *(short *)&ph->iPage = 0;
    (*(int (*)[1])&ph->aFlags[0])[*(unsigned short *)&ph->nPages] = 1;
    cont = (RmPage *)br_new_obj(sizeof(BrUiPage_), (void *(*)(void *))BrUiPageCtor_10048470);
    (*(RmPage * (*)[22])&ph->aPages[0])[*(unsigned short *)&ph->nPages] = cont;
    if (cont == 0)
        FUN_100378c0(4);
    *(unsigned short *)&ph->nPages += 1;
    cont->f340 = parent;
    cont->f10 = 0;
    cont->f338 = 195.0f;
    cont->f33C = 130.0f;

    /* the backdrop's frame, as every page starts */
    add(parent, cont, 0, 0, 9, 0, 0);
    cont->w14 += 1;
    /* the title */
    p = add(parent, cont, cont->f338, 10.0f, 0x100009, 1, -1);
    (*(unsigned short *)&((BrUiCtl_ *)(p))->w1E20C) = 3;
    p->s34((char *)"REMASTER OPTIONS", 1, 1, (char *)(&DAT_100aaca8));
    cont->w14 += 1;

    s_row[ROW_WIDE] = row(parent, cont, k_dy[0], act_wide, row_text(ROW_WIDE));
    s_row[ROW_MUSIC] = row(parent, cont, k_dy[1], act_music, row_text(ROW_MUSIC));
    s_row[ROW_FPS] = row(parent, cont, k_dy[2], act_fps, row_text(ROW_FPS));
    s_row[ROW_VIEW] = row(parent, cont, k_dy[3], act_view, row_text(ROW_VIEW));
    s_row[ROW_MENUS] = row(parent, cont, k_dy[4], act_menus, row_text(ROW_MENUS));
    p = row(parent, cont, k_dy[6], act_back, BrStrGet(0x0C));
    (*(RmCtl **)&g_brUipAA29C8) = p;     /* Escape's row, as the game's pages name their Back */

    /* the emblem, top left, and the soundtrack's machine, top right */
    add(parent, cont, 80.0f, 46.0f, 9, 0, SPR_WRENCH);
    cont->w14 += 1;
    add(parent, cont, 392.0f, 112.0f, 9, 0, SPR_MUSIC);   /* (its centre: the lobe's) */
    cont->w14 += 1;
    return 1;
}

/* Options' Remaster Options row: this page, made once and kept in its slot */
extern "C" int rm_menuopts_fn(void *)
{
    BrPhase_ *p = (BrPhase_ *)s_slot;
    if (p == 0) {
        p = (BrPhase_ *)br_new_obj(sizeof(BrPhase_), (void *(*)(void *))BrOptObjCtor);
        s_slot = p;
        (*(BrPhase_ **)&g_brPAA29B8) = p;
        if (p == 0)
            return 0;
        *(PhaseEnterFn *)&p->pfnEnter = (PhaseEnterFn)enter;
        (*(PhaseEnterFn *)&p->pfnEnter)((Phase *)p);
        *(int *)&p->f0C = 1;
        *(int *)&p->f68 = 1;
    } else {
        (*(BrPhase_ **)&g_brPAA29B8) = p;
    }
    return 1;
}

/* the menus are the app's again: the rows say what is in force now (the
 * other game's options may have changed them) */
extern "C" void rm_menuopts_refresh(void)
{
    int r;
    for (r = 0; r < NROW; r++)
        relabel(r);
}

/* the menus shut down (a race): this page goes with the game's */
extern "C" void rm_menuopts_release(void)
{
    if (s_slot != 0) {
        BR_VFN(s_slot, 7, void (*)(void *))(s_slot);
        br_vdelete(s_slot);
        s_slot = 0;
    }
    memset(s_row, 0, sizeof s_row);
    s_menus_tgr = 0;
}
