/* br_texbaked.c -- drawing: baked-in pictures handed to the texture backend.
 *
 * RESPONSIBILITY: drawing/ -- turn geometry and images into pixels.
 *
 * Filed out of the address batches, which are not modules.  Each function
 * here is one fourteen-argument call through the backend texture
 * constructor at 0x118AA0B0, turning a block of pixels that ships inside the
 * DLL into a texture and remembering the handle.  They are near-identical
 * and were scattered across three different batches.
 *
 * ONE DECLARATION FOR THE CONSTRUCTOR.  The three batches each spelled the
 * pointer's type differently -- `int`, `uint32_t` and `void *` for the same
 * fourteen 32-bit arguments -- which was invisible while they sat in
 * separate translation units and is a duplicate-declaration error once they
 * share one.  They are unified below and every function is byte-exact
 * against the original with the unified spelling.
 *
 * 0x118AA0B0 also has a SECOND name in this tree, `g_pfn18AA0B0`, used by
 * br_font.c and slice2_16.c; its only definition was the one in the
 * 0x10073B00 batch and it is kept here, so the two names for one address
 * now sit side by side rather than in separate translation units.  Merging
 * them is a rename across four files and is not part of a refile.
 */
#include <stdint.h>

#ifdef BR_MATCHING_BUILD

/* 0x118AA0B0 -- the backend texture constructor.  cdecl, last argument
 * pushed first; the handle comes back in eax. */
extern void *(*g_18AA0B0)(void *pSrc, void *pA2,
                          uint32_t w, uint32_t h,
                          uint32_t fmt, uint32_t siz,
                          uint32_t a7, uint32_t a8,
                          uint32_t a9, uint32_t a10,
                          uint32_t a11, uint32_t a12,
                          uint32_t a13, uint32_t a14);

/* ==========================================================================
 * 0x100739B0  (slice1_09)
 *
 * Fourteen constant arguments through the constructor -- the same cdecl as
 * 0x10073980, last-arg-first: 0x40 x 0x40, fmt 0, siz 4, source 0x100B94A8,
 * result stored at 0x11829314.
 * ========================================================================== */

extern void           *g_1829314;     /* 0x11829314 */
extern unsigned char   g_0B94A8[];    /* 0x100B94A8 */

/* WHAT IT DOES: turns a baked-in 64-by-64 picture into a texture the rest of
 * the game can draw with, and remembers the handle the graphics backend
 * returns. */
/* @implements 0x100739B0 d3d BrSub100739B0 */
void BrSub100739B0(void)
{
    g_1829314 = g_18AA0B0(g_0B94A8, 0, 0x40, 0x40, 0, 4,
                          0, 0, 0, 0, 0, 0, 1, 0);
}

/* ==========================================================================
 * 0x10073AC0  (slice6_74) -- backend texture constructor wrapper
 *
 * Fourteen constant arguments through the cdecl function pointer at
 * 0x118AA0B0 (the same constructor BrGbiTexCreate / 0x1002A280 uses), then
 * the handle in eax is stored at 0x100A6498.  Neighbours 0x10073950 and
 * 0x100739E0 are the same shape with different source/size/format.
 *
 * C argument order (last push is first arg): source 0x118AA8F8, aux
 * 0x118AA0D8, width 0x20, height 0x80, fmt 0, siz 2, then eight zeros.
 * fmt/siz (0, 2) is the RGBA16 pair BrGbiTexCreate selects for flag 0x1.
 * ========================================================================== */

/* 0x118AA8F8 -- source texel block; the original pushes the ADDRESS. */
extern uint8_t g_18AA8F8[];
/* 0x118AA0D8 -- second constructor argument; likewise an address. */
extern uint8_t g_18AA0D8[];
/* 0x100A6498 -- resulting texture handle. */
extern void *g_0A6498;

/* WHAT IT DOES: asks the graphics backend to turn a fixed 32-by-128 block of
 * pixels already in memory into a texture, and stores the handle it returns. */
/* @implements 0x10073AC0 d3d BrSub10073AC0 */
void BrSub10073AC0(void)
{
    g_0A6498 = g_18AA0B0(g_18AA8F8, g_18AA0D8,
                         0x20u, 0x80u, 0u, 2u,
                         0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u);
}

/* ==========================================================================
 * 7. 0x10073B00 -- create the 32x128 backend texture and keep the handle
 * ==========================================================================
 *
 * Twin of 0x10073AC0: same fourteen-argument call through the backend
 * texture constructor at 0x118AA0B0 (slice2_16.h's BrGbiTexCreateFn), same
 * 0x20 x 0x80 / fmt 0 / siz 2, but this one writes the returned handle to
 * BOTH 0x100A64A0 and 0x100A649C.  Store order is load-bearing: 0x100A64A0
 * first.  The two pointer arguments are the ADDRESSES of the globals, not
 * their contents -- `push imm32` of each VA, not `push dword ptr [VA]`.
 *
 * Matching-only: the constructor pointer and the two source objects have no
 * owner in the port tree, and inventing a size for the pixel buffers would
 * be a second view of storage this packet does not own. */
typedef void *(*BrTexCreateFn10073B00)(void *pSrc, void *pArg2,
                                       int w, int h, int fmt, int siz,
                                       int a7, int a8, int a9, int a10,
                                       int a11, int a12, int a13, int a14);

BrTexCreateFn10073B00 g_pfn18AA0B0; /* 0x118AA0B0 */
char                  g_18AA0F8;    /* 0x118AA0F8 */
char                  g_18AB0F8;    /* 0x118AB0F8 */
void                 *g_0A64A0;     /* 0x100A64A0 */
void                 *g_0A649C;     /* 0x100A649C */

/* WHAT IT DOES: asks the graphics backend to turn one of the game's pixel
 * buffers into a 32-by-128 texture and then remembers the handle in two
 * slots so later readers of either slot see the same texture. */
/* @implements 0x10073B00 d3d BrSub10073B00 */
void BrSub10073B00(void)
{
    /* Chained so eax is stored twice without a reload. */
    g_0A649C = g_0A64A0 = g_pfn18AA0B0(&g_18AA0F8, &g_18AB0F8,
                                       0x20, 0x80, 0, 2,
                                       0, 0, 0, 0,
                                       0, 0, 0, 0);
}

#endif /* BR_MATCHING_BUILD */
