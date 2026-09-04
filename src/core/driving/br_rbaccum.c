/* br_rbaccum.c -- driving: one frame's force accumulation for a rigid body.
 *
 * Filed out of the address batches (slice3_42.c, section 5).  The helpers it
 * drives -- BrRbAccumOwnForces, BrRbAccumChildForces and BrRbSolveAccel --
 * are still in that batch and are reached through slice3_42.h, which declares
 * all four.
 */

#include "slice3_42.h"

/* 0x1006B260 */
/* WHAT IT DOES: one whole physics pass for a body: wipe last frame's answer,
 * add up everything pushing on the body itself and on the four bodies
 * attached to it, and work out the resulting acceleration and spin. This is
 * the step that decides how a car moves this frame. The attached bodies' spin
 * is deliberately not wiped, so theirs carries over from last frame. */
/* @implements 0x1006B260 d3d BrRbAccumAll */
/* @n64 0x8025993C located */
void BrRbAccumAll(BrRbBodyFull *pB)
{
    pB->accel.x = 0.0f;
    pB->accel.y = 0.0f;
    pB->accel.z = 0.0f;
    pB->angAccel.x = 0.0f;
    pB->angAccel.y = 0.0f;
    pB->angAccel.z = 0.0f;

    /* orig reloads pB->child[k] for every store (`mov r,[esi+off]; mov
     * [r+0xfc],eax`) and unrolls both the zeros and the four child-force
     * calls. A counted loop is the extra dec/jne in the bag. */
    pB->child[0]->accel.x = 0.0f;
    pB->child[0]->accel.y = 0.0f;
    pB->child[0]->accel.z = 0.0f;
    pB->child[1]->accel.x = 0.0f;
    pB->child[1]->accel.y = 0.0f;
    pB->child[1]->accel.z = 0.0f;
    pB->child[2]->accel.x = 0.0f;
    pB->child[2]->accel.y = 0.0f;
    pB->child[2]->accel.z = 0.0f;
    pB->child[3]->accel.x = 0.0f;
    pB->child[3]->accel.y = 0.0f;
    pB->child[3]->accel.z = 0.0f;

    BrRbAccumOwnForces(pB);
    BrRbAccumChildForces(pB, pB->child[0]);
    BrRbAccumChildForces(pB, pB->child[1]);
    BrRbAccumChildForces(pB, pB->child[2]);
    BrRbAccumChildForces(pB, pB->child[3]);
    BrRbSolveAccel(pB);
}
