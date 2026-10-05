/* loadsave.c -- the Controller Pak data screen: load or save the season,
 * the time-attack ghost car or the options, with every Controller Pak
 * message the game can show on the way.
 *
 * BrLoadSaveScreen goes through IDO's global optimiser.  Its locals are
 * declared in the order of the original's frame; the screen's row, state and
 * pak flags are function-static (.data, then .bss after D_80316420), and the
 * file-scope state it shares with the race and frame loop is defined here.
 * The mode byte D_802724F4 is copied into a local for its compare chains;
 * the last compare reads the copy through an expression (mode | 0), so uopt
 * orders the constant first as the ROM has it.  The box runs it
 * with no pak and with an empty pak (save, overwrite, options, ghost, load);
 * the pak error screens other than "not found" / "no pak" are not reached yet.
 */
#include "tgr/common.h"
#include "tgr/car.h"
#include "tgr/pad.h"
#include "tgr/season.h"
#include "tgr/gbi.h"

/* -- declarations -- */
typedef struct BrImage {        /* as drawing/image.c (0x30 bytes) */
  unsigned char *data;
  int x4;
  int x8;
  unsigned char siz;
  char pad0d[3];
  int w;                        /* 0x10 */
  unsigned int h;               /* 0x14 */
  unsigned int stripH;          /* 0x18 */
  int x;                        /* 0x1C */
  int y;                        /* 0x20 */
  char pad24[0x30 - 0x24];
} BrImage;
extern BrImage D_8028CB40;      /* the background */
extern BrImage D_8028CB70;      /* the panel */
extern BrImage D_8028D0B0;      /* the A button */
extern BrImage D_8028D0E0;      /* the B button */

typedef struct OSPfs {          /* libultra's Controller Pak handle (0x68 bytes) */
  int status;
  void *queue;
  int channel;
  unsigned char id[32];         /* 0x0C */
  char pad2c[0x68 - 0x2C];
} OSPfs;
extern OSPfs D_80369EC0[4];     /* one per port */
extern OSPfs D_8031A3F8[4];     /* the Rumble Pak's, one per port */
extern unsigned char D_803163E0[4][32];  /* the id of the pak last used on each port */
unsigned char D_80316420[2];            /* a pak has been used on the port (ports 2-3 run on into the statics) */
extern unsigned char D_8031B1E8[4];      /* the port has a Rumble Pak */
extern char D_80272D48[];       /* the serial interface's message queue */

extern short D_802A4BE8;        /* a mode change is under way */
extern int D_80271FA8;          /* the port of the controlling pad */
extern int D_8028AAB0;          /* screen width */
extern int D_8028AAB4;          /* screen height */
extern int D_80270784;          /* bytes in the saved ghost */
extern int D_8028B940;          /* the track */
extern unsigned char D_80307F00[];  /* the saved ghost: track, kind, ... */
extern char D_802723D0[];       /* the options (12 bytes) */
extern Gfx *D_8028A858;

void osViBlack(int black);
void osSyncPrintf(char *fmt, ...);
void *memcpy(void *dst, void *src, unsigned int n);
int bcmp(void *a, void *b, unsigned int n);
int osPfsIsPlug(void *mq, unsigned char *pattern);
int osPfsInitPak(void *mq, OSPfs *pfs, int channel);
int osPfsRepairId(OSPfs *pfs);
int osMotorInit(void *mq, OSPfs *pfs, int channel);
int osMotorStop(OSPfs *pfs);
int osPfsFindFile(OSPfs *pfs, unsigned short company, unsigned int game, unsigned char *name,
                  unsigned char *ext, int *file);
int osPfsChecker(OSPfs *pfs);
int osPfsReadWriteFile(OSPfs *pfs, int file, unsigned char flag, int offset, int size,
                       unsigned char *buf);
int osPfsFreeBlocks(OSPfs *pfs, int *bytes);
int osPfsAllocateFile(OSPfs *pfs, unsigned short company, unsigned int game, unsigned char *name,
                      unsigned char *ext, int size, int *file);
void BrIfaceMemReset(void);
void BrAllocPaintShopGfxMem(BrImage *img);
unsigned int BrIfaceMemAlloc(int size);
void BrImageDrawTinted(BrImage *img, unsigned char r, unsigned char g, unsigned char b);
void BrImageDrawAt(BrImage *img, int x, int y);
void BrMenuCameraSet(float w, float h);
int BrFadeInDone(void);
int BrSfxFadeDone(void);
int BrFadeOutDone(void);
void BrFadeTo(float level, float seconds);
void BrSfxFadeTo(float level, float seconds);
void BrMusicFadeTo(float level, float seconds);
void BrFadeStep(void);
void BrFadeBarsDraw(void);
void BrClockTick(void);
void BrVolumesApply(void);
void BrChampionshipStart(void);
void BrTimeAttackStart(void);
void BrMainMenu(void);
void func_80209434(void);
void BrFrameBeginLayout1(void);
void BrFrameEnd(void);
void BrViewportSet(int x, int y, int w, int h, int scissor);
void BrZBufferClear(void);
void BrScreenClear(int r, int g, int b);
void BrModeSet(int mode);
int func_8021C878(unsigned char *dst, int max, unsigned char *src, int n, int level);
int func_8021CB4C(unsigned char *dst, int max, unsigned char *src, int level);
void func_80246F90(int x, int y, int w, int h, int a, int b, int c, int r, int g, int bl);
void BrTextHighlightOff(void);
void BrTextAlignCentre(void);
void BrTextAlignLeft(void);
void BrTextSetColours(int a, int b, int c, int d, int e, int f);
void BrTextSetFont(int font);
void BrTextPrint(char *s, int x, int y);
void BrPadConsume(BrPadRec *pad, unsigned int bits);
void BrPadStickToButtons(BrPadRec *pad);
/* -- end declarations -- */

#define PAD (&D_8036A8E0[D_80271FA8])
#define PFS (&D_80369EC0[D_80271FA8])
#define COMPANY 0x3544          /* "5D" */
#define GAME 0x4E475245         /* "NGRE" */

/* WHAT IT DOES: One frame of the Controller Pak data screen (a game mode).
 * The first frame loads its artwork and picks the row from how it was
 * entered (the menu, or straight to saving the season or the ghost).  Then a
 * state machine: wait for the fade in; take the row choice (up/down, A to go,
 * B to leave); fade the sound; probe the pak (a Rumble Pak in the way asks
 * to swap it; a new pak's id is remembered, a changed one is announced); find
 * the file, or on a save allocate it (confirming an overwrite); read or write
 * it -- the season, the ghost (compressed) or the options and the player's
 * car settings; show the result or the error and wait for a button; offer to
 * put the Rumble Pak back.  When it has faded out it starts the loaded
 * championship or time attack, or returns to where it came from. */
int D_802724F0 = 0;                     /* the last Controller Pak status */
unsigned char D_802724F4 = 0;           /* 0 the load/save menu, 1 save the season, 2 save the ghost */
unsigned char D_802724F8 = 0;           /* the screen has been set up */
unsigned char D_802724FC = 0;           /* the Rumble Pak was taken out for this */
unsigned char *D_80272500 = 0;          /* the transfer buffer */

/* @implements 0x80211D70 tgr BrLoadSaveScreen */
void BrLoadSaveScreen(void)
{
  /* declared in the order of the original's frame (first = highest slot) */
  int i;
  int mode;
  int h;
  int unusedD0;
  int unusedCC;
  int unusedC8;
  int unusedC4;
  int unusedC0;
  int y;
  int unusedB8;
  unsigned char *name;
  int freeBytes;
  unsigned char pakBits;
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
  char *labels[6] = {
    "LOAD SEASON DATA", "SAVE SEASON DATA", "LOAD GHOST CAR DATA", "SAVE GHOST CAR DATA",
    "LOAD OPTIONS", "SAVE OPTIONS"
  };
  int newSize;
  static unsigned char D_80272550 = 0;  /* the row: load/save season, ghost, options */
  static unsigned char D_80272554 = 1;  /* the screen's state */
  static unsigned char D_80316422;      /* data was loaded */
  static unsigned char D_80316423;      /* the menu is live (not behind a message) */
  static int D_80316424;                /* the open file */
  static unsigned char D_80316428;      /* frames the fade has been done */

  D_802A4BE8 = 0;
  if (D_802724F8 == 0) {
    D_802724F8 = 1;
    D_80316422 = 0;
    D_80316423 = 1;
    D_802724F0 = 0;
    D_80316428 = 0;
    BrIfaceMemReset();
    osViBlack(1);
    BrFrameBeginLayout1();
    BrFrameEnd();
    osViBlack(1);
    BrFrameBeginLayout1();
    BrFrameEnd();
    BrAllocPaintShopGfxMem(&D_8028CB40);
    BrAllocPaintShopGfxMem(&D_8028CB70);
    BrAllocPaintShopGfxMem(&D_8028D0B0);
    BrAllocPaintShopGfxMem(&D_8028D0E0);
    D_80272500 = (unsigned char *)BrIfaceMemAlloc(0x3A00);
    if (D_802724F4 == 0) {
      D_80272550 = 0;
    } else if ((mode = D_802724F4) == 1) {
      D_80272550 = 1;
    } else if (mode == 2) {
      D_80272550 = 3;
    }
    D_80272554 = 0;
    BrFadeTo(1.0f, 0.2f);
  }
  BrClockTick();
  BrFadeStep();
  BrFrameBeginLayout1();
  BrMenuCameraSet((float)D_8028AAB0, (float)D_8028AAB4);
  BrViewportSet(0, 0, D_8028AAB0, D_8028AAB4, 1);
  BrZBufferClear();
  BrScreenClear(0, 0, 0);
  BrImageDrawTinted(&D_8028CB40, 0x60, 0, 0x80);
  BrImageDrawAt(&D_8028CB70, D_8028CB70.x, D_8028CB70.y + 8);
  switch (D_80272550) {
  case 0:
  case 1:
    name = nameSeason;
    size = 0x200;
    break;
  case 2:
  case 3:
    name = nameGhost;
    size = 0x3A00;
    break;
  case 4:
  case 5:
    name = nameOptions;
    size = 0x100;
    break;
  }
  BrTextHighlightOff();
  BrTextAlignCentre();
  BrTextSetFont(30);
  if (D_802724F4 == 0) {
    BrTextPrint("%ryDATA LOAD/SAVE", 160, 41);
  } else {
    BrTextPrint("%ryDATA SAVE", 160, 41);
  }
  if (D_802724F4 == 0) {
    BrTextSetFont(16);
    for (i = 0; i != 6; i++) {
      if (D_80316423 != 0) {
        if (i == D_80272550) {
          BrTextSetColours(0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0);
        } else {
          BrTextSetColours(0xFF, 0xF5, 0, 200, 0, 0);
        }
      } else if (i == D_80272550) {
        BrTextSetColours(0x80, 0x80, 0x80, 0x84, 0x7C, 0);
      } else {
        BrTextSetColours(0x80, 0x78, 0, 100, 0, 0);
      }
      BrTextPrint(labels[i], 158, 87 + i * 18);
    }
    BrImageDrawAt(&D_8028D0B0, 202, 432);
    BrImageDrawAt(&D_8028D0E0, 322, 432);
    BrTextAlignLeft();
    BrTextSetFont(11);
    BrTextPrint("%wwSelect", 116, D_8028AAB4 * 19 / 20 - 3);
    BrTextPrint("%wwGo Back", 176, D_8028AAB4 * 19 / 20 - 3);
  } else {
    BrTextSetFont(20);
    if (D_80316423 != 0) {
      BrTextSetColours(0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0);
    } else {
      BrTextSetColours(0x80, 0x80, 0x80, 0x84, 0x7C, 0);
    }
    if ((mode = D_802724F4) == 1) {
      BrTextPrint("SAVE SEASON DATA?", 158, 128);
      D_80272550 = 1;
    } else if (mode == 2) {
      BrTextPrint("SAVE GHOST CAR DATA?", 158, 128);
      D_80272550 = 3;
    }
    BrImageDrawAt(&D_8028D0B0, 236, 432);
    BrImageDrawAt(&D_8028D0E0, 340, 432);
    BrTextAlignLeft();
    BrTextSetFont(11);
    BrTextPrint("%wwYES", 132, D_8028AAB4 * 19 / 20 - 3);
    BrTextPrint("%wwNO", 185, D_8028AAB4 * 19 / 20 - 3);
  }

  switch (D_80272554) {
  case 0:                       /* fading in */
    if (BrFadeInDone() != 0) {
      D_80272554 = 1;
    }
    break;
  case 1:                       /* the menu */
    D_80316423 = 1;
    if (D_802724F4 == 0) {
      BrPadStickToButtons(PAD);
      if (PAD->pressed & 2) {
        BrPadConsume(PAD, 2);
        if (D_80272550 == 5) {
          D_80272550 = 0;
        } else {
          D_80272550++;
        }
      } else if (PAD->pressed & 8) {
        BrPadConsume(PAD, 8);
        if (D_80272550 == 0) {
          D_80272550 = 5;
        } else {
          D_80272550--;
        }
      }
    }
    if (PAD->pressed & 0x10) {
      BrPadConsume(PAD, 0x10);
      D_80272554 = 2;
    } else if (PAD->pressed & 0x20) {
      BrPadConsume(PAD, 0x20);
      D_80272554 = 15;
    }
    break;
  case 2:
    BrSfxFadeTo(0.0f, 0.2f);
    BrMusicFadeTo(0.0f, 0.2f);
    D_80272554 = 3;
    break;
  case 3:                       /* wait for the sound to go */
    if (BrSfxFadeDone() != 0 && D_80316428++ == 4) {
      D_80272554 = 4;
      D_80316428 = 0;
    }
    break;
  case 4:                       /* probe the pak */
    osPfsIsPlug(D_80272D48, &pakBits);
    if ((pakBits & (1 << D_80271FA8)) == 0) {
      D_802724F0 = 1;
      D_80272554 = 12;
    } else {
      osSyncPrintf("\nInitializing controller pak...\n");
      D_802724F0 = osPfsInitPak(D_80272D48, PFS, D_80271FA8);
      if (D_802724F0 != 0) {
        if (D_802724F0 == 10) {
          if (osMotorInit(D_80272D48, &D_8031A3F8[D_80271FA8], D_80271FA8) == 0) {
            osMotorStop(&D_8031A3F8[D_80271FA8]);
            D_802724F0 = 9999;
            D_802724FC = 1;
            D_80272554 = 12;
          } else {
            D_802724F0 = 0;
            D_80272554 = 5;
          }
        } else {
          D_8031B1E8[D_80271FA8] = 0;
          D_80272554 = 12;
        }
      } else if (D_802724F0 == 0) {
        D_8031B1E8[D_80271FA8] = 0;
        if (D_80316420[D_80271FA8] == 0) {
          D_80316420[D_80271FA8] = 1;
          memcpy(D_803163E0[D_80271FA8], D_80369EC0[D_80271FA8].id, 32);
          D_80272554 = 7;
        } else if (bcmp(D_803163E0[D_80271FA8], D_80369EC0[D_80271FA8].id, 32) != 0) {
          memcpy(D_803163E0[D_80271FA8], D_80369EC0[D_80271FA8].id, 32);
          D_80272554 = 6;
        } else {
          D_80272554 = 7;
        }
      }
    }
    break;
  case 5:
    D_802724F0 = osPfsRepairId(PFS);
    if (D_802724F0 != 0) {
      D_80272554 = 12;
    } else {
      D_80272554 = 4;
    }
    break;
  case 6:                       /* a different pak */
    D_80316423 = 0;
    BrTextHighlightOff();
    BrTextAlignLeft();
    BrTextSetFont(12);
    BrTextSetColours(0xFF, 0xFF, 0xFF, 0xFF, 0xF5, 0);
    func_80246F90(0xAF, 0xB9, 0x122, 0x89, 3, 0, 0, 0x80, 0x80, 0x80);
    BrTextPrint("A NEW CONTROLLER PAK", 95, 108);
    BrTextPrint("WAS INSERTED.", 95, 121);
    BrTextPrint("THIS CONTROLLER PAK", 95, 140);
    BrTextPrint("WILL BE USED.", 95, 153);
    if (PAD->pressed & 0x8030) {
      BrPadConsume(PAD, 0x8030);
      D_80272554 = 7;
    }
    break;
  case 7:                       /* find the file */
    D_802724F0 = osPfsFindFile(PFS, COMPANY, GAME, name, ext, &D_80316424);
    if (D_802724F0 == 3) {
      osSyncPrintf("File system is corrupted, attempting repair...\n");
      osPfsChecker(PFS);
      D_802724F0 = osPfsFindFile(PFS, COMPANY, GAME, name, ext, &D_80316424);
    }
    if (D_802724F0 == 0) {
      switch (D_80272550) {
      case 0:
      case 2:
      case 4:
        D_80272554 = 8;
        break;
      case 1:
      case 3:
        D_80272554 = 10;
        break;
      case 5:
        D_80272554 = 11;
        break;
      default:
        D_80272554 = 1;
        break;
      }
    } else if (D_802724F0 == 5) {
      if (D_80272550 == 0 || D_80272550 == 2 || D_80272550 == 4) {
        D_80272554 = 12;
      } else {
        D_80272554 = 9;
      }
    } else {
      D_80272554 = 12;
    }
    break;
  case 8:                       /* load */
    if (D_802724F0 == 0) {
      if (D_80272550 == 0) {
        osSyncPrintf("Loading season data...\n");
      } else if (D_80272550 == 2) {
        osSyncPrintf("Loading compressed ghost data...\n");
      } else if (D_80272550 == 4) {
        osSyncPrintf("Loading Options...\n");
      }
      if (D_80272550 != 4) {
        D_802724F0 = osPfsReadWriteFile(PFS, D_80316424, 0, 0, size, D_80272500);
      } else {
        D_802724F0 = osPfsReadWriteFile(PFS, D_80316424, 0, 0, 0x80, D_80272500);
      }
      if (D_802724F0 != 0) {
        D_80272554 = 12;
      } else {
        if (D_80272550 == 0) {
          if (D_802724F4 == 0) {
            opts[0] = D_8031B760[D_80271FA8 ^ 1].season->xd4;
            opts[1] = D_8031B760[D_80271FA8 ^ 1].season->xd8;
            opts[2] = D_8031B760[D_80271FA8 ^ 1].season->xdc;
            opts[3] = D_8031B760[D_80271FA8 ^ 1].season->xe0;
            opts[4] = D_8031B760[D_80271FA8 ^ 1].season->xe4;
          }
          memcpy(D_8031B760[0].season, D_80272500, 0x128);
          memcpy(D_8031B760[1].season, D_80272500, 0x128);
          if (D_802724F4 == 0) {
            D_8031B760[D_80271FA8 ^ 1].season->xd4 = opts[0];
            D_8031B760[D_80271FA8 ^ 1].season->xd8 = opts[1];
            D_8031B760[D_80271FA8 ^ 1].season->xdc = opts[2];
            D_8031B760[D_80271FA8 ^ 1].season->xe0 = opts[3];
            D_8031B760[D_80271FA8 ^ 1].season->xe4 = opts[4];
          }
          osSyncPrintf("Done!\n");
          if (D_802724FC == 0) {
            D_80272554 = 15;
          } else {
            D_80272554 = 13;
          }
          D_80316422 = 1;
        } else if (D_80272550 == 2) {
          osSyncPrintf("Extracting ghost data...\n");
          D_80270784 = func_8021CB4C(D_80307F00, 0xDE5C, D_80272500, 2);
          D_8028B940 = D_80307F00[0];
          D_8031B760[0].season->xce |= 1 << D_80307F00[0];
          D_8031B760[0].season->unlocked |= 1 << D_80307F00[1];
          osSyncPrintf("Done!\n");
          if (D_802724FC == 0) {
            D_80272554 = 15;
          } else {
            D_80272554 = 13;
          }
          D_80316422 = 1;
        } else if (D_80272550 == 4) {
          memcpy(D_802723D0, D_80272500, 12);
          D_802724F0 = osPfsReadWriteFile(PFS, D_80316424, 0, 0x80, 0x80, D_80272500);
          if (D_802724F0 == 0) {
            memcpy(opts, D_80272500, 20);
            D_8031B760[D_80271FA8].season->xd4 = opts[0];
            D_8031B760[D_80271FA8].season->xd8 = opts[1];
            D_8031B760[D_80271FA8].season->xdc = opts[2];
            D_8031B760[D_80271FA8].season->xe0 = opts[3];
            D_8031B760[D_80271FA8].season->xe4 = opts[4];
            osSyncPrintf("Done!\n");
            BrVolumesApply();
            D_80272554 = 13;
          } else {
            D_80272554 = 12;
          }
        }
      }
    }
    break;
  case 9:                       /* make room for a new file */
    D_802724F0 = osPfsFreeBlocks(PFS, &freeBytes);
    if (D_802724F0 == 0) {
      if (freeBytes < size) {
        D_802724F0 = 7;
        D_80272554 = 12;
      } else {
        if (D_80272550 == 1) {
          osSyncPrintf("Allocating %d bytes for season data...\n", size);
        } else if (D_80272550 == 3) {
          osSyncPrintf("Allocating %d bytes for ghost data...\n", size);
        } else if (D_80272550 == 5) {
          osSyncPrintf("Allocating %d bytes for options...\n", size);
        }
        D_802724F0 = osPfsAllocateFile(PFS, COMPANY, GAME, name, ext, size, &D_80316424);
        if (D_802724F0 == 3) {
          osPfsChecker(PFS);
          D_802724F0 = osPfsAllocateFile(PFS, COMPANY, GAME, name, ext, size, &D_80316424);
        }
        if (D_802724F0 == 0) {
          D_80272554 = 11;
        } else {
          D_80272554 = 12;
        }
      }
    } else {
      D_80272554 = 12;
    }
    break;
  case 10:                      /* overwrite? */
    D_80316423 = 0;
    h = 0xBF;
    if (D_802724F4 == 0) {
      h = 0xC9;
    }
    y = h - D_8028D0B0.w + 0x62;
    func_80246F90(0xD5, h, 0xD6, 0x6A, 3, 0, 0, 0x80, 0x80, 0x80);
    BrTextAlignLeft();
    BrTextHighlightOff();
    BrTextSetFont(13);
    BrTextSetColours(0xFF, 0xFF, 0xFF, 0xFF, 0xF5, 0);
    BrTextPrint("OK TO OVERWRITE", 114, (h + 30) >> 1);
    BrTextPrint("SAVED DATA?", 114, ((h + 30) >> 1) + 14);
    BrTextSetFont(10);
    BrTextPrint("%wwOK", (D_8028D0B0.w + 0xEDU) >> 1, (y + 18) >> 1);
    BrTextPrint("%wwCANCEL", (D_8028D0E0.h + 0x13BU) >> 1, (y + 18) >> 1);
    BrImageDrawAt(&D_8028D0B0, 0xE7, y);
    BrImageDrawAt(&D_8028D0E0, 0x135, y);
    if (PAD->pressed & 0x10) {
      BrPadConsume(PAD, 0x10);
      D_80272554 = 11;
    } else if (PAD->pressed & 0x20) {
      BrPadConsume(PAD, 0x20);
      if (D_802724F4 == 0) {
        BrSfxFadeTo(1.0f, 0.2f);
        BrMusicFadeTo(1.0f, 0.2f);
        D_80272554 = 1;
      } else {
        D_80272554 = 15;
      }
    }
    break;
  case 11:                      /* save */
    if (D_80272550 == 1) {
      osSyncPrintf("Saving season data...\n");
      memcpy(D_80272500, D_8031B760[0].season, 0x128);
      D_802724F0 = osPfsReadWriteFile(PFS, D_80316424, 1, 0, 0x200, D_80272500);
      if (D_802724F0 == 0) {
        osSyncPrintf("Done!\n");
        D_80272554 = 13;
      } else {
        D_80272554 = 12;
      }
    } else if (D_80272550 == 3) {
      osSyncPrintf("Compressing ghost data...\n");
      newSize = func_8021C878(D_80272500, 0x3A00, D_80307F00, D_80270784, 2);
      osSyncPrintf("new size = %d\n", newSize);
      if (newSize != -1) {
        osSyncPrintf("Saving ghost data to controller pak...\n");
        D_802724F0 = osPfsReadWriteFile(PFS, D_80316424, 1, 0, 0x3A00, D_80272500);
        if (D_802724F0 == 0) {
          osSyncPrintf("Done!\n");
          D_80272554 = 13;
        } else {
          D_80272554 = 12;
        }
      } else {
        D_802724F0 = -99999;
        D_80272554 = 12;
      }
    } else if (D_80272550 == 5) {
      osSyncPrintf("Saving options...\n");
      memcpy(D_80272500, D_802723D0, 12);
      D_802724F0 = osPfsReadWriteFile(PFS, D_80316424, 1, 0, 0x80, D_80272500);
      if (D_802724F0 != 0) {
        D_80272554 = 12;
      } else {
        opts[0] = D_8031B760[D_80271FA8].season->xd4;
        opts[1] = D_8031B760[D_80271FA8].season->xd8;
        opts[2] = D_8031B760[D_80271FA8].season->xdc;
        opts[3] = D_8031B760[D_80271FA8].season->xe0;
        opts[4] = D_8031B760[D_80271FA8].season->xe4;
        memcpy(D_80272500, opts, 20);
        D_802724F0 = osPfsReadWriteFile(PFS, D_80316424, 1, 0x80, 0x80, D_80272500);
        if (D_802724F0 == 0) {
          osSyncPrintf("Done!\n");
          D_80272554 = 13;
        } else {
          D_80272554 = 12;
        }
      }
    }
    break;
  case 12:                      /* an error, or the Rumble Pak is in the way */
    D_80316423 = 0;
    BrTextHighlightOff();
    BrTextAlignLeft();
    BrTextSetFont(12);
    BrTextSetColours(0xFF, 0xFF, 0xFF, 0xFF, 0xF5, 0);
    switch (D_802724F0) {
    case 10:
      func_80246F90(0xBD, 0xD9, 0x106, 0x4A, 3, 0, 0, 0x80, 0x80, 0x80);
      BrTextPrint("THE CONTROLLER PAK", 102, 124);
      BrTextPrint("IS NONFUNCTIONAL.", 102, 138);
      break;
    case 1:
    case 11:
      func_80246F90(0x94, 0xD8, 0x158, 0x4C, 3, 0, 0, 0x80, 0x80, 0x80);
      BrTextPrint("CONTROLLER PAK NOT FOUND.", 82, 124);
      if ((unusedD0 = D_80272550) == 0 || D_80272550 == 2 || D_80272550 == 4) {
        BrTextPrint("DATA CANNOT BE LOADED.", 82, 138);
      } else if (unusedD0 == 1 || unusedD0 == 3 || unusedD0 == 5) {
        BrTextPrint("DATA CANNOT BE SAVED.", 82, 138);
      }
      break;
    case 9999:
      y = 0x146 - D_8028D0B0.w;
      func_80246F90(0x79, 0xAE, 0x18D, 0xA0, 3, 0, 0, 0x80, 0x80, 0x80);
      BrTextPrint("PLEASE REMOVE THE RUMBLE PAK", 68, 103);
      BrTextPrint("AND INSERT THE CONTROLLER PAK", 68, 116);
      BrTextPrint("INTO THE CONTROLLER.  PRESS", 68, 129);
      BrTextPrint("THE A BUTTON WHEN READY.", 68, 142);
      BrTextAlignLeft();
      BrTextSetFont(10);
      BrTextPrint("%wwOK", (D_8028D0B0.w + 0xE3U) >> 1, (y + 18) >> 1);
      BrTextPrint("%wwCANCEL", (D_8028D0E0.w + 0x144U) >> 1, (y + 18) >> 1);
      BrImageDrawAt(&D_8028D0B0, 0xDD, y);
      BrImageDrawAt(&D_8028D0E0, 0x13E, y);
      break;
    case 7:
    case 8:
      func_80246F90(0xA5, 0x9F, 0x136, 0xBD, 3, 0, 0, 0x80, 0x80, 0x80);
      if (D_80272550 == 1) {
        BrTextPrint("INSUFFICIENT FREE PAGES", 90, 95);
        BrTextPrint("OR FREE NOTES IN THE", 90, 108);
        BrTextPrint("CONTROLLER PAK.", 90, 121);
        BrTextPrint("TWO PAGES AND ONE NOTE", 90, 140);
        BrTextPrint("ARE NEEDED TO SAVE THE", 90, 153);
        BrTextPrint("SEASON DATA.", 90, 166);
      } else if (D_80272550 == 3) {
        BrTextPrint("INSUFFICIENT FREE PAGES", 90, 95);
        BrTextPrint("OR FREE NOTES IN THE", 90, 108);
        BrTextPrint("CONTROLLER PAK.", 90, 121);
        BrTextPrint("58 PAGES AND ONE NOTE", 90, 140);
        BrTextPrint("ARE NEEDED TO SAVE THE", 90, 153);
        BrTextPrint("GHOST CAR DATA.", 90, 166);
      } else if (D_80272550 == 5) {
        BrTextPrint("INSUFFICIENT FREE PAGES", 90, 95);
        BrTextPrint("OR FREE NOTES IN THE", 90, 108);
        BrTextPrint("CONTROLLER PAK.", 90, 121);
        BrTextPrint("ONE PAGE AND ONE NOTE", 90, 140);
        BrTextPrint("ARE NEEDED TO SAVE THE", 90, 153);
        BrTextPrint("OPTIONS.", 90, 166);
      }
      break;
    case -99999:
      func_80246F90(0x75, 0xBD, 0x196, 0x82, 3, 0, 0, 0x80, 0x80, 0x80);
      BrTextPrint("UNABLE TO SAVE GHOST CAR DATA.", 66, 110);
      BrTextPrint("GHOST CAR DATA IS LARGER THAN", 66, 124);
      BrTextPrint("THE MAXIMUM ALLOCATED SPACE", 66, 138);
      BrTextPrint("IN THE CONTROLLER PAK.", 66, 152);
      break;
    case 4:
      func_80246F90(0xBF, 0xD9, 0x102, 0x4A, 3, 0, 0, 0x80, 0x80, 0x80);
      BrTextPrint("CONTROLLER ERROR", 103, 124);
      BrTextPrint("HAS BEEN DETECTED!", 103, 138);
      break;
    case 5:
      if ((unusedCC = D_80272550) == 0 || D_80272550 == 2 || D_80272550 == 4) {
        func_80246F90(0xAD, 0xD9, 0x125, 0x4A, 3, 0, 0, 0x80, 0x80, 0x80);
        BrTextPrint("SAVED DATA NOT FOUND", 94, 124);
        BrTextPrint("IN CONTROLLER PAK.", 94, 138);
      } else if (unusedCC == 1 || unusedCC == 3 || unusedCC == 5) {
        func_80246F90(0xB6, 0xD9, 0x114, 0x4A, 3, 0, 0, 0x80, 0x80, 0x80);
        BrTextPrint("UNABLE TO SAVE DATA", 99, 124);
        BrTextPrint("TO CONTROLLER PAK.", 99, 138);
      }
      break;
    case 2:
      func_80246F90(0xAF, 0xBB, 0x122, 0x89, 3, 0, 0, 0x80, 0x80, 0x80);
      BrTextPrint("A NEW CONTROLLER PAK", 95, 109);
      BrTextPrint("WAS INSERTED.", 95, 122);
      BrTextPrint("THIS CONTROLLER PAK", 95, 141);
      BrTextPrint("WILL BE USED.", 95, 154);
      break;
    default:
      func_80246F90(0xB1, 0xD9, 0x11E, 0x4A, 3, 0, 0, 0x80, 0x80, 0x80);
      BrTextPrint("CONTROLLER PAK ERROR", 96, 124);
      BrTextPrint("HAS BEEN DETECTED!", 96, 138);
      break;
    }
    if (D_802724F0 != 9999) {
      if (PAD->pressed & 0x8030) {
        BrPadConsume(PAD, 0x8030);
        D_802724F0 = 0;
        if (D_802724F0 == 2) {
          D_80272554 = 4;
        } else {
          D_80272554 = 1;
        }
      }
    } else if (PAD->pressed & 0x10) {
      BrPadConsume(PAD, 0x10);
      D_802724F0 = 0;
      D_80272554 = 4;
    } else if (PAD->pressed & 0x20) {
      BrPadConsume(PAD, 0x20);
      D_802724F0 = 0;
      BrSfxFadeTo(1.0f, 0.2f);
      BrMusicFadeTo(1.0f, 0.2f);
      D_80272554 = 1;
    }
    break;
  case 13:                      /* done */
    D_80316423 = 0;
    BrTextAlignCentre();
    BrTextHighlightOff();
    BrTextSetFont(12);
    switch (D_80272550) {
    case 0:
      if (D_802724F4 == 0) {
        h = 0xD9;
      } else {
        h = 0xCE;
      }
      func_80246F90(0xE2, h, 0xBC, 0x4A, 3, 0, 0, 0x80, 0x80, 0x80);
      BrTextPrint("%ywSEASON DATA", 159, (h + 32) >> 1);
      BrTextPrint("%ywLOADED OK!", 159, ((h + 32) >> 1) + 14);
      break;
    case 2:
      if (D_802724F4 == 0) {
        h = 0xD9;
      } else {
        h = 0xCE;
      }
      func_80246F90(0xE2, h, 0xBC, 0x4A, 3, 0, 0, 0x80, 0x80, 0x80);
      BrTextPrint("%ywGHOST DATA", 159, (h + 32) >> 1);
      BrTextPrint("%ywLOADED OK!", 159, ((h + 32) >> 1) + 14);
      break;
    case 1:
      if (D_802724F4 == 0) {
        h = 0xD9;
      } else {
        h = 0xCE;
      }
      func_80246F90(0xE2, h, 0xBC, 0x4A, 3, 0, 0, 0x80, 0x80, 0x80);
      BrTextPrint("%ywSEASON DATA", 159, (h + 32) >> 1);
      BrTextPrint("%ywSAVED OK!", 159, ((h + 32) >> 1) + 14);
      break;
    case 3:
      if (D_802724F4 == 0) {
        h = 0xD9;
      } else {
        h = 0xCE;
      }
      func_80246F90(0xD8, h, 0xD0, 0x4A, 3, 0, 0, 0x80, 0x80, 0x80);
      BrTextPrint("%ywGHOST CAR DATA", 159, (h + 32) >> 1);
      BrTextPrint("%ywSAVED OK!", 159, ((h + 32) >> 1) + 14);
      break;
    case 4:
      func_80246F90(0xC3, 0xE7, 0xFA, 0x2E, 3, 0, 0, 0x80, 0x80, 0x80);
      BrTextPrint("%ywOPTIONS LOADED OK!", 159, 131);
      break;
    case 5:
      func_80246F90(0xC3, 0xE7, 0xFA, 0x2E, 3, 0, 0, 0x80, 0x80, 0x80);
      BrTextPrint("%ywOPTIONS SAVED OK!", 159, 131);
      break;
    default:
      D_80272554 = 1;
      break;
    }
    if (PAD->pressed & 0x8030) {
      BrPadConsume(PAD, 0x8030);
      if (D_802724F4 == 0) {
        if (D_802724FC != 0) {
          D_80272554 = 14;
        } else {
          BrSfxFadeTo(1.0f, 0.2f);
          BrMusicFadeTo(1.0f, 0.2f);
          D_80272554 = 1;
        }
      } else if (D_802724FC != 0) {
        D_80272554 = 14;
      } else {
        D_80272554 = 15;
      }
    }
    break;
  case 14:                      /* put the Rumble Pak back */
    D_80316423 = 0;
    BrTextHighlightOff();
    BrTextAlignLeft();
    BrTextSetFont(12);
    BrTextSetColours(0xFF, 0xFF, 0xFF, 0xFF, 0xF5, 0);
    func_80246F90(0x72, 0xB2, 0x19C, 0x98, 3, 0, 0, 0x80, 0x80, 0x80);
    BrTextPrint("IF THE RUMBLE PAK IS TO BE USED,", 65, 105);
    BrTextPrint("REMOVE THE CONTROLLER PAK AND", 65, 118);
    BrTextPrint("INSERT THE RUMBLE PAK INTO THE", 65, 131);
    BrTextPrint("CONTROLLER.  PRESS THE A BUTTON", 65, 144);
    BrTextPrint("TO CONTINUE.", 65, 157);
    if (PAD->pressed & 0x10) {
      BrPadConsume(PAD, 0x10);
      if (D_802724F4 == 0) {
        if (D_80272550 == 0 || D_80272550 == 2) {
          D_80272554 = 15;
        } else {
          BrSfxFadeTo(1.0f, 0.2f);
          BrMusicFadeTo(1.0f, 0.2f);
          D_80272554 = 1;
        }
      } else {
        D_80272554 = 15;
      }
      D_802724FC = 0;
      if (osMotorInit(D_80272D48, &D_8031A3F8[D_80271FA8], D_80271FA8) == 0) {
        D_8031B1E8[D_80271FA8] = 1;
      }
    }
    break;
  case 15:
    BrFadeTo(0.0f, 0.2f);
    D_80272554 = 16;
    break;
  case 16:
    BrSfxFadeTo(1.0f, 0.2f);
    BrMusicFadeTo(1.0f, 0.2f);
    break;
  }
  {
    Gfx *g = D_8028A858++;
    g->words.w1 = 0;
    g->words.w0 = 0xE7000000;
  }
  BrFadeBarsDraw();
  BrFrameEnd();
  if (BrFadeOutDone() != 0) {
    D_802724F8 = 0;
    BrFrameBeginLayout1();
    BrScreenClear(0, 0, 0);
    BrFrameEnd();
    BrFrameBeginLayout1();
    BrScreenClear(0, 0, 0);
    BrFrameEnd();
    D_802724F0 = 0;
    if (D_80272550 == 0 && D_80316422 != 0) {
      D_80316422 = 0;
      D_802A4BE8 = 1;
      BrModeSet((int)BrChampionshipStart);
    } else if (D_80272550 == 2 && D_80316422 != 0) {
      D_80316422 = 0;
      D_802A4BE8 = 1;
      BrModeSet((int)BrTimeAttackStart);
    } else {
      D_80272550 = 0;
      if (D_802724F4 == 0) {
        D_802A4BE8 = 1;
        BrModeSet((int)BrMainMenu);
      } else {
        mode = D_802724F4;
        if (D_802724F4 == 1) {
          D_802A4BE8 = 1;
          BrModeSet((int)func_80209434);
        } else if ((mode | 0) == 2) {
          D_802A4BE8 = 1;
          BrModeSet((int)BrMainMenu);
        }
      }
    }
  }
  D_802A4BE8 = 0;
}
