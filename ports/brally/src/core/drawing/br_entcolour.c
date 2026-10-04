/* br_entcolour.c -- drawing: an entity's artwork record and its paint.
 *
 * RESPONSIBILITY: drawing/ -- turn geometry and images into pixels.
 *
 * Filed out of slice3_45.c, an address batch and not a module.  Attaching a
 * car artwork record to an object and pushing the chosen paint colour down
 * into it are one pair: the setter ends by calling the repaint.  The colour
 * loses precision on the way, which is why reading it back does not give
 * what was chosen.
 *
 * slice3_45.c's preamble is carried over verbatim.  An include set that
 * looks redundant has already been shown elsewhere in this module to move
 * VC5's register allocation (see br_rdpmode.c).
 */
#include <math.h>
#include "slice3_41.h"   /* BrDriverCar, the canonical record */
#include <string.h>

#include "br_match.h"
/* Header is cdecl (this, x, y, z). Original is thiscall with ret 0xC. */
#define BrEntSetPos BrEntSetPos_hdr
/* The entity setters are thiscall with three stack floats; hide the
 * port's cdecl prototypes so the twins can carry the fastcall shape. */
#define BrEntSetMatrix      BrEntSetMatrix_port
#define BrEntSetVel         BrEntSetVel_port
#define BrEntSetAngVel      BrEntSetAngVel_port
#define BrEntSetOrientation BrEntSetOrientation_port
#define BrEntSetHeading     BrEntSetHeading_port
#include "slice3_45.h"
#undef BrEntSetMatrix
#undef BrEntSetVel
#undef BrEntSetAngVel
#undef BrEntSetOrientation
#undef BrEntSetHeading
#undef BrEntSetPos

/* 0x10076A00 */
/* WHAT IT DOES: pushes the paint colour a car has been given down into the
 * artwork the renderer actually uses, which is how the player's chosen colour
 * reaches the screen. The colour loses precision on the way -- it is stored
 * more coarsely than it was chosen -- so reading it back does not give the
 * same value. */
/* @implements 0x10076A00 d3d BrEntRefreshColour */
/* @n64 0x80220398 located */
void __fastcall BrEntRefreshColour(BrEntCar *pE)
{
    BrCarGfxSetColour((*(BrCarGfx * *)&((BrDriverCar *)(pE))->pModel), (*(unsigned char *)&((BrDriverCar *)(pE))->f29AC) >> 3, (*(unsigned char *)&((BrDriverCar *)(pE))->f29AD) >> 3, (*(unsigned char *)&((BrDriverCar *)(pE))->f29AE) >> 3);
    BrSub10062C50(pE);
}

/* 0x10076A40 */
/* WHAT IT DOES: attaches one of the sixteen car artwork records to this
 * object -- which model and textures it is drawn with -- and immediately
 * repaints it in its own colour. The record number is not checked at all, so
 * an out-of-range one silently points at whatever memory follows the table. */
/* @implements 0x10076A40 d3d BrEntSetRecord */
/* @n64 0x802203F0 located */
void __fastcall BrEntSetRecord(BrEntCar *pE, void *_dummy, int32_t idx)
{
    /* The original's exact shift/add chain, in uint32_t so it wraps the same
     * way: ((((idx*11) << 6) - idx) << 4) + idx, times 8 == idx * 89992. */
    uint32_t t = (uint32_t)idx;
    uint32_t d = t + t * 4u;      /* lea edx,[eax+eax*4] */
    d = t + d * 2u;               /* lea edx,[eax+edx*2] */
    d <<= 6;
    d -= t;
    d <<= 4;
    d += t;
    d *= 8u;                      /* lea eax,[edx*8 + 0x100C12A0] */

    (*(BrCarGfx * *)&((BrDriverCar *)(pE))->pModel) = (BrCarGfx *)(void *)(g_aBrC12A0 + d);
    BrEntRefreshColour(pE);
}
