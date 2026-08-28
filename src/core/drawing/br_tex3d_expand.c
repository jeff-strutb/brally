/* 0x100250D0 BrTex3dExpand — matching transcription from Ghidra decomp.
 * COLORING WALL at insn 3 (orig+0x7). Verdict in docs/idioms-A.md.
 * Do not re-run the probes listed there. */
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

unsigned int FUN_100271f0(unsigned int);

void BrTex3dExpand(unsigned short *param_1,int param_2,int param_3,unsigned char *param_4,int param_5,int param_6,
                 int param_7,int param_8,int param_9,int param_10,int param_11,unsigned char param_12,
                 int param_13,unsigned char param_14,unsigned char param_15,unsigned char param_16,unsigned char param_17,unsigned char param_18,
                 unsigned char param_19,unsigned char param_20,unsigned char param_21,int param_22)

{
  int cVar1;
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
  cbMax = param_2;
  puVar21 = param_1;
  for (iVar10 = param_9; iVar10 < param_10; iVar10 = iVar10 + 1) {
    iVar5 = iVar10 * 0x40 + param_11;
    param_4 = (unsigned char *)(iVar3 + *(int *)(iVar10 * 0x40 + 0xc + param_11) * 8);
    puVar9 = puVar21;
    if (param_3 == 0) {
      if (param_6 == 2) {
        if (((param_12 & 2) != 0) && (iVar10 == 1)) {
          iVar17 = 1 << (*(int *)(param_11 + 0x60) - 1);
          param_1 = (unsigned short *)0x0;
          iVar15 = 1 << *(int *)(param_11 + 0x64);
          if (0 < iVar15) {
            do {
              if (((unsigned int)param_1 & param_22) == 0) {
                iVar16 = 0;
                pbVar12 = param_4;
                if (0 < iVar17) {
                  do {
                    bVar11 = *pbVar12;
                    *puVar21 = (unsigned short)(bVar11 >> 4);
                    if (iVar22 + 2 >= cbMax) {
                      return;
                    }
                    iVar22 = iVar22 + 4;
                    puVar21[1] = (unsigned short)(bVar11 & 0xf);
                    puVar21 = puVar21 + 2;
                    if (iVar22 >= cbMax) {
                      return;
                    }
                    pbVar12 = pbVar12 + 1;
                    iVar16 = iVar16 + 1;
                  } while (iVar16 < iVar17);
                }
              }
              else {
                iVar16 = 0;
                local_34 = 0;
                pbVar12 = param_4;
                if (0 < iVar17) {
                  do {
                    pbVar12 = pbVar12 + 4;
                    local_24 = 0;
                    do {
                      if (iVar17 <= iVar16) break;
                      bVar11 = *pbVar12;
                      *puVar21 = (unsigned short)(bVar11 >> 4);
                      if (iVar22 + 2 >= cbMax) {
                        return;
                      }
                      iVar22 = iVar22 + 4;
                      puVar21[1] = (unsigned short)(bVar11 & 0xf);
                      puVar21 = puVar21 + 2;
                      if (iVar22 >= cbMax) {
                        return;
                      }
                      pbVar12 = pbVar12 + 1;
                      iVar16 = iVar16 + 1;
                      local_24 = local_24 + 1;
                      local_34 = iVar16;
                    } while (local_24 < 4);
                    pbVar12 = pbVar12 + -8;
                    iVar13 = 0;
                    do {
                      if (iVar17 <= iVar16) break;
                      bVar11 = *pbVar12;
                      *puVar21 = (unsigned short)(bVar11 >> 4);
                      if (iVar22 + 2 >= cbMax) {
                        return;
                      }
                      iVar22 = iVar22 + 4;
                      puVar21[1] = (unsigned short)(bVar11 & 0xf);
                      puVar21 = puVar21 + 2;
                      if (iVar22 >= cbMax) {
                        return;
                      }
                      pbVar12 = pbVar12 + 1;
                      iVar16 = local_34 + 1;
                      iVar13 = iVar13 + 1;
                      local_34 = iVar16;
                    } while (iVar13 < 4);
                    pbVar12 = pbVar12 + 4;
                  } while (iVar16 < iVar17);
                }
              }
              if ((param_7 != 0) && (iVar16 = 0, puVar9 = puVar21, 0 < iVar17)) {
                do {
                  *puVar21 = puVar9[-1];
                  puVar9 = puVar9 + -2;
                  if (iVar22 + 2 >= cbMax) {
                    return;
                  }
                  iVar22 = iVar22 + 4;
                  puVar21[1] = *puVar9;
                  puVar21 = puVar21 + 2;
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
          puVar9 = puVar21;
          if ((param_8 != 0) && (param_1 = (unsigned short *)0x0, 0 < iVar15)) {
            do {
              iVar16 = iVar17 * 2;
              if (param_7 == 0) {
                iVar16 = iVar17;
              }
              puVar21 = puVar21 + iVar16 * -2;
              puVar2 = puVar21;
              iVar16 = iVar17 * 2;
              if (param_7 == 0) {
                iVar16 = iVar17;
              }
              for (; 0 < iVar16; iVar16 = iVar16 + -1) {
                *puVar9 = *puVar2;
                if (iVar22 + 2 >= cbMax) {
                  return;
                }
                iVar22 = iVar22 + 4;
                puVar9[1] = puVar2[1];
                puVar9 = puVar9 + 2;
                if (iVar22 >= cbMax) {
                  return;
                }
                puVar2 = puVar2 + 2;
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
              param_1 = (unsigned short *)0x0;
              if ((local_44 & param_22) == 0) {
                if (0 < iVar17) {
                  do {
                    bCI4a = *(unsigned char *)param_9;
                    pal = *(unsigned short *)(param_5 + (unsigned int)(bCI4a >> 4) * 2);
                    uVar4 = FUN_100271f0(pal);
                    *puVar21 = uVar4;
                    if (iVar22 + 2 >= cbMax) {
                      return;
                    }
                    pal = *(unsigned short *)(param_5 + (bCI4a & 0xf) * 2);
                    uVar4 = FUN_100271f0(pal);
                    puVar21[1] = uVar4;
                    iVar22 = iVar22 + 4;
                    puVar21 = puVar21 + 2;
                    if (iVar22 >= cbMax) {
                      return;
                    }
                    param_9 = param_9 + 1;
                    param_1 = (unsigned short *)((int)param_1 + 1);
                  } while ((int)param_1 < iVar17);
                }
              }
              else if (0 < iVar17) {
                do {
                  local_38 = 0;
                  param_9 = param_9 + 4;
                  do {
                    if (iVar17 <= (int)param_1) break;
                    bCI4b = *(unsigned char *)param_9;
                    uVar4 = FUN_100271f0(*(unsigned short *)(param_5 + (unsigned int)(bCI4b >> 4) * 2));
                    *puVar21 = uVar4;
                    if (iVar22 + 2 >= cbMax) {
                      return;
                    }
                    uVar4 = FUN_100271f0(*(unsigned short *)(param_5 + (bCI4b & 0xf) * 2));
                    puVar21[1] = uVar4;
                    iVar22 = iVar22 + 4;
                    puVar21 = puVar21 + 2;
                    if (iVar22 >= cbMax) {
                      return;
                    }
                    param_9 = param_9 + 1;
                    param_1 = (unsigned short *)((int)param_1 + 1);
                    local_38 = local_38 + 1;
                  } while ((int)local_38 < 4);
                  local_38 = 0;
                  param_9 = param_9 + -8;
                  do {
                    if (iVar17 <= (int)param_1) break;
                    bCI4c = *(unsigned char *)param_9;
                    uVar4 = FUN_100271f0(*(unsigned short *)(param_5 + (unsigned int)(bCI4c >> 4) * 2));
                    *puVar21 = uVar4;
                    if (iVar22 + 2 >= cbMax) {
                      return;
                    }
                    uVar4 = FUN_100271f0(*(unsigned short *)(param_5 + (bCI4c & 0xf) * 2));
                    puVar21[1] = uVar4;
                    iVar22 = iVar22 + 4;
                    puVar21 = puVar21 + 2;
                    if (iVar22 >= cbMax) {
                      return;
                    }
                    param_9 = param_9 + 1;
                    param_1 = (unsigned short *)((int)param_1 + 1);
                    local_38 = local_38 + 1;
                  } while ((int)local_38 < 4);
                  param_9 = param_9 + 4;
                } while ((int)param_1 < iVar17);
              }
              if ((param_7 != 0) && (iVar16 = 0, puVar9 = puVar21, 0 < iVar17)) {
                do {
                  *puVar21 = puVar9[-1];
                  puVar9 = puVar9 + -2;
                  if (iVar22 + 2 >= cbMax) {
                    return;
                  }
                  iVar22 = iVar22 + 4;
                  puVar21[1] = *puVar9;
                  puVar21 = puVar21 + 2;
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
          puVar9 = puVar21;
          if ((param_8 != 0) && (local_44 = 0, 0 < iVar15)) {
            do {
              iVar5 = iVar17 * 2;
              if (param_7 == 0) {
                iVar5 = iVar17;
              }
              puVar21 = puVar21 + iVar5 * -2;
              puVar2 = puVar21;
              iVar5 = iVar17 * 2;
              if (param_7 == 0) {
                iVar5 = iVar17;
              }
              for (; 0 < iVar5; iVar5 = iVar5 + -1) {
                *puVar9 = *puVar2;
                if (iVar22 + 2 >= cbMax) {
                  return;
                }
                iVar22 = iVar22 + 4;
                puVar9[1] = puVar2[1];
                puVar9 = puVar9 + 2;
                if (iVar22 >= cbMax) {
                  return;
                }
                puVar2 = puVar2 + 2;
              }
              local_44 = local_44 + 1;
            } while ((int)local_44 < iVar15);
          }
                }
      }
      else if (param_6 == 4) {
        cVar1 = *(int *)(iVar5 + 0x20);
        if (param_13 == 1) {
          iVar17 = 1 << (cVar1 - 1);
          iVar15 = 1 << *(int *)(iVar5 + 0x24);
          local_3c = 0;
          if (0 < iVar15) {
            do {
              local_64 = param_4;
              local_54 = 0;
              if ((local_3c & param_22) == 0) {
                if (0 < iVar17) {
                  puVar9 = puVar21;
                  do {
                    lo0 = param_18 & 0xff;
                    iVar16 = (param_14 & 0xff) - lo0;
                    uVar6 = param_19 & 0xff;
                    iVar13 = (param_15 & 0xff) - uVar6;
                    uVar7 = param_20 & 0xff;
                    iVar18 = (param_16 & 0xff) - uVar7;
                    uVar8 = param_21 & 0xff;
                    iVar20 = (param_17 & 0xff) - uVar8;
                    bVar11 = *local_64;
                    bI4inten = (unsigned char)(bVar11 >> 4 | bVar11 & 0xf0);
                    uVar19 = (unsigned int)bI4inten;
                    chR = (unsigned char)((int)((int)(uVar19 * iVar16) / 0xff + lo0) >> 3);
                    chG = (unsigned char)((int)((int)(uVar19 * iVar13) / 0xff + uVar6) >> 3);
                    chB = (unsigned char)((int)((int)(uVar19 * iVar18) / 0xff + uVar7) >> 3);
                    chA = (unsigned char)((int)((int)(uVar19 * iVar20) / 0xff + uVar8) >> 7);
                    *puVar9 = (unsigned short)(((((unsigned int)chA << 5 | (unsigned int)chR) << 5 |
                                        (unsigned int)chG) << 5) | (unsigned int)chB);
                    if (iVar22 + 2 >= cbMax) {
                      return;
                    }
                    iVar22 = iVar22 + 4;
                    puVar21 = puVar9 + 2;
                    uVar19 = (unsigned int)(unsigned char)(bVar11 << 4 | bVar11 & 0xf);
                    chR = (unsigned char)((int)((int)(uVar19 * iVar16) / 0xff + lo0) >> 3);
                    chG = (unsigned char)((int)((int)(uVar19 * iVar13) / 0xff + uVar6) >> 3);
                    chB = (unsigned char)((int)((int)(uVar19 * iVar18) / 0xff + uVar7) >> 3);
                    chA = (unsigned char)((int)((int)(uVar19 * iVar20) / 0xff + uVar8) >> 7);
                    puVar9[1] = (unsigned short)(((((unsigned int)chA << 5 | (unsigned int)chR) << 5 |
                                          (unsigned int)chG) << 5) | (unsigned int)chB);
                    if (iVar22 >= cbMax) {
                      return;
                    }
                    local_64 = local_64 + 1;
                    local_54 = local_54 + 1;
                    puVar9 = puVar21;
                  } while (local_54 < iVar17);
                }
              }
              else if (0 < iVar17) {
                do {
                  local_34 = 0;
                  local_64 = local_64 + 4;
                  do {
                    puVar9 = puVar21;
                    if (iVar17 <= local_54) break;
                    uVar19 = param_18 & 0xff;
                    bVar11 = *local_64;
                    iVar16 = (param_14 & 0xff) - uVar19;
                    uVar14 = (unsigned int)(unsigned char)(bVar11 >> 4 | bVar11 & 0xf0);
                    uVar6 = param_19 & 0xff;
                    iVar13 = (param_15 & 0xff) - uVar6;
                    param_9 = (int)((int)(uVar14 * iVar16) / 0xff + uVar19) >> 3 & 0xff;
                    uVar7 = param_20 & 0xff;
                    iVar20 = (param_16 & 0xff) - uVar7;
                    uVar8 = param_21 & 0xff;
                    iVar18 = (param_17 & 0xff) - uVar8;
                    *puVar21 = (unsigned short)(((((int)((int)(iVar18 * uVar14) / 0xff + uVar8) >> 7 & 0xffU
                                          ) << 5 | (unsigned int)param_9) << 5 |
                                        (int)((int)(iVar13 * uVar14) / 0xff + uVar6) >> 3 & 0xffU)
                                       << 5) |
                               (unsigned short)(unsigned char)((int)((int)(iVar20 * uVar14) / 0xff + uVar7) >> 3);
                    if (iVar22 + 2 >= cbMax) {
                      return;
                    }
                    iVar22 = iVar22 + 4;
                    puVar9 = puVar21 + 2;
                    uVar14 = (unsigned int)(unsigned char)(bVar11 << 4 | bVar11 & 0xf);
                    param_9 = (int)((int)(uVar14 * iVar16) / 0xff + uVar19) >> 3 & 0xff;
                    puVar21[1] = (unsigned short)(((((int)((int)(iVar18 * uVar14) / 0xff + uVar8) >> 7 &
                                            0xffU) << 5 | (unsigned int)param_9) << 5 |
                                          (int)((int)(iVar13 * uVar14) / 0xff + uVar6) >> 3 & 0xffU)
                                         << 5) |
                                 (unsigned short)(unsigned char)((int)((int)(iVar20 * uVar14) / 0xff + uVar7) >> 3);
                    if (iVar22 >= cbMax) {
                      return;
                    }
                    local_64 = local_64 + 1;
                    local_54 = local_54 + 1;
                    local_34 = local_34 + 1;
                    puVar21 = puVar9;
                  } while (local_34 < 4);
                  local_34 = 0;
                  local_64 = local_64 + -8;
                  do {
                    puVar21 = puVar9;
                    if (iVar17 <= local_54) break;
                    bVar11 = *local_64;
                    uVar7 = param_18 & 0xff;
                    iVar16 = (param_14 & 0xff) - uVar7;
                    uVar14 = (unsigned int)(unsigned char)(bVar11 >> 4 | bVar11 & 0xf0);
                    uVar8 = param_19 & 0xff;
                    iVar13 = (param_15 & 0xff) - uVar8;
                    uVar19 = param_20 & 0xff;
                    param_9 = (int)((int)(uVar14 * iVar13) / 0xff + uVar8) >> 3 & 0xff;
                    iVar18 = (param_16 & 0xff) - uVar19;
                    uVar6 = param_21 & 0xff;
                    iVar20 = (param_17 & 0xff) - uVar6;
                    *puVar9 = (unsigned short)(((((int)((int)(uVar14 * iVar20) / 0xff + uVar6) >> 7 & 0xffU)
                                         << 5 | (int)((int)(uVar14 * iVar16) / 0xff + uVar7) >> 3 &
                                                0xffU) << 5 | (unsigned int)param_9) << 5) |
                              (unsigned short)(unsigned char)((int)((int)(uVar14 * iVar18) / 0xff + uVar19) >> 3);
                    if (iVar22 + 2 >= cbMax) {
                      return;
                    }
                    iVar22 = iVar22 + 4;
                    puVar21 = puVar9 + 2;
                    uVar14 = (unsigned int)(unsigned char)(bVar11 << 4 | bVar11 & 0xf);
                    param_9 = (int)((int)(uVar14 * iVar13) / 0xff + uVar8) >> 3 & 0xff;
                    puVar9[1] = (unsigned short)(((((int)((int)(uVar14 * iVar20) / 0xff + uVar6) >> 7 &
                                           0xffU) << 5 |
                                          (int)((int)(uVar14 * iVar16) / 0xff + uVar7) >> 3 & 0xffU)
                                          << 5 | (unsigned int)param_9) << 5) |
                                (unsigned short)(unsigned char)((int)((int)(uVar14 * iVar18) / 0xff + uVar19) >> 3);
                    if (iVar22 >= cbMax) {
                      return;
                    }
                    local_64 = local_64 + 1;
                    local_54 = local_54 + 1;
                    local_34 = local_34 + 1;
                    puVar9 = puVar21;
                  } while (local_34 < 4);
                  local_64 = local_64 + 4;
                } while (local_54 < iVar17);
              }
              if ((param_7 != 0) && (iVar16 = 0, puVar9 = puVar21, 0 < iVar17)) {
                do {
                  *puVar21 = puVar9[-1];
                  puVar9 = puVar9 + -2;
                  if (iVar22 + 2 >= cbMax) {
                    return;
                  }
                  iVar22 = iVar22 + 4;
                  puVar21[1] = *puVar9;
                  puVar21 = puVar21 + 2;
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
          puVar9 = puVar21;
          if ((param_8 != 0) && (local_3c = 0, 0 < iVar15)) {
            do {
              iVar5 = iVar17 * 2;
              if (param_7 == 0) {
                iVar5 = iVar17;
              }
              puVar21 = puVar21 + iVar5 * -2;
              puVar2 = puVar21;
              iVar5 = iVar17 * 2;
              if (param_7 == 0) {
                iVar5 = iVar17;
              }
              for (; 0 < iVar5; iVar5 = iVar5 + -1) {
                *puVar9 = *puVar2;
                if (iVar22 + 2 >= cbMax) {
                  return;
                }
                iVar22 = iVar22 + 4;
                puVar9[1] = puVar2[1];
                puVar9 = puVar9 + 2;
                if (iVar22 >= cbMax) {
                  return;
                }
                puVar2 = puVar2 + 2;
              }
              local_3c = local_3c + 1;
            } while ((int)local_3c < iVar15);
          }
        }
        else {
          iVar17 = 1 << (cVar1 - 1);
          iVar15 = 1 << *(int *)(iVar5 + 0x24);
          local_38 = 0;
          if (0 < iVar15) {
            do {
              if ((local_38 & param_22) == 0) {
                param_9 = 0;
                pbVar12 = param_4;
                if (0 < iVar17) {
                  do {
                    bVar11 = *pbVar12;
                    *(unsigned char *)puVar21 = bVar11 & 0xf0 | bVar11 >> 4;
                    if (iVar22 + 1 >= cbMax) {
                      return;
                    }
                    *(unsigned char *)((int)puVar21 + 1) = bVar11 << 4 | bVar11 & 0xf;
                    puVar21 = puVar21 + 1;
                    iVar22 = iVar22 + 2;
                    if (iVar22 >= cbMax) {
                      return;
                    }
                    pbVar12 = pbVar12 + 1;
                    param_9 = param_9 + 1;
                  } while (param_9 < iVar17);
                }
              }
              else {
                iVar16 = 0;
                param_9 = 0;
                pbVar12 = param_4;
                if (0 < iVar17) {
                  do {
                    pbVar12 = pbVar12 + 4;
                    param_1 = (unsigned short *)0x0;
                    do {
                      if (iVar17 <= iVar16) break;
                      bVar11 = *pbVar12;
                      *(unsigned char *)puVar21 = bVar11 >> 4 | bVar11 & 0xf0;
                      if (iVar22 + 1 >= cbMax) {
                        return;
                      }
                      *(unsigned char *)((int)puVar21 + 1) = bVar11 << 4 | bVar11 & 0xf;
                      puVar21 = puVar21 + 1;
                      iVar22 = iVar22 + 2;
                      if (iVar22 >= cbMax) {
                        return;
                      }
                      pbVar12 = pbVar12 + 1;
                      iVar16 = param_9 + 1;
                      param_1 = (unsigned short *)((int)param_1 + 1);
                      param_9 = iVar16;
                    } while ((int)param_1 < 4);
                    pbVar12 = pbVar12 + -8;
                    param_1 = (unsigned short *)0x0;
                    do {
                      if (iVar17 <= iVar16) break;
                      bVar11 = *pbVar12;
                      *(unsigned char *)puVar21 = bVar11 & 0xf0 | bVar11 >> 4;
                      if (iVar22 + 1 >= cbMax) {
                        return;
                      }
                      *(unsigned char *)((int)puVar21 + 1) = bVar11 & 0xf | bVar11 << 4;
                      puVar21 = puVar21 + 1;
                      iVar22 = iVar22 + 2;
                      if (iVar22 >= cbMax) {
                        return;
                      }
                      pbVar12 = pbVar12 + 1;
                      iVar16 = param_9 + 1;
                      param_1 = (unsigned short *)((int)param_1 + 1);
                      param_9 = iVar16;
                    } while ((int)param_1 < 4);
                    pbVar12 = pbVar12 + 4;
                  } while (iVar16 < iVar17);
                }
              }
              if ((param_7 != 0) && (iVar16 = 0, puVar9 = puVar21, 0 < iVar17)) {
                do {
                  *(unsigned char *)puVar21 = *(unsigned char *)((int)puVar9 + -1);
                  puVar9 = puVar9 + -1;
                  if (iVar22 + 1 >= cbMax) {
                    return;
                  }
                  *(unsigned char *)((int)puVar21 + 1) = (unsigned char)*puVar9;
                  puVar21 = puVar21 + 1;
                  iVar22 = iVar22 + 2;
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
          puVar9 = puVar21;
          if ((param_8 != 0) && (local_38 = 0, 0 < iVar15)) {
            do {
              iVar5 = iVar17 * 2;
              if (param_7 == 0) {
                iVar5 = iVar17;
              }
              puVar21 = puVar21 + -iVar5;
              puVar2 = puVar21;
              iVar5 = iVar17 * 2;
              if (param_7 == 0) {
                iVar5 = iVar17;
              }
              for (; 0 < iVar5; iVar5 = iVar5 + -1) {
                *(unsigned char *)puVar9 = (unsigned char)*puVar2;
                if (iVar22 + 1 >= cbMax) {
                  return;
                }
                *(unsigned char *)((int)puVar9 + 1) = *(unsigned char *)((int)puVar2 + 1);
                puVar9 = puVar9 + 1;
                iVar22 = iVar22 + 2;
                if (iVar22 >= cbMax) {
                  return;
                }
                puVar2 = puVar2 + 1;
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
            if ((param_22 & local_44) == 0) {
              param_1 = (unsigned short *)0x0;
              iVar16 = iVar15;
              pbVar12 = param_4;
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
            else {
              iVar16 = 0;
              param_1 = (unsigned short *)0x0;
              pbVar12 = param_4;
              if (0 < iVar15) {
                do {
                  pbVar12 = pbVar12 + 4;
                  local_34 = 0;
                  iVar13 = iVar15;
                  do {
                    if (iVar15 <= iVar16) break;
                    uVar4 = FUN_100271f0(*(unsigned short *)(param_5 + (unsigned int)*pbVar12 * 2));
                    *puVar21 = uVar4;
                    iVar22 = iVar22 + 2;
                    puVar21 = puVar21 + 1;
                    if (iVar22 >= cbMax) {
                      return;
                    }
                    pbVar12 = pbVar12 + 1;
                    iVar16 = (int)param_1 + 1;
                    iVar13 = local_34 + 1;
                    param_1 = (unsigned short *)iVar16;
                    local_34 = iVar13;
                  } while (iVar13 < 4);
                  pbVar12 = pbVar12 + -8;
                  local_34 = 0;
                  do {
                    if (iVar15 <= iVar16) break;
                    uVar4 = FUN_100271f0(*(unsigned short *)(param_5 + (unsigned int)*pbVar12 * 2));
                    *puVar21 = uVar4;
                    iVar22 = iVar22 + 2;
                    puVar21 = puVar21 + 1;
                    if (iVar22 >= cbMax) {
                      return;
                    }
                    pbVar12 = pbVar12 + 1;
                    iVar16 = (int)param_1 + 1;
                    iVar13 = local_34 + 1;
                    param_1 = (unsigned short *)iVar16;
                    local_34 = iVar13;
                  } while (iVar13 < 4);
                  pbVar12 = pbVar12 + 4;
                } while (iVar16 < iVar15);
              }
            }
            if ((param_7 != 0) && (iVar16 = 0, puVar9 = puVar21, 0 < iVar15)) {
              do {
                puVar9 = puVar9 + -1;
                iVar22 = iVar22 + 2;
                *puVar21 = *puVar9;
                puVar21 = puVar21 + 1;
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
        puVar9 = puVar21;
        if ((param_8 != 0) && (local_44 = 0, 0 < iVar17)) {
          do {
            iVar5 = iVar15 * 2;
            if (param_7 == 0) {
              iVar5 = iVar15;
            }
            puVar21 = puVar21 + -iVar5;
            puVar2 = puVar21;
            iVar5 = iVar15 * 2;
            if (param_7 == 0) {
              iVar5 = iVar15;
            }
            for (; 0 < iVar5; iVar5 = iVar5 + -1) {
              iVar22 = iVar22 + 2;
              *puVar9 = *puVar2;
              puVar9 = puVar9 + 1;
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
        cVar1 = *(int *)(iVar5 + 0x20);
        if (param_13 == one) {
          iVar17 = one << cVar1;
          iVar15 = 1 << *(int *)(iVar5 + 0x24);
          local_44 = 0;
          if (0 < iVar15) {
            do {
              param_9 = (int)param_4;
              param_1 = (unsigned short *)0x0;
              if ((local_44 & param_22) == 0) {
                if (0 < iVar17) {
                  puVar9 = puVar21;
                  do {
                    iVar22 = iVar22 + 2;
                    puVar21 = puVar9 + 1;
                    bIA8a = *(unsigned char *)param_9;
                    uVar14 = bIA8a & 0xf0 | (unsigned int)(bIA8a >> 4);
                    *puVar9 = (unsigned short)((((bIA8a & 0xf) << 4 |
                                        (uVar14 * ((param_14 & 0xff) - (param_18 & 0xff))) / 0xff +
                                        (param_18 & 0xff) >> 4) << 4 |
                                       (uVar14 * ((param_15 & 0xff) - (param_19 & 0xff))) / 0xff +
                                       (param_19 & 0xff) >> 4) << 4) |
                              (unsigned short)((uVar14 * ((param_16 & 0xff) - (param_20 & 0xff))) / 0xff +
                                       (param_20 & 0xff) >> 4);
                    if (iVar22 >= cbMax) {
                      return;
                    }
                    param_9 = param_9 + 1;
                    param_1 = (unsigned short *)((int)param_1 + 1);
                    puVar9 = puVar21;
                  } while ((int)param_1 < iVar17);
                }
              }
              else if (0 < iVar17) {
                do {
                  local_34 = 0;
                  param_9 = param_9 + 4;
                  do {
                    puVar9 = puVar21;
                    if (iVar17 <= (int)param_1) break;
                    iVar22 = iVar22 + 2;
                    puVar9 = puVar21 + 1;
                    bIA8b = *(unsigned char *)param_9;
                    uVar14 = (unsigned int)(bIA8b >> 4) | bIA8b & 0xf0;
                    *puVar21 = (unsigned short)((((bIA8b & 0xf) << 4 |
                                         (((param_14 & 0xff) - (param_18 & 0xff)) * uVar14) / 0xff +
                                         (param_18 & 0xff) >> 4) << 4 |
                                        (((param_15 & 0xff) - (param_19 & 0xff)) * uVar14) / 0xff +
                                        (param_19 & 0xff) >> 4) << 4) |
                               (unsigned short)((((param_16 & 0xff) - (param_20 & 0xff)) * uVar14) / 0xff +
                                        (param_20 & 0xff) >> 4);
                    if (iVar22 >= cbMax) {
                      return;
                    }
                    param_9 = param_9 + 1;
                    param_1 = (unsigned short *)((int)param_1 + 1);
                    local_34 = local_34 + 1;
                    puVar21 = puVar9;
                  } while (local_34 < 4);
                  local_34 = 0;
                  param_9 = param_9 + -8;
                  do {
                    puVar21 = puVar9;
                    if (iVar17 <= (int)param_1) break;
                    iVar22 = iVar22 + 2;
                    puVar21 = puVar9 + 1;
                    bIA8c = *(unsigned char *)param_9;
                    uVar14 = (unsigned int)(bIA8c >> 4) | bIA8c & 0xf0;
                    *puVar9 = (unsigned short)((((bIA8c & 0xf) << 4 |
                                        (((param_14 & 0xff) - (param_18 & 0xff)) * uVar14) / 0xff +
                                        (param_18 & 0xff) >> 4) << 4 |
                                       (((param_15 & 0xff) - (param_19 & 0xff)) * uVar14) / 0xff +
                                       (param_19 & 0xff) >> 4) << 4) |
                              (unsigned short)((((param_16 & 0xff) - (param_20 & 0xff)) * uVar14) / 0xff +
                                       (param_20 & 0xff) >> 4);
                    if (iVar22 >= cbMax) {
                      return;
                    }
                    param_9 = param_9 + 1;
                    param_1 = (unsigned short *)((int)param_1 + 1);
                    local_34 = local_34 + 1;
                    puVar9 = puVar21;
                  } while (local_34 < 4);
                  param_9 = param_9 + 4;
                } while ((int)param_1 < iVar17);
              }
              if ((param_7 != 0) && (iVar16 = 0, puVar9 = puVar21, 0 < iVar17)) {
                do {
                  puVar9 = puVar9 + -1;
                  iVar22 = iVar22 + 2;
                  *puVar21 = *puVar9;
                  puVar21 = puVar21 + 1;
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
          puVar9 = puVar21;
          if ((param_8 != 0) && (local_44 = 0, 0 < iVar15)) {
            do {
              iVar5 = iVar17 * 2;
              if (param_7 == 0) {
                iVar5 = iVar17;
              }
              puVar21 = puVar21 + -iVar5;
              puVar2 = puVar21;
              iVar5 = iVar17 * 2;
              if (param_7 == 0) {
                iVar5 = iVar17;
              }
              for (; 0 < iVar5; iVar5 = iVar5 + -1) {
                iVar22 = iVar22 + 2;
                *puVar9 = *puVar2;
                puVar9 = puVar9 + 1;
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
          iVar17 = 1 << cVar1;
          iVar15 = 1 << *(int *)(iVar5 + 0x24);
          param_1 = (unsigned short *)0x0;
          if (0 < iVar15) {
            do {
              if (((unsigned int)param_1 & param_22) == 0) {
                param_9 = 0;
                pbVar12 = param_4;
                if (0 < iVar17) {
                  do {
                    *(unsigned char *)puVar21 = *pbVar12 >> 4 | *pbVar12 << 4;
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
              else {
                iVar16 = 0;
                pbVar12 = param_4;
                if (0 < iVar17) {
                  do {
                    pbVar12 = pbVar12 + 4;
                    param_9 = 0;
                    do {
                      if (iVar17 <= iVar16) break;
                      *(unsigned char *)puVar21 = *pbVar12 << 4 | *pbVar12 >> 4;
                      puVar21 = (unsigned short *)((int)puVar21 + 1);
                      iVar22 = iVar22 + 1;
                      if (iVar22 >= cbMax) {
                        return;
                      }
                      pbVar12 = pbVar12 + 1;
                      iVar16 = iVar16 + 1;
                      param_9 = param_9 + 1;
                    } while (param_9 < 4);
                    pbVar12 = pbVar12 + -8;
                    param_9 = 0;
                    do {
                      if (iVar17 <= iVar16) break;
                      *(unsigned char *)puVar21 = *pbVar12 >> 4 | *pbVar12 << 4;
                      puVar21 = (unsigned short *)((int)puVar21 + 1);
                      iVar22 = iVar22 + 1;
                      if (iVar22 >= cbMax) {
                        return;
                      }
                      pbVar12 = pbVar12 + 1;
                      iVar16 = iVar16 + 1;
                      param_9 = param_9 + 1;
                    } while (param_9 < 4);
                    pbVar12 = pbVar12 + 4;
                  } while (iVar16 < iVar17);
                }
              }
              if ((param_7 != 0) && (iVar16 = 0, puVar9 = puVar21, 0 < iVar17)) {
                do {
                  puVar9 = (unsigned short *)((int)puVar9 + -1);
                  *(unsigned char *)puVar21 = *(unsigned char *)puVar9;
                  puVar21 = (unsigned short *)((int)puVar21 + 1);
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
          puVar9 = puVar21;
          if ((param_8 != 0) && (param_1 = (unsigned short *)0x0, 0 < iVar15)) {
            do {
              iVar5 = iVar17 * 2;
              if (param_7 == 0) {
                iVar5 = iVar17;
              }
              puVar21 = (unsigned short *)((int)puVar21 - iVar5);
              puVar2 = puVar21;
              iVar5 = iVar17 * 2;
              if (param_7 == 0) {
                iVar5 = iVar17;
              }
              for (; 0 < iVar5; iVar5 = iVar5 + -1) {
                *(unsigned char *)puVar9 = (unsigned char)*puVar2;
                puVar9 = (unsigned short *)((int)puVar9 + 1);
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
        iVar17 = 1 << *(int *)(iVar5 + 0x20);
        iVar15 = 1 << *(int *)(iVar5 + 0x24);
        param_1 = (unsigned short *)0x0;
        if (0 < iVar15) {
          do {
            if ((param_22 & (unsigned int)param_1) == 0) {
              iVar16 = 0;
              pbVar12 = param_4;
              if (0 < iVar17) {
                do {
                  *(unsigned char *)puVar21 = *pbVar12;
                  puVar21 = (unsigned short *)((int)puVar21 + 1);
                  iVar22 = iVar22 + 1;
                  if (iVar22 >= cbMax) {
                    return;
                  }
                  pbVar12 = pbVar12 + 1;
                  iVar16 = iVar16 + 1;
                } while (iVar16 < iVar17);
              }
            }
            else {
              iVar16 = 0;
              pbVar12 = param_4;
              if (0 < iVar17) {
                do {
                  pbVar12 = pbVar12 + 4;
                  iVar13 = 0;
                  do {
                    if (iVar17 <= iVar16) break;
                    *(unsigned char *)puVar21 = *pbVar12;
                    puVar21 = (unsigned short *)((int)puVar21 + 1);
                    iVar22 = iVar22 + 1;
                    if (iVar22 >= cbMax) {
                      return;
                    }
                    pbVar12 = pbVar12 + 1;
                    iVar16 = iVar16 + 1;
                    iVar13 = iVar13 + 1;
                  } while (iVar13 < 4);
                  pbVar12 = pbVar12 + -8;
                  iVar13 = 0;
                  do {
                    if (iVar17 <= iVar16) break;
                    *(unsigned char *)puVar21 = *pbVar12;
                    puVar21 = (unsigned short *)((int)puVar21 + 1);
                    iVar22 = iVar22 + 1;
                    if (iVar22 >= cbMax) {
                      return;
                    }
                    pbVar12 = pbVar12 + 1;
                    iVar16 = iVar16 + 1;
                    iVar13 = iVar13 + 1;
                  } while (iVar13 < 4);
                  pbVar12 = pbVar12 + 4;
                } while (iVar16 < iVar17);
              }
            }
            if ((param_7 != 0) && (local_24 = 0, puVar9 = puVar21, 0 < iVar17)) {
              do {
                puVar9 = (unsigned short *)((int)puVar9 + -1);
                *(unsigned char *)puVar21 = *(unsigned char *)puVar9;
                puVar21 = (unsigned short *)((int)puVar21 + 1);
                iVar22 = iVar22 + 1;
                if (iVar22 >= cbMax) {
                  return;
                }
                local_24 = local_24 + 1;
              } while (local_24 < iVar17);
            }
            param_4 = param_4 + *(int *)(iVar5 + 8);
            param_1 = (unsigned short *)((int)param_1 + 1);
          } while ((int)param_1 < iVar15);
        }
        puVar9 = puVar21;
        if ((param_8 != 0) && (param_1 = (unsigned short *)0x0, 0 < iVar15)) {
          do {
            iVar5 = iVar17 * 2;
            if (param_7 == 0) {
              iVar5 = iVar17;
            }
            puVar21 = (unsigned short *)((int)puVar21 - iVar5);
            puVar2 = puVar21;
            iVar5 = iVar17 * 2;
            if (param_7 == 0) {
              iVar5 = iVar17;
            }
            for (; 0 < iVar5; iVar5 = iVar5 + -1) {
              *(unsigned char *)puVar9 = (unsigned char)*puVar2;
              puVar9 = (unsigned short *)((int)puVar9 + 1);
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
          if ((param_22 & local_44) == 0) {
            pbVar12 = param_4;
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
          else {
            uVar6 = local_44;
            pbVar12 = param_4;
            if (0 < (int)uVar14) {
              for (;;) {
                uVar4 = FUN_100271f0(*(unsigned short *)(pbVar12 + 4));
                *puVar21 = uVar4;
                puVar9 = puVar21 + 1;
                iVar17 = iVar22 + 2;
                if ((int)uVar14 <= param_9 + 1) break;
                if (iVar22 + 2 >= cbMax) {
                  return;
                }
                uVar4 = FUN_100271f0(*(unsigned short *)(pbVar12 + 6));
                puVar21[1] = uVar4;
                puVar9 = puVar21 + 2;
                iVar17 = iVar22 + 4;
                if ((int)uVar14 <= param_9 + 2) break;
                if (iVar22 + 4 >= cbMax) {
                  return;
                }
                uVar4 = FUN_100271f0(*(unsigned short *)pbVar12);
                puVar21[2] = uVar4;
                puVar9 = puVar21 + 3;
                iVar17 = iVar22 + 6;
                if ((int)uVar14 <= param_9 + 3) break;
                if (iVar22 + 6 >= cbMax) {
                  return;
                }
                uVar4 = FUN_100271f0(*(unsigned short *)(pbVar12 + 2))
                ;
                puVar21[3] = uVar4;
                puVar21 = puVar21 + 4;
                pbVar12 = pbVar12 + 8;
                iVar22 = iVar22 + 8;
                param_9 = param_9 + 4;
                puVar9 = puVar21;
                iVar17 = iVar22;
                if ((int)uVar14 <= param_9) break;
                uVar6 = uVar14;
                if (iVar22 >= cbMax) {
                  return;
                }
              }
            }
          }
          iVar22 = iVar17;
          puVar21 = puVar9;
          if ((param_7 != 0) && (iVar17 = 0, puVar9 = puVar21, 0 < (int)uVar14)) {
            do {
              puVar9 = puVar9 + -1;
              iVar22 = iVar22 + 2;
              *puVar21 = *puVar9;
              puVar21 = puVar21 + 1;
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
      puVar9 = puVar21;
      if ((param_8 != 0) && (local_44 = 0, 0 < iVar15)) {
        do {
          uVar6 = uVar14 * 2;
          if (param_7 == 0) {
            uVar6 = uVar14;
          }
          puVar21 = puVar21 + -uVar6;
          puVar2 = puVar21;
          uVar6 = uVar14 * 2;
          if (param_7 == 0) {
            uVar6 = uVar14;
          }
          for (; 0 < (int)uVar6; uVar6 = uVar6 - 1) {
            iVar22 = iVar22 + 2;
            *puVar9 = *puVar2;
            puVar9 = puVar9 + 1;
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
    puVar21 = puVar9;
  }
}


#endif /* BR_MATCHING_BUILD */
