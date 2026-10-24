/* rm_menuwide.c: Boss Rally's menus as wide as the window, as Top Gear Rally
 * Remastered (ports/remastered) has them with Widescreen on.
 *
 * The menus are one 640x480 16-bit picture the game blits its pages into
 * (DAT_10ac5d84) and writes to the screen whole (BrMenuCallback41DB0).  Here
 * that picture is as wide as the window at the game's height
 * (plat_menu_width, br_gfx.c: 854 at 16:9), and every page keeps its 640
 * columns, centred: each blit lands rm_menu_ox() further right
 * (BrSprFontDraw), the pointer reaches across the whole width (BrGlNavPoll),
 * and the LFB write places the wide picture centred on the game's
 * (grLfbWriteRegion).
 *
 * The keys (and a controller, its d-pad and stick the arrows: race_handoff.c)
 * choose a page's row only while the pointer is off the rows, the pointer
 * winning wherever it is (BrUiNavCtlHit_10047A60).  So an arrow or Return
 * puts the pointer away, off every row and not drawn (rm_menu_nav,
 * BrGlNavPoll; BrSprFontDraw), until the mouse moves or presses again.
 *
 * The backdrop (work1a.bmp, every page's first sprite, the whole 640x480)
 * is widened rather than boxed: its edges are black, and its two vertical
 * bars (columns 156-191, dark to light, and 313-348, light to dark: a pair
 * either side of the badge) are carried on across the width at the same
 * pitch, 157, alternating as they do, behind everything the picture draws:
 * inside the 640 columns only on the black that reaches the picture's edge
 * (a flood from its border over pure black), never on the black of the
 * logo, the panel or a screw; outside them, everywhere.  The bars' colours
 * are the picture's own, its bottom row's.  Whatever picture the backdrop
 * sprite holds (the loading screen's too) is widened the same way; one with
 * no bars there carries none on. */
#include <stdlib.h>
#include <string.h>
#include "br_coretypes.h"
#include "br_surf.h"
#include "slice2_25.h"

int plat_menu_width(void);              /* platform/common/br_gfx.c */

#define GW 640                          /* the pages' width */
#define PITCH 157
#define BAR 36
#define BAR_L 156                       /* the backdrop's own two bars */
#define BAR_R 313

static struct {
    uint64_t hash;                      /* the backdrop it was made from (its pixels: the game
                                           reloads the sprite's picture, the same memory may
                                           come back with another) */
    int w;
    BrSurf *wide;
} s_bd;

static uint64_t hash(const BrSurf *s)
{
    uint64_t h = 1469598103934665603ull;
    size_t i, n = (size_t)s->cx * (size_t)s->cy;
    for (i = 0; i < n; i++)
        h = (h ^ s->pPix[i]) * 1099511628211ull;
    return h;
}

/* how far right the pages sit in the menus' picture */
int rm_menu_ox(void)
{
    const BrSurf *s = (const BrSurf *)DAT_10ac5d84;
    return s && s->cx > GW ? (s->cx - GW) / 2 : 0;
}

/* the menus' picture at the window's width, between frames (its next frame
   draws the whole of it: the backdrop first) */
void rm_menu_frame(void)
{
    BrSurf *s = (BrSurf *)DAT_10ac5d84, *n;
    int w = plat_menu_width();
    if (!s || s->cx == w || s->cx < GW)
        return;
    if (!(n = BrSurfNew(w, s->cy)))
        return;
    memset(n->pPix, 0, (size_t)w * (size_t)n->cy * 2u);
    n->key = s->key;
    BrSurfFree(s);
    DAT_10ac5d84 = n;
}

/* the backdrop widened to w (see the top) */
static BrSurf *widen(const BrSurf *src, int w)
{
    int h = src->cy, o = (w - GW) / 2, x, y, k, n = 0;
    unsigned char *bg = (unsigned char *)calloc((size_t)GW * (size_t)h, 1);
    int *q = (int *)malloc((size_t)GW * (size_t)h * sizeof(int));
    BrSurf *d = BrSurfNew(w, h);
    uint16_t prof[2][BAR];
    if (!bg || !q || !d || src->cx != GW) {
        free(bg);
        free(q);
        if (d)
            BrSurfFree(d);
        return 0;
    }
    /* the background: pure black reached from the border */
#define SEED(X, Y) do { int i_ = (Y) * GW + (X); \
        if (!bg[i_] && src->pPix[i_] == 0) { bg[i_] = 1; q[n++] = i_; } } while (0)
    for (x = 0; x < GW; x++) {
        SEED(x, 0);
        SEED(x, h - 1);
    }
    for (y = 0; y < h; y++) {
        SEED(0, y);
        SEED(GW - 1, y);
    }
    for (k = 0; k < n; k++) {
        int i = q[k];
        x = i % GW;
        y = i / GW;
        if (x > 0) SEED(x - 1, y);
        if (x < GW - 1) SEED(x + 1, y);
        if (y > 0) SEED(x, y - 1);
        if (y < h - 1) SEED(x, y + 1);
    }
#undef SEED
    for (k = 0; k < BAR; k++) {         /* the bars' colours: the bottom row's */
        prof[0][k] = src->pPix[(h - 1) * GW + BAR_L + k];
        prof[1][k] = src->pPix[(h - 1) * GW + BAR_R + k];
    }
    memset(d->pPix, 0, (size_t)w * (size_t)h * 2u);
    for (y = 0; y < h; y++)
        memcpy(d->pPix + (size_t)y * w + o, src->pPix + (size_t)y * GW, GW * 2);
    /* the bars on, at the pitch, either way from the backdrop's own */
    for (k = -(o / PITCH + 2); k <= (w - o) / PITCH + 2; k++) {
        int bx = BAR_L + k * PITCH;     /* in the backdrop's columns */
        const uint16_t *pr = prof[k & 1];
        if (k == 0 || k == 1)
            continue;                   /* (its own) */
        for (x = 0; x < BAR; x++) {
            int gx = bx + x, cx = gx + o;
            if (cx < 0 || cx >= w)
                continue;
            for (y = 0; y < h; y++)
                if (gx < 0 || gx >= GW || bg[y * GW + gx])
                    d->pPix[(size_t)y * w + cx] = pr[x];
        }
    }
    d->key = src->key;
    free(bg);
    free(q);
    return d;
}

/* BrSprFontDraw's: 1 when this blit was the backdrop, drawn here across the
   whole picture */
int rm_menu_backdrop(int x, int y, struct BrSurf *src, const int *rect)
{
    BrSurf *dst = (BrSurf *)DAT_10ac5d84;
    int full[4];
    uint64_t hs;
    if (!dst || dst->cx <= GW || !src || x != 0 || y != 0 || src->cx != GW || src->cy != dst->cy ||
        rect[0] != 0 || rect[1] != 0 || rect[2] != GW || rect[3] != src->cy)
        return 0;
    hs = hash(src);
    if (s_bd.hash != hs || s_bd.w != dst->cx || !s_bd.wide) {
        if (s_bd.wide)
            BrSurfFree(s_bd.wide);
        s_bd.wide = widen(src, dst->cx);
        s_bd.hash = hs;
        s_bd.w = dst->cx;
        if (!s_bd.wide)
            return 0;
    }
    full[0] = 0;
    full[1] = 0;
    full[2] = dst->cx;
    full[3] = dst->cy;
    BrUiSprBlit(dst, 0, 0, s_bd.wide, full, 0);
    return 1;
}

/* ---- the keys and the pointer ---------------------------------------------- */
extern int plat_pointer_seq;            /* dx.c */
void plat_mouse_park(void);
static int s_keys;                      /* the keys have the menus: the pointer put away */

/* BrGlNavPoll's, after its reading: the cursor at (*x, *y) */
void rm_menu_nav(int *x, int *y)
{
    static int seq;
    static const int k[] = { 200, 208, 203, 205, 28, 156 };   /* the arrows, Return */
    size_t i;
    if (plat_pointer_seq != seq) {
        seq = plat_pointer_seq;
        s_keys = 0;
    }
    for (i = 0; i < sizeof k / sizeof k[0]; i++)
        if (g_BrDikEdge[k[i]])
            s_keys = 1;
    if (s_keys) {
        plat_mouse_park();
        *x = -rm_menu_ox();             /* the picture's top left corner: no row there */
        *y = 0;
    }
}

/* the cursor's sprite is not drawn while the keys have the menus */
int rm_menu_pointer_hidden(void) { return s_keys; }
