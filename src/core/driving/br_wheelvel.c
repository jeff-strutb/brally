/* br_wheelvel.c -- driving: the per-wheel suspension-height step.
 *
 * Filed out of the address batch slice6_76.c.  The damper 0x10068600 that
 * used to sit here moved to src/core/driving/br_carphys.c (its module, with
 * the other three force generators) as BrCarPhysDamper.
 */

#ifdef BR_MATCHING_BUILD

extern float _DAT_10077bc8;

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
