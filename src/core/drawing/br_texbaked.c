/* br_texbaked.c -- drawing: baked-in pictures handed to the texture backend.
 *
 * RESPONSIBILITY: drawing/ -- turn geometry and images into pixels.
 *
 * Filed out of the address batches, which are not modules.  Each function
 * here is one fourteen-argument call through the backend texture
 * constructor at 0x118AA0B0, turning a block of pixels that ships inside the
 * DLL into a texture and remembering the handle.  They are near-identical
 * and were scattered across three different batches.
 */
#include <stdint.h>

#ifdef BR_MATCHING_BUILD
/* 0x100739B0
 *
 * Fourteen constant arguments through the backend texture constructor at
 * 0x118AA0B0 -- the same cdecl as 0x10073980, last-arg-first: 0x40 x 0x40,
 * fmt 0, siz 4, source 0x100B94A8, result stored at 0x11829314. */
/* WHAT IT DOES: turns a baked-in 64-by-64 picture into a texture the rest of
 * the game can draw with, and remembers the handle the graphics backend
 * returns. */
/* @implements 0x100739B0 d3d BrSub100739B0 */
typedef void *(*BrSub100739B0Fn)(void *pSrc, int a2, int w, int h,
                                 int fmt, int siz, int b31, int b30,
                                 int b29, int b28, int a11, int a12,
                                 int a13, int a14);

extern BrSub100739B0Fn g_18AA0B0;     /* 0x118AA0B0 */
extern void           *g_1829314;     /* 0x11829314 */
extern unsigned char   g_0B94A8[];    /* 0x100B94A8 */

void BrSub100739B0(void)
{
    g_1829314 = g_18AA0B0(g_0B94A8, 0, 0x40, 0x40, 0, 4,
                          0, 0, 0, 0, 0, 0, 1, 0);
}
#endif
