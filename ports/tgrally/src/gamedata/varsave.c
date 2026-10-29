/* varsave.c -- saving and restoring blocks of game variables: a list of
 * {address, size} entries copied into or out of a buffer
 */
#include "tgr/common.h"
#include "tgr/car.h"
#include "tgr/pad.h"

/* -- declarations -- */
typedef struct BrVarEnt {       /* one registered variable */
  TgrAddr ptr;                    /* char * -- 0 ends the list */
  int size;
} BrVarEnt;
void BrFatal(char *msg);
extern BrVarEnt D_8028BB0C[];
extern BrVarEnt D_8028BBC4[];
extern char D_8034E570[];
extern int D_80270788;                  /* a replay is running */
extern int D_8026FF18;                  /* the game mode */
extern int D_8026FF08;                  /* players */
extern int D_8028B304;                  /* laps in the race */
typedef struct { char raw[0xdf88]; } BrCarModelBuf;
extern BrCarModelBuf D_803C8000[];      /* the four cars' model buffers */
/* -- end declarations -- */

/* WHAT IT DOES: Copy every variable in a list into a buffer, one after the
 * other; using more than the space available is fatal, with both sizes in
 * the message.  Europe reads each variable at an offset from its registered
 * address (0 for the game's own copy) and returns the bytes used. */
/* @implements 0x8022ADCC tgr BrVarListSave */
#if TGR_REGION_EUR || TGR_REGION_JPN
int BrVarListSave(BrVarEnt *e, char *dst, int avail, int off)
#else
void BrVarListSave(BrVarEnt *e, char *dst, int avail)
#endif
{
  int i;
  char *start;
  char msg[80];

  start = dst;
  for (i = 0; e[i].ptr != 0; i++) {
#if TGR_REGION_EUR || TGR_REGION_JPN
    memcpy(dst, e[i].ptr + off, e[i].size);
#else
    memcpy(dst, TGR_PTR(char *, e[i].ptr), e[i].size);
#endif
    dst += e[i].size;
  }
#if TGR_REGION_EUR || TGR_REGION_JPN
  i = dst - start;
  if (avail < i) {
    sprintf(msg, "VAR SAVE OVERFLOW (%d avail, %d used)", avail, i);
    BrFatal(msg);
  }
  return i;
#else
  if (avail < dst - start) {
    sprintf(msg, "VAR SAVE OVERFLOW (%d avail, %d used)", avail, dst - start);
    BrFatal(msg);
  }
#endif
}

/* WHAT IT DOES: Copy every variable in a list back out of a buffer
 * (Europe: to an offset from each registered address, returning the bytes
 * read). */
/* @implements 0x8022AE70 tgr BrVarListLoad */
#if TGR_REGION_EUR || TGR_REGION_JPN
int BrVarListLoad(BrVarEnt *e, char *src, int off)
#else
void BrVarListLoad(BrVarEnt *e, char *src)
#endif
{
  int i;
#if TGR_REGION_EUR || TGR_REGION_JPN
  char *start;

  start = src;
#endif

  for (i = 0; e[i].ptr != 0; i++) {
#if TGR_REGION_EUR || TGR_REGION_JPN
    memcpy(e[i].ptr + off, src, e[i].size);
#else
    memcpy(TGR_PTR(char *, e[i].ptr), src, e[i].size);
#endif
    src += e[i].size;
  }
#if TGR_REGION_EUR || TGR_REGION_JPN
  return src - start;
#endif
}

/* WHAT IT DOES: Copy every registered game variable into the save buffer,
 * checking it fits the space reserved for it. */
/* @implements 0x8022AED8 tgr BrVarSaveAll */
void BrVarSaveAll(char *buf)
{
#if TGR_REGION_EUR || TGR_REGION_JPN
  BrVarListSave(D_8028BB0C, buf + 0x7080, 0xdf88, 0);
#else
  BrVarListSave(D_8028BB0C, buf + 0x7080, 0xdf88);
#endif
}

/* WHAT IT DOES: Copy every registered game variable back out of the save
 * buffer. */
/* @implements 0x8022AF08 tgr BrVarLoadAll */
void BrVarLoadAll(char *buf)
{
#if TGR_REGION_EUR || TGR_REGION_JPN
  BrVarListLoad(D_8028BB0C, buf + 0x7080, 0);
#else
  BrVarListLoad(D_8028BB0C, buf + 0x7080);
#endif
}

/* WHAT IT DOES: Save the second variable list into its own 64-byte buffer. */
/* @implements 0x8022AF34 tgr BrVarSaveSmall */
void BrVarSaveSmall(void)
{
#if TGR_REGION_EUR || TGR_REGION_JPN
  BrVarListSave(D_8028BBC4, D_8034E570, 0x40, 0);
#else
  BrVarListSave(D_8028BBC4, D_8034E570, 0x40);
#endif
}

/* WHAT IT DOES: Restore the second variable list from its 64-byte buffer. */
/* @implements 0x8022AF64 tgr BrVarLoadSmall */
void BrVarLoadSmall(void)
{
#if TGR_REGION_EUR || TGR_REGION_JPN
  BrVarListLoad(D_8028BBC4, D_8034E570, 0);
#else
  BrVarListLoad(D_8028BBC4, D_8034E570);
#endif
}

#if TGR_REGION_EUR || TGR_REGION_JPN
#if TGR_REGION_EUR
extern int eur_D_8028DEC0;              /* demo frames played */
extern BrVarEnt eur_D_8028DEC4[];       /* the demo's resync lists */
extern BrVarEnt eur_D_8028E22C[];
extern BrVarEnt eur_D_8028E2CC[];
#define DEMO_FRAMES eur_D_8028DEC0
#define DEMO_VARS_A eur_D_8028E2CC
#define DEMO_VARS_B eur_D_8028E22C
#define DEMO_VARS_C eur_D_8028DEC4
#else
extern int jpn_D_8028D8F0;              /* demo frames played */
extern BrVarEnt jpn_D_8028D8F4[];       /* the demo's resync lists */
extern BrVarEnt jpn_D_8028DC5C[];
extern BrVarEnt jpn_D_8028DCFC[];
#define DEMO_FRAMES jpn_D_8028D8F0
#define DEMO_VARS_A jpn_D_8028DCFC
#define DEMO_VARS_B jpn_D_8028DC5C
#define DEMO_VARS_C jpn_D_8028D8F4
#endif
extern int D_8026FF1C;                  /* which demo */
void BrCamTracksidePick(BrCar *car);
void BrCamTracksideAim(BrCar *car, BrCarCam *cam);

/* WHAT IT DOES: The intro demo (mode 4, demo 0; Europe and Japan) resyncs itself from
 * its recording: count the frame, and where the recording being played
 * for the first car has an 0xFF 0xFF marker at its read position, skip it
 * and copy three variable lists back out of the recording (the read
 * position steps over them, rounded up to even), then put the first car on
 * its trackside camera. */
/* @implements eur:0x8022C340 tgr BrDemoVarsLoad */
/* @implements jpn:0x8022BAF8 tgr BrDemoVarsLoad */
void BrDemoVarsLoad(void)
{
  BrPadRec *p;
  int n;

  DEMO_FRAMES++;
  if (D_8026FF18 == 4 && D_8026FF1C == 0) {
    p = (BrPadRec *)D_8031B760[0].pad;
    if (p->ghostPos < p->ghostLen && (p->ghostPos + p->ghost)[0] == 0xff
        && (p->ghostPos + p->ghost)[1] == 0xff) {
      p->ghostPos += 2;
      p = (BrPadRec *)D_8031B760[0].pad;
      n = BrVarListLoad(DEMO_VARS_A, p->ghostPos + (char *)p->ghost, 0);
      p = (BrPadRec *)D_8031B760[0].pad;
      n += BrVarListLoad(DEMO_VARS_B, p->ghostPos + (char *)p->ghost + n, 0);
      p = (BrPadRec *)D_8031B760[0].pad;
      n += BrVarListLoad(DEMO_VARS_C, p->ghostPos + (char *)p->ghost + n, 0);
      p = (BrPadRec *)D_8031B760[0].pad;
      p->ghostPos += (n + 1) & ~1;
      D_8031B760[0].xf48 = 1;
      BrCamTracksidePick(&D_8031B760[0]);
      BrCamTracksideAim(&D_8031B760[0], &D_8031B760[0].cams[3]);
    }
  }
}
#endif

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
      && ((BrPadRec *)TGR_PTR(unsigned int *, car->pad))->rec[car->slot] == 0) {
    tgr_print("SAVING LAST LAP INFO\n");
    for (i = 0; i < D_8026FF08; i++) {
      ((BrPadRec *)TGR_PTR(unsigned int *, D_8031B760[i].pad))->recLen[car->slot] = 0;
      ((BrPadRec *)TGR_PTR(unsigned int *, D_8031B760[i].pad))->recKeep[car->slot] = 0x3840;
      ((BrPadRec *)TGR_PTR(unsigned int *, D_8031B760[i].pad))->rec[car->slot] = tgr_addr32((unsigned char *)D_803C8000[3 - car->slot].raw + i * 0x3840);
    }
    BrVarSaveAll(D_803C8000[3 - car->slot].raw);
  }
}
