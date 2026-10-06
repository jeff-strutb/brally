/* br_entstate.c -- writing an entity's position back into every copy of it.
 *
 * RESPONSIBILITY: what is in the world and where.  The entity here is
 * slice3_45.h's BrEntCar -- the physics-sized record -- which is a DIFFERENT
 * struct from slice1_05.h's same-named one in br_entity.c, so the two cannot
 * share a translation unit.
 *
 * Moved here out of src/brally/core/slice3_45.c (an address batch, not a module).
 */
#include "br_match.h"
#include "slice3_41.h"   /* BrDriverCar, the canonical record */

/* Header is cdecl (this, x, y, z). Original is thiscall with ret 0xC. */
#define BrEntSetPos BrEntSetPos_hdr
#include "slice3_45.h"
#undef BrEntSetPos

/* 0x10076420 */
/* WHAT IT DOES: move an entity to a position, writing the same three
 * coordinates into all the places the entity keeps them -- its matrix, its
 * cached position and two more copies. They are kept in step here rather
 * than derived, so all of them must be written. */
/* @implements 0x10076420 d3d BrEntSetPos */
/* @n64 0x8021FE04 located */
/* Struct second arg is not register-eligible, so __fastcall is thiscall. */
void BR_THISCALL1 BrEntSetPos(BrEntCar *pE, float x, float y, float z)
{
    /* Store order is the original's: mat0.m[3], f26C8, st, stB, stA. */
    (*(struct BrMat4 *)&((BrDriverCar *)(pE))->fwd).m[3][0] = x;
    (*(struct BrMat4 *)&((BrDriverCar *)(pE))->fwd).m[3][1] = y;
    (*(struct BrMat4 *)&((BrDriverCar *)(pE))->fwd).m[3][2] = z;

    (*(float (*)[3])&((BrDriverCar *)(pE))->f26C8)[0] = x;
    (*(float (*)[3])&((BrDriverCar *)(pE))->f26C8)[1] = y;
    (*(float (*)[3])&((BrDriverCar *)(pE))->f26C8)[2] = z;

    (*(struct BrRbState *)&((BrDriverCar *)(pE))->aBody[0].rb.st).pos.x = x;
    (*(struct BrRbState *)&((BrDriverCar *)(pE))->aBody[0].rb.st).pos.y = y;
    (*(struct BrRbState *)&((BrDriverCar *)(pE))->aBody[0].rb.st).pos.z = z;

    (*(struct BrRbState *)&((BrDriverCar *)(pE))->aBody[0].rb.st2).pos.x = x;
    (*(struct BrRbState *)&((BrDriverCar *)(pE))->aBody[0].rb.st2).pos.y = y;
    (*(struct BrRbState *)&((BrDriverCar *)(pE))->aBody[0].rb.st2).pos.z = z;

    (*(struct BrRbState *)&((BrDriverCar *)(pE))->aBody[0].rb.st1).pos.x = x;
    (*(struct BrRbState *)&((BrDriverCar *)(pE))->aBody[0].rb.st1).pos.y = y;
    (*(struct BrRbState *)&((BrDriverCar *)(pE))->aBody[0].rb.st1).pos.z = z;

    BrRbBuildMatrix(&(*(struct BrMat4 *)&((BrDriverCar *)(pE))->aBody[0].rb.m), &(*(struct BrRbState *)&((BrDriverCar *)(pE))->aBody[0].rb.st));
}
