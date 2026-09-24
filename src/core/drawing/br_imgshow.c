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
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include <stdlib.h>

typedef struct {
    float x, y, z;
    float r, g, b;
    float ooz, a, oow;
    float sow0, tow0, oow0;
    float sow1, tow1, oow1;
} GrVertex;

extern int  FUN_10003320(const char *pszName);                  /* CHK_FReadOpen */
extern void FUN_100034c0(void *pv, int cb, int n, int fp);      /* CHK_FRead     */
extern void FUN_100035e0(int fp);                               /* CHK_FClose    */
extern unsigned int FUN_10001000(unsigned int adler, const void *pv, unsigned int cb);
extern void FUN_100281c0(void);
extern int  FUN_10028200(int, int, int, int, int, int, int, int,
                         int, int, int, int, int, int, int);
extern void FUN_100283c0(int hTex, void *pv, int);
extern void FUN_10028420(int hTex);
extern void __stdcall grTexCombine(int, int, int, int, int, int, int);
extern void __stdcall grColorCombine(int, int, int, int, int);
extern void __stdcall grClipWindow(int, int, int, int);
extern void __stdcall grBufferClear(int, int, int);
extern void __stdcall grDrawTriangle(const GrVertex *, const GrVertex *,
                                     const GrVertex *);
extern void (*DAT_106b7ab8)(void);
extern int  DAT_100a7514, DAT_100a7518;                 /* screen width, height */
extern unsigned char DAT_1184c488[];                    /* the pixel buffer     */

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
    int fp;
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
    FUN_100283c0(hTex, DAT_1184c488, 0);
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
    a.sow0 = 0.0f;
    a.tow0 = 0.0f;
    a.r = 255.0f;
    a.g = 255.0f;
    a.b = 255.0f;
    b.x = x1;
    b.y = y1;
    b.oow = 1.0f;
    b.a = 255.0f;
    b.sow0 = s1;
    b.tow0 = 0.0f;
    b.r = 255.0f;
    b.g = 255.0f;
    b.b = 255.0f;
    c.x = x0;
    c.y = y0;
    c.oow = 1.0f;
    c.a = 255.0f;
    c.sow0 = 0.0f;
    c.tow0 = t1;
    c.r = 255.0f;
    c.g = 255.0f;
    c.b = 255.0f;
    d.x = x1;
    d.y = y0;
    d.oow = 1.0f;
    d.a = 255.0f;
    d.sow0 = s1;
    d.tow0 = t1;
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
#endif /* BR_MATCHING_BUILD */
