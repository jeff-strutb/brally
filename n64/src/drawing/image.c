/* image.c -- drawing 2D images (the front end's pictures) in horizontal strips
 */
#include "tgr/common.h"
#include "tgr/gbi.h"

/* -- declarations -- */
typedef struct BrImage {
  unsigned char *data;          /* 0x00  pixels, top row first */
  int x4;
  int x8;
  unsigned char siz;            /* 0x0C  bits per pixel */
  char pad0d[3];
  int w;                        /* 0x10  pixels */
  unsigned int h;               /* 0x14  rows */
  unsigned int stripH;          /* 0x18  rows per strip (one texture load) */
  int x;                        /* 0x1C  where it is drawn */
  int y;                        /* 0x20 */
  int drawW;                    /* 0x24  drawn size */
  int drawH;                    /* 0x28 */
} BrImage;
void BrImageStrip(BrImage *img, unsigned char *data, int w, int h, int x, int y, int dw, int dh,
                  unsigned char r, unsigned char g, unsigned char b);
extern Gfx *D_8028A858;
extern int D_8028A850;
/* -- end declarations -- */

/* WHAT IT DOES: Draw an image at its own position and size, strip by
 * strip from the bottom of its pixel data. */
/* @implements 0x80245470 tgr BrImageDraw */
void BrImageDraw(BrImage *img)
{
  unsigned int total;
  unsigned int strip;
  int n;
  int step;
  int i;

  if (img->siz == 4) {
    total = img->w * img->h >> 1;
    strip = img->w * img->stripH >> 1;
  } else {
    total = img->w * img->h * (img->siz >> 3);
    strip = img->w * img->stripH * (img->siz >> 3);
  }
  n = img->h / img->stripH;
  step = img->drawH / n;
  for (i = 1; i <= n; i++) {
    BrImageStrip(img, img->data + total - strip * i, img->w, img->stripH, img->x,
                 img->y + step * (i - 1), img->drawW, step, 0, 0, 0);
  }
}

/* WHAT IT DOES: Draw an image at (x, y) at its own size, strip by strip. */
/* @implements 0x80245604 tgr BrImageDrawAt */
void BrImageDrawAt(BrImage *img, int x, int y)
{
  unsigned int total;
  unsigned int strip;
  int n;
  int step;
  int i;

  if (img->siz == 4) {
    total = img->w * img->h >> 1;
    strip = img->w * img->stripH >> 1;
  } else {
    total = img->w * img->h * (img->siz >> 3);
    strip = img->w * img->stripH * (img->siz >> 3);
  }
  n = img->h / img->stripH;
  step = img->drawH / n;
  for (i = 1; i <= n; i++) {
    BrImageStrip(img, img->data + total - strip * i, img->w, img->stripH, x, y + step * (i - 1),
                 img->drawW, step, 0, 0, 0);
  }
}

/* WHAT IT DOES: Draw an image at its own position and size, tinted by the
 * given colour. */
/* @implements 0x80245798 tgr BrImageDrawTinted */
void BrImageDrawTinted(BrImage *img, unsigned char r, unsigned char g, unsigned char b)
{
  unsigned int total;
  unsigned int strip;
  int n;
  int step;
  int i;

  if (img->siz == 4) {
    total = img->w * img->h >> 1;
    strip = img->w * img->stripH >> 1;
  } else {
    total = img->w * img->h * (img->siz >> 3);
    strip = img->w * img->stripH * (img->siz >> 3);
  }
  n = img->h / img->stripH;
  step = img->drawH / n;
  for (i = 1; i <= n; i++) {
    BrImageStrip(img, img->data + total - strip * i, img->w, img->stripH, img->x,
                 img->y + step * (i - 1), img->drawW, step, r, g, b);
  }
}

/* WHAT IT DOES: Draw an image at (x, y), w by h on screen, tinted by the
 * given colour. */
/* @implements 0x8024594C tgr BrImageDrawRect */
void BrImageDrawRect(BrImage *img, int x, int y, int w, int h, unsigned char r, unsigned char g,
                     unsigned char b)
{
  int step;
  unsigned int total;
  unsigned int strip;
  int n;
  int i;

  if (img->siz == 4) {
    total = img->w * img->h >> 1;
    strip = img->w * img->stripH >> 1;
  } else {
    total = img->w * img->h * (img->siz >> 3);
    strip = img->w * img->stripH * (img->siz >> 3);
  }
  n = img->h / img->stripH;
  step = h / n;
  for (i = 1; i <= n; i++) {
    BrImageStrip(img, img->data + total - strip * i, img->w, img->stripH, x, y + step * (i - 1),
                 w, step, r, g, b);
  }
}

/* WHAT IT DOES: Plot one grey pixel at (x, y) (fill mode, RGBA5551), in
 * 640-wide coordinates halved on a low-res screen. */
/* @implements 0x80246A80 tgr BrFillPoint */
void BrFillPoint(int x, int y, unsigned char v)
{
  if (D_8028A850 == 0) {
    x >>= 1;
    y >>= 1;
  }
  gDPPipeSync(D_8028A858++);
  gDPSetCycleType(D_8028A858++, G_CYC_FILL);
  gDPSetRenderMode(D_8028A858++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
  gDPSetFillColor(D_8028A858++, GPACK_RGBA5551(v, v, v, 1) << 16 | GPACK_RGBA5551(v, v, v, 1));
  gDPFillRectangle(D_8028A858++, x, y, x + 1, y + 1);
  gDPPipeSync(D_8028A858++);
  gDPSetCycleType(D_8028A858++, G_CYC_1CYCLE);
}

/* WHAT IT DOES: Draw the outline of a w by h box at (x, y), t pixels thick,
 * in one colour: top, bottom, left and right edges as fill rectangles. */
/* @implements 0x80246BCC tgr BrFillFrame */
void BrFillFrame(int x, int y, int w, int h, int t, unsigned char r, unsigned char g,
                 unsigned char b)
{
  if (D_8028A850 == 0) {
    x >>= 1;
    y >>= 1;
    w >>= 1;
    h >>= 1;
    t >>= 1;
  }
  if (t < 2) {
    t = 1;
  }
  gDPPipeSync(D_8028A858++);
  gDPSetCycleType(D_8028A858++, G_CYC_FILL);
  gDPSetRenderMode(D_8028A858++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
  gDPSetFillColor(D_8028A858++, GPACK_RGBA5551(r, g, b, 1) << 16 | GPACK_RGBA5551(r, g, b, 1));
  gDPFillRectangle(D_8028A858++, x, y, x + w, y + t);
  gDPFillRectangle(D_8028A858++, x, y + h - t, x + w, y + h);
  gDPFillRectangle(D_8028A858++, x, y, x + t, y + h);
  gDPFillRectangle(D_8028A858++, x + w - t, y, x + w, y + h);
  gDPPipeSync(D_8028A858++);
  gDPSetCycleType(D_8028A858++, G_CYC_1CYCLE);
}

/* WHAT IT DOES: Fill a w by h rectangle at (x, y) in one colour (fill mode,
 * RGBA5551), in 640-wide coordinates halved on a low-res screen; at least
 * one pixel each way. */
/* @implements 0x80246E10 tgr BrFillRect */
void BrFillRect(int x, int y, int w, int h, unsigned char r, unsigned char g, unsigned char b)
{
  if (D_8028A850 == 0) {
    x >>= 1;
    y >>= 1;
    w >>= 1;
    h >>= 1;
  }
  if (w < 2) {
    w = 1;
  }
  if (h < 2) {
    h = 1;
  }
  gDPPipeSync(D_8028A858++);
  gDPSetCycleType(D_8028A858++, G_CYC_FILL);
  gDPSetRenderMode(D_8028A858++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
  gDPSetFillColor(D_8028A858++, GPACK_RGBA5551(r, g, b, 1) << 16 | GPACK_RGBA5551(r, g, b, 1));
  gDPFillRectangle(D_8028A858++, x, y, x + w, y + h);
  gDPPipeSync(D_8028A858++);
  gDPSetCycleType(D_8028A858++, G_CYC_1CYCLE);
}
