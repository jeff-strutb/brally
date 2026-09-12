/* br_cardrive.c -- 0x100645A0, the axle velocity constraint.
 *
 * Matching twin of the port's BrCarPhysDrive.  The original is six arguments
 * (body, dt, and the four car+0xE74..0xE80 scalars); the port folds those
 * into the car object.  This file is the matching build only.
 */

#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)

extern float _DAT_10077a78;
extern float _DAT_10077a7c;
extern float _DAT_10077a80;
extern float _DAT_10077a84;
extern float _DAT_10077a88;
extern float _DAT_10077a8c;
extern float _DAT_10077a90;
extern double _DAT_10077a98;
extern float _DAT_10077aa0;
extern float _DAT_10077aa4;
extern float _DAT_10077aa8;
extern float _DAT_10077aac;
extern double _DAT_10077ab0;
extern float _DAT_10077ab8;
extern float _DAT_10077abc;
extern float _DAT_10077ac0;
extern float _DAT_10077ac4;
extern float _DAT_10077ac8;
extern float _DAT_10077ad0;
extern float _DAT_10077ad4;
extern float _DAT_10077ad8;
extern char DAT_100b5178;
extern int DAT_104b15e8;
extern int DAT_11778808;
extern int DAT_11778820;

void BrRbVelAtPoint(void *pOut, int body, void *pPoint);
void BrMat4MulVec3(void *pOut, int m, void *pV);
void BrMat4MulVec3Transposed(void *pOut, int m, void *pV);
float BrSqrtF(float x);
float BrCosF(float a);
float BrSinF(float a);

/* WHAT IT DOES: constrain a car body's linear and angular velocity from the
 * two axle velocities. When both axles have contact it strips lateral slip,
 * applies the brake term on X, solves one yaw rate and overwrites vel and
 * angVel through the body matrix, then eases the visual roll toward the
 * retained side force. Not an engine: there is no throttle or gearbox.
 *
 * Residue: sideForce/ran are an adjacent float+int aggregate (orig [esp+0x58]/
 * [esp+0x5c]); remaining unpaired is x87 scheduling (fcom vs fcomp, fsub st(2)
 * vs fsubr, extra fld st / fstp st on the roll abs). Frame sub esp, 0x8c.
 * Insn gap 1.
 *
 * The `hold` compare (orig +0x416..0x4fe): the original SELECTS at the read
 * (`cmp byte [param_5]; je; fld K; jmp; fld hold-slot`) and carries the value
 * in st across the whole table block to a `fcomp st(1)`; ours stores on the
 * *param_5 arm and compares memory.  DEAD 2026-09-12, do not re-run: the
 * select spelled as a ternary into a REUSED local (fVar11) and into a FRESH
 * single-use local both HOME the result to a slot (+3 insns, regnorm 23+22 ->
 * 26+22 both).  The st-carry is not reachable from a named local; whatever
 * spelling the original had keeps both fVar2 and the selected bound on the
 * fp stack -- same axis as the BrCrRespWalk stack-dup fork.  Park. */
/* @t4-pass 0x100645A0 1 2026-09-09 probes 10 bytes 3093 insns 863 regions 17 rows 55 census yes */
/* @t4-pass 0x100645A0 2 2026-09-09 probes 10 bytes 3093 insns 863 regions 17 rows 55 census yes */
/* @implements 0x100645A0 glide BrCarPhysDriveMatch */
void BrCarPhysDriveMatch(int param_1, float param_2, float *param_3,
                         float *param_4, char *param_5, char *param_6)
{
  float fVar1;
  float fVar2;
  unsigned char bVar3;
  unsigned char bVar4;
  int iVar5;
  float fVar6;
  float fVar7;
  float fVar8;
  int iVar9;
  int iVar10;
  float fVar11;
  float local_8c;
  float local_88;
  int local_84;
  int local_x;
  float local_80;
  int local_6c;
  float hold;
  float local_3c;
  float local_38;
  unsigned int local_1c;
  float vA[3];
  float vB[3];
  float wld[3];
  float pt[3];
  float tmp[3];
  float tmpB[3];
  float lat[3];
  float svB[3];
  struct {
    float sideForce;
    int ran;
  } g;

  g.sideForce = 0.0f;
  g.ran = 0;
  if (*(int *)(*(int *)(param_1 + 4) + 0x19c) == g.ran) {
    *(int *)(*(int *)(param_1 + 4) + 0x1b4) = g.ran;
  }
  if (*(int *)(*(int *)(param_1 + 8) + 0x19c) == g.ran) {
    *(int *)(*(int *)(param_1 + 8) + 0x1b4) = g.ran;
  }
  if (*(int *)(*(int *)(param_1 + 0xc) + 0x19c) == g.ran) {
    *(int *)(*(int *)(param_1 + 0xc) + 0x1b4) = g.ran;
  }
  if (*(int *)(*(int *)(param_1 + 0x10) + 0x19c) == g.ran) {
    *(int *)(*(int *)(param_1 + 0x10) + 0x1b4) = g.ran;
  }
  iVar5 = *(int *)(param_1 + 0xc);
  if (*(float *)(iVar5 + 0x1d0) < _DAT_10077a78) {
    fVar1 = -*(float *)(iVar5 + 0x1d0);
  } else {
    fVar1 = *(float *)(iVar5 + 0x1d0);
  }
  if (*(float *)(iVar5 + 0x1c4) == _DAT_10077a78) {
    fVar7 = _DAT_10077a78;
  } else if (*(float *)(iVar5 + 0x1c4) > _DAT_10077a78) {
    fVar7 = _DAT_10077a7c;
  } else {
    fVar7 = _DAT_10077a80;
  }
  fVar1 = fVar7 * fVar1 * _DAT_10077a84;
  fVar1 = fVar1 / *(float *)(iVar5 + 0x1c8);
  iVar9 = *(int *)(param_1 + 4);
  if (*(float *)(iVar9 + 0x1d0) < _DAT_10077a78) {
    fVar2 = -*(float *)(iVar9 + 0x1d0);
  } else {
    fVar2 = *(float *)(iVar9 + 0x1d0);
  }
  if (*(float *)(iVar9 + 0x1c4) == _DAT_10077a78) {
    fVar6 = _DAT_10077a78;
  } else if (*(float *)(iVar9 + 0x1c4) > _DAT_10077a78) {
    fVar6 = _DAT_10077a7c;
  } else {
    fVar6 = _DAT_10077a80;
  }
  fVar2 = fVar6 * fVar2 * _DAT_10077a84;
  fVar2 = fVar2 / *(float *)(iVar9 + 0x1c8);
  fVar8 = *(float *)(param_1 + 0x2c) * _DAT_10077a88;
  local_88 = (fVar1 / fVar8) * param_2 * param_2;
  local_8c = (fVar2 / fVar8) * param_2 * param_2;
  if (local_88 < _DAT_10077a78) {
    fVar1 = -local_88;
  } else {
    fVar1 = local_88;
  }
  if (fVar1 > _DAT_10077a7c) {
    if (local_88 == _DAT_10077a78) {
      fVar1 = _DAT_10077a78;
    } else if (local_88 > _DAT_10077a78) {
      fVar1 = _DAT_10077a7c;
    } else {
      fVar1 = _DAT_10077a80;
    }
    local_88 = fVar1 * _DAT_10077a8c;
  }
  if (local_8c < _DAT_10077a78) {
    fVar1 = -local_8c;
  } else {
    fVar1 = local_8c;
  }
  if (fVar1 > _DAT_10077a7c) {
    if (local_8c == _DAT_10077a78) {
      fVar1 = _DAT_10077a78;
    } else if (local_8c > _DAT_10077a78) {
      fVar1 = _DAT_10077a7c;
    } else {
      fVar1 = _DAT_10077a80;
    }
    local_8c = fVar1 * _DAT_10077a8c;
  }
  pt[2] = 0.0f;
  pt[1] = 0.0f;
  pt[0] = *(float *)(iVar9 + 0x78);
  BrRbVelAtPoint(tmp, param_1, pt);
  iVar5 = param_1 + 0xbc;
  BrMat4MulVec3(vA, iVar5, tmp);
  bVar3 = *(unsigned char *)(*(int *)(param_1 + 4) + 0x1a0);
  *(unsigned char *)&local_3c = bVar3;
  bVar4 = *(unsigned char *)(*(int *)(param_1 + 8) + 0x1a0);
  *(unsigned char *)&local_80 = bVar4;
  *(unsigned char *)&local_1c = *(unsigned char *)(*(int *)(param_1 + 0xc) + 0x1a0);
  *(unsigned char *)&local_38 = *(unsigned char *)(*(int *)(param_1 + 0x10) + 0x1a0);
  iVar9 = DAT_104b15e8 + -1;
  if ((2 < (short)iVar9) || ((short)iVar9 < 0)) {
    iVar9 = 0;
  }
  local_6c = iVar9 << 3;
  *(unsigned char *)(param_1 + 0x209) = 0;
  if (((*(int *)(*(int *)(param_1 + 4) + 0x1b4) == 0) &&
      (*(int *)(*(int *)(param_1 + 8) + 0x1b4) == 0)) ||
     ((*(int *)(*(int *)(param_1 + 0xc) + 0x1b4) == 0 &&
      (*(int *)(*(int *)(param_1 + 0x10) + 0x1b4) == 0)))) {
    *param_5 = '\0';
  }
  else {
    if (vA[1] < _DAT_10077a78) {
      fVar1 = -vA[1];
    } else {
      fVar1 = vA[1];
    }
    if (*param_3 < _DAT_10077a78) {
      fVar7 = -*param_3;
    } else {
      fVar7 = *param_3;
    }
    g.ran = 1;
    *(int *)&hold = 0x45FA0000;
    if (local_88 < _DAT_10077a78) {
      fVar2 = -local_88;
    } else {
      fVar2 = local_88;
    }
    if (!(fVar2 > _DAT_10077a98)) {
      if (local_8c < _DAT_10077a78) {
        fVar2 = -local_8c;
      } else {
        fVar2 = local_8c;
      }
      local_84 = (fVar2 > _DAT_10077a98);
      fVar2 = (float)local_84 * _DAT_10077aa4;
    }
    else {
      if (local_8c < _DAT_10077a78) {
        fVar2 = -local_8c;
      } else {
        fVar2 = local_8c;
      }
      local_84 = (fVar2 > _DAT_10077a98);
      fVar2 = (float)local_84 * _DAT_10077aa0;
    }
    fVar8 = (*(float *)(param_1 + 0x2c) * fVar1) / param_2 + fVar7;
    fVar2 = fVar8 - fVar2;
    if (*param_5 != '\0') {
      hold = _DAT_10077aa8;
    }
    *param_5 = '\0';
    iVar9 = (int)((*(unsigned int *)&local_80 & 0xff) + 1 +
                  (*(unsigned int *)&local_3c & 0xff)) >> 1;
    iVar10 = (iVar9 + (int)(short)local_6c) * 4;
    local_3c = *(float *)(DAT_11778808 +
                         ((int)(short)local_6c +
                          (unsigned int)*(unsigned char *)(param_1 + 0x1fd) * 0x18 +
                          iVar9) * 4);
    local_84 = *(int *)(&DAT_100b5178 + iVar10);
    fVar7 = *(float *)(DAT_11778820 + iVar10);
    if (fVar2 > fVar7) {
      fVar2 = fVar7;
    }
    if (!(fVar2 >= *(float *)&local_84)) {
      fVar2 = *(float *)&local_84;
    }
    fVar2 = *(float *)&local_84 / fVar2;
    local_80 = fVar2 * local_3c * _DAT_10077aac;
    if (*(float *)(*(int *)(param_1 + 0xc) + 0x1c0) == _DAT_10077a78) {
      local_80 = local_80 * _DAT_10077ab0;
    }
    if (local_80 < _DAT_10077a78) {
      fVar7 = -local_80;
    } else {
      fVar7 = local_80;
    }
    if (fVar7 > _DAT_10077a7c) {
      local_80 = 1.0f;
    }
    if (!(fVar2 < hold)) {
      /* orig: mov y-bits, mov x-bits, fld z — integer copies of x/y so the
       * squares go through stack slots, not fld [body+0x84]. */
      local_84 = *(int *)(param_1 + 0x88);
      local_x = *(int *)(param_1 + 0x84);
      fVar11 = *(float *)(param_1 + 0x8c);
      fVar11 = BrSqrtF((*(float *)&local_x * *(float *)&local_x
                        + *(float *)&local_84 * *(float *)&local_84)
                       + fVar11 * fVar11);
      if (fVar11 < _DAT_10077ab8) {
        fVar11 = (_DAT_10077ab8 - fVar11) * _DAT_10077abc;
        if (local_80 < fVar11) {
          local_80 = fVar11;
        }
      }
      fVar1 = vA[1] * local_80;
      vA[1] = vA[1] - fVar1;
      *param_5 = '\x01';
      g.sideForce = local_80 * vA[1];
    }
    else {
      g.sideForce = 0.0f;
      vA[1] = _DAT_10077a78;
    }
    if (vA[0] < _DAT_10077a78) {
      fVar1 = -vA[0];
    } else {
      fVar1 = vA[0];
    }
    if (fVar1 > _DAT_10077a7c) {
      fVar1 = vA[1] / vA[0];
      if (fVar1 < _DAT_10077a78) {
        fVar1 = -fVar1;
      }
      if (fVar1 > _DAT_10077a88) {
        *(unsigned char *)(param_1 + 0x209) = 0x80;
      }
    } else {
      if (vA[1] < _DAT_10077a78) {
        fVar1 = -vA[1];
      } else {
        fVar1 = vA[1];
      }
      if (fVar1 > _DAT_10077a7c) {
        *(unsigned char *)(param_1 + 0x209) = 0x80;
      } else {
        *(unsigned char *)(param_1 + 0x209) = 0;
      }
    }
    fVar2 = vA[0];
    vA[0] = vA[0] - local_8c;
    if (vA[0] < _DAT_10077a78) {
      fVar1 = -vA[0];
    } else {
      fVar1 = vA[0];
    }
    if (fVar1 > _DAT_10077ac0) {
      if (vA[0] == _DAT_10077a78) {
        fVar1 = _DAT_10077a78;
      } else if (vA[0] > _DAT_10077a78) {
        fVar1 = _DAT_10077a7c;
      } else {
        fVar1 = _DAT_10077a80;
      }
      if (fVar2 == _DAT_10077a78) {
        fVar7 = _DAT_10077a78;
      } else if (fVar2 > _DAT_10077a78) {
        fVar7 = _DAT_10077a7c;
      } else {
        fVar7 = _DAT_10077a80;
      }
      if (fVar1 != fVar7) {
        vA[0] = 0.0f;
      }
    }
  }
  pt[0] = *(float *)(*(int *)(param_1 + 0xc) + 0x78);
  pt[1] = 0.0f;
  pt[2] = 0;
  BrRbVelAtPoint(tmpB, param_1, pt);
  BrMat4MulVec3(vB, iVar5, tmpB);
  if (((*(int *)(*(int *)(param_1 + 0xc) + 0x1b4) != 0) ||
      (*(int *)(*(int *)(param_1 + 0x10) + 0x1b4) != 0)) &&
     ((*(int *)(*(int *)(param_1 + 4) + 0x1b4) != 0 ||
      (*(int *)(*(int *)(param_1 + 8) + 0x1b4) != 0)))) {
    g.ran = 1;
    pt[0] = BrCosF(*(float *)(*(int *)(param_1 + 0xc) + 0x1c0));
    lat[2] = vB[2];
    pt[1] = BrSinF(*(float *)(*(int *)(param_1 + 0xc) + 0x1c0));
    svB[2] = vB[2];
    svB[0] = vB[0];
    fVar1 = vB[1] * pt[1] + vB[0] * pt[0];
    fVar7 = fVar1 * pt[0];
    fVar1 = fVar1 * pt[1];
    svB[1] = vB[1];
    vB[2] = 0.0f;
    lat[0] = vB[0] - fVar7;
    pt[2] = 0;
    lat[1] = vB[1] - fVar1;
    lat[2] = lat[2] - vB[2];
    if (!(*param_4 < _DAT_10077a78)) {
      local_8c = *param_4;
    }
    else {
      local_8c = -*param_4;
    }
    vB[0] = fVar7;
    vB[1] = fVar1;
    fVar11 = BrSqrtF(lat[2] * lat[2] + lat[1] * lat[1] + lat[0] * lat[0]);
    if (local_88 < _DAT_10077a78) {
      local_88 = -local_88;
    }
    local_88 = (float)(int)(local_88 > _DAT_10077a98);
    fVar1 = (fVar11 * *(float *)(param_1 + 0x2c)) / param_2 + local_8c;
    fVar1 = fVar1 - local_88 * _DAT_10077aa0;
    fVar7 = _DAT_10077a90;
    if (*param_6 != '\0') {
      fVar7 = _DAT_10077ac4;
    }
    *param_6 = '\0';
    if (!(fVar1 > fVar7)) {
      *param_6 = '\0';
    }
    else {
      iVar9 = (int)((local_1c & 0xff) + 1 + (*(unsigned int *)&local_38 & 0xff)) >> 1;
      iVar10 = (iVar9 + (int)(short)local_6c) * 4;
      local_38 = *(float *)(DAT_11778808 +
                           (iVar9 + (unsigned int)*(unsigned char *)(param_1 + 0x1fd) * 0x18 +
                            (int)(short)local_6c) * 4);
      local_84 = *(int *)(&DAT_100b5178 + iVar10);
      local_80 = *(float *)(DAT_11778820 + iVar10);
      if (fVar1 > local_80) {
        fVar1 = local_80;
      }
      if (!(fVar1 >= *(float *)&local_84)) {
        fVar1 = *(float *)&local_84;
      }
      fVar1 = *(float *)&local_84 / fVar1;
      local_80 = fVar1 * local_38 * _DAT_10077aac;
      if (*(float *)(*(int *)(param_1 + 0xc) + 0x1c0) == _DAT_10077a78) {
        local_80 = local_80 * _DAT_10077ab0;
      }
      if (local_80 < _DAT_10077a78) {
        fVar1 = -local_80;
      } else {
        fVar1 = local_80;
      }
      if (fVar1 > _DAT_10077a7c) {
        local_80 = 1.0f;
      }
      local_84 = *(int *)(param_1 + 0x88);
      local_x = *(int *)(param_1 + 0x84);
      fVar11 = *(float *)(param_1 + 0x8c);
      fVar11 = BrSqrtF((*(float *)&local_x * *(float *)&local_x
                        + *(float *)&local_84 * *(float *)&local_84)
                       + fVar11 * fVar11);
      if (fVar11 < _DAT_10077ab8) {
        fVar11 = (_DAT_10077ab8 - fVar11) * _DAT_10077abc;
        if (local_80 < fVar11) {
          local_80 = fVar11;
        }
      }
      vB[0] = svB[0] - lat[0] * local_80;
      vB[1] = svB[1] - lat[1] * local_80;
      vB[2] = svB[2] - lat[2] * local_80;
      *param_6 = '\x01';
    }
  }
  if (g.ran != 0) {
    svB[0] = (vA[0] + vB[0]) * _DAT_10077ac8;
    svB[2] = (vA[1] - vB[1]) /
             (*(float *)(*(int *)(param_1 + 4) + 0x78) -
              *(float *)(*(int *)(param_1 + 0xc) + 0x78));
    svB[1] = vA[1] - svB[2] * *(float *)(*(int *)(param_1 + 4) + 0x78);
    BrMat4MulVec3(wld, iVar5, param_1 + 0xa0);
    wld[2] = svB[2];
    BrMat4MulVec3Transposed((void *)(param_1 + 0xa0), iVar5, wld);
    BrMat4MulVec3(wld, iVar5, param_1 + 0x84);
    wld[0] = svB[0];
    wld[1] = svB[1];
    BrMat4MulVec3Transposed((void *)(param_1 + 0x84), iVar5, wld);
  }
  fVar1 = g.sideForce;
  if (g.sideForce < _DAT_10077a78) {
    fVar1 = -g.sideForce;
  }
  if (fVar1 > _DAT_10077ac8) {
    if (g.sideForce == _DAT_10077a78) {
      fVar1 = _DAT_10077a78;
    } else if (g.sideForce > _DAT_10077a78) {
      fVar1 = _DAT_10077a7c;
    } else {
      fVar1 = _DAT_10077a80;
    }
    g.sideForce = fVar1 * _DAT_10077ac8;
  }
  fVar1 = (g.sideForce + g.sideForce) * _DAT_10077ad0;
  fVar7 = *(float *)(param_1 + 0x1d4) - fVar1;
  if (fVar7 < _DAT_10077a78) {
    fVar7 = -fVar7;
  }
  if (!(fVar7 < _DAT_10077ad4)) {
    if (*(float *)(param_1 + 0x1d4) < fVar1) {
      *(float *)(param_1 + 0x1d4) = *(float *)(param_1 + 0x1d4) - _DAT_10077ad8;
      return;
    }
    fVar1 = *(float *)(param_1 + 0x1d4) - _DAT_10077ad4;
  }
  *(float *)(param_1 + 0x1d4) = fVar1;
  return;
}

#endif /* BR_MATCHING_BUILD */
