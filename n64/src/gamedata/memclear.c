/* memclear.c -- clearing memory blocks
 */
#include "tgr/common.h"

/* -- declarations -- */
extern int D_80225F5C;
extern int D_80225F60;
extern char * D_8028B834;
extern int D_8028B838;
/* -- end declarations -- */

/* WHAT IT DOES: Zero every memory block in the game's clear list (a
 * null-terminated list of address and length pairs), used when a race
 * starts. */
/* @implements 0x80225F30 tgr BrClearList */
void BrClearList(void)
{
  int *puVar1;
  int *piVar2;
  char *puVar3;
  int *puVar4;
  char *puVar5;
  int iVar6;
  
  puVar4 = &D_8028B834;
  puVar3 = D_8028B834;
  iVar6 = D_8028B838;
  if (D_8028B834 != (char *)0x0) {
    while( 1 ) {
      puVar5 = puVar3 + iVar6;
      for (; puVar3 < puVar5; puVar3 = puVar3 + 1) {
        *puVar3 = 0;
      }
      puVar1 = puVar4 + 2;
      if ((char *)*puVar1 == (char *)0x0) break;
      piVar2 = puVar4 + 3;
      puVar4 = puVar4 + 2;
      puVar3 = (char *)*puVar1;
      iVar6 = *piVar2;
    }
  }
}
