/* romimage.h -- a ROM file holding an image (0x14 bytes): BrRomFile's
 * range and load address, then the image's size and format.  The records
 * are static in .data; BrRomImageDraw (gamedata/romfile.c) draws them.
 */
#ifndef TGR_ROMIMAGE_H
#define TGR_ROMIMAGE_H

typedef struct BrRomImage {
    int start;                  /* 0x00  ROM range */
    int end;                    /* 0x04 */
    TgrAddr data;               /* unsigned char * -- 0x08  where it was loaded */
    unsigned short w;           /* 0x0C */
    unsigned short h;           /* 0x0E */
    unsigned char fmt;          /* 0x10  G_IM_FMT_*; 2 = colour-indexed */
    unsigned char siz;          /* 0x11  0 4-bit, 1 8-bit, 2 16-bit */
} BrRomImage;

#endif
