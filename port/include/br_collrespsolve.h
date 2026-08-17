/* br_collrespsolve.h -- the OBB collision RESPONSE.
 *
 * REFERENCE IS orig/BRGlide.dll.  This module owns 0x10067710 (the response
 * walker) and its impulse solver 0x10065C80, plus the two helpers they drive
 * (0x10067470 the contact-plane resolver, 0x10065980).  They are kept in one
 * file because they communicate through a bank of file-scope globals
 * (0x117787F0..FC and 0x117781A0..A8), exactly as the original does -- see
 * br_collresp.h for how the broad phase feeds this consumer.
 *
 * Every function here is transcribed against the disassembly and pinned to
 * golden vectors produced by tools/x87emu.py executing the real opcode stream
 * (see test_br_collrespsolve.c).  The equivalence is per-function, not "it
 * links" and not "the car settles".
 */
#ifndef BR_COLLRESPSOLVE_H
#define BR_COLLRESPSOLVE_H

#include <stdint.h>

#include "br_vec.h"
#include "br_mat.h"       /* BrMat4                                          */
#include "slice3_44.h"    /* BrMat3 + the packed-3x3 helpers the solver drives */

/* The shared contact-plane state.  0x10067710 and its helpers hand geometry to
 * one another through these, not through arguments. */
typedef struct BrCrPlaneState {
    BrVec3   normal;   /* 0x117787F0 / F4 / F8 -- the contact normal.  In mode 2
                        *   the tail rewrites .x to ext.x*ext.y*.x; .y and .z
                        *   are left as the raw +-0.5. */
    uint32_t modeFC;   /* 0x117787FC -- READ AS INT at 0x10067478 to select the
                        *   mode (==2 is the box-face path), WRITTEN AS FLOAT in
                        *   the tail (scaled by ext.z).  One dword, two types:
                        *   load-bearing, do not split it. */
    BrVec3   out;      /* 0x117781A0 / A4 / A8 -- the plane normal scaled by the
                        *   signed plane distance (arg3 - dot(a, normal)). */
} BrCrPlaneState;

extern BrCrPlaneState g_brCrPlane;

/* 0x10067470 -- resolve one candidate contact plane and write g_brCrPlane.out.
 *
 * pA      the axis/normal being processed (a BrVec3).
 * planeD  the plane's signed offset.
 * pEdgeN  used ONLY in the != 2 mode, as the plane normal directly.
 * pExt    the box half-extents (body+0x1DC/0x1E0/0x1E4); read only in mode 2.
 * aVerts  three box-space triangle vertices; read only in mode 2.
 *
 * Mode is g_brCrPlane.modeFC (== 2 -> box face, else -> edge/explicit).  Both
 * modes finish by writing out = (planeD - dot(pA, V)) * pA, with V = the built
 * box normal (mode 2) or pEdgeN (otherwise).  Mode 2 additionally updates the
 * shared normal/modeFC state; the other mode leaves it untouched. */
void BrCrPlaneResolve(const BrVec3 *pExt, const BrVec3 *pA, float planeD,
                      const BrVec3 *pEdgeN, const BrVec3 aVerts[3]);

/* The impact "effect" record the solver stamps on a struck body (original
 * offsets body+0x1EC..0x200).  It is the damage/particle cue, not physics --
 * the impulse is applied whether or not it is written.  Field ORDER here is not
 * the original's byte order (that does not survive to a 64-bit host); these are
 * the fields, named. */
typedef struct BrCrEffect {
    uint8_t  threshold;   /* +0x200  IN  -- > 10 arms the damping/peak path   */
    uint8_t  intensity;   /* +0x1FC  OUT -- trunc(min(|approach|, 27))        */
    uint8_t  peak;        /* +0x1FF  IN/OUT -- max(peak, trunc(128 + k*inten))*/
    uint32_t color[3];    /* +0x1EC/1F0/1F4 OUT -- the contact normal's bits  */
} BrCrEffect;

/* 0x10065C80 -- the impulse solver.  THE function that stops a car falling
 * through the world: given one contact it computes and applies the collision
 * impulse to the body's `next` velocity and angular velocity.
 *
 * mass          body+0x2C.
 * pInvInertia   body+0x54, the body-frame inverse inertia (BrMat3).
 * pOrient       body+0xBC, the body->world orientation (BrMat4; only its 3x3).
 * pVel          body+0x164, next.vel     -- READ and WRITTEN.
 * pAngVel       body+0x180, next.angVel  -- READ and WRITTEN.
 * pNormal       arg2, the contact normal (== g_brCrPlane.normal); its dwords
 *               are ALSO copied verbatim into pEffect->color.
 * pRelDir       arg3, the world-space direction the approach is measured along.
 * flag          arg4; when non-zero a tangential term (0.2x) is folded into the
 *               solve's right-hand side, otherwise only the normal component is.
 * restOffset    arg5; the walker passes 0.  Adds to the restitution multiplier
 *               (which is then 1.05 = 1 + e) and, when >= 1e-4, SUPPRESSES the
 *               damping/peak path.  Its non-zero behaviour is transcribed but
 *               unexercised by the shipped caller.
 * pEffect       the impact record (see above); may be written even on a light
 *               hit (intensity/color always, peak only on the damping path).
 *
 * Returns 1 if an impulse was applied, 0 if the contact was separating
 * (approach speed >= 0) and nothing was touched. */
int BrCrImpulseSolve(float mass, const BrMat3 *pInvInertia, const BrMat4 *pOrient,
                     BrVec3 *pVel, BrVec3 *pAngVel,
                     const BrVec3 *pNormal, const BrVec3 *pRelDir,
                     int flag, float restOffset, BrCrEffect *pEffect);

/* 0x10065980 -- the contact "kick": the impulse-free branch of the response.
 * Where 0x10065C80 solves the full inertia contact, this one just reflects the
 * body's linear velocity off the contact plane with restitution and (optionally)
 * folds the spin onto the contact axis -- the cheaper path the walker takes when
 * the full solve is not wanted.
 *
 * pVel      body+0x164, next.vel     -- READ and WRITTEN (the reflection).
 * pAngVel   body+0x180, next.angVel  -- WRITTEN only when spinFlag is set.
 * pNormal   arg2, the contact normal; drives the gate and the reflection.
 * dampFlag  arg3; when non-zero, multiplies the reflected velocity by 0.9.
 * spinFlag  arg4; when non-zero, replaces angVel with M diag(Mt N) Mt angVel,
 *           where M's rows are (N; the quadratic tangent NxNy-Nz^2, ...; N x that)
 *           -- it isolates the spin about the contact normal.
 * pEffect   threshold IN; when threshold >= 10 the intensity/peak/colour are
 *           written and the reflected velocity is additionally damped 0.9.  The
 *           colour is g_brCrPlane.normal's dwords (the shared bank), NOT pNormal.
 *
 * Returns 1 if it acted, 0 if the contact was separating (dot(N, vel) >= 0). */
int BrCrContactKick(BrVec3 *pVel, BrVec3 *pAngVel, const BrVec3 *pNormal,
                    int dampFlag, int spinFlag, BrCrEffect *pEffect);

/* 0x10067710 -- the response walker (see the .c).  Walks the broad phase's
 * candidate list g_pBrCollRespList and resolves each surviving contact.
 *
 * mass/pInvInertia/pOrient   the chassis body (pOrient is REBUILT on a hit).
 * ext                        box half-extents body+0x1DC..0x1E4 plus the +0x1E8
 *                            z bias (ext[3]); drives the contact bank.
 * pNext                      the `next` rigid-body state; vel/angVel/pos/quat/qDot
 *                            are read and WRITTEN.
 * pSavePos                   the pre-substep position, for the push-out depth.
 * pQuatSrc                   the quaternion restored into next before the rebuild.
 * pEffect                    the impact record.
 * pMatBox                    the world->box matrix the vertices are tested in.
 *
 * The solver's "no torque" gate is orient.m[2][2] (body+0xE4 aliases it).
 * Returns the number of contacts that produced a response. */
int BrCrRespWalk(float mass, const BrMat3 *pInvInertia, BrMat4 *pOrient,
                 const float ext[4],
                 BrRbState *pNext, const BrVec3 *pSavePos, const BrVec3 *pQuatSrc,
                 BrCrEffect *pEffect, const BrMat4 *pMatBox);

#endif /* BR_COLLRESPSOLVE_H */
