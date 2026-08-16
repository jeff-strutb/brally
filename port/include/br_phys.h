/* br_phys.h -- vehicle physics: how a wheel finds the ground.
 *
 * REFERENCE IS BRGlide.dll.  D3D addresses are given second because the rest
 * of this tree is keyed to them and because `config/shared.csv` classifies
 * two of these three as `d3d_only` -- WHICH IS WRONG, and is worth recording
 * because the same mistake has now been made repeatedly on this project:
 *
 *     0x1006F0C0  d3d_only in shared.csv   ->  really Glide 0x10068070
 *     0x1006F310  d3d_only in shared.csv   ->  really Glide 0x100682C0
 *     0x1006F4A0  shared                   ->       Glide 0x10068450
 *
 * The whole neighbourhood pairs on a constant D3D->Glide delta of 0x7050
 * (0x1006AEB0/0x10063E60, 0x1006C740/0x100656F0, 0x1006F4A0/0x10068450 ...)
 * and both "missing" functions sit exactly there, call the same callees and
 * are called by the same callers.  `d3d_only` is an upper bound on divergence
 * (CONVENTIONS.md says so); here it is simply two holes in the Glide map.
 *
 * ======================================================================
 * WHERE THIS SITS IN THE FRAME
 * ======================================================================
 * The live physics object is NOT `BrCarState` (slice1_02.h).  That 0xA0-byte
 * block is the NETWORK WIRE FORMAT.  The live state is a rigid body:
 *
 *   car + 0x164   BrRbBodyFull (slice3_42.h), 0x1DC bytes -- the CHASSIS
 *     +0x04..0x10   child[4]  -> the four WHEEL bodies
 *                   == slice3_40.h's BR_CAR_SUBPTR(pCar, i), i.e.
 *                      car+0x168 .. car+0x174.  Same four pointers.
 *     +0x18         pForces, head of a BrRbForce list
 *     +0x78         BrRbState (slice3_44.h): pos, vel, quat, angVel, qDot
 *                   == car+0x1DC, which slice3_40.h already calls a BrRbState
 *     +0xBC         BrMat4 m, body -> world, row-vector  == car+0x220
 *     +0xFC         accel      == car+0x260
 *     +0x108        angAccel   == car+0x26C
 *
 * Those four equalities are not inferred: 0x10068450 (below) reads
 * [body+4..body+0x10] as the four sub-object pointers with `body == car+0x164`,
 * which lands exactly on car+0x168..0x174.
 *
 * The per-frame step is 0x10061720 (D3D) / 0x1005A7A0 (Glide), 1206 bytes,
 * __thiscall on the car record.  It is NOT PORTED.  Its timestep is a
 * COMPILE-TIME CONSTANT pushed as the immediate 0x3D088889 at every
 * integrator call site -- 0.03333333507180214f, i.e. 1/30 s exactly as a
 * float.  Fixed step, no accumulator, no frame-time input.
 *
 * ======================================================================
 * THE CONTACT MODEL
 * ======================================================================
 * FOUR contact points, one per wheel body, and the "suspension" is geometric
 * before it is dynamic:
 *
 *   1. 0x1006F0C0 casts a ray from the wheel's mount point along the
 *      CHASSIS's own down axis and returns the distance to the nearest
 *      accepted triangle, or 100.0f.
 *   2. 0x1006F4A0 writes -that- into the wheel body: f1D8 gets the raw
 *      value and f78.z (the mount point's Z) is clamped into [-0.4, 0].
 *      So the wheel is DISPLACED along the body Z by the ground distance,
 *      with 0.4 units of travel.  Nothing else moves it.
 *   3. 0x1006F540 (not ported here) turns f1D8 into the spring force it
 *      writes into the chassis force list -- sign(v) * v * v * k, with a
 *      per-wheel contact counter at wheel+0x1B4.
 *
 * Because f78 is what BrRbAccumChildForces and BrRbVelAtBodyPointXY use as
 * the lever arm, moving f78.z IS the suspension.
 *
 * ======================================================================
 * NAMING / INTEGRATION DEBT, stated plainly
 * ======================================================================
 * 0x1006F310 ALREADY HAS A NAME AND A DECLARATION in this tree:
 *     slice2_12.h:49   extern float BrProbe1006F310(const float av3[3]);
 * and `port/host/br_stubs.c` DEFINES it as `long BrProbe1006F310(void)`.
 * Defining it here as well would be a duplicate symbol at the host link, and
 * port/host is not this module's to edit.  So the body lands under
 * `BrGroundProbeZ` and the integration step is two lines:
 *     - delete the BrProbe1006F310 stub from port/host/br_stubs.c;
 *     - add `float BrProbe1006F310(const float av3[3])
 *            { return BrGroundProbeZ((const BrVec3 *)(const void *)av3); }`
 * The stub is `long(void)` and the real function returns FLOAT in xmm0, so
 * every one of its seven call sites reads an uninitialised register today --
 * slice7_82.h already flags this as "ACTIVELY WRONG".  This module is the
 * body it was waiting for.
 *
 * Likewise 0x1006F4A0 is declared `void BrSub1006F4A0(void *pCar164)`
 * (slice3_40.h:187) and stubbed in br_stubs.c.  slice7_82.h declined it
 * because a `void *` cannot reach the four sub-objects.  It can now: the
 * argument IS a BrRbBodyFull and the sub-objects are its own child[] array.
 * Landed here as `BrWheelSuspensionSetZ`; same two-line integration.
 */
#ifndef BR_PHYS_H
#define BR_PHYS_H

#include "br_vec.h"
#include "br_mat.h"
#include "slice2_11.h"   /* BrCollPlane, g_pBrCollGrid, BrCollGridCellAcquire */
#include "slice3_42.h"   /* BrRbBodyFull -- the chassis and wheel bodies      */

/* Every constant below was read out of orig/BRGlide.dll, not assumed. */

/* 0x10077C60 / the immediate 0x42C80000.  The "no ground" answer, and also
 * the initial best-t.  A miss is NOT a sentinel the callers test -- they use
 * it as a distance, so a miss reads as "ground is 100 units away". */
#define BR_PHYS_PROBE_MISS   100.0f

/* The plane must be within +-2 of the probe point, and the hit must be within
 * +-2 along the ray.  0x10077BA8 / 0x10077BB0, and they are DOUBLES in the
 * original (`fcom qword ptr`), so the comparison happens in double. */
#define BR_PHYS_PROBE_NEAR   (-2.0)
#define BR_PHYS_PROBE_FAR    ( 2.0)

/* 0x10077BB8.  |dot(normal, direction)| must exceed this or the triangle is
 * treated as edge-on and skipped.  Double. */
#define BR_PHYS_PROBE_EPSDOT ( 0.001)

/* 0x10077BC0.  Only UPWARD-facing triangles are ground: n.z must exceed this.
 * Double.  A wall is invisible to the ground probe. */
#define BR_PHYS_PROBE_MINNZ  ( 0.2)

/* 0x10077BC8, and the same value again as the immediate 0xBECCCCCD in ebp --
 * the second sighting is what pins it.  Suspension travel. */
#define BR_PHYS_SUSP_MIN     (-0.400000006f)

/* The fixed timestep, pushed as the immediate 0x3D088889 at every integrator
 * call site in 0x1005A7A0.  Exposed here because it is the single most
 * load-bearing number in the whole subsystem and nothing else in the tree
 * names it. */
#define BR_PHYS_DT           0.03333333507180214f

/* ======================================================================
 * 0x1006F310 (D3D) / 0x100682C0 (Glide), 387 bytes
 *
 * Straight-down ground query at a WORLD point.  Returns the distance from
 * pPoint to the nearest accepted triangle measured along -Z, or
 * BR_PHYS_PROBE_MISS.
 *
 * A POSITIVE result means the ground is that far BELOW pPoint.  The result is
 * NOT clamped to positive: the accepted window is (-2, 2), so ground up to
 * two units ABOVE pPoint reports a negative distance.  It is a segment test,
 * not a ray cast, and callers that assume "downward only" are wrong.
 *
 * Acceptance, in the original's order:
 *   plane distance in (-2, 2);  |n . d| > 0.001;  t in (-2, 2);  t < best;
 *   n.z > 0.2;  and the hit point inside the triangle by the 2D test.
 *
 * The grid cell is chosen from (pPoint->x, pPoint->y) -- world coordinates,
 * which is what makes this the well-behaved sibling of the wheel probe below.
 * ====================================================================== */
float BrGroundProbeZ(const BrVec3 *pPoint);

/* ======================================================================
 * 0x1006F0C0 (D3D) / 0x10068070 (Glide), 581 bytes
 *
 * The same search, but the ray is the CHASSIS's own down axis rather than
 * world -Z, and the origin is a wheel's mount point:
 *
 *     P = (pWheel->f78.x, pWheel->f78.y, 0) * pBody->m     (body -> world)
 *     D = (0, 0, -1)                        * pBody->m     (rotation only)
 *
 * so a rolled car probes sideways-down, and the returned t is measured along
 * the CAR's down axis, not the world's.
 *
 * On a hit it records the winning triangle in the wheel body:
 *     +0x19C  the BrCollPlane *        (pWheel->f19C -- see the DEVIATION)
 *     +0x1A0  the surface byte (plane->flags, already masked with 7)
 *     +0x1A4..+0x1B0  a copy of the plane (nx, ny, nz, d)
 * +0x19C is cleared to 0 on entry, so it is also the "did I touch anything"
 * flag.  On a miss the other five fields keep last frame's values.
 *
 * DEVIATION (LP64), and it is forced, not chosen.  Those six fields cannot be
 * written through slice3_42.h's BrRbBodyFull: +0x19C is typed `float` there
 * (0x10074870 only ever clears it) but the original stores a POINTER, and
 * +0x1A0..+0x1B0 fall inside a byte pad whose offsets do not survive LP64
 * anyway -- the struct has five pointers ahead of them.  So the contact
 * record goes to a typed out-parameter, `BrGroundHit`, and the one field that
 * IS declared, f19C, carries 0.0f / 1.0f as the "did I touch anything" flag
 * the original gets from NULL / non-NULL.  Pass NULL for pHit to drop it.
 *
 * GOTCHA, and it looks like a genuine defect in the original -- reproduced:
 * the collision grid cell is looked up from pWheel->f78.x / .y DIRECTLY,
 * i.e. from the wheel's BODY-LOCAL mount offset, never from the world point
 * P that every subsequent test uses.  The two are unrelated once the car
 * leaves the origin.  The bytes are unambiguous: f78.x/.y are read into two
 * stack slots BEFORE the transform, the transform writes elsewhere, and those
 * same two slots are reloaded and pushed to 0x1006F720.  Recorded here rather
 * than "fixed", and asserted by test_br_phys so a future pass that decides it
 * is a misreading has something concrete to argue with.
 * ====================================================================== */
/* The six fields the original writes at wheel+0x19C..+0x1B0.  See the
 * DEVIATION above for why they are not written through BrRbBodyFull.
 * On a miss NOTHING here is written -- the original leaves the previous
 * frame's values in place and only clears the +0x19C slot. */
typedef struct BrGroundHit {
    const BrCollPlane *pPlane;   /* +0x19C in the original (a pointer)   */
    unsigned char      surface;  /* +0x1A0  plane->flags, already & 7    */
    float              nx;       /* +0x1A4                               */
    float              ny;       /* +0x1A8                               */
    float              nz;       /* +0x1AC                               */
    float              d;        /* +0x1B0                               */
} BrGroundHit;

float BrWheelGroundProbe(const BrRbBodyFull *pBody, BrRbBodyFull *pWheel,
                         BrGroundHit *pHit);

/* ======================================================================
 * 0x1006F4A0 (D3D) / 0x10068450 (Glide), 134 bytes
 *
 * For each of pBody->child[0..3]:
 *     v          = -BrWheelGroundProbe(pBody, child)
 *     child->f1D8 = v                              (unclamped)
 *     w          = (v > 0) ? 0 : v                 (clamp above at 0)
 *     child->f78.z = !(w >= -0.4) ? -0.4 : w       (clamp below at -0.4)
 *
 * Both clamps are written as negated comparisons because the original's
 * `fcom` + `test ah` takes the true side when the compare is UNORDERED: a
 * NaN keeps v at the first clamp and lands on -0.4 at the second.  Writing
 * them the tidy way inverts both.
 *
 * All four children are dereferenced unconditionally, exactly as the
 * original's four-arm jump table does.
 * ====================================================================== */
void BrWheelSuspensionSetZ(BrRbBodyFull *pBody);

#endif /* BR_PHYS_H */
