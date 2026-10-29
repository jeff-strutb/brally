/* image.h -- a front-end image: a picture drawn in texture-sized strips
 * (0x30 bytes; native game state, its pixels loaded cartridge data).
 */
#ifndef TGR_IMAGE_H
#define TGR_IMAGE_H

typedef struct BrImage {
    TgrAddr data;        /* unsigned char * -- 0x00  pixels, top row first */
    int x4;
    int x8;
    unsigned char siz;          /* 0x0C  bits per pixel */
    char pad0d[3];
    int w;                      /* 0x10  pixels */
    unsigned int h;             /* 0x14  rows */
    unsigned int stripH;        /* 0x18  rows per strip (one texture load) */
    int x;                      /* 0x1C  where it is drawn */
    int y;                      /* 0x20 */
    int drawW;                  /* 0x24  drawn size */
    int drawH;                  /* 0x28 */
    unsigned char kind;         /* 0x2C  'c': drawn with the car-select combiner */
    char pad2d[3];
} BrImage;

void BrImageDrawAt(BrImage *img, int x, int y);
/* Europe and Japan pass each strip's palette (a TLUT) along: 0 is none */
#if TGR_REGION_EUR || TGR_REGION_JPN
void BrImageDraw(BrImage *img, void *tlut);
void BrImageStrip(BrImage *img, unsigned char *data, void *tlut, int w, int h, int x, int y, int dw,
                  int dh, unsigned char r, unsigned char g, unsigned char b);
#define BR_IMAGE_DRAW(img) BrImageDraw(img, 0)
#define STRIP_TLUT(t) t,
#else
void BrImageDraw(BrImage *img);
void BrImageStrip(BrImage *img, unsigned char *data, int w, int h, int x, int y, int dw, int dh,
                  unsigned char r, unsigned char g, unsigned char b);
#define BR_IMAGE_DRAW(img) BrImageDraw(img)
#define STRIP_TLUT(t)
#endif
#endif
