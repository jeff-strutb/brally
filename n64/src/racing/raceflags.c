/* raceflags.c -- per-race option flags
 */
#include "tgr/common.h"
#include "tgr/car.h"

typedef struct BrFlagObj {      /* a track object (0x54 bytes) */
  char pad00[0x44];
  unsigned int *dl;             /* 0x44  its display list */
  char pad48[0x4C - 0x48];
  unsigned short flags;         /* 0x4C  4: keeps the colour override; 8: set when patched */
  char pad4e[0x54 - 0x4E];
} BrFlagObj;
typedef struct BrFlagHdr {      /* the loaded track's header */
  char pad00[0x60];
  BrFlagObj *objs;              /* 0x60 */
  int nObjs;                    /* 0x64 */
} BrFlagHdr;
extern BrFlagHdr D_80025C00;

/* -- declarations -- */
void BrRaceFlagsApply(void);
extern int D_8028AA78;
extern int D_8028AA80;
extern int D_8028AA84;
extern int D_8028AA8C;
int BrDlRaceKindPatch();
extern unsigned int D_8028ABA8[6][4][2];
extern unsigned int D_8028AC68[6][4][2];
extern unsigned int D_8028AD28[6][4][2];
extern int D_8028ADE8;
extern int D_8028B7F4;
extern int D_8028B940;
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


typedef struct {
  unsigned int w0, w1;
  unsigned int to0, to1;
} DlSwap1;

extern DlSwap1 D_8028AB88[1];
extern unsigned int D_8028AB98[2][2];

/* WHAT IT DOES: Walk a display list up to its end command and patch it for
 * the race kind: each other-mode command found in the six-entry table is
 * replaced by that entry's variant for the kind (returns 1 if one of the last
 * three entries matched); a combine command is swapped for its kind-1/2
 * replacement, and with kind 3 set and the list flagged, a combine found in
 * the two-entry list marks the following primitive and environment colours
 * to be overridden. Clears the flag. */
/* @implements 0x8021B72C tgr BrDlRaceKindPatch */
int BrDlRaceKindPatch(unsigned int *dl, unsigned int (*table)[4][2])
{
  int plain;
  int ret;
  int kind;
  int tint;
  int i;

  ret = 0;
  plain = D_8028AA80 == 0 && D_8028AA8C == 0;
  kind = !plain;
  kind = D_8028AA78 + kind + 1;
  tint = 0;
  if (dl != 0) {
    for (;; dl += 2) {
      switch ((unsigned char)(dl[0] >> 24)) {
      case 0xB8:
        goto done;
      case 0xB9:
        for (i = 0; i < 6; i++) {
          if (dl[0] == table[i][0][0] && dl[1] == table[i][0][1]) {
            dl[0] = table[i][kind][0];
            dl[1] = table[i][kind][1];
            if (i >= 3) {
              ret = 1;
            }
            break;
          }
        }
        break;
      case 0xFC:
        if (plain) {
          for (i = 0; i < 1; i++) {
            if (dl[0] == D_8028AB88[i].w0 && dl[1] == D_8028AB88[i].w1) {
              dl[0] = D_8028AB88[i].to0;
              dl[1] = D_8028AB88[i].to1;
              break;
            }
          }
        }
        if (D_8028AA84 != 0 && D_8028ADE8 != 0) {
          for (i = 0; i < 2; i++) {
            if (dl[0] == D_8028AB98[i][0] && dl[1] == D_8028AB98[i][1]) {
              break;
            }
          }
          if (i < 2) {
            tint = 1;
          } else {
            tint = 0;
          }
        }
        break;
      case 0xFA:
        if (tint && D_8028AA84 != 0) {
          dl[1] = 0x60789000;
        }
        break;
      case 0xFB:
        if (tint && D_8028AA84 != 0) {
          dl[1] = 0x8C9CA800;
        }
        break;
      }
    }
  }
done:
  D_8028ADE8 = 0;
  return ret;
}

/* WHAT IT DOES: Re-evaluate everything that depends on the race-kind flags:
 * marks each track object whose condition list now holds, and does the same
 * for every car's model parts. */
/* @implements 0x8021B97C tgr BrRaceFlagsApply */
void BrRaceFlagsApply(void)
{
  int flag;
  int i;
  int n;
  int j;

  flag = D_8028B940 != 2 && D_8028B940 != 7;
  for (i = 0; i < D_80025C00.nObjs; i++) {
    if ((D_80025C00.objs[i].flags & 4) == 0) {
      D_8028ADE8 = flag;
    }
    if (BrDlRaceKindPatch(D_80025C00.objs[i].dl, D_8028ABA8)) {
      D_80025C00.objs[i].flags |= 8;
    }
  }
  for (n = 0; n < D_8028B7F4; n++) {
    for (j = 0; j < 3; j++) {
      if (D_8031B760[n].colour[3] == 2) {
        for (i = 0; i < 10; i++) {
          BrDlRaceKindPatch(((BrCarModel *)D_8031B760[n].model)->dl[j][i], D_8028AC68);
        }
        for (i = 0; i < 3; i++) {
          BrDlRaceKindPatch(((BrCarModel *)D_8031B760[n].model)->dl2[j][i], D_8028AD28);
        }
      } else {
        for (i = 0; i < 10; i++) {
          BrDlRaceKindPatch(((BrCarModel *)D_8031B760[n].model)->dl[j][i], D_8028ABA8);
        }
        for (i = 0; i < 3; i++) {
          BrDlRaceKindPatch(((BrCarModel *)D_8031B760[n].model)->dl2[j][i], D_8028ABA8);
        }
      }
    }
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

