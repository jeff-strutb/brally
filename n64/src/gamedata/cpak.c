/* cpak.c -- the Controller Pak save data
 */
#include "tgr/common.h"
#include "tgr/car.h"
#include "tgr/pad.h"

/* -- declarations -- */
void BrVolumesApply(void);
int BrRumbleInsertPrompt(int anyPad);
int func_8021CB4C();
void func_80223750(float param_1,float param_2);
void func_802237D0(float param_1,float param_2);
int BrSfxFadeDone(void);
void BrTextHighlightOff(void);
void BrTextAlignCentre(void);
void BrTextAlignLeft(void);
int BrTextSetColours();
void BrTextSetFont(int param_1);
void BrTextPrint();
void BrImageDrawAt(int *param_1,int param_2,int param_3);
int func_80246F90();
void BrPadConsume(unsigned int *param_1,unsigned int param_2);
void osSyncPrintf();
void *memcpy(void *dst, void *src, int n);
int func_80261940(void *param_1,unsigned char *param_2);
int func_80261CB0();
int func_80261F20(int param_1);
int func_80262370();
int bcmp(void *a, void *b, unsigned int n);
int func_80262660();
int func_80262A80();
extern int D_80216398;
extern int D_80270784;
extern int D_80271FA8;
extern int D_802723D0;
extern int D_802724F0;
extern unsigned char D_802724F4;
extern unsigned char D_802724F8;
extern unsigned char D_802724FC;
extern unsigned char *D_80272500;   /* the transfer buffer */
extern int D_80272558;
extern int D_8027255C;
extern int D_80272560;
extern int D_80272564;
extern int D_80272568;
extern int D_8027256C;
extern int D_80272570;
extern int D_80272574;
extern int D_80272578;
extern int D_8027257C;
extern int D_80272580;
extern int D_80272584;
extern int D_80272588;
extern int D_80272D48;
extern int D_8028D0B0;
extern int D_8028D0C0;
extern int D_8028D0E0;
extern int D_8028D0F0;
extern short D_802A4BE8;
extern float D_802A8FE4;
extern float D_802A8FE8;
extern float D_802A9018;
extern float D_802A901C;
extern unsigned char D_80307F00;
extern int D_80307F01;
extern char D_80316420;
extern char D_8031B1E8;
typedef struct { char raw[0x68]; } BrPfs;              /* an OSPfs */
typedef struct BrPfsState {    /* an OSPfsState, 0x20 bytes */
  unsigned int size;
  unsigned int company;         /* 0x04 */
  unsigned short game;          /* 0x08 */
  char pad0a[0x16];
} BrPfsState;
void func_802674D0(void *src, void *dst, int n);
int func_802677E0(BrPfs *pfs, int *maxFiles, int *used);
int func_80267930(BrPfs *pfs, int file, BrPfsState *state);
extern BrPfs D_80369EC0[2];
extern BrPfs D_8031A3F8[4];
extern char D_803163E0[];
extern int D_8026FF08;
int osPfsFindFile(BrPfs *pfs, unsigned short company, unsigned int game, char *name,
                  unsigned char *ext, int *file);
int osPfsReadWriteFile(BrPfs *pfs, int file, unsigned char flag, int offset, int size,
                       unsigned char *buf);
int osPfsFreeBlocks(BrPfs *pfs, int *bytes);
int osPfsAllocateFile(BrPfs *pfs, unsigned short company, unsigned int game, char *name,
                      unsigned char *ext, int size, int *file);
/* -- end declarations -- */

/* WHAT IT DOES: Ask the player to swap the Controller Pak for the Rumble
 * Pak: draw the message box and its five lines, then answer 1 (and consume
 * the press) once a confirm button (mask 0x8030) is down on the player's
 * pad -- or on either of the first two pads when anyPad is set -- else 0. */
/* @implements 0x80214A88 tgr BrRumbleInsertPrompt */
int BrRumbleInsertPrompt(int anyPad)
{
  int i;

  BrTextHighlightOff();
  BrTextAlignLeft();
  BrTextSetFont(12);
  BrTextSetColours(0xff, 0xff, 0xff, 0xff, 0xf5, 0);
  func_80246F90(0x73, 0xb4, 0x19a, 0x98, 3, 0, 0, 0x80, 0x80, 0x80);
  BrTextPrint("IF THE RUMBLE PAK IS TO BE USED,", 0x41, 0x6a);
  BrTextPrint("REMOVE THE CONTROLLER PAK AND", 0x41, 0x77);
  BrTextPrint("INSERT THE RUMBLE PAK INTO THE", 0x41, 0x84);
  BrTextPrint("CONTROLLER.  PRESS THE A BUTTON", 0x41, 0x91);
  BrTextPrint("TO CONTINUE.", 0x41, 0x9e);
  for (i = 0; i < 2; i++) {
    if (anyPad == 0 && i != D_80271FA8) continue;
    if (*(unsigned int *)((char *)&D_8036A8E0 + i * 0x15c) & 0x8030) {
      BrPadConsume((unsigned int *)((char *)&D_8036A8E0 + i * 0x15c), 0x8030);
      return 1;
    }
  }
  return 0;
}

/* WHAT IT DOES: Probe every connected controller for a Rumble Pak: each one
 * that answers is marked present and its motor stopped.  Pak access is
 * flagged busy meanwhile.
 * RESIDUE (5): the flag slot's address.  The ROM adds i to the table base
 * before the stop call and keeps the sum in s2; ours keeps the base in s2 and
 * adds after the call.  A named pfs pointer and an integer-cast table address
 * (which stops IDO hoisting the base out of the loop) took it from 51. */
/* @t4-pass 0x80214BEC 1 2026-10-03 compiles 121 best 5 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x80214BEC 2 2026-10-03 compiles 121 best 5 moved 0  (n64/tools/n64permute.py) */
/* @t3 0x80214BEC */
/* @implements 0x80214BEC tgr BrRumbleProbe */
void BrRumbleProbe(void)
{
  int i;
  BrPfs *pfs;
  char *flag;

  D_802A4BE8 = 0;
  for (i = 0; i < D_8026FF08; i++) {
    pfs = &D_8031A3F8[i];
    if (func_80262370(&D_80272D48, (int)pfs, i) == 0) {
      flag = (char *)((int)&D_8031B1E8 + i);
      func_80261F20((int)pfs);
      *flag = 1;
    }
  }
  D_802A4BE8 = 1;
}

/* WHAT IT DOES: Initialise a Rumble Pak on every connected controller:
 * answer 1 as soon as a controller has nothing plugged in, holds a
 * Controller Pak (the pak initialises) or fails to start its motor, and 0
 * when every one carries a working Rumble Pak (motor stopped).  Nothing in
 * the ROM calls it. */
/* @implements 0x80214CB0 tgr BrRumbleInitAll */
int BrRumbleInitAll(void)
{
  unsigned char bits;
  int i;

  D_802A4BE8 = 0;
  for (i = 0; i < D_8026FF08; i++) {
    func_80261940(&D_80272D48, &bits);
    if ((bits & (1 << i)) == 0)
      goto fail;
    D_802724F0 = func_80261CB0(&D_80272D48, &D_80369EC0[i], i);
    if (D_802724F0 != 0) {
      if (D_802724F0 != 10)
        goto fail;
      if (func_80262370(&D_80272D48, i * 0x68 + (int)D_8031A3F8, i) != 0)
        goto fail;
      func_80261F20(i * 0x68 + (int)D_8031A3F8);
    }
    if (D_802724F0 == 0)
      goto fail;
  }
  D_802A4BE8 = 1;
  return 0;
fail:
  D_802A4BE8 = 1;
  return 1;
}

/* WHAT IT DOES: One step of the Controller Pak check a screen runs before it
 * loads the season (kind 0), the ghost (1) or the configuration (2), or saves
 * the configuration (3); quiet skips the fades and messages.  A state machine
 * kept in function statics: fade the sound, probe the pak (a Rumble Pak in
 * the way asks to swap it; a new pak's id is remembered, a changed one is
 * announced), find the file (and on a save allocate it), read or write it --
 * the season keeping the other player's options, the ghost unlocking its
 * track and car, the configuration with the player's car settings -- show
 * the result or the error and wait for a button, offer to put the Rumble Pak
 * back.  Answers 1 on the frame it finishes, else 0.  The frame is the ROM's
 * 0xB8: the unused ints hold the slots of h and y, and hBase is a byte the
 * ROM reads without ever setting (the "loaded" box's height when the screen
 * was not entered from a save).
 * RESIDUE (30): three temporaries numbered the other way round -- the fade
 * counter's v0/v1, the error code's v0/v1 for the message switch, and the
 * text row's a2/a3 in the "loaded" box. */
/* @t4-pass 0x80214E0C 1 2026-09-26 compiles 17 best 1632 moved 7  (n64/tools/n64permute.py) */
/* @t4-pass 0x80214E0C 2 2026-09-26 compiles 17 best 1639 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x80214E0C 3 2026-09-26 compiles 17 best 1637 moved 2  (n64/tools/n64permute.py) */
/* @t4-pass 0x80214E0C 4 2026-09-26 compiles 41 best 1637 moved 0  (n64/tools/n64permute.py) */
/* @implements 0x80214E0C tgr BrCpakCheck */
int BrCpakCheck(int kind, unsigned char quiet)
{
  static int file;               /* 0x8031642C: the open file */
  static unsigned char state;    /* 0x80316430 */
  static unsigned char wait;     /* 0x80316431: frames the fade has been done */
  int unusedB4;
  int h;
  int unused9C[5];
  int y;
  int unused94;
  char *name;
  int freeBytes;
  unsigned char pakBits;
  unsigned char hBase;           /* never set */
  int size;
  int opts[5];
  unsigned char nameSeason[16] = {
    0x2D, 0x28, 0x29, 0x0F, 0x20, 0x1E, 0x1A, 0x2B, 0x0F, 0x2C, 0x1E, 0x1A, 0x2C, 0x28, 0x27, 0x00
  };
  unsigned char nameGhost[16] = {
    0x2D, 0x28, 0x29, 0x0F, 0x20, 0x1E, 0x1A, 0x2B, 0x0F, 0x20, 0x21, 0x28, 0x2C, 0x2D, 0x00, 0x00
  };
  unsigned char nameOptions[16] = {
    0x2D, 0x28, 0x29, 0x0F, 0x20, 0x1E, 0x1A, 0x2B, 0x0F, 0x28, 0x29, 0x2D, 0x22, 0x28, 0x27, 0x2C
  };
  unsigned char ext[4] = { 0, 0, 0, 0 };

  D_802A4BE8 = 0;
  if (D_802724F8 == 0) {
    D_802724F8 = 1;
    D_802724F0 = 0;
    state = 0;
    wait = 0;
  }
  if (kind == 0) {
    name = (char *)nameSeason;
    size = 0x200;
  } else if (kind == 1) {
    name = (char *)nameGhost;
    size = 0x3A00;
  } else if (kind == 2 || kind == 3) {
    name = (char *)nameOptions;
    size = 0x100;
  }
  switch (state) {
  case 0:                       /* fade the sound */
    if (quiet == 0) {
      func_80223750(0.0f, 0.2f);
      func_802237D0(0.0f, 0.2f);
      state = 1;
    } else {
      state = 2;
    }
    break;
  case 1:                       /* wait for it to go */
    if (BrSfxFadeDone() != 0 && wait++ == 3) {
      state = 2;
    }
    break;
  case 2:                       /* probe the pak */
    func_80261940(&D_80272D48, &pakBits);
    if ((pakBits & (1 << D_80271FA8)) == 0) {
      D_802724F0 = 1;
      state = 9;
    } else {
      osSyncPrintf("\nInitializing controller pak...\n");
      D_802724F0 = func_80261CB0(&D_80272D48, &D_80369EC0[D_80271FA8], D_80271FA8);
      if (D_802724F0 != 0) {
        if (D_802724F0 == 10) {         /* a Rumble Pak */
          if (func_80262370(&D_80272D48, &D_8031A3F8[D_80271FA8], D_80271FA8) == 0) {
            func_80261F20((int)&D_8031A3F8[D_80271FA8]);
            D_802724F0 = 9999;
            D_802724FC = 1;
            state = 9;
          } else {
            D_802724F0 = 0;
            state = 3;
          }
        } else {
          (&D_8031B1E8)[D_80271FA8] = 0;
          state = 9;
        }
      } else if (D_802724F0 == 0) {
        (&D_8031B1E8)[D_80271FA8] = 0;
        if ((&D_80316420)[D_80271FA8] == 0) {
          (&D_80316420)[D_80271FA8] = 1;
          memcpy(&D_803163E0[D_80271FA8 * 32], D_80369EC0[D_80271FA8].raw + 0xc, 32);
          state = 5;
        } else if (bcmp(&D_803163E0[D_80271FA8 * 32], D_80369EC0[D_80271FA8].raw + 0xc, 32) != 0) {
          memcpy(&D_803163E0[D_80271FA8 * 32], D_80369EC0[D_80271FA8].raw + 0xc, 32);
          state = 4;
        } else {
          state = 5;
        }
      }
    }
    break;
  case 3:
    D_802724F0 = func_80262660(&D_80369EC0[D_80271FA8]);
    if (D_802724F0 != 0) {
      state = 9;
    } else {
      state = 2;
    }
    break;
  case 4:                       /* a different pak */
    if (quiet != 0) {
      state = 5;
    } else {
      BrTextHighlightOff();
      BrTextAlignLeft();
      BrTextSetFont(12);
      BrTextSetColours(0xFF, 0xFF, 0xFF, 0xFF, 0xF5, 0);
      func_80246F90(0xAF, 0xC3, 0x122, 0x89, 3, 0, 0, 0x80, 0x80, 0x80);
      BrTextPrint("A NEW CONTROLLER PAK", 95, 113);
      BrTextPrint("WAS INSERTED.", 95, 126);
      BrTextPrint("THIS CONTROLLER PAK", 95, 145);
      BrTextPrint("WILL BE USED.", 95, 158);
      if (D_8036A8E0[D_80271FA8].pressed & 0x8030) {
        BrPadConsume(&D_8036A8E0[D_80271FA8].pressed, 0x8030);
        state = 5;
      }
    }
    break;
  case 5:                       /* find the file */
    osSyncPrintf("Finding file...\n");
    D_802724F0 = osPfsFindFile(&D_80369EC0[D_80271FA8], 0x3544, 0x4E475245, name, ext, &file);
    if (D_802724F0 == 3) {
      func_80262A80(&D_80369EC0[D_80271FA8]);
      D_802724F0 = osPfsFindFile(&D_80369EC0[D_80271FA8], 0x3544, 0x4E475245, name, ext, &file);
    }
    if (D_802724F0 == 0) {
      if (kind == 3) {
        state = 8;
      } else {
        state = 6;
      }
    } else if (kind == 3) {
      state = 7;
    } else if (quiet != 0) {
      state = 12;
    } else {
      state = 9;
    }
    break;
  case 6:                       /* load */
    if (kind == 0) {
      osSyncPrintf("Loading season data...\n");
    } else if (kind == 1) {
      osSyncPrintf("Loading ghost data...\n");
    } else if (kind == 2) {
      osSyncPrintf("Loading configuration...\n");
    }
    if (kind != 2) {
      D_802724F0 = osPfsReadWriteFile(&D_80369EC0[D_80271FA8], file, 0, 0, size, D_80272500);
    } else {
      D_802724F0 = osPfsReadWriteFile(&D_80369EC0[D_80271FA8], file, 0, 0, 0x80, D_80272500);
    }
    if (D_802724F0 == 0) {
      if (kind == 0) {
        opts[0] = D_8031B760[D_80271FA8 ^ 1].season->xd4;
        opts[1] = D_8031B760[D_80271FA8 ^ 1].season->xd8;
        opts[2] = D_8031B760[D_80271FA8 ^ 1].season->xdc;
        opts[3] = D_8031B760[D_80271FA8 ^ 1].season->xe0;
        opts[4] = D_8031B760[D_80271FA8 ^ 1].season->xe4;
        memcpy(D_8031B760[0].season, D_80272500, 0x128);
        memcpy(D_8031B760[1].season, D_80272500, 0x128);
        D_8031B760[D_80271FA8 ^ 1].season->xd4 = opts[0];
        D_8031B760[D_80271FA8 ^ 1].season->xd8 = opts[1];
        D_8031B760[D_80271FA8 ^ 1].season->xdc = opts[2];
        D_8031B760[D_80271FA8 ^ 1].season->xe0 = opts[3];
        D_8031B760[D_80271FA8 ^ 1].season->xe4 = opts[4];
        osSyncPrintf("Done!\n");
      } else if (kind == 1) {
        osSyncPrintf("Extracting ghost data...\n");
        D_80270784 = func_8021CB4C(&D_80307F00, 0xDE5C, D_80272500, 2);
        D_8031B760[0].season->xce |= 1 << (&D_80307F00)[0];
        D_8031B760[0].season->unlocked |= 1 << (&D_80307F00)[1];
        osSyncPrintf("Done!\n");
      } else if (kind == 2) {
        memcpy(&D_802723D0, D_80272500, 12);
        osSyncPrintf("Loading car equipment settings...\n");
        D_802724F0 = osPfsReadWriteFile(&D_80369EC0[D_80271FA8], file, 0, 0x80, 0x80, D_80272500);
        if (D_802724F0 == 0) {
          memcpy(opts, D_80272500, 20);
          D_8031B760[D_80271FA8].season->xd4 = opts[0];
          D_8031B760[D_80271FA8].season->xd8 = opts[1];
          D_8031B760[D_80271FA8].season->xdc = opts[2];
          D_8031B760[D_80271FA8].season->xe0 = opts[3];
          D_8031B760[D_80271FA8].season->xe4 = opts[4];
          osSyncPrintf("Done!\n");
        } else {
          state = 9;
        }
      }
      if (state != 9) {
        if (kind == 2) {
          state = 11;
        } else if (D_802724FC == 0) {
          state = 12;
        } else {
          state = 11;
        }
      }
    } else {
      state = 9;
    }
    break;
  case 7:                       /* make room for the configuration */
    D_802724F0 = osPfsFreeBlocks(&D_80369EC0[D_80271FA8], &freeBytes);
    if (D_802724F0 == 0) {
      if (freeBytes < 0x100) {
        D_802724F0 = 7;
        state = 9;
      } else {
        osSyncPrintf("Allocating %d bytes for configuration...\n", 0x100);
        D_802724F0 = osPfsAllocateFile(&D_80369EC0[D_80271FA8], 0x3544, 0x4E475245, name, ext, 0x100, &file);
        if (D_802724F0 == 3) {
          func_80262A80(&D_80369EC0[D_80271FA8]);
          D_802724F0 = osPfsAllocateFile(&D_80369EC0[D_80271FA8], 0x3544, 0x4E475245, name, ext, 0x100, &file);
        }
        if (D_802724F0 == 0) {
          state = 8;
        } else {
          state = 9;
        }
      }
    } else {
      state = 9;
    }
    break;
  case 8:                       /* save the configuration */
    osSyncPrintf("Saving configuration...\n");
    memcpy(D_80272500, &D_802723D0, 12);
    D_802724F0 = osPfsReadWriteFile(&D_80369EC0[D_80271FA8], file, 1, 0, 0x80, D_80272500);
    if (D_802724F0 != 0) {
      state = 9;
    } else {
      opts[0] = D_8031B760[D_80271FA8].season->xd4;
      opts[1] = D_8031B760[D_80271FA8].season->xd8;
      opts[2] = D_8031B760[D_80271FA8].season->xdc;
      opts[3] = D_8031B760[D_80271FA8].season->xe0;
      opts[4] = D_8031B760[D_80271FA8].season->xe4;
      memcpy(D_80272500, opts, 20);
      D_802724F0 = osPfsReadWriteFile(&D_80369EC0[D_80271FA8], file, 1, 0x80, 0x80, D_80272500);
      if (D_802724F0 == 0) {
        osSyncPrintf("Done!\n");
        state = 11;
      } else {
        state = 9;
      }
    }
    break;
  case 9:                       /* an error, or the Rumble Pak is in the way */
    if (quiet != 0) {
      state = 12;
      break;
    }
    BrTextHighlightOff();
    BrTextAlignLeft();
    BrTextSetFont(12);
    BrTextSetColours(0xFF, 0xFF, 0xFF, 0xFF, 0xF5, 0);
    switch (D_802724F0) {
    case 10:
      func_80246F90(0xBD, 0xD9, 0x106, 0x4A, 3, 0, 0, 0x80, 0x80, 0x80);
      BrTextPrint("THE CONTROLLER PAK", 102, 124);
      BrTextPrint("IS NONFUNCTIONAL!", 102, 138);
      break;
    case 1:
    case 11:
      func_80246F90(0x94, 0xD8, 0x158, 0x4C, 3, 0, 0, 0x80, 0x80, 0x80);
      BrTextPrint("CONTROLLER PAK NOT FOUND.", 82, 124);
      if (kind == 3) {
        BrTextPrint("DATA CANNOT BE SAVED.", 82, 138);
      } else {
        BrTextPrint("DATA CANNOT BE LOADED.", 82, 138);
      }
      break;
    case 9999:
      y = 0x148 - D_8028D0C0;
      func_80246F90(0x79, 0xB0, 0x18D, 0xA0, 3, 0, 0, 0x80, 0x80, 0x80);
      BrTextPrint("PLEASE REMOVE THE RUMBLE PAK", 68, 104);
      BrTextPrint("AND INSERT THE CONTROLLER PAK", 68, 117);
      BrTextPrint("INTO THE CONTROLLER.  PRESS", 68, 130);
      BrTextPrint("THE A BUTTON WHEN READY.", 68, 143);
      BrTextAlignLeft();
      BrTextSetFont(10);
      BrTextPrint("%wwOK", (D_8028D0C0 + 0xE3U) >> 1, (y + 18) >> 1);
      BrTextPrint("%wwCANCEL", (D_8028D0F0 + 0x144U) >> 1, (y + 18) >> 1);
      BrImageDrawAt(&D_8028D0B0, 0xDD, y);
      BrImageDrawAt(&D_8028D0E0, 0x13E, y);
      break;
    case 7:
    case 8:
      func_80246F90(0xA5, 0xA5, 0x136, 0xBD, 3, 0, 0, 0x80, 0x80, 0x80);
      BrTextPrint("INSUFFICIENT FREE PAGES", 90, 98);
      BrTextPrint("OR FREE NOTES IN THE", 90, 111);
      BrTextPrint("CONTROLLER PAK.", 90, 124);
      BrTextPrint("ONE PAGE AND ONE NOTE", 90, 143);
      BrTextPrint("ARE NEEDED TO SAVE THE", 90, 156);
      BrTextPrint("OPTIONS.", 90, 169);
      break;
    case 4:
      func_80246F90(0xBF, 0xD9, 0x102, 0x4A, 3, 0, 0, 0x80, 0x80, 0x80);
      BrTextPrint("CONTROLLER ERROR", 103, 124);
      BrTextPrint("HAS BEEN DETECTED!", 103, 138);
      break;
    case 5:
      func_80246F90(0xAF, 0xD9, 0x122, 0x4A, 3, 0, 0, 0x80, 0x80, 0x80);
      if (kind == 3) {
        BrTextPrint("CONTROLLER PAK ERROR", 95, 124);
        BrTextPrint("HAS BEEN DETECTED!", 95, 138);
      } else {
        BrTextPrint("SAVED DATA NOT FOUND", 95, 124);
        BrTextPrint("IN CONTROLLER PAK.", 95, 138);
      }
      break;
    case 2:
      func_80246F90(0xAF, 0xB9, 0x122, 0x89, 3, 0, 0, 0x80, 0x80, 0x80);
      BrTextPrint("A NEW CONTROLLER PAK", 95, 108);
      BrTextPrint("WAS INSERTED.", 95, 121);
      BrTextPrint("THIS CONTROLLER PAK", 95, 140);
      BrTextPrint("WILL BE USED.", 95, 153);
      break;
    default:
      func_80246F90(0xAF, 0xD9, 0x122, 0x4A, 3, 0, 0, 0x80, 0x80, 0x80);
      BrTextPrint("CONTROLLER PAK ERROR", 95, 124);
      BrTextPrint("HAS BEEN DETECTED.", 95, 138);
      break;
    }
    if (D_802724F0 != 9999) {
      if (D_8036A8E0[D_80271FA8].pressed & 0x8030) {
        BrPadConsume(&D_8036A8E0[D_80271FA8].pressed, 0x8030);
        D_802724F0 = 0;
        if (D_802724F0 == 2) {
          state = 2;
        } else {
          state = 12;
        }
      }
    } else if (D_8036A8E0[D_80271FA8].pressed & 0x10) {
      BrPadConsume(&D_8036A8E0[D_80271FA8].pressed, 0x10);
      D_802724F0 = 0;
      state = 2;
    } else if (D_8036A8E0[D_80271FA8].pressed & 0x20) {
      BrPadConsume(&D_8036A8E0[D_80271FA8].pressed, 0x20);
      state = 12;
    }
    break;
  case 11:                      /* done: say so and wait for a button */
    if (quiet != 0) {
      state = 12;
      break;
    }
    BrTextAlignCentre();
    BrTextHighlightOff();
    BrTextSetFont(12);
    switch (kind) {
    case 0:
      if (D_802724F4 == 0) {
        h = hBase + 0xCB;
      } else {
        h = 0xCE;
      }
      func_80246F90(0xE2, h, 0xBC, 0x4A, 3, 0, 0, 0x80, 0x80, 0x80);
      BrTextPrint("%ywSEASON DATA", 159, (h + 32) >> 1, (h + 32) >> 1);
      BrTextPrint("%ywLOADED OK!", 159, ((h + 32) >> 1) + 14);
      break;
    case 1:
      if (D_802724F4 == 0) {
        h = hBase + 0xCB;
      } else {
        h = 0xCE;
      }
      func_80246F90(0xE2, h, 0xBC, 0x4A, 3, 0, 0, 0x80, 0x80, 0x80);
      BrTextPrint("%ywGHOST DATA", 159, (h + 32) >> 1, (h + 32) >> 1);
      BrTextPrint("%ywLOADED OK!", 159, ((h + 32) >> 1) + 14);
      break;
    case 2:
    case 3:
      func_80246F90(0xD8, 0xD9, 0xD0, 0x4A, 3, 0, 0, 0x80, 0x80, 0x80);
      BrTextPrint("%ywCONFIGURATION", 159, 124);
      if (kind == 2) {
        BrTextPrint("%ywLOADED OK!", 159, 138);
      } else {
        BrTextPrint("%ywSAVED OK!", 159, 138);
      }
      break;
    }
    if (D_8036A8E0[D_80271FA8].pressed & 0x8030) {
      BrPadConsume(&D_8036A8E0[D_80271FA8].pressed, 0x8030);
      if (kind == 2) {
        BrVolumesApply();
      }
      if (D_802724FC != 0) {
        state = 10;
      } else {
        state = 12;
      }
    }
    break;
  case 10:                      /* offer to put the Rumble Pak back */
    if (BrRumbleInsertPrompt(0) != 0) {
      D_802724FC = 0;
      if (func_80262370(&D_80272D48, &D_8031A3F8[D_80271FA8], D_80271FA8) == 0) {
        (&D_8031B1E8)[D_80271FA8] = 1;
      }
      state = 12;
    }
    break;
  case 12:                      /* done */
    D_802724F0 = 0;
    D_802724F8 = 0;
    if (quiet == 0) {
      func_80223750(1.0f, 0.2f);
      func_802237D0(1.0f, 0.2f);
    }
    D_802A4BE8 = 1;
    return 1;
  }
  D_802A4BE8 = 1;
  return 0;
}
