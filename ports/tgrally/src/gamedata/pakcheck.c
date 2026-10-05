/* pakcheck.c -- the start-up Controller Pak check: does the pak in port 1
 * hold only this game's files.  Its own object in the ROM (statics at
 * 0x8028DDA0 and 0x8036A278, apart from cpak.c's).
 */
#include "tgr/common.h"

/* -- declarations -- */
extern int D_802724F0;
extern int D_80272D48;
extern short D_802A4BE8;
typedef struct { char raw[0x68]; } BrPfs;              /* an OSPfs */
typedef struct BrPfsState {    /* an OSPfsState, 0x20 bytes */
  unsigned int size;
  unsigned int company;         /* 0x04 */
  unsigned short game;          /* 0x08 */
  char pad0a[0x16];
} BrPfsState;
extern BrPfs D_80369EC0[2];
extern BrPfs D_8031A3F8[4];
extern char D_803163E0[];
/* -- end declarations -- */

/* WHAT IT DOES: Check the controller pak in port 1 before the game uses it:
 * initialise it (a rumble pak, or a pak with a damaged id that repairs,
 * is taken as fine), put the saved pak id back, count its files (checking
 * a pak reported inconsistent), then read every file's state and fail if
 * a readable one belongs to another game (not company NGRE, game 5D).
 * Returns 1 when the pak can be used.
 * The ROM's frame is 0x80 with the file-count word at sp+0x64: two
 * declared, never-used buffers around it (24 bytes above, 40 below) hold
 * their slots under IDO. */
/* @implements 0x80254620 tgr BrPakCheckFiles */
int BrPakCheckFiles(void)
{
  extern unsigned short D_8028DDA0;          /* 0x8028DDA0: files whose state would not read */
  extern int D_8036A278;                        /* 0x8036A278 */
  extern BrPfsState D_8036A280[16];           /* 0x8036A280 */
  char unusedA[24];
  int maxFiles;
  char unusedB[40];
  int i;

  D_802A4BE8 = 0;
  D_802724F0 = osPfsInitPak(&D_80272D48, &D_80369EC0[0], 0);
  if (D_802724F0 != 0 && D_802724F0 == 10) {
    if (osMotorInit(&D_80272D48, &D_8031A3F8[0], 0) == 0) {
      D_802A4BE8 = 1;
      return 1;
    }
    D_802724F0 = osPfsRepairId(&D_80369EC0[0]);
    if (D_802724F0 == 0) {
      osPfsInitPak(&D_80272D48, &D_80369EC0[0], 0);
    }
  }
  if (D_802724F0 != 0) {
    D_802A4BE8 = 1;
    return 1;
  }
  bcopy(D_80369EC0[0].raw + 0xc, D_803163E0, 0x20);
  D_802724F0 = osPfsNumFiles(&D_80369EC0[0], &maxFiles, &D_8036A278);
  if (D_802724F0 != 0) {
    if (D_802724F0 == 3) {
      if (osPfsChecker(&D_80369EC0[0]) != 0) {
        D_802A4BE8 = 1;
        return 1;
      }
    } else {
      D_802A4BE8 = 1;
      return 1;
    }
  }
  for (i = 0; i < 16; i++) {
    if (osPfsFileState(&D_80369EC0[0], i, &D_8036A280[i]) != 0) {
      D_8028DDA0 |= 1 << i;
    }
  }
  for (i = 0; i < 16; i++) {
    if (!(D_8028DDA0 & (1 << i))) {
      osSyncPrintf("%d: %08x %04x\n", i, D_8036A280[i].company, D_8036A280[i].game);
      if (D_8036A280[i].company != 0x4e475245 || D_8036A280[i].game != 0x3544) {
        D_802A4BE8 = 1;
        return 0;
      }
    }
  }
  D_802A4BE8 = 1;
  return 1;
}
