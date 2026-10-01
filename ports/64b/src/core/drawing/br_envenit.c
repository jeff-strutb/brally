#include "br_coretypes.h"   /* br_globals: its objects */
#include "br_mat.h"   /* br_globals: its objects */
#include "br_vec.h"   /* br_globals: its objects */
#include "slice1_05.h"   /* br_globals: its objects */
/* br_envenit.c -- matching arm for BrEnvEmit (0x10017110).
 *
 * Track/environment display-list emitter.  The port lives in br_drawenv.c
 * behind #ifndef BR_MATCHING_BUILD; this file is the matching body.
 */
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)

/* BrMat4: br_mat.h */

/* FUN_10008d60: prototype in br_funcs.h */
/* FUN_1001cf90: prototype in br_funcs.h */
/* FUN_10034b70: prototype in br_funcs.h */
/* FUN_10034af0: prototype in br_funcs.h */
/* FUN_100349c0: prototype in br_funcs.h */
/* FUN_100344d0: prototype in br_funcs.h */
/* FUN_100597f0: prototype in br_funcs.h */

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

/* RESIDUE (2026-09-20): bytes 2038/2039, insn gap 7, 15 regions.  The old
 * transcription wrote each display-list command THROUGH the global cursor
 * (`*DAT_106e7710 = cmd; DAT_106e7710 = puVar6;`), which cost a spare pointer
 * and a lea per command and left it -89 bytes / 22 insns short.  The original
 * advances the cursor off the already-loaded value and writes through the saved
 * pointer (`puVar7 = DAT_106e7710; DAT_106e7710 = DAT_106e7710 + 2; *puVar7 =
 * cmd;`).  Respelling that idiom closed the structural gap to a single byte.
 * What is left is pure colouring, read out of the diff:
 *   - the constant 8 (the per-command cursor bump) is cached in a spare
 *     register (ebx frees after the two enter-flag loads), so we emit `add
 *     ecx,ebx` where the original keeps DAT_106ed6b4 live in ebx and uses the
 *     immediate `add ecx,8` -- 15 add R,R vs 15 add R,8.
 *   - x87 stack scheduling in the per-segment projection: fst/fld/fxch order
 *     and fcomp-mem vs fcomp-st (test ah,1 vs ah,0x41), and the tile pack
 *     folded as shl 0xc vs shl 0xa + and 0xfff000.
 * A5 oracle EQUIVALENT on 48 seeds (oracle_profiles _env_bss): the display-list
 * command stream, the per-segment transform coefficients, the scale constants
 * and the tile-pack are all verified exactly (negative controls on each fire
 * DIFF).  No source lever moved the byte count off 2038 across 10+ spellings
 * and 5 opt levels. */
/* WHAT IT DOES: emit the per-frame track-surface display list -- pipe/combiner
 * preamble, texture bind, prim colour, a 64x64 projected visibility bitmap
 * stamped along the section light, and one tile command per track segment. */
/* @t4-pass 0x10017110 3 2026-09-20 probes 12 bytes 2038 insns 505 regions 15 rows 109 census no  (fn.py: cursor-advance spellings (+=, &x[2], char+8), flag hoist, extra decl, write-then-advance, O2/O2y/O2p/Od/O1.  Best -1 byte/+7 insns at O2; none reached 0.  Residue is the constant-8 register cache + x87 scheduling.) */
/* @t4-pass 0x10017110 4 2026-09-20 probes 12 bytes 2038 insns 505 regions 15 rows 109 census yes  (census of unpaired multiset: 15 add R,R (EXTRA) pair with 15 add R,8 (MISSING) = the cursor-bump constant cached in ebx vs immediate, a register-allocation choice; fst/fld/fstp/fxch/fcomp-st (EXTRA) vs fst-mem/fcomp-mem (MISSING) = x87 stack scheduling of the projection; shl 0xa+and 0xfff000+or (EXTRA) vs shl 0xc (MISSING) = same tile-pack value, different encoding.  Every divergent row is register allocation or instruction scheduling of identical logic.  A5 oracle EQUIVALENT on 48 seeds.) */
/* @t3 0x10017110 2026-09-28 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 2064/2039 insns 511/498 rows 39+52 regions 17 oracle EQUIVALENT
 * @t3-effort passes 4 zero-movement 3 4
 * RECERTIFIED 2026-09-28.  The 2026-09-20 certificate read the missing
 * `fst dword [esp+..]` rows as x87 scheduling; they were ROUNDING POINTS.
 * The body kept w, x, y, 1/w and the two projected coordinates at 80 bits,
 * so a sprite whose projected x landed on an integer boundary came out one
 * quarter pixel off -- seen by the whole-image run on 12 cars and on snow and
 * storm tracks, never by the original 26 scripts.  The projection loop now
 * rounds exactly the six values the original stores (volatile floats, sums in
 * the original's z-first order); A5 EQUIVALENT and A7 IDENTICAL on the
 * sessions that exposed it (34_desert_snow_car9, 36_amazon_fog_car11).
 */
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
    psVar16 = (&DAT_104add50[iVar11][0][2]);
    if (DAT_106ed6b4) {
      FUN_10008d60();
    }
    puVar7 = DAT_106e7710;
    DAT_106e7710 = DAT_106e7710 + 2;
    *puVar7 = 0xe7000000;
    puVar7[1] = iVar15;
    puVar7 = DAT_106e7710;
    DAT_106e7710 = DAT_106e7710 + 2;
    *puVar7 = 0xba001402;
    puVar7[1] = iVar15;
    puVar7 = DAT_106e7710;
    DAT_106e7710 = DAT_106e7710 + 2;
    *puVar7 = 0xbb000001;
    puVar7[1] = 0xffffffff;
    puVar7 = DAT_106e7710;
    DAT_106e7710 = DAT_106e7710 + 2;
    *puVar7 = 0xba000c02;
    puVar7[1] = DAT_106e72e8;
    puVar7 = DAT_106e7710;
    DAT_106e7710 = DAT_106e7710 + 2;
    FUN_1001cf90(puVar7, 0, 0, 0, 0x3eb, 0, 0, 0, 0x3e9, 0, 0, 0, 0x3eb, 0, 0, 0, 0x3e9);
    puVar7 = DAT_106e7710;
    DAT_106e7710 = DAT_106e7710 + 2;
    *puVar7 = 0xb900031d;
    puVar7[1] = 0x504240;
    puVar7 = DAT_106e7710;
    if (DAT_106ed6b4 == 0) {
      DAT_106e7710 = DAT_106e7710 + 2;
      *puVar7 = DAT_1184c478 & 0xffffff | 0xdc000000;
    } else {
      DAT_106e7710 = DAT_106e7710 + 2;
      *puVar7 = *(unsigned int *)((char *)&DAT_1184c460 + DAT_106ec798 * 4) & 0xffffff | 0xdd000000;
      puVar7[1] = (unsigned int)(&DAT_104af5c8 + DAT_106ec798 * 0x1000);
      puVar7 = DAT_106e7710;
      DAT_106e7710 = DAT_106e7710 + 2;
      *puVar7 = *(unsigned int *)((char *)&DAT_1184c460 + DAT_106ec798 * 4) & 0xffffff | 0xdc000000;
    }
    puVar7[1] = 1;
    puVar7 = DAT_106e7710;
    DAT_106e7710 = DAT_106e7710 + 2;
    *puVar7 = 0xf2002002;
    puVar7[1] = 0xfe0fe;
    puVar7 = DAT_106e7710;
    DAT_106e7710 = DAT_106e7710 + 2;
    *puVar7 = 0xb6000000;
    puVar7[1] = 0x3000;
    puVar7 = DAT_106e7710;
    if (DAT_106ed6b0 == 0) {
      DAT_106e7710 = DAT_106e7710 + 2;
      *puVar7 = 0xfa00ffff;
      puVar7[1] = 0x788088ff;
    } else {
      DAT_106e7710 = DAT_106e7710 + 2;
      *puVar7 = 0xfa00ffff;
      puVar7[1] = 0xe0e0ffff;
    }
    FUN_10034b70(&DAT_106e78f0, DAT_106ed520);
    g_BrDrawCombined.m[3][2] = 0.0f;
    g_BrDrawCombined.m[3][1] = 0.0f;
    g_BrDrawCombined.m[3][0] = 0.0f;
    g_BrDrawScale.m[0][0] = 0.0f;
    g_BrDrawScale.m[0][1] = 0.0f;
    g_BrDrawScale.m[0][2] = 6.103701889514923e-05f;
    g_BrDrawScale.m[0][3] = 0.0f;
    g_BrDrawScale.m[1][0] = 6.103701889514923e-05f;
    g_BrDrawScale.m[1][1] = 0.0f;
    g_BrDrawScale.m[1][2] = 0.0f;
    g_BrDrawScale.m[1][3] = 0.0f;
    g_BrDrawScale.m[2][0] = 0.0f;
    g_BrDrawScale.m[2][1] = 6.103701889514923e-05f;
    g_BrDrawScale.m[2][2] = 0.0f;
    g_BrDrawScale.m[2][3] = 0.0f;
    g_BrDrawScale.m[3][0] = 0.0f;
    g_BrDrawScale.m[3][1] = 0.0f;
    g_BrDrawScale.m[3][2] = 0.0f;
    g_BrDrawScale.m[3][3] = 1.0f;
    FUN_10034af0(&DAT_106e78f0, &DAT_106e78f0, &DAT_106e7930);
    if (DAT_106ed6b4) {
      FUN_100597f0(&DAT_104af5c8 + DAT_106ec798 * 0x1000, 0x1000, 0);
      FUN_100349c0(local_c, (&DAT_104b15d0[DAT_106ec798].x), &DAT_106e78f0);
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
    FUN_10008d60();
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
        /* Precision is behaviour here.  The original rounds exactly these
         * six values to float (each is stored to a stack slot and reloaded):
         * the three transformed coordinates, the reciprocal of w and the two
         * projected screen coordinates.  Everything else stays on the x87
         * stack at 80 bits, and each dot product is summed z-term first.
         * `volatile` pins those store/reload points; without it a projected
         * coordinate that lands on an integer boundary truncates one quarter
         * pixel differently (found by the whole-image run, 2026-09-28). */
        volatile float vW, vX, vY, vR, vSx, vSy;
        fVar4 = (float)(int)psVar16[-2];
        fVar1 = (float)(int)psVar16[-1];
        fVar2 = (float)(int)*psVar16;
        vW = ((fVar2 * g_BrDrawCombined.m[2][3] + fVar1 * g_BrDrawCombined.m[1][3])
              + fVar4 * g_BrDrawCombined.m[0][3]) + g_BrDrawCombined.m[3][3];
        vX = ((fVar2 * g_BrDrawCombined.m[2][0] + fVar1 * g_BrDrawCombined.m[1][0])
              + fVar4 * g_BrDrawCombined.m[0][0]) + g_BrDrawCombined.m[3][0];
        vY = ((fVar2 * g_BrDrawCombined.m[2][1] + fVar1 * g_BrDrawCombined.m[1][1])
              + fVar4 * g_BrDrawCombined.m[0][1]) + g_BrDrawCombined.m[3][1];
        if (DAT_10077364 <= vW) {
          vR = DAT_10077314 / vW;
          vSy = vR * vY;
          if ((DAT_10077368 <= vSy) && (vSy <= DAT_10077304)) {
            vSx = vR * vX;
            if ((DAT_10077368 <= vSx) && (vSx <= DAT_10077304)) {
              iVar11 = (int)(vR * scaleA);
              if (iVar11 >= 2) {
                iVar12 = (int)(vR * scaleB);
                if (iVar12 >= 2) {
                  iVar8 = (int)(vSx * viewW);
                  iVar8 = iVar8 + (DAT_100a7514 / 2) * 4;
                  iVar9 = (int)(vSy * viewH);
                  iVar9 = iVar9 + (DAT_100a7518 / 2) * 4;
                  puVar7 = DAT_106e7710;
                  DAT_106e7710 = DAT_106e7710 + 2;
                  iVar10 = (iVar8 + iVar11) & 0x3ffc | 0x38c000;
                  *puVar7 =
                      ((int)iVar10 >> 2) << 0xc | (iVar12 + iVar9 >> 2) & 0xfff;
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
    DAT_106e7710 = DAT_106e7710 + 2;
    *puVar7 = 0xe7000000;
    puVar7[1] = 0;
    puVar7 = DAT_106e7710;
    DAT_106e7710 = DAT_106e7710 + 2;
    *puVar7 = 0xba001301;
    puVar7[1] = 0x80000;
  }
  return;
}

