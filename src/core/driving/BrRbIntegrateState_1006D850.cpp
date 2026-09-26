/* WHAT IT DOES: advances a rigid body one time step: moves it by its speed,
 * turns it by the rate its orientation is changing, and renormalises the
 * orientation afterwards so accumulated rounding does not slowly distort the
 * body.  Speed, spin and the orientation rate are carried across unchanged,
 * since the previous step already updated them. */
/* @implements 0x1006D850 glide BrRbIntegrateState
 * @cpp_symbol _BrRbIntegrateState
 *
 * C++: the same body compiled as C puts the member on the fld side of every
 * `dt * member` product and packs the spills into the dead parameter slots.
 * As C++ each product is `fld dt; fmul member`, dt is reloaded per product,
 * and the three spilled products get their own 0x10 frame -- the original's
 * shape, byte for byte.  Source facts that carry the rest of it:
 *   - the products go through ONE four-float scratch array reused by the
 *     position and the orientation step (the spills land at +8, +4, +0xC of
 *     that one 16-byte frame);
 *   - speed, spin and rate are copied field by field (a struct assignment
 *     builds address temporaries the original does not have);
 *   - the TU's symbol table must be large (<math.h>): with only this file's
 *     own declarations VC5 breaks an x87 tie the other way (218 B differ).
 *     Any header of a few hundred declarations works (stdio, windows). */
#include <math.h>

struct BrVec3r { float x, y, z; };
struct BrVec4r { float f00, f04, f08, f0C; };

struct BrRbState {
    BrVec3r pos;      /* 0x00 */
    BrVec3r vel;      /* 0x0C */
    BrVec4r quat;     /* 0x18  w,x,y,z */
    BrVec3r angVel;   /* 0x28 */
    BrVec4r qDot;     /* 0x34  w,x,y,z */
};                    /* 0x44 */

extern "C" void BrVec4Normalise(BrVec4r *pV);       /* 0x1006DAD0 */

extern "C" void BrRbIntegrateState(BrRbState *pDst, const BrRbState *pSrc, float dt)
{
    float t[4];

    t[0] = dt * pSrc->vel.x;
    t[1] = dt * pSrc->vel.y;
    t[2] = dt * pSrc->vel.z;
    pDst->pos.x = pSrc->pos.x + t[0];
    pDst->pos.y = pSrc->pos.y + t[1];
    pDst->pos.z = pSrc->pos.z + t[2];
    pDst->vel.x = pSrc->vel.x;
    pDst->vel.y = pSrc->vel.y;
    pDst->vel.z = pSrc->vel.z;

    t[0] = dt * pSrc->qDot.f00;
    t[1] = dt * pSrc->qDot.f04;
    t[2] = dt * pSrc->qDot.f08;
    t[3] = dt * pSrc->qDot.f0C;
    pDst->quat.f00 = pSrc->quat.f00 + t[0];
    pDst->quat.f04 = pSrc->quat.f04 + t[1];
    pDst->quat.f08 = pSrc->quat.f08 + t[2];
    pDst->quat.f0C = pSrc->quat.f0C + t[3];

    BrVec4Normalise(&pDst->quat);

    pDst->angVel.x = pSrc->angVel.x;
    pDst->angVel.y = pSrc->angVel.y;
    pDst->angVel.z = pSrc->angVel.z;
    pDst->qDot.f00 = pSrc->qDot.f00;
    pDst->qDot.f04 = pSrc->qDot.f04;
    pDst->qDot.f08 = pSrc->qDot.f08;
    pDst->qDot.f0C = pSrc->qDot.f0C;
}
