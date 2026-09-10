/* br_envenit.c -- matching arm for BrEnvEmit (0x10017110).
 *
 * Track/environment display-list emitter.  The port lives in br_drawenv.c
 * behind #ifndef BR_MATCHING_BUILD; this file is the matching body.
 */
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)

typedef struct BrMat4 { float m[4][4]; } BrMat4;

int FUN_10008d60();
void FUN_1001cf90(unsigned int *, int, int, int, int, int, int, int, int,
                  int, int, int, int, int, int, int, int);
void FUN_10034b70(BrMat4 *, int);
void FUN_10034af0(BrMat4 *, BrMat4 *, BrMat4 *);
void FUN_100349c0(float *, float *, BrMat4 *);
void FUN_100344d0(float *);
void FUN_100597f0(void *, unsigned int, int);

extern unsigned int *DAT_106e7710;
extern int DAT_106ed6b0;
extern int DAT_106ed6b4;
extern int DAT_106e8a18;
extern unsigned short DAT_106ed528;
extern int DAT_106eed38;
extern int DAT_106ec798;
extern int DAT_106e72e8;
extern unsigned int DAT_1184c460;
extern unsigned int DAT_1184c478;
extern unsigned char DAT_104af547;
extern unsigned char DAT_104af586;
extern unsigned char DAT_104af587;
extern unsigned char DAT_104af588;
extern unsigned char DAT_104af589;
extern unsigned char DAT_104af58a;
extern unsigned char DAT_104af5c6;
extern unsigned char DAT_104af5c7;
extern unsigned char DAT_104af5c8;
extern unsigned char DAT_104af5c9;
extern unsigned char DAT_104af5ca;
extern unsigned char DAT_104af606;
extern unsigned char DAT_104af607;
extern unsigned char DAT_104af608;
extern unsigned char DAT_104af609;
extern unsigned char DAT_104af60a;
extern unsigned char DAT_104af647;
extern unsigned char DAT_104af648;
extern unsigned char DAT_104af649;
extern int DAT_106ed520;
extern BrMat4 DAT_106e78f0;
extern BrMat4 DAT_106e7930;
extern float DAT_104b15d0;
extern int DAT_106ea3f4;
extern int DAT_106e8204;
extern float DAT_10077344;
extern float DAT_10077348;
extern float DAT_1007734c;
extern float DAT_10077350;
extern float DAT_10077354;
extern float DAT_10077304;
extern float DAT_10077314;
extern float DAT_10077364;
extern float DAT_10077368;
extern BrMat4 DAT_106e72a8;
extern int DAT_104add38;
extern short DAT_104add54;
extern int DAT_100a7514;
extern int DAT_100a7518;

/* Residue: insn gap 22, mset 71+49, 46 unpaired.  Frame is sub esp,0x2c.
 * Scale matrix is c705 immediates.  Inner x87 is stack-index/fxch plus
 * fcomp-mem vs fcomp-st (test ah,1 vs 0x41).  Tile pack folds to
 * shl/and/or.  ScaleA/B live on the x87 stack (fld/fstp) instead of
 * mov-imm to [esp].  n64 BrEnvEmit MISS.  21 probes, none moved
 * REGNORM 32+54.
 */
/* WHAT IT DOES: emit the per-frame track-surface display list -- pipe/combiner
 * preamble, texture bind, prim colour, a 64x64 projected visibility bitmap
 * stamped along the section light, and one tile command per track segment. */
/* @t4-pass 0x10017110 1 2026-09-09 probes 11 bytes 1950 insns 476 regions 13 rows 120 census yes  (fn.py: commute, scale-int pun, recast, decl reverse; corpus +0xb MISS) */
/* @t4-pass 0x10017110 2 2026-09-09 probes 10 bytes 1950 insns 476 regions 13 rows 120 census yes  (fn.py: xor/!=, combiner zeros, index, fchs, pack parens, stamp bounds) */
/* @implements 0x10017110 glide BrEnvEmit */
void BrEnvEmit(void)
{
  float fVar1;
  float fVar2;
  float fVar3;
  float fVar4;
  float fVar5;
  float scaleA;
  float scaleB;
  float viewW;
  float viewH;
  unsigned int *puVar6;
  unsigned int *puVar7;
  int iVar8;
  int iVar9;
  int iVar10;
  int iVar11;
  int iVar12;
  unsigned char bVar13;
  unsigned short *puVar14;
  int iVar15;
  short *psVar16;
  float local_c[3];

  if (DAT_106ed6b0 || DAT_106ed6b4) {
    iVar11 = DAT_106e8a18;
    for (iVar15 = 0; iVar15 < iVar11; iVar15++) {
      if ((*(unsigned char *)(DAT_106eed38 + 0x4c + (unsigned int)(&DAT_106ed528)[iVar15] * 0x54) & 0x10) != 0) {
        return;
      }
    }
    iVar15 = 0;
    iVar11 = DAT_106ec798;
    psVar16 = &DAT_104add54 + iVar11 * 0x61e;
    if (DAT_106ed6b4) {
      FUN_10008d60();
    }
    puVar7 = DAT_106e7710;
    puVar6 = DAT_106e7710 + 2;
    *DAT_106e7710 = 0xe7000000;
    DAT_106e7710 = puVar6;
    puVar7[1] = iVar15;
    puVar7 = DAT_106e7710;
    puVar6 = DAT_106e7710 + 2;
    *DAT_106e7710 = 0xba001402;
    DAT_106e7710 = puVar6;
    puVar7[1] = iVar15;
    puVar7 = DAT_106e7710;
    puVar6 = DAT_106e7710 + 2;
    *DAT_106e7710 = 0xbb000001;
    DAT_106e7710 = puVar6;
    puVar7[1] = 0xffffffff;
    puVar7 = DAT_106e7710;
    puVar6 = DAT_106e7710 + 2;
    *DAT_106e7710 = 0xba000c02;
    DAT_106e7710 = puVar6;
    puVar7[1] = DAT_106e72e8;
    puVar7 = DAT_106e7710;
    DAT_106e7710 = DAT_106e7710 + 2;
    FUN_1001cf90(puVar7, 0, 0, 0, 0x3eb, 0, 0, 0, 0x3e9, 0, 0, 0, 0x3eb, 0, 0, 0, 0x3e9);
    puVar7 = DAT_106e7710;
    puVar6 = DAT_106e7710 + 2;
    *DAT_106e7710 = 0xb900031d;
    DAT_106e7710 = puVar6;
    puVar7[1] = 0x504240;
    puVar7 = DAT_106e7710;
    if (DAT_106ed6b4 == 0) {
      puVar6 = DAT_106e7710 + 2;
      *DAT_106e7710 = DAT_1184c478 & 0xffffff | 0xdc000000;
      DAT_106e7710 = puVar6;
    } else {
      puVar6 = DAT_106e7710 + 2;
      *DAT_106e7710 = *(unsigned int *)((char *)&DAT_1184c460 + DAT_106ec798 * 4) & 0xffffff | 0xdd000000;
      DAT_106e7710 = puVar6;
      puVar7[1] = (unsigned int)(&DAT_104af5c8 + DAT_106ec798 * 0x1000);
      puVar7 = DAT_106e7710;
      puVar6 = DAT_106e7710 + 2;
      *DAT_106e7710 = *(unsigned int *)((char *)&DAT_1184c460 + DAT_106ec798 * 4) & 0xffffff | 0xdc000000;
      DAT_106e7710 = puVar6;
    }
    puVar7[1] = 1;
    puVar7 = DAT_106e7710;
    puVar6 = DAT_106e7710 + 2;
    *DAT_106e7710 = 0xf2002002;
    DAT_106e7710 = puVar6;
    puVar7[1] = 0xfe0fe;
    puVar7 = DAT_106e7710;
    puVar6 = DAT_106e7710 + 2;
    *DAT_106e7710 = 0xb6000000;
    DAT_106e7710 = puVar6;
    puVar7[1] = 0x3000;
    puVar7 = DAT_106e7710;
    if (DAT_106ed6b0 == 0) {
      puVar6 = DAT_106e7710 + 2;
      *DAT_106e7710 = 0xfa00ffff;
      DAT_106e7710 = puVar6;
      puVar7[1] = 0x788088ff;
    } else {
      puVar6 = DAT_106e7710 + 2;
      *DAT_106e7710 = 0xfa00ffff;
      DAT_106e7710 = puVar6;
      puVar7[1] = 0xe0e0ffff;
    }
    FUN_10034b70(&DAT_106e78f0, DAT_106ed520);
    DAT_106e78f0.m[3][2] = 0.0f;
    DAT_106e78f0.m[3][1] = 0.0f;
    DAT_106e78f0.m[3][0] = 0.0f;
    DAT_106e7930.m[0][0] = 0.0f;
    DAT_106e7930.m[0][1] = 0.0f;
    DAT_106e7930.m[0][2] = 6.103701889514923e-05f;
    DAT_106e7930.m[0][3] = 0.0f;
    DAT_106e7930.m[1][0] = 6.103701889514923e-05f;
    DAT_106e7930.m[1][1] = 0.0f;
    DAT_106e7930.m[1][2] = 0.0f;
    DAT_106e7930.m[1][3] = 0.0f;
    DAT_106e7930.m[2][0] = 0.0f;
    DAT_106e7930.m[2][1] = 6.103701889514923e-05f;
    DAT_106e7930.m[2][2] = 0.0f;
    DAT_106e7930.m[2][3] = 0.0f;
    DAT_106e7930.m[3][0] = 0.0f;
    DAT_106e7930.m[3][1] = 0.0f;
    DAT_106e7930.m[3][2] = 0.0f;
    DAT_106e7930.m[3][3] = 1.0f;
    FUN_10034af0(&DAT_106e78f0, &DAT_106e78f0, &DAT_106e7930);
    if (DAT_106ed6b4) {
      FUN_100597f0(&DAT_104af5c8 + DAT_106ec798 * 0x1000, 0x1000, 0);
      FUN_100349c0(local_c, &DAT_104b15d0 + DAT_106ec798 * 3, &DAT_106e78f0);
      FUN_100344d0(local_c);
      fVar1 = local_c[0];
      if (DAT_106ea3f4 ^ DAT_106e8204) {
        fVar1 = -fVar1;
      }
      fVar2 = local_c[1];
      fVar3 = fVar1 * DAT_10077350;
      fVar4 = DAT_10077348 - fVar1 * DAT_10077344;
      fVar5 = fVar2 * DAT_10077354;
      fVar2 = DAT_10077348 - fVar2 * DAT_1007734c;
      iVar12 = DAT_106ec798;
      do {
        fVar4 = fVar4 + fVar3;
        fVar2 = fVar2 + fVar5;
        iVar8 = (int)fVar4;
        iVar9 = (int)fVar2;
        iVar10 = iVar15 << 4;
        iVar10 = iVar10 | iVar15;
        bVar13 = (unsigned char)iVar10;
        if (((iVar9 >= 2) && (iVar9 <= 0x3d)) && ((iVar8 >= 2) && (iVar8 <= 0x3d))) {
          iVar9 = iVar12 * 0x40 + iVar9;
          iVar10 = iVar9 * 0x40 + iVar8;
          iVar8 = (iVar9 + -2) * 0x40 + iVar8;
          (&DAT_104af547)[iVar10] = bVar13;
          (&DAT_104af5c8)[iVar8] = bVar13;
          (&DAT_104af5c9)[iVar8] = bVar13;
          (&DAT_104af586)[iVar10] = bVar13;
          (&DAT_104af587)[iVar10] = bVar13;
          (&DAT_104af588)[iVar10] = bVar13;
          (&DAT_104af589)[iVar10] = bVar13;
          (&DAT_104af58a)[iVar10] = bVar13;
          (&DAT_104af5c6)[iVar10] = bVar13;
          (&DAT_104af5c7)[iVar10] = bVar13;
          (&DAT_104af5c8)[iVar10] = bVar13;
          (&DAT_104af5c9)[iVar10] = bVar13;
          (&DAT_104af5ca)[iVar10] = bVar13;
          (&DAT_104af606)[iVar10] = bVar13;
          (&DAT_104af607)[iVar10] = bVar13;
          (&DAT_104af608)[iVar10] = bVar13;
          (&DAT_104af609)[iVar10] = bVar13;
          (&DAT_104af60a)[iVar10] = bVar13;
          (&DAT_104af647)[iVar10] = bVar13;
          (&DAT_104af648)[iVar10] = bVar13;
          (&DAT_104af649)[iVar10] = bVar13;
        }
        iVar15 = iVar15 + 1;
      } while (iVar15 < 0x10);
    }
    FUN_10008d60(0, 0x80, 0xff, 0xff, 0xff);
    FUN_10034af0(&DAT_106e78f0, &DAT_106e78f0, &DAT_106e72a8);
    if (DAT_106ed6b0 != 0) {
      scaleA = 96.0f;
      scaleB = 144.0f;
    } else {
      scaleA = 128.0f;
      scaleB = 128.0f;
    }
    if (DAT_106ea3f4 ^ DAT_106e8204) {
      viewW = (float)(-DAT_100a7514) + (float)(-DAT_100a7514);
    } else {
      viewW = (float)DAT_100a7514 + (float)DAT_100a7514;
    }
    viewH = (float)DAT_100a7518 + (float)DAT_100a7518;
    iVar15 = 0;
    if (0 < DAT_104add38) {
      do {
        fVar4 = (float)(int)psVar16[-2];
        fVar1 = (float)(int)psVar16[-1];
        fVar2 = (float)(int)*psVar16;
        fVar3 = fVar4 * DAT_106e78f0.m[0][3] +
                (float)(int)psVar16[-1] * DAT_106e78f0.m[1][3] + (float)(int)*psVar16 * DAT_106e78f0.m[2][3] +
                DAT_106e78f0.m[3][3];
        if (DAT_10077364 <= fVar3) {
          fVar3 = DAT_10077314 / fVar3;
          fVar5 = fVar3 * (fVar4 * DAT_106e78f0.m[0][1] + fVar1 * DAT_106e78f0.m[1][1] + fVar2 * DAT_106e78f0.m[2][1] +
                           DAT_106e78f0.m[3][1]);
          if ((DAT_10077368 <= fVar5) && (fVar5 <= DAT_10077304)) {
            fVar1 = fVar3 * (fVar4 * DAT_106e78f0.m[0][0] + fVar1 * DAT_106e78f0.m[1][0] + fVar2 * DAT_106e78f0.m[2][0] +
                             DAT_106e78f0.m[3][0]);
            if ((DAT_10077368 <= fVar1) && (fVar1 <= DAT_10077304)) {
              iVar11 = (int)(fVar3 * scaleA);
              if (iVar11 >= 2) {
                iVar12 = (int)(fVar3 * scaleB);
                if (iVar12 >= 2) {
                  iVar8 = (int)(fVar1 * viewW);
                  iVar8 = iVar8 + (DAT_100a7514 / 2) * 4;
                  iVar9 = (int)(fVar5 * viewH);
                  iVar9 = iVar9 + (DAT_100a7518 / 2) * 4;
                  puVar7 = DAT_106e7710;
                  puVar6 = DAT_106e7710 + 2;
                  iVar10 = (iVar8 + iVar11) & 0x3ffc | 0x38c000;
                  *DAT_106e7710 =
                      ((int)iVar10 >> 2) << 0xc | (iVar12 + iVar9 >> 2) & 0xfff;
                  DAT_106e7710 = puVar6;
                  puVar7[1] = (iVar8 >> 2 & 0xfff) << 0xc | iVar9 >> 2 & 0xfff;
                }
              }
            }
          }
        }
        iVar15 = iVar15 + 1;
        psVar16 = psVar16 + 3;
      } while (iVar15 < DAT_104add38);
    }
    puVar7 = DAT_106e7710;
    puVar6 = DAT_106e7710 + 2;
    *DAT_106e7710 = 0xe7000000;
    DAT_106e7710 = puVar6;
    puVar7[1] = 0;
    puVar7 = DAT_106e7710;
    puVar6 = DAT_106e7710 + 2;
    *DAT_106e7710 = 0xba001301;
    DAT_106e7710 = puVar6;
    puVar7[1] = 0x80000;
  }
  return;
}

#endif /* BR_MATCHING_BUILD */
