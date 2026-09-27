/* entcolour.c -- car artwork records and their paint colours
 */
#include "tgr/common.h"
#include "tgr/car.h"

/* -- declarations -- */
void func_8021D140(int param_1,unsigned int param_2,unsigned int param_3,int param_4);
void func_80222D54(int param_1);
void func_8021D5E4(int param_1,int param_2,int param_3);
void func_802203F0(int param_1,int param_2);
void func_8021D070(unsigned int *param_1,unsigned int param_2,unsigned int param_3,int param_4);
void func_8021D098(unsigned int *param_1,unsigned int param_2,unsigned int param_3,int param_4);
void func_80220398();
unsigned int func_8021CD30();
void func_8021D32C(int param_1);
extern int D_8028AE20;
typedef struct { char raw[0xdf88]; } BrCarModelBuf;
extern BrCarModelBuf D_803C8000[];
extern int D_8031B238[];
extern unsigned char D_8028B904[][3];
void func_80220620(BrCar *car);
typedef struct BrCarModelPart { void *a; void *b; char pad08[0x1c]; } BrCarModelPart;
typedef struct BrCarModel { char pad00[0x10]; int nParts; BrCarModelPart *parts; unsigned int *dl[3][10]; void *x90; void *x94; char pad98[0xbc - 0x98]; unsigned int *dl2[3][3]; char pade0[0x11c - 0xe0]; void **x11c; } BrCarModel;
extern char D_803D5F88[];
/* -- end declarations -- */

/* WHAT IT DOES: Repaint a car's artwork record in the car's own colour (its
 * red, green and blue reduced to the 5-bit texture format), then refresh
 * what depends on it. */
/* @implements 0x80220398 tgr BrEntRefreshColour */
void BrEntRefreshColour(int param_1)
{
  func_8021D140(*(int *)(param_1 + 0x2078),(int)(unsigned int)*(unsigned char *)(param_1 + 0x2060) >> 3,
               (int)(unsigned int)*(unsigned char *)(param_1 + 0x2061) >> 3,
               (int)(unsigned int)*(unsigned char *)(param_1 + 0x2062) >> 3);
  func_80222D54(param_1);
}

/* WHAT IT DOES: Load a car into its slot and attach that car's artwork
 * record to the entity, repainting it. */
/* @implements 0x80220438 tgr BrEntLoadRecord */
void BrEntLoadRecord(int param_1,int param_2,int param_3)
{
  func_8021D5E4(param_2,param_3,0);
  func_802203F0(param_1,param_2);
}

/* WHAT IT DOES: Tell whether an entity slot is unused (it has no owner
 * yet). */
/* @implements 0x80220534 tgr BrEntIsFree */
int BrEntIsFree(int param_1)
{
  return *(int *)(param_1 + 0x18) == 0;
}

/* WHAT IT DOES: Paint a car's colourable texture in the given colour: every
 * texel of the paint layer gets the 5-bit red, green and blue, keeping its
 * own alpha bit. */
/* @t4-pass 0x8021D140 1 2026-09-26 compiles 17 best 74 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x8021D140 2 2026-09-26 compiles 17 best 74 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x8021D140 3 2026-09-26 compiles 16 best 74 moved 0  (n64/tools/n64permute.py) */
/* @implements 0x8021D140 tgr BrEntPaintTexture */
void BrEntPaintTexture(int param_1,unsigned int param_2,unsigned int param_3,int param_4)
{
  int iVar1;
  int iVar2;
  int iVar3;
  short *puVar4;
  int iVar5;
  unsigned int uVar6;
  
  iVar1 = 0;
  iVar5 = param_1;
  do {
    iVar2 = *(int *)(param_1 + 0x14);
    iVar3 = (unsigned int)*(unsigned char *)(iVar5 + 0x110) * 0x24 + iVar2;
    puVar4 = *(short **)(iVar3 + 4);
    if (puVar4 == (short *)0x0) {
      uVar6 = (unsigned int)*(unsigned char *)(iVar5 + 0x111);
    }
    else if ((*(unsigned char *)(iVar3 + 0x20) & 0xf) == 1) {
      *puVar4 = puVar4[iVar1] & 1 | (short)(param_2 << 0xb) | (short)(param_3 << 6) |
                (short)(param_4 << 1);
      puVar4[1] = puVar4[iVar1] & 1 | (short)((param_2 & 0x1e) << 10) |
                  (short)((param_3 & 0x1e) << 5) | (short)param_4 & 0x1e;
      iVar2 = *(int *)(param_1 + 0x14);
      uVar6 = (unsigned int)*(unsigned char *)(iVar5 + 0x111);
    }
    else {
      uVar6 = (unsigned int)*(unsigned char *)(iVar5 + 0x111);
    }
    iVar2 = uVar6 * 0x24 + iVar2;
    puVar4 = *(short **)(iVar2 + 4);
    if ((puVar4 != (short *)0x0) && ((*(unsigned char *)(iVar2 + 0x20) & 0xf) == 1)) {
      *puVar4 = puVar4[iVar1 + 1] & 1 | (short)(param_2 << 0xb) | (short)(param_3 << 6) |
                (short)(param_4 << 1);
      puVar4[1] = puVar4[iVar1 + 1] & 1 | (short)((param_2 & 0x1e) << 10) |
                  (short)((param_3 & 0x1e) << 5) | (short)param_4 & 0x1e;
    }
    iVar1 = iVar1 + 2;
    iVar5 = iVar5 + 2;
  } while (iVar1 != 0xc);
}

/* WHAT IT DOES: Fix up a car model just loaded into its slot: every part's
 * address and display list is moved from the loading area to the slot's own
 * copy. */
/* @implements 0x8021D32C tgr BrEntRebaseModel */
void BrEntRebaseModel(BrCarModel *m)
{
  int i;
  int j;

  for (i = 0; i < 3; i++) {
    for (j = 0; j < 10; j++) {
      func_8021D070((unsigned int *)&m->dl[i][j], (unsigned int)D_803C8000, (unsigned int)D_803D5F88, (int)m);
      func_8021D098(m->dl[i][j], (unsigned int)D_803C8000, (unsigned int)D_803D5F88, (int)m);
    }
    for (j = 0; j < 3; j++) {
      if (m->dl2[i][j] != 0) {
        func_8021D070((unsigned int *)&m->dl2[i][j], (unsigned int)D_803C8000, (unsigned int)D_803D5F88, (int)m);
        func_8021D098(m->dl2[i][j], (unsigned int)D_803C8000, (unsigned int)D_803D5F88, (int)m);
      }
    }
  }
  func_8021D070((unsigned int *)&m->parts, (unsigned int)D_803C8000, (unsigned int)D_803D5F88, (int)m);
  func_8021D070((unsigned int *)&m->x90, (unsigned int)D_803C8000, (unsigned int)D_803D5F88, (int)m);
  func_8021D070((unsigned int *)&m->x94, (unsigned int)D_803C8000, (unsigned int)D_803D5F88, (int)m);
  for (i = 0; i < m->nParts; i++) {
    func_8021D070((unsigned int *)&m->parts[i].a, (unsigned int)D_803C8000, (unsigned int)D_803D5F88, (int)m);
    func_8021D070((unsigned int *)&m->parts[i].b, (unsigned int)D_803C8000, (unsigned int)D_803D5F88, (int)m);
  }
  func_8021D070((unsigned int *)&m->x11c, (unsigned int)D_803C8000, (unsigned int)D_803D5F88, (int)m);
  for (i = 0; i < 12; i++) {
    func_8021D070((unsigned int *)&m->x11c[i], (unsigned int)D_803C8000, (unsigned int)D_803D5F88, (int)m);
  }
}


/* WHAT IT DOES: Attach one of the car artwork records to a car and repaint
 * it in its own colour. */
/* @implements 0x802203F0 tgr BrEntSetRecord */
void BrEntSetRecord(int param_1,int param_2)
{
  *(int *)(param_1 + 0x2078) = (int)&D_803C8000[param_2];
  func_80220398();
}

/* WHAT IT DOES: Load a car's model into its slot when the slot has an
 * owner, remember which model is there, and fix up the model's addresses. */
/* @implements 0x80220544 tgr BrEntLoadModel */
int BrEntLoadModel(int *p)
{
  if (p[6] != 0) {
    func_8021CD30((int)&D_803C8000[p[8]], *(int *)((char *)&D_8028AE20 + p[9] * 0x60));
    if (p[6] == 0) {
      D_8031B238[p[8]] = p[9];
      func_8021D32C((int)&D_803C8000[p[8]]);
    }
  }
  return p[6];
}


/* WHAT IT DOES: Give a car its slot's default body colour from the
 * three-byte colour table (0x8028B904) after resetting its model, and clear
 * the colour's fourth byte and two related fields. */
/* @implements 0x802260A0 tgr BrCarDefaultColour */
void BrCarDefaultColour(BrCar *car)
{
  unsigned char r;
  unsigned char g;
  unsigned char b;

  func_80220620(car);
  r = D_8028B904[car->slot][0];
  g = D_8028B904[car->slot][1];
  b = D_8028B904[car->slot][2];
  car->colour[3] = 0;
  car->x2068 = 0;
  car->xed8 = 0;
  car->colour[0] = r;
  car->colour[1] = g;
  car->colour[2] = b;
}

