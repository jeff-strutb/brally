/* br_wheelvel.c -- driving: the per-wheel suspension-height step.
 *
 * Filed out of the address batch slice6_76.c.  The damper 0x10068600 that
 * used to sit here moved to src/core/driving/br_carphys.c (its module, with
 * the other three force generators) as BrCarPhysDamper.
 */

#ifdef BR_MATCHING_BUILD

extern float _DAT_10077bc8;

/* 0x10068070 */
/* WHAT IT DOES: finds how far one wheel can drop before it meets the ground.
 * The wheel's mount point (its x and y offset, height ignored) is taken into
 * the world by the car body's matrix and the car's own down axis is rotated
 * the same way; then the collision-grid cell under that point is searched
 * exactly as BrGroundProbeZ searches it, along that axis instead of straight
 * down. On a hit the wheel records the plane, its surface byte and the
 * plane's normal and constant. Returns the shortest accepted drop, or 100. */
/* Transcribed from the Glide bytes: the body matrix is pBody+0xBC; the
 * contact is cleared (wheel+0x19C = 0) before the search and set to the
 * plane pointer on a hit; the hit point reuses the mount's slots; the drop
 * h is kept in memory (stored once, reloaded for every use).
 * T2, 581/581 B, same 174 instructions, REGNORM 12+12 (2026-09-25): only
 * the operand roles of the two dot products differ (the original loads the
 * local vector and multiplies by the plane; this build the reverse). The
 * vectors declared first put everything else in place. Dead: operand order
 * in all products, struct vectors, an inline hit-point helper, volatile h
 * (breaks the slot packing), pad count 1..32, 90 declaration orders. */
/* @implements 0x10068070 glide BrWheelGroundProbe */
float BrWheelGroundProbe(int pBody, int pWheel)
{
    extern unsigned char  DAT_11773698[];
    extern unsigned short DAT_11778800[];
    extern short BrCollGridCellAcquire(float x, float y);
    extern float BrCrPlaneDist(const float *pN, float planeD,
                               const float *pPoint);
    extern short FUN_100656F0(const float *pTri, const float *pP);
    extern void BrMat4TransformPoint(float *pOut, const void *pM,
                                     const float *pV);
    extern void BrMat4MulVec3Transposed(float *pOut, const void *pM,
                                        const float *pV);
    float world[3], dir[3], mount[3], down[3];
    const float *pPl;
    float best, t, h;
    int n;
    short cell;

    best = 100.0f;
    mount[0] = *(float *)(pWheel + 0x78);
    mount[1] = *(float *)(pWheel + 0x7c);
    down[0] = 0.0f;
    down[1] = 0.0f;
    down[2] = -1.0f;
    mount[2] = 0.0f;
    BrMat4TransformPoint(world, (const void *)(pBody + 0xbc), mount);
    BrMat4MulVec3Transposed(dir, (const void *)(pBody + 0xbc), down);
    *(int *)(pWheel + 0x19c) = 0;
    cell = BrCollGridCellAcquire(world[0], world[1]);
    pPl = (const float *)(DAT_11773698 + cell * 0x12C0);
    for (n = DAT_11778800[cell]; n > 0; n--, pPl += 8) {
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
            *(const float **)(pWheel + 0x19c) = pPl;
            *(unsigned char *)(pWheel + 0x1a0) =
                *((const unsigned char *)pPl + 0x1e);
            *(float *)(pWheel + 0x1a4) = pPl[0];
            *(float *)(pWheel + 0x1a8) = pPl[1];
            *(float *)(pWheel + 0x1ac) = pPl[2];
            *(float *)(pWheel + 0x1b0) = pPl[3];
        }
    }
    return best;
}

/* 0x100682C0 */
/* WHAT IT DOES: finds the height of the ground straight below a point. It
 * takes the collision-grid cell under the point and tries each of that
 * cell's surface planes that the point is within two units of and that is
 * not nearly vertical: the drop from the point to the plane along -z is
 * accepted if it is under two units, lower than the best so far, the plane
 * faces up (normal z above 0.2) and the point directly below lies inside
 * the plane's triangle. Returns the smallest accepted drop, or 100 when
 * nothing below qualifies. */
/* Transcribed from the Glide bytes: 150 planes of 0x20 bytes per cell at
 * 0x11773698 with the counts as words at 0x11778800; the direction (0,0,-1)
 * is folded (n.z * -1.0f, 0.0f added to x and y); the four windows are
 * double constants.
 * T2, 387/387 B, same 126 instructions, REGNORM 2+2 (2026-09-25). The
 * direction is a local vector VC5 propagates (literal 0.0f + x folds away;
 * the original keeps it), and the hit point is scaled then added. Residue is
 * the x87 order of the hit point's x term; with BrWheelGroundProbe as the
 * real predecessor the first operand role already matches. */
/* @implements 0x100682C0 glide BrGroundProbeZ */
float BrGroundProbeZ(const float *pPoint)
{
    extern unsigned char  DAT_11773698[];
    extern unsigned short DAT_11778800[];
    extern short BrCollGridCellAcquire(float x, float y);
    extern float BrCrPlaneDist(const float *pN, float planeD,
                               const float *pPoint);
    extern short FUN_100656F0(const float *pTri, const float *pP);
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
    pPl = (const float *)(DAT_11773698 + cell * 0x12C0);
    for (n = DAT_11778800[cell]; n > 0; n--, pPl += 8) {
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

/* 0x10068070: the per-wheel ground probe. The port's br_phys.h gives it a
 * third (hit-record) argument; the original pushes exactly two. */
float BrWheelGroundProbe(int pBody, int pWheel);

/* WHAT IT DOES: set each of a car's four wheels' suspension height from the
 * ground probe: the probe measures downwards, so its result is negated and
 * recorded raw at wheel+0x1D8, then clamped -- anything above ground
 * becomes 0, anything deeper than -0.4 becomes -0.4 -- into the wheel's
 * z offset at +0x80. The four wheels are a switch because the original
 * reads them from four separate fields (as the damper 0x10068600, now in
 * br_carphys.c). */
/* @implements 0x10068450 glide BrWheelSuspensionSetZ */
void BrWheelSuspensionSetZ(int pCar)
{
    int i;
    double v;
    float w;

    for (i = 0; i < 4; i++) {
        /* pWheel is scoped to the loop and UNINITIALISED, and both facts are
         * load-bearing.  The switch has no default, so the compiler keeps a
         * reachable fall-through path in which pWheel is never assigned; for
         * that path it homes pWheel on the incoming parameter's own stack
         * slot, which is why the original loads [esp+arg] TWICE (esi and
         * edi) instead of copying one register to the other.  Seeding it
         * (`int pWheel = pCar;`), hoisting it above the loop, or making the
         * parameter the wheel variable all turn the second load into
         * `mov edi, esi` and cost 2 bytes -- all six spellings were probed. */
        int pWheel;
        switch (i) {
        case 0: pWheel = *(int *)(pCar + 4);    break;
        case 1: pWheel = *(int *)(pCar + 8);    break;
        case 2: pWheel = *(int *)(pCar + 0xc);  break;
        case 3: pWheel = *(int *)(pCar + 0x10); break;
        }
        /* `fchs` on the return value -- the probe measures downwards. */
        v = -BrWheelGroundProbe(pCar, pWheel);
        w = v;
        *(float *)(pWheel + 0x1d8) = v;
        if (w > 0.0f)
            w = 0.0f;
        if (w < _DAT_10077bc8)
            *(float *)(pWheel + 0x80) = -0.4f;
        else
            *(float *)(pWheel + 0x80) = w;
    }
}

#endif /* BR_MATCHING_BUILD */
