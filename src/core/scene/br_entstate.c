/* br_entstate.c -- writing an entity's position back into every copy of it.
 *
 * RESPONSIBILITY: what is in the world and where.  The entity here is
 * slice3_45.h's BrEnt -- the physics-sized record -- which is a DIFFERENT
 * struct from slice1_05.h's same-named one in br_entity.c, so the two cannot
 * share a translation unit.
 *
 * Moved here out of src/core/slice3_45.c (an address batch, not a module).
 */
#include "br_match.h"

#ifdef BR_MATCHING_BUILD
/* Header is cdecl (this, x, y, z). Original is thiscall with ret 0xC. */
#define BrEntSetPos BrEntSetPos_hdr
#endif
#include "slice3_45.h"
#ifdef BR_MATCHING_BUILD
#undef BrEntSetPos
#endif

/* 0x10076420 */
/* WHAT IT DOES: move an entity to a position, writing the same three
 * coordinates into all the places the entity keeps them -- its matrix, its
 * cached position and two more copies. They are kept in step here rather
 * than derived, so all of them must be written. */
/* @implements 0x10076420 d3d BrEntSetPos */
/* @n64 0x8021FE04 located */
#ifdef BR_MATCHING_BUILD
/* Struct second arg is not register-eligible, so __fastcall is thiscall. */
typedef struct { float x, y, z; } BrEntSetPosArgs;
void BR_THISCALL1 BrEntSetPos(BrEnt *pE, BrEntSetPosArgs a)
{
    /* Store order is the original's: mat0.m[3], f26C8, st, stB, stA. */
    pE->mat0.m[3][0] = a.x;
    pE->mat0.m[3][1] = a.y;
    pE->mat0.m[3][2] = a.z;

    pE->f26C8[0] = a.x;
    pE->f26C8[1] = a.y;
    pE->f26C8[2] = a.z;

    pE->st.pos.x = a.x;
    pE->st.pos.y = a.y;
    pE->st.pos.z = a.z;

    pE->stB.pos.x = a.x;
    pE->stB.pos.y = a.y;
    pE->stB.pos.z = a.z;

    pE->stA.pos.x = a.x;
    pE->stA.pos.y = a.y;
    pE->stA.pos.z = a.z;

    BrRbBuildMatrix(&pE->matrix, &pE->st);
}
#else
void BrEntSetPos(BrEnt *pE, float x, float y, float z)
{
    /* Store order is the original's: mat0.m[3], f26C8, st, stB, stA. */
    pE->mat0.m[3][0] = x;
    pE->mat0.m[3][1] = y;
    pE->mat0.m[3][2] = z;

    pE->f26C8[0] = x;
    pE->f26C8[1] = y;
    pE->f26C8[2] = z;

    pE->st.pos.x = x;
    pE->st.pos.y = y;
    pE->st.pos.z = z;

    pE->stB.pos.x = x;
    pE->stB.pos.y = y;
    pE->stB.pos.z = z;

    pE->stA.pos.x = x;
    pE->stA.pos.y = y;
    pE->stA.pos.z = z;

    BrRbBuildMatrix(&pE->matrix, &pE->st);
}
#endif
