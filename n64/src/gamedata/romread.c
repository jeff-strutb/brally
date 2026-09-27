/* romread.c -- reading words and packed assets out of cartridge ROM, and fixing up models loaded from it
 */
#include "tgr/common.h"

/* -- declarations -- */
typedef struct BrIoMesg { int words[6]; } BrIoMesg;   /* an OSIoMesg */
void func_8021C748(int param_1,int param_2,int param_3);
extern int D_8031B328;
extern int D_8031B330;
int func_8021DC34();
unsigned int func_8021CD30(unsigned int param_1,int param_2,unsigned int *param_3);
BrIoMesg *BrRomDmaSlot(void);
void func_8021735C(void);
int func_80264650();
void func_802662E0(unsigned int param_1,unsigned int param_2);
extern int D_80319F88;
int osRecvMesg(int param_1,int *param_2,int param_3);
extern int D_80272D44;
extern int D_80272D40;
void func_8021D070(unsigned int *param_1,unsigned int param_2,unsigned int param_3,int param_4);
void BrDlRebase(unsigned int *dl, unsigned int lo, unsigned int hi, int base);
extern int D_8021DC94;
extern int D_8021DC98;
extern BrIoMesg D_8031A020[32];
typedef struct BrModelPart {
  int x0;
  void *a;                      /* 0x04 */
  void *b;                      /* 0x08 */
  int n;                        /* 0x0C */
  char pad10[0x10];
  void *v[1];                   /* 0x20  n of them */
} BrModelPart;
typedef struct BrModelParts {
  int count;
  BrModelPart *part[1];         /* count of them */
} BrModelParts;
typedef struct BrModelDl {      /* 0x14 bytes */
  unsigned int *dl;
  int pad[4];
} BrModelDl;
typedef struct BrModel {
  unsigned int nDl;             /* 0x00 */
  BrModelParts *parts;          /* 0x04 */
  BrModelDl dls[1];             /* 0x08  nDl of them */
} BrModel;
/* -- end declarations -- */

/* WHAT IT DOES: Read the unpacked size of a packed asset in ROM: the second
 * word of its header. Callers check it against the buffer they allocated. */
/* @implements 0x8021C814 tgr BrRomReadSize */
int BrRomReadSize(int param_1)

{
  func_8021C748(&D_8031B328,param_1 + 4,4);
  return D_8031B328;
}

/* WHAT IT DOES: Read one 32-bit word from cartridge ROM. */
/* @implements 0x8021C848 tgr BrRomReadWord */
int BrRomReadWord(int param_1)

{
  func_8021C748(&D_8031B330,param_1,4);
  return D_8031B330;
}

/* WHAT IT DOES: Correct one address inside data just loaded from ROM: if it
 * points into the old block [lo, hi) it is moved to the same offset from
 * the new base, otherwise it is left alone. */
/* @implements 0x8021D070 tgr BrDlRebaseWord */
void BrDlRebaseWord(unsigned int *param_1,unsigned int param_2,unsigned int param_3,int param_4)

{
  unsigned int uVar1;
  
  uVar1 = *param_1;
  if ((param_2 <= uVar1) && (uVar1 < param_3)) {
    *param_1 = (uVar1 - param_2) + param_4;
  }
}

/* WHAT IT DOES: Copy an unpacked model straight out of ROM, from start to
 * end, into buf and turn its stored offsets into real addresses. Returns
 * buf. */
/* @implements 0x8021DDFC tgr BrModelReadRaw */
int BrModelReadRaw(int param_1,int param_2,int param_3)

{
  func_8021C748(param_1,param_2,param_3 - param_2);
  func_8021DC34(param_1);
  return param_1;
}

/* WHAT IT DOES: Unpack a packed model from ROM into buf and turn its stored
 * offsets into real addresses. Returns buf. */
/* @implements 0x8021DE2C tgr BrModelLoad */
int BrModelLoad(int param_1,int param_2)

{
  func_8021CD30(param_1,param_2,0);
  func_8021DC34(param_1);
  return param_1;
}

/* WHAT IT DOES: Copy len bytes from cartridge ROM into RAM: invalidate the
 * data cache over the destination, then DMA it across in 4 KB pieces, each
 * queued on the ROM message queue. */
/* @implements 0x8021C748 tgr BrRomRead */
void BrRomRead(int param_1,int param_2,int param_3)
{
  int uVar1;
  
  func_802662E0(param_1,param_3);
  for (; 0x1000 < param_3; param_3 = param_3 + -0x1000) {
    uVar1 = BrRomDmaSlot();
    func_80264650(uVar1,0,0,param_2,param_1,0x1000,(&D_80319F88));
    param_2 = param_2 + 0x1000;
    param_1 = param_1 + 0x1000;
  }
  uVar1 = BrRomDmaSlot();
  func_80264650(uVar1,0,0,param_2,param_1,param_3,(&D_80319F88));
  func_8021735C();
}

/* WHAT IT DOES: Wait until every ROM transfer still outstanding has
 * finished, taking one completion message off the ROM queue for each. */
/* @implements 0x8021735C tgr BrRomWaitAll */
void BrRomWaitAll(void)
{
  for (; D_80272D44 != 0; D_80272D44 = D_80272D44 + -1) {
    osRecvMesg((&D_80319F88),0,1);
  }
}

/* WHAT IT DOES: Reset a load stream: nothing consumed yet, and the given
 * source position. */
/* @implements 0x8021CD24 tgr BrStreamInit */
void BrStreamInit(int param_1,int param_2)
{
  *(int *)(param_1 + 4) = 0;
  *(int *)(param_1 + 8) = param_2;
}

/* WHAT IT DOES: Take the next of the 32 ROM transfer slots: if all are in
 * use, wait for one transfer to finish first; returns the slot's I/O
 * message block. */
/* @implements 0x802172D0 tgr BrRomDmaSlot */
BrIoMesg *BrRomDmaSlot(void)
{
  if (D_80272D44 == 32) {
    osRecvMesg(&D_80319F88, 0, 1);
  } else {
    D_80272D44++;
  }
  D_80272D40 = (D_80272D40 + 1) % 32;
  return &D_8031A020[D_80272D40];
}


/* WHAT IT DOES: Walk a display list loaded from ROM and correct every
 * address in it that pointed into the old block (vertex and texture-image
 * commands) so it points at the new copy; stops at the end of the list. */
/* @implements 0x8021D098 tgr BrDlRebase */
void BrDlRebase(unsigned int *dl, unsigned int lo, unsigned int hi, int base)
{
  if (dl == 0) {
    return;
  }
  for (;;) {
    switch ((unsigned char)(dl[0] >> 24)) {
    case 0x04:
    case 0xfd:
      func_8021D070(dl + 1, lo, hi, base);
      break;
    case 0xb8:
      return;
    }
    dl += 2;
  }
}


/* WHAT IT DOES: Turn the offsets stored in a model just loaded from ROM
 * into real addresses: its part table, each part's geometry and each part's
 * display list. */
/* @implements 0x8021DC34 tgr BrModelRebase */
void BrModelRebase(BrModel *m)
{
  int i;
  int j;

  if (m->parts != 0) {
    func_8021D070((unsigned int *)&m->parts, 0, 0x7fffffff, (int)m);
    for (i = 0; i < m->parts->count; i++) {
      func_8021D070((unsigned int *)&m->parts->part[i], 0, 0x7fffffff, (int)m);
      func_8021D070((unsigned int *)&m->parts->part[i]->a, 0, 0x7fffffff, (int)m);
      func_8021D070((unsigned int *)&m->parts->part[i]->b, 0, 0x7fffffff, (int)m);
      for (j = 0; j < m->parts->part[i]->n; j++) {
        func_8021D070((unsigned int *)&m->parts->part[i]->v[j], 0, 0x7fffffff, (int)m);
      }
    }
  }
  for (i = 0; i < m->nDl; i++) {
    func_8021D070((unsigned int *)&m->dls[i].dl, 0, 0x7fffffff, (int)m);
    if (m->dls[i].dl != 0) {
      func_8021D070((unsigned int *)&m->dls[i].dl, 0, 0x7fffffff, (int)m);
      BrDlRebase(m->dls[i].dl, 0, 0x7fffffff, (int)m);
    }
  }
}

