/* paintshop.c -- the paint shop
 */
#include "tgr/common.h"

/* -- declarations -- */
char * memcpy(char *param_1,char *param_2,int param_3);
extern unsigned char D_8028DB68;
extern unsigned char D_8028DB74;
extern int D_8028DB78;
extern int D_8028DB80;
extern unsigned char D_8028DBB4;
extern unsigned char D_8028DBDC;
int func_8024D3F0(int *param_1);
extern int D_8028D12C;
extern int D_8028D130;
extern int D_8028DB94;
extern unsigned char D_8028DBBC;
extern unsigned char D_8028DBC4;
extern unsigned char D_8028DBE8;
extern float D_802AB20C;
extern float D_802AB210;
extern int D_8036A8E0;
extern int D_8036A8F8;
/* -- end declarations -- */

/* WHAT IT DOES: Store the paint shop's working decal (2 KB) into the decal
 * buffer, move the edit point on, and mark the decal as changed so it is
 * redrawn. */
/* @implements 0x80244CA8 tgr BrPaintDecalCommit */
void BrPaintDecalCommit(void)
{
  memcpy(D_8028DB80,D_8028DB78,0x800);
  D_8028DB74 = D_8028DB68;
  D_8028DBB4 = D_8028DBB4 + '\x01';
  D_8028DBDC = 1;
}

/* WHAT IT DOES: Swap two bytes in place. The paint shop uses it to reorder
 * pixel data. */
/* @implements 0x80252F50 tgr BrSwapBytes */
void BrSwapBytes(char *param_1,char *param_2)
{
  char uVar1;
  
  uVar1 = *param_1;
  *param_1 = *param_2;
  *param_2 = uVar1;
}

/* WHAT IT DOES: Move the paint shop cursor from the stick once it is pushed
 * past the dead zone, unless the cursor is locked. */
/* @t4-pass 0x8024BE78 1 2026-09-26 compiles 17 best 219 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x8024BE78 2 2026-09-26 compiles 17 best 219 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x8024BE78 3 2026-09-26 compiles 16 best 219 moved 0  (n64/tools/n64permute.py) */
/* @implements 0x8024BE78 tgr BrPaintStickMove */
void BrPaintStickMove(void)
{
  int iVar1;
  unsigned int *puVar2;
  float fVar3;
  
  if (D_8028DBE8 == '\0') {
    puVar2 = (unsigned int *)(&D_8036A8E0 + (unsigned int)D_8028DBBC * 0x15c);
    fVar3 = *(float *)(&D_8036A8F8 + (unsigned int)D_8028DBBC * 0x15c);
    if (fVar3 < 0.0f) {
      fVar3 = -fVar3;
    }
    if (D_802AB20C <= fVar3) {
      iVar1 = func_8024D3F0(&D_8028DB94);
      if ((iVar1 == 0) || (D_8028DBC4 != '\0')) {
        puVar2 = (unsigned int *)(&D_8036A8E0 + (unsigned int)D_8028DBBC * 0x15c);
        D_8028D12C = D_8028D12C +
                       (int)(*(float *)(&D_8036A8F8 + (unsigned int)D_8028DBBC * 0x15c) * 12.0f);
      }
      else {
        puVar2 = (unsigned int *)(&D_8036A8E0 + (unsigned int)D_8028DBBC * 0x15c);
        D_8028D12C = D_8028D12C +
                       (int)(*(float *)(&D_8036A8F8 + (unsigned int)D_8028DBBC * 0x15c) * 6.0f);
      }
    }
    else if ((*puVar2 & 0x201) == 0) {
      if ((*puVar2 & 0x804) != 0) {
        D_8028D12C = D_8028D12C + -1;
      }
    }
    else {
      D_8028D12C = D_8028D12C + 1;
    }
    fVar3 = (float)puVar2[7];
    if (fVar3 < 0.0f) {
      fVar3 = -fVar3;
    }
    if (D_802AB210 <= fVar3) {
      iVar1 = func_8024D3F0(&D_8028DB94);
      if ((iVar1 == 0) || (D_8028DBC4 != '\0')) {
        D_8028D130 = D_8028D130 -
                       (int)(*(float *)((unsigned int)D_8028DBBC * 0x15c + -0x7fc95704) * 12.0f);
      }
      else {
        D_8028D130 = D_8028D130 -
                       (int)(*(float *)((unsigned int)D_8028DBBC * 0x15c + -0x7fc95704) * 6.0f);
      }
    }
    else if ((*puVar2 & 0x402) == 0) {
      if ((*puVar2 & 0x108) != 0) {
        D_8028D130 = D_8028D130 + -1;
      }
    }
    else {
      D_8028D130 = D_8028D130 + 1;
    }
    if (D_8028D12C < 0x262) {
      if (D_8028D12C < 0x1f) {
        D_8028D12C = 0x1f;
      }
    }
    else {
      D_8028D12C = 0x261;
    }
    if (D_8028D130 < 0x16) {
      D_8028D130 = 0x16;
    }
    else if (0x1ca < D_8028D130) {
      D_8028D130 = 0x1ca;
    }
  }
}
