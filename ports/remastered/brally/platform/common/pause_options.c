/* pause_options.c: the race's pause menu as Top Gear Rally Remastered has it
 * (pause_options.h): the game's menu with a Remaster Options row before the
 * quits, the page that row opens, taps on either, and the frame rate.
 *
 * The page is drawn as the menu is (BrFrameDraw: centred, the game's font,
 * its title at 40 and its rows at 20 a row apart from 5/11 down), one row an
 * option and its choices side by side:
 *
 *   label    the row's colour: the cursor's (%y1) on the row it is on, the
 *            menu's (%ry) on the others
 *   choices  the one in force in its row's colour, the other faded (the
 *            menu's colours at FADE of their brightness)
 *
 * Up and down move between the rows, left and right choose, at once: the
 * choice reaches both games through the app (options.c, rm_option_set).
 * Return on an option takes its other choice; on Back, or Escape anywhere,
 * the page closes onto the menu at its row.
 *
 * Built with the core's flags, for its types. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "br_addr32.h"
#include "br_coretypes.h"
#include "slice3_41.h"     /* BrDriverCar, g_aBrRaceCar */
#include "br_race.h"
#include "br_bits.h"       /* BrBitLatch */
#include "pause_options.h"

/* the app's (ports/remastered/platform/options.h) */
int  rm_option(int which);
void rm_option_set(int which, int value);
enum { RM_OPT_WIDESCREEN, RM_OPT_MUSIC, RM_OPT_FPS, RM_OPT_MENUS, RM_OPT_VIEWDIST };
enum { RM_MUSIC_PC, RM_MUSIC_N64, RM_MUSIC_N64_PAL };
enum { RM_VIEW_ORIGINAL, RM_VIEW_UNLIMITED };
uint32_t plat_time_ms(void);

#define ROW     0x14                /* the menu's row: its text size and its spacing */
#define TITLE   0x28                /* the menu's title size */
#define GAP     18                  /* game pixels between a label and its choices, and between choices */
#define FADE    0.6f

static const struct {
    const char *label;
    int opt;
    int n;                          /* its choices */
    const char *choice[3];
    int value[3];                   /* the option's value each choice sets */
} k_rows[] = {
    { "Widescreen", RM_OPT_WIDESCREEN, 2, { "On", "Off" }, { 1, 0 } },
    { "Music", RM_OPT_MUSIC, 3, { "Boss Rally", "TGR USA", "TGR PAL/Japan" },
      { RM_MUSIC_PC, RM_MUSIC_N64, RM_MUSIC_N64_PAL } },
    { "Show FPS", RM_OPT_FPS, 2, { "On", "Off" }, { 1, 0 } },
    { "View Distance", RM_OPT_VIEWDIST, 2, { "Original", "Unlimited" }, { RM_VIEW_ORIGINAL, RM_VIEW_UNLIMITED } },
};
#define NOPT ((int)(sizeof k_rows / sizeof k_rows[0]))
#define NROW (NOPT + 1)             /* and Back */

static int s_open, s_row;
static int s_presses;

void rm_pause_note(int presses) { s_presses |= presses; }
int  rm_pause_take(void) { int f = s_presses; s_presses = 0; return f; }

int rm_pause_active(void)
{
    if (s_open && g_BrX06909B4 != 1)
        s_open = 0;                 /* the menu closed under it (the race left, restarted) */
    return s_open;
}

void rm_pause_open(void)
{
    s_open = 1;
    s_row = 0;
}

static void close_page(void)
{
    s_open = 0;
    DAT_105bc8dc = RM_PAUSE_ROW;    /* back on the row that opened it */
}

static int chosen(int r)
{
    int c;
    for (c = 0; c < k_rows[r].n; c++)
        if (rm_option(k_rows[r].opt) == k_rows[r].value[c])
            return c;
    return 0;
}

static void choose(int r, int c)
{
    rm_option_set(k_rows[r].opt, k_rows[r].value[c]);
}

/* one frame's presses (g_BrX18ABAD0: 0x1000 up, 0x2000 down, 1 left, 2 right,
 * 0x8000 Return, 0x4000 Escape) */
void rm_pause_step(int f)
{
    if (f && getenv("RM_PAUSELOG"))      /* each frame's presses on the page (checks) */
        fprintf(stderr, "pause: presses %04x on row %d\n", f, s_row);
    if (f & 0xC010)
        BrBitLatchTake((BrBitLatch *)g_aBrRaceCar[0].pCtl, 0xc010);   /* taken, as the menu takes them */
    if (f & 0x4000) {
        DAT_10226a50 = 0;
        close_page();
        return;
    }
    if (f & 0x1000)
        s_row = (s_row + NROW - 1) % NROW;
    if (f & 0x2000)
        s_row = (s_row + 1) % NROW;
    if (s_row < NOPT) {
        int n = k_rows[s_row].n;
        if (f & 1)                  /* left and right step through the choices */
            choose(s_row, (chosen(s_row) + n - 1) % n);
        if (f & 2)
            choose(s_row, (chosen(s_row) + 1) % n);
        if (f & 0x8000)             /* Return: the next */
            choose(s_row, (chosen(s_row) + 1) % n);
    } else if (f & 0x8000) {
        close_page();
    }
}

/* ---- drawing ---------------------------------------------------------------------- */
static int width(const char *s, int size) { return BrFontMeasure(s, size); }

/* text at x, y, left-aligned, in a faded copy of the menu's colours */
static void draw_faded(const char *s, int x, int y)
{
    int sv[7];
    sv[0] = DAT_100a6c68; sv[1] = DAT_100a6c6c; sv[2] = DAT_100a6c70; sv[3] = DAT_104abb4c;
    sv[4] = DAT_104abb50; sv[5] = DAT_104abb54; sv[6] = DAT_104abb58;
    /* %ry: 0xBE0000 at the top, 0xD2BE00 at the bottom (br_font.c) */
    BrTextSetColors((int)(0xD2 * FADE), (int)(0xBE * FADE), 0, (int)(0xBE * FADE), 0, 0);
    BrTextDraw(s, x, y);
    DAT_100a6c68 = sv[0]; DAT_100a6c6c = sv[1]; DAT_100a6c70 = sv[2]; DAT_104abb4c = sv[3];
    DAT_104abb50 = sv[4]; DAT_104abb54 = sv[5]; DAT_104abb58 = sv[6];
}

/* an option's row: its whole width (game pixels) and, drawing, its pieces */
static int row_width(int r)
{
    int w = width(k_rows[r].label, ROW), c;
    for (c = 0; c < k_rows[r].n; c++)
        w += GAP + width(k_rows[r].choice[c], ROW);
    return w;
}

static int title_size(void)
{
    int size = TITLE;
    while (size > ROW && width("Remaster Options", size) > 600)
        size -= 2;
    return size;
}

void rm_pause_draw(int x, int h)
{
    char buf[96];
    int r, y = h * 5 / 11, c;
    static int logged;
    if (!logged && getenv("RM_PAUSELOG")) {      /* the page's pieces, game pixels (checks) */
        int yy = y;
        logged = 1;
        for (r = 0; r < NOPT; r++, yy += ROW) {
            int px = x - row_width(r) / 2;
            fprintf(stderr, "pause: row %d y %d label %d+%d", r, yy, px, width(k_rows[r].label, ROW));
            px += width(k_rows[r].label, ROW) + GAP;
            for (c = 0; c < k_rows[r].n; c++) {
                fprintf(stderr, " %s %d+%d", k_rows[r].choice[c], px, width(k_rows[r].choice[c], ROW));
                px += width(k_rows[r].choice[c], ROW) + GAP;
            }
            fprintf(stderr, "\n");
        }
        fprintf(stderr, "pause: back y %d %d+%d\n", yy, x - width("Back", ROW) / 2, width("Back", ROW));
    }
    BrTextFlag358Clear();
    BrSet_10019270();
    BrSetGlobal_ABB30(title_size());
    BrTextDraw("%ryRemaster Options", x, h * 5 / 16);
    BrSetGlobal_ABB30(ROW);
    for (r = 0; r < NOPT; r++, y += ROW) {
        const char *col = r == s_row ? "%y1" : "%ry";
        int px = x - row_width(r) / 2, pick = chosen(r);
        BrSub_10019280();           /* left-aligned, piece by piece */
        snprintf(buf, sizeof buf, "%s%s", col, k_rows[r].label);
        BrTextDraw(buf, px, y);
        px += width(k_rows[r].label, ROW) + GAP;
        for (c = 0; c < k_rows[r].n; c++) {
            if (c == pick) {
                snprintf(buf, sizeof buf, "%s%s", col, k_rows[r].choice[c]);
                BrTextDraw(buf, px, y);
            } else {
                draw_faded(k_rows[r].choice[c], px, y);
            }
            px += width(k_rows[r].choice[c], ROW) + GAP;
        }
    }
    BrSet_10019270();
    BrTextDraw(s_row == NOPT ? "%y1Back" : "%ryBack", x, y);
}

/* ---- taps --------------------------------------------------------------------------- */
/* a tap at nx, ny into game pixels: the menu's rows are one piece of 2D to
 * the screen map (rows 20 apart touch), placed by the box of them all */
static void tap_at(float nx, float ny, int x, int y0, int rows, float *gx, float *gy)
{
    float b[4];
    b[0] = (float)(x - 150);
    b[1] = (float)(y0 - 15);
    b[2] = (float)(x + 150);
    b[3] = (float)(y0 + (rows - 1) * ROW + 5);
    plat_tap_to_game(nx, ny, b, gx, gy);
}

/* inside row r's cell: the text's, 15 above its baseline to 5 below (br_font.c) */
static int on_row(float gx, float gy, int x, int y, int w)
{
    return gx >= x - w / 2 - 6 && gx <= x + w / 2 + 6 && gy >= y - 16 && gy < y + 5;
}

/* a tap on the paused race at nx, ny (0..1 across the window): 1 when it met a
 * row; *press set when the menu's row should then be chosen (Return) */
int rm_pause_tap(float nx, float ny, int x, int h, int *press)
{
    float gx, gy;
    int r, y0 = h * 5 / 11;
    *press = 0;
    if (g_BrX06909B4 != 1)
        return 0;
    if (!s_open) {
        /* the menu's rows: Continue, Restart, the two volumes, ours, the quits */
        static const char *const k_menu[7] = {
            "Continue", "Restart Race", "Music Volume: 9", "SFX Volume: 9", "Remaster Options",
            "Quit Championship", "Quit Game" };
        tap_at(nx, ny, x, y0, RM_PAUSE_ROWS, &gx, &gy);
        for (r = 0; r < RM_PAUSE_ROWS; r++)
            if (on_row(gx, gy, x, y0 + r * ROW, width(k_menu[r], ROW))) {
                if (r == 1 && g_brRaceNet)
                    return 1;
                DAT_105bc8dc = r;
                *press = r != 2 && r != 3;      /* a volume: the cursor only (left and right set it) */
                return 1;
            }
        return 0;
    }
    tap_at(nx, ny, x, y0, NROW, &gx, &gy);
    for (r = 0; r < NROW; r++) {
        int y = y0 + r * ROW, w = r < NOPT ? row_width(r) : width("Back", ROW);
        if (!on_row(gx, gy, x, y, w))
            continue;
        s_row = r;
        if (r == NOPT) {
            close_page();
        } else {
            /* the label moves the cursor; a choice is chosen */
            float c0 = (float)(x - w / 2 + width(k_rows[r].label, ROW) + GAP / 2);
            int c;
            for (c = 0; c < k_rows[r].n && gx >= c0; c++) {
                float c1 = c0 + GAP / 2 + width(k_rows[r].choice[c], ROW) + GAP / 2;
                if (gx < c1 || c == k_rows[r].n - 1) {
                    choose(r, c);
                    break;
                }
                c0 = c1;
            }
        }
        return 1;
    }
    return 0;
}

/* ---- the frame rate ----------------------------------------------------------------- */
static int s_fps = -1;
static uint32_t s_t0;
static int s_n;

/* each race frame drawn (BrFrameDraw), and the rate shown when it is asked for */
void rm_fps_frame(int x, int h)
{
    uint32_t now = plat_time_ms();
    char buf[32];
    if (!s_t0)
        s_t0 = now;
    s_n++;
    if (now - s_t0 >= 1000) {
        s_fps = (int)((s_n * 1000u + (now - s_t0) / 2) / (now - s_t0));
        s_t0 = now;
        s_n = 0;
    }
    if (!rm_option(RM_OPT_FPS) || s_fps < 0)
        return;
    snprintf(buf, sizeof buf, "%%ry%d FPS", s_fps);
    BrTextFlag358Clear();
    BrSet_10019270();
    BrSetGlobal_ABB30(ROW);
    BrTextDraw(buf, x, h - 8);
}
