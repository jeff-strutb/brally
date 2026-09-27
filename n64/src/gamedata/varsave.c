/* varsave.c -- saving and restoring blocks of game variables: a list of
 * {address, size} entries copied into or out of a buffer
 */
#include "tgr/common.h"

/* -- declarations -- */
typedef struct BrVarEnt {       /* one registered variable */
  char *ptr;                    /* 0 ends the list */
  int size;
} BrVarEnt;
void *memcpy(void *d, void *s, int n);
int sprintf(char *buf, const char *fmt, ...);
void BrFatal(char *msg);
extern BrVarEnt D_8028BB0C[];
extern BrVarEnt D_8028BBC4[];
extern char D_8034E570[];
/* -- end declarations -- */

/* WHAT IT DOES: Copy every variable in a list into a buffer, one after the
 * other; using more than the space available is fatal, with both sizes in
 * the message. */
/* @implements 0x8022ADCC tgr BrVarListSave */
void BrVarListSave(BrVarEnt *e, char *dst, int avail)
{
  int i;
  char *start;
  char msg[80];

  start = dst;
  for (i = 0; e[i].ptr != 0; i++) {
    memcpy(dst, e[i].ptr, e[i].size);
    dst += e[i].size;
  }
  if (avail < dst - start) {
    sprintf(msg, "VAR SAVE OVERFLOW (%d avail, %d used)", avail, dst - start);
    BrFatal(msg);
  }
}

/* WHAT IT DOES: Copy every variable in a list back out of a buffer. */
/* @implements 0x8022AE70 tgr BrVarListLoad */
void BrVarListLoad(BrVarEnt *e, char *src)
{
  int i;

  for (i = 0; e[i].ptr != 0; i++) {
    memcpy(e[i].ptr, src, e[i].size);
    src += e[i].size;
  }
}

/* WHAT IT DOES: Copy every registered game variable into the save buffer,
 * checking it fits the space reserved for it. */
/* @implements 0x8022AED8 tgr BrVarSaveAll */
void BrVarSaveAll(char *buf)
{
  BrVarListSave(D_8028BB0C, buf + 0x7080, 0xdf88);
}

/* WHAT IT DOES: Copy every registered game variable back out of the save
 * buffer. */
/* @implements 0x8022AF08 tgr BrVarLoadAll */
void BrVarLoadAll(char *buf)
{
  BrVarListLoad(D_8028BB0C, buf + 0x7080);
}

/* WHAT IT DOES: Save the second variable list into its own 64-byte buffer. */
/* @implements 0x8022AF34 tgr BrVarSaveSmall */
void BrVarSaveSmall(void)
{
  BrVarListSave(D_8028BBC4, D_8034E570, 0x40);
}

/* WHAT IT DOES: Restore the second variable list from its 64-byte buffer. */
/* @implements 0x8022AF64 tgr BrVarLoadSmall */
void BrVarLoadSmall(void)
{
  BrVarListLoad(D_8028BBC4, D_8034E570);
}
