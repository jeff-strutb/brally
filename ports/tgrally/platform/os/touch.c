/* touch.c: the game's menus by touch (tgr_touch.h).
 *
 * The targets the game names while it builds a frame are kept with that
 * frame: handed over with its display list (tgr_touch_built), then on the
 * screen with the frame's shape once the RCP has drawn it (tgr_touch_shown).
 * A tap is matched against what is on the screen, in the game's pixels: a
 * race frame stretched over a wide window moves its 2D the way gfx/rcp.c
 * places it (held at the edge or the centre it sat nearest).
 *
 * What a tap does is played to the pad as presses of six retraces with six
 * between, a quick thumb: a button, the stick pushed one way, or, for a row
 * of a list, the stick down or up one row at a time (the cursor read back
 * from each new frame, so rows a list skips are skipped) and then A.  A tap
 * that meets no target is A when the screen has no items to choose between
 * (a title, a results screen: tap to go on), else nothing. */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "host.h"
#include "tgr_touch.h"

enum { T_A, T_B, T_START, T_LEFT, T_RIGHT, T_ROW };
enum { B_A = 0x8000, B_B = 0x4000, B_START = 0x1000 };
#define NT 64
#define PRESS_ON 6                      /* retraces held */
#define PRESS_OFF 6                     /* retraces released after */
#define ROW_STEPS 24                    /* at most this many moves to reach a row */

typedef struct { float x0, y0, x1, y1; int act, row; } Target;
typedef struct {
    Target t[NT];
    int n;
    int items;                          /* any target besides the prompts */
    int cur;                            /* a list's cursor (-1: no list) */
    int value;                          /* the selected row takes up and down as a value */
} Set;

extern int D_8028AAB0, D_8028AAB4;      /* the game's screen, in its own pixels (320 by 240) */

static host_mutex *s_m;
static Set s_build, s_ready, s_shown;
static float s_fbw = 320, s_fbh = 240, s_kx = 1, s_ky = 1;
static int s_race;
static int s_tag = -1, s_tagcur;        /* the next string is this row of a list */

/* what is being played to the pad */
static struct {
    unsigned short b;
    int x, y;
    int left;                           /* retraces of this press still to go (on, then off) */
    int seek, steps;                    /* moving the cursor to row `seek` (-1: not) */
} s_play = { 0, 0, 0, 0, -1, 0 };

static void lock(void)
{
    if (!s_m)
        s_m = host_mutex_new();
    host_mutex_lock(s_m);
}
static void unlock(void) { host_mutex_unlock(s_m); }

static void add(float x0, float y0, float x1, float y1, int act, int row)
{
    Target *t;
    if (s_build.n >= NT)
        return;
    t = &s_build.t[s_build.n++];
    t->x0 = x0; t->y0 = y0; t->x1 = x1; t->y1 = y1;
    t->act = act;
    t->row = row;
    if (act != T_A && act != T_B && act != T_START)
        s_build.items = 1;
}

/* ---- the game's side ------------------------------------------------------------ */
void tgr_touch_carousel(void)
{
    /* the ring's arrows sit either side of it, the item above its middle
       (menus/menudraw.c: a ring of radius 64 about x 160, y 90) */
    lock();
    add(36, 112, 112, 200, T_LEFT, 0);
    add(208, 112, 284, 200, T_RIGHT, 0);
    add(112, 36, 208, 200, T_A, 0);
    s_build.items = 1;
    unlock();
}

void tgr_touch_value_row(void) { s_build.value = 1; }

void tgr_touch_row(int row, int cur)
{
    s_tag = row;
    s_tagcur = cur;
}

static const struct { const char *s; int act; } k_prompts[] = {
    { "%wwSelect", T_A }, { "%wwContinue", T_A }, { "%wwYES", T_A }, { "%wwOK", T_A },
    { "%wwSELECT", T_A }, { "%wwGo Back", T_B }, { "%wwExit", T_B }, { "%wwNO", T_B },
    { "%wwCANCEL", T_B }, { "%wwEXIT", T_START },
};

static int prompt(const char *s)
{
    size_t i;
    for (i = 0; i < sizeof k_prompts / sizeof k_prompts[0]; i++)
        if (!strcmp(s, k_prompts[i].s))
            return k_prompts[i].act;
    return -1;
}

int tgr_touch_wants(const char *s) { return s_tag >= 0 || prompt(s) >= 0; }

/* the text is drawn above its y (y is the baseline), about 0.85 of the font's size tall */
void tgr_touch_text(const char *s, int x0, int y, int w, int size)
{
    float top = y - size * 0.85f;
    int act;
    lock();
    if (s_tag >= 0) {
        add(x0 - 8, top - 3, x0 + w + 8, y + 3, T_ROW, s_tag);
        s_build.cur = s_tagcur;
        s_tag = -1;
    } else if ((act = prompt(s)) >= 0) {
        add(x0 - 18, top - 5, x0 + w + 4, y + 5, act, 0);   /* with its button's picture, left of it */
    }
    unlock();
}

/* ---- the platform's side ---------------------------------------------------------- */
void tgr_touch_built(void)
{
    lock();
    if (getenv("TGR_TOUCHLOG") && s_build.n)
        fprintf(stderr, "touch: built %d targets\n", s_build.n);
    s_ready = s_build;
    memset(&s_build, 0, sizeof s_build);
    s_build.cur = -1;
    s_tag = -1;
    unlock();
}

void tgr_touch_shown(float fb_w, float fb_h, int race, float kx, float ky)
{
    lock();
    if (getenv("TGR_TOUCHLOG") && s_ready.n != s_shown.n)
        fprintf(stderr, "touch: shown %d targets (%gx%g)\n", s_ready.n, fb_w, fb_h);
    s_shown = s_ready;
    s_fbw = fb_w;
    s_fbh = fb_h;
    s_race = race;
    s_kx = race ? kx : 1;
    s_ky = race ? ky : 1;
    unlock();
}

static void press(unsigned short b, int x, int y)
{
    s_play.b = b;
    s_play.x = x;
    s_play.y = y;
    s_play.left = PRESS_ON + PRESS_OFF;
}

void tgr_touch_input(unsigned short *buttons, int *x, int *y)
{
    lock();
    if (s_play.left == 0 && s_play.seek >= 0) {
        /* the next move towards the row, from where the cursor is now */
        int cur = s_shown.cur;
        if (cur < 0 || s_play.steps++ >= ROW_STEPS)
            s_play.seek = -1;
        else if (cur == s_play.seek) {
            s_play.seek = -1;
            press(B_A, 0, 0);
        } else
            press(0, 0, cur < s_play.seek ? -80 : 80);
    }
    if (s_play.left > 0) {
        if (s_play.left > PRESS_OFF) {
            *buttons |= s_play.b;
            if (s_play.x || s_play.y) {
                *x = s_play.x;
                *y = s_play.y;
            }
        }
        s_play.left--;
    }
    unlock();
}

/* ---- the host's side ------------------------------------------------------------ */
/* one axis of a 2D element's place in a race frame stretched by k (gfx/rcp.c place()) */
static float placed(float v, float lo, float hi, float r, float k)
{
    if (hi - lo >= 0.9f * r)
        return v * k;
    if (hi <= 0.42f * r)
        return v;
    if (lo >= 0.58f * r)
        return v + r * (k - 1);
    return v + r * 0.5f * (k - 1);
}

void tgr_touch_tap(float u, float v)
{
    float fw, fh, s, ox, oy, px, py, best = 1e9f;
    int dw = 0, dh = 0, i, hit = -1;
    host_window_pixels(&dw, &dh);
    if (dw <= 0 || dh <= 0)
        return;
    lock();
    /* the frame as the renderer shows it: its shape, as large as the area holds, centred */
    fw = s_fbw * s_kx;
    fh = s_fbh * s_ky;
    s = fminf(dw / fw, dh / fh);
    ox = (dw - fw * s) * 0.5f;
    oy = (dh - fh * s) * 0.5f;
    /* the game's pixels: a high-res frame (640 by 480) is drawn from them doubled */
    px = (u * dw - ox) / s * D_8028AAB0 / s_fbw;
    py = (v * dh - oy) / s * D_8028AAB4 / s_fbh;
    for (i = 0; i < s_shown.n; i++) {
        const Target *t = &s_shown.t[i];
        float x0 = t->x0, x1 = t->x1, y0 = t->y0, y1 = t->y1, dx, dy, d;
        if (s_race) {
            x0 = placed(t->x0, t->x0, t->x1, (float)D_8028AAB0, s_kx);
            x1 = placed(t->x1, t->x0, t->x1, (float)D_8028AAB0, s_kx);
            y0 = placed(t->y0, t->y0, t->y1, (float)D_8028AAB4, s_ky);
            y1 = placed(t->y1, t->y0, t->y1, (float)D_8028AAB4, s_ky);
        }
        /* inside, or the nearest within a few pixels (a thumb is wider than a row) */
        dx = px < x0 ? x0 - px : px > x1 ? px - x1 : 0;
        dy = py < y0 ? y0 - py : py > y1 ? py - y1 : 0;
        d = dx * dx + dy * dy;
        if (d < best && d <= 8 * 8) {
            best = d;
            hit = i;
        }
    }
    if (getenv("TGR_TOUCHLOG")) {           /* TGR_TOUCHLOG=1: each tap and the targets it met */
        fprintf(stderr, "touch: tap %.3f,%.3f -> game %.1f,%.1f (frame %gx%g race %d, %d targets, items %d, cur %d): hit %d\n",
                u, v, px, py, s_fbw * s_kx, s_fbh * s_ky, s_race, s_shown.n, s_shown.items, s_shown.cur, hit);
        for (i = 0; i < s_shown.n; i++)
            fprintf(stderr, "touch:   %d act %d row %d box %.0f,%.0f..%.0f,%.0f\n", i, s_shown.t[i].act, s_shown.t[i].row,
                    s_shown.t[i].x0, s_shown.t[i].y0, s_shown.t[i].x1, s_shown.t[i].y1);
    }
    s_play.seek = -1;
    if (hit < 0) {
        if (!s_shown.items)
            press(B_A, 0, 0);
    } else {
        const Target *t = &s_shown.t[hit];
        switch (t->act) {
        case T_A: press(B_A, 0, 0); break;
        case T_B: press(B_B, 0, 0); break;
        case T_START: press(B_START, 0, 0); break;
        case T_LEFT: press(0, -80, 0); break;
        case T_RIGHT: press(0, 80, 0); break;
        case T_ROW:
            s_play.left = 0;
            s_play.seek = t->row;
            s_play.steps = 0;
            break;
        }
    }
    unlock();
}

/* the content follows the finger, as on iOS: a swipe left brings on what is
 * to the right (the stick right), a swipe up what is below (the stick down);
 * a value (a volume) goes up with the finger */
void tgr_touch_swipe(float dx, float dy)
{
    lock();
    s_play.seek = -1;
    if (fabsf(dx) >= fabsf(dy))
        press(0, dx < 0 ? 80 : -80, 0);
    else if (s_shown.value)
        press(0, 0, dy < 0 ? 80 : -80);
    else
        press(0, 0, dy < 0 ? -80 : 80);
    unlock();
}

void tgr_touch_press(unsigned short button)
{
    lock();
    s_play.seek = -1;
    press(button, 0, 0);
    unlock();
}
