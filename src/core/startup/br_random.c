/* br_random.c -- startup: the game's random number source.
 *
 * Filed out of the address batch slice4_52.c.  The Glide build keeps its own
 * generator state here; the port arm steps the D3D one through slice2_22.h,
 * which is how that batch reached it.
 */
#include <stdint.h>

#include "slice4_52.h"      /* g_brA9BFD0 */
#include "slice2_22.h"      /* BrDPlayRandStep */

/* WHAT IT DOES: the game's random number source. Each call moves the shared
 * generator on one step and hands back the new value, which is always positive
 * because the generator only ever produces 31 bits (masked by 0x7FFFFFFF). */
/* @implements 0x100353D0 glide BrRandom */
/* @implements 0x1003BD50 d3d BrRandom */
#ifdef BR_MATCHING_BUILD
/* Glide 0x100353D0: seed * 16807 & 0x7FFFFFFF via LEA chain (41 B, 2 relocs).
 * Compiler loads seed into ECX, builds EAX = ECX*16807 via shifts+LEA,
 * masks to 31 bits, stores back, returns. D3D's state is at 0x10A9BFD0 via
 * BrDPlayRandStep; Glide has its own global at 0x10AC3060. */
static int32_t g_brAC3060; /* 0x10AC3060 -- Glide RNG state, separate from D3D's */
int BrRandom(void)
{
    uint32_t s = (uint32_t)g_brAC3060;
    s = (s * 16807u) & 0x07FFFFFFu;
    g_brAC3060 = (int32_t)s;
    return (int)s;
}
#else
int BrRandom(void)
{
    /* The D3D build uses g_brA9BFD0 and 27-bit mask via BrDPlayRandStep. */
    return (int)BrDPlayRandStep(&g_brA9BFD0);
}
#endif
