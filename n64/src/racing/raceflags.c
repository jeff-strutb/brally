/* raceflags.c -- per-race option flags
 */
#include "tgr/common.h"

/* -- declarations -- */
void BrRaceFlagsApply(void);
extern int D_8028AA78;
extern int D_8028AA80;
extern int D_8028AA84;
extern int D_8028AA8C;
int func_8021B72C(unsigned int *param_1,unsigned int *param_2);
extern int D_8028ABA8;
extern int D_8028AC68;
extern int D_8028AD28;
extern int D_8028ADE8;
extern int D_8028B7F4;
extern int D_8028B940;
extern int D_8031B760;
extern int D_8026FF08;
extern int D_80025C50;
/* -- end declarations -- */

/* WHAT IT DOES: Set the race-kind flags from a kind number (0-4): clears
 * the three option flags, sets the main flag for kinds 1-4 and the one
 * option flag that kind uses, then refreshes the track objects that depend
 * on them. */
/* @implements 0x80200050 tgr BrRaceSetKind */
void BrRaceSetKind(int kind)
{
  D_8028AA80 = D_8028AA8C = D_8028AA84 = 0;
  switch (kind) {
  case 0:
    D_8028AA78 = 0;
    break;
  case 1:
    D_8028AA78 = 1;
    break;
  case 2:
    D_8028AA78 = 1;
    D_8028AA8C = 1;
    break;
  case 3:
    D_8028AA78 = 1;
    D_8028AA84 = 1;
    break;
  case 4:
    D_8028AA78 = 1;
    D_8028AA80 = 1;
    break;
  }
  BrRaceFlagsApply();
}


/* WHAT IT DOES: Re-evaluate everything that depends on the race-kind flags:
 * marks each track object whose condition list now holds, and does the same
 * for every car's model parts. */
/* @t4-pass 0x8021B97C 1 2026-09-26 compiles 17 best 137 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x8021B97C 2 2026-09-26 compiles 17 best 137 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x8021B97C 3 2026-09-26 compiles 17 best 137 moved 0  (n64/tools/n64permute.py) */
/* @implements 0x8021B97C tgr BrRaceFlagsApply */
void BrRaceFlagsApply(void)
{
  char cVar1;
  int iVar2;
  int iVar3;
  unsigned int uVar4;
  char *puVar5;
  int iVar6;
  int local_c;
  
  uVar4 = 0;
  if (D_8028B940 != 2) {
    uVar4 = (unsigned int)(D_8028B940 != 7);
  }
  iVar6 = 0;
  iVar3 = 0;
  if (0 < (*(int *)0x80025C64)) {
    do {
      if ((*(unsigned short *)((*(int *)0x80025C60) + iVar3 + 0x4c) & 4) == 0) {
        D_8028ADE8 = uVar4;
      }
      iVar2 = func_8021B72C(*(int *)((*(int *)0x80025C60) + iVar3 + 0x44),&D_8028ABA8);
      if (iVar2 != 0) {
        *(unsigned short *)((*(int *)0x80025C60) + iVar3 + 0x4c) = *(unsigned short *)((*(int *)0x80025C60) + iVar3 + 0x4c) | 8;
      }
      iVar6 = iVar6 + 1;
      iVar3 = iVar3 + 0x54;
    } while (iVar6 < (*(int *)0x80025C64));
  }
  local_c = 0;
  if (0 < D_8028B7F4) {
    puVar5 = &D_8031B760;
    do {
      iVar3 = 0;
      cVar1 = puVar5[0x2063];
      while( 1 ) {
        iVar6 = 0;
        if (cVar1 == '\x02') {
          iVar2 = 0;
          iVar6 = *(int *)(puVar5 + 0x2078);
          while( 1 ) {
            func_8021B72C(*(int *)(iVar6 + iVar3 * 0x28 + iVar2 + 0x18),&D_8028AC68);
            iVar2 = iVar2 + 4;
            if (0x27 < iVar2) break;
            iVar6 = *(int *)(puVar5 + 0x2078);
          }
          iVar2 = 0;
          iVar6 = *(int *)(puVar5 + 0x2078);
          while( 1 ) {
            func_8021B72C(*(int *)(iVar6 + iVar3 * 0xc + iVar2 + 0xbc),&D_8028AD28);
            iVar2 = iVar2 + 4;
            if (iVar2 == 0xc) break;
            iVar6 = *(int *)(puVar5 + 0x2078);
          }
        }
        else {
          iVar2 = *(int *)(puVar5 + 0x2078);
          while( 1 ) {
            func_8021B72C(*(int *)(iVar2 + iVar3 * 0x28 + iVar6 + 0x18),&D_8028ABA8);
            iVar6 = iVar6 + 4;
            if (0x27 < iVar6) break;
            iVar2 = *(int *)(puVar5 + 0x2078);
          }
          iVar6 = 0;
          iVar2 = *(int *)(puVar5 + 0x2078);
          while( 1 ) {
            func_8021B72C(*(int *)(iVar2 + iVar3 * 0xc + iVar6 + 0xbc),&D_8028ABA8);
            iVar6 = iVar6 + 4;
            if (iVar6 == 0xc) break;
            iVar2 = *(int *)(puVar5 + 0x2078);
          }
        }
        iVar3 = iVar3 + 1;
        if (iVar3 == 3) break;
        cVar1 = puVar5[0x2063];
      }
      local_c = local_c + 1;
      puVar5 = puVar5 + 0x2090;
    } while (local_c < D_8028B7F4);
  }
}

/* WHAT IT DOES: Tell whether the race is shown split: any of the split
 * options is set, or two players are racing. */
/* @implements 0x8022F900 tgr BrRaceSplitScreen */
int BrRaceSplitScreen(void)
{
  return D_8028AA80 != 0 || D_8028AA84 != 0 || D_8028AA8C != 0 || D_80025C50 == 0 ||
         D_8026FF08 == 2;
}

