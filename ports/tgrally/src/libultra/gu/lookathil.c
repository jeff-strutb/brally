/* lookathil.c -- libultra's guLookAtHiliteF: a look-at matrix, the
 * reflection light directions and the two highlight positions for a
 * texture of twidth x theight.  (The ROM links it at 0x80266BE0; the decomp
 * fenced it as library code, so the port carries it like the rest of gu.)
 */
#include "tgr/common.h"

typedef struct {
  unsigned char col[3];
  char pad1;
  unsigned char colc[3];
  char pad2;
  signed char dir[3];
  char pad3;
  char pad4[4];                 /* libultra's Light: a union with long[4], 16 bytes */
} BrLookAtLight;
typedef struct { BrLookAtLight l[2]; } BrLookAt;     /* libultra's LookAt (RSP memory: bytes) */
typedef struct { int x1, y1, x2, y2; } BrHilite;      /* libultra's Hilite */

float sqrtf(float x);
void guMtxIdentF(float mf[4][4]);

#define FTOFRAC8(x) ((int)MIN(((x) * (128.0)), 127.0) & 0xff)
#define THRESH2 0.1
#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

/* WHAT IT DOES: The view matrix from eye to at with up, the two reflection
 * directions (right and up, as signed 8-bit fractions) with their colours,
 * and for each light the texture position of its highlight. */
/* @implements 0x80266BE0 tgr guLookAtHiliteF */
void guLookAtHiliteF(float mf[4][4], void *lp, void *hp, float xEye, float yEye, float zEye,
                     float xAt, float yAt, float zAt, float xUp, float yUp, float zUp,
                     float xl1, float yl1, float zl1, float xl2, float yl2, float zl2, int twidth,
                     int theight)
{
  BrLookAt *l = (BrLookAt *)lp;
  BrHilite *h = (BrHilite *)hp;
  float len, xLook, yLook, zLook, xRight, yRight, zRight, xHilite, yHilite, zHilite;

  guMtxIdentF(mf);

  xLook = xAt - xEye;
  yLook = yAt - yEye;
  zLook = zAt - zEye;
  len = -1.0 / sqrtf(xLook * xLook + yLook * yLook + zLook * zLook);
  xLook *= len;
  yLook *= len;
  zLook *= len;

  xRight = yUp * zLook - zUp * yLook;
  yRight = zUp * xLook - xUp * zLook;
  zRight = xUp * yLook - yUp * xLook;
  len = 1.0 / sqrtf(xRight * xRight + yRight * yRight + zRight * zRight);
  xRight *= len;
  yRight *= len;
  zRight *= len;

  xUp = yLook * zRight - zLook * yRight;
  yUp = zLook * xRight - xLook * zRight;
  zUp = xLook * yRight - yLook * xRight;
  len = 1.0 / sqrtf(xUp * xUp + yUp * yUp + zUp * zUp);
  xUp *= len;
  yUp *= len;
  zUp *= len;

  len = 1.0 / sqrtf(xl1 * xl1 + yl1 * yl1 + zl1 * zl1);
  xl1 *= len;
  yl1 *= len;
  zl1 *= len;

  xHilite = xl1 + xLook;
  yHilite = yl1 + yLook;
  zHilite = zl1 + zLook;
  len = sqrtf(xHilite * xHilite + yHilite * yHilite + zHilite * zHilite);
  if (len > THRESH2) {
    len = 1.0 / len;
    xHilite *= len;
    yHilite *= len;
    zHilite *= len;
    h->x1 = twidth * 4 + (xHilite * xRight + yHilite * yRight + zHilite * zRight) * twidth * 2;
    h->y1 = theight * 4 + (xHilite * xUp + yHilite * yUp + zHilite * zUp) * theight * 2;
  } else {
    h->x1 = twidth * 2;
    h->y1 = theight * 2;
  }

  len = 1.0 / sqrtf(xl2 * xl2 + yl2 * yl2 + zl2 * zl2);
  xl2 *= len;
  yl2 *= len;
  zl2 *= len;

  xHilite = xl2 + xLook;
  yHilite = yl2 + yLook;
  zHilite = zl2 + zLook;
  len = sqrtf(xHilite * xHilite + yHilite * yHilite + zHilite * zHilite);
  if (len > THRESH2) {
    len = 1.0 / len;
    xHilite *= len;
    yHilite *= len;
    zHilite *= len;
    h->x2 = twidth * 4 + (xHilite * xRight + yHilite * yRight + zHilite * zRight) * twidth * 2;
    h->y2 = theight * 4 + (xHilite * xUp + yHilite * yUp + zHilite * zUp) * theight * 2;
  } else {
    h->x2 = twidth * 2;
    h->y2 = theight * 2;
  }

  l->l[0].dir[0] = FTOFRAC8(xRight);
  l->l[0].dir[1] = FTOFRAC8(yRight);
  l->l[0].dir[2] = FTOFRAC8(zRight);
  l->l[1].dir[0] = FTOFRAC8(xUp);
  l->l[1].dir[1] = FTOFRAC8(yUp);
  l->l[1].dir[2] = FTOFRAC8(zUp);
  l->l[0].col[0] = 0x00;
  l->l[0].col[1] = 0x00;
  l->l[0].col[2] = 0x00;
  l->l[0].pad1 = 0x00;
  l->l[0].colc[0] = 0x00;
  l->l[0].colc[1] = 0x00;
  l->l[0].colc[2] = 0x00;
  l->l[0].pad2 = 0x00;
  l->l[1].col[0] = 0x00;
  l->l[1].col[1] = 0x80;
  l->l[1].col[2] = 0x00;
  l->l[1].pad1 = 0x00;
  l->l[1].colc[0] = 0x00;
  l->l[1].colc[1] = 0x80;
  l->l[1].colc[2] = 0x00;
  l->l[1].pad2 = 0x00;

  mf[0][0] = xRight;
  mf[1][0] = yRight;
  mf[2][0] = zRight;
  mf[3][0] = -(xEye * xRight + yEye * yRight + zEye * zRight);

  mf[0][1] = xUp;
  mf[1][1] = yUp;
  mf[2][1] = zUp;
  mf[3][1] = -(xEye * xUp + yEye * yUp + zEye * zUp);

  mf[0][2] = xLook;
  mf[1][2] = yLook;
  mf[2][2] = zLook;
  mf[3][2] = -(xEye * xLook + yEye * yLook + zEye * zLook);

  mf[0][3] = 0;
  mf[1][3] = 0;
  mf[2][3] = 0;
  mf[3][3] = 1;
}
