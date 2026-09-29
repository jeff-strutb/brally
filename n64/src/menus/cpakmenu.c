/* cpakmenu.c -- the Controller Pak menu
 */
#include "tgr/common.h"

/* -- declarations -- */
extern int D_8028A850;                  /* hi-res screen */
extern int D_8028A85C;                  /* the frame buffer being drawn */
extern unsigned int D_8031AA28[];       /* the frame buffers */
extern int D_8028AAB0;                  /* the screen width */
extern int D_8028DDD0;                  /* the debug text's x */
extern int D_8028DDD4;                  /* and y */
extern int D_8028DDD8;                  /* use the small font */
extern short D_8028DDDC;       /* the text colour */
extern short D_8028DFC0[][7][5];   /* the 5 by 7 font */
extern short D_8028F2A0[][7][3];   /* the 3 by 7 font, its own colours */

/* -- end declarations -- */

/* WHAT IT DOES: Does nothing. An empty function the retail build kept in
 * the Controller Pak menu code. */
/* @implements 0x80254870 tgr BrStub80254870 */
void BrStub80254870(void)
{
}

/* WHAT IT DOES: Print a string straight into the frame buffer shown last
 * (the debug text): each glyph, 5 by 7 (or 3 by 7 in the small font, whose
 * entries are their own colours), is drawn one pixel right and down of a
 * black 3 by 3 block around each of its pixels, in the text colour; a
 * newline moves down 8 rows (9 on a hi-res screen) and sets the next
 * line's x to 16 (64 hi-res); lower case prints as upper case, and
 * anything else unprintable as a space.  The frame buffer, fonts and colour
 * are shorts (the ROM re-reads the first store of each block with lh).
 * RESIDUE (308): the ROM keeps the column in a stack home and orders the
 * block stores row by row; ours holds more in saved registers. */
/* @implements 0x80254878 tgr BrDebugPrint */
void BrDebugPrint(unsigned char *s)
{
  short col;
  int w;
  unsigned char c;
  short *p;
  int x0;
  unsigned int n;
  short *g;
  int i;

  col = D_8028DDDC;
  w = D_8028AAB0 << D_8028A850;
  c = *s;
  p = (short *)D_8031AA28[D_8028A85C ^ 1] + D_8028DDD4 * w + D_8028DDD0;
  x0 = D_8028DDD0;
  for (;;) {
    if (c == 0) {
      D_8028DDD0 = x0;
      return;
    }
    if (c < 0x20 || c > 0x7e) {
      n = 0;
      if (c != '\n') {
        goto draw;
      }
      x0 = 16 << (D_8028A850 << 1);
      D_8028DDD4 += D_8028A850 + 8;
      p += (D_8028A850 + 8) * w;
    } else {
      if (c >= 'a' && c <= 'z') {
        n = (unsigned char)(c - 0x40);
      } else {
        if (c > 'z' && c < 0x7f) {
          c -= 0x1a;
        }
        n = (unsigned char)(c - 0x20);
      }
    draw:
#define DOT(k) \
      if (g[k]) { \
        p[k + 2] = p[k + 1] = p[k] = p[w + k + 2] = p[w + k] = p[w * 2 + k] = p[w * 2 + k + 1] = p[w * 2 + k + 2] = 1; \
      }
      if (D_8028DDD8 == 0) {
        g = D_8028DFC0[n][0];
        for (i = 0; i != 8; i++, p += w, g += 5) {
          if (i < 7) {
            DOT(0)
            DOT(1)
            DOT(2)
            DOT(3)
            DOT(4)
          }
          if (i != 0) {
            if (g[-5]) {
              p[1] = col;
            }
            if (g[-4]) {
              p[2] = col;
            }
            if (g[-3]) {
              p[3] = col;
            }
            if (g[-2]) {
              p[4] = col;
            }
            if (g[-1]) {
              p[5] = col;
            }
          }
        }
        p -= w * 8 - D_8028A850 - 6;
      } else {
        g = D_8028F2A0[n][0];
        for (i = 0; i != 8; i++, p += w, g += 3) {
          if (i < 7) {
            DOT(0)
            DOT(1)
            DOT(2)
          }
          if (i != 0) {
            if (g[-3]) {
              p[1] = g[-3];
            }
            if (g[-2]) {
              p[2] = g[-2];
            }
            if (g[-1]) {
              p[3] = g[-1];
            }
          }
        }
        p -= w * 8 - D_8028A850 - 4;
      }
#undef DOT
    }
    c = *++s;
  }
}

/* WHAT IT DOES: Does nothing. A second empty function in the Controller Pak
 * menu code. */
/* @implements 0x80254F2C tgr BrStub80254F2C */
void BrStub80254F2C(void)
{
}
