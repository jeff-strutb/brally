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
void func_8021D098(unsigned int *param_1,int param_2,int param_3,int param_4);
extern int D_8021DC94;
extern int D_8021DC98;
extern BrIoMesg D_8031A020[32];
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
void BrDlRebase(unsigned int *dl, int oldBase, int newBase, int size)
{
  if (dl == 0) {
    return;
  }
  for (;;) {
    switch ((unsigned char)(dl[0] >> 24)) {
    case 0x04:
    case 0xfd:
      func_8021D070(dl + 1, oldBase, newBase, size);
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
/* @t4-pass 0x8021DC34 1 2026-09-26 compiles 17 best 116 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x8021DC34 2 2026-09-26 compiles 17 best 116 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x8021DC34 3 2026-09-26 compiles 17 best 116 moved 0  (n64/tools/n64permute.py) */
/* @implements 0x8021DC34 tgr BrModelRebase */
void BrModelRebase(unsigned int *param_1)
{
  int iVar1;
  int *piVar2;
  int iVar3;
  unsigned int *puVar4;
  int iVar5;
  unsigned int *puVar6;
  int iVar7;
  int iVar8;
  int iVar9;
  unsigned int uVar10;
  
  if (param_1[1] != 0) {
    func_8021D070(param_1 + 1,0,0x7fffffff,param_1);
    iVar9 = 0;
    iVar8 = 0;
    piVar2 = (int *)param_1[1] + 1;
    if (0 < *(int *)param_1[1]) {
      while( 1 ) {
        iVar7 = 0;
        func_8021D070(piVar2,0,0x7fffffff,param_1);
        func_8021D070(*(int *)(param_1[1] + iVar8 + 4) + 4,0,0x7fffffff,param_1);
        func_8021D070(*(int *)(param_1[1] + iVar8 + 4) + 8,0,0x7fffffff,param_1);
        piVar2 = (int *)param_1[1];
        iVar5 = 0;
        iVar1 = (int)piVar2 + iVar8;
        iVar3 = *(int *)(iVar1 + 4);
        if (0 < *(int *)(iVar3 + 0xc)) {
          while( 1 ) {
            func_8021D070(iVar3 + 0x20,0,0x7fffffff,param_1);
            piVar2 = (int *)param_1[1];
            iVar7 = iVar7 + 1;
            iVar5 = iVar5 + 4;
            iVar1 = (int)piVar2 + iVar8;
            if (*(int *)(*(int *)(iVar1 + 4) + 0xc) <= iVar7) break;
            iVar3 = *(int *)(iVar1 + 4) + iVar5;
          }
        }
        iVar9 = iVar9 + 1;
        iVar8 = iVar8 + 4;
        if (*piVar2 <= iVar9) break;
        piVar2 = (int *)(iVar1 + 8);
      }
    }
  }
  uVar10 = 0;
  if (*param_1 != 0) {
    puVar6 = param_1 + 2;
    puVar4 = param_1;
    do {
      func_8021D070(puVar6,0,0x7fffffff,param_1);
      if (puVar4[2] != 0) {
        func_8021D070(puVar6,0,0x7fffffff,param_1);
        func_8021D098(puVar4[2],0,0x7fffffff,param_1);
      }
      uVar10 = uVar10 + 1;
      puVar4 = puVar4 + 5;
      puVar6 = puVar6 + 5;
    } while (uVar10 < *param_1);
  }
}
