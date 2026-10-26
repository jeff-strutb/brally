/* rmoptions.c (port): the Remaster Options on Top Gear Rally's own options
 * screen, rows of the carousel beside the volumes and the units, as Top Gear
 * Rally Remastered has it.
 *
 * After Units come Widescreen, Music, Show FPS and View Distance (the options
 * the race's pause menu offers too: ports/remastered/platform/options.c) and
 * Menus, then the
 * game's own controller, save, load and credits rows.  Each is a row like
 * Units: up or down (a swipe on the iPhone: tgr_touch_value_row) or A takes
 * its other choice, at once, both games told.  Menus takes Boss Rally's
 * menus: the game waits while they are in front (src/racing/handoff.c,
 * tgr_menus_away) and comes back here when the player chooses Top Gear
 * Rally's on Boss Rally's Remaster Options page.  Their icons, turning on the
 * ring as the others do:
 *
 *   Widescreen  the PC's monitor, 4:3 when off and drawn 16:9 when on
 *   Music       the PC's monitor for the PC's soundtrack, the console for the N64's
 *   Show FPS    Time Attack's stopwatch (the main menu's, from the cartridge)
 *   View Distance  the Coastline course's own (the course menu's, from the
 *               cartridge): its lighthouse is what Original lets pop in
 *   Menus       the console: these are the N64 game's (Boss Rally's, the PC
 *               game's, are the monitor's on the Music row's terms)
 *
 * The monitor and the console are built by hand
 * (ports/remastered/tools/handicons.py: platform/os/rm_icons.c, the game's own
 * icon format).  They are placed once in the arena's upper half, which the
 * game never uses (it runs in the N64's 4 MB, no Expansion Pak): copied, the
 * wide monitor's vertices stretched across, and rebased as BrModelLoad rebases
 * a model from the cartridge.
 *
 * options.c (port) asks for the screen's items (rm_options_items), passes its
 * rows through rm_options_row (ours handled here, the game's given back the
 * numbers its switch knows) and shows the help line on ours too. */
#include "tgr/common.h"
#include "tgr/menu.h"
#include "tgr/model.h"

/* -- declarations -- */
void BrModelRebase(BrModel *m);
unsigned int BrRomUnpack(unsigned char *dst, unsigned int rom, void *s);
int  BrRomReadSize(int rom);
extern MenuItem *D_80272360[];          /* the main menu's rows: [2] is Time Attack */
extern MenuItem *D_80271D1C[];          /* the course menu's rows (seasonmenu.c): [2] is Coastline */
int  rm_option(int which);              /* the app's (ports/remastered/platform/options.h) */
void rm_option_set(int which, int value);
extern const unsigned char rm_icon_crt[], rm_icon_n64[];
extern const unsigned int rm_icon_crt_len, rm_icon_n64_len;
/* -- end declarations -- */

enum { RM_OPT_WIDESCREEN, RM_OPT_MUSIC, RM_OPT_LIGHTING, RM_OPT_FPS, RM_OPT_MENUS, RM_OPT_VIEWDIST };
enum { RM_MUSIC_PC, RM_MUSIC_N64 };
enum { RM_MENUS_TGR, RM_MENUS_BR };
enum { RM_VIEW_ORIGINAL, RM_VIEW_UNLIMITED };
void tgr_menus_away(void);              /* src/racing/handoff.c */

#define RM_FIRST 3                      /* our rows: after BGM, SFX and Units */
#define RM_ROWS 5
#define RM_ICONS 0x80600000u            /* the arena's upper half (no Expansion Pak in the game) */

static const int k_opt[RM_ROWS] = { RM_OPT_WIDESCREEN, RM_OPT_MUSIC, RM_OPT_FPS, RM_OPT_VIEWDIST, RM_OPT_MENUS };
static MenuItem s_row[RM_ROWS];
static char s_label[RM_ROWS][32];
static MenuItem *s_items[16];
static unsigned int s_crt, s_wide, s_n64;   /* the icons, placed */
static unsigned int s_course, s_coursecut; /* View Distance's: Unlimited's, Original's */

/* a model copied to `at` (stretched across by kx: its vertices' x) and rebased;
 * answers where the next may go */
static unsigned int place(unsigned int at, const unsigned char *src, unsigned int n, float kx)
{
  BrModel *m = TGR_PTR(BrModel *, at);
  unsigned int i, k;

  memcpy(m, src, n);
  if (kx != 1.0f) {
    for (i = 0; i < BE32(m->nDl); i++) {
      unsigned char *g = (unsigned char *)m + BE32(m->dls[i].dl);
      for (;; g += 8) {
        unsigned int w0 = tgr_be32(*(unsigned int *)g);
        if (w0 >> 24 == 0xB8) {
          break;
        }
        if (w0 >> 24 == 0x04) {
          unsigned char *v = (unsigned char *)m + tgr_be32(*(unsigned int *)(g + 4));
          for (k = 0; k < ((w0 >> 10) & 0x3F); k++, v += 16) {
            short x = (short)(v[0] << 8 | v[1]);
            x = (short)(x * kx);
            v[0] = (unsigned char)((unsigned short)x >> 8);
            v[1] = (unsigned char)x;
          }
        }
      }
    }
  }
  BrModelRebase(m);
  return (at + n + 15) & ~15u;
}

/* the Coastline course's icon unpacked to `at`; with `cut`, as Original's
 * view sees it: every triangle reaching past RM_CUT_X gone, the far end of
 * the course with the lighthouse (the model's -x end: the course runs from
 * x -1674 to 1608, the lighthouse stands at -1437..-1304, the windmill at the
 * other end) not drawn.  Rebased;
 * answers where the next may go */
#define RM_CUT_X (-360)
static unsigned int course(unsigned int at, int cut)
{
  unsigned int rom = D_80271D1C[2]->iconRomStart;
  BrModel *m = TGR_PTR(BrModel *, at);
  unsigned int n = (unsigned int)BrRomReadSize((int)rom);
  unsigned int i, k;

  BrRomUnpack((unsigned char *)m, rom, 0);
  for (i = 0; cut && i < BE32(m->nDl); i++) {
    float px = BEF(m->dls[i].pos[0]), x[64] = { 0 };
    unsigned char *g;
    if (BE32(m->dls[i].dl) == 0) {
      continue;
    }
    for (g = (unsigned char *)m + BE32(m->dls[i].dl);; g += 8) {
      unsigned int w0 = tgr_be32(*(unsigned int *)g), w1 = tgr_be32(*(unsigned int *)(g + 4));
      unsigned int op = w0 >> 24;
      if (op == 0xB8) {
        break;
      }
      if (op == 0x04) {                 /* G_VTX: each vertex's x, by its slot */
        unsigned char *v = (unsigned char *)m + w1;
        unsigned int v0 = ((w0 >> 16) & 0xFF) / 2;
        for (k = 0; k < ((w0 >> 10) & 0x3F) && v0 + k < 64; k++, v += 16) {
          x[v0 + k] = px + (short)(v[0] << 8 | v[1]);
        }
      } else if (op == 0xB1 || op == 0xBF) {
        /* G_TRI2 holds a triangle in each word, G_TRI1 one in w1: each kept
           only when its three corners are this side of the cut */
        int keep0 = op == 0xB1 && x[((w0 >> 16) & 0xFF) / 2] >= RM_CUT_X && x[((w0 >> 8) & 0xFF) / 2] >= RM_CUT_X &&
                    x[(w0 & 0xFF) / 2] >= RM_CUT_X;
        int keep1 = x[((w1 >> 16) & 0xFF) / 2] >= RM_CUT_X && x[((w1 >> 8) & 0xFF) / 2] >= RM_CUT_X &&
                    x[(w1 & 0xFF) / 2] >= RM_CUT_X;
        if (keep0 && !keep1) {
          w1 = w0 & 0xFFFFFF;
        }
        if (keep0 && keep1) {
          continue;
        }
        w0 = keep0 || keep1 ? 0xBF000000u : 0;   /* the one left as G_TRI1, or G_SPNOOP */
        if (w0 == 0) {
          w1 = 0;
        }
        *(unsigned int *)g = tgr_be32(w0);
        *(unsigned int *)(g + 4) = tgr_be32(w1);
      }
    }
  }
  BrModelRebase(m);
  return (at + n + 15) & ~15u;
}

static void icons(void)
{
  unsigned int at = RM_ICONS;

  if (s_crt != 0) {
    return;
  }
  s_crt = at;
  at = place(at, rm_icon_crt, rm_icon_crt_len, 1.0f);
  s_wide = at;
  at = place(at, rm_icon_crt, rm_icon_crt_len, 16.0f / 12.0f);
  s_n64 = at;
  at = place(at, rm_icon_n64, rm_icon_n64_len, 1.0f);
  s_course = at;
  at = course(at, 0);
  s_coursecut = at;
  at = course(at, 1);
}

/* the console's icon, placed (the main menu's Quit: rmmainmenu.c) */
unsigned int rm_options_icon_console(void)
{
  icons();
  return s_n64;
}

/* the screen's rows: the game's three, ours, the game's others (NULL-terminated) */
MenuItem **rm_options_items(MenuItem **game)
{
  int i, n = 0, k;

  icons();
  for (i = 0; game[i] != 0 && i < RM_FIRST; i++) {
    s_items[n++] = game[i];
  }
  for (k = 0; k < RM_ROWS; k++) {
    s_row[k].flags = 0x03;              /* as Units: up, down and A all take the other choice */
    s_items[n++] = &s_row[k];
  }
  for (; game[i] != 0 && n < 15; i++) {
    s_items[n++] = game[i];
  }
  s_items[n] = 0;
  /* the stopwatch: the cartridge's, loaded with the screen as the game's own icons are */
  s_row[2].iconRomStart = D_80272360[2]->iconRomStart;
  s_row[2].iconRomEnd = D_80272360[2]->iconRomEnd;
  return s_items;
}

/* each frame: our rows' labels and icons, from the options in force */
void rm_options_frame(void)
{
  int wide = rm_option(RM_OPT_WIDESCREEN) != 0;
  int pc = rm_option(RM_OPT_MUSIC) == RM_MUSIC_PC;

  sprintf(s_label[0], "Widescreen: %s", wide ? "On" : "Off");
  sprintf(s_label[1], "Music: %s", pc ? "PC" : "N64");
  sprintf(s_label[2], "Show FPS: %s", rm_option(RM_OPT_FPS) ? "On" : "Off");
  s_row[0].label = tgr_addr32(s_label[0]);
  s_row[1].label = tgr_addr32(s_label[1]);
  sprintf(s_label[3], "View Distance: %s", rm_option(RM_OPT_VIEWDIST) == RM_VIEW_UNLIMITED ? "Unlimited" : "Original");
  sprintf(s_label[4], "Menus: Top Gear Rally");
  s_row[2].label = tgr_addr32(s_label[2]);
  s_row[3].label = tgr_addr32(s_label[3]);
  s_row[4].label = tgr_addr32(s_label[4]);
  s_row[0].icon = wide ? s_wide : s_crt;
  s_row[1].icon = pc ? s_crt : s_n64;
  s_row[3].icon = rm_option(RM_OPT_VIEWDIST) == RM_VIEW_UNLIMITED ? s_course : s_coursecut;
  s_row[4].icon = s_n64;
}

/* 1 for one of our rows */
int rm_options_ours(int row)
{
  return row >= RM_FIRST && row < RM_FIRST + RM_ROWS;
}

/* the menu's answer on `row`: ours taken here (-1 back: nothing for the
 * game's switch); else the row's number in the game's own screen.  B (2: the
 * menu backed out) is the screen's whatever the row, and goes to its switch */
int rm_options_row(int result, int row)
{
  if (rm_options_ours(row) && result != 2) {
    if (result == 1 || result == 3 || result == 4) {
      int o = k_opt[row - RM_FIRST];
      if (o == RM_OPT_MENUS) {
        rm_option_set(o, RM_MENUS_BR);
        tgr_menus_away();               /* Boss Rally's menus, until the player is back */
      } else if (o == RM_OPT_MUSIC) {
        rm_option_set(o, rm_option(o) == RM_MUSIC_PC ? RM_MUSIC_N64 : RM_MUSIC_PC);
      } else {
        rm_option_set(o, !rm_option(o));
      }
      rm_options_frame();
    }
    return -1;
  }
  return row < RM_FIRST ? row : rm_options_ours(row) ? 0 : row - RM_ROWS;
}
