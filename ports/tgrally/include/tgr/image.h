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
#endif
