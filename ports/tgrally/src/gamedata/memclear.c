/* memclear.c -- clearing memory blocks
 */
#include "tgr/common.h"

/* -- declarations -- */
extern int D_80225F5C;
extern int D_80225F60;
typedef struct BrClearEnt { TgrAddr ptr; int size; } BrClearEnt;   /* ptr: char * */
extern BrClearEnt D_8028B834[];
/* -- end declarations -- */

/* WHAT IT DOES: Zero every memory block in the game's clear list (a
 * null-terminated list of address and length pairs), used when a race
 * starts. */
/* @implements 0x80225F30 tgr BrClearList */
void BrClearList(void)
{
  int i;
  char *p;
  char *end;

  for (i = 0; D_8028B834[i].ptr != 0; i++) {
    p = TGR_PTR(char *, D_8028B834[i].ptr);
    end = p + D_8028B834[i].size;
    for (; p < end; p++) {
      *p = 0;
    }
  }
}

