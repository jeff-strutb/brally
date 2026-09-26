/* romread.c -- reading words and packed assets out of cartridge ROM, and fixing up models loaded from it
 */
#include "tgr/common.h"

/* -- declarations -- */
void func_8021C748(int param_1,int param_2,int param_3);
extern int D_8031B328;
extern int D_8031B330;
int func_8021DC34();
unsigned int func_8021CD30(unsigned int param_1,int param_2,unsigned int *param_3);
int func_802172D0(void);
void func_8021735C(void);
int func_80264650();
void func_802662E0(unsigned int param_1,unsigned int param_2);
extern int D_80319F88;
int func_802642E0(int param_1,int *param_2,int param_3);
extern int D_80272D44;
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
    uVar1 = func_802172D0();
    func_80264650(uVar1,0,0,param_2,param_1,0x1000,(&D_80319F88));
    param_2 = param_2 + 0x1000;
    param_1 = param_1 + 0x1000;
  }
  uVar1 = func_802172D0();
  func_80264650(uVar1,0,0,param_2,param_1,param_3,(&D_80319F88));
  func_8021735C();
}

/* WHAT IT DOES: Wait until every ROM transfer still outstanding has
 * finished, taking one completion message off the ROM queue for each. */
/* @implements 0x8021735C tgr BrRomWaitAll */
void BrRomWaitAll(void)
{
  for (; D_80272D44 != 0; D_80272D44 = D_80272D44 + -1) {
    func_802642E0((&D_80319F88),0,1);
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
