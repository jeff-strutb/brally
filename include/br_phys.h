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
 * The grid cell is chosen from P -- the WORLD point -- exactly as in the
 * straight-down sibling.  See THE GRID-KEY ADJUDICATION below; this was read
 * as a body-local key by two passes and it is not one.
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
 * THE GRID-KEY ADJUDICATION.  IT WAS A MISREADING.
 * ======================================================================
 * This header, br_phys.c, test_br_phys.c and CONVENTIONS.md all used to state
 * as established fact that 0x10068070 keys the collision grid on the wheel's
 * BODY-LOCAL mount offset, and that the shipped wheel probe therefore can
 * never find ground on any track.  Two independent passes reached that
 * conclusion.  Both were wrong, in the same way, and the way is worth keeping.
 *
 * The claim rested on this pair of instructions:
 *
 *     10068091  mov  dword ptr [esp+0x18], eax     ; <- f78.x spilled
 *     10068099  mov  dword ptr [esp+0x1c], ecx     ; <- f78.y spilled
 *     ...
 *     100680DC  mov  ecx, dword ptr [esp+0x1c]     ; <- "the same slots"
 *     100680E0  mov  edx, dword ptr [esp+0x18]     ;    reloaded
 *     100680F1  push ecx
 *     100680F2  push edx
 *     100680F3  call 0x100686D0                    ; BrCollGridCellAcquire
 *
 * Identical displacements, so identical slots -- except that ESP IS NOT THE
 * SAME AT THE TWO POINTS.  Take R as esp on entry (pointing at the return
 * address) and walk it:
 *
 *     10068070  sub  esp,0x34        esp = R-0x34
 *     10068077  push esi             esp = R-0x38
 *     10068078  push edi             esp = R-0x3C
 *     10068091  [esp+0x18] -> R-0x24     f78.x     } the MOUNT vector,
 *     10068099  [esp+0x1c] -> R-0x20     f78.y     } R-0x24 .. R-0x1C
 *     1006809D  push eax             esp = R-0x40   (arg3 = &mount)
 *     1006809E  lea  ecx,[esp+0x10] -> R-0x30       (arg1 = &world)
 *     100680A2  push esi             esp = R-0x44   (arg2 = &body->m)
 *     100680A3  push ecx             esp = R-0x48
 *     100680BC  [esp+0x2c] -> R-0x1C = mount.z = 0  -- pins the mount vector
 *     100680C4  call 0x1006DA20      writes WORLD at R-0x30/-0x2C/-0x28
 *     100680C9  add  esp,0xc         esp = R-0x3C
 *     100680D0  lea  eax,[esp+0x24] -> R-0x18       (arg1 = &dir)
 *     100680D4  push edx / push esi / push eax      esp = R-0x48
 *     100680D7  call 0x1006D9D0      writes DIR at R-0x18/-0x14/-0x10
 *     100680DC  [esp+0x1c] -> R-0x2C = WORLD.y      <-- esp is STILL R-0x48
 *     100680E0  [esp+0x18] -> R-0x30 = WORLD.x
 *     100680E4  add  esp,0xc         esp = R-0x3C   <-- the cleanup is HERE
 *
 * The cleanup for the second transform's three arguments happens AFTER the
 * two reloads, not before, so at 0x100680DC esp is 0xC lower than it was at
 * the spill and `[esp+0x18]`/`[esp+0x1c]` name R-0x30/R-0x2C -- which is
 * 0x1006DA20's OUTPUT, i.e. the world point.  The wheel probe keys the grid
 * on the world point.  There is no defect.
 *
 * FOUR independent confirmations, none of which relies on the frame walk:
 *
 *  1. THE CALLEES ARE CDECL.  0x1006DA20, 0x1006D9D0 and 0x100686D0 all end
 *     in a bare `ret`, never `ret 0xC`.  Callee-cleanup was the only way the
 *     old reading could hold, and it would additionally make the explicit
 *     `add esp,0xc` at 0x100680E4 corrupt the frame.
 *  2. THE MOUNT SLOTS ARE REUSED, so "the same slots are reloaded" could not
 *     have been true anyway.  At 0x10068206/0x1006820C/0x10068212 the hit
 *     point is stored to R-0x24/R-0x20/R-0x1C -- exactly the three words the
 *     mount vector occupied -- and 0x1006824F passes R-0x24 to the 2D
 *     containment test.  The mount vector is dead by then.
 *  3. EVERY OTHER READ AGREES.  R-0x30/-0x2C/-0x28 is passed to BrPlaneEval
 *     (0x10068128), dotted with the normal (0x1006819B) and added to t*D to
 *     build the hit point (0x100681F4).  R-0x18/-0x14/-0x10 is the direction
 *     (0x10068159).  One consistent labelling covers the whole function.
 *  4. THE D3D BUILD IS THE SAME FUNCTION.  0x1006F0C0 is byte-identical to
 *     0x10068070 apart from relocations (callees 0x100747C0 / 0x10074770 /
 *     0x1006F720, tables 0x11750338 / 0x117554A0).  Same displacements, same
 *     `add esp,0xc` after the same two reloads.  The brief's decisive test --
 *     "if one build has the bug and the other does not" -- returns "neither".
 *
 * WHAT THE OLD CLAIM PREDICTED, AND WHY NOBODY CAUGHT IT: it predicted a
 * total failure of wheel ground contact on every track, which the shipped
 * game plainly does not exhibit.  That contradiction was noticed and then
 * explained away ("the contact must live in 0x10067C30's unported callees")
 * instead of being treated as a refutation.  A reading that requires the
 * shipped game to be broken needs more evidence than a reading that does not,
 * and it had less: it had one displacement match, taken at face value.
 *
 * LESSON, and it generalises past this function: a stack displacement is
 * meaningless without the ESP it is relative to.  When two `[esp+N]` with the
 * same N are claimed to be the same slot, walk every push, pop, `sub esp` and
 * `add esp` between them -- and check whether the callees in between are
 * cdecl or stdcall, because that decides where the cleanup happens.
 * ======================================================================
 *
 * VESTIGIAL.  This was the switch that selected between the two readings so
 * the "defect" could be measured.  There is one reading now and this changes
 * nothing.  It survives only so port/host/brally.c -- which parses
 * `-worldkey` and prints a COUNTERFACTUAL banner when it is set -- keeps
 * compiling; that flag and its banner should be deleted by whoever owns that
 * file, and this declaration with them. */
extern int g_brPhysWheelGridWorldKey;

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

/* The same function with the per-wheel contact record kept.  The original
 * writes it at wheel+0x1A0..+0x1B0 and 0x10067F30 (the aerodynamic drag pass,
 * br_carphys.c) reads the surface byte of all four wheels; the DEVIATION
 * above moved those fields to BrGroundHit, so somebody has to own the array.
 * BrWheelSuspensionSetZ is this with aHit == NULL. */
void BrWheelSuspensionSetZHit(BrRbBodyFull *pBody, BrGroundHit aHit[4]);

#endif /* BR_PHYS_H */
