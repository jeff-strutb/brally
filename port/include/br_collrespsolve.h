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

#endif /* BR_COLLRESPSOLVE_H */
