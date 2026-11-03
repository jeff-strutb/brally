/* rmprompt.c (port): Top Gear Rally Remastered's button prompts.  The screens
 * show the N64's A and B buttons (the menus' foot, the popups' buttons) and say
 * "press A"; Remastered's controls are the app's (ports/remastered/platform/
 * controls.h), so each prompt names what the first player has that control on:
 * the key, or the pad button when a pad was the last thing they used.  A
 * button's picture becomes a button of the cartridge's kind with that name on
 * it, the size of the picture: round, A blue and B green, lit from the upper
 * left as its pictures are; a name too wide for it is drawn in a smaller font,
 * then on a longer button with round ends.  On the iPhone the prompts
 * are touch's: the words at the foot are buttons to tap, a popup's button
 * pictures say TAP (their words are tapped), and a line says tap or swipe. */
#include "tgr/common.h"
#include "tgr/image.h"
#include "tgr/romimage.h"
#include <math.h>

/* -- declarations -- */
void BrTextPrint(char *s, int x, int y);
int BrTextWidth(unsigned char *s, int size);
void BrBevelPanel(int x, int y, int w, int h, int bevel, char noFill, char sunken, unsigned char r,
                  unsigned char g, unsigned char b);
extern char D_8028BDC0;                 /* the text printer's state (textstate.c) */
extern unsigned char D_8028BDC4;
extern int D_8028BDCC, D_8028BDD0, D_8028BDD4, D_8028BDD8, D_8028BDDC, D_8028BDE0, D_8028BDE4;
extern int D_803519D8;
extern BrImage D_8028D0B0;              /* the A button */
extern BrImage D_8028D0E0;              /* the B button */
int rm_bind(int player, int act, int kind);   /* the app's (controls.h) */
const char *rm_bind_label(int kind, int code, char *buf, int n);
int tgr_input_prompt_mode(void);        /* si.c: 0 keys, 1 a pad, 2 touch */
void BrRomImageDraw(BrRomImage *img, int x, int y, int w, int h, int pr, int pg, int pb, int pa, int er, int eg,
                    int eb, int ea, int mode);
/* -- end declarations -- */

enum { RM_BIND_KEY, RM_BIND_PAD };

/* a name short enough for a key cap */
static const struct { const char *from, *to; } k_short[] = {
    { "ESCAPE", "ESC" }, { "UP ARROW", "UP" }, { "DOWN ARROW", "DOWN" }, { "LEFT ARROW", "LEFT" },
    { "RIGHT ARROW", "RIGHT" }, { "BACKSPACE", "BKSP" }, { "LEFT CTRL", "LCTRL" }, { "RIGHT CTRL", "RCTRL" },
    { "LEFT SHIFT", "LSHIFT" }, { "RIGHT SHIFT", "RSHIFT" }, { "LEFT ALT", "LALT" }, { "RIGHT ALT", "RALT" },
    { "PAD ENTER", "ENTER" }, { "RETURN", "ENTER" }, { "PAGE UP", "PGUP" }, { "PAGE DOWN", "PGDN" },
    { "DELETE", "DEL" }, { "INSERT", "INS" },
};

/* what player `player` presses for action `act` (the prompts' device: the
 * first player's last) */
static const char *prompt_of(int player, int act)
{
  static char buf[4][24];
  static int n;
  char *b = buf[n++ & 3];
  int mode = tgr_input_prompt_mode(), pad = mode == 1;
  int code = rm_bind(player, act, pad ? RM_BIND_PAD : RM_BIND_KEY);
  unsigned i;

  if (mode == 2) {
    return "TAP";
  }
  if (pad ? code < 0 : code <= 0) {     /* nothing there: the other's */
    pad = !pad;
    code = rm_bind(player, act, pad ? RM_BIND_PAD : RM_BIND_KEY);
  }
  rm_bind_label(pad ? RM_BIND_PAD : RM_BIND_KEY, code, b, sizeof buf[0]);
  if (pad && !strncmp(b, "PAD ", 4)) {
    memmove(b, b + 4, strlen(b + 4) + 1);
  }
  for (i = 0; i < sizeof k_short / sizeof k_short[0]; i++) {
    if (!strcmp(b, k_short[i].from)) {
      strcpy(b, k_short[i].to);
    }
  }
  return b;
}

/* what the first player presses for action `act` */
const char *rm_prompt(int act)
{
  return prompt_of(0, act);
}

/* the text printer's state, kept round a key cap */
typedef struct { int t[10]; } TextState;
static TextState save(void)
{
  TextState s;
  s.t[0] = D_8028BDC0; s.t[1] = D_8028BDC4; s.t[2] = D_8028BDCC; s.t[3] = D_8028BDD0;
  s.t[4] = D_8028BDD4; s.t[5] = D_8028BDD8; s.t[6] = D_8028BDDC; s.t[7] = D_8028BDE0;
  s.t[8] = D_8028BDE4; s.t[9] = D_803519D8;
  return s;
}

/* ---- the buttons' pictures ------------------------------------------------
 * The cartridge's A and B (the foot of its menus) are round buttons lit from
 * the upper left: a dark outline, a rim bright at the top left (1.55 to 1.65
 * times the face's colour) and dark at the bottom right (0.72), a face
 * shaded the same way more gently (1.3 by the rim at the top left, 0.78 at
 * the bottom right), measured from the pictures as the game draws them.  A
 * button here is drawn the same way at the size asked, with round ends when
 * it is longer than it is high, as a picture (RGBA 5551, four texels a pixel
 * of the 320-wide screen) made once a size and colour in the arena's top
 * (below rmregion.c's names, far above rmoptions.c's icons) and drawn as the
 * game draws its pictures (BrRomImageDraw). */
#define RM_CAPS 0x807B0000u
#define RM_CAPS_END 0x807F0000u           /* (rmregion.c's names) */
#define CAP_TEX 4
#define NCAP 16

static const unsigned char k_face[3][3] = {
  { 0x1e, 0x00, 0x9e },                 /* A: the face's middle, measured */
  { 0x00, 0x7a, 0x19 },                 /* B */
  { 0x58, 0x58, 0x60 },                 /* the others: the N64's grey */
};

typedef struct { int col, w, h; BrRomImage img; } Cap;
static Cap s_cap[NCAP];
static int s_ncap;
static unsigned int s_capat = RM_CAPS;

static unsigned char shade(int c, float k)
{
  float v = c * k;
  return (unsigned char)(v > 255.0f ? 255 : v < 0.0f ? 0 : v);
}

/* the picture of a w by h texel button in colour `col` */
static void cap_make(unsigned char *t, int col, int w, int h)
{
  const unsigned char *f = k_face[col];
  float r0 = h * 0.5f - 0.5f;           /* the outline's outside: the picture's edge, as the cartridge's */
  float ow = h / 24.0f, rw = h * 0.085f;    /* the outline (a pixel of its 24) and the rim, in from it */
  float cy = h * 0.5f, cx0 = h * 0.5f, cx1 = w - h * 0.5f;
  int i, j;

  for (j = 0; j < h; j++) {
    for (i = 0; i < w; i++) {
      float px = i + 0.5f, py = j + 0.5f;
      float qx = px < cx0 ? cx0 : px > cx1 ? cx1 : px;
      float dx = px - qx, dy = py - cy, r = sqrtf(dx * dx + dy * dy), d = r - r0;
      float s = r > 0.0f ? (-dx - dy) * 0.70710678f / r : 0.0f;   /* toward the light: 1 the top left */
      unsigned char c[3];
      int a = 1, k;

      if (d > 0.0f || d > -ow) {        /* the outline, and outside it (its colour: no light fringe) */
        c[0] = 3, c[1] = 2, c[2] = 3;
        a = d <= 0.0f;
      } else if (d > -(ow + rw)) {      /* the rim */
        float m = s > 0.0f ? 1.0f + 0.6f * s : 1.0f + 0.28f * s;
        for (k = 0; k < 3; k++) {
          c[k] = shade(f[k], m);
        }
      } else {                          /* the face: the light the stronger nearer the rim */
        float e = r / (r0 - ow - rw), m = s > 0.0f ? 1.0f + 0.3f * s * e : 1.0f + 0.22f * s * e;
        for (k = 0; k < 3; k++) {
          c[k] = shade(f[k], m);
        }
      }
      {
        unsigned int v = (c[0] >> 3) << 11 | (c[1] >> 3) << 6 | (c[2] >> 3) << 1 | a;
        unsigned char *o = t + ((h - 1 - j) * w + i) * 2;   /* (the bottom row first, as BrRomImageDraw has them) */
        o[0] = (unsigned char)(v >> 8);  /* (big-endian, as the RDP reads RDRAM) */
        o[1] = (unsigned char)v;
      }
    }
  }
}

/* the picture for a w by h texel button in colour `col`, made the first time */
static BrRomImage *cap_image(int col, int w, int h)
{
  int i;
  unsigned int n = (unsigned int)(w * h * 2 + 15) & ~15u;
  Cap *c;

  for (i = 0; i < s_ncap; i++) {
    if (s_cap[i].col == col && s_cap[i].w == w && s_cap[i].h == h) {
      return &s_cap[i].img;
    }
  }
  if (s_ncap == NCAP || s_capat + n > RM_CAPS_END) {   /* (full: begin again) */
    s_ncap = 0;
    s_capat = RM_CAPS;
  }
  c = &s_cap[s_ncap++];
  c->col = col;
  c->w = w;
  c->h = h;
  memset(&c->img, 0, sizeof c->img);
  c->img.data = s_capat;
  c->img.w = (unsigned short)w;
  c->img.h = (unsigned short)h;
  c->img.fmt = 0;                       /* G_IM_FMT_RGBA */
  c->img.siz = 2;                       /* 16-bit */
  cap_make(TGR_PTR(unsigned char *, s_capat), col, w, h);
  s_capat += n;
  return &c->img;
}

/* a button for action `act` at x, y, w by h (640-wide, as the panels are) */
static void cap_panel(int act, int x, int y, int w, int h)
{
  int col = act == 0 ? 0 : act == 1 ? 1 : 2;
  int tw = (w >> 1) * CAP_TEX, th = (h >> 1) * CAP_TEX;

  if (tw < th) {
    tw = th;
  }
  BrRomImageDraw(cap_image(col, tw, th), x >> 1, y >> 1, tw / CAP_TEX, th / CAP_TEX, 0, 0, 0, 0xff, 0, 0, 0, 0xff, 0);
}

/* a button for action `act` at x, y, w by h (in the 640-wide coordinates the
 * panels use) and the name on it, in white; its width (at least w: longer when
 * the name needs it, from x, its round ends kept clear) */
int rm_keycap(int act, int x, int y, int w, int h)
{
  TextState s = save();
  const char *name = rm_prompt(act);
  int size = 11, tw;

  while ((tw = BrTextWidth((unsigned char *)name, size)) * 2 + h * 3 / 10 > w && size > 8) {
    size--;                             /* (a long name: smaller, then a longer button) */
  }
  if (tw * 2 + h * 3 / 10 > w) {        /* (the text kept off the round ends) */
    w = (tw * 2 + h * 3 / 10 + 1) & ~1;
  }
  cap_panel(act, x, y, w, h);
  D_8028BDC0 = 0;                       /* highlight off */
  D_8028BDC4 = 2;                       /* centred */
  D_8028BDCC = 1;                       /* white */
  D_8028BDD0 = D_8028BDD4 = D_8028BDD8 = 0xff;
  D_8028BDDC = D_8028BDE0 = D_8028BDE4 = 0xff;
  D_803519D8 = size;
  /* (y: near the text's foot; the font's slant puts its centre a pixel right of
     where it is asked for, so asked a pixel left: on the button's middle, a
     pixel up and left of it as the cartridge's letters are) */
  BrTextPrint((char *)name, ((x + w / 2) >> 1) - 1, ((y + h / 2) >> 1) + size / 2 - 2);
  D_8028BDC0 = s.t[0]; D_8028BDC4 = s.t[1]; D_8028BDCC = s.t[2]; D_8028BDD0 = s.t[3];
  D_8028BDD4 = s.t[4]; D_8028BDD8 = s.t[5]; D_8028BDDC = s.t[6]; D_8028BDE0 = s.t[7];
  D_8028BDE4 = s.t[8]; D_803519D8 = s.t[9];
  return w;
}

/* a foot-of-screen prompt: the cap for `act`, then its word, from x (320 wide), at
 * text height ty; on touch the word alone, on a button (it is tapped); the x
 * after it */
int rm_foot_prompt(int act, const char *word, int x, int ty)
{
  char buf[40];
  int w;

  w = BrTextWidth((unsigned char *)word, 0xb);
  if (tgr_input_prompt_mode() == 2) {   /* touch: the word on its button, white */
    TextState t = save();
    cap_panel(act, x * 2 - 6, 0x1ac, w * 2 + 12, 0x20);
    D_8028BDCC = 1;                     /* white */
    D_8028BDD0 = D_8028BDD4 = D_8028BDD8 = 0xff;
    D_8028BDDC = D_8028BDE0 = D_8028BDE4 = 0xff;
    D_803519D8 = 0xb;
    BrTextPrint((char *)word, x, ty);
    D_8028BDCC = t.t[2]; D_8028BDD0 = t.t[3]; D_8028BDD4 = t.t[4]; D_8028BDD8 = t.t[5];
    D_8028BDDC = t.t[6]; D_8028BDE0 = t.t[7]; D_8028BDE4 = t.t[8];
    return x + w;
  }
  x += rm_keycap(act, x * 2, 0x1b0, 0x18, 0x18) / 2 + 3;
  sprintf(buf, "%%ww%s", word);
  D_803519D8 = 0xb;
  BrTextPrint(buf, x, ty);
  return x + w;
}

/* BrImageDrawAt's: the A and B buttons' pictures as key caps (1: drawn) */
int rm_keycap_image(BrImage *img, int x, int y)
{
  if (img == &D_8028D0B0) {
    rm_keycap(0, x, y, img->drawW, img->drawH);   /* Accelerate / Select: A */
    return 1;
  }
  if (img == &D_8028D0E0) {
    rm_keycap(1, x, y, img->drawW, img->drawH);   /* Brake / Back: B */
    return 1;
  }
  return 0;
}

/* a line naming controls: {A} (Accelerate / Select), {B} (Brake / Back), {UP},
 * {DOWN} and {START} filled in with what player `player` has them on */
char *rm_ptext_p(int player, const char *fmt)
{
  static const struct { const char *keys, *touch; } k_touch[] = {   /* touch's words for them */
    { "PRESS {A}", "TAP" }, { "Press {A}", "Tap" }, { "pressing {A}", "tapping" }, { "{A} WHEN", "TAP WHEN" },
    { "Push {UP} or {DOWN}", "Swipe up or down" },
  };
  static char tbuf[96];
  static const struct { const char *tok; int act; } k_tok[] = {
    { "{A}", 0 }, { "{B}", 1 }, { "{UP}", 8 }, { "{DOWN}", 9 }, { "{START}", 10 },
  };
  static char buf[4][96];
  static int n;
  char *out = buf[n++ & 3], *o = out;
  unsigned i;

  if (tgr_input_prompt_mode() == 2) {   /* the phrase as touch says it */
    char *t = tbuf;
    const char *f = fmt;
    while (*f && t < tbuf + sizeof tbuf - 1) {
      for (i = 0; i < sizeof k_touch / sizeof k_touch[0]; i++) {
        size_t l = strlen(k_touch[i].keys);
        if (!strncmp(f, k_touch[i].keys, l) && t + strlen(k_touch[i].touch) < tbuf + sizeof tbuf - 1) {
          strcpy(t, k_touch[i].touch);
          t += strlen(t);
          f += l;
          break;
        }
      }
      if (i == sizeof k_touch / sizeof k_touch[0]) {
        *t++ = *f++;
      }
    }
    *t = 0;
    fmt = tbuf;
  }

  while (*fmt && o < out + sizeof buf[0] - 1) {
    for (i = 0; i < sizeof k_tok / sizeof k_tok[0]; i++) {
      size_t l = strlen(k_tok[i].tok);
      if (!strncmp(fmt, k_tok[i].tok, l)) {
        const char *p = prompt_of(player, k_tok[i].act);
        while (*p && o < out + sizeof buf[0] - 1) {
          *o++ = *p++;
        }
        fmt += l;
        break;
      }
    }
    if (i == sizeof k_tok / sizeof k_tok[0]) {
      *o++ = *fmt++;
    }
  }
  *o = 0;
  return out;
}

/* the same, the first player's */
char *rm_ptext(const char *fmt)
{
  return rm_ptext_p(0, fmt);
}
