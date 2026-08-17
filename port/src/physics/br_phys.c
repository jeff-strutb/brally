/* br_phys.c -- vehicle physics: the ground query and the wheel drop.
 *
 * Transcribed from orig/BRGlide.dll at 0x100682C0, 0x10068070 and 0x10068450
 * (D3D 0x1006F310, 0x1006F0C0, 0x1006F4A0).  See br_phys.h for the contracts,
 * the offset map of the car's rigid body, and the integration debt.
 *
 * ON THE COMPARISONS.  Every accept/reject below is written as the exact
 * negation the x87 flag test implies, never the tidy positive form.  The
 * original tests `fcom` + `test ah,<mask>` and an UNORDERED compare sets both
 * C0 and C3, so a NaN takes the true side of any C0- or C3-keyed branch.
 * Concretely:
 *     test ah,0x41 (C0|C3) taken   ->  "less, equal or unordered"
 *     test ah,1    (C0)    taken   ->  "less or unordered"
 *     test ah,0x40 (C3)    taken   ->  "equal or unordered"
 * so `!(x > K)` rejects NaN and `x >= K` does not.  Both spellings appear
 * below and they are not interchangeable.
 *
 * ON PRECISION.  The four window constants are DOUBLES in the original
 * (`fcom qword ptr`), the operands are floats, and x87 compares them after
 * promotion.  C does the same promotion for a float-vs-double comparison, so
 * these are written against double literals on purpose -- narrowing them to
 * float would change the boundary.
 */
#include <stddef.h>

#include "br_phys.h"
#include "slice1_07.h"   /* BrTri, BrTriContainsPoint2D  (0x1006C740) */
#include "slice1_08.h"   /* BrPlaneEval                  (0x1006C9A0) */
#include "slice1_09.h"   /* BrMat4TransformPoint         (0x100747C0) */

/* ==================================================================== */
/* The search loop, shared by both probes                                */
/*                                                                       */
/* The original has it TWICE, once per probe, and the two copies differ   */
/* only in that the straight-down one has the direction folded in as the  */
/* constant (0, 0, -1) -- 0x100682C0 multiplies n.z by the -1.0f at       */
/* 0x10077A80 where 0x10068070 forms the full three-term dot product, and */
/* builds its hit point by adding 0.0f to x and y.  Folding the two is a  */
/* DEVIATION in representation only; the same precedent is set by         */
/* slice2_11.c, which folds the camera sweep's two identical passes.      */
/*                                                                       */
/* The summation order is the original's and is the same one BrPlaneEval  */
/* uses: (a.y*b.y + a.z*b.z) + a.x*b.x.  Float addition is not            */
/* associative, so this is not cosmetic.                                  */
/* ==================================================================== */

/* VESTIGIAL.  It used to select between a "body-local" and a "world" grid key
 * for the wheel probe, because two passes read 0x10068070 as keying the grid on
 * the wheel's body-local mount offset.  That reading was WRONG -- see the
 * adjudication in br_phys.h -- and the wheel probe now keys on the world point
 * unconditionally, which is what the bytes say and what the D3D twin says too.
 *
 * The symbol survives only because port/host/brally.c reads it (`-worldkey`)
 * and that file belongs to another pass.  Setting it changes NOTHING. */
int g_brPhysWheelGridWorldKey = 0;

static float BrPhysDot(const BrVec3 *pA, const BrVec3 *pB)
{
    return (pA->y * pB->y + pA->z * pB->z) + pA->x * pB->x;
}

/* Returns the best t found in cell `cell`, or `best` unchanged.  *ppBest is
 * left alone unless something is accepted. */
static float BrPhysProbeCell(short cell, const BrVec3 *pP, const BrVec3 *pD,
                             float best, const BrCollPlane **ppBest)
{
    const BrCollPlane *pPlane;
    unsigned           n, i;

    /* DEVIATION (memory safety).  The original indexes both tables blind;
     * 0x1006F720 leaves them NULL until a track is resident, and every other
     * consumer in this tree already guards.  A NULL grid here means "no
     * ground", which is what an unloaded track should look like. */
    if (g_pBrCollGrid == NULL || g_pBrCollGridCount == NULL) {
        return best;
    }

    /* `movzx eax, word ptr [...]` then `test eax,eax` / `jle` -- a signed
     * test on a zero-extended u16, so it can only ever mean "count == 0". */
    n = g_pBrCollGridCount[(unsigned)(int)cell];
    if (n == 0u) {
        return best;
    }

    /* The original walks 32 bytes per record from cell*4800 + base.  A
     * BrCollPlane is 32 bytes only on a 32-bit host, so this walks elements.
     * DEVIATION: representation only. */
    pPlane = g_pBrCollGrid + (size_t)(int)cell * BR_COLL_CELL_PLANES;

    for (i = 0; i < n; ++i, ++pPlane) {
        BrVec3 nrm;
        BrTri  tri;
        float  hit[3];
        float  dist, den, mag, t;

        nrm.x = pPlane->nx;
        nrm.y = pPlane->ny;
        nrm.z = pPlane->nz;

        /* The plane test.  The original calls 0x1006C9A0 with the record
         * pointer as the normal and its OWN +0x0C loaded separately as d --
         * two arguments for one 16-byte object.  Kept. */
        dist = BrPlaneEval(&nrm, pPlane->d, pP);

        /* test ah,0x41 -> reject "less, equal or unordered": accept only
         * dist > -2.  NaN is rejected here, which is why the next test can
         * be spelled the other way round without consequence. */
        if (!(dist > BR_PHYS_PROBE_NEAR)) {
            continue;
        }
        /* test ah,1 + `je` -> reject when NOT less, i.e. accept dist < 2 but
         * let unordered through.  `>=` reproduces that: NaN >= 2.0 is false. */
        if (dist >= BR_PHYS_PROBE_FAR) {
            continue;
        }

        den = BrPhysDot(pD, &nrm);

        /* fcom 0.0 / test ah,1 / `je` skips the fchs -- so the negation runs
         * for "less OR unordered".  A NaN would be negated in the original
         * and is not here; it is rejected by the very next test either way. */
        mag = (den < 0.0f) ? -den : den;
        if (!(mag > BR_PHYS_PROBE_EPSDOT)) {
            continue;
        }

        /* The original recomputes dot(n,P)+d inline here rather than reusing
         * the value it already has.  Same expression, same association, same
         * bits -- so `dist` is reused. */
        t = -(dist / den);

        hit[0] = pP->x + t * pD->x;
        hit[1] = pP->y + t * pD->y;
        hit[2] = pP->z + t * pD->z;

        /* Same three spellings again, in the original's order. */
        if (!(t > BR_PHYS_PROBE_NEAR)) {
            continue;
        }
        if (t >= BR_PHYS_PROBE_FAR) {
            continue;
        }
        if (t >= best) {
            continue;
        }
        /* Upward-facing only.  This is what makes walls invisible to the
         * ground probe, and it is tested on the RAW n.z, not on n.z relative
         * to the ray -- so it is a world-space test even in the wheel probe,
         * whose ray is not world-vertical. */
        if (!(nrm.z > BR_PHYS_PROBE_MINNZ)) {
            continue;
        }

        /* 0x1006C740.  BrTri and BrCollPlane have the same layout and the
         * original simply passes the record; building the BrTri explicitly
         * avoids a cast between two unrelated struct types, and the two
         * pointer fields it does not use are left as they are.
         * DEVIATION: representation only. */
        tri.n[0] = nrm.x;
        tri.n[1] = nrm.y;
        tri.n[2] = nrm.z;
        tri.f0C  = pPlane->d;
        tri.pA   = (const float *)(const void *)pPlane->pV0;
        tri.pB   = (const float *)(const void *)pPlane->pV1;
        tri.pC   = (const float *)(const void *)pPlane->pV2;

        if (BrTriContainsPoint2D(&tri, hit) == 0) {
            continue;
        }

        best = t;
        if (ppBest != NULL) {
            *ppBest = pPlane;
        }
    }
    return best;
}

/* ==================================================================== */
/* 0x1006F310 (D3D) / 0x100682C0 (Glide)                                 */
/* ==================================================================== */

float BrGroundProbeZ(const BrVec3 *pPoint)
{
    /* The straight-down direction is the -1.0f at 0x10077A80 and the two
     * 0.0f at 0x10077A78 that the original adds to x and y. */
    static const BrVec3 kDown = { 0.0f, 0.0f, -1.0f };
    short cell;

    cell = BrCollGridCellAcquire(pPoint->x, pPoint->y);
    return BrPhysProbeCell(cell, pPoint, &kDown, BR_PHYS_PROBE_MISS, NULL);
}

/* ==================================================================== */
/* 0x1006F0C0 (D3D) / 0x10068070 (Glide)                                 */
/* ==================================================================== */

float BrWheelGroundProbe(const BrRbBodyFull *pBody, BrRbBodyFull *pWheel,
                         BrGroundHit *pHit)
{
    static const BrVec3 kDownLocal = { 0.0f, 0.0f, -1.0f };
    const BrCollPlane  *pBest = NULL;
    BrVec3              mount, world, dir;
    float               best;
    short               cell;

    /* The mount point with its Z DISCARDED.  Z is not read: it is the
     * suspension displacement BrWheelSuspensionSetZ writes, and feeding it
     * back into the probe origin would make the probe recursive. */
    mount.x = pWheel->f78.x;
    mount.y = pWheel->f78.y;
    mount.z = 0.0f;

    /* body -> world.  0x100747C0 applies the upper 3x3 in the row-vector
     * sense and adds m[3][0..2] as the translation, which is exactly the
     * matrix BrRbBuildMatrix produces. */
    BrMat4TransformPoint(&world, &pBody->m, &mount);
    /* rotation only, same handedness: the car's own down axis in world. */
    BrMat4MulVec3Transposed(&dir, &pBody->m, &kDownLocal);

    /* Cleared BEFORE the search, so it doubles as the "no contact" flag. */
    pWheel->f19C = 0.0f;

    /* The grid cell comes from the WORLD point, exactly as in the straight-down
     * sibling.  This line was `BrCollGridCellAcquire(pWheel->f78.x, ...)` for
     * two passes on the strength of a stack-offset misreading; the frame
     * arithmetic that settles it is written out in br_phys.h.  In one line:
     * the original's reload
     *
     *     100680DC  mov ecx, [esp+0x1c]
     *     100680E0  mov edx, [esp+0x18]
     *     100680E4  add esp, 0xc          <-- AFTER the reload, not before
     *
     * happens while esp is still 0xC below the frame the SPILL used, so the
     * two identical-looking displacements name two different pairs of slots:
     * the spill wrote f78.x/.y, the reload reads 0x1006DA20's OUTPUT. */
    cell = BrCollGridCellAcquire(world.x, world.y);

    best = BrPhysProbeCell(cell, &world, &dir, BR_PHYS_PROBE_MISS, &pBest);

    if (pBest != NULL) {
        pWheel->f19C = 1.0f;      /* the original stores the pointer here */
        if (pHit != NULL) {
            pHit->pPlane  = pBest;
            pHit->surface = pBest->flags;
            pHit->nx      = pBest->nx;
            pHit->ny      = pBest->ny;
            pHit->nz      = pBest->nz;
            pHit->d       = pBest->d;
        }
    }
    return best;
}

/* ==================================================================== */
/* 0x1006F4A0 (D3D) / 0x10068450 (Glide)                                 */
/* ==================================================================== */

/* The original's 0x1006F0C0 records the winning triangle's surface byte at
 * wheel+0x1A0, and 0x10067F30 (the drag pass, br_carphys.c) reads all four of
 * them.  br_phys.h's DEVIATION moved those six fields to a typed out-parameter
 * because they cannot be written through BrRbBodyFull on LP64 -- which left
 * BrWheelSuspensionSetZ passing NULL and the surface bytes unreachable.
 *
 * This is that function with the out-parameter threaded through; the NULL
 * spelling below is the same body, and there is still exactly ONE
 * transcription of 0x1006F4A0. */
void BrWheelSuspensionSetZHit(BrRbBodyFull *pBody, BrGroundHit aHit[4])
{
    int i;

    for (i = 0; i < 4; ++i) {
        BrRbBodyFull *pWheel = pBody->child[i];
        float v, w;

        /* `fchs` on the return value -- the probe measures downwards and the
         * wheel offset is negative-down. */
        v = -BrWheelGroundProbe(pBody, pWheel,
                                (aHit != NULL) ? &aHit[i] : NULL);

        pWheel->f1D8 = v;

        /* test ah,0x41 taken keeps v ("less, equal or unordered"), so the
         * replacement by 0 happens only for a strictly positive v.  Written
         * as `v > 0` rather than `!(v <= 0)` so NaN keeps v, as it must. */
        w = (v > 0.0f) ? 0.0f : v;

        /* test ah,1 taken stores the constant ("less or unordered"), so NaN
         * lands on -0.4.  `!(w >= -0.4f)` reproduces that; `w < -0.4f` would
         * store the NaN instead. */
        pWheel->f78.z = !(w >= BR_PHYS_SUSP_MIN) ? BR_PHYS_SUSP_MIN : w;
    }
}

void BrWheelSuspensionSetZ(BrRbBodyFull *pBody)
{
    BrWheelSuspensionSetZHit(pBody, NULL);
}

/* ==========================================================================
 * ADAPTERS -- and a LIVE BUG they close.
 *
 * Both addresses below already had callers and a declaration, and neither had
 * a body, so both were satisfied by a generated stub. For 0x1006F310 that was
 * not merely "absent": the stub is `long BrProbe1006F310(void)`, the real
 * function returns a FLOAT, and a float return arrives in xmm0 -- which a stub
 * returning an integer never writes. So all seven call sites in slice2_12.c
 * have been reading whatever happened to be in xmm0, not zero and not a
 * height.
 *
 * That is the exact hazard recorded when the stub file was generated ("a
 * caller expecting a float gets whatever was in xmm0"). It sat undetected
 * because a wrong height is not a crash and nothing exercised the path.
 *
 * The bodies exist under the names this module gave them; these are thin
 * adapters to the declarations the callers already use, not second
 * transcriptions.
 * ========================================================================== */

float BrProbe1006F310(const float av3[3])
{
    BrVec3 p;
    if (!av3) return 100.0f;      /* the original's "no ground" value */
    p.x = av3[0]; p.y = av3[1]; p.z = av3[2];
    return BrGroundProbeZ(&p);
}

void BrSub1006F4A0(void *pCar164)
{
    /* slice3_40.h declares the argument `void *pCar164`. The physics recon
     * established that pCar+0x164 IS the BrRbBodyFull, which is what this
     * module's port takes -- so the cast is the identity the two headers were
     * describing from opposite ends, not a reinterpretation. */
    if (pCar164) BrWheelSuspensionSetZ((BrRbBodyFull *)pCar164);
}
