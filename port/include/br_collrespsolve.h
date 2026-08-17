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

#endif /* BR_COLLRESPSOLVE_H */
