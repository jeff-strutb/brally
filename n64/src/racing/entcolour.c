/* entcolour.c -- car artwork records and their paint colours
 */
#include "tgr/common.h"
#include "tgr/car.h"

/* -- declarations -- */
void func_8021D140(int param_1,unsigned int param_2,unsigned int param_3,int param_4);
void func_80222D54(int param_1);
void BrCarModelInstall(int slot, int car, void *src);
unsigned int BrRomReadSize(int rom);
void osSyncPrintf(char *fmt, ...);
int sprintf(char *buf, char *fmt, ...);
void BrFatal(char *msg);
void *memcpy(void *dst, void *src, unsigned int n);
void BrEntRebaseModel(BrCarModel *m);
typedef struct BrStream {       /* a streamed ROM read in flight */
  char pad00[0x20];
  int slot;                     /* 0x20  model slot it fills */
  int car;                      /* 0x24  car it loads */
} BrStream;
void BrStreamInit(BrStream *s, char *buf);
extern char D_802F7F00[][0x8000];
void func_802203F0(int param_1,int param_2);
void func_8021D070(unsigned int *param_1,unsigned int param_2,unsigned int param_3,int param_4);
void func_8021D098(unsigned int *param_1,unsigned int param_2,unsigned int param_3,int param_4);
void func_80220398();
unsigned int BrRomUnpack();
void func_8021D32C(int param_1);
typedef struct { char raw[0xdf88]; } BrCarModelBuf;
extern BrCarModelBuf D_803C8000[];
extern int D_8031B238[];
extern unsigned char D_8028B904[][3];
void BrCarResetFrames(BrCar *car);
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
  BrCarModelInstall(param_2,param_3,0);
  func_802203F0(param_1,param_2);
}

/* WHAT IT DOES: Start streaming car n's model file from ROM into a slot's
 * model buffer through one of the 32 KB stream buffers, recording its size
 * and, in the stream, which slot and car it is for. */
/* @implements 0x80220474 tgr BrCarModelStream */
void BrCarModelStream(BrStream *s, int slot, int car, int bufIdx)
{
  D_8028AE0C[car].size = BrRomReadSize(D_8028AE0C[car].rom);
  BrStreamInit(s, D_802F7F00[bufIdx]);
  BrRomUnpack(&D_803C8000[slot], D_8028AE0C[car].rom, s);
  s->slot = slot;
  s->car = car;
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
/* @t3 0x8021D140 */
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

/* WHAT IT DOES: Take a car's body colour from its model: the first
 * palette entry of the paint part (when that part is paletted), widened
 * from RGBA5551 to 8 bits a channel. */
/* @t4-pass 0x8021D2A0 1 2026-10-03 compiles 120 best 29 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x8021D2A0 2 2026-10-03 compiles 117 best 29 moved 0  (n64/tools/n64permute.py) */
/* @implements 0x8021D2A0 tgr BrCarColourFromModel */
void BrCarColourFromModel(BrCar *car, BrCarModel *m)
{
  BrCarModelPart *p = &m->parts[m->paintPart];
  unsigned short c;
  unsigned char r;
  unsigned char g;
  unsigned char b;

  if (p->b != 0 && (p->fmt & 0xf) == 1) {
    c = p->b[0];
    r = ((c >> 13) & 7) | ((c >> 8) & 0xf8);
    g = ((c >> 8) & 7) | ((c >> 3) & 0xf8);
    b = ((c >> 3) & 7) | ((c << 2) & 0xf8);
    car->colour[2] = b;
    car->colour[1] = g;
    car->colour[0] = r;
  }
}

/* WHAT IT DOES: Fix up a car model just loaded into its slot: every part's
 * address and display list is moved from the loading area to the slot's own
 * copy. */
/* @t4-pass 0x8021D32C 1 2026-10-03 compiles 121 best 2 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x8021D32C 2 2026-10-03 compiles 121 best 2 moved 0  (n64/tools/n64permute.py) */
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


/* WHAT IT DOES: Load car n's model file from ROM into a model buffer,
 * recording its size; a file bigger than a buffer is a fatal error. */
/* @implements 0x8021D534 tgr BrCarModelLoad */
void BrCarModelLoad(void *buf, int car)
{
  char msg[80];

  if ((D_8028AE0C[car].size = BrRomReadSize(D_8028AE0C[car].rom)) > sizeof(BrCarModelBuf)) {
    sprintf(msg, "Car %d too big (%d vs. %d)", car, D_8028AE0C[car].size, sizeof(BrCarModelBuf));
    BrFatal(msg);
  } else {
    osSyncPrintf("Loading car %d (%d / %d)\n", car, D_8028AE0C[car].size, sizeof(BrCarModelBuf));
  }
  BrRomUnpack(buf, D_8028AE0C[car].rom, 0);
}

/* WHAT IT DOES: Put car n's model into a slot's model buffer -- loaded
 * from ROM, or copied from a model already in memory -- rebase its
 * pointers there and record which car the slot holds. */
/* @implements 0x8021D5E4 tgr BrCarModelInstall */
void BrCarModelInstall(int slot, int car, void *src)
{
  if (src == 0) {
    BrCarModelLoad(&D_803C8000[slot], car);
  } else {
    memcpy(&D_803C8000[slot], src, D_8028AE0C[car].size);
  }
  BrEntRebaseModel((BrCarModel *)&D_803C8000[slot]);
  D_8031B238[slot] = car;
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
    BrRomUnpack((int)&D_803C8000[p[8]], D_8028AE0C[p[9]].rom);
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

  BrCarResetFrames(car);
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

