/* common.h -- base types for the Top Gear Rally (N64) decomp.
 *
 * The N64 tree is its own project: nothing here includes a PC header, and no
 * PC file includes anything here.  Engine types keep the PC decomp's names
 * (BrVec3, ...) so the two trees read the same, but each tree owns its copy --
 * the 1997 and 1999 sources are not the same source.
 */
#ifndef TGR_COMMON_H
#define TGR_COMMON_H

typedef signed char        s8;
typedef unsigned char      u8;
typedef signed short       s16;
typedef unsigned short     u16;
typedef signed int         s32;
typedef unsigned int       u32;
typedef float              f32;
typedef double             f64;

#define NULL 0

#endif
