/* rmoptions.c (port): the Remaster Options on Top Gear Rally's own options
 * screen, rows of the carousel beside the volumes and the units, as Top Gear
 * Rally Remastered has it.
 *
 * After Units come Widescreen, Music, Show FPS and View Distance (the options
 * the race's pause menu offers too: ports/remastered/platform/options.c) and
 * Menus, then the
 * game's own controller, save, load and credits rows.  Each is a row like
 * Units: up or down (a swipe on the iPhone: tgr_touch_value_row) or A takes
 * its other choice, at once, both games told.  Menus is chosen the same way
 * but taken only when the player leaves the screen (B): the menus chosen,
 * Boss Rally's or the other cartridge's, come in front then, the game waiting
 * while they are (src/racing/handoff.c, tgr_menus_away, tgr_menus_other), and
 * the player comes back to this game's main menu.  Their icons, turning on
 * the ring as the others do:
 *
 *   Widescreen  the PC's monitor, 4:3 when off and drawn 16:9 when on
 *   Music       the box of the game whose soundtrack it is: Boss Rally's, Top
 *               Gear Rally's USA box, or its Japanese (the PAL/Japan cartridge's)
 *   Show FPS    Time Attack's stopwatch (the main menu's, from the cartridge)
 *   View Distance  the Coastline course's own (the course menu's, from the
 *               cartridge): its lighthouse is what Original lets pop in
 *   Menus       the box of the game whose menus they are, as Music's
 *
 * A box chosen on Music or Menus turns to the next: a whole turn in half a
 * second over the ring's own spin, the box taken when it is edge on, as the
 * game turns an icon to a new one (BrMenu, an item's flag 4), up turning it
 * one way and down the other (rm_options_turn, frontbuttons.c (port)).
 *
 * The monitor and the console are built by hand
 * (ports/remastered/tools/handicons.py: platform/os/rm_icons.c, the game's own
 * icon format), the boxes from their pictures (tools/boxicons.py:
 * platform/os/rm_boxes.c, the same format, their faces textured).  They are
 * placed once in the arena's upper half, which the
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
float floorf(float x);
unsigned int BrRomUnpack(unsigned char *dst, unsigned int rom, void *s);
int  BrRomReadSize(int rom);
extern MenuItem *D_80272360[];          /* the main menu's rows: [2] is Time Attack */
extern MenuItem *D_80271D1C[];          /* the course menu's rows (seasonmenu.c): [2] is Coastline */
int  rm_option(int which);              /* the app's (ports/remastered/platform/options.h) */
void rm_option_set(int which, int value);
extern const unsigned char rm_icon_crt[], rm_icon_n64[];
extern const unsigned int rm_icon_crt_len, rm_icon_n64_len;
extern const unsigned char rm_box_usa[], rm_box_jpn[], rm_box_br[];
extern const unsigned int rm_box_usa_len, rm_box_jpn_len, rm_box_br_len;
extern float D_80316228;                /* the ring's icon spin (BrMenu) */
extern float D_8028AAD8;                /* seconds this frame */
/* -- end declarations -- */

enum { RM_OPT_WIDESCREEN, RM_OPT_MUSIC, RM_OPT_FPS, RM_OPT_MENUS, RM_OPT_VIEWDIST };
enum { RM_MUSIC_PC, RM_MUSIC_N64, RM_MUSIC_N64_PAL, RM_MUSIC_COUNT };
enum { RM_MENUS_TGR, RM_MENUS_BR, RM_MENUS_TGR_PAL };
#if TGR_REGION_JPN
#define RM_MENUS_OWN RM_MENUS_TGR_PAL   /* this game's menus: the Menus choice that is ours */
#else
#define RM_MENUS_OWN RM_MENUS_TGR
#endif
enum { RM_VIEW_ORIGINAL, RM_VIEW_UNLIMITED };
void tgr_menus_away(void);              /* src/racing/handoff.c */
void tgr_menus_other(int menus);        /* src/racing/handoff.c: the other cartridge's menus */
void rm_menu_tune_restart(void);        /* rmregion.c */

#define RM_FIRST 3                      /* our rows: after BGM, SFX and Units */
#define RM_ROWS 5
#define RM_ICONS 0x80600000u            /* the arena's upper half (no Expansion Pak in the game) */

static const int k_opt[RM_ROWS] = { RM_OPT_WIDESCREEN, RM_OPT_MUSIC, RM_OPT_FPS, RM_OPT_VIEWDIST, RM_OPT_MENUS };
static MenuItem s_row[RM_ROWS];
static char s_label[RM_ROWS][32];
static MenuItem *s_items[16];
static unsigned int s_crt, s_wide, s_n64;   /* the icons, placed */
static unsigned int s_course, s_coursecut; /* View Distance's: Unlimited's, Original's */
static unsigned int s_box[3];           /* the boxes, by Menus choice: TGR USA, Boss Rally, TGR PAL/Japan */
static int s_menus_next = -1;           /* the Menus chosen, taken on leaving the screen (-1: none) */
static int s_turn_row = -1;             /* the row whose box is turning (-1: none) */
static float s_turn;                    /* its turn left, 1 to 0 */
static float s_turn_dir;                /* 1 or -1: the way it turns */
static unsigned int s_turn_old;         /* the box it turns from, until it is edge on */
static float s_turn_at;                 /* its angle as drawn, unwrapped (BrMenu wraps its spin) */
static int s_turn_half;                 /* the half turn of the spin it started in */
static int s_turn_fresh;                /* started this frame: rm_options_row's own rm_options_frame */

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
  s_box[RM_MENUS_TGR] = at;
  at = place(at, rm_box_usa, rm_box_usa_len, 1.0f);
  s_box[RM_MENUS_BR] = at;
  at = place(at, rm_box_br, rm_box_br_len, 1.0f);
  s_box[RM_MENUS_TGR_PAL] = at;
  at = place(at, rm_box_jpn, rm_box_jpn_len, 1.0f);
}

/* the box of a Music choice */
static unsigned int music_box(int music)
{
  return s_box[music == RM_MUSIC_PC ? RM_MENUS_BR : music == RM_MUSIC_N64 ? RM_MENUS_TGR : RM_MENUS_TGR_PAL];
}

/* the turn, on the screen's own spin: the angle it adds (BrMenu draws the
 * row's icon at D_80316228 and this) */
static float turn_angle(void)
{
  return s_turn_dir * -s_turn * 6.2831855f;
}

/* a half turn of the spin as drawn, counted from edge on (an icon's front,
 * its -y, faces the screen at 0: BrMenuIconDraw turns it about its z, then
 * stands it up, -90 degrees about x) */
static int half_turn(float a)
{
  return (int)floorf((a - 1.5707964f) / 3.1415927f);
}

/* the row's box turns from `old` to its new one */
static void turn_start(int k, unsigned int old, int dir)
{
  s_turn_row = RM_FIRST + k;
  s_turn = 1.0f;
  s_turn_dir = (float)dir;
  s_turn_old = old;
  s_turn_at = D_80316228 + turn_angle();
  s_turn_half = half_turn(s_turn_at);
  s_turn_fresh = 1;
}

/* the angle a row's icon turns by (BrMenu, frontbuttons.c (port)): the turn
 * of a box on this screen's row, else 0 */
float rm_options_turn(MenuItem **items, int row)
{
  return items == s_items && row == s_turn_row ? turn_angle() : 0.0f;
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
  unsigned int old = 0;
  int wide = rm_option(RM_OPT_WIDESCREEN) != 0;
  int music = rm_option(RM_OPT_MUSIC);
#if TGR_REGION_JPN                      /* (Japan's font has no brackets) */
  static const char *const k_music[RM_MUSIC_COUNT] = { "Boss Rally", "TGR USA", "TGR PAL/Japan" };
#else
  static const char *const k_music[RM_MUSIC_COUNT] = { "Boss Rally", "TGR (USA)", "TGR (PAL/Japan)" };
#endif

  sprintf(s_label[0], "Widescreen: %s", wide ? "On" : "Off");
  sprintf(s_label[1], "Music: %s", k_music[music]);
  sprintf(s_label[2], "Show FPS: %s", rm_option(RM_OPT_FPS) ? "On" : "Off");
  s_row[0].label = tgr_addr32(s_label[0]);
  s_row[1].label = tgr_addr32(s_label[1]);
  sprintf(s_label[3], "View Distance: %s", rm_option(RM_OPT_VIEWDIST) == RM_VIEW_UNLIMITED ? "Unlimited" : "Original");
  {
    int m = s_menus_next >= 0 ? s_menus_next : RM_MENUS_OWN;
#if TGR_REGION_JPN
    sprintf(s_label[4], "Menus: %s", m == RM_MENUS_BR ? "Boss Rally" : m == RM_MENUS_TGR ? "TGR USA" : "TGR PAL/Japan");
#else
    sprintf(s_label[4], "Menus: %s", m == RM_MENUS_BR ? "Boss Rally" : m == RM_MENUS_TGR ? "TGR (USA)" : "TGR (PAL/Japan)");
#endif
    s_row[4].icon = s_box[m];
  }
  s_row[2].label = tgr_addr32(s_label[2]);
  s_row[3].label = tgr_addr32(s_label[3]);
  s_row[4].label = tgr_addr32(s_label[4]);
  s_row[0].icon = wide ? s_wide : s_crt;
  s_row[1].icon = music_box(music);
  s_row[3].icon = rm_option(RM_OPT_VIEWDIST) == RM_VIEW_UNLIMITED ? s_course : s_coursecut;
  /* a box turning: on, the box it turns from until the spin as drawn this
     frame (BrMenu's, after this) has passed edge on */
  if (s_turn_row >= 0 && s_turn_fresh) {
    s_turn_fresh = 0;                   /* (its frame drawn: the next frame's turns it) */
    old = s_turn_old;
  } else if (s_turn_row >= 0) {
    float was = turn_angle();
    s_turn -= D_8028AAD8 + D_8028AAD8;  /* (the game's own turn: BrMenu's D_80316224) */
    if (s_turn < 0.0f) {
      s_turn = 0.0f;
    }
    s_turn_at += D_8028AAD8 + D_8028AAD8 + turn_angle() - was;   /* (the spin: BrMenu's step) */
    if (s_turn == 0.0f) {
      s_turn_row = -1;
    } else if (half_turn(s_turn_at) == s_turn_half) {
      old = s_turn_old;
    }
    if (old != 0) {
      s_row[s_turn_row - RM_FIRST].icon = old;
    }
  }
}

/* 1 for one of our rows */
int rm_options_ours(int row)
{
  return row >= RM_FIRST && row < RM_FIRST + RM_ROWS;
}

/* leaving the screen: the Menus chosen on it, if not this game's, in front
 * (until the player is back, to the main menu the screen leaves for) */
static void menus_leave(void)
{
  int next = s_menus_next;

  s_menus_next = -1;
  if (next < 0 || next == RM_MENUS_OWN) {
    return;
  }
  rm_option_set(RM_OPT_MENUS, next);
  if (next == RM_MENUS_BR) {
    tgr_menus_away();                   /* Boss Rally's menus, until the player is back */
  } else {
    tgr_menus_other(next);              /* the other cartridge's, likewise */
  }
}

/* the menu's answer on `row`: ours taken here (-1 back: nothing for the
 * game's switch); else the row's number in the game's own screen.  B (2: the
 * menu backed out) is the screen's whatever the row, and goes to its switch,
 * the Menus chosen taken first */
int rm_options_row(int result, int row)
{
  if (result == 2) {
    menus_leave();
  }
  if (rm_options_ours(row) && result != 2) {
    if (result == 1 || result == 3 || result == 4) {
      int o = k_opt[row - RM_FIRST];
      if (o == RM_OPT_MENUS) {
        /* the next of the three, USA, PAL/Japan, Boss Rally (down goes the
           other way round), shown now and taken on leaving the screen */
        static const int k_cycle[3] = { RM_MENUS_TGR, RM_MENUS_TGR_PAL, RM_MENUS_BR };
        int m = s_menus_next >= 0 ? s_menus_next : RM_MENUS_OWN, i;
        for (i = 0; i < 3 && k_cycle[i] != m; i++) {
        }
        turn_start(row - RM_FIRST, s_box[m], result == 4 ? -1 : 1);
        m = k_cycle[(i + (result == 4 ? 2 : 1)) % 3];
        s_menus_next = m == RM_MENUS_OWN ? -1 : m;
      } else if (o == RM_OPT_MUSIC) {
        int was = rm_option(o), now = (was + (result == 4 ? RM_MUSIC_COUNT - 1 : 1)) % RM_MUSIC_COUNT;
        turn_start(row - RM_FIRST, music_box(was), result == 4 ? -1 : 1);
        rm_option_set(o, now);
        if (was != RM_MUSIC_PC && now != RM_MUSIC_PC) {
          rm_menu_tune_restart();       /* the other cartridge's front-end tune */
        }
      } else {
        rm_option_set(o, !rm_option(o));
      }
      rm_options_frame();
    }
    return -1;
  }
  return row < RM_FIRST ? row : rm_options_ours(row) ? 0 : row - RM_ROWS;
}
