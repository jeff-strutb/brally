/* br_wheelvel.c -- driving: the per-wheel contact-velocity step.
 *
 * Filed out of the address batch slice6_76.c.  The two declarations below are
 * the ones that batch made locally: BrRbVelAtBodyPointXY is spelled with an
 * unprototyped declaration on purpose, because the call sites here pass the
 * original's raw int arguments rather than slice3_42.h's typed ones.
 */

#ifdef BR_MATCHING_BUILD

extern float _DAT_10077a78;
extern float _DAT_10077bc8;
int BrRbVelAtBodyPointXY();

/* WHAT IT DOES: work out the contact velocity at each of a car's four wheels
 * in turn and update that wheel from it. The per-wheel step of the driving
 * model, with the four wheels unrolled into a switch because the original
 * reads them from four separate fields. */
/* @implements 0x10068600 glide FUN_10068600 */
/* auto-filed from ghidra --refine; transforms: as-is */

void FUN_10068600(int param_1)

{
  float fVar1;
  int iVar2;
  int i;
  int *puVar4;
  int zero;
  float vel[3];
  
  zero = 0;
  puVar4 = *(int **)(param_1 + 0x18);
  i = 0;
  do {
    puVar4[3] = zero;
    puVar4[2] = zero;
    switch (i - zero) {
    case 0:
      BrRbVelAtBodyPointXY(vel, param_1, *(int *)(param_1 + 4));
      iVar2 = *(int *)(param_1 + 4);
      break;
    case 1:
      BrRbVelAtBodyPointXY(vel, param_1, *(int *)(param_1 + 8));
      iVar2 = *(int *)(param_1 + 8);
      break;
    case 2:
      BrRbVelAtBodyPointXY(vel, param_1, *(int *)(param_1 + 0xc));
      iVar2 = *(int *)(param_1 + 0xc);
      break;
    default:
      BrRbVelAtBodyPointXY(vel, param_1, *(int *)(param_1 + 0x10));
      iVar2 = *(int *)(param_1 + 0x10);
      break;
    }
    if (*(int *)(iVar2 + 0x1b4) == zero) {
      fVar1 = _DAT_10077a78;
    }
    else if (vel[2] < _DAT_10077a78) {
      fVar1 = _DAT_10077a78;
    }
    else {
      fVar1 = *(float *)(param_1 + 0x1bc) * vel[2];
    }
    *(float *)(puVar4 + 4) = fVar1;
    puVar4 = (int *)*puVar4;
    i = i + 1;
  } while (i < 4);
  return;
}

/* 0x10068070: the per-wheel ground probe. The port's br_phys.h gives it a
 * third (hit-record) argument; the original pushes exactly two. */
float BrWheelGroundProbe(int pBody, int pWheel);

/* WHAT IT DOES: set each of a car's four wheels' suspension height from the
 * ground probe: the probe measures downwards, so its result is negated and
 * recorded raw at wheel+0x1D8, then clamped -- anything above ground
 * becomes 0, anything deeper than -0.4 becomes -0.4 -- into the wheel's
 * z offset at +0x80. The four wheels are a switch because the original
 * reads them from four separate fields (as 0x10068600 above). */
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
