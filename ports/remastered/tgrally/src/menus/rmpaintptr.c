/* rmpaintptr.c -- Top Gear Rally Remastered: the paint shop by mouse and touch
 *
 * The pointer (tgr_touch.h: the mouse, or a finger on the iPhone) works the
 * paint shop beside the pad.  The cursor goes where the pointer is (the stick
 * still moves it), and the left button or a finger is A, the right button B,
 * given to the game in its pad record at the start of the screen's frame with
 * the cursor already placed, so a click acts where it lands.  A press the
 * game has acted on (consumed) is not given again until the button comes up.
 *
 *   the brush   held, it paints; a stroke faster than a texel a frame is
 *               painted along the way (BrPaintClick at each texel between)
 *   line, rectangle, oval
 *               two clicks as before, or a drag: the button coming up a few
 *               pixels from where it went down is the second click
 *   B           the right button: on the decal the colour under the cursor,
 *               elsewhere it cancels (as the pad's B)
 *   popups      a click on a choice chooses it, a click on the chosen one is
 *               A; the A and B button pictures (and their words) are those
 *               buttons (rm_paint_drawn, from the pictures drawn); the right
 *               button is B; the colour mixer's bars are dragged; a click on
 *               a Controller Pak message with no buttons is A, as on the pad
 *
 * The screen calls rm_paint_pointer before it reads the cursor and
 * rm_paint_pointer_after at its end (paintscreen.c). */
#include "tgr/common.h"
#include "tgr/image.h"
#include "tgr/pad.h"
#include "tgr_touch.h"

typedef struct { int x, y, w, h; } RmRect;
typedef struct { int x, y, w, h; unsigned char c[3]; } RmSwatch;
extern BrImage D_8028D110;              /* the cursor: its x, y */
extern RmRect D_8028DB94;               /* the paint area on screen */
extern RmSwatch D_80369B98[16];         /* the palette */
extern BrImage D_8028D0B0;              /* the A button */
extern BrImage D_8028D0E0;              /* the B button */
extern unsigned char D_8028DBBC;        /* the controller */
extern unsigned char D_8028DB58;        /* the chosen palette colour */
extern unsigned char D_8028DB60;        /* the tool (7-10, 12: a popup of its own) */
extern unsigned char D_8028DBC0;        /* a stroke is under way */
extern unsigned char D_8028DBC4;        /* the keyboard is up */
extern unsigned char D_8028DBD4;        /* the colour mixer is open */
extern unsigned char D_8028DBE0;        /* a popup is open */
extern unsigned char D_8028DBE8;        /* leaving */
extern unsigned char D_8028DCE0;        /* the mixer's channel */
void BrPaintClick(void);
unsigned char BrPaintGet(int x, int y);
extern unsigned char D_8028DB88;        /* the decal's width and height, texels */
extern unsigned char D_8028DB8C;
extern unsigned char D_8028DB68;        /* the decal being painted */
extern unsigned char D_8028DBB8;        /* a popup's choice */
extern RmRect D_80369CD8[13];           /* the tool buttons */
extern unsigned char D_8028CF5C;        /* the rectangle style */
unsigned int tgr_frame(void);   /* the retraces so far (platform/os/thread.c) */

enum { P_A = 0x10, P_B = 0x20 };        /* the game's bits for A and B (BrPadMapRead) */
#define FRESH 8                         /* retraces between frames that are one visit */
#define SLOP 4                          /* pixels around a choice or a bar */

static TgrPointer s_p;                  /* this frame's pointer */
static unsigned int s_at;                   /* the retrace of the last frame */
static int s_given;                     /* the bits given this frame */
static int s_spent;                     /* given bits the game took while the button stays down */
static int s_pending;                   /* a drag's end: A to give this frame */
static int s_bar = -1;                  /* the mixer's bar being dragged */
static struct { int bit, x, y, h; } s_btn[8];   /* the A and B pictures drawn since the last frame */
static int s_nbtn;

static BrPadRec *pad(void) { return &D_8036A8E0[D_8028DBBC]; }
static int iabs(int v) { return v < 0 ? -v : v; }

static int inside(int x, int y, int x0, int y0, int w, int h, int slop)
{
    return x >= x0 - slop && x <= x0 + w + slop && y >= y0 - slop && y <= y0 + h + slop;
}

static int on_decal(int x, int y)
{
    return inside(x, y, D_8028DB94.x, D_8028DB94.y, D_8028DB94.w, D_8028DB94.h, 0);
}

/* the cursor to (x, y), held where the stick holds it (BrPaintStickMove); a
 * brush stroke is painted at each texel on the way */
static void place(int x, int y)
{
    int ox = D_8028D110.x, oy = D_8028D110.y, dx, dy, n, i;
    x = x < 31 ? 31 : x > 609 ? 609 : x;
    y = y < 22 ? 22 : y > 458 ? 458 : y;
    if (D_8028DB60 == 1 && D_8028DBC0 && !D_8028DBC4 && s_p.down & 1 && !(s_spent & P_A) &&
        on_decal(ox, oy) && on_decal(x, y)) {
        dx = x - ox;
        dy = y - oy;
        n = (iabs(dx) > iabs(dy) ? iabs(dx) : iabs(dy)) / 4;     /* a texel is 4 pixels */
        pad()->pressed |= P_A;
        for (i = 1; i < n; i++) {
            D_8028D110.x = ox + dx * i / n;
            D_8028D110.y = oy + dy * i / n;
            BrPaintClick();
        }
    }
    D_8028D110.x = x;
    D_8028D110.y = y;
}

void rm_paint_pointer(void)
{
    unsigned int now = tgr_frame();
    int fresh = s_at && now - s_at <= FRESH, bits = 0, i, any = 0;
    int popup, pending = s_pending;

    tgr_pointer_frame(&s_p);
    s_at = now;
    s_given = 0;
    s_pending = 0;
    if (!fresh) {                       /* a new visit: nothing from before it acts */
        s_p.clicks = s_p.dragged = 0;
        s_spent = (s_p.down & 1 ? P_A : 0) | (s_p.down & 2 ? P_B : 0);
        s_nbtn = 0;
        s_bar = -1;
        pending = 0;
    }
    if (!(s_p.down & 1))
        s_spent &= ~P_A;
    if (!(s_p.down & 2))
        s_spent &= ~P_B;
    if (D_8028DBE8) {                   /* leaving */
        s_nbtn = 0;
        return;
    }
    /* a popup is drawn instead of the cursor (BrPaintShopScreen) */
    popup = !on_decal(D_8028D110.x, D_8028D110.y) &&
            (D_8028DBE0 || D_8028DBD4 || (D_8028DB60 >= 7 && D_8028DB60 <= 10) || D_8028DB60 == 12);
    if (!popup) {
        if (s_p.moved)
            place(s_p.x, s_p.y);
        if ((s_p.down | s_p.clicks) & 1 || pending)
            bits |= P_A;
        if ((s_p.down | s_p.clicks) & 2)
            bits |= P_B;
        if (s_p.dragged && (D_8028DB60 == 3 || D_8028DB60 == 5 || D_8028DB60 == 6) && D_8028DBC0 && !D_8028DBC4)
            s_pending = 1;              /* the drag's end is the second click, next frame */
    } else {
        for (i = 0; i < s_nbtn; i++) {
            /* a picture and its word to its right: up to the next picture, or 150 */
            int j, right = s_btn[i].x + 150;
            for (j = 0; j < s_nbtn; j++)
                if (s_btn[j].x > s_btn[i].x && s_btn[j].x - 8 < right)
                    right = s_btn[j].x - 8;
            if (s_p.clicks & 1 && inside(s_p.x, s_p.y, s_btn[i].x, s_btn[i].y, right - s_btn[i].x, s_btn[i].h, 6)) {
                bits |= s_btn[i].bit;
                s_p.clicks &= ~1;
            }
            any = 1;
        }
        if (s_p.clicks & 2)
            bits |= P_B;
        if (s_p.clicks & 1 && !any && (D_8028DB60 == 9 || D_8028DB60 == 10))
            bits |= P_A;                /* a Controller Pak message: any button goes on */
    }
    s_nbtn = 0;
    bits &= ~s_spent;
    pad()->pressed |= bits;
    s_given = bits;
}

/* the screen's frame is done: what the game took of what it was given.
 * Checks: RM_PAINTLOG=1 logs each frame; RM_PAINTDUMP=F prints the decal's
 * texels (colour indices, a row a line, its top row first) at retrace F */
void rm_paint_pointer_after(void)
{
    static int dumped;
    int dump;
    s_spent |= s_given & pad()->held;
    if (tgr_check_env("RM_PAINTLOG", 0))
        tgr_check_print("paint: f %u cur %d,%d ptr %d,%d down %d given %x spent %x tool %d stroke %d kbd %d popup %d mix %d choice %d colour %d decal %d area %d,%d %dx%d\n",
                tgr_frame(), D_8028D110.x, D_8028D110.y, s_p.x, s_p.y, s_p.down, s_given, s_spent, D_8028DB60,
                D_8028DBC0, D_8028DBC4, D_8028DBE0, D_8028DBD4, D_8028DBB8, D_8028DB58, D_8028DB68,
                D_8028DB94.x, D_8028DB94.y, D_8028DB94.w, D_8028DB94.h);
    if (!dumped && (dump = tgr_check_env("RM_PAINTDUMP", 0)) > 0 && tgr_frame() >= (unsigned int)dump) {
        int x, y;
        char row[257];
        dumped = 1;
        tgr_check_print("paintdump: %d %d rectstyle %d\n", D_8028DB88, D_8028DB8C, D_8028CF5C);
        for (x = 0; x < 13; x++)
            tgr_check_print("painttool: %d %d %d %d %d\n", x, D_80369CD8[x].x, D_80369CD8[x].y, D_80369CD8[x].w, D_80369CD8[x].h);
        for (x = 0; x < 16; x++)
            tgr_check_print("paintswatch: %d %d %d %d %d rgb %d %d %d\n", x, D_80369B98[x].x, D_80369B98[x].y, D_80369B98[x].w,
                            D_80369B98[x].h, D_80369B98[x].c[0], D_80369B98[x].c[1], D_80369B98[x].c[2]);
        for (y = D_8028DB8C - 1; y >= 0; y--) {
            for (x = 0; x < D_8028DB88 && x < 256; x++)
                row[x] = "0123456789abcdef"[BrPaintGet(x, y) & 0xf];
            row[x] = 0;
            tgr_check_print("paintrow:%s\n", row);
        }
    }
}

/* BrImageDrawAt: the A and B pictures of a popup's buttons are clicked as those buttons */
void rm_paint_drawn(BrImage *img, int x, int y)
{
    if ((img != &D_8028D0B0 && img != &D_8028D0E0) || tgr_frame() - s_at > FRESH || s_nbtn >= 8)
        return;
    s_btn[s_nbtn].bit = img == &D_8028D0B0 ? P_A : P_B;
    s_btn[s_nbtn].x = x;
    s_btn[s_nbtn].y = y;
    s_btn[s_nbtn].h = img->drawH > 28 ? img->drawH : 28;
    s_nbtn++;
    if (tgr_check_env("RM_PAINTLOG", 0))
        tgr_check_print("paintbtn: %c %d %d %d\n", img == &D_8028D0B0 ? 'A' : 'B', x, y, img->drawH);
}

/* a popup's choice i at (x, y, w, h): a click there chooses it, and on the
 * one already chosen is A */
void rm_paint_choose(int x, int y, int w, int h, unsigned char *sel, int i)
{
    if (!(s_p.clicks & 1) || !inside(s_p.x, s_p.y, x, y, w, h, SLOP))
        return;
    s_p.clicks &= ~1;
    if (*sel == i) {
        pad()->pressed |= P_A;
        s_given |= P_A;
    } else {
        *sel = i;
    }
}

/* the colour mixer's three bars (0x80 wide from 0x11D, 0x10 high every 0x16
 * from 0xC2): pressed on one, it is the channel, and dragged along it, its
 * value (the bar shows half the value, the shadow colour its value) */
void rm_paint_bar(int *step)
{
    int i, off, v;
    if (s_p.clicks & 1) {
        for (i = 0; i < 3; i++)
            if (inside(s_p.x, s_p.y, 0x11d, 0xc2 + i * 0x16, 0x80, 0x10, SLOP))
                s_bar = i;
    }
    if (s_bar < 0)
        return;
    D_8028DCE0 = s_bar;
    off = s_p.x - 0x11d;
    off = off < 0 ? 0 : off > 0x80 ? 0x80 : off;
    if (D_8028DB58 == 1)
        v = off > 0x7f ? 0x7f : off;
    else
        v = off * 2 > 0xff ? 0xff : off * 2;
    *step = v - D_80369B98[D_8028DB58].c[s_bar];
    if (!(s_p.down & 1))
        s_bar = -1;
}
