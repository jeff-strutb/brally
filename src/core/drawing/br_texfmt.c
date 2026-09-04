/* br_texfmt.c -- drawing: describing a texture to the rasteriser.
 *
 * RESPONSIBILITY: drawing/ -- turn geometry and images into pixels.
 *
 * Filed out of slice1_04.c, an address batch.  These are the three small
 * codecs that turn a texture's width, height and N64 format code into the
 * numbers the backend wants: a size shift, an aspect code, and a pixel
 * format.  See slice1_04.h for the addresses and the packet they came from.
 */
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include "slice1_04.h"

/* 0x100251A0 */
/* WHAT IT DOES: picks the size code for a texture from its width and height:
 * it takes whichever is larger and works out which power of two it fits in,
 * counted downward from 256. It also reports whether the size was an exact
 * power of two or had to be rounded up. */
/* @implements 0x100251A0 d3d BrTexShiftFromSize */
int BrTexShiftFromSize(int *pShift, int a, int b)
{
    /* orig is two textually identical signed ladders (`cmp; jg`), one on `a`
     * when a > b and one on `b` otherwise -- not a shared helper. A factored
     * `BrTexShiftLadder` is two `call`s and 42 B against orig 430. */
    if (a > b) {
        if (a <=   1) { *pShift = 8; return 1; }
        if (a <=   2) { *pShift = 7; return 1; }
        if (a <=   4) { *pShift = 6; return 1; }
        if (a <=   8) { *pShift = 5; return 1; }
        if (a <=  16) { *pShift = 4; return 1; }
        if (a <=  32) { *pShift = 3; return 1; }
        if (a <=  64) { *pShift = 2; return 1; }
        if (a <= 128) { *pShift = 1; return 1; }
        if (a <= 256) { *pShift = 0; return 1; }
        *pShift = 0;
        return 0;
    }
    if (b <=   1) { *pShift = 8; return 1; }
    if (b <=   2) { *pShift = 7; return 1; }
    if (b <=   4) { *pShift = 6; return 1; }
    if (b <=   8) { *pShift = 5; return 1; }
    if (b <=  16) { *pShift = 4; return 1; }
    if (b <=  32) { *pShift = 3; return 1; }
    if (b <=  64) { *pShift = 2; return 1; }
    if (b <= 128) { *pShift = 1; return 1; }
    if (b <= 256) { *pShift = 0; return 1; }
    *pShift = 0;
    return 0;
}

/* 0x10028200 */
int BrTexAspectFromSize(int *pCode, int a, int b)
{
    int r;

    if (a > b) {
        r = (a * 8) / b;
        if (r == 0x40) { *pCode = 0; return 1; }
        if (r == 0x20) { *pCode = 1; return 1; }
        if (r == 0x10) { *pCode = 2; return 1; }
        /* No exact rung for r == 8 here: that would mean a == b, which this
         * branch has already excluded. */
        if (r > 0x40)  { *pCode = 0; return 0; }
        if (r > 0x20)  { *pCode = 1; return 0; }
        if (r > 0x10)  { *pCode = 2; return 0; }
        *pCode = 3;
        return 0;
    }

    r = (b * 8) / a;
    if (r == 0x40) { *pCode = 6; return 1; }
    if (r == 0x20) { *pCode = 5; return 1; }
    if (r == 0x10) { *pCode = 4; return 1; }
    if (r == 0x08) { *pCode = 3; return 1; }
    if (r > 0x40)  { *pCode = 6; return 0; }
    if (r > 0x20)  { *pCode = 5; return 0; }
    if (r > 0x10)  { *pCode = 4; return 0; }
    *pCode = 3;
    return 0;
}


/* 0x10027B90 */
/* WHAT IT DOES: decides which of the backend's pixel formats a texture
 * should be created in, from the N64's own format and size codes plus one
 * extra mode flag. Most combinations fall through to the same general
 * format; only a couple of specific pairings get a format of their own. */
/* @implements 0x10027B90 d3d BrTexFormatCode */
#ifdef BR_MATCHING_BUILD
/* The original keeps two SEMANTICALLY REDUNDANT `if (b == 2) return 11;`
 * early-outs (each a cmp/je straight into the shared return-11 tail) and a
 * dead read of b in the a == 2 arm (`mov eax,[esp+8]` immediately
 * overwritten). Both are source-level; folding them drops 5 compares. */
int BrTexFormatCode(int a, int b, int c)
{
    if (a == 0) {
        if (b == 2)
            return 11;
        if (b == 4) {
            /* `dec/neg/sbb eax,eax` yields 0 for c == 1 and -1 otherwise;
             * `and al,0xF7` then turns -1 into -9, and +11 gives 11 or 2. */
            return (c == 1) ? 11 : 2;
        }
    } else if (a == 1) {
        if (b == 2)
            return 11;
        if (b == 3) {
            /* same idiom, masked with 0xF8 and biased by 12 */
            return (c == 1) ? 12 : 4;
        }
        if (b == 4)
            return 2;
    } else if (a == 2) {
        (void)*(volatile int *)&b;      /* the dead load */
    }
    return 11;
}
#else
int BrTexFormatCode(int a, int b, int c)
{
    if (a == 0) {
        if (b == 4) {
            /* `dec/neg/sbb eax,eax` yields 0 for c == 1 and -1 otherwise;
             * `and al,0xF7` then turns -1 into -9, and +11 gives 11 or 2. */
            return (c == 1) ? 11 : 2;
        }
        return 11;
    }
    if (a == 1) {
        if (b == 3) {
            /* same idiom, masked with 0xF8 and biased by 12 */
            return (c == 1) ? 12 : 4;
        }
        if (b == 4) {
            return 2;
        }
        return 11;
    }
    /* a == 2 reloads b in the original and then discards it -- a dead load,
     * not a missing case. Everything here returns 11. */
    return 11;
}
#endif
