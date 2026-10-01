/* br_imgshow.c -- drawing: the full-screen `.img` blit, Glide 0x1006C990.
 *
 * Matching arm only.  The port's version of the same work is br_imgblit.c,
 * which splits it into load / place / quad / pass helpers behind an ops
 * table; this file is the original's one function, alone in its translation
 * unit as it was in BRGlide.dll (its two float constants, 0x10077C10 = 0.0f
 * and 0x10077C14 = 1.0f, are that TU's whole constant pool).
 *
 * Source facts the bytes fix, all measured:
 *  - the four vertices are four named locals initialised in the order the
 *    quad is drawn, (a, b, c) then (b, d, c); VC5 lays them out b, c, a, d;
 *  - each vertex is written x, y, oow, a, sow, tow, r, g, b;
 *  - the Adler seed is its own statement (a nested call pushes the outer
 *    arguments first);
 *  - the far corner computes y1 before x1.
 */
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include <stdlib.h>

/* GrVertex: glide.h (two TMUs, 0x3C bytes) */

/* FUN_10003320: prototype in br_funcs.h */
/* FUN_100034c0: prototype in br_funcs.h */
/* FUN_100035e0: prototype in br_funcs.h */
/* FUN_10001000: prototype in br_funcs.h */
/* FUN_100281c0: prototype in br_funcs.h */
/* FUN_10028200: prototype in br_funcs.h */
/* FUN_100283c0: prototype in br_funcs.h */
/* FUN_10028420: prototype in br_funcs.h */
/* grTexCombine: prototype in br_funcs.h */
/* grColorCombine: prototype in br_funcs.h */
/* grClipWindow: prototype in br_funcs.h */
/* grBufferClear: prototype in br_funcs.h */
/* grDrawTriangle: prototype in br_funcs.h */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                 /* screen width, height */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                    /* the pixel buffer     */

/* WHAT IT DOES: puts a full-screen picture up -- the boot splash and the
 * loading screen.  It reads the .img file (width, height, a format word and
 * the 16-bit pixels) into the shared buffer, checks it against the expected
 * checksum and quits the game if it does not match, loads it as one texture,
 * and draws it centred on screen as two triangles, twice -- once into each
 * buffer -- before releasing the textures again. */
/* @implements 0x1006C990 glide BrImgShowFullScreen */
void BrImgShowFullScreen(const char *pszName, unsigned int key)
{
    GrVertex a, b, c, d;
    FILE **fp;
    int cx, cy;
    int cb;
    int hTex;
    unsigned int seed;
    float x0, y0, x1, y1, s1, t1;

    fp = FUN_10003320(pszName);
    FUN_100034c0(&cx, 4, 1, fp);
    FUN_100034c0(&cy, 4, 1, fp);
    cb = cx * cy * 2;
    FUN_100034c0(DAT_1184c488, 4, 1, fp);
    FUN_100034c0(DAT_1184c488, cb, 1, fp);
    FUN_100035e0(fp);
    if (key != 0) {
        seed = FUN_10001000(0, 0, 0);
        if (FUN_10001000(seed, DAT_1184c488, cb) != key)
            exit(1);
    }

    FUN_100281c0();
    hTex = FUN_10028200(0, 3, 0x80, 0x20, 0xB, 2, 0, 0, 3, 0, 0, 0, 0, 0, 0);
    FUN_100283c0(hTex, DAT_1184c488);
    FUN_10028420(hTex);
    grTexCombine(0, 1, 0, 1, 0, 0, 0);
    grColorCombine(3, 8, 1, 1, 0);

    x0 = (float)((DAT_100a7514 - 0x100) / 2);
    if (x0 < 0.0f)
        x0 = 0.0f;
    y0 = (float)((DAT_100a7518 - 0x100) / 2);
    if (y0 < 0.0f)
        y0 = 0.0f;
    y1 = (float)cy + y0 - 1.0f;
    x1 = (float)cx + x0 - 1.0f;
    s1 = (float)(cx - 1);
    t1 = (float)(cy - 1);

    a.x = x0;
    a.y = y1;
    a.oow = 1.0f;
    a.a = 255.0f;
    a.tmuvtx[0].sow = 0.0f;
    a.tmuvtx[0].tow = 0.0f;
    a.r = 255.0f;
    a.g = 255.0f;
    a.b = 255.0f;
    b.x = x1;
    b.y = y1;
    b.oow = 1.0f;
    b.a = 255.0f;
    b.tmuvtx[0].sow = s1;
    b.tmuvtx[0].tow = 0.0f;
    b.r = 255.0f;
    b.g = 255.0f;
    b.b = 255.0f;
    c.x = x0;
    c.y = y0;
    c.oow = 1.0f;
    c.a = 255.0f;
    c.tmuvtx[0].sow = 0.0f;
    c.tmuvtx[0].tow = t1;
    c.r = 255.0f;
    c.g = 255.0f;
    c.b = 255.0f;
    d.x = x1;
    d.y = y0;
    d.oow = 1.0f;
    d.a = 255.0f;
    d.tmuvtx[0].sow = s1;
    d.tmuvtx[0].tow = t1;
    d.r = 255.0f;
    d.g = 255.0f;
    d.b = 255.0f;

    grClipWindow(0, 0, DAT_100a7514, DAT_100a7518);
    grBufferClear(0, 0, 0xFFFF);
    grDrawTriangle(&a, &b, &c);
    grDrawTriangle(&b, &d, &c);
    DAT_106b7ab8();
    grClipWindow(0, 0, DAT_100a7514, DAT_100a7518);
    grBufferClear(0, 0, 0xFFFF);
    grDrawTriangle(&a, &b, &c);
    grDrawTriangle(&b, &d, &c);
    DAT_106b7ab8();
    FUN_100281c0();
}
