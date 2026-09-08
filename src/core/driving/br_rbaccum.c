/* br_rbaccum.c -- driving: one frame's force accumulation for a rigid body.
 *
 * Filed out of the address batches (slice3_42.c, section 5).  BrRbSolveAccel
 * followed on 2026-09-07 and must stay LAST in this file (its byte-exact
 * spelling depends on end-of-TU placement).  BrRbAccumOwnForces and
 * BrRbAccumChildForces are still in that batch and are reached through
 * slice3_42.h, which declares all four.
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

/* 0x1006B170 */
/* WHAT IT DOES: turns all the pushes and twists that have been piled onto a
 * physical body this frame into how fast it is about to speed up and how fast
 * it is about to start spinning -- heavier bodies respond less, and the shape
 * of the body decides how readily it turns. The forces on the four bodies
 * attached to it are folded in too, but only sideways and forwards: the
 * up-and-down direction ignores them entirely, which looks like an oversight
 * in the original rather than an intention. */
/* Byte-exact 2026-09-07 (tools/crank.py): the three stores of the rotated
 * force are in x, y, z order and the function sits at the END of its TU --
 * in slice3_42.c the z, x, y order the old comment defended and its
 * mid-file position were the 14-byte residue. */
/* @implements 0x1006B170 d3d BrRbSolveAccel */
/* @n64 0x8025980C located */
void BrRbSolveAccel(BrRbBodyFull *pB)
{
    BrVec3 t, u, w;

    BrMat4MulVec3(&t, &pB->m, &pB->accel);
    pB->accel.x = t.x;
    pB->accel.y = t.y;
    pB->accel.z = t.z;

    /* orig x: fld child[3], fadd [2],[1],[0], fadd t.x.  y: fld child[0],
     * fadd [1],[2],[3], fadd t.y.  Z never sees the children.  Named child
     * locals spill six extra stack movs. */
    t.x = ((((pB->child[3]->accel.x + pB->child[2]->accel.x)
             + pB->child[1]->accel.x) + pB->child[0]->accel.x) + t.x)
          / pB->mass;
    t.y = ((((pB->child[0]->accel.y + pB->child[1]->accel.y)
             + pB->child[2]->accel.y) + pB->child[3]->accel.y) + t.y)
          / pB->mass;
    t.z = t.z / pB->mass;
    BrMat4MulVec3Transposed(&pB->accel, &pB->m, &t);
    BrMat4MulVec3(&u, &pB->m, &pB->angAccel);
    BrMat3MulVec3(&w, &pB->invInertia, &u);
    BrMat4MulVec3Transposed(&pB->angAccel, &pB->m, &w);
}
