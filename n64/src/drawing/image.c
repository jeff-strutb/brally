/* image.c -- drawing 2D images (the front end's pictures) in horizontal strips
 */
#include "tgr/common.h"

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
