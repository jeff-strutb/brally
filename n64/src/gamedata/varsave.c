/* varsave.c -- saving and restoring blocks of game variables: a list of
 * {address, size} entries copied into or out of a buffer
 */
#include "tgr/common.h"
#include "tgr/car.h"
#include "tgr/pad.h"

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
void osSyncPrintf(const char *fmt, ...);
extern int D_80270788;                  /* a replay is running */
extern int D_8026FF18;                  /* the game mode */
extern int D_8026FF08;                  /* players */
extern int D_8028B304;                  /* laps in the race */
typedef struct { char raw[0xdf88]; } BrCarModelBuf;
extern BrCarModelBuf D_803C8000[];      /* the four cars' model buffers */
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

/* WHAT IT DOES: When a player starts the last lap (or the race is one lap)
 * in a mode that keeps laps, and nothing is being recorded for them yet:
 * borrow a spare car model buffer (the fourth for player 0, the third for
 * player 1), give every car's pad record a 0x3840-byte recording slot for
 * that player inside it, and save the game variables into the same buffer
 * so the lap can be replayed from its start. */
/* @implements 0x8022AF90 tgr BrLastLapSave */
void BrLastLapSave(BrCar *car)
{
  int i;

  if (D_80270788 == 0 && D_8026FF18 != 2 && D_8026FF18 != 4 && car->slot < D_8026FF08
      && (D_8028B304 == car->laps + 1 || D_8028B304 < 2)
      && ((BrPadRec *)car->pad)->rec[car->slot] == 0) {
    osSyncPrintf("SAVING LAST LAP INFO\n");
    for (i = 0; i < D_8026FF08; i++) {
      ((BrPadRec *)D_8031B760[i].pad)->recLen[car->slot] = 0;
      ((BrPadRec *)D_8031B760[i].pad)->recKeep[car->slot] = 0x3840;
      ((BrPadRec *)D_8031B760[i].pad)->rec[car->slot] = (unsigned char *)D_803C8000[3 - car->slot].raw + i * 0x3840;
    }
    BrVarSaveAll(D_803C8000[3 - car->slot].raw);
  }
}
