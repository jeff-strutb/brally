/* rmprompt.c (port): Top Gear Rally Remastered's button prompts.  The screens
 * show the N64's A and B buttons (the menus' foot, the popups' buttons) and say
 * "press A"; Remastered's controls are the app's (ports/remastered/platform/
 * controls.h), so each prompt names what the first player has that control on:
 * the key, or the pad button when a pad was the last thing they used.  A
 * button's picture becomes a key cap with that name, the size of the picture;
 * a name too wide for it is drawn in a smaller font.  On the iPhone the prompts
 * are touch's: the words at the foot are buttons to tap, a popup's button
 * pictures say TAP (their words are tapped), and a line says tap or swipe. */
#include "tgr/common.h"
#include "tgr/image.h"

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

/* what the first player presses for action `act` */
const char *rm_prompt(int act)
{
  static char buf[4][24];
  static int n;
  char *b = buf[n++ & 3];
  int mode = tgr_input_prompt_mode(), pad = mode == 1;
  int code = rm_bind(0, act, pad ? RM_BIND_PAD : RM_BIND_KEY);
  unsigned i;

  if (mode == 2) {
    return "TAP";
  }
  if (pad ? code < 0 : code <= 0) {     /* nothing there: the other's */
    pad = !pad;
    code = rm_bind(0, act, pad ? RM_BIND_PAD : RM_BIND_KEY);
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

/* a cap's panel in its button's colour, as the cartridge's pictures have them:
 * A blue, B green (measured from its foot-of-screen icons); white names on them */
static void cap_panel(int act, int x, int y, int w, int h)
{
  if (act == 0) {
    BrBevelPanel(x, y, w, h, 2, 0, 0, 0x20, 0x00, 0x9c);
  } else if (act == 1) {
    BrBevelPanel(x, y, w, h, 2, 0, 0, 0x00, 0x77, 0x18);
  } else {
    BrBevelPanel(x, y, w, h, 2, 0, 0, 0x50, 0x50, 0x58);
  }
}

/* a key cap for action `act`: a raised panel at x, y, w by h (in the 640-wide
 * coordinates the panels use) and the name on it, in white; its width (at least
 * w: wider when the name needs it, from x) */
int rm_keycap(int act, int x, int y, int w, int h)
{
  TextState s = save();
  const char *name = rm_prompt(act);
  int size = 11, tw;

  while ((tw = BrTextWidth((unsigned char *)name, size)) * 2 + 4 > w && size > 8) {
    size--;                             /* (a long name: smaller, then a wider cap) */
  }
  if (tw * 2 + 8 > w) {
    w = tw * 2 + 8;
  }
  cap_panel(act, x, y, w, h);
  D_8028BDC0 = 0;                       /* highlight off */
  D_8028BDC4 = 2;                       /* centred */
  D_8028BDCC = 1;                       /* white */
  D_8028BDD0 = D_8028BDD4 = D_8028BDD8 = 0xff;
  D_8028BDDC = D_8028BDE0 = D_8028BDE4 = 0xff;
  D_803519D8 = size;
  BrTextPrint((char *)name, (x + w / 2) >> 1, ((y + h / 2) >> 1) + size / 2 - 3);   /* (y: near the text's foot) */
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
 * {DOWN} and {START} filled in with what the first player has them on */
char *rm_ptext(const char *fmt)
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
        const char *p = rm_prompt(k_tok[i].act);
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
