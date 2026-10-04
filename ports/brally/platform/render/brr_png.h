/* brr_png.h: a frame to a PNG file (brr_png.c). */
#ifndef BR_BRR_PNG_H
#define BR_BRR_PNG_H

#include <stdint.h>

enum { BRR_PNG_RGBA, BRR_PNG_BGRA };   /* byte order of each 4-byte pixel */

/* w x h pixels, stride bytes apart, top row first; 1 when written */
int brr_png_write(const char *path, const uint8_t *px, int w, int h, int stride, int order);

#endif
