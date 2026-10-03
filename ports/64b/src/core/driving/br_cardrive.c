#include "br_collrespsolve.h"   /* br_globals: its objects */
/* br_cardrive.c -- 0x100645A0, the axle velocity constraint.
 *
 * Matching twin of the port's BrCarPhysDrive.  The original is six arguments
 * (body, dt, and the four car+0xE74..0xE80 scalars); the port folds those
 * into the car object.  This file is the matching build only.
 */

/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)

/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* BrRbVelAtPoint: prototype in br_funcs.h */
/* 64-bit core: declared once, by its definition's header */
/* 64-bit core: declared once, by its definition's header */
/* BrSqrtF: prototype in br_funcs.h */
/* BrCosF: prototype in br_funcs.h */
/* BrSinF: prototype in br_funcs.h */

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
/* @t3 0x100645A0 2026-09-23 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 3105/3070 insns 864/862 rows 30+32 regions 20 oracle EQUIVALENT
 * @t3-effort passes 2 zero-movement 1 2
 * A5 EQUIV-MODULO-FP (insn gap 1; residue is x87 scheduling -- fcom/fcomp,
 * fsub st vs fsubr, the sideForce/ran float+int aggregate -- see the header
 * above; byte-exact is that colouring wall).  Behaviourally verified with a
 * seeded car object graph (param_1 -> car with 4 axle records, valid geometry
 * so the axle-difference divisions are finite, param_2 nonzero divisor);
 * negative-controlled: an integer flag write and a velocity-solve term (svB[0])
 * both DIFF, correct code stays equivalent.  Do not reopen before the
 * end-grind. */
/* @implements 0x100645A0 glide BrCarPhysDriveMatch */
void BrCarPhysDriveMatch(BrCarBody *param_1, float param_2, float *param_3, float *param_4, char *param_5, char *param_6)
{
  BrRbBody *pR_iVar5;
  BrRbBody *pR_iVar9;
  float fVar1;
  float fVar2;
  float speed;
  unsigned char bVar3;
  unsigned char bVar4;
  BrMat4 *iVar5;
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
  double q;
  int dotBits;
  int sinBits;   /* pt[1] read back through its float slot */
  struct {
    float sideForce;
    int ran;
  } g;

  g.sideForce = 0.0f;
  g.ran = 0;
  if ((param_1->rb.child[0]->pPlane) == g.ran) {
    (*(int *)&param_1->rb.child[0]->f1B4) = g.ran;
  }
  if ((param_1->rb.child[1]->pPlane) == g.ran) {
    (*(int *)&param_1->rb.child[1]->f1B4) = g.ran;
  }
  if ((param_1->rb.child[2]->pPlane) == g.ran) {
    (*(int *)&param_1->rb.child[2]->f1B4) = g.ran;
  }
  if ((param_1->rb.child[3]->pPlane) == g.ran) {
    (*(int *)&param_1->rb.child[3]->f1B4) = g.ran;
  }
  pR_iVar5 = param_1->rb.child[2];
  if ((*(float *)&pR_iVar5->f1D0) < BrCrK_Zero) {
    fVar1 = -(*(float *)&pR_iVar5->f1D0);
  } else {
    fVar1 = (*(float *)&pR_iVar5->f1D0);
  }
  if ((*(float *)&pR_iVar5->f1C4) == BrCrK_Zero) {
    fVar7 = BrCrK_Zero;
  } else if ((*(float *)&pR_iVar5->f1C4) > BrCrK_Zero) {
    fVar7 = DAT_10077a7c;
  } else {
    fVar7 = DAT_10077a80;
  }
  fVar1 = fVar7 * fVar1 * DAT_10077a84;
  fVar1 = fVar1 / (*(float *)&pR_iVar5->f1C8);
  pR_iVar9 = param_1->rb.child[0];
  if ((*(float *)&pR_iVar9->f1D0) < BrCrK_Zero) {
    fVar2 = -(*(float *)&pR_iVar9->f1D0);
  } else {
    fVar2 = (*(float *)&pR_iVar9->f1D0);
  }
  if ((*(float *)&pR_iVar9->f1C4) == BrCrK_Zero) {
    fVar6 = BrCrK_Zero;
  } else if ((*(float *)&pR_iVar9->f1C4) > BrCrK_Zero) {
    fVar6 = DAT_10077a7c;
  } else {
    fVar6 = DAT_10077a80;
  }
  fVar2 = fVar6 * fVar2 * DAT_10077a84;
  fVar2 = fVar2 / (*(float *)&pR_iVar9->f1C8);
  fVar8 = (*(float *)&param_1->rb.mass) * _DAT_10077a88;
  local_88 = (fVar1 / fVar8) * param_2 * param_2;
  local_8c = (fVar2 / fVar8) * param_2 * param_2;
  if (local_88 < BrCrK_Zero) {
    fVar1 = -local_88;
  } else {
    fVar1 = local_88;
  }
  if (fVar1 > DAT_10077a7c) {
    if (local_88 == BrCrK_Zero) {
      fVar1 = BrCrK_Zero;
    } else if (local_88 > BrCrK_Zero) {
      fVar1 = DAT_10077a7c;
    } else {
      fVar1 = DAT_10077a80;
    }
    local_88 = fVar1 * _DAT_10077a8c;
  }
  if (local_8c < BrCrK_Zero) {
    fVar1 = -local_8c;
  } else {
    fVar1 = local_8c;
  }
  if (fVar1 > DAT_10077a7c) {
    if (local_8c == BrCrK_Zero) {
      fVar1 = BrCrK_Zero;
    } else if (local_8c > BrCrK_Zero) {
      fVar1 = DAT_10077a7c;
    } else {
      fVar1 = DAT_10077a80;
    }
    local_8c = fVar1 * _DAT_10077a8c;
  }
  pt[2] = 0.0f;
  pt[1] = 0.0f;
  pt[0] = (*(float *)&pR_iVar9->st.pos.x);
  BrRbVelAtPoint(tmp, param_1, pt);
  iVar5 = ((void *)&param_1->rb.m.m[0][0]);
  BrMat4MulVec3(vA, iVar5, tmp);
  bVar3 = (*(unsigned char *)&param_1->rb.child[0]->f01A0);
  *(unsigned char *)&local_3c = bVar3;
  bVar4 = (*(unsigned char *)&param_1->rb.child[1]->f01A0);
  *(unsigned char *)&local_80 = bVar4;
  *(unsigned char *)&local_1c = (*(unsigned char *)&param_1->rb.child[2]->f01A0);
  *(unsigned char *)&local_38 = (*(unsigned char *)&param_1->rb.child[3]->f01A0);
  iVar9 = DAT_104b15e8 + -1;
  if ((2 < (short)iVar9) || ((short)iVar9 < 0)) {
    iVar9 = 0;
  }
  local_6c = iVar9 << 3;
  (*(unsigned char *)&param_1->f0209) = 0;
  if ((((*(int *)&param_1->rb.child[0]->f1B4) == 0) &&
      ((*(int *)&param_1->rb.child[1]->f1B4) == 0)) ||
     (((*(int *)&param_1->rb.child[2]->f1B4) == 0 &&
      ((*(int *)&param_1->rb.child[3]->f1B4) == 0)))) {
    *param_5 = '\0';
  }
  else {
    if (vA[1] < BrCrK_Zero) {
      fVar1 = -vA[1];
    } else {
      fVar1 = vA[1];
    }
    if (*param_3 < BrCrK_Zero) {
      fVar7 = -*param_3;
    } else {
      fVar7 = *param_3;
    }
    g.ran = 1;
    *(int *)&hold = 0x45FA0000;
    if (local_88 < BrCrK_Zero) {
      fVar2 = -local_88;
    } else {
      fVar2 = local_88;
    }
    if (!(fVar2 > _DAT_10077a98)) {
      if (local_8c < BrCrK_Zero) {
        fVar2 = -local_8c;
      } else {
        fVar2 = local_8c;
      }
      local_84 = (fVar2 > _DAT_10077a98);
      fVar2 = (float)local_84 * _DAT_10077aa4;
    }
    else {
      if (local_8c < BrCrK_Zero) {
        fVar2 = -local_8c;
      } else {
        fVar2 = local_8c;
      }
      local_84 = (fVar2 > _DAT_10077a98);
      fVar2 = (float)local_84 * _DAT_10077aa0;
    }
    fVar8 = ((*(float *)&param_1->rb.mass) * fVar1) / param_2 + fVar7;
    fVar2 = fVar8 - fVar2;
    if (*param_5 != '\0') {
      hold = _DAT_10077aa8;
    }
    *param_5 = '\0';
    iVar9 = (int)((*(unsigned int *)&local_80 & 0xff) + 1 +
                  (*(unsigned int *)&local_3c & 0xff)) >> 1;
    iVar10 = (iVar9 + (int)(short)local_6c) * 4;
    local_3c = *(float *)((const char *)g_brCrPlane.pDrvT1 +
                         ((int)(short)local_6c +
                          (unsigned int)(*(unsigned char *)&param_1->gripClass) * 0x18 +
                          iVar9) * 4);
    local_84 = *(int *)(&(*(char *)&g_aBrCarPhysDrvT3) + iVar10);
    fVar7 = *(float *)((const char *)g_brCrPlane.pDrvT2 + iVar10);
    /* !! `speed` (fVar8 - fVar2 above) survives the clamp: the original keeps
     * it on the x87 stack (`fld st(2)` at +0x484 clamps a COPY) and tests
     * THAT against `hold` at +0x4FE.  Reusing one variable for the clamped
     * value and the ratio made the hold test compare the ratio (~0.3)
     * against 8000 -- the grip branch never ran.  Live oracle, frame 352. */
    speed = fVar2;
    /* The grip factor stays on the FPU: limit / clamped speed, times the
     * surface grip and the scale, is fst'd to local_80 and carried on
     * unrounded -- into the x1.5 (a DOUBLE constant) and into the |.| > 1
     * test.  Live oracle: 2-ulp local_80 differences (AI car, quick race). */
    {
      double r = speed;
      int bits;
      if (r > fVar7) {
        r = fVar7;
      }
      if (!(r >= *(float *)&local_84)) {
        r = *(float *)&local_84;
      }
      r = *(float *)&local_84 / r;
      r = r * local_3c * _DAT_10077aac;
      local_80 = (float)r;
      if ((*(float *)&param_1->rb.child[2]->f1C0) == BrCrK_Zero) {
        r = r * _DAT_10077ab0;
        local_80 = (float)r;
      }
      bits = *(int *)&local_80;
      local_80 = *(float *)&bits;
      if ((r < BrCrK_Zero ? -r : r) > DAT_10077a7c) {
        local_80 = 1.0f;
      }
    }
    if (!(speed < hold)) {
      /* orig: mov y-bits, mov x-bits, fld z: integer copies of x/y so the
       * squares go through stack slots, not fld [body+0x84]. */
      local_84 = (*(int *)&param_1->rb.st.vel.y);
      local_x = (*(int *)&param_1->rb.st.vel.x);
      fVar11 = (*(float *)&param_1->rb.st.vel.z);
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
      vA[1] = BrCrK_Zero;
    }
    if (vA[0] < BrCrK_Zero) {
      fVar1 = -vA[0];
    } else {
      fVar1 = vA[0];
    }
    if (fVar1 > DAT_10077a7c) {
      fVar1 = vA[1] / vA[0];
      if (fVar1 < BrCrK_Zero) {
        fVar1 = -fVar1;
      }
      if (fVar1 > _DAT_10077a88) {
        (*(unsigned char *)&param_1->f0209) = 0x80;
      }
    } else {
      if (vA[1] < BrCrK_Zero) {
        fVar1 = -vA[1];
      } else {
        fVar1 = vA[1];
      }
      if (fVar1 > DAT_10077a7c) {
        (*(unsigned char *)&param_1->f0209) = 0x80;
      } else {
        (*(unsigned char *)&param_1->f0209) = 0;
      }
    }
    fVar2 = vA[0];
    vA[0] = vA[0] - local_8c;
    if (vA[0] < BrCrK_Zero) {
      fVar1 = -vA[0];
    } else {
      fVar1 = vA[0];
    }
    if (fVar1 > _DAT_10077ac0) {
      if (vA[0] == BrCrK_Zero) {
        fVar1 = BrCrK_Zero;
      } else if (vA[0] > BrCrK_Zero) {
        fVar1 = DAT_10077a7c;
      } else {
        fVar1 = DAT_10077a80;
      }
      if (fVar2 == BrCrK_Zero) {
        fVar7 = BrCrK_Zero;
      } else if (fVar2 > BrCrK_Zero) {
        fVar7 = DAT_10077a7c;
      } else {
        fVar7 = DAT_10077a80;
      }
      if (fVar1 != fVar7) {
        vA[0] = 0.0f;
      }
    }
  }
  pt[0] = (*(float *)&param_1->rb.child[2]->st.pos.x);
  pt[1] = 0.0f;
  pt[2] = 0;
  BrRbVelAtPoint(tmpB, param_1, pt);
  BrMat4MulVec3(vB, iVar5, tmpB);
  if ((((*(int *)&param_1->rb.child[2]->f1B4) != 0) ||
      ((*(int *)&param_1->rb.child[3]->f1B4) != 0)) &&
     (((*(int *)&param_1->rb.child[0]->f1B4) != 0 ||
      ((*(int *)&param_1->rb.child[1]->f1B4) != 0)))) {
    g.ran = 1;
    pt[0] = BrCosF((*(float *)&param_1->rb.child[2]->f1C0));
    lat[2] = vB[2];
    pt[1] = BrSinF((*(float *)&param_1->rb.child[2]->f1C0));
    svB[2] = vB[2];
    svB[0] = vB[0];
    /* The original `fst`s the dot product to a float slot and reloads it:
     * the cos term is taken from the UNROUNDED register copy, the sin term
     * from the rounded reload.  The double is the register; the int image
     * forces the float rounding VC5 would otherwise forward away.  Live
     * oracle, 1-ulp difference in vB[1]. */
    /* sin is stored (0x10064D3A) and every use reloads the float; VC5
     * would multiply by the unrounded return register (whole-image run,
     * quick race finish frame 1069: lat[0] off, then the grip factor) */
    sinBits = *(int *)&pt[1];
    q = (double)vB[1] * *(float *)&sinBits + (double)vB[0] * pt[0];
    fVar7 = (float)(q * pt[0]);
    fVar1 = (float)q;
    dotBits = *(int *)&fVar1;
    fVar1 = *(float *)&dotBits * *(float *)&sinBits;
    /* lat[] below subtracts the STORED vB[0]/vB[1] (rounded floats). */
    dotBits = *(int *)&fVar7;
    fVar7 = *(float *)&dotBits;
    dotBits = *(int *)&fVar1;
    fVar1 = *(float *)&dotBits;
    svB[1] = vB[1];
    vB[2] = 0.0f;
    lat[0] = vB[0] - fVar7;
    pt[2] = 0;
    lat[1] = vB[1] - fVar1;
    lat[2] = lat[2] - vB[2];
    if (!(*param_4 < BrCrK_Zero)) {
      local_8c = *param_4;
    }
    else {
      local_8c = -*param_4;
    }
    vB[0] = fVar7;
    vB[1] = fVar1;
    fVar11 = BrSqrtF(lat[2] * lat[2] + lat[1] * lat[1] + lat[0] * lat[0]);
    if (local_88 < BrCrK_Zero) {
      local_88 = -local_88;
    }
    local_88 = (float)(int)(local_88 > _DAT_10077a98);
    /* The original stores the sum to its float slot (fstp at 0x10064E35)
     * and subtracts from the stored value; the int image forces that
     * rounding.  Whole-image run: 1 ulp in the speed, then in the grip
     * factor and the lateral velocity. */
    fVar1 = (fVar11 * (*(float *)&param_1->rb.mass)) / param_2 + local_8c;
    dotBits = *(int *)&fVar1;
    fVar1 = *(float *)&dotBits - local_88 * _DAT_10077aa0;
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
      local_38 = *(float *)((const char *)g_brCrPlane.pDrvT1 +
                           (iVar9 + (unsigned int)(*(unsigned char *)&param_1->gripClass) * 0x18 +
                            (int)(short)local_6c) * 4);
      local_84 = *(int *)(&(*(char *)&g_aBrCarPhysDrvT3) + iVar10);
      local_80 = *(float *)((const char *)g_brCrPlane.pDrvT2 + iVar10);
      /* as the front axle: the grip factor chain stays unrounded */
      {
        double r;
        int bits;
        bits = *(int *)&fVar1;
        r = *(float *)&bits;                       /* the stored speed */
        if (r > local_80) {
          r = local_80;
        }
        if (!(r >= *(float *)&local_84)) {
          r = *(float *)&local_84;
        }
        r = *(float *)&local_84 / r;
        r = r * local_38 * _DAT_10077aac;
        local_80 = (float)r;
        if ((*(float *)&param_1->rb.child[2]->f1C0) == BrCrK_Zero) {
          r = r * _DAT_10077ab0;
          local_80 = (float)r;
        }
        bits = *(int *)&local_80;
        local_80 = *(float *)&bits;
        if ((r < BrCrK_Zero ? -r : r) > DAT_10077a7c) {
          local_80 = 1.0f;
        }
      }
      local_84 = (*(int *)&param_1->rb.st.vel.y);
      local_x = (*(int *)&param_1->rb.st.vel.x);
      fVar11 = (*(float *)&param_1->rb.st.vel.z);
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
    svB[0] = (vA[0] + vB[0]) * BrCrK_Half;
    /* The original `fst`s the yaw-rate quotient and goes on from the x87
     * register: the lateral term below uses the UNROUNDED quotient, not the
     * float just stored.  The double models that register.  Live oracle,
     * 1-ulp difference in the lateral velocity handed back to the body. */
    q = ((double)vA[1] - (double)vB[1]) /
        ((double)(*(float *)&param_1->rb.child[0]->st.pos.x) -
         (double)(*(float *)&param_1->rb.child[2]->st.pos.x));
    svB[2] = (float)q;
    svB[1] = (float)((double)vA[1] - q * (double)(*(float *)&param_1->rb.child[0]->st.pos.x));
    BrMat4MulVec3(wld, iVar5, ((void *)&param_1->rb.st.angVel.x));
    wld[2] = svB[2];
    BrMat4MulVec3Transposed((void *)(((void *)&param_1->rb.st.angVel.x)), iVar5, wld);
    BrMat4MulVec3(wld, iVar5, ((void *)&param_1->rb.st.vel.x));
    wld[0] = svB[0];
    wld[1] = svB[1];
    BrMat4MulVec3Transposed((void *)(((void *)&param_1->rb.st.vel.x)), iVar5, wld);
  }
  fVar1 = g.sideForce;
  if (g.sideForce < BrCrK_Zero) {
    fVar1 = -g.sideForce;
  }
  if (fVar1 > BrCrK_Half) {
    if (g.sideForce == BrCrK_Zero) {
      fVar1 = BrCrK_Zero;
    } else if (g.sideForce > BrCrK_Zero) {
      fVar1 = DAT_10077a7c;
    } else {
      fVar1 = DAT_10077a80;
    }
    g.sideForce = fVar1 * BrCrK_Half;
  }
  fVar1 = (g.sideForce + g.sideForce) * DAT_10077ad0;
  fVar7 = (*(float *)&param_1->rb.f1D4) - fVar1;
  if (fVar7 < BrCrK_Zero) {
    fVar7 = -fVar7;
  }
  if (!(fVar7 < _DAT_10077ad4)) {
    if ((*(float *)&param_1->rb.f1D4) < fVar1) {
      (*(float *)&param_1->rb.f1D4) = (*(float *)&param_1->rb.f1D4) - _DAT_10077ad8;
      return;
    }
    fVar1 = (*(float *)&param_1->rb.f1D4) - _DAT_10077ad4;
  }
  (*(float *)&param_1->rb.f1D4) = fVar1;
  return;
}

