#include <stddef.h>
/* br_carphys.c -- 0x1005A7A0 and the four force generators it drives.
 *
 * Transcribed from orig/BRGlide.dll:
 *
 *   0x1005A7A0  1206 B   BrCarPhysStep      the frame
 *   0x100684F0   265 B   BrCarPhysSpring    the suspension spring
 *   0x10068600   200 B   BrCarPhysDamper    the shock absorber
 *   0x10067F30   305 B   BrCarPhysDrag      aerodynamic drag
 *   0x10067C30   762 B   BrCarPhysAdvance   the four-substep position step
 *
 * 0x10067C30's own collision callees live in br_collresp.c; read that file's
 * header before changing BrCarPhysAdvance, because the substep loop's shape
 * (in particular the conditional BrRbQuatDerivative + BrRbBuildMatrix pair at
 * 0x10067DA7) is load-bearing and was missing here for three passes.
 *
 * plus the car constructor's rigid-body half, D3D 0x10062C50 / 0x10063000
 * (the Glide twins are in the same packet and were read for the offsets; the
 * IMMEDIATES quoted below were read out of BRD3D.dll because that is the
 * build whose disassembly of the constructor was already in work/).
 *
 * See br_carphys.h for the contracts, the ten steps, the sign-change damper,
 * the dead duplicate call and the three holes.
 *
 * ON THE COMPARISONS.  Same rule as br_phys.c and for the same reason: every
 * clamp below is the exact negation the x87 flag test implies.
 *     test ah,0x41 (C0|C3) taken -> "less, equal or unordered"
 *     test ah,1    (C0)    taken -> "less or unordered"
 *     test ah,0x40 (C3)    taken -> "equal or unordered"
 * An integrator is nothing but clamps, and one of them backwards inverts the
 * whole thing silently.
 */
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#define BrCarPhysStep BrCarPhysStep_port   /* ditto: the original is __fastcall (pCar in ecx) */
#define BrCarPhysAdvance BrCarPhysAdvance_port /* ditto: the original is 2-arg */
#define BrCrRespWalk BrCrRespWalk    /* ditto: the original is 2-arg */
#define BrCollRespTipKick BrCollRespTipKick_portproto /* ditto: original is 1-arg */
#include "br_vec.h"   /* br_globals: its objects */
#include "slice1_08.h"   /* br_globals: its objects */
#include <float.h>
#include "br_cartypes.h"
#include "slice3_41.h"
#include <math.h>
#include <string.h>

#include "br_carphys.h"
#include "br_collresp.h"
#include "br_collrespsolve.h"
/* initialised in the original source (restored: the 64-bit core keeps them
 * private to this file; no Glide relocation places them elsewhere) */
const float *g_pBrCarPhysDrvT1 = g_aBrCarPhysDrvT1A;
const float *g_pBrCarPhysDrvT2 = g_aBrCarPhysDrvT2A;

/* ==================================================================== */
/* The holes                                                             */
/* ==================================================================== */

BrCarPhysHooks g_brCarPhysHooks;
uint32_t       g_aBrCarPhysHole[BR_CP_HOLE_COUNT];

static const char *const g_aBrCarPhysHoleName[BR_CP_HOLE_COUNT] = {
    "0x10067710 OBB response (+0x10065C80)",
    "0x10068F80 car vs car  (1444 B)"
};

/* (port-only BrCarPhysHoleReset removed) */


/* (port-only BrCarPhysHoleName removed) */


/* ==================================================================== */
/* Two models of one object, bridged rather than cast                    */
/*                                                                       */
/* slice3_44.h's BrRbBody and slice3_42.h's BrRbBodyFull describe the SAME */
/* original 0x1DC-byte object.  BrRbBodyFull has five pointers in it, so   */
/* under LP64 `accel` sits at a different HOST offset in the two, and a    */
/* cast between them is exactly the "two models of one object" bug         */
/* CONVENTIONS.md records.  These two adapters copy the fields the callee  */
/* reads or writes instead.  They are the price of not merging the two     */
/* headers, which is not this module's to do.                              */
/* ==================================================================== */

/* @n64 0x80225EB8 located */
/* (port-only BrCarPhysBodyState removed) */


/* 0x1006D600 == D3D 0x100743A0 == BrRbIntegrateVelocity, which reads only
 * pBody->accel and pBody->angAccel. */
/* WHAT IT DOES: adds one time step's worth of acceleration to a car's speed
 * and spin. This is a thin adapter: the actual work lives with the rigid-
 * body physics, and the copying here exists only because two modules model
 * the car's body differently. */
/* @implements 0x1006D600 glide BrCpIntegrateVelocity */
static void BrCpIntegrateVelocity(BrRbState *pS, const BrRbBodyFull *pB,
                                  float dt)
{
    /* Orig loads all six body floats onto the x87 stack, then fxch/fmul
     * dt, and homes exactly THREE of the products -- the angular ones -- in
     * three consecutive frame slots (`sub esp,0xc`; `fst [esp]`, `[esp+4]`,
     * `[esp+8]`).  Consecutive homes for a set of values that are also
     * kept on the x87 stack is an AGGREGATE local: the deltas are two
     * BrVec3s.  Byte-exact 2026-09-04.  Six named float locals put the
     * three homes in the dead dt slot instead (no frame, -6 B), inline
     * products lose the load hoist entirely (-38 B), one aggregate plus
     * three named scalars is 2 B off, one aggregate plus inline vel is
     * -14 B, and a float[3] behaves like the single aggregate. */
    {
        BrVec3 dv, da;
        dv.x = pB->accel.x * dt;
        dv.y = pB->accel.y * dt;
        dv.z = pB->accel.z * dt;
        da.x = pB->angAccel.x * dt;
        da.y = pB->angAccel.y * dt;
        da.z = pB->angAccel.z * dt;
        pS->vel.x    = pS->vel.x + dv.x;
        pS->vel.y    = pS->vel.y + dv.y;
        pS->vel.z    = pS->vel.z + dv.z;
        pS->angVel.x = pS->angVel.x + da.x;
        pS->angVel.y = pS->angVel.y + da.y;
        pS->angVel.z = pS->angVel.z + da.z;
    }
}

/* 0x10074870 == BrRbInitInertia, which reads mode/dim/mass and writes the two
 * 3x3 tensors plus a dozen scalars. */
/* (port-only BrCpInitInertia removed) */


/* ==================================================================== */
/* 0x100684F0 -- the suspension spring                                   */
/* ==================================================================== */

/* __inline: the original open-codes this three-way classify at every call
 * site (it is not a standalone function in the binary), keeping the result on
 * the x87 stack.  Marking it inline lets VC5 do the same in BrCarPhysStep's
 * damper and every other caller, instead of a call that spills to memory. */
/* (port-only BrCarPhysSign removed) */


/* WHAT IT DOES: pushes each wheel up with its suspension spring -- the
 * further the wheel is compressed past its rest point, the harder the push,
 * growing with the square of the compression. A wheel that has left the
 * ground contributes nothing, and the first frame a wheel touches back down
 * sets the car's touchdown flag so other systems can react. */
/* The spring's constants are the original's .rdata cells.  Named, so the
 * transcription resolves at the original's addresses -- and the contact
 * floor is the original's qword, -0.3999 ROUNDED THROUGH FLOAT
 * (-0.39990001341...), which the double literal is not. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* -0.3999f as a double */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* -0.3f */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /*  0.0f */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /*  1.0f */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* -1.0f */
/* @t4-pass 0x100684F0 1 2026-09-24 probes 10 bytes 265 insns 87 regions 1 rows 0 census no  (hand, after the in-place square reached 265/265: five in-place scale spellings and five forms keeping s live past the f1B8 multiply; the dead-s pop order never moved) */
/* @t4-pass 0x100684F0 2 2026-09-24 probes 34 bytes 265 insns 87 regions 1 rows 0 census yes  (hand, slot census: every top-level slot of br_carphys.c; residue identical in all 34) */
/* @t3 0x100684F0 2026-09-24 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 265/265 insns 87/87 rows 0+0 regions 1 oracle EQUIVALENT
 * @t3-effort passes 2 zero-movement 1 2
 * residue is one x87 pop order: the dead sign `s` is popped before the
 * f1B8 multiply instead of after it (same instructions, same size).  Dead
 * list in the body comment.
 * Do not reopen before the end-grind. */
/* @implements 0x100684F0 glide BrCarPhysSpring */
void BrCarPhysSpring(BrRbBodyFull *pBody)
{
    BrRbForce *pNode;
    int        i;

    pNode = pBody->pForces;
    for (i = 0; i < 4; ++i) {
        BrRbBodyFull  *pWheel;
        float          v;
        float          s;
        int32_t        contact;
        unsigned short wasLo;

        pNode->f.y = 0.0f;
        pNode->f.x = 0.0f;

        switch (i) {
        case 0:  pWheel = pBody->child[0]; break;
        case 1:  pWheel = pBody->child[1]; break;
        case 2:  pWheel = pBody->child[2]; break;
        default: pWheel = pBody->child[3]; break;
        }

        /* f1B4 is an int32 here (`mov eax` / `inc` / integer store) and its
         * low WORD is snapshotted before the increment for the touchdown
         * edge below. */
        contact = *(int32_t *)&pWheel->f1B4;
        wasLo   = *(unsigned short *)&pWheel->f1B4;
        v       = pWheel->f1D8;

        if (contact < BR_CP_CONTACT_MAX) {
            *(int32_t *)&pWheel->f1B4 = contact + 1;
        }

        /* `fcom qword` -- the compare happens in double. Runs for
         * less-equal-or-unordered. */
        if (!((double)v > DAT_10077bd0)) {
            v                        = DAT_10077bd8;
            *(int32_t *)&pWheel->f1B4 = 0;
        }
        if (v > BrCrK_Zero) {
            v = BrCrK_Zero;
        }
        v = v - DAT_10077bd8;
        if (!(v >= BrCrK_Zero)) {
            v = BrCrK_Zero;
        }

        /* The sign triple, inline: `test ah,0x40` for the equal arm (VC5
         * float == reads C3 alone, so NaN lands there too), then
         * `test ah,0x41` for strictly-greater. */
        if (v == BrCrK_Zero) {
            s = BrCrK_Zero;
        } else if (v > BrCrK_Zero) {
            s = DAT_10077a7c;
        } else {
            s = DAT_10077a80;
        }

        /* PARKED at 1 region / 5 msetdiff rows: the square. Orig duplicates
         * v and `fmulp st(2)` DESTRUCTIVELY into v's slot (then fxch to
         * bring it up); ours keeps v alive (`fmul st(2)`) and drops both v
         * and s at the end. One x87 pop-discipline decision; corpus MISS on
         * the 12-insn window at +0xcf. Dead (all 53 diffs, same region):
         * s*(v*v)*k, v*v*s*k (60, drops the dup entirely), s*v*v*k (60),
         * named `float vv = v*v` (identical -- folded), `v = v*v;` own
         * statement (identical -- single use, copy-propagated back).
         * Everything before +0xcf and after +0xdf is byte-exact. */
        /* DEAD 2026-09-13: `v *= v` and `v = v * v` before the product,
         * with s first or v first, and the pair scoped in an inner block
         * with the store outside -- all byte-identical (v stays live). */
        /* 2026-09-24: squared IN PLACE, then scaled in place -- the
         * original's `fld st(1); fmulp st(2)` is `v *= v` on v's own
         * register.  Residue: the dead `s` is popped before the f1B8
         * multiply instead of after (same size, regnorm 0+0). */
        v *= v;
        v *= s;
        pNode->f.z = v * pBody->f1B8;

        /* Touchdown edge: f1B4 is RE-READ (a wheel just reset above does not
         * trip it), the node advances between the read and the test, and the
         * flag byte lands at body+0x208 -- pCar->b208, the body being at
         * car+0x164. */
        contact = *(int32_t *)&pWheel->f1B4;
        pNode   = pNode->pNext;
        if (contact != 0 && wasLo == 0) {
            ((unsigned char *)pBody)[0x208] = (unsigned char)BR_CP_TOUCHDOWN;
        }
    }
}

/* ==================================================================== */
/* 0x10068600 -- the shock absorber                                      */
/* ==================================================================== */

/* WHAT IT DOES: the shock absorber -- resists each wheel's upward motion
 * with a force proportional to how fast the wheel is moving up. A wheel
 * moving down, or one that has left the ground, gets no damping at all. */
/* @implements 0x10068600 glide BrCarPhysDamper */
void BrCarPhysDamper(BrRbBodyFull *pBody)
{
    BrRbForce *pNode;
    int        i;

    pNode = pBody->pForces;
    for (i = 0; i < 4; ++i) {
        BrRbBodyFull *pWheel;
        BrVec3        v;
        float         f;

        pNode->f.y = 0.0f;
        pNode->f.x = 0.0f;

        /* The velocity call and the wheel pointer are IN each switch arm --
         * the original re-reads child[k] after the call rather than keeping
         * it across it. */
        switch (i) {
        case 0:
            BrRbVelAtBodyPointXY(&v, pBody, pBody->child[0]);
            pWheel = pBody->child[0];
            break;
        case 1:
            BrRbVelAtBodyPointXY(&v, pBody, pBody->child[1]);
            pWheel = pBody->child[1];
            break;
        case 2:
            BrRbVelAtBodyPointXY(&v, pBody, pBody->child[2]);
            pWheel = pBody->child[2];
            break;
        default:
            BrRbVelAtBodyPointXY(&v, pBody, pBody->child[3]);
            pWheel = pBody->child[3];
            break;
        }

        if (*(int32_t *)&pWheel->f1B4 == 0) {
            f = 0.0f;
        } else if (!(v.z >= 0.0f)) {
            /* `fcomp` + `test ah,1` + `je` jumps to the multiply, so the
             * ZERO arm is LESS OR UNORDERED. */
            f = 0.0f;
        } else {
            f = pBody->f1BC * v.z;
        }

        pNode->f.z = f;
        pNode = pNode->pNext;
    }
}

/* ==================================================================== */
/* 0x100651A0 -- the per-wheel tyre pass                                 */
/*                                                                       */
/* See br_carphys.h for what this is and, more importantly, for what it   */
/* is NOT: there is no slip angle, no lateral force and no load transfer  */
/* anywhere in these 1355 bytes.                                         */
/* ==================================================================== */

/* Every `fabs` in 0x100651A0 is spelled as `fcom 0` + `test ah,1` + a
 * conditional `fchs`, i.e. the sign is flipped for LESS OR UNORDERED.  So a
 * NaN comes back NEGATED, not absolute.  fabsf() clears the sign bit and is
 * therefore the wrong function here. */
/* (port-only BrCpAbsX87 removed) */


/* 0x100B4F30 and 0x100B5050, 288 bytes each, read out of BRGlide.dll.
 * [24*compound + 8*weather + surface]. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* See the DEVIATION in br_carphys.h: the original's 0x11773690 is .bss and
 * starts NULL. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* 64-bit core: declared once, in br_globals.h or its struct's header */          /* 0x104B15E8, .bss, starts 0 */

/* (port-only BrCarPhysSelectCar removed) */


/* 0x1006543F..0x10065486 and the identical pair inside 0x100645A0.  The
 * weather index is `n - 1` clamped into [0, 2] by SIXTEEN-BIT signed
 * compares, so 0x10001 answers row 0 and not row 0x10000. */
/* (port-only BrCpWeatherRow removed) */


/* Constants shared with the other matching arms; the tyre arm itself sits
 * after the port's drive helpers (see its PLACEMENT note). */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0.0f   */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 1.0f   */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* -1.0f  */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* -4.0f  */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0.7    */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 2.943  */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 3.5f   */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* -0.5f  */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0.1f   */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0.4f   */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 300.0f */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 57.29578f */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* -36000.0 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 36000.0 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 360.0  */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0.0    */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* -360.0f */
/* BrCosF: prototype in br_funcs.h */
/* BrSinF: prototype in br_funcs.h */

/* (port-only BrCarPhysTyre removed) */


/* ==================================================================== */
/* 0x100645A0 -- the drivetrain, i.e. the axle velocity constraint       */
/*                                                                       */
/* See br_carphys.h.  The short version: this is what stops the car       */
/* spinning, and it does it by OVERWRITING body->vel and body->angVel,    */
/* not by adding a force.                                                */
/* ==================================================================== */

/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* 0x11778808 / 0x11778820.  Same DEVIATION as g_pBrCarPhysGrip: .bss in the
 * original, aimed here at the set 0x10069530 gives car index 0. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* 0x100649D4..0x10064A4B and the identical block at 0x10064EC6..0x10064F21,
 * then 0x10064A4F..0x10064B38 / 0x10064F25..0x10064FE8.
 *
 * The surface index is the MEAN of the pair's two surface bytes, formed as
 * `(s0 + s1 + 1) >> 1` on zero-extended bytes -- so it rounds up, and a pair
 * straddling surfaces 3 and 4 answers 4.
 *
 * Returns the fraction of the axle's lateral velocity to remove. */
/* (port-only BrCpDrvSlip removed) */


/* 0x10064603..0x100647EE, once per axle.  The axle's brake contribution, in
 * the same units as the axle velocities below. */
/* (port-only BrCpDrvBrake removed) */


/* Declared here, ahead of the ground probe: the probe and the tyre pass are
 * scheduled against the TU's symbol table, and these five names (with the
 * drag pass's literals, now at the end of the file in address order) are
 * what they were matched against. */
/* BrSqrtF: prototype in br_funcs.h */
/* 64-bit core: declared once, in br_globals.h or its struct's header */             /* 0.0f  the sign triple          */
/* 64-bit core: declared once, in br_globals.h or its struct's header */             /* 1.0f                           */
/* 64-bit core: declared once, in br_globals.h or its struct's header */             /* -1.0f                          */

/* 0x10068070 */
/* Transcribed from the Glide bytes: the body matrix is pBody+0xBC; the
 * contact is cleared (wheel+0x19C = 0) before the search and set to the
 * plane pointer on a hit; the hit point reuses the mount's slots; the drop
 * h is kept in memory (stored once, reloaded for every use).
 * Byte-exact only in this TU and at this position (after the drive
 * helpers, ahead of the tyre model): in its own file the product operand
 * roles of both dot products came out reversed, and the pad-count probe
 * could not reach the state. The vectors are declared ahead of the rest. */
/* WHAT IT DOES: finds how far one wheel can drop before it meets the ground.
 * The wheel's mount point (its x and y offset, height ignored) is taken into
 * the world by the car body's matrix and the car's own down axis is rotated
 * the same way; then the collision-grid cell under that point is searched
 * exactly as BrGroundProbeZ searches it, along that axis instead of straight
 * down. On a hit the wheel records the plane, its surface byte and the
 * plane's normal and constant. Returns the shortest accepted drop, or 100. */
/* @implements 0x10068070 glide BrWheelGroundProbe */
float BrWheelGroundProbe(BrCarBody *pBody, BrCarBody *pWheel)
{
    /* 64-bit core: declared once, in br_globals.h or its struct's header */
    /* 64-bit core: declared once, in br_globals.h or its struct's header */
/* BrCollGridCellAcquire: prototype in br_funcs.h */
/* BrCrPlaneDist: prototype in br_funcs.h */
/* FUN_100656F0: prototype in br_funcs.h */
/* 64-bit core: declared by the platform headers */

/* 64-bit core: declared by the platform headers */

    float world[3], dir[3], mount[3], down[3];
    const float *pPl;
    float best, t, h;
    int n;
    short cell;

    best = 100.0f;
    mount[0] = pWheel->rb.st.pos.x;
    mount[1] = pWheel->rb.st.pos.y;
    down[0] = 0.0f;
    down[1] = 0.0f;
    down[2] = -1.0f;
    mount[2] = 0.0f;
    BrMat4TransformPoint(world, &pBody->rb.m.m[0], mount);
    BrMat4MulVec3Transposed(dir, &pBody->rb.m.m[0], down);
    pWheel->rb.pPlane = 0;
    cell = BrCollGridCellAcquire(world[0], world[1]);
    /* the cell's plane records, one BrCollPlane each (0x20 bytes in the
     * original, so `pPl += 8` floats there); counts are words at 0x11778800 */
    pPl = (const float *)&DAT_11773698[cell][0];
    for (n = g_brCrPlane.aCellCount[cell]; n > 0;
         n--, pPl = (const float *)((const BrCollPlane *)pPl + 1)) {
        float d = BrCrPlaneDist(pPl, pPl[3], world);

        if (!(d > -2.0) || !(d < 2.0))
            continue;
        t = (dir[1] * pPl[1] + dir[2] * pPl[2]) + dir[0] * pPl[0];
        if (!((t < 0.0f ? -t : t) > 0.001))
            continue;
        h = -(((world[1] * pPl[1] + world[2] * pPl[2]) + world[0] * pPl[0]
               + pPl[3]) / t);
        mount[0] = dir[0] * h;
        mount[1] = dir[1] * h;
        mount[2] = dir[2] * h;
        mount[0] += world[0];
        mount[1] += world[1];
        mount[2] += world[2];
        if (h > -2.0 && h < 2.0 && h < best && pPl[2] > 0.2
            && FUN_100656F0(pPl, mount)) {
            best = h;
            pWheel->rb.pPlane = (struct BrCollPlane *)pPl;
            pWheel->rb.f01A0 = ((const BrCollPlane *)pPl)->flags;
            pWheel->rb.f01A4 = pPl[0];
            pWheel->rb.f01A8 = pPl[1];
            pWheel->rb.f01AC = pPl[2];
            pWheel->rb.f01B0 = pPl[3];
        }
    }
    return best;
}


#undef BrCarPhysTyre
/* The wheel body as 0x100651A0 reads it: the hit record lives in the WHEEL at
 * +0x19C (plane pointer, surface byte, normal) and the spin state follows. */
typedef struct BrTyreView {
    /* BrRbBody's layout, member for member (asserted below), with the
     * ground hit and the contact count named as the tyre model reads them.
     * The i386 offsets are in the comments; a padded view at those offsets
     * misplaces everything after child[] in a 64-bit build. */
    float               f00;              /* 0x000                          */
    struct BrTyreView  *child[4];         /* 0x004                          */
    float               f14;              /* 0x014                          */
    BrRbForce          *pForces;          /* 0x018                          */
    int                 mode;             /* 0x01C                          */
    float               dim[3];           /* 0x020                          */
    float               mass;             /* 0x02C                          */
    BrMat3              inertia;          /* 0x030                          */
    BrMat3              invInertia;       /* 0x054                          */
    BrRbState           st;               /* 0x078                          */
    BrMat4              m;                /* 0x0BC                          */
    BrVec3              accel;            /* 0x0FC                          */
    BrVec3              angAccel;         /* 0x108                          */
    BrRbState           st1;              /* 0x114                          */
    BrRbState           st2;              /* 0x158                          */
    BrGroundHit         hit;              /* 0x19C  plane, surface, normal  */
    int32_t             f1B4;             /* 0x1B4  contact count           */
    float               f1B8;             /* 0x1B8                          */
    float               f1BC;             /* 0x1BC                          */
    float               f1C0;             /* 0x1C0  steer angle             */
    float               f1C4;             /* 0x1C4  spin                    */
    float               f1C8;             /* 0x1C8  radius                  */
    float               f1CC;             /* 0x1CC  drive torque            */
    float               f1D0;             /* 0x1D0                          */
    float               f1D4;             /* 0x1D4  display angle, degrees  */
    float               f1D8;             /* 0x1D8                          */
} BrTyreView;
_Static_assert(sizeof(BrTyreView) == sizeof(BrRbBody), "BrTyreView is BrRbBody");
_Static_assert(offsetof(BrTyreView, hit) == offsetof(BrRbBody, pPlane), "hit");
_Static_assert(offsetof(BrTyreView, m) == offsetof(BrRbBody, m), "m");
_Static_assert(offsetof(BrTyreView, f1B4) == offsetof(BrRbBody, f1B4), "f1B4");
_Static_assert(offsetof(BrTyreView, f1D4) == offsetof(BrRbBody, f1D4), "f1D4");







/* `fcom 0` + `test ah,1` + a conditional `fchs`: negated for less OR
 * unordered, read twice (a store-form abs CSEs the load). */
#define BR_TYRE_ABS(v) ((v) < 0.0f ? -(v) : (v))

/* T4 2026-09-24 (hand re-transcription from the asm): byte-exact, 1355/1355 B.
 * Shape: the wheel body is the SECOND parameter and carries its own hit
 * record; one rolling vector `c`; the body-frame force lands in `a`;
 * single-expression spin; && contact gate 0,2,1,3 with the free-spin else;
 * && wrap tail with the zero else; float -360 global; `_finite` import.
 * What each non-obvious spelling buys (every one was measured):
 *  - `volatile float cs` + the two-statement combine `c = cs*c;
 *    c = (sn*d) + c`: the original stores cs*c back into c, pops sin to its
 *    slot and re-reads both from memory (rounded to float) before the add.
 *    Without volatile VC5 forwards cs*c and sin in registers (different
 *    rounding, different bytes); no other qualifier, cast, prototype,
 *    union, aggregate or statement form stops the forwarding without
 *    breaking the cross-product block.  0x100645A0 (same original TU) shows
 *    the same store-and-reload of its sin result.
 *  - the load/dot block order is a.z, load, *pA, dot with
 *    `load = (a.z*nz)*3.5f` and `dot = ((vx*cx) + (vy*cy)) + (vz*cz)`: VC5
 *    honours explicit parentheses in its x87 schedule, and these are the
 *    only combination (of 80 orders x groupings) that loads v.z before the
 *    mass*g product and spills q after it.
 *  - the traction clamp negates `load/q` in place, reusing `load`: `fmul [q]`
 *    straight off the abs, `jmp` over the skip arm's pop.
 *  - the push-back force is `c.x * (0.0f - q)`: VC5 folds it to ONE fchs and
 *    multiplies registers; `c.x * -q` becomes -(c.x*q) per component.
 *  - the display angle is `(f1C4 * K) * dt` (explicit grouping).
 *  - no `pM` local (`&pBody->m` at both matrix calls): a pM local flips the
 *    spin's `r * X` operand order through CSE numbering.
 *  - PLACEMENT: this arm sits after the port's drive helpers, just before
 *    BrCarPhysDrive.  The grip-index evaluation order and the BrMat4MulVec3
 *    argument registers are TU state (the definitions compiled before this
 *    one), not source: every index spelling is byte-identical, while moving
 *    the arm flips them.  Earlier in the file they are not exact.
 *  - BrCosF/BrSinF are declared returning double, as br_cos.c defines them.
 */
/* WHAT IT DOES: the per-wheel tyre pass.  For a wheel on the ground it takes
 * the car's own sideways axis, projects it into the contact plane and turns
 * it by the steer angle to get the rolling direction, then -- if all four
 * wheels are touching -- turns the drive torque into a force along that
 * direction, caps it by the load the wheel carries (past the cap it falls to
 * a tenth), adds it to the wheel's force list, reports half the raw force
 * back through pA, and spins the wheel up by the reaction (a wheel in the
 * air just free-spins on its torque).  The spin is clamped to +-300 and the
 * display angle advanced and wrapped into a turn; a non-finite or runaway
 * angle is reset to zero. */
/* @implements 0x100651A0 glide BrCarPhysTyre */
void BrCarPhysTyre(BrTyreView *pBody, BrTyreView *pWheel, float *pA,
                   const unsigned char *pB, float dt)
{
    BrVec3  c;
    BrVec3  a;
    BrVec3  d;
    BrVec3  axis;
    BrVec3  v;
    BrVec3  fw;
    volatile float cs;
    float   sn;
    float   tq;
    float   q;
    float   load;
    float   dot;
    float   w;
    float   s;
    short   row;
    int     idx;

    axis.x = 0.0f;
    axis.y = 1.0f;
    axis.z = 0.0f;
    if (pWheel->hit.pPlane == 0)
        return;
    if ((double)pWheel->hit.nz < 0.7)
        return;
    BrMat4MulVec3Transposed(&a, &pBody->m, &axis);
    c.x = a.y * pWheel->hit.nz - a.z * pWheel->hit.ny;
    c.y = a.z * pWheel->hit.nx - a.x * pWheel->hit.nz;
    c.z = a.x * pWheel->hit.ny - a.y * pWheel->hit.nx;
    d.x = c.z * pWheel->hit.ny - c.y * pWheel->hit.nz;
    d.y = c.x * pWheel->hit.nz - c.z * pWheel->hit.nx;
    d.z = c.y * pWheel->hit.nx - c.x * pWheel->hit.ny;
    cs = BrCosF(pWheel->f1C0);
    sn = BrSinF(pWheel->f1C0);
    c.x = cs * c.x;
    c.y = cs * c.y;
    c.z = cs * c.z;
    c.x = (sn * d.x) + c.x;
    c.y = (sn * d.y) + c.y;
    c.z = (sn * d.z) + c.z;
    if (pBody->child[0]->f1B4 != 0 && pBody->child[2]->f1B4 != 0
     && pBody->child[1]->f1B4 != 0 && pBody->child[3]->f1B4 != 0) {
        BrRbVelAtBodyPoint(&v, (const BrRbBodyFull *)pBody, (BrRbBodyFull *)pWheel);
        tq = pWheel->f1CC;
        q = tq / pWheel->f1C8;
        a.x = 0.0f;
        a.y = 0.0f;
        a.z = (pBody->mass - pWheel->mass * -4.0f) * 2.943000316619873f;
        load = (a.z * pWheel->hit.nz) * 3.5f;
        *pA = *pA - q * -0.5f;
        dot = ((v.x * c.x) + (v.y * c.y)) + (v.z * c.z);
        if (*pB != 0) {
            row = (short)((*(int32_t *)&DAT_104b15e8) - 1);
            if (row > 2 || row < 0)
                row = 0;
            row = row * 8;
            idx = row + ((unsigned char *)pBody)[0x1FD] * 24
                + ((pWheel->hit.surface + pWheel->hit.surface + 1) >> 1);
            q = g_pBrCarPhysGrip[idx] * q;
        }
        if (BR_TYRE_ABS(q) > BR_TYRE_ABS(load)) {
            load = load / q;
            if (load < 0.0f)
                load = -load;
            q = load * q * 0.10000000149011612f;
        }
        fw.x = c.x * (0.0f - q);
        fw.y = c.y * (0.0f - q);
        fw.z = c.z * (0.0f - q);
        BrMat4MulVec3(&a, &pBody->m, &fw);
        pWheel->pForces->f.x = a.x + pWheel->pForces->f.x;
        pWheel->pForces->f.y = a.y + pWheel->pForces->f.y;
        pWheel->pForces->f.z = a.z + pWheel->pForces->f.z;
        w = ((tq - q * pWheel->f1C8) * dt + pWheel->f1C4)
          - (pWheel->f1C8 * ((tq - q * pWheel->f1C8) * dt + pWheel->f1C4) + dot) * 0.4000000059604645f;
    } else {
        w = pWheel->f1CC * dt + pWheel->f1C4;
    }
    pWheel->f1C4 = w;
    if (BR_TYRE_ABS(w) > 300.0f) {
        if (w == 0.0f)
            s = 0.0f;
        else if (w > 0.0f)
            s = 1.0f;
        else
            s = -1.0f;
        pWheel->f1C4 = s * 300.0f;
    }
    pWheel->f1D4 = pWheel->f1D4 - (pWheel->f1C4 * 57.2957763671875f) * dt;
    if (_finite((double)pWheel->f1D4) != 0 && !((double)pWheel->f1D4 < -36000.0)
     && (double)pWheel->f1D4 < 36000.0) {
        while ((double)pWheel->f1D4 > 360.0)
            pWheel->f1D4 = (float)((double)pWheel->f1D4 - 360.0);
        if ((double)pWheel->f1D4 < 0.0) {
            double t = (double)pWheel->f1D4;
            do {
                t = t - DAT_10077b28;
            } while (t < 0.0);
            pWheel->f1D4 = (float)t;
        }
    } else {
        pWheel->f1D4 = 0.0f;
    }
}

/* 0x100682C0 */
/* Transcribed from the Glide bytes: 150 planes of 0x20 bytes per cell at
 * 0x11773698 with the counts as words at 0x11778800; the direction (0,0,-1)
 * is folded (n.z * -1.0f, 0.0f added to x and y); the four windows are
 * double constants.
 * The direction is a local vector VC5 propagates (a literal 0.0f + x
 * folds away; the original keeps it), the hit point is scaled then added,
 * and like BrWheelGroundProbe it is byte-exact only in this TU and at this
 * position (after the tyre model). */
/* WHAT IT DOES: finds the height of the ground straight below a point. It
 * takes the collision-grid cell under the point and tries each of that
 * cell's surface planes that the point is within two units of and that is
 * not nearly vertical: the drop from the point to the plane along -z is
 * accepted if it is under two units, lower than the best so far, the plane
 * faces up (normal z above 0.2) and the point directly below lies inside
 * the plane's triangle. Returns the smallest accepted drop, or 100 when
 * nothing below qualifies. */
/* @implements 0x100682C0 glide BrGroundProbeZ */
float BrGroundProbeZ(const float *pPoint)
{
    /* 64-bit core: declared once, in br_globals.h or its struct's header */
    /* 64-bit core: declared once, in br_globals.h or its struct's header */
/* BrCollGridCellAcquire: prototype in br_funcs.h */
/* BrCrPlaneDist: prototype in br_funcs.h */
/* FUN_100656F0: prototype in br_funcs.h */
    const float *pPl;
    float best, t, h, q[3];
    float dir[3];
    int n;
    short cell;

    best = 100.0f;
    dir[0] = 0.0f;
    dir[1] = 0.0f;
    dir[2] = -1.0f;
    cell = BrCollGridCellAcquire(pPoint[0], pPoint[1]);
    pPl = (const float *)&DAT_11773698[cell][0];
    for (n = g_brCrPlane.aCellCount[cell]; n > 0;
         n--, pPl = (const float *)((const BrCollPlane *)pPl + 1)) {
        float d = BrCrPlaneDist(pPl, pPl[3], pPoint);

        if (!(d > -2.0) || !(d < 2.0))
            continue;
        t = pPl[2] * dir[2];
        if (!((t < 0.0f ? -t : t) > 0.001))
            continue;
        h = -(((pPl[1] * pPoint[1] + pPl[2] * pPoint[2]) + pPl[0] * pPoint[0]
               + pPl[3]) / t);
        q[0] = dir[0] * h;
        q[1] = dir[1] * h;
        q[2] = dir[2] * h;
        q[0] += pPoint[0];
        q[1] += pPoint[1];
        q[2] += pPoint[2];
        if (h > -2.0 && h < 2.0 && h < best && pPl[2] > 0.2
            && FUN_100656F0(pPl, q))
            best = h;
    }
    return best;
}

#define BrCarPhysTyre BrCarPhysTyre

/* (port-only BrCarPhysDrive removed) */


/* ==================================================================== */
/* 0x1005AA52 .. 0x1005AB7F -- the sign-change damper                    */
/* ==================================================================== */

/* (port-only BrCarPhysSignDamp removed) */


/* ==================================================================== */
/* 0x10067C30 -- the four-substep position integration                   */
/* ==================================================================== */

/* (port-only BrCarPhysAdvance removed) */


/* ==================================================================== */
/* 0x1005A7A0 -- one frame                                               */
/* ==================================================================== */

#undef BrCarPhysStep
#undef BrCarPhysAdvance
/* BrPodNop: prototype in br_funcs.h */
/* FUN_10064210: prototype in br_funcs.h */
/* FUN_1006d530: prototype in br_funcs.h */
/* BrCarPhysDriveMatch: prototype in br_funcs.h */
/* 64-bit core: declared once, by its definition's header */    /* 0x10067C30          */
typedef struct { int d[17]; } BrCpStateImage;      /* 0x44-byte rigid state */


#define BrCpSign(v) ((v) == DAT_10077780 ? DAT_10077780 : \
                     (v) > DAT_10077780 ? DAT_10077784 : DAT_10077788)
/* WHAT IT DOES: advances one car's rigid-body physics by a single frame.
 * It hangs the wheel force lists off the body, zeroes the per-wheel forces,
 * then runs the force generators in order -- spring, the four tyre passes
 * (gated so they run at most once per frame), drive, drag and damper -- and
 * integrates the body's velocity and orientation, first into a scratch copy
 * and then into the live state, applying a sign-change damper between the two.
 * Finally it advances the car record, records the suspension height for next
 * frame, and rebuilds every wheel's own matrix from its integrated state. */
/* @implements 0x1005A7A0 glide BrCarPhysStep */
void __fastcall BrCarPhysStep(BrDriverCar *pCar)
{
    char  *pBody;
    char  *pState;
    float *pf;
    int    k;

    BrPodNop();

    /* the body's force list starts at node 0; each wheel's at nodes 12,14,13,15
     * (car +0xBA0, +0xD20, +0xD60, +0xD40, +0xD80), and the wheel nodes' force
     * vectors are cleared */
    pCar->aBody[0].rb.pForces = &pCar->aForce[0];
    pCar->aBody[0].rb.child[0]->pForces = &pCar->aForce[12];
    pCar->aBody[0].rb.child[1]->pForces = &pCar->aForce[14];
    pCar->aBody[0].rb.child[2]->pForces = &pCar->aForce[13];
    pCar->aBody[0].rb.child[3]->pForces = &pCar->aForce[15];
    for (k = 0; k < 4; k++) {
        pCar->aBody[0].rb.child[k]->pForces->f.x = 0.0f;
        pCar->aBody[0].rb.child[k]->pForces->f.y = 0.0f;
        pCar->aBody[0].rb.child[k]->pForces->f.z = 0.0f;
    }

    pBody = (char *)&pCar->aBody[0].rb.f00;
    BrCarPhysSpring((BrRbBodyFull *)pBody);

    if (pCar->f0E84 == 0) {
        pCar->f0E74 = 0.0f;
        pCar->f0E7C = 0.0f;
        pCar->f0E80 = 0;
        BrCarPhysTyre((BrTyreView *)pBody, (BrTyreView *)((*(char * *)&pCar->aBody[0].rb.child[((0))])), &pCar->f0E7C,
                      &pCar->f0E80, BR_PHYS_DT);
        BrCarPhysTyre((BrTyreView *)pBody, (BrTyreView *)((*(char * *)&pCar->aBody[0].rb.child[((1))])), &pCar->f0E7C,
                      &pCar->f0E80, BR_PHYS_DT);
        BrCarPhysTyre((BrTyreView *)pBody, (BrTyreView *)((*(char * *)&pCar->aBody[0].rb.child[((2))])), &pCar->f0E74,
                      &pCar->f0E78, BR_PHYS_DT);
        BrCarPhysTyre((BrTyreView *)pBody, (BrTyreView *)((*(char * *)&pCar->aBody[0].rb.child[((3))])), &pCar->f0E74,
                      &pCar->f0E78, BR_PHYS_DT);
    }

    pCar->f0E84 = 0;
    pCar->aBody[0].rb.accel.x = 0.0f;
    pCar->aBody[0].rb.accel.y = 0.0f;
    pCar->aBody[0].rb.accel.z = 0.0f;
    pCar->aBody[0].rb.angAccel.x = 0.0f;
    pCar->aBody[0].rb.angAccel.y = 0.0f;
    pCar->aBody[0].rb.angAccel.z = 0.0f;
    BrRbAccumAll(pBody);

    pState = (char *)&pCar->aBody[0].rb.st.pos.x;
    BrCpIntegrateVelocity((BrRbState *)pState, (BrRbBodyFull *)pBody, BR_PHYS_DT);
    BrCarPhysDriveMatch((struct BrCarBody *)pBody, BR_PHYS_DT, &pCar->f0E7C, &pCar->f0E74, (char *)&pCar->f0E80, (char *)&pCar->f0E78);
    BrRbQuatDerivative(pState);

    pCar->aBody[0].rb.pForces = &pCar->aForce[4];   /* car+0xC20 */
    ((*(char **)(((((*(char * *)&pCar->aBody[0].rb.child[((0))])))) + 0x18))) = 0;
    ((*(char **)(((((*(char * *)&pCar->aBody[0].rb.child[((2))])))) + 0x18))) = 0;
    ((*(char **)(((((*(char * *)&pCar->aBody[0].rb.child[((1))])))) + 0x18))) = 0;
    ((*(char **)(((((*(char * *)&pCar->aBody[0].rb.child[((3))])))) + 0x18))) = 0;
    BrCarPhysDrag((BrRbBodyFull *)pBody, &pCar->aForce[11]);   /* car+0xD00 */
    BrCarPhysDamper((BrRbBodyFull *)pBody);

    pCar->aBody[0].rb.accel.x = 0.0f;
    pCar->aBody[0].rb.accel.y = 0.0f;
    pCar->aBody[0].rb.accel.z = 0.0f;
    pCar->aBody[0].rb.angAccel.x = 0.0f;
    pCar->aBody[0].rb.angAccel.y = 0.0f;
    pCar->aBody[0].rb.angAccel.z = 0.0f;
    BrRbAccumAll(pBody);

    *(BrCpStateImage *)&pCar->aBody[0].rb.st2.pos.x = *(BrCpStateImage *)pState;
    BrCpIntegrateVelocity(&pCar->aBody[0].rb.st2, (BrRbBodyFull *)pBody, BR_PHYS_DT);

    pf = &pCar->aBody[0].rb.st.angVel.x;
    k = 3;
    do {
        if (BrCpSign(pf[0]) != BrCpSign(pf[0x38]))
            pf[0] = 0.0f;
        else
            pf[0] = pf[0x38];
        if (BrCpSign(pf[-7]) != BrCpSign(pf[0x31]))
            pf[-7] = 0.0f;
        else
            pf[-7] = pf[0x31];
        pf++;
    } while (--k != 0);

    *(BrCpStateImage *)&pCar->aBody[0].rb.st1.pos.x = *(BrCpStateImage *)&pCar->aBody[0].rb.st.pos.x;
    BrRbQuatDerivative((char *)&pCar->aBody[0].rb.st1.pos.x);
    BrRbQuatDerivative((char *)&pCar->aBody[0].rb.st1.pos.x);
    BrCarPhysAdvance(pCar, pBody);
    *(BrCpStateImage *)&pCar->aBody[0].rb.st.pos.x = *(BrCpStateImage *)&pCar->aBody[0].rb.st2.pos.x;
    BrWheelSuspensionSetZ((BrRbBodyFull *)pBody);

    BrRbBuildMatrix((BrMat4 *)(((*(char * *)&pCar->aBody[0].rb.child[((0))])) + 0xbc), (BrRbState *)(((*(char * *)&pCar->aBody[0].rb.child[((0))])) + 0x78));
    BrRbBuildMatrix((BrMat4 *)(((*(char * *)&pCar->aBody[0].rb.child[((1))])) + 0xbc), (BrRbState *)(((*(char * *)&pCar->aBody[0].rb.child[((1))])) + 0x78));
    BrRbBuildMatrix((BrMat4 *)(((*(char * *)&pCar->aBody[0].rb.child[((2))])) + 0xbc), (BrRbState *)(((*(char * *)&pCar->aBody[0].rb.child[((2))])) + 0x78));
    BrRbBuildMatrix((BrMat4 *)(((*(char * *)&pCar->aBody[0].rb.child[((3))])) + 0xbc), (BrRbState *)(((*(char * *)&pCar->aBody[0].rb.child[((3))])) + 0x78));

    BrPodNop();
}

/* ==================================================================== */
/* Construction -- D3D 0x10062C50 / 0x10063000                           */
/* ==================================================================== */

/* (port-only BrCpNode removed) */


/* (port-only BrCarPhysInit removed) */


/* ==================================================================== */
/* 0x1006FD90 -- the car-data apply                                      */
/* ==================================================================== */

const BrCarData *g_pBrCarPhysCarData;   /* car+0x29C4 */

/* (port-only BrCarPhysApplyCarData removed) */


/* (port-only BrCarPhysPlace removed) */


/* ==================================================================== */
/* 0x10067C30 -- the original two-argument position pass                 */
/* ==================================================================== */

#undef BrCarPhysAdvance

/* 64-bit core: declared once, in br_globals.h or its struct's header */             /* 0.5f  -- BR_CP_UPRIGHT_MIN     */
/* 64-bit core: declared once, in br_globals.h or its struct's header */             /* 1/120 -- BR_CP_SUBSTEP         */
/* 64-bit core: declared once, in br_globals.h or its struct's header */             /* 0.002 -- BR_CP_SUBSTEP_EPS     */
/* 64-bit core: declared once, in br_globals.h or its struct's header */            /* the tip-kick trace string      */
/* 64-bit core: declared once, in br_globals.h or its struct's header */  /* 0x117781B0 -- the node pool    */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x11778844 -- its bump cursor  */
/* 64-bit core: declared once, in br_globals.h or its struct's header */  /* 0x1177819C                 */
/* The plane-distance vector is its own global in the original (0x117781A0,
 * next to the current-plane pointer), not the tail of the 0x117787F0 bank
 * the port gathers into BrCrPlaneState. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */           /* 0x117781A0                     */
/* BrPodNop: prototype in br_funcs.h */
/* BrCarCarCollide: prototype in br_funcs.h */
/* BrCtlAi: prototype in br_funcs.h */
/* BrCrRespWalk: prototype in br_funcs.h */
#undef BrCollRespTipKick
/* BrCollRespTipKick: prototype in br_funcs.h */

/* WHAT IT DOES: one frame of the car's position physics, in four fixed
 * substeps: reset this frame's contact list and shared plane state, gather
 * the nearby track triangles once, then repeatedly kick a tipped car
 * upright, collide car against car, integrate the state forward 1/120s,
 * rebuild the body matrix, hand the unit-box view of the world to the
 * collision response, and commit the result -- until 1/30s is used up.
 * Afterwards a stuck-detection timer counts down: an AI car resets it
 * whenever it stands upright, a player car also has to have moved a metre
 * since last frame, and the last position is remembered for that test. */
/* Source facts that moved bytes: the three reciprocal extents and the
 * matBox z drop each read their field into a float temp first (VC5 then
 * loads the field and divides/subtracts the other side from it: `fld
 * [ext]; fdivr [1.0f]`, `fld [h]; fsubr [mat]`); the upright threshold is
 * the literal 0.5f, not the extern (the extern defers the fcomp past the
 * state copy); the stuck distance is written inline as three repeated
 * differences -- named dx/dy/dz locals spill to two slots where the
 * original's CSE temps share one.  Also: literal 1.0f, the upright test
 * read directly in BOTH arms, not-upright arm first, the -1 clamp as its
 * own if followed by an unconditional decrement, z/y/x store order for the
 * 0.1f scale trio. */
/* @implements 0x10067C30 glide BrCarPhysAdvance */
void BrCarPhysAdvance(BrDriverCar *pCar, BrCarBody *pBody)
{
    BrMat4  mat;
    BrVec3  scale;
    float   t;
    float   h;

    g_pBrCollRespList = NULL;
    g_pBrCrCursor = s_aNode;
    g_brCrPlane.normal.x = 0.0f;
    g_brCrPlane.normal.y = 0.0f;
    g_brCrPlane.normal.z = 0.0f;
    g_brCrPlaneOut.x = 0.0f;
    g_brCrPlaneOut.y = 0.0f;
    g_brCrPlaneOut.z = 0.0f;
    g_brCrPlane.modeFC = 0;
    g_pBrCrCurPlane = NULL;
    BrPodNop();
    scale.z = 0.1f;
    scale.y = 0.1f;
    scale.x = 0.1f;
    BrMat4BuildScaledTransposed(&pBody->rb.m, &mat, &scale);
    BrCollRespBroadPhase((const BrRbBodyFull *)&pBody->rb.f00, &mat);
    BrPodNop();
    h = pBody->f01DC;
    scale.x = 1.0f / h;
    t = 0.033333335f;
    h = pBody->f01E0;
    scale.y = 1.0f / h;
    h = pBody->f01E4;
    scale.z = 1.0f / h;
    do {
        if (BrCollRespTipKick(&pBody->rb.f00) != 0) {
            BrPodNop();
        }
        BrCarCarCollide();
        BrRbIntegrateState(&pBody->rb.st2,
                           &pBody->rb.st1, 0.008333334f);
        BrRbBuildMatrix(&pBody->rb.m, &pBody->rb.st2);
        BrMat4BuildScaledTransposed(&pBody->rb.m, &mat, &scale);
        h = pBody->f01E8;
        mat.m[3][2] -= h;
        if (BrCrRespWalk(pBody, &mat) != 0) {
            BrRbQuatDerivative(&pBody->rb.st2);
            BrRbBuildMatrix(&pBody->rb.m, &pBody->rb.st2);
        }
        t = t - _DAT_10077b94;
        pBody->rb.st1 = pBody->rb.st2;
    } while (t > _DAT_10077b98);
    BrRbBuildMatrix(&pBody->rb.m, &pBody->rb.st2);
    BrPodNop();
    pBody->rb.st1 = pBody->rb.st2;
    if (*(void * *)&pCar->pfnControl == (void *)BrCtlAi) {
        if (!(pBody->rb.m.m[2][2] >= 0.5f)) {
            if (pBody->f01F8 < 0) {
                pBody->f01F8 = -1;
            }
            pBody->f01F8 = pBody->f01F8 - 1;
        } else {
            pBody->f01F8 = 0x23;
        }
    } else if (!(pBody->rb.m.m[2][2] >= 0.5f)) {
        if (pBody->f01F8 < 0) {
            pBody->f01F8 = -1;
        }
        if (!((*(float *)&pBody->rb.m.m[3] - pCar->f2AB0) * (*(float *)&pBody->rb.m.m[3] - pCar->f2AB0) + (pBody->rb.m.m[3][1] - pCar->f2AB4) * (pBody->rb.m.m[3][1] - pCar->f2AB4) + (pBody->rb.m.m[3][2] - pCar->f2AB8) * (pBody->rb.m.m[3][2] - pCar->f2AB8) >= DAT_10077a7c)) {
            pBody->f01F8 = pBody->f01F8 - 1;
        }
    } else {
        pBody->f01F8 = 0x78;
    }
    *(int *)&pCar->f2AB0 = *(int *)&pBody->rb.m.m[3];
    *(int *)&pCar->f2AB4 = *(int *)&pBody->rb.m.m[3][1];
    *(int *)&pCar->f2AB8 = *(int *)&pBody->rb.m.m[3][2];
}

/* ==================================================================== */
/* 0x10067F30 -- aerodynamic drag                                        */
/* ==================================================================== */

/* WHAT IT DOES: slows the car down with air resistance -- a push opposite
 * the velocity that grows with speed. Above walking pace, and only when a
 * wheel is on the special surface (4) and the weather is not mode 3, a
 * second, stronger term is stacked on top. */
/* vx and vy are float locals that VC5 homes in the dead argument slots
 * (copied through integer registers); vz stays on the x87 stack and is
 * popped unused after the sum.  The sum goes straight into BrSqrtF -- a
 * named `sq` local moves the argument store behind all four surface loads.
 * The four surface bytes are read before the speed test.  The second term
 * goes through a BrVec3 local: its frame is the `sub esp,0xc`, and the z
 * product's home store (`fst [esp+0x18]`) is the one VC5 keeps. */
/* @implements 0x10067F30 glide BrCarPhysDrag */
void BrCarPhysDrag(BrRbBodyFull *pBody, BrRbForce *pNode)
{
    float   vx;
    BrVec3  d;
    float   vy;
    float   vz;
    int32_t s0, s1, s2, s3;

    pNode->f.x = pBody->st.vel.x * BR_CP_DRAG_K;
    pNode->f.y = pBody->st.vel.y * BR_CP_DRAG_K;
    pNode->f.z = pBody->st.vel.z * BR_CP_DRAG_K;

    vx = pBody->st.vel.x;
    vy = pBody->st.vel.y;
    vz = pBody->st.vel.z;

    s0 = *(signed char *)&pBody->child[0]->f01A0;
    s1 = *(signed char *)&pBody->child[1]->f01A0;
    s2 = *(signed char *)&pBody->child[2]->f01A0;
    s3 = *(signed char *)&pBody->child[3]->f01A0;

    if (BrSqrtF(vx * vx + vy * vy + vz * vz) > BR_CP_DRAG_SPEED
        && (*(int32_t *)&DAT_104b15e8) != 3
        && (s0 == BR_CP_DRAG_SURFACE || s1 == BR_CP_DRAG_SURFACE
            || s2 == BR_CP_DRAG_SURFACE || s3 == BR_CP_DRAG_SURFACE)) {
        d.x = pBody->st.vel.x * BR_CP_DRAG_K2;
        d.y = pBody->st.vel.y * BR_CP_DRAG_K2;
        d.z = pBody->st.vel.z * BR_CP_DRAG_K2;
        pNode->f.x = d.x + pNode->f.x;
        pNode->f.y = d.y + pNode->f.y;
        pNode->f.z = d.z + pNode->f.z;
    }
}
