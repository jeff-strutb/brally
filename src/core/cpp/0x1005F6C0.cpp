/* WHAT IT DOES: for this car, score every driver slot by wrapped along-track
 * delta plus world distance, sort the others by that score, then for each of
 * two groups copy lap, gate and pose from attached cars onto the driver
 * record (the "saving lap/gate" trace) and write them back onto empty slots
 * of the same group (the "restoring lap/gate" trace). */
/* RESIDUE: 1 byte of 2104.  At +0x252 the original spills the group counter
 * from its own register (`xor edx,edx; mov [esp+0x28],edx`, 89 54); ours
 * constant-propagates the zero and stores the zero register (89 5c).  Every
 * other byte, reloc-masked, is identical.
 * Source facts that closed the rest (2026-09-21, from 2076 B / 47 rows):
 *   - C++ member: BrEntSetMatrix is a two-argument thiscall; the C fastcall
 *     shim cost a dummy `xor edx,edx`.
 *   - score loop is positive && chains with one 1e9 store per arm and one
 *     BrVec3Dist call per arm (the compiler cross-jumps the calls).
 *   - the lap delta is written inline, `(float)(carLap - myLap)`; a named int
 *     temp is what produced the "x87 scheduling wall" (fxch st(2) x4).
 *   - the rank / saved-car / slot walks are INDEXED; the pointer walks and
 *     `sub r,4` are the compiler's own strength reduction.
 *   - restore loop is `while (n != 0) { if (i >= count) break; ... }`.
 *   - trace-call arguments are read back from the records, not held in temps.
 *   - tint bytes pass through three int temps (xor/mov al x3), read b,g,r.
 *   - the facing copy is an inlined V3Copy(dst, src) -- strict load/store
 *     interleave because the inlined pointers may alias.
 *   - zero-velocity arm first; qsort swap writes key before clearing index.
 * Dead for the last byte (13 probes): for-loop forms, store at loop top,
 * chained assignment, goto entry, unsigned either side, volatile slot,
 * address-taken slot, single compiler-spilled variable (rotates everything),
 * slot-first init. */
/* @t4-pass 0x1005F6C0 5 2026-09-21 probes 58 bytes 2104 insns 562 regions 1 rows 0 census yes */
/* @t4-pass 0x1005F6C0 6 2026-09-21 probes 13 bytes 2104 insns 562 regions 1 rows 0 census no */
/* @t3 0x1005F6C0 2026-09-21 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 2104/2104 insns 562/562 rows 0+0 regions 1 oracle EQUIVALENT
 * @t3-effort passes 2 zero-movement 5 6
 * Residue is one register choice in one store (see RESIDUE above); A5 oracle
 * EQUIVALENT on the C++ object.  Passes 1-4 are in the git history of
 * src/core/racing/br_lapsave.c (C lane, 2076 B). */
/* @implements 0x1005F6C0 glide BrLapSaveRestore
 * @cpp_symbol ?LapSaveRestore@BrCar@@QAEXXZ */
#define _CRTIMP __declspec(dllimport)
#include <stdint.h>
#include <stdlib.h>

class BrCar {
public:
    void LapSaveRestore();
    void Sub10062C50();
    void InitTables();
    void SetPos(float x, float y, float z);
    void SetVel(float x, float y, float z);
    void SetMatrix(void *pSrc);
};
#define pCar ((uint8_t *)this)

extern "C" {
extern int DAT_106eed48;
extern int DAT_100b2f00;
extern int DAT_100a9360;
extern int DAT_100b3858;
extern int DAT_10af07f8;
extern int DAT_10af0858;
extern char DAT_10af085c;
extern int DAT_10af0860;
extern char DAT_10af086c;
extern char DAT_10af0800;
extern char DAT_10af0804;
extern char DAT_10af0808;
extern char DAT_10af080c;
extern char DAT_10af0810;
extern char DAT_10af0814;
extern char DAT_10af0818;
extern char DAT_10af081c;
extern char DAT_10af0820;
extern char DAT_10af0824;
extern char DAT_10af0828;
extern char DAT_10af082c;
extern char DAT_10af0830;
extern char DAT_10af0834;
extern char DAT_10af0838;
extern char DAT_10af083c;
extern char DAT_10af0840;
extern char DAT_10af0844;
extern char DAT_10af0848;
extern char DAT_10af084c;
extern char DAT_10af0854;
extern char DAT_10af0855;
extern char DAT_10af0856;
extern int DAT_10af07fc;
extern int DAT_10af1208;
extern double DAT_10077990;

int BrRankCmpKey(const void *pA, const void *pB);
void BrPodNop(const char *fmt, int a, int b, int c, int d);
float BrVec3Dist(void *pA, void *pB);
void BrVec3Sub(void *pOut, void *pA, void *pB);
void BrVec3Direction(void *pOut, void *pA, void *pB);
void BrVec3Midpoint(void *pOut, void *pA, void *pB);
void BrVec3Cross(void *pOut, void *pA, void *pB);
void BrVec3NormaliseGuard(void *pV);
struct BrV3 { float x, y, z; };
}
inline void V3Copy(float *d, float *s) { d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; }
extern "C" {
void BrModelSlotApply(uint8_t *pCar_, void *pDrv);
}

void BrCar::LapSaveRestore()
{
  float *pfVar1;
  unsigned int uTintB;
  unsigned int uTintG;
  unsigned int uTintR;
  int iVar6;
  uint8_t *pfVar7;
  int bVar8;
  int iVar10;
  int iVar11;
  float *pfVar12;
  int iVar13;
  int *piVar14;
  int iVar15;
  int *puVar16;
  float local_d8;
  union { float f; int i; } local_d4;
  int local_d0;
  int local_cc;
  int local_c0;
  char local_b8[12];
  char local_ac[12];
  float local_a0[40];

  local_d4.f = *(float *)(DAT_106eed48 + 100);
  local_d0 = 0;
  if (DAT_100b2f00 <= 0) {
  } else {
    pfVar12 = local_a0;
    piVar14 = (int *)&DAT_10af085c;
    do {
      iVar10 = piVar14[-1];
      if (iVar10 != 0) {
        if ((iVar10 != (int)pCar) && (*(int *)(iVar10 + 0x140) < DAT_100b3858)) {
          *pfVar12 = 1e+10f;
        }
        else if (((DAT_100a9360 == 0) &&
                 (*(int *)(iVar10 + 0x140) >= DAT_100b3858)) &&
                (((*(unsigned char *)(*(int *)(iVar10 + 0xf00) + 0x68) & 2) != 0 &&
                 ((*(unsigned char *)(iVar10 + 0x29af) == 2 &&
                  (*(float *)(iVar10 + 0x29b0) == 0.0f)))))) {
          *pfVar12 = 1e+09f;
        }
        else {
          local_d8 = (*(float *)(pCar + 0xff4) - *(float *)(iVar10 + 0xff4)) +
                     (float)(*(int *)(iVar10 + 0xfac) - *(int *)(pCar + 0xfac)) * local_d4.f;
          if (!(local_d8 <= local_d4.f * 0.5f)) {
            local_d8 = local_d8 - local_d4.f;
          }
          else if (local_d8 < local_d4.f * -0.5f) {
            local_d8 = local_d4.f + local_d8;
          }
          *pfVar12 = local_d8 * local_d8 + BrVec3Dist(pCar + 0x30, (void *)(iVar10 + 0x30));
        }
      }
      else if (((DAT_100a9360 == 0) && (*piVar14 >= DAT_100b3858)) &&
              ((*(unsigned char *)(piVar14 + 1) & 2) != 0)) {
        *pfVar12 = 1e+09f;
      }
      else {
        local_d8 = (*(float *)(pCar + 0xff4) - *(float *)&piVar14[-5]) +
                   (float)(piVar14[-8] - *(int *)(pCar + 0xfac)) * local_d4.f;
        if (!(local_d8 <= local_d4.f * 0.5f)) {
          local_d8 = local_d8 - local_d4.f;
        }
        else if (local_d8 < local_d4.f * -0.5f) {
          local_d8 = local_d4.f + local_d8;
        }
        *pfVar12 = local_d8 * local_d8 + BrVec3Dist(pCar + 0x30, piVar14 + -0x19);
      }
      *(int *)(pfVar12 + 1) = local_d0;
      local_d0 = local_d0 + 1;
      piVar14 = piVar14 + 0x20;
      pfVar12 = pfVar12 + 2;
    } while (local_d0 < DAT_100b2f00);
  }
  if (DAT_100b2f00 <= 1) {
  } else {
    iVar10 = *(int *)(pCar + 0x140);
    local_d8 = local_a0[iVar10 * 2];
    local_a0[iVar10 * 2] = local_a0[0];
    *(int *)(local_a0 + 1) = iVar10;
    local_a0[0] = local_d8;
    *(int *)(local_a0 + iVar10 * 2 + 1) = 0;
    qsort(local_a0 + 2, DAT_100b2f00 - 1, 8, BrRankCmpKey);
  }
  iVar10 = 0;
  local_c0 = iVar10;
  do {
    iVar11 = 0;
    bVar8 = 1;
    local_d0 = 0;
    local_d4.i = 0;
    if (DAT_100b2f00 <= 0) {
    } else {
      do {
        puVar16 = &DAT_10af07f8 + *(int *)(local_a0 + local_d0 * 2 + 1) * 0x20;
        if (puVar16[0x1d] == iVar10) {
          iVar13 = puVar16[0x19];
          iVar6 = DAT_100b3858;
          if (iVar13 < iVar6) {
          } else if (local_d4.i < 1) {
            local_d4.i = local_d4.i + 1;
            iVar15 = puVar16[0x18];
            if (iVar15 != 0) {
              bVar8 = 0;
              if (((*((unsigned char *)puVar16 + 0x68) & 2) != 0) &&
                 ((*(unsigned char *)(iVar15 + 0x29af) == 2 &&
                  (*(float *)(iVar15 + 0x29b0) == 0.0f)))) {
LAB_save:
                puVar16[0] = *(int *)(iVar15 + 0x30);
                puVar16[1] = *(int *)(iVar15 + 0x34);
                puVar16[2] = *(int *)(iVar15 + 0x38);
                puVar16[6] = *(int *)(iVar15 + 0x1024);
                puVar16[7] = *(int *)(iVar15 + 0x1028);
                puVar16[8] = *(int *)(iVar15 + 0x102c);
                puVar16[9] = *(int *)(iVar15 + 0x1030);
                puVar16[10] = *(int *)(iVar15 + 0xf8c);
                puVar16[11] = *(int *)(iVar15 + 0xf90);
                puVar16[12] = *(int *)(iVar15 + 0xfb0);
                puVar16[13] = *(int *)(iVar15 + 0xfe4);
                puVar16[14] = *(int *)(iVar15 + 0xfec);
                puVar16[15] = *(int *)(iVar15 + 0xff0);
                puVar16[16] = *(int *)(iVar15 + 0xfa8);
                puVar16[17] = *(int *)(iVar15 + 0xfac);
                puVar16[18] = *(int *)(iVar15 + 0xfa0);
                puVar16[19] = *(int *)(iVar15 + 0xfa4);
                BrPodNop("saving lap (%d/%d) and gate (%d/%d)\n",
                         puVar16[16], puVar16[17], puVar16[18], puVar16[19]);
                iVar6 = puVar16[0x18];
                puVar16[20] = *(int *)(iVar15 + 0xff4);
                puVar16[21] = *(int *)(iVar15 + 0xff8);
                iVar10 = local_c0;
                (&local_cc)[iVar11] = iVar6;
                iVar11 = iVar11 + 1;
                puVar16[0x18] = 0;
                *(int *)(iVar15 + 0xf00) = 0;
                *(int *)(iVar15 + 0xf04) = 0x3c;
              }
            }
          }
          else {
            iVar15 = puVar16[0x18];
            if (iVar15 != 0) {
              bVar8 = 0;
              if ((*(int *)(iVar15 + 0xf04) == 0) && (DAT_10077990 < local_a0[local_d0 * 2]))
                goto LAB_save;
            }
          }
        }
        local_d0 = local_d0 + 1;
      } while (local_d0 < DAT_100b2f00);
    }
    if (bVar8) {
      (&local_cc)[iVar11] = (int)(&DAT_10af1208 + (iVar10 + DAT_100b3858) * 0xada);
      iVar11 = iVar11 + 1;
    }
    local_d0 = 0;
    while (iVar11 != 0) {
      {
        if (local_d0 >= DAT_100b2f00) break;
        puVar16 = &DAT_10af07f8 + *(int *)(local_a0 + local_d0 * 2 + 1) * 0x20;
        if (((puVar16[0x1d] == iVar10) &&
            (!(puVar16[0x19] < DAT_100b3858))) &&
           (((*((unsigned char *)puVar16 + 0x68) & 2) == 0 &&
            (puVar16[0x18] == 0)))) {
          iVar11 = iVar11 + -1;
          pfVar7 = (uint8_t *)(&local_cc)[iVar11];
          puVar16[0x18] = (int)pfVar7;
          *(int *)(pfVar7 + 0x1030) = puVar16[9];
          *(int *)(pfVar7 + 0xf8c) = puVar16[10];
          *(int *)(pfVar7 + 0xf90) = puVar16[11];
          *(int *)(pfVar7 + 0xfb0) = puVar16[12];
          *(int *)(pfVar7 + 0xfe4) = puVar16[13];
          *(int *)(pfVar7 + 0xfec) = puVar16[14];
          *(int *)(pfVar7 + 0xff0) = puVar16[15];
          *(int *)(pfVar7 + 0xfa8) = puVar16[16];
          *(int *)(pfVar7 + 0xfac) = puVar16[17];
          *(int *)(pfVar7 + 0xfa0) = puVar16[18];
          *(int *)(pfVar7 + 0xfa4) = puVar16[19];
          BrPodNop("restoring lap (%d/%d) and gate (%d/%d)\n",
                   *(int *)(pfVar7 + 0xfa8),
                   *(int *)(pfVar7 + 0xfac), *(int *)(pfVar7 + 0xfa0), *(int *)(pfVar7 + 0xfa4));
          *(int *)(pfVar7 + 0xff4) = puVar16[20];
          *(int *)(pfVar7 + 0xff8) = puVar16[21];
          *(int **)(pfVar7 + 0xf00) = puVar16;
          pfVar7[0x29af] = 0;
          *(float *)(pfVar7 + 0x29b0) = 1.0f;
          *(int *)(pfVar7 + 0xf78) = 1;
          ((BrCar *)pfVar7)->Sub10062C50();
          ((BrCar *)pfVar7)->InitTables();
          *(int *)(pfVar7 + 0xf80) = puVar16[3];
          *(int *)(pfVar7 + 0xf84) = puVar16[4];
          *(int *)(pfVar7 + 0xf88) = puVar16[5];
          ((BrCar *)pfVar7)->SetPos(
                      *(float *)puVar16,
                      *(float *)(puVar16 + 1),
                      *(float *)(puVar16 + 2) - -0.1f);
          pfVar7[0x29af] = 0;
          *(float *)(pfVar7 + 0x29b0) = 1.0f;
          uTintB = *((unsigned char *)puVar16 + 0x5e);
          uTintG = *((unsigned char *)puVar16 + 0x5d);
          uTintR = *((unsigned char *)puVar16 + 0x5c);
          pfVar7[0x29ac] = (uint8_t)uTintR;
          pfVar7[0x29ad] = (uint8_t)uTintG;
          pfVar7[0x29ae] = (uint8_t)uTintB;
          BrModelSlotApply(pfVar7, puVar16);
          iVar10 = *(int *)(pfVar7 + 0xf8c) + *(int *)(pfVar7 + 0xf90) * 0x28;
          BrVec3Sub(local_b8, (void *)(iVar10 + 0x40), (void *)(iVar10 + 0x4c));
          iVar10 = *(int *)(pfVar7 + 0xf8c) + *(int *)(pfVar7 + 0xf90) * 0x28;
          BrVec3Sub(local_ac, (void *)(iVar10 + 0x68), (void *)(iVar10 + 0x74));
          iVar10 = *(int *)(pfVar7 + 0xf8c) + *(int *)(pfVar7 + 0xf90) * 0x28;
          BrVec3Direction(pfVar7, (void *)(iVar10 + 0x4c), (void *)(iVar10 + 0x74));
          pfVar1 = (float *)(pfVar7 + 0x10);
          BrVec3Midpoint(pfVar1, local_b8, local_ac);
          BrVec3Cross((float *)(pfVar7 + 0x20), pfVar7, pfVar1);
          BrVec3NormaliseGuard(pfVar7 + 0x20);
          BrVec3Cross(pfVar1, (float *)(pfVar7 + 0x20), pfVar7);
          BrVec3NormaliseGuard(pfVar1);
          V3Copy((float *)(pfVar7 + 0xf94), (float *)pfVar7);
          ((BrCar *)pfVar7)->SetMatrix(pfVar7);
          if ((*(unsigned char *)(*(int *)(pfVar7 + 0xf00) + 0x68) & 1) != 0) {
            ((BrCar *)pfVar7)->SetVel( 0.0f, 0.0f, 0.0f);
          }
          else {
            ((BrCar *)pfVar7)->SetVel(
                        *(float *)pfVar7 * 50.0f,
                        *(float *)(pfVar7 + 4) * 50.0f,
                        *(float *)(pfVar7 + 8) * 50.0f);
          }
          *(int *)(*(int *)(pfVar7 + 0x168) + 0x19c) = 0;
          *(int *)(*(int *)(pfVar7 + 0x168) + 0x1b4) = 0;
          *(unsigned char *)(*(int *)(pfVar7 + 0x168) + 0x1a0) = 2;
          *(int *)(*(int *)(pfVar7 + 0x16c) + 0x19c) = 0;
          *(int *)(*(int *)(pfVar7 + 0x16c) + 0x1b4) = 0;
          *(unsigned char *)(*(int *)(pfVar7 + 0x16c) + 0x1a0) = 2;
          *(int *)(*(int *)(pfVar7 + 0x174) + 0x19c) = 0;
          *(int *)(*(int *)(pfVar7 + 0x174) + 0x1b4) = 0;
          *(unsigned char *)(*(int *)(pfVar7 + 0x174) + 0x1a0) = 2;
          *(int *)(*(int *)(pfVar7 + 0x170) + 0x19c) = 0;
          *(int *)(*(int *)(pfVar7 + 0x170) + 0x1b4) = 0;
          *(unsigned char *)(*(int *)(pfVar7 + 0x170) + 0x1a0) = 2;
          *(int *)(pfVar7 + 0xea0) = 0;
          *(int *)(pfVar7 + 0xea4) = 0;
          *(int *)(pfVar7 + 0xea8) = 0;
          *(int *)(pfVar7 + 0xeac) = 0xffffff4c;
          *(int *)(pfVar7 + 0x35c) = 0x28;
          *(int *)(pfVar7 + 0xe20) = 0;
          iVar10 = local_c0;
        }
        *(int *)(pCar + 0xeb0 + local_d0 * 4) = (int)(&DAT_10af07f8 + *(int *)(local_a0 + local_d0 * 2 + 1) * 0x20);
        local_d0 = local_d0 + 1;
      }
    }
    iVar10 = iVar10 + 1;
    local_c0 = iVar10;
  } while (iVar10 < 2);
}

