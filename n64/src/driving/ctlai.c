/* ctlai.c -- the computer drivers
 */
#include "tgr/common.h"

/* -- declarations -- */
void func_8022762C(int ctl);
extern float D_802A9C7C;
extern float D_802A9C80;
extern float D_802A9C84;
/* -- end declarations -- */

/* WHAT IT DOES: Drive one computer-controlled car for this frame: the
 * out-of-line entry to the AI driver's main body. */
/* @implements 0x802288B4 tgr BrCtlAi */
void BrCtlAi(int ctl)
{
    func_8022762C(ctl);
}

/* WHAT IT DOES: Does nothing with its argument. An empty function the
 * retail build kept among the AI driver code. */
/* @implements 0x80228E44 tgr BrStub80228E44 */
void BrStub80228E44(int arg0)
{
}

/* WHAT IT DOES: Set up a computer driver's racing lanes: the lane offsets
 * follow from its starting slot, and its lane targets are reset. */
/* @t4-pass 0x802288D4 1 2026-09-26 compiles 17 best 144 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x802288D4 2 2026-09-26 compiles 17 best 144 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x802288D4 3 2026-09-26 compiles 16 best 144 moved 0  (n64/tools/n64permute.py) */
/* @implements 0x802288D4 tgr BrAiLaneSetup */
void BrAiLaneSetup(int param_1)
{
  float fVar1;
  short *puVar2;
  short *puVar3;
  unsigned int uVar4;
  int iVar5;
  float fVar6;
  float fVar7;
  
  fVar1 = D_802A9C7C;
  fVar6 = (float)*(int *)(param_1 + 0x140) * D_802A9C80;
  *(float *)(param_1 + 0x1060) = fVar6;
  uVar4 = 0;
  fVar6 = fVar6 + fVar1;
  fVar7 = fVar6 + fVar1;
  *(float *)(param_1 + 0x1064) = fVar6;
  *(float *)(param_1 + 0x1068) = fVar7;
  *(float *)(param_1 + 0x106c) = fVar7 + fVar1;
  fVar1 = D_802A9C84;
  *(int *)(param_1 + 0x109c) = 0;
  *(int *)(param_1 + 0x107c) = 2;
  *(int *)(param_1 + 0x1098) = 0;
  *(int *)(param_1 + 0x1078) = 2;
  *(int *)(param_1 + 0x1094) = 0;
  *(int *)(param_1 + 0x1074) = 2;
  *(int *)(param_1 + 0x1070) = 2;
  *(float *)(param_1 + 0x1020) = fVar1 * 0.0f;
  *(int *)(param_1 + 0x1090) = 0;
  *(float *)(param_1 + 0x1024) = fVar1 * 1.0f;
  *(int *)(param_1 + 0x108c) = 0;
  *(int *)(param_1 + 0x106c) = 0;
  *(int *)(param_1 + 0x1088) = 0;
  *(int *)(param_1 + 0x1068) = 0;
  *(int *)(param_1 + 0x1084) = 0;
  *(int *)(param_1 + 0x1064) = 0;
  *(int *)(param_1 + 0x1060) = 0;
  *(int *)(param_1 + 0x1080) = 0;
  *(float *)(param_1 + 0x1028) = fVar1 * 2.0f;
  *(float *)(param_1 + 0x102c) = fVar1 * 3.0f;
  puVar2 = (short *)(param_1 + 0x19d0);
  puVar3 = (short *)(param_1 + 0x10d0);
  do {
    puVar2[2] = 0;
    puVar2[1] = puVar2[2];
    *puVar2 = puVar2[2];
    *puVar3 = 0;
    puVar3[1] = 0;
    puVar3[2] = 0;
    *(char *)(puVar3 + 6) = 0;
    if ((uVar4 & 1) == 0) {
      *(char *)((int)puVar3 + 0xd) = 0;
    }
    else {
      *(char *)((int)puVar3 + 0xd) = 0;
    }
    uVar4 = uVar4 + 1;
    *(char *)(puVar3 + 7) = 0;
    *(char *)((int)puVar3 + 0xf) = 0xff;
    puVar2 = puVar2 + 3;
    puVar3 = puVar3 + 8;
  } while ((int)uVar4 < 0x90);
  iVar5 = 0;
  do {
    iVar5 = iVar5 + 4;
    *(short *)(param_1 + 0x1d32) = 2;
    *(short *)(param_1 + 0x1d34) = 2;
    *(short *)(param_1 + 0x1d36) = 2;
    *(short *)(param_1 + 0x1d30) = 2;
    param_1 = param_1 + 8;
  } while (iVar5 != 0x24);
}

/* WHAT IT DOES: Clear a computer driver's steering and pedal outputs. */
/* @implements 0x80228A3C tgr BrAiInputClear */
void BrAiInputClear(short *car)
{
  int i;

  for (i = 0; i < 8; i += 4) {
    car[0x103f + i] = 0;
    car[0x1040 + i] = 0;
    car[0x1041 + i] = 0;
    car[0x103e + i] = 0;
  }
  car[0x1046] = 0;
}

