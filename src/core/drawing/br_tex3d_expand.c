/* 0x100250D0 BrTex3dExpand — matching transcription from Ghidra decomp.
 * The insn-3 "coloring wall" is BROKEN (for-loop with a raw-parameter bound);
 * the store-idiom wall is BROKEN too (Ghidra folded orig's two separate
 * `count += 2` updates into one `+= 4` with a `count + 2 >= cbMax` guard --
 * the real source bumps the counter BEFORE each store, advances pOut by ONE
 * element per store, and puts each budget check on its own control edge).
 * State, do-not-re-run probe lists and the open levers: docs/idioms-A.md.
 * 2026-09-01: the IDX4 arm's width is the reused `param_9` (orig homes it in
 * the dead param_9 arg slot [esp+0x9c] and reloads every bound from there);
 * that closed the five 0xae-0x1ee regions.  The same rename on the CI8 arm
 * breaks the frame -- measured, do not apply there.  The copy-back
 * preamble's doubling ternary `(param_7 != 0) ? w * 2 : w` (idioms-A.md)
 * had regressed to if-form at all 18 sites; restoring it at the IDX4 pair
 * alone closes the whole IDX4 tail (0x206-0x343).  Restoring ANY second
 * pair flips the global allocation (+28 insns, tile pointer ebx->ebp):
 * measured per pair and in five combinations, do not re-run. */
#ifdef BR_MATCHING_BUILD

#define _CRTIMP __declspec(dllimport)
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifndef true
#define true 1
#define false 0
#endif

unsigned int FUN_100271f0(unsigned short);

void BrTex3dExpand(unsigned short *param_1,int param_2,int param_3,unsigned char *param_4,int param_5,int param_6,
                 int param_7,int param_8,int param_9,int param_10,int param_11,unsigned char param_12,
                 int param_13,unsigned char param_14,unsigned char param_15,unsigned char param_16,unsigned char param_17,unsigned char param_18,
                 unsigned char param_19,unsigned char param_20,unsigned char param_21,int param_22)

{
  unsigned short *puVar2;
  int iVar3;
  unsigned short uVar4;
  int iVar5;
  unsigned int uVar6;
  unsigned int uVar7;
  unsigned int uVar8;
  unsigned short *puVar9;
  int iVar10;
  unsigned char bVar11;
  unsigned char bCI4a, bCI4b, bCI4c;
  unsigned char bIA8a, bIA8b, bIA8c;
  unsigned char bI4inten;
  unsigned char chA, chR, chG, chB;
  unsigned short pal;
  int lo0;
  int loIA8;
  unsigned char *pbVar12;
  int iVar13;
  unsigned int uVar14;
  int iVar15;
  int iVar16;
  int iVar17;
  int iVar18;
  unsigned int uVar19;
  int iVar20;
  unsigned short *puVar21;
  int iVar22;
  unsigned char *local_64;
  int local_54;
  unsigned int local_44;
  unsigned int local_3c;
  unsigned int local_38;
  int local_34;
  int local_24;
  int cbMax;
  int one;
  
  iVar3 = (int)param_4;
  iVar22 = 0;
  iVar10 = param_9;
  if (param_9 >= param_10) {
    return;
  }
  cbMax = param_2;
  puVar21 = param_1;
  for (; iVar10 < param_10; iVar10 = iVar10 + 1) {
    iVar5 = iVar10 * 0x40 + param_11;
    param_4 = (unsigned char *)(iVar3 + *(int *)(iVar10 * 0x40 + 0xc + param_11) * 8);
    if (param_3 == 0) {
      if (param_6 == 2) {
        if (((param_12 & 2) != 0) && (iVar10 == 1)) {
          param_9 = 1 << (*(int *)(param_11 + 0x60) - 1);
          param_1 = (unsigned short *)0x0;
          iVar15 = 1 << *(int *)(param_11 + 0x64);
          if (0 < iVar15) {
            do {
              pbVar12 = param_4;
              if (((unsigned int)param_1 & param_22) != 0) {
                iVar16 = 0;
                if (0 < param_9) {
                  do {
                    pbVar12 = pbVar12 + 4;
                    for (local_24 = 0; local_24 < 4; local_24 = local_24 + 1) {
                      if (iVar16 >= param_9) break;
                      bVar11 = *pbVar12;
                      iVar22 = iVar22 + 2;
                      *puVar21 = (unsigned short)(bVar11 >> 4);
                      puVar21 = puVar21 + 1;
                      if (iVar22 >= cbMax) {
                        return;
                      }
                      iVar22 = iVar22 + 2;
                      *puVar21 = (unsigned short)(bVar11 & 0xf);
                      puVar21 = puVar21 + 1;
                      if (iVar22 >= cbMax) {
                        return;
                      }
                      pbVar12 = pbVar12 + 1;
                      iVar16 = iVar16 + 1;
                    }
                    pbVar12 = pbVar12 + -8;
                    for (iVar13 = 0; iVar13 < 4; iVar13 = iVar13 + 1) {
                      if (iVar16 >= param_9) break;
                      bVar11 = *pbVar12;
                      iVar22 = iVar22 + 2;
                      *puVar21 = (unsigned short)(bVar11 >> 4);
                      puVar21 = puVar21 + 1;
                      if (iVar22 >= cbMax) {
                        return;
                      }
                      iVar22 = iVar22 + 2;
                      *puVar21 = (unsigned short)(bVar11 & 0xf);
                      puVar21 = puVar21 + 1;
                      if (iVar22 >= cbMax) {
                        return;
                      }
                      pbVar12 = pbVar12 + 1;
                      iVar16 = iVar16 + 1;
                    }
                    pbVar12 = pbVar12 + 4;
                  } while (iVar16 < param_9);
                }
              }
              else {
                iVar16 = 0;
                if (0 < param_9) {
                  do {
                    bVar11 = *pbVar12;
                    iVar22 = iVar22 + 2;
                    *puVar21 = (unsigned short)(bVar11 >> 4);
                    puVar21 = puVar21 + 1;
                    if (iVar22 >= cbMax) {
                      return;
                    }
                    iVar22 = iVar22 + 2;
                    *puVar21 = (unsigned short)(bVar11 & 0xf);
                    puVar21 = puVar21 + 1;
                    if (iVar22 >= cbMax) {
                      return;
                    }
                    pbVar12 = pbVar12 + 1;
                    iVar16 = iVar16 + 1;
                  } while (iVar16 < param_9);
                }
              }
              if ((param_7 != 0) && (iVar16 = 0, puVar9 = puVar21 + -1, 0 < param_9)) {
                do {
                  iVar22 = iVar22 + 2;
                  *puVar21 = *puVar9;
                  puVar21 = puVar21 + 1;
                  puVar9 = puVar9 + -1;
                  if (iVar22 >= cbMax) {
                    return;
                  }
                  iVar22 = iVar22 + 2;
                  *puVar21 = *puVar9;
                  puVar21 = puVar21 + 1;
                  puVar9 = puVar9 + -1;
                  if (iVar22 >= cbMax) {
                    return;
                  }
                  iVar16 = iVar16 + 1;
                } while (iVar16 < param_9);
              }
              param_4 = param_4 + *(int *)(param_11 + 0x48);
              param_1 = (unsigned short *)((int)param_1 + 1);
            } while ((int)param_1 < iVar15);
          }
          if ((param_8 != 0) && (puVar9 = puVar21, param_1 = (unsigned short *)0x0, 0 < iVar15)) {
            do {
              iVar16 = (param_7 != 0) ? param_9 * 2 : param_9;
              puVar9 = puVar9 + iVar16 * -2;
              puVar2 = puVar9;
              iVar16 = (param_7 != 0) ? param_9 * 2 : param_9;
              for (; 0 < iVar16; iVar16 = iVar16 + -1) {
                iVar22 = iVar22 + 2;
                *puVar21 = *puVar2;
                puVar21 = puVar21 + 1;
                puVar2 = puVar2 + 1;
                if (iVar22 >= cbMax) {
                  return;
                }
                iVar22 = iVar22 + 2;
                *puVar21 = *puVar2;
                puVar21 = puVar21 + 1;
                puVar2 = puVar2 + 1;
                if (iVar22 >= cbMax) {
                  return;
                }
              }
              param_1 = (unsigned short *)((int)param_1 + 1);
            } while ((int)param_1 < iVar15);
          }
                } else {
          iVar17 = 1 << (*(int *)(iVar5 + 0x20) - 1);
          iVar15 = 1 << *(int *)(iVar5 + 0x24);
          local_44 = 0;
          if (0 < iVar15) {
            do {
              param_9 = (int)param_4;
              if ((local_44 & param_22) != 0) {
                for (param_1 = (unsigned short *)0x0; (int)param_1 < iVar17;) {
                    param_9 = param_9 + 4;
                    for (local_38 = 0; (int)local_38 < 4; local_38 = local_38 + 1) {
                      if ((int)param_1 >= iVar17) break;
                      bCI4b = *(unsigned char *)param_9;
                      uVar4 = FUN_100271f0(*(unsigned short *)(param_5 + (unsigned int)(bCI4b >> 4) * 2));
                      *puVar21 = uVar4;
                      iVar22 = iVar22 + 2;
                      puVar21 = puVar21 + 1;
                      if (iVar22 >= cbMax) {
                        return;
                      }
                      uVar4 = FUN_100271f0(*(unsigned short *)(param_5 + (bCI4b & 0xf) * 2));
                      *puVar21 = uVar4;
                      iVar22 = iVar22 + 2;
                      puVar21 = puVar21 + 1;
                      if (iVar22 >= cbMax) {
                        return;
                      }
                      param_9 = param_9 + 1;
                      param_1 = (unsigned short *)((int)param_1 + 1);
                    }
                    param_9 = param_9 + -8;
                    for (local_38 = 0; (int)local_38 < 4; local_38 = local_38 + 1) {
                      if ((int)param_1 >= iVar17) break;
                      bCI4c = *(unsigned char *)param_9;
                      uVar4 = FUN_100271f0(*(unsigned short *)(param_5 + (unsigned int)(bCI4c >> 4) * 2));
                      *puVar21 = uVar4;
                      iVar22 = iVar22 + 2;
                      puVar21 = puVar21 + 1;
                      if (iVar22 >= cbMax) {
                        return;
                      }
                      uVar4 = FUN_100271f0(*(unsigned short *)(param_5 + (bCI4c & 0xf) * 2));
                      *puVar21 = uVar4;
                      iVar22 = iVar22 + 2;
                      puVar21 = puVar21 + 1;
                      if (iVar22 >= cbMax) {
                        return;
                      }
                      param_9 = param_9 + 1;
                      param_1 = (unsigned short *)((int)param_1 + 1);
                    }
                    param_9 = param_9 + 4;
                }
              }
              else {
                for (param_1 = (unsigned short *)0x0; (int)param_1 < iVar17;) {
                  bCI4a = *(unsigned char *)param_9;
                  pal = *(unsigned short *)(param_5 + (unsigned int)(bCI4a >> 4) * 2);
                  uVar4 = FUN_100271f0(pal);
                  *puVar21 = uVar4;
                  iVar22 = iVar22 + 2;
                  puVar21 = puVar21 + 1;
                  if (iVar22 >= cbMax) {
                    return;
                  }
                  pal = *(unsigned short *)(param_5 + (bCI4a & 0xf) * 2);
                  uVar4 = FUN_100271f0(pal);
                  *puVar21 = uVar4;
                  iVar22 = iVar22 + 2;
                  puVar21 = puVar21 + 1;
                  if (iVar22 >= cbMax) {
                    return;
                  }
                  param_9 = param_9 + 1;
                  param_1 = (unsigned short *)((int)param_1 + 1);
                }
              }
              if ((param_7 != 0) && (iVar16 = 0, puVar9 = puVar21 + -1, 0 < iVar17)) {
                do {
                  iVar22 = iVar22 + 2;
                  *puVar21 = *puVar9;
                  puVar21 = puVar21 + 1;
                  puVar9 = puVar9 + -1;
                  if (iVar22 >= cbMax) {
                    return;
                  }
                  iVar22 = iVar22 + 2;
                  *puVar21 = *puVar9;
                  puVar21 = puVar21 + 1;
                  puVar9 = puVar9 + -1;
                  if (iVar22 >= cbMax) {
                    return;
                  }
                  iVar16 = iVar16 + 1;
                } while (iVar16 < iVar17);
              }
              param_4 = param_4 + *(int *)(iVar5 + 8);
              local_44 = local_44 + 1;
            } while ((int)local_44 < iVar15);
          }
          if ((param_8 != 0) && (puVar9 = puVar21, local_44 = 0, 0 < iVar15)) {
            do {
              iVar5 = (param_7 != 0) ? iVar17 * 2 : iVar17;
              puVar9 = puVar9 + iVar5 * -2;
              puVar2 = puVar9;
              iVar5 = (param_7 != 0) ? iVar17 * 2 : iVar17;
              for (; 0 < iVar5; iVar5 = iVar5 + -1) {
                iVar22 = iVar22 + 2;
                *puVar21 = *puVar2;
                puVar21 = puVar21 + 1;
                puVar2 = puVar2 + 1;
                if (iVar22 >= cbMax) {
                  return;
                }
                iVar22 = iVar22 + 2;
                *puVar21 = *puVar2;
                puVar21 = puVar21 + 1;
                puVar2 = puVar2 + 1;
                if (iVar22 >= cbMax) {
                  return;
                }
              }
              local_44 = local_44 + 1;
            } while ((int)local_44 < iVar15);
          }
                }
      }
      else if (param_6 == 4) {
        if (param_13 == 1) {
          iVar17 = 1 << (*(int *)(iVar5 + 0x20) - 1);
          iVar15 = 1 << *(int *)(iVar5 + 0x24);
          local_3c = 0;
          if (0 < iVar15) {
            do {
              local_64 = param_4;
              local_54 = 0;
              if ((local_3c & param_22) != 0) {
                if (0 < iVar17) {
                do {
                  local_64 = local_64 + 4;
                  for (local_34 = 0; local_34 < 4; local_34 = local_34 + 1) {
                    if (local_54 >= iVar17) break;
                    lo0 = param_18 & 0xff;
                    iVar16 = (param_14 & 0xff) - lo0;
                    bVar11 = *local_64;
                    bI4inten = (unsigned char)(bVar11 >> 4 | bVar11 & 0xf0);
                    chR = (unsigned char)((int)((int)((unsigned int)bI4inten * iVar16) / 0xff + lo0) >> 3);
                    uVar6 = param_19 & 0xff;
                    iVar13 = (param_15 & 0xff) - uVar6;
                    chG = (unsigned char)((int)((int)((unsigned int)bI4inten * iVar13) / 0xff + uVar6) >> 3);
                    uVar7 = param_20 & 0xff;
                    iVar18 = (param_16 & 0xff) - uVar7;
                    chB = (unsigned char)((int)((int)((unsigned int)bI4inten * iVar18) / 0xff + uVar7) >> 3);
                    uVar8 = param_21 & 0xff;
                    iVar20 = (param_17 & 0xff) - uVar8;
                    chA = (unsigned char)((int)((int)((unsigned int)bI4inten * iVar20) / 0xff + uVar8) >> 7);
                    iVar22 = iVar22 + 2;
                    *puVar21 = (unsigned short)(((((unsigned int)chA << 5 | (unsigned int)chR) << 5 |
                                        (unsigned int)chG) << 5) | (unsigned int)chB);
                    puVar21 = puVar21 + 1;
                    if (iVar22 >= cbMax) {
                      return;
                    }
                    bI4inten = bVar11 << 4 | bVar11 & 0xf;
                    chR = (unsigned char)((int)((int)((unsigned int)bI4inten * iVar16) / 0xff + lo0) >> 3);
                    chG = (unsigned char)((int)((int)((unsigned int)bI4inten * iVar13) / 0xff + uVar6) >> 3);
                    chB = (unsigned char)((int)((int)((unsigned int)bI4inten * iVar18) / 0xff + uVar7) >> 3);
                    chA = (unsigned char)((int)((int)((unsigned int)bI4inten * iVar20) / 0xff + uVar8) >> 7);
                    iVar22 = iVar22 + 2;
                    *puVar21 = (unsigned short)(((((unsigned int)chA << 5 | (unsigned int)chR) << 5 |
                                        (unsigned int)chG) << 5) | (unsigned int)chB);
                    puVar21 = puVar21 + 1;
                    if (iVar22 >= cbMax) {
                      return;
                    }
                    local_64 = local_64 + 1;
                    local_54 = local_54 + 1;
                  }
                  local_64 = local_64 + -8;
                  for (local_34 = 0; local_34 < 4; local_34 = local_34 + 1) {
                    if (local_54 >= iVar17) break;
                    lo0 = param_18 & 0xff;
                    iVar16 = (param_14 & 0xff) - lo0;
                    bVar11 = *local_64;
                    bI4inten = (unsigned char)(bVar11 >> 4 | bVar11 & 0xf0);
                    chR = (unsigned char)((int)((int)((unsigned int)bI4inten * iVar16) / 0xff + lo0) >> 3);
                    uVar6 = param_19 & 0xff;
                    iVar13 = (param_15 & 0xff) - uVar6;
                    chG = (unsigned char)((int)((int)((unsigned int)bI4inten * iVar13) / 0xff + uVar6) >> 3);
                    uVar7 = param_20 & 0xff;
                    iVar18 = (param_16 & 0xff) - uVar7;
                    chB = (unsigned char)((int)((int)((unsigned int)bI4inten * iVar18) / 0xff + uVar7) >> 3);
                    uVar8 = param_21 & 0xff;
                    iVar20 = (param_17 & 0xff) - uVar8;
                    chA = (unsigned char)((int)((int)((unsigned int)bI4inten * iVar20) / 0xff + uVar8) >> 7);
                    iVar22 = iVar22 + 2;
                    *puVar21 = (unsigned short)(((((unsigned int)chA << 5 | (unsigned int)chR) << 5 |
                                        (unsigned int)chG) << 5) | (unsigned int)chB);
                    puVar21 = puVar21 + 1;
                    if (iVar22 >= cbMax) {
                      return;
                    }
                    bI4inten = bVar11 << 4 | bVar11 & 0xf;
                    chR = (unsigned char)((int)((int)((unsigned int)bI4inten * iVar16) / 0xff + lo0) >> 3);
                    chG = (unsigned char)((int)((int)((unsigned int)bI4inten * iVar13) / 0xff + uVar6) >> 3);
                    chB = (unsigned char)((int)((int)((unsigned int)bI4inten * iVar18) / 0xff + uVar7) >> 3);
                    chA = (unsigned char)((int)((int)((unsigned int)bI4inten * iVar20) / 0xff + uVar8) >> 7);
                    iVar22 = iVar22 + 2;
                    *puVar21 = (unsigned short)(((((unsigned int)chA << 5 | (unsigned int)chR) << 5 |
                                        (unsigned int)chG) << 5) | (unsigned int)chB);
                    puVar21 = puVar21 + 1;
                    if (iVar22 >= cbMax) {
                      return;
                    }
                    local_64 = local_64 + 1;
                    local_54 = local_54 + 1;
                  }
                  local_64 = local_64 + 4;
                } while (local_54 < iVar17);
                }
              }
              else {
                if (0 < iVar17) {
                  do {
                    lo0 = param_18 & 0xff;
                    iVar16 = (param_14 & 0xff) - lo0;
                    bVar11 = *local_64;
                    bI4inten = (unsigned char)(bVar11 >> 4 | bVar11 & 0xf0);
                    uVar19 = (unsigned int)bI4inten;
                    chR = (unsigned char)((int)((int)(uVar19 * iVar16) / 0xff + lo0) >> 3);
                    uVar6 = param_19 & 0xff;
                    iVar13 = (param_15 & 0xff) - uVar6;
                    chG = (unsigned char)((int)((int)(uVar19 * iVar13) / 0xff + uVar6) >> 3);
                    uVar7 = param_20 & 0xff;
                    iVar18 = (param_16 & 0xff) - uVar7;
                    chB = (unsigned char)((int)((int)(uVar19 * iVar18) / 0xff + uVar7) >> 3);
                    uVar8 = param_21 & 0xff;
                    iVar20 = (param_17 & 0xff) - uVar8;
                    chA = (unsigned char)((int)((int)(uVar19 * iVar20) / 0xff + uVar8) >> 7);
                    iVar22 = iVar22 + 2;
                    *puVar21 = (unsigned short)(((((unsigned int)chA << 5 | (unsigned int)chR) << 5 |
                                        (unsigned int)chG) << 5) | (unsigned int)chB);
                    puVar21 = puVar21 + 1;
                    if (iVar22 >= cbMax) {
                      return;
                    }
                    bI4inten = bVar11 << 4 | bVar11 & 0xf;
                    uVar19 = (unsigned int)bI4inten;
                    chR = (unsigned char)((int)((int)(uVar19 * iVar16) / 0xff + lo0) >> 3);
                    chG = (unsigned char)((int)((int)(uVar19 * iVar13) / 0xff + uVar6) >> 3);
                    chB = (unsigned char)((int)((int)(uVar19 * iVar18) / 0xff + uVar7) >> 3);
                    chA = (unsigned char)((int)((int)(uVar19 * iVar20) / 0xff + uVar8) >> 7);
                    iVar22 = iVar22 + 2;
                    *puVar21 = (unsigned short)(((((unsigned int)chA << 5 | (unsigned int)chR) << 5 |
                                          (unsigned int)chG) << 5) | (unsigned int)chB);
                    puVar21 = puVar21 + 1;
                    if (iVar22 >= cbMax) {
                      return;
                    }
                    local_64 = local_64 + 1;
                    local_54 = local_54 + 1;
                  } while (local_54 < iVar17);
                }
              }
              if ((param_7 != 0) && (iVar16 = 0, puVar9 = puVar21 + -1, 0 < iVar17)) {
                do {
                  iVar22 = iVar22 + 2;
                  *puVar21 = *puVar9;
                  puVar21 = puVar21 + 1;
                  puVar9 = puVar9 + -1;
                  if (iVar22 >= cbMax) {
                    return;
                  }
                  iVar22 = iVar22 + 2;
                  *puVar21 = *puVar9;
                  puVar21 = puVar21 + 1;
                  puVar9 = puVar9 + -1;
                  if (iVar22 >= cbMax) {
                    return;
                  }
                  iVar16 = iVar16 + 1;
                } while (iVar16 < iVar17);
              }
              param_4 = param_4 + *(int *)(iVar5 + 8);
              local_3c = local_3c + 1;
            } while ((int)local_3c < iVar15);
          }
          if ((param_8 != 0) && (puVar9 = puVar21, local_3c = 0, 0 < iVar15)) {
            do {
              iVar5 = (param_7 != 0) ? iVar17 * 2 : iVar17;
              puVar9 = puVar9 + iVar5 * -2;
              puVar2 = puVar9;
              iVar5 = (param_7 != 0) ? iVar17 * 2 : iVar17;
              for (; 0 < iVar5; iVar5 = iVar5 + -1) {
                iVar22 = iVar22 + 2;
                *puVar21 = *puVar2;
                puVar21 = puVar21 + 1;
                puVar2 = puVar2 + 1;
                if (iVar22 >= cbMax) {
                  return;
                }
                iVar22 = iVar22 + 2;
                *puVar21 = *puVar2;
                puVar21 = puVar21 + 1;
                puVar2 = puVar2 + 1;
                if (iVar22 >= cbMax) {
                  return;
                }
              }
              local_3c = local_3c + 1;
            } while ((int)local_3c < iVar15);
          }
        }
        else {
          iVar17 = 1 << (*(int *)(iVar5 + 0x20) - 1);
          iVar15 = 1 << *(int *)(iVar5 + 0x24);
          local_38 = 0;
          if (0 < iVar15) {
            do {
              pbVar12 = param_4;
              if ((local_38 & param_22) != 0) {
                for (puVar2 = (unsigned short *)0x0; (int)puVar2 < iVar17;) {
                    pbVar12 = pbVar12 + 4;
                    for (param_1 = (unsigned short *)0x0; (int)param_1 < 4; param_1 = (unsigned short *)((int)param_1 + 1)) {
                      if ((int)puVar2 >= iVar17) break;
                      bVar11 = *pbVar12;
                      iVar22 = iVar22 + 1;
                      *(unsigned char *)puVar21 = bVar11 >> 4 | bVar11 & 0xf0;
                      puVar21 = (unsigned short *)((int)puVar21 + 1);
                      if (iVar22 >= cbMax) {
                        return;
                      }
                      iVar22 = iVar22 + 1;
                      *(unsigned char *)puVar21 = bVar11 << 4 | bVar11 & 0xf;
                      puVar21 = (unsigned short *)((int)puVar21 + 1);
                      if (iVar22 >= cbMax) {
                        return;
                      }
                      pbVar12 = pbVar12 + 1;
                      puVar2 = (unsigned short *)((int)puVar2 + 1);
                    }
                    pbVar12 = pbVar12 + -8;
                    for (param_1 = (unsigned short *)0x0; (int)param_1 < 4; param_1 = (unsigned short *)((int)param_1 + 1)) {
                      if ((int)puVar2 >= iVar17) break;
                      bVar11 = *pbVar12;
                      iVar22 = iVar22 + 1;
                      *(unsigned char *)puVar21 = bVar11 & 0xf0 | bVar11 >> 4;
                      puVar21 = (unsigned short *)((int)puVar21 + 1);
                      if (iVar22 >= cbMax) {
                        return;
                      }
                      iVar22 = iVar22 + 1;
                      *(unsigned char *)puVar21 = bVar11 & 0xf | bVar11 << 4;
                      puVar21 = (unsigned short *)((int)puVar21 + 1);
                      if (iVar22 >= cbMax) {
                        return;
                      }
                      pbVar12 = pbVar12 + 1;
                      puVar2 = (unsigned short *)((int)puVar2 + 1);
                    }
                    pbVar12 = pbVar12 + 4;
                }
              }
              else {
                for (puVar2 = (unsigned short *)0x0; (int)puVar2 < iVar17;) {
                    bVar11 = *pbVar12;
                    iVar22 = iVar22 + 1;
                    *(unsigned char *)puVar21 = bVar11 & 0xf0 | bVar11 >> 4;
                    puVar21 = (unsigned short *)((int)puVar21 + 1);
                    if (iVar22 >= cbMax) {
                      return;
                    }
                    iVar22 = iVar22 + 1;
                    *(unsigned char *)puVar21 = bVar11 << 4 | bVar11 & 0xf;
                    puVar21 = (unsigned short *)((int)puVar21 + 1);
                    if (iVar22 >= cbMax) {
                      return;
                    }
                    pbVar12 = pbVar12 + 1;
                    puVar2 = (unsigned short *)((int)puVar2 + 1);
                  }
              }
              if ((param_7 != 0) && (iVar16 = 0, puVar9 = (unsigned short *)((int)puVar21 + -1), 0 < iVar17)) {
                do {
                  iVar22 = iVar22 + 1;
                  *(unsigned char *)puVar21 = *(unsigned char *)puVar9;
                  puVar21 = (unsigned short *)((int)puVar21 + 1);
                  puVar9 = (unsigned short *)((int)puVar9 + -1);
                  if (iVar22 >= cbMax) {
                    return;
                  }
                  iVar22 = iVar22 + 1;
                  *(unsigned char *)puVar21 = *(unsigned char *)puVar9;
                  puVar21 = (unsigned short *)((int)puVar21 + 1);
                  puVar9 = (unsigned short *)((int)puVar9 + -1);
                  if (iVar22 >= cbMax) {
                    return;
                  }
                  iVar16 = iVar16 + 1;
                } while (iVar16 < iVar17);
              }
              param_4 = param_4 + *(int *)(iVar5 + 8);
              local_38 = local_38 + 1;
            } while ((int)local_38 < iVar15);
          }
          if ((param_8 != 0) && (puVar9 = puVar21, local_38 = 0, 0 < iVar15)) {
            do {
              iVar16 = iVar17 * 2;
              if (param_7 == 0) {
                iVar16 = iVar17;
              }
              puVar9 = puVar9 + -iVar16;
              puVar2 = puVar9;
              iVar16 = iVar17 * 2;
              if (param_7 == 0) {
                iVar16 = iVar17;
              }
              for (; 0 < iVar16; iVar16 = iVar16 + -1) {
                iVar22 = iVar22 + 1;
                *(unsigned char *)puVar21 = *(unsigned char *)puVar2;
                puVar21 = (unsigned short *)((int)puVar21 + 1);
                puVar2 = (unsigned short *)((int)puVar2 + 1);
                if (iVar22 >= cbMax) {
                  return;
                }
                iVar22 = iVar22 + 1;
                *(unsigned char *)puVar21 = *(unsigned char *)puVar2;
                puVar21 = (unsigned short *)((int)puVar21 + 1);
                puVar2 = (unsigned short *)((int)puVar2 + 1);
                if (iVar22 >= cbMax) {
                  return;
                }
              }
              local_38 = local_38 + 1;
            } while ((int)local_38 < iVar15);
          }
        }
      }
    }
    else {
      one = 1;
      if (param_3 == one) {
      if (param_6 == 2) {
        iVar15 = one << *(int *)(iVar5 + 0x20);
        local_44 = 0;
        iVar17 = one << *(int *)(iVar5 + 0x24);
        if (0 < iVar17) {
          do {
            pbVar12 = param_4;
            if ((param_22 & local_44) != 0) {
              iVar16 = 0;
              param_1 = (unsigned short *)0x0;
              if (0 < iVar15) {
                do {
                  pbVar12 = pbVar12 + 4;
                  for (local_34 = 0; local_34 < 4; local_34 = local_34 + 1) {
                    if (iVar16 >= iVar15) break;
                    uVar4 = FUN_100271f0(*(unsigned short *)(param_5 + (unsigned int)*pbVar12 * 2));
                    *puVar21 = uVar4;
                    iVar22 = iVar22 + 2;
                    puVar21 = puVar21 + 1;
                    if (iVar22 >= cbMax) {
                      return;
                    }
                    pbVar12 = pbVar12 + 1;
                    iVar16 = (int)param_1 + 1;
                    param_1 = (unsigned short *)iVar16;
                  }
                  pbVar12 = pbVar12 + -8;
                  for (local_34 = 0; local_34 < 4; local_34 = local_34 + 1) {
                    if (iVar16 >= iVar15) break;
                    uVar4 = FUN_100271f0(*(unsigned short *)(param_5 + (unsigned int)*pbVar12 * 2));
                    *puVar21 = uVar4;
                    iVar22 = iVar22 + 2;
                    puVar21 = puVar21 + 1;
                    if (iVar22 >= cbMax) {
                      return;
                    }
                    pbVar12 = pbVar12 + 1;
                    iVar16 = (int)param_1 + 1;
                    param_1 = (unsigned short *)iVar16;
                  }
                  pbVar12 = pbVar12 + 4;
                } while (iVar16 < iVar15);
              }
            }
            else {
              param_1 = (unsigned short *)0x0;
              iVar16 = iVar15;
              if (0 < iVar15) {
                do {
                  uVar4 = FUN_100271f0(*(unsigned short *)(param_5 + (unsigned int)*pbVar12 * 2));
                  *puVar21 = uVar4;
                  iVar22 = iVar22 + 2;
                  puVar21 = puVar21 + 1;
                  if (iVar22 >= cbMax) {
                    return;
                  }
                  pbVar12 = pbVar12 + 1;
                  iVar16 = (int)param_1 + 1;
                  param_1 = (unsigned short *)iVar16;
                } while (iVar16 < iVar15);
              }
            }
            if ((param_7 != 0) && (iVar16 = 0, puVar9 = puVar21 + -1, 0 < iVar15)) {
              do {
                iVar22 = iVar22 + 2;
                *puVar21 = *puVar9;
                puVar21 = puVar21 + 1;
                puVar9 = puVar9 + -1;
                if (iVar22 >= cbMax) {
                  return;
                }
                iVar16 = iVar16 + 1;
              } while (iVar16 < iVar15);
            }
            param_4 = param_4 + *(int *)(iVar5 + 8);
            local_44 = local_44 + 1;
          } while ((int)local_44 < iVar17);
        }
        if ((param_8 != 0) && (puVar9 = puVar21, local_44 = 0, 0 < iVar17)) {
          do {
            iVar5 = iVar15 * 2;
            if (param_7 == 0) {
              iVar5 = iVar15;
            }
            puVar9 = puVar9 + -iVar5;
            puVar2 = puVar9;
            iVar5 = iVar15 * 2;
            if (param_7 == 0) {
              iVar5 = iVar15;
            }
            for (; 0 < iVar5; iVar5 = iVar5 + -1) {
              iVar22 = iVar22 + 2;
              *puVar21 = *puVar2;
              puVar21 = puVar21 + 1;
              if (iVar22 >= cbMax) {
                return;
              }
              puVar2 = puVar2 + 1;
            }
            local_44 = local_44 + 1;
          } while ((int)local_44 < iVar17);
        }
      }
      else if (param_6 == 3) {
        if (param_13 == one) {
          iVar17 = one << *(int *)(iVar5 + 0x20);
          iVar15 = 1 << *(int *)(iVar5 + 0x24);
          local_44 = 0;
          if (0 < iVar15) {
            do {
              param_9 = (int)param_4;
              param_1 = (unsigned short *)0x0;
              if ((local_44 & param_22) != 0) {
                if (0 < iVar17) {
                do {
                  param_9 = param_9 + 4;
                  for (local_34 = 0; local_34 < 4; local_34 = local_34 + 1) {
                    if ((int)param_1 >= iVar17) break;
                    iVar22 = iVar22 + 2;
                    bIA8b = *(unsigned char *)param_9;
                    loIA8 = (unsigned int)bIA8b & 0xf;
                    uVar14 = ((unsigned int)bIA8b >> 4) | ((unsigned int)bIA8b & 0xf0);
                    *puVar21 = (unsigned short)((((loIA8) << 4 |
                                         (((param_14 & 0xff) - (param_18 & 0xff)) * uVar14) / 0xff +
                                         (param_18 & 0xff) >> 4) << 4 |
                                        (((param_15 & 0xff) - (param_19 & 0xff)) * uVar14) / 0xff +
                                        (param_19 & 0xff) >> 4) << 4) |
                               (unsigned short)((((param_16 & 0xff) - (param_20 & 0xff)) * uVar14) / 0xff +
                                        (param_20 & 0xff) >> 4);
                    puVar21 = puVar21 + 1;
                    if (iVar22 >= cbMax) {
                      return;
                    }
                    param_9 = param_9 + 1;
                    param_1 = (unsigned short *)((int)param_1 + 1);
                  }
                  param_9 = param_9 + -8;
                  for (local_34 = 0; local_34 < 4; local_34 = local_34 + 1) {
                    if ((int)param_1 >= iVar17) break;
                    iVar22 = iVar22 + 2;
                    bIA8c = *(unsigned char *)param_9;
                    loIA8 = (unsigned int)bIA8c & 0xf;
                    uVar14 = ((unsigned int)bIA8c >> 4) | ((unsigned int)bIA8c & 0xf0);
                    *puVar21 = (unsigned short)((((loIA8) << 4 |
                                        (((param_14 & 0xff) - (param_18 & 0xff)) * uVar14) / 0xff +
                                        (param_18 & 0xff) >> 4) << 4 |
                                       (((param_15 & 0xff) - (param_19 & 0xff)) * uVar14) / 0xff +
                                       (param_19 & 0xff) >> 4) << 4) |
                              (unsigned short)((((param_16 & 0xff) - (param_20 & 0xff)) * uVar14) / 0xff +
                                       (param_20 & 0xff) >> 4);
                    puVar21 = puVar21 + 1;
                    if (iVar22 >= cbMax) {
                      return;
                    }
                    param_9 = param_9 + 1;
                    param_1 = (unsigned short *)((int)param_1 + 1);
                  }
                  param_9 = param_9 + 4;
                } while ((int)param_1 < iVar17);
                }
              }
              else {
                if (0 < iVar17) {
                  do {
                    iVar22 = iVar22 + 2;
                    bIA8a = *(unsigned char *)param_9;
                    loIA8 = (unsigned int)bIA8a & 0xf;
                    uVar14 = ((unsigned int)bIA8a & 0xf0) | ((unsigned int)bIA8a >> 4);
                    *puVar21 = (unsigned short)((((loIA8) << 4 |
                                        (uVar14 * ((param_14 & 0xff) - (param_18 & 0xff))) / 0xff +
                                        (param_18 & 0xff) >> 4) << 4 |
                                       (uVar14 * ((param_15 & 0xff) - (param_19 & 0xff))) / 0xff +
                                       (param_19 & 0xff) >> 4) << 4) |
                              (unsigned short)((uVar14 * ((param_16 & 0xff) - (param_20 & 0xff))) / 0xff +
                                       (param_20 & 0xff) >> 4);
                    puVar21 = puVar21 + 1;
                    if (iVar22 >= cbMax) {
                      return;
                    }
                    param_9 = param_9 + 1;
                    param_1 = (unsigned short *)((int)param_1 + 1);
                  } while ((int)param_1 < iVar17);
                }
              }
              if ((param_7 != 0) && (iVar16 = 0, puVar9 = puVar21 + -1, 0 < iVar17)) {
                do {
                  iVar22 = iVar22 + 2;
                  *puVar21 = *puVar9;
                  puVar21 = puVar21 + 1;
                  puVar9 = puVar9 + -1;
                  if (iVar22 >= cbMax) {
                    return;
                  }
                  iVar16 = iVar16 + 1;
                } while (iVar16 < iVar17);
              }
              param_4 = param_4 + *(int *)(iVar5 + 8);
              local_44 = local_44 + 1;
            } while ((int)local_44 < iVar15);
          }
          if ((param_8 != 0) && (puVar9 = puVar21, local_44 = 0, 0 < iVar15)) {
            do {
              iVar5 = iVar17 * 2;
              if (param_7 == 0) {
                iVar5 = iVar17;
              }
              puVar9 = puVar9 + -iVar5;
              puVar2 = puVar9;
              iVar5 = iVar17 * 2;
              if (param_7 == 0) {
                iVar5 = iVar17;
              }
              for (; 0 < iVar5; iVar5 = iVar5 + -1) {
                iVar22 = iVar22 + 2;
                *puVar21 = *puVar2;
                puVar21 = puVar21 + 1;
                if (iVar22 >= cbMax) {
                  return;
                }
                puVar2 = puVar2 + 1;
              }
              local_44 = local_44 + 1;
            } while ((int)local_44 < iVar15);
          }
        }
        else {
          iVar17 = 1 << *(int *)(iVar5 + 0x20);
          iVar15 = 1 << *(int *)(iVar5 + 0x24);
          param_1 = (unsigned short *)0x0;
          if (0 < iVar15) {
            do {
              pbVar12 = param_4;
              if (((unsigned int)param_1 & param_22) != 0) {
                iVar16 = 0;
                if (0 < iVar17) {
                  do {
                    pbVar12 = pbVar12 + 4;
                    for (param_9 = 0; param_9 < 4; param_9 = param_9 + 1) {
                      if (iVar16 >= iVar17) break;
                      bVar11 = *pbVar12;
                      *(unsigned char *)puVar21 = bVar11 << 4 | bVar11 >> 4;
                      puVar21 = (unsigned short *)((int)puVar21 + 1);
                      iVar22 = iVar22 + 1;
                      if (iVar22 >= cbMax) {
                        return;
                      }
                      pbVar12 = pbVar12 + 1;
                      iVar16 = iVar16 + 1;
                    }
                    pbVar12 = pbVar12 + -8;
                    for (param_9 = 0; param_9 < 4; param_9 = param_9 + 1) {
                      if (iVar16 >= iVar17) break;
                      bVar11 = *pbVar12;
                      *(unsigned char *)puVar21 = bVar11 >> 4 | bVar11 << 4;
                      puVar21 = (unsigned short *)((int)puVar21 + 1);
                      iVar22 = iVar22 + 1;
                      if (iVar22 >= cbMax) {
                        return;
                      }
                      pbVar12 = pbVar12 + 1;
                      iVar16 = iVar16 + 1;
                    }
                    pbVar12 = pbVar12 + 4;
                  } while (iVar16 < iVar17);
                }
              }
              else {
                param_9 = 0;
                if (0 < iVar17) {
                  do {
                    bVar11 = *pbVar12;
                    *(unsigned char *)puVar21 = bVar11 >> 4 | bVar11 << 4;
                    puVar21 = (unsigned short *)((int)puVar21 + 1);
                    iVar22 = iVar22 + 1;
                    if (iVar22 >= cbMax) {
                      return;
                    }
                    pbVar12 = pbVar12 + 1;
                    param_9 = param_9 + 1;
                  } while (param_9 < iVar17);
                }
              }
              if ((param_7 != 0) && (iVar16 = 0, puVar9 = (unsigned short *)((int)puVar21 + -1), 0 < iVar17)) {
                do {
                  *(unsigned char *)puVar21 = *(unsigned char *)puVar9;
                  puVar21 = (unsigned short *)((int)puVar21 + 1);
                  puVar9 = (unsigned short *)((int)puVar9 + -1);
                  iVar22 = iVar22 + 1;
                  if (iVar22 >= cbMax) {
                    return;
                  }
                  iVar16 = iVar16 + 1;
                } while (iVar16 < iVar17);
              }
              param_4 = param_4 + *(int *)(iVar5 + 8);
              param_1 = (unsigned short *)((int)param_1 + 1);
            } while ((int)param_1 < iVar15);
          }
          if ((param_8 != 0) && (puVar9 = puVar21, param_1 = (unsigned short *)0x0, 0 < iVar15)) {
            do {
              iVar16 = iVar17 * 2;
              if (param_7 == 0) {
                iVar16 = iVar17;
              }
              puVar9 = (unsigned short *)((int)puVar9 - iVar16);
              puVar2 = puVar9;
              iVar16 = iVar17 * 2;
              if (param_7 == 0) {
                iVar16 = iVar17;
              }
              for (; 0 < iVar16; iVar16 = iVar16 + -1) {
                *(unsigned char *)puVar21 = (unsigned char)*puVar2;
                puVar21 = (unsigned short *)((int)puVar21 + 1);
                iVar22 = iVar22 + 1;
                if (iVar22 >= cbMax) {
                  return;
                }
                puVar2 = (unsigned short *)((int)puVar2 + 1);
              }
              param_1 = (unsigned short *)((int)param_1 + 1);
            } while ((int)param_1 < iVar15);
          }
        }
      }
      else if (param_6 == 4) {
        param_9 = 1 << *(int *)(iVar5 + 0x20);
        iVar15 = 1 << *(int *)(iVar5 + 0x24);
        param_1 = (unsigned short *)0x0;
        if (0 < iVar15) {
          do {
            pbVar12 = param_4;
            if ((param_22 & (unsigned int)param_1) != 0) {
              iVar16 = 0;
              if (0 < param_9) {
                do {
                  pbVar12 = pbVar12 + 4;
                  for (local_24 = 0; local_24 < 4; local_24 = local_24 + 1) {
                    if (iVar16 >= param_9) break;
                    *(unsigned char *)puVar21 = *pbVar12;
                    puVar21 = (unsigned short *)((int)puVar21 + 1);
                    iVar22 = iVar22 + 1;
                    if (iVar22 >= cbMax) {
                      return;
                    }
                    pbVar12 = pbVar12 + 1;
                    iVar16 = iVar16 + 1;
                  }
                  pbVar12 = pbVar12 + -8;
                  for (iVar13 = 0; iVar13 < 4; iVar13 = iVar13 + 1) {
                    if (iVar16 >= param_9) break;
                    *(unsigned char *)puVar21 = *pbVar12;
                    puVar21 = (unsigned short *)((int)puVar21 + 1);
                    iVar22 = iVar22 + 1;
                    if (iVar22 >= cbMax) {
                      return;
                    }
                    pbVar12 = pbVar12 + 1;
                    iVar16 = iVar16 + 1;
                  }
                  pbVar12 = pbVar12 + 4;
                } while (iVar16 < param_9);
              }
            }
            else {
              iVar16 = 0;
              if (0 < param_9) {
                do {
                  *(unsigned char *)puVar21 = *pbVar12;
                  puVar21 = (unsigned short *)((int)puVar21 + 1);
                  iVar22 = iVar22 + 1;
                  if (iVar22 >= cbMax) {
                    return;
                  }
                  pbVar12 = pbVar12 + 1;
                  iVar16 = iVar16 + 1;
                } while (iVar16 < param_9);
              }
            }
            if ((param_7 != 0) && (local_24 = 0, puVar9 = (unsigned short *)((int)puVar21 + -1), 0 < param_9)) {
              do {
                *(unsigned char *)puVar21 = *(unsigned char *)puVar9;
                puVar21 = (unsigned short *)((int)puVar21 + 1);
                puVar9 = (unsigned short *)((int)puVar9 + -1);
                iVar22 = iVar22 + 1;
                if (iVar22 >= cbMax) {
                  return;
                }
                local_24 = local_24 + 1;
              } while (local_24 < param_9);
            }
            param_4 = param_4 + *(int *)(iVar5 + 8);
            param_1 = (unsigned short *)((int)param_1 + 1);
          } while ((int)param_1 < iVar15);
        }
        if ((param_8 != 0) && (puVar9 = puVar21, param_1 = (unsigned short *)0x0, 0 < iVar15)) {
          do {
            iVar5 = param_9 * 2;
            if (param_7 == 0) {
              iVar5 = param_9;
            }
            puVar9 = (unsigned short *)((int)puVar9 - iVar5);
            puVar2 = puVar9;
            iVar5 = param_9 * 2;
            if (param_7 == 0) {
              iVar5 = param_9;
            }
            for (; 0 < iVar5; iVar5 = iVar5 + -1) {
              *(unsigned char *)puVar21 = (unsigned char)*puVar2;
              puVar21 = (unsigned short *)((int)puVar21 + 1);
              iVar22 = iVar22 + 1;
              if (iVar22 >= cbMax) {
                return;
              }
              puVar2 = (unsigned short *)((int)puVar2 + 1);
            }
            param_1 = (unsigned short *)((int)param_1 + 1);
          } while ((int)param_1 < iVar15);
        }
      }
    } else if ((param_3 == 2) && (param_6 == 0)) {
      uVar14 = 1 << *(int *)(iVar5 + 0x20);
      local_44 = 0;
      iVar15 = 1 << *(int *)(iVar5 + 0x24);
      if (0 < iVar15) {
        do {
          param_9 = 0;
          puVar9 = puVar21;
          iVar17 = iVar22;
          pbVar12 = param_4;
          if ((param_22 & local_44) != 0) {
            uVar6 = local_44;
            if (0 < (int)uVar14) {
              while (1) {
                uVar4 = FUN_100271f0(*(unsigned short *)(pbVar12 + 4));
                *puVar21 = uVar4;
                puVar21 = puVar21 + 1;
                pbVar12 = pbVar12 + 2;
                iVar22 = iVar22 + 2;
                param_9 = param_9 + 1;
                if (param_9 >= (int)uVar14) break;
                if (iVar22 >= cbMax) {
                  return;
                }
                uVar4 = FUN_100271f0(*(unsigned short *)(pbVar12 + 4));
                *puVar21 = uVar4;
                puVar21 = puVar21 + 1;
                pbVar12 = pbVar12 + 2;
                iVar22 = iVar22 + 2;
                param_9 = param_9 + 1;
                if (param_9 >= (int)uVar14) break;
                if (iVar22 >= cbMax) {
                  return;
                }
                uVar4 = FUN_100271f0(*(unsigned short *)(pbVar12 + -4));
                *puVar21 = uVar4;
                puVar21 = puVar21 + 1;
                pbVar12 = pbVar12 + 2;
                iVar22 = iVar22 + 2;
                param_9 = param_9 + 1;
                if (param_9 >= (int)uVar14) break;
                if (iVar22 >= cbMax) {
                  return;
                }
                uVar4 = FUN_100271f0(*(unsigned short *)(pbVar12 + -4));
                *puVar21 = uVar4;
                puVar21 = puVar21 + 1;
                pbVar12 = pbVar12 + 2;
                iVar22 = iVar22 + 2;
                param_9 = param_9 + 1;
                if (param_9 >= (int)uVar14) break;
                if (iVar22 >= cbMax) {
                  return;
                }
              }
              puVar9 = puVar21;
              iVar17 = iVar22;
            }
          }
          else {
            if (0 < (int)uVar14) {
              do {
                uVar4 = FUN_100271f0(*(unsigned short *)pbVar12);
                *puVar21 = uVar4;
                iVar22 = iVar22 + 2;
                puVar21 = puVar21 + 1;
                pbVar12 = pbVar12 + 2;
                if (iVar22 >= cbMax) {
                  return;
                }
                param_9 = param_9 + 1;
                puVar9 = puVar21;
                iVar17 = iVar22;
              } while (param_9 < (int)uVar14);
            }
          }
          iVar22 = iVar17;
          puVar21 = puVar9;
          if ((param_7 != 0) && (iVar17 = 0, puVar9 = puVar21 + -1, 0 < (int)uVar14)) {
            do {
              iVar22 = iVar22 + 2;
              *puVar21 = *puVar9;
              puVar21 = puVar21 + 1;
              puVar9 = puVar9 + -1;
              if (iVar22 >= cbMax) {
                return;
              }
              iVar17 = iVar17 + 1;
            } while (iVar17 < (int)uVar14);
          }
          param_4 = param_4 + *(int *)(iVar5 + 8);
          local_44 = local_44 + 1;
        } while ((int)local_44 < iVar15);
      }
      if ((param_8 != 0) && (puVar9 = puVar21, local_44 = 0, 0 < iVar15)) {
        do {
          uVar6 = uVar14 * 2;
          if (param_7 == 0) {
            uVar6 = uVar14;
          }
          puVar9 = puVar9 + -uVar6;
          puVar2 = puVar9;
          uVar6 = uVar14 * 2;
          if (param_7 == 0) {
            uVar6 = uVar14;
          }
          for (; 0 < (int)uVar6; uVar6 = uVar6 - 1) {
            iVar22 = iVar22 + 2;
            *puVar21 = *puVar2;
            puVar21 = puVar21 + 1;
            if (iVar22 >= cbMax) {
              return;
            }
            puVar2 = puVar2 + 1;
          }
          local_44 = local_44 + 1;
        } while ((int)local_44 < iVar15);
      }
    }
    }
  }
}


#endif /* BR_MATCHING_BUILD */
