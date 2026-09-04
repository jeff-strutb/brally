/* br_wheelvel.c -- driving: the per-wheel contact-velocity step.
 *
 * Filed out of the address batch slice6_76.c.  The two declarations below are
 * the ones that batch made locally: BrRbVelAtBodyPointXY is spelled with an
 * unprototyped declaration on purpose, because the call sites here pass the
 * original's raw int arguments rather than slice3_42.h's typed ones.
 */

#ifdef BR_MATCHING_BUILD

extern float _DAT_10077a78;
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

#endif /* BR_MATCHING_BUILD */
