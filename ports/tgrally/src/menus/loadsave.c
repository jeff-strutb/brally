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
#include "tgr/image.h"
extern BrImage D_8028CB40;      /* the background */
extern BrImage D_8028CB70;      /* the panel */
extern BrImage D_8028D0B0;      /* the A button */
extern BrImage D_8028D0E0;      /* the B button */

extern TgrPak D_80369EC0[4];     /* one per port */
extern TgrPak D_8031A3F8[4];     /* the Rumble Pak's, one per port */
extern unsigned char D_803163E0[2][32];  /* the id of the pak last used on each port */
extern unsigned char D_80316420[2];          /* a pak has been used on the port (ports 2-3 run on into the statics) */
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
void BrTrackSelectScreen(void);
void BrFrameBeginLayout1(void);
void BrFrameEnd(void);
void BrViewportSet(int x, int y, int w, int h, int scissor);
void BrZBufferClear(void);
void BrScreenClear(int r, int g, int b);
void BrModeSet(void (*mode)(void));
int BrRleEncode(unsigned char *dst, int max, unsigned char *src, int n, int level);
int BrRlePlanesUnpack(unsigned char *dst, int max, unsigned char *src, int level);
void BrBevelPanel(int, int, int, int, int, char, char, unsigned char, unsigned char, unsigned char);
void BrTextHighlightOff(void);
void BrTextAlignCentre(void);
void BrTextAlignLeft(void);
void BrTextSetColours(int a, int b, int c, int d, int e, int f);
void BrTextSetFont(int font);
void BrTextPrint(char *s, int x, int y);
void BrPadConsume(BrPadRec *pad, unsigned int bits);
void BrPadStickToButtons(int);
/* -- end declarations -- */

#define PAD (&D_8036A8E0[D_80271FA8])
#define PFS (&D_80369EC0[D_80271FA8])
#if TGR_REGION_JPN
#define COMPANY 0x3238          /* "28" */
#define GAME 0x4E54524A         /* "NTRJ" */
#elif TGR_REGION_EUR
#define COMPANY 0x3746          /* "7F" */
#define GAME 0x4E545250         /* "NTRP" */
#else
#define COMPANY 0x3544          /* "5D" */
#define GAME 0x4E475245         /* "NGRE" */
#endif

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
extern int D_802724F0;                     /* the last Controller Pak status */
extern unsigned char D_802724F4;           /* 0 the load/save menu, 1 save the season, 2 save the ghost */
extern unsigned char D_802724F8;           /* the screen has been set up */
extern unsigned char D_802724FC;           /* the Rumble Pak was taken out for this */
extern unsigned char *D_80272500;          /* the transfer buffer */

#if TGR_REGION_EUR
#define DBG(args) tgr_print args   /* Europe keeps the debug output */
#else
#define DBG(args)
#endif
#if TGR_REGION_JPN || TGR_REGION_EUR
typedef struct BrDataItem {     /* one row of the data screen's menu */
  unsigned char kind;           /* 0-5 Controller Pak rows (as the USA's), 6-9 the Game Pak's
                                   EEPROM (load/save season, load/save options), 10 erase,
                                   11 leave; 13 a heading, 14 and 15 a half and a whole gap */
  char *label;                  /* 0 ends the menu */
} BrDataItem;
typedef struct BrEepSave {      /* the EEPROM block after its checksum word */
  unsigned char season[0x128];  /* player 1's season record */
  int opts[5];                  /* player 2's per-season words */
  unsigned char cfg[12];        /* the options */
} BrEepSave;
void BrCarReset(BrCar *car);
extern int D_8028A884;          /* a screen flush holds the picture blank */
#if TGR_REGION_JPN
void BrScreenBlank(void);
#else
void BrVideoBlank(void);
#define BrScreenBlank BrVideoBlank
#endif
#endif

#if TGR_REGION_JPN || TGR_REGION_EUR
/* WHAT IT DOES: One frame of the data screen (a game mode).  The first frame
 * loads its artwork and picks the menu from how it was entered: the whole
 * menu, or a save-the-season or save-the-ghost menu.  Japan's menu is a
 * table of rows: the Game Pak's EEPROM (load or save the season or the
 * options, erase both) and the Controller Pak's six rows, with headings and
 * gaps the cursor skips.  Then a state machine: wait for the fade in; take
 * the row (up/down, A to go, B to leave); fade the sound; for the EEPROM
 * read the block, check its sum and load, or confirm and save; for the
 * Controller Pak probe it, find or allocate the file and read or write it;
 * show the result or the error; offer to put the Rumble Pak back.  When it
 * has faded out it starts the loaded championship or time attack, or returns
 * to where it came from.
 * The menu pointer is set on the first frame only (a local the later frames
 * find in its stack slot, as the ROM has it).
 * The menu rows and the panels keep their y in two locals, so the panels'
 * y is short-lived and the shared (y + 18) >> 1 takes s0 as in the ROM.
 * The cursor-up search loops by label: with no landing block the row store
 * stays on its last test and the pad is read afresh after the arms.  The
 * two season records are written through p2 (data + 0x128) set beside
 * data, and the kind tests that follow a pak read go through mode.  Japan
 * dispatches on the item kind with switches where the USA tests in turn,
 * and its delete prompt's panel width is its own local (newSize), apart
 * from h. */
/* @implements 0x80211D70 tgr BrLoadSaveScreen */
void BrLoadSaveScreen(void)
{
  /* declared in the order of the original's frame (first = highest slot) */
  int i;
  int mode;
  int h;
  int newSize;                  /* Europe: the packed ghost's size; Japan: a panel's width */
  int unusedCC;
  int unusedC8;
  int unusedC4;
  int unusedC0;
  int y;
  int menuY;                    /* the menu rows' y; y is the panels' */
  unsigned char *name;
  int freeBytes;
  unsigned char pakBits;
  static BrDataItem *items;      /* port: set on the first frame; the ROM finds it in its stack slot after */
  int unusedE4;
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
  int u94;                      /* unused: the frame has it */
  int k;
  int *p2;
  unsigned char *data;
  int u84[2];                   /* unused: the frame has them */
  int len;
  int sum;
  int check;
  static BrDataItem jpn_D_80273F98[] = {
    { 13, "-- GAME PAK --" },
    { 6, "LOAD SEASON DATA" },
    { 7, "SAVE SEASON DATA" },
    { 8, "LOAD OPTIONS" },
    { 9, "SAVE OPTIONS" },
    { 14, "" },
    { 10, "ERASE DATA" },
    { 15, "" },
    { 13, "-- CONTROLLER PAK --" },
    { 0, "LOAD SEASON DATA" },
    { 1, "SAVE SEASON DATA" },
    { 2, "LOAD GHOST CAR DATA" },
    { 3, "SAVE GHOST CAR DATA" },
    { 4, "LOAD OPTIONS" },
    { 5, "SAVE OPTIONS" },
    { 0, 0 },
  };
  static BrDataItem jpn_D_80274018[] = {
#if TGR_REGION_EUR
    { 7, "SAVE SEASON DATA TO GAME PAK" },
    { 15, "" },
    { 1, "SAVE SEASON DATA TO CONTROLLER PAK" },
    { 15, "" },
    { 11, "CONTINUE" },
#else
    { 7, "%j\xB9(\xA6\x8D\x8F\xA1" "D\x92(\xAA\xCF\xBB(\x83w\xB2(\xAC" "B6R&" },
    { 15, "" },
    { 1, "%j\xC1\xCF\xC3\xC8(\x88\x8D\x8F\xA1" "D\x92(\xAA\xCF\xBB(\x83w\xB2(\xAC" "B6R&" },
    { 15, "" },
    { 11, "OK" },
#endif
    { 0, 0 },
  };
  static BrDataItem jpn_D_80274048[] = {
#if TGR_REGION_EUR
    { 3, "SAVE GHOST CAR DATA TO CONTROLLER PAK" },
    { 15, "" },
    { 11, "DONE" },
#else
    { 3, "%j\xC1\xCF\xC3\xC8(\x88\x8D\x8F\xA1" "D\xC9(\xA2\xC3\x81(\xBB(\x83w\xB2(\xAC" "B6R&" },
    { 15, "" },
    { 11, "OK" },
#endif
    { 0, 0 },
  };
  extern unsigned char D_80272550;  /* the row (initialised data: 0) */
  extern unsigned char D_80272554;  /* the screen's state (initialised data: 1) */
  extern unsigned char D_80316422;      /* data was loaded */
  extern unsigned char D_80316423;      /* the menu is live (not behind a message) */
  extern int D_80316424;                /* the open file */
#if TGR_REGION_JPN
  extern unsigned char jpn_D_80316D38;  /* the EEPROM save was confirmed over good data */
#else
  static unsigned char jpn_D_80316D38;  /* (Europe's has no name of its own: a function static) */
#endif
  extern unsigned char D_80316428;      /* frames the fade has been done */

  if (D_802724F8 == 0) {
    D_802724F8 = 1;
    D_80316422 = 0;
    D_80316423 = 1;
    D_802724F0 = 0;
    D_80316428 = 0;
    BrIfaceMemReset();
    D_8028A884 = 1;
    BrScreenBlank();
    BrFrameBeginLayout1();
    BrFrameEnd();
    BrScreenBlank();
    BrFrameBeginLayout1();
    BrFrameEnd();
    BrAllocPaintShopGfxMem(&D_8028CB40);
    BrAllocPaintShopGfxMem(&D_8028CB70);
    BrAllocPaintShopGfxMem(&D_8028D0B0);
    BrAllocPaintShopGfxMem(&D_8028D0E0);
    D_80272500 = TGR_PTR(unsigned char *, BrIfaceMemAlloc(0x3A00));
    jpn_D_80316D38 = 0;
    D_80272550 = 0;
    if (D_802724F4 == 0) {
      items = jpn_D_80273F98;
    } else if ((mode = D_802724F4) == 1) {
      items = jpn_D_80274018;
    } else if (mode == 2) {
      items = jpn_D_80274048;
    }
    if (items[0].kind == 13) {
      D_80272550 = 1;
    }
    D_80272554 = 0;
    BrFadeTo(1.0f, 0.2f);
    D_8028A884 = 0;
  }
  BrClockTick();
  D_802A4BE8 = 0;
  BrFadeStep();
  BrFrameBeginLayout1();
  BrMenuCameraSet((float)D_8028AAB0, (float)D_8028AAB4);
  BrViewportSet(0, 0, D_8028AAB0, D_8028AAB4, 1);
  BrZBufferClear();
  BrScreenClear(0, 0, 0);
  BrImageDrawTinted(&D_8028CB40, 0x60, 0, 0x80);
  BrImageDrawAt(&D_8028CB70, D_8028CB70.x, D_8028CB70.y + 8);
  switch (items[D_80272550].kind) {
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
  menuY = 0;
  BrTextSetFont(13);
  for (i = 0; items[i].label != 0; i++) {
    if (items[i].kind == 15) {
      menuY += 6;
    } else if (items[i].kind == 14) {
      menuY += 3;
    } else {
      menuY += 12;
    }
  }
  menuY = 0x8B - (menuY >> 1);
  for (i = 0; items[i].label != 0; i++) {
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
    BrTextPrint(items[i].label, 158, menuY);
    if (items[i].kind == 15) {
      menuY += 6;
    } else if (items[i].kind == 14) {
      menuY += 3;
    } else {
      menuY += 12;
    }
  }
  BrImageDrawAt(&D_8028D0B0, 222, 432);
  BrImageDrawAt(&D_8028D0E0, 342, 432);
  BrTextAlignLeft();
  BrTextSetFont(11);
  BrTextPrint("%wwSelect", 126, D_8028AAB4 * 19 / 20 - 3);
  BrTextPrint("%wwExit", 186, D_8028AAB4 * 19 / 20 - 3);

  switch (D_80272554) {
  case 0:                       /* fading in */
    if (BrFadeInDone() != 0) {
      D_80272554 = 1;
    }
    break;
  case 1:                       /* the menu: the cursor skips headings and gaps */
    D_80316423 = 1;
    BrPadStickToButtons(tgr_addr32(PAD));
    if (PAD->pressed & 2) {
      BrPadConsume(PAD, 2);
      do {
        D_80272550++;
        if (items[D_80272550].label == 0) {
          D_80272550 = 0;
        }
      } while (items[D_80272550].kind == 15 || items[D_80272550].kind == 14 ||
               items[D_80272550].kind == 13);
    } else if (PAD->pressed & 8) {
      BrPadConsume(PAD, 8);
    up:
      if (D_80272550 == 0) {
        while (items[D_80272550].label != 0) {
          D_80272550++;
        }
      }
      D_80272550--;
      if (items[D_80272550].kind == 15 || items[D_80272550].kind == 14 ||
          items[D_80272550].kind == 13) {
        goto up;
      }
    }
    if (PAD->pressed & 0x10) {
      BrPadConsume(PAD, 0x10);
      D_80272554 = 2;
    } else if (PAD->pressed & 0x20) {
      BrPadConsume(PAD, 0x20);
      D_80272554 = 18;
    }
    break;
  case 2:
    BrSfxFadeTo(0.0f, 0.2f);
    BrMusicFadeTo(0.0f, 0.2f);
    D_80272554 = 3;
    break;
  case 3:                       /* wait for the sound to go */
    if (BrSfxFadeDone() != 0 && D_80316428++ == 4) {
      D_80316428 = 0;
      if (items[D_80272550].kind < 6) {
        D_80272554 = 7;
      } else if (items[D_80272550].kind < 10) {
        D_80272554 = 4;
      } else if (items[D_80272550].kind == 10) {
        D_80272554 = 6;
      } else {
        D_80272554 = 18;
      }
    }
    break;
  case 4:                       /* the Game Pak's EEPROM: load, or save */
    D_802724FC = 0;
    if (jpn_D_80316D38 == 0 && tgr_eeprom_probe((TgrMsgQueue *)D_80272D48) == 0) {
      D_802724F0 = 1;
      D_80272554 = 15;
      break;
    }
    data = D_80272500 + 4;
    p2 = (int *)(data + 0x128);
    len = (data - D_80272500 + 0x14f) & ~7;
    if (jpn_D_80316D38 == 0) {
      if (tgr_eeprom_long_read((TgrMsgQueue *)D_80272D48, 0, D_80272500, len) != 0 &&
          ((mode = items[D_80272550].kind) == 6 || mode == 8)) {
        D_802724F0 = 5;
        D_80272554 = 15;
        break;
      }
      memcpy(&sum, D_80272500, 4);
      check = 0xBFED812;
      for (k = 4; k < len; k++) {
        check += D_80272500[k];
      }
      if (check != sum) {
#if TGR_REGION_EUR
        tgr_print("checksum fail (%08x vs %08x/%02x%02x%02x%02x) (%d)\n", check, sum, D_80272500[0],
                     D_80272500[1], D_80272500[2], D_80272500[3], len);
#endif
        if ((mode = items[D_80272550].kind) == 6 || mode == 8) {
          D_802724F0 = 5;
          D_80272554 = 15;
          break;
        }
        memcpy(data, TGR_PTR(BrSeason *, D_8031B760[0].season), 0x128);
        p2[0] = TGR_PTR(BrSeason *, D_8031B760[1].season)->xd4;
        p2[1] = TGR_PTR(BrSeason *, D_8031B760[1].season)->xd8;
        p2[2] = TGR_PTR(BrSeason *, D_8031B760[1].season)->xdc;
        p2[3] = TGR_PTR(BrSeason *, D_8031B760[1].season)->xe0;
        p2[4] = TGR_PTR(BrSeason *, D_8031B760[1].season)->xe4;
        memcpy(data + 0x13c, D_802723D0, 12);
      } else {
#if TGR_REGION_EUR
        tgr_print("checksum pass\n");
#endif
        if (items[D_80272550].kind == 7) {
          D_80272554 = 13;
          break;
        }
      }
    }
    jpn_D_80316D38 = 0;
    switch (items[D_80272550].kind) {
    case 6:
      memcpy(TGR_PTR(BrSeason *, D_8031B760[0].season), data, 0x128);
      memcpy(TGR_PTR(BrSeason *, D_8031B760[1].season), data, 0x128);
      TGR_PTR(BrSeason *, D_8031B760[1].season)->xd4 = p2[0];
      TGR_PTR(BrSeason *, D_8031B760[1].season)->xd8 = p2[1];
      TGR_PTR(BrSeason *, D_8031B760[1].season)->xdc = p2[2];
      TGR_PTR(BrSeason *, D_8031B760[1].season)->xe0 = p2[3];
      TGR_PTR(BrSeason *, D_8031B760[1].season)->xe4 = p2[4];
      D_80272554 = 18;
      D_80316422 = 1;
      break;
    case 8:
      TGR_PTR(BrSeason *, D_8031B760[0].season)->xd4 = ((BrSeason *)data)->xd4;
      TGR_PTR(BrSeason *, D_8031B760[0].season)->xd8 = ((BrSeason *)data)->xd8;
      TGR_PTR(BrSeason *, D_8031B760[0].season)->xdc = ((BrSeason *)data)->xdc;
      TGR_PTR(BrSeason *, D_8031B760[0].season)->xe0 = ((BrSeason *)data)->xe0;
      TGR_PTR(BrSeason *, D_8031B760[0].season)->xe4 = ((BrSeason *)data)->xe4;
      TGR_PTR(BrSeason *, D_8031B760[1].season)->xd4 = p2[0];
      TGR_PTR(BrSeason *, D_8031B760[1].season)->xd8 = p2[1];
      TGR_PTR(BrSeason *, D_8031B760[1].season)->xdc = p2[2];
      TGR_PTR(BrSeason *, D_8031B760[1].season)->xe0 = p2[3];
      TGR_PTR(BrSeason *, D_8031B760[1].season)->xe4 = p2[4];
      memcpy(D_802723D0, data + 0x13c, 12);
      BrVolumesApply();
      D_80316422 = 1;
      D_80272554 = 16;
      break;
    case 9:
      ((BrSeason *)data)->xd4 = TGR_PTR(BrSeason *, D_8031B760[0].season)->xd4;
      ((BrSeason *)data)->xd8 = TGR_PTR(BrSeason *, D_8031B760[0].season)->xd8;
      ((BrSeason *)data)->xdc = TGR_PTR(BrSeason *, D_8031B760[0].season)->xdc;
      ((BrSeason *)data)->xe0 = TGR_PTR(BrSeason *, D_8031B760[0].season)->xe0;
      ((BrSeason *)data)->xe4 = TGR_PTR(BrSeason *, D_8031B760[0].season)->xe4;
      k = 4;
      memcpy(data + 0x13c, D_802723D0, 12);
      goto write;
    case 7:
      k = 4;
      memcpy(data, TGR_PTR(BrSeason *, D_8031B760[0].season), 0x128);
    write:
      p2[0] = TGR_PTR(BrSeason *, D_8031B760[1].season)->xd4;
      p2[1] = TGR_PTR(BrSeason *, D_8031B760[1].season)->xd8;
      p2[2] = TGR_PTR(BrSeason *, D_8031B760[1].season)->xdc;
      p2[3] = TGR_PTR(BrSeason *, D_8031B760[1].season)->xe0;
      p2[4] = TGR_PTR(BrSeason *, D_8031B760[1].season)->xe4;
      sum = 0xBFED812;
      for (k = 4; k < len; k++) {
        sum += D_80272500[k];
      }
      memcpy(D_80272500, &sum, 4);
      DBG(("checksum is (%08x/%02x%02x%02x%02x) (%d)\n", sum, D_80272500[0], D_80272500[1],
           D_80272500[2], D_80272500[3], len));
      if (tgr_eeprom_long_write((TgrMsgQueue *)D_80272D48, 0, D_80272500, len) != 0) {
        D_802724F0 = 5;
        D_80272554 = 15;
      } else {
        D_80272554 = 16;
      }
      break;
    }
    break;
  case 5:                       /* erase: both seasons afresh, written to the EEPROM */
    D_802724FC = 0;
    if (tgr_eeprom_probe((TgrMsgQueue *)D_80272D48) == 0) {
      D_802724F0 = 1;
      D_80272554 = 15;
    } else {
      unsigned char *blk;
      int j;
      int unused6C;
      int n;
      int s;
      int *p2;

      blk = D_80272500 + 4;
      p2 = (int *)(blk + 0x128);
      n = (blk - D_80272500 + 0x14f) & ~7;
      BrCarReset(&D_8031B760[0]);
      BrCarReset(&D_8031B760[1]);
      memcpy(blk, TGR_PTR(BrSeason *, D_8031B760[0].season), 0x128);
      p2[0] = TGR_PTR(BrSeason *, D_8031B760[1].season)->xd4;
      p2[1] = TGR_PTR(BrSeason *, D_8031B760[1].season)->xd8;
      p2[2] = TGR_PTR(BrSeason *, D_8031B760[1].season)->xdc;
      p2[3] = TGR_PTR(BrSeason *, D_8031B760[1].season)->xe0;
      p2[4] = TGR_PTR(BrSeason *, D_8031B760[1].season)->xe4;
      memcpy(blk + 0x13c, D_802723D0, 12);
      s = 0xBFED812;
      for (j = 4; j < n; j++) {
        s += D_80272500[j];
      }
      memcpy(D_80272500, &s, 4);
#if TGR_REGION_EUR
      tgr_print("checksum is (%08x/%02x%02x%02x%02x) (%d)\n", s, D_80272500[0], D_80272500[1],
                   D_80272500[2], D_80272500[3], n);
#endif
      if (tgr_eeprom_long_write((TgrMsgQueue *)D_80272D48, 0, D_80272500, n) != 0) {
        D_802724F0 = 5;
        D_80272554 = 15;
      } else {
        D_80272554 = 16;
      }
    }
    break;
  case 7:                       /* probe the pak */
    tgr_pak_present(D_80272D48, &pakBits);
    if ((pakBits & (1 << D_80271FA8)) == 0) {
      D_802724F0 = 1;
      D_80272554 = 15;
    } else {
#if TGR_REGION_EUR
      tgr_print("\nInitializing controller pak...\n");
#endif
      D_802724F0 = tgr_pak_open_new(D_80272D48, PFS, D_80271FA8);
      if (D_802724F0 != 0) {
        if (D_802724F0 == 10) {
          if (tgr_rumble_open(D_80272D48, &D_8031A3F8[D_80271FA8], D_80271FA8) == 0) {
            tgr_rumble_off(&D_8031A3F8[D_80271FA8]);
            D_802724F0 = 9999;
            D_802724FC = 1;
            D_80272554 = 15;
          } else {
            D_802724F0 = 0;
            D_80272554 = 8;
          }
        } else {
          D_8031B1E8[D_80271FA8] = 0;
          D_80272554 = 15;
        }
      } else if (D_802724F0 == 0) {
        D_8031B1E8[D_80271FA8] = 0;
        if (D_80316420[D_80271FA8] == 0) {
          D_80316420[D_80271FA8] = 1;
          memcpy(D_803163E0[D_80271FA8], D_80369EC0[D_80271FA8].id, 32);
          D_80272554 = 10;
        } else if (bcmp(D_803163E0[D_80271FA8], D_80369EC0[D_80271FA8].id, 32) != 0) {
          memcpy(D_803163E0[D_80271FA8], D_80369EC0[D_80271FA8].id, 32);
          D_80272554 = 9;
        } else {
          D_80272554 = 10;
        }
      }
    }
    break;
  case 8:
    D_802724F0 = tgr_pak_repair(PFS);
    if (D_802724F0 != 0) {
      D_80272554 = 15;
    } else {
      D_80272554 = 7;
    }
    break;
  case 9:                       /* a different pak */
    D_80316423 = 0;
    BrTextHighlightOff();
    BrTextAlignLeft();
#if TGR_REGION_EUR
    BrTextSetFont(12);
    BrTextSetColours(0xFF, 0xFF, 0xFF, 0xFF, 0xF5, 0);
    BrBevelPanel(0xAF, 0xB9, 0x122, 0x89, 3, 0, 0, 0x80, 0x80, 0x80);
    BrTextPrint("A NEW CONTROLLER PAK", 95, 108);
    BrTextPrint("WAS INSERTED.", 95, 121);
    BrTextPrint("THIS CONTROLLER PAK", 95, 140);
    BrTextPrint("WILL BE USED.", 95, 153);
#else
    BrTextSetFont(14);
    BrTextSetColours(0xFF, 0xFF, 0xFF, 0xFF, 0xF5, 0);
    BrBevelPanel(0x99, 0xB9, 0x14E, 0x89, 3, 0, 0, 0x80, 0x80, 0x80);
    BrTextPrint("%j038B@\xC1\xCF\xC3\xC8(\x88\x8D\x8F\xA1" "9", 84, 108);
    BrTextPrint("%j2Bq6hc@6R&", 84, 121);
    BrTextPrint("%jqt\xC1\xCF\xC3\xC8(\x88\x8D\x8F\xA1w'", 84, 140);
    BrTextPrint("%j\xDE@6R&", 84, 153);
#endif
    if (PAD->pressed & 0x8030) {
      BrPadConsume(PAD, 0x8030);
      D_80272554 = 10;
    }
    break;
  case 10:                      /* find the file */
    D_802724F0 = tgr_pak_find(PFS, COMPANY, GAME, name, ext, &D_80316424);
    if (D_802724F0 == 3) {
      DBG(("File system is corrupted, attempting repair...\n"));
      tgr_pak_check(PFS);
      D_802724F0 = tgr_pak_find(PFS, COMPANY, GAME, name, ext, &D_80316424);
    }
    if (D_802724F0 == 0) {
      switch (items[D_80272550].kind) {
      case 0:
      case 2:
      case 4:
        D_80272554 = 11;
        break;
      case 1:
      case 3:
        D_80272554 = 13;
        break;
      case 5:
        D_80272554 = 14;
        break;
      default:
        D_80272554 = 1;
        break;
      }
    } else if (D_802724F0 == 5) {
#if TGR_REGION_EUR || TGR_REGION_JPN
      switch (items[D_80272550].kind) {
      case 0:
      case 2:
      case 4:
        D_80272554 = 15;
        break;
      default:
        D_80272554 = 12;
        break;
      }
#else
      if (items[D_80272550].kind == 0 || items[D_80272550].kind == 2 || items[D_80272550].kind == 4) {
        D_80272554 = 15;
      } else {
        D_80272554 = 12;
      }
#endif
    } else {
      D_80272554 = 15;
    }
    break;
  case 11:                      /* load */
    if (D_802724F0 == 0) {
#if TGR_REGION_EUR
      switch (items[D_80272550].kind) {
      case 0:
        tgr_print("Loading season data...\n");
        break;
      case 2:
        tgr_print("Loading compressed ghost data...\n");
        break;
      case 4:
        tgr_print("Loading Options...\n");
        break;
      }
#endif
      if (items[D_80272550].kind != 4) {
        D_802724F0 = tgr_pak_io(PFS, D_80316424, 0, 0, size, D_80272500);
      } else {
        D_802724F0 = tgr_pak_io(PFS, D_80316424, 0, 0, 0x80, D_80272500);
      }
      if (D_802724F0 != 0) {
        D_80272554 = 15;
#if TGR_REGION_EUR || TGR_REGION_JPN
      } else switch (items[D_80272550].kind) {
      case 0:
#else
      } else if (items[D_80272550].kind == 0) {
#endif
        if (D_802724F4 == 0) {
          opts[0] = TGR_PTR(BrSeason *, D_8031B760[D_80271FA8 ^ 1].season)->xd4;
          opts[1] = TGR_PTR(BrSeason *, D_8031B760[D_80271FA8 ^ 1].season)->xd8;
          opts[2] = TGR_PTR(BrSeason *, D_8031B760[D_80271FA8 ^ 1].season)->xdc;
          opts[3] = TGR_PTR(BrSeason *, D_8031B760[D_80271FA8 ^ 1].season)->xe0;
          opts[4] = TGR_PTR(BrSeason *, D_8031B760[D_80271FA8 ^ 1].season)->xe4;
        }
        memcpy(TGR_PTR(BrSeason *, D_8031B760[0].season), D_80272500, 0x128);
        memcpy(TGR_PTR(BrSeason *, D_8031B760[1].season), D_80272500, 0x128);
        if (D_802724F4 == 0) {
          TGR_PTR(BrSeason *, D_8031B760[D_80271FA8 ^ 1].season)->xd4 = opts[0];
          TGR_PTR(BrSeason *, D_8031B760[D_80271FA8 ^ 1].season)->xd8 = opts[1];
          TGR_PTR(BrSeason *, D_8031B760[D_80271FA8 ^ 1].season)->xdc = opts[2];
          TGR_PTR(BrSeason *, D_8031B760[D_80271FA8 ^ 1].season)->xe0 = opts[3];
          TGR_PTR(BrSeason *, D_8031B760[D_80271FA8 ^ 1].season)->xe4 = opts[4];
        }
        DBG(("Done!\n"));
        if (D_802724FC == 0) {
          D_80272554 = 18;
        } else {
          D_80272554 = 16;
        }
        D_80316422 = 1;
#if TGR_REGION_EUR || TGR_REGION_JPN
        break;
      case 2:
#else
      } else if (items[D_80272550].kind == 2) {
#endif
        DBG(("Extracting ghost data...\n"));
        D_80270784 = BrRlePlanesUnpack(D_80307F00, 0xDE5C, D_80272500, 2);
        D_8028B940 = D_80307F00[0];
#if TGR_REGION_EUR || TGR_REGION_JPN
        TGR_PTR(BrSeason *, D_8031B760[0].season)->xce |= 1 << D_80307F00[0];
#else
        TGR_PTR(BrSeason *, D_8031B760[0].season)->xce |= 1 << D_8028B940;
#endif
        TGR_PTR(BrSeason *, D_8031B760[0].season)->unlocked |= 1 << D_80307F00[1];
        DBG(("Done!\n"));
        if (D_802724FC == 0) {
          D_80272554 = 18;
        } else {
          D_80272554 = 16;
        }
        D_80316422 = 1;
#if TGR_REGION_EUR || TGR_REGION_JPN
        break;
      case 4:
#else
      } else if (items[D_80272550].kind == 4) {
#endif
        memcpy(D_802723D0, D_80272500, 12);
        D_802724F0 = tgr_pak_io(PFS, D_80316424, 0, 0x80, 0x80, D_80272500);
        if (D_802724F0 == 0) {
          memcpy(opts, D_80272500, 20);
          TGR_PTR(BrSeason *, D_8031B760[D_80271FA8].season)->xd4 = opts[0];
          TGR_PTR(BrSeason *, D_8031B760[D_80271FA8].season)->xd8 = opts[1];
          TGR_PTR(BrSeason *, D_8031B760[D_80271FA8].season)->xdc = opts[2];
          TGR_PTR(BrSeason *, D_8031B760[D_80271FA8].season)->xe0 = opts[3];
          TGR_PTR(BrSeason *, D_8031B760[D_80271FA8].season)->xe4 = opts[4];
          DBG(("Done!\n"));
          BrVolumesApply();
          D_80272554 = 16;
        } else {
          D_80272554 = 15;
        }
#if TGR_REGION_EUR || TGR_REGION_JPN
        break;
#endif
      }
#if TGR_REGION_EUR || TGR_REGION_JPN
    }
#else
    } else {
      D_80272554 = 15;
    }
#endif
    break;
  case 12:                      /* make room for a new file */
    D_802724F0 = tgr_pak_free(PFS, &freeBytes);
    if (D_802724F0 == 0) {
      if (freeBytes < size) {
        D_802724F0 = 7;
        D_80272554 = 15;
      } else {
#if TGR_REGION_EUR
        switch (items[D_80272550].kind) {
        case 1:
          tgr_print("Allocating %d bytes for season data...\n", size);
          break;
        case 3:
          tgr_print("Allocating %d bytes for ghost data...\n", size);
          break;
        case 5:
          tgr_print("Allocating %d bytes for options...\n", size);
          break;
        }
#endif
        D_802724F0 = tgr_pak_create(PFS, COMPANY, GAME, name, ext, size, &D_80316424);
        if (D_802724F0 == 3) {
          tgr_pak_check(PFS);
          D_802724F0 = tgr_pak_create(PFS, COMPANY, GAME, name, ext, size, &D_80316424);
        }
        if (D_802724F0 == 0) {
          D_80272554 = 14;
        } else {
          D_80272554 = 15;
        }
      }
    } else {
      D_80272554 = 15;
    }
    break;
  case 6:                       /* erase both seasons and the options? */
    D_80316423 = 0;
    y = 0x139 - D_8028D0B0.w;
#if TGR_REGION_EUR
    BrBevelPanel(0x73, 0xBB, 0x19A, 0x86, 3, 0, 0, 0x80, 0x80, 0x80);
    BrTextAlignLeft();
    BrTextHighlightOff();
    BrTextSetColours(0xFF, 0xFF, 0xFF, 0xFF, 0xF5, 0);
    BrTextSetFont(13);
    BrTextPrint("ABOUT TO ERASE SEASON DATA AND", 65, 108);
    BrTextPrint("OPTION DATA STORED ON GAME PAK.", 65, 122);
    BrTextPrint("DO YOU REALLY WANT TO DO THIS?", 65, 136);
    BrTextSetFont(10);
    BrTextPrint("%wwOK", (D_8028D0B0.w + 0x8BU) >> 1, (y + 18) >> 1);
    BrTextPrint("%wwCANCEL", (D_8028D0E0.h + 0x19DU) >> 1, (y + 18) >> 1);
    BrImageDrawAt(&D_8028D0B0, 0x85, y);
    BrImageDrawAt(&D_8028D0E0, 0x197, y);
#else
    BrBevelPanel(0x5F, 0xBB, 0x1C2, 0x86, 3, 0, 0, 0x80, 0x80, 0x80);
    BrTextAlignLeft();
    BrTextHighlightOff();
    BrTextSetColours(0xFF, 0xFF, 0xFF, 0xFF, 0xF5, 0);
    BrTextSetFont(14);
    BrTextPrint("SEASON DATA %js%e OPTION DATA %j9", 55, 115);
    BrTextPrint("%jRlc\xE0`6R9'u\x7FsPD\xE0" "B6R1%%", 55, 129);
    BrTextSetFont(10);
    BrTextPrint("%wwOK", (D_8028D0B0.w + 0x77U) >> 1, (y + 18) >> 1);
    BrTextPrint("%wwCANCEL", (D_8028D0E0.h + 0x1B1U) >> 1, (y + 18) >> 1);
    BrImageDrawAt(&D_8028D0B0, 0x71, y);
    BrImageDrawAt(&D_8028D0E0, 0x1AB, y);
#endif
    if (PAD->pressed & 0x10) {
      BrPadConsume(PAD, 0x10);
      D_80272554 = 5;
    } else if (PAD->pressed & 0x20) {
      BrPadConsume(PAD, 0x20);
      BrSfxFadeTo(1.0f, 0.2f);
      BrMusicFadeTo(1.0f, 0.2f);
      D_80272554 = 1;
    }
    break;
  case 13:                      /* overwrite? */
    D_80316423 = 0;
    h = 0xBF;
    if (D_802724F4 == 0) {
      h = 0xC9;
    }
    y = h - D_8028D0B0.w + 0x62;
    BrBevelPanel(0xD5, h, 0xD6, 0x6A, 3, 0, 0, 0x80, 0x80, 0x80);
    BrTextAlignLeft();
    BrTextHighlightOff();
    BrTextSetColours(0xFF, 0xFF, 0xFF, 0xFF, 0xF5, 0);
#if TGR_REGION_EUR
    BrTextSetFont(13);
    BrTextPrint("OK TO OVERWRITE", 114, (h + 30) >> 1);
    BrTextPrint("SAVED DATA?", 114, ((h + 30) >> 1) + 14);
#else
    BrTextSetFont(14);
    BrTextPrint("%j\xDA\xE4" "AB6R1%%", 114, ((h + 30) >> 1) + 7);
#endif
    BrTextSetFont(10);
    BrTextPrint("%wwOK", (D_8028D0B0.w + 0xEDU) >> 1, (y + 18) >> 1);
    BrTextPrint("%wwCANCEL", (D_8028D0E0.h + 0x13BU) >> 1, (y + 18) >> 1);
    BrImageDrawAt(&D_8028D0B0, 0xE7, y);
    BrImageDrawAt(&D_8028D0E0, 0x135, y);
    if (PAD->pressed & 0x10) {
      BrPadConsume(PAD, 0x10);
      if (items[D_80272550].kind == 7) {
        D_80272554 = 4;
        jpn_D_80316D38 = 1;
      } else {
        D_80272554 = 14;
      }
    } else if (PAD->pressed & 0x20) {
      BrPadConsume(PAD, 0x20);
      BrSfxFadeTo(1.0f, 0.2f);
      BrMusicFadeTo(1.0f, 0.2f);
      D_80272554 = 1;
    }
    break;
  case 14:                      /* save */
#if TGR_REGION_EUR
    switch (items[D_80272550].kind) {
    case 1:
      tgr_print("Saving season data...\n");
      memcpy(D_80272500, TGR_PTR(BrSeason *, D_8031B760[0].season), 0x128);
      D_802724F0 = tgr_pak_io(PFS, D_80316424, 1, 0, 0x200, D_80272500);
      if (D_802724F0 == 0) {
        tgr_print("Done!\n");
        D_80272554 = 16;
      } else {
        D_80272554 = 15;
      }
      break;
    case 3:
      tgr_print("Compressing ghost data...\n");
      newSize = BrRleEncode(D_80272500, 0x3A00, D_80307F00, D_80270784, 2);
      tgr_print("new size = %d\n", newSize);
      if (newSize != -1) {
        tgr_print("Saving ghost data to controller pak...\n");
        D_802724F0 = tgr_pak_io(PFS, D_80316424, 1, 0, 0x3A00, D_80272500);
        if (D_802724F0 == 0) {
          tgr_print("Done!\n");
          D_80272554 = 16;
        } else {
          D_80272554 = 15;
        }
      } else {
        D_802724F0 = -99999;
        D_80272554 = 15;
      }
      break;
    case 5:
      tgr_print("Saving options...\n");
      memcpy(D_80272500, D_802723D0, 12);
      D_802724F0 = tgr_pak_io(PFS, D_80316424, 1, 0, 0x80, D_80272500);
      if (D_802724F0 != 0) {
        D_80272554 = 15;
      } else {
        opts[0] = TGR_PTR(BrSeason *, D_8031B760[D_80271FA8].season)->xd4;
        opts[1] = TGR_PTR(BrSeason *, D_8031B760[D_80271FA8].season)->xd8;
        opts[2] = TGR_PTR(BrSeason *, D_8031B760[D_80271FA8].season)->xdc;
        opts[3] = TGR_PTR(BrSeason *, D_8031B760[D_80271FA8].season)->xe0;
        opts[4] = TGR_PTR(BrSeason *, D_8031B760[D_80271FA8].season)->xe4;
        memcpy(D_80272500, opts, 20);
        D_802724F0 = tgr_pak_io(PFS, D_80316424, 1, 0x80, 0x80, D_80272500);
        if (D_802724F0 == 0) {
          tgr_print("Done!\n");
          D_80272554 = 16;
        } else {
          D_80272554 = 15;
        }
      }
      break;
    }
#elif TGR_REGION_JPN
    switch (items[D_80272550].kind) {
    case 1:
      memcpy(D_80272500, TGR_PTR(BrSeason *, D_8031B760[0].season), 0x128);
      D_802724F0 = tgr_pak_io(PFS, D_80316424, 1, 0, 0x200, D_80272500);
      if (D_802724F0 == 0) {
        D_80272554 = 16;
      } else {
        D_80272554 = 15;
      }
      break;
    case 3:
      if (BrRleEncode(D_80272500, 0x3A00, D_80307F00, D_80270784, 2) != -1) {
        D_802724F0 = tgr_pak_io(PFS, D_80316424, 1, 0, 0x3A00, D_80272500);
        if (D_802724F0 == 0) {
          D_80272554 = 16;
        } else {
          D_80272554 = 15;
        }
      } else {
        D_802724F0 = -99999;
        D_80272554 = 15;
      }
      break;
    case 5:
      memcpy(D_80272500, D_802723D0, 12);
      D_802724F0 = tgr_pak_io(PFS, D_80316424, 1, 0, 0x80, D_80272500);
      if (D_802724F0 != 0) {
        D_80272554 = 15;
      } else {
        opts[0] = TGR_PTR(BrSeason *, D_8031B760[D_80271FA8].season)->xd4;
        opts[1] = TGR_PTR(BrSeason *, D_8031B760[D_80271FA8].season)->xd8;
        opts[2] = TGR_PTR(BrSeason *, D_8031B760[D_80271FA8].season)->xdc;
        opts[3] = TGR_PTR(BrSeason *, D_8031B760[D_80271FA8].season)->xe0;
        opts[4] = TGR_PTR(BrSeason *, D_8031B760[D_80271FA8].season)->xe4;
        memcpy(D_80272500, opts, 20);
        D_802724F0 = tgr_pak_io(PFS, D_80316424, 1, 0x80, 0x80, D_80272500);
        if (D_802724F0 == 0) {
          D_80272554 = 16;
        } else {
          D_80272554 = 15;
        }
      }
      break;
    }
#else
    if (items[D_80272550].kind == 1) {
      memcpy(D_80272500, TGR_PTR(BrSeason *, D_8031B760[0].season), 0x128);
      D_802724F0 = tgr_pak_io(PFS, D_80316424, 1, 0, 0x200, D_80272500);
      if (D_802724F0 == 0) {
        D_80272554 = 16;
      } else {
        D_80272554 = 15;
      }
    } else if (items[D_80272550].kind == 3) {
      if (BrRleEncode(D_80272500, 0x3A00, D_80307F00, D_80270784, 2) != -1) {
        D_802724F0 = tgr_pak_io(PFS, D_80316424, 1, 0, 0x3A00, D_80272500);
        if (D_802724F0 == 0) {
          D_80272554 = 16;
        } else {
          D_80272554 = 15;
        }
      } else {
        D_802724F0 = -99999;
        D_80272554 = 15;
      }
    } else if (items[D_80272550].kind == 5) {
      memcpy(D_80272500, D_802723D0, 12);
      D_802724F0 = tgr_pak_io(PFS, D_80316424, 1, 0, 0x80, D_80272500);
      if (D_802724F0 != 0) {
        D_80272554 = 15;
      } else {
        opts[0] = TGR_PTR(BrSeason *, D_8031B760[D_80271FA8].season)->xd4;
        opts[1] = TGR_PTR(BrSeason *, D_8031B760[D_80271FA8].season)->xd8;
        opts[2] = TGR_PTR(BrSeason *, D_8031B760[D_80271FA8].season)->xdc;
        opts[3] = TGR_PTR(BrSeason *, D_8031B760[D_80271FA8].season)->xe0;
        opts[4] = TGR_PTR(BrSeason *, D_8031B760[D_80271FA8].season)->xe4;
        memcpy(D_80272500, opts, 20);
        D_802724F0 = tgr_pak_io(PFS, D_80316424, 1, 0x80, 0x80, D_80272500);
        if (D_802724F0 == 0) {
          D_80272554 = 16;
        } else {
          D_80272554 = 15;
        }
      }
    }
#endif
    break;
  case 15:                      /* an error, or the Rumble Pak is in the way */
    D_80316423 = 0;
    BrTextHighlightOff();
    BrTextAlignLeft();
#if TGR_REGION_EUR
    BrTextSetFont(12);
    BrTextSetColours(0xFF, 0xFF, 0xFF, 0xFF, 0xF5, 0);
    switch (D_802724F0) {
    case 10:
      BrBevelPanel(0xBD, 0xD9, 0x106, 0x4A, 3, 0, 0, 0x80, 0x80, 0x80);
      BrTextPrint("THE CONTROLLER PAK", 102, 124);
      BrTextPrint("IS NONFUNCTIONAL.", 102, 138);
      break;
    case 1:
    case 11:
      BrBevelPanel(0x50, 0xD8, 0x1E0, 0x4C, 3, 0, 0, 0x80, 0x80, 0x80);
      switch (items[D_80272550].kind) {
      case 6:
      case 7:
      case 8:
      case 9:
        BrTextPrint("CANNOT ACCESS GAME PAK SAVE AREA.", 48, 124);
        break;
      default:
        BrTextPrint("CONTROLLER PAK NOT FOUND.", 48, 124);
        break;
      }
      switch (items[D_80272550].kind) {
      case 0:
      case 2:
      case 4:
      case 6:
      case 8:
        BrTextPrint("DATA CANNOT BE LOADED.", 48, 138);
        break;
      default:
        BrTextPrint("DATA CANNOT BE SAVED.", 48, 138);
        break;
      }
      break;
    case 9999:
      y = 0x146 - D_8028D0B0.w;
      BrBevelPanel(0x79, 0xAE, 0x18D, 0xA0, 3, 0, 0, 0x80, 0x80, 0x80);
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
      BrBevelPanel(0xA5, 0x9F, 0x136, 0xBD, 3, 0, 0, 0x80, 0x80, 0x80);
      switch (items[D_80272550].kind) {
      case 1:
        BrTextPrint("INSUFFICIENT FREE PAGES", 90, 95);
        BrTextPrint("OR FREE NOTES IN THE", 90, 108);
        BrTextPrint("CONTROLLER PAK.", 90, 121);
        BrTextPrint("TWO PAGES AND ONE NOTE", 90, 140);
        BrTextPrint("ARE NEEDED TO SAVE THE", 90, 153);
        BrTextPrint("SEASON DATA.", 90, 166);
        break;
      case 3:
        BrTextPrint("INSUFFICIENT FREE PAGES", 90, 95);
        BrTextPrint("OR FREE NOTES IN THE", 90, 108);
        BrTextPrint("CONTROLLER PAK.", 90, 121);
        BrTextPrint("58 PAGES AND ONE NOTE", 90, 140);
        BrTextPrint("ARE NEEDED TO SAVE THE", 90, 153);
        BrTextPrint("GHOST CAR DATA.", 90, 166);
        break;
      case 5:
        BrTextPrint("INSUFFICIENT FREE PAGES", 90, 95);
        BrTextPrint("OR FREE NOTES IN THE", 90, 108);
        BrTextPrint("CONTROLLER PAK.", 90, 121);
        BrTextPrint("ONE PAGE AND ONE NOTE", 90, 140);
        BrTextPrint("ARE NEEDED TO SAVE THE", 90, 153);
        BrTextPrint("OPTIONS.", 90, 166);
        break;
      }
      break;
    case -99999:
      BrBevelPanel(0x75, 0xBD, 0x196, 0x82, 3, 0, 0, 0x80, 0x80, 0x80);
      BrTextPrint("UNABLE TO SAVE GHOST CAR DATA.", 66, 110);
      BrTextPrint("GHOST CAR DATA IS LARGER THAN", 66, 124);
      BrTextPrint("THE MAXIMUM ALLOCATED SPACE", 66, 138);
      BrTextPrint("IN THE CONTROLLER PAK.", 66, 152);
      break;
    case 4:
      BrBevelPanel(0xBF, 0xD9, 0x102, 0x4A, 3, 0, 0, 0x80, 0x80, 0x80);
      BrTextPrint("CONTROLLER ERROR", 103, 124);
      BrTextPrint("HAS BEEN DETECTED!", 103, 138);
      break;
    case 5:
      switch (items[D_80272550].kind) {
      case 6:
      case 8:
        BrBevelPanel(0xAD, 0xD9, 0x125, 0x4A, 3, 0, 0, 0x80, 0x80, 0x80);
        BrTextPrint("SAVED DATA NOT FOUND", 94, 124);
        BrTextPrint("IN GAME PAK.", 94, 138);
        break;
      case 7:
      case 9:
        BrBevelPanel(0xAD, 0xD9, 0x125, 0x4A, 3, 0, 0, 0x80, 0x80, 0x80);
        BrTextPrint("ERROR WRITING TO THE", 94, 124);
        BrTextPrint("GAME PAK SAVE AREA.", 94, 138);
        break;
      case 0:
      case 2:
      case 4:
        BrBevelPanel(0xAD, 0xD9, 0x125, 0x4A, 3, 0, 0, 0x80, 0x80, 0x80);
        BrTextPrint("SAVED DATA NOT FOUND", 94, 124);
        BrTextPrint("IN CONTROLLER PAK.", 94, 138);
        break;
      case 1:
      case 3:
      case 5:
        BrBevelPanel(0xB6, 0xD9, 0x114, 0x4A, 3, 0, 0, 0x80, 0x80, 0x80);
        BrTextPrint("UNABLE TO SAVE DATA", 99, 124);
        BrTextPrint("TO CONTROLLER PAK.", 99, 138);
        break;
      }
      break;
    case 2:
      BrBevelPanel(0xAF, 0xBB, 0x122, 0x89, 3, 0, 0, 0x80, 0x80, 0x80);
      BrTextPrint("A NEW CONTROLLER PAK", 95, 109);
      BrTextPrint("WAS INSERTED.", 95, 122);
      BrTextPrint("THIS CONTROLLER PAK", 95, 141);
      BrTextPrint("WILL BE USED.", 95, 154);
      break;
    default:
      BrBevelPanel(0xB1, 0xD9, 0x11E, 0x4A, 3, 0, 0, 0x80, 0x80, 0x80);
      BrTextPrint("CONTROLLER PAK ERROR", 96, 124);
      BrTextPrint("HAS BEEN DETECTED!", 96, 138);
      break;
    }
#else
    BrTextSetFont(14);
    BrTextSetColours(0xFF, 0xFF, 0xFF, 0xFF, 0xF5, 0);
    switch (D_802724F0) {
    case 10:
      BrBevelPanel(0x50, 0xD9, 0x1E0, 0x4A, 3, 0, 0, 0x80, 0x80, 0x80);
      BrTextPrint("%j\xC1\xCF\xC3\xC8(\x88\x8D\x8F\xA1" "9'qghc@6R&", 48, 124);
      BrTextPrint("%j038B@\xC1\xCF\xC3\xC8(\x88\x8D\x8F\xA1w'2Bc\xDB" "2@&", 48, 138);
      break;
    case 1:
    case 11:
      switch (items[D_80272550].kind) {
      case 6:
      case 7:
      case 8:
      case 9:
        newSize = 0x172;
        break;
      default:
        newSize = 0x1E0;
        break;
      }
      y = (0x280 - newSize) >> 1;
      BrBevelPanel(y, 0xD8, newSize, 0x4C, 3, 0, 0, 0x80, 0x80, 0x80);
      switch (items[D_80272550].kind) {
      case 6:
      case 7:
      case 8:
      case 9:
        BrTextPrint("%j\xB9(\xA6\x8D\x8F\xA1" "e\x80\xA1\xB2\xA2kA6b\x7F&", (y + 16) >> 1, 124);
        break;
      default:
        BrTextPrint("%j\xC1\xCF\xC3\xC8(\x88\x8D\x8F\xA1" "90H6b\x7F&", (y + 16) >> 1, 124);
        break;
      }
      switch (items[D_80272550].kind) {
      case 0:
      case 2:
      case 4:
      case 6:
      case 8:
        BrTextPrint("%j\xBB(\x83w\xC8(\xCBkA6b\x7F&", (y + 16) >> 1, 138);
        break;
      default:
        BrTextPrint("%j\xBB(\x83w\xB2(\xACkA6b\x7F&", (y + 16) >> 1, 138);
        break;
      }
      break;
    case 9999:
      y = 0x139 - D_8028D0B0.w;
      BrBevelPanel(0xC8, 0xBB, 0xF0, 0x86, 3, 0, 0, 0x80, 0x80, 0x80);
      BrTextPrint("%j\xD2\xD3\x8D\x8F\xA1s", 108, 109);
      BrTextPrint("%j\xC1\xCF\xC3\xC8(\x88\x8D\x8F\xA1w", 108, 122);
      BrTextPrint("%j\xD4\xD5" "Bc\xDB" "2@&", 108, 135);
      BrTextAlignLeft();
      BrTextSetFont(10);
      BrTextPrint("%wwOK", (D_8028D0B0.w + 0xDEU) >> 1, (y + 18) >> 1);
      BrTextPrint("%wwCANCEL", (D_8028D0E0.w + 0x14AU) >> 1, (y + 18) >> 1);
      BrImageDrawAt(&D_8028D0B0, 0xD8, y);
      BrImageDrawAt(&D_8028D0E0, 0x144, y);
      break;
    case 7:
    case 8:
      BrBevelPanel(0x82, 0xB7, 0x17C, 0x8D, 3, 0, 0, 0x80, 0x80, 0x80);
      switch (items[D_80272550].kind) {
      case 1:
        BrTextPrint("%j\xC1\xCF\xC3\xC8(\x88\x8D\x8F\xA1" "D", 73, 107);
        BrTextPrint("%j\xE1" "A\xBD(\x9A" "90H6b\x7F&", 73, 120);
        BrTextPrint("%j\x92(\xAA\xCF\xBB(\x83w\xB2(\xACRXD5", 73, 139);
        BrTextPrint("2%j\xBD(\x9A\xD6\xD7kR&", 73, 152);
        break;
      case 3:
        BrTextPrint("%j\xC1\xCF\xC3\xC8(\x88\x8D\x8F\xA1" "D", 73, 107);
        BrTextPrint("%j\xE1" "A\xBD(\x9A" "90H6b\x7F&", 73, 120);
        BrTextPrint("%j\xC9(\xA2\xC3\x81(\xBB(\x83w\xB2(\xACRXD5", 73, 139);
        BrTextPrint("58%j\xBD(\x9A\xD6\xD7kR&", 73, 152);
        break;
      case 5:
        BrTextPrint("%j\xC1\xCF\xC3\xC8(\x88\x8D\x8F\xA1" "D", 73, 107);
        BrTextPrint("%j\xE1" "A\xBD(\x9A" "90H6b\x7F&", 73, 120);
        BrTextPrint("%j\xC0\xAD\x92\xBF\xCF\xBB(\x83w\xB2(\xACRXD5", 73, 139);
        BrTextPrint("1%j\xBD(\x9A\xD6\xD7kR&", 73, 152);
        break;
      }
      break;
    case -99999:
      BrBevelPanel(0xB9, 0xBD, 0x10E, 0x82, 3, 0, 0, 0x80, 0x80, 0x80);
      BrTextPrint("%j\xC9(\xA2\xC3\x81(\xBB(\x83w", 100, 110);
      BrTextPrint("%j\xB2(\xACkA6b\x7F&", 100, 124);
      BrTextPrint("%j\xC1\xCF\xC3\xC8(\x88\x8D\x8F\xA1t", 100, 138);
      BrTextPrint("%j\xE1" "A\xBD(\x9A" "9\xE2H6b\x7F&", 100, 152);
      break;
    case 4:
      BrBevelPanel(0xBF, 0xD9, 0x102, 0x4A, 3, 0, 0, 0x80, 0x80, 0x80);
      BrTextPrint("%j\xC1\xCF\xC3\xC8(\x88" "D\xB0\x88(9", 103, 124);
      BrTextPrint("%j\xED\xEE" "B6B3&", 103, 138);
      break;
    case 5:
      switch (items[D_80272550].kind) {
      case 6:
      case 8:
        BrBevelPanel(0xAA, 0xD9, 0x12C, 0x4A, 3, 0, 0, 0x80, 0x80, 0x80);
        BrTextPrint("%j\xB9(\xA6\x8D\x8F\xA1" "D", 93, 124);
        BrTextPrint("%j\xB2(\xAC\xBB(\x83" "90H6b\x7F&", 93, 138);
        break;
      case 7:
      case 9:
        BrBevelPanel(0xCD, 0xD9, 0xE6, 0x4A, 3, 0, 0, 0x80, 0x80, 0x80);
        BrTextPrint("%j\xB9(\xA6\x8D\x8F\xA1" "D", 110, 124);
        BrTextPrint("%j\xE4" "A\xE5" "FkA6b\x7F&", 110, 138);
        break;
      case 0:
      case 2:
      case 4:
        BrBevelPanel(0xB6, 0xD9, 0x113, 0x4A, 3, 0, 0, 0x80, 0x80, 0x80);
        BrTextPrint("%j\xC1\xCF\xC3\xC8(\x88\x8D\x8F\xA1" "D", 99, 124);
        BrTextPrint("%j\xB2(\xAC\xBB(\x83" "90H6b\x7F&", 99, 138);
        break;
      case 1:
      case 3:
      case 5:
        BrBevelPanel(0xB6, 0xD9, 0x114, 0x4A, 3, 0, 0, 0x80, 0x80, 0x80);
        BrTextPrint("%j\xC1\xCF\xC3\xC8(\x88\x8D\x8F\xA1" "D", 99, 124);
        BrTextPrint("%j\xB2(\xACkA6b\x7F&", 99, 138);
        break;
      }
      break;
    case 2:
      BrBevelPanel(0x99, 0xBB, 0x14E, 0x89, 3, 0, 0, 0x80, 0x80, 0x80);
      BrTextPrint("%j038B@\xC1\xCF\xC3\xC8(\x88\x8D\x8F\xA1" "9", 84, 109);
      BrTextPrint("%j2Bq6hc@6R&", 84, 122);
      BrTextPrint("%jqt\xC1\xCF\xC3\xC8(\x88\x8D\x8F\xA1w'", 84, 141);
      BrTextPrint("%j\xDE@6R&", 84, 154);
      break;
    default:
      BrBevelPanel(0xB1, 0xD9, 0x11E, 0x4A, 3, 0, 0, 0x80, 0x80, 0x80);
      BrTextPrint("%j\xC1\xCF\xC3\xC8(\x88\x8D\x8F\xA1", 96, 124);
      BrTextPrint("%j9\xD0\xD1kR&", 96, 138);
      break;
    }
#endif
    if (D_802724F0 != 9999) {
      if (PAD->pressed & 0x8030) {
        BrPadConsume(PAD, 0x8030);
        D_802724F0 = 0;
        if (D_802724F0 == 2) {
          D_80272554 = 7;
        } else {
          D_80272554 = 1;
        }
      }
    } else if (PAD->pressed & 0x10) {
      BrPadConsume(PAD, 0x10);
      D_802724F0 = 0;
      D_80272554 = 7;
    } else if (PAD->pressed & 0x20) {
      BrPadConsume(PAD, 0x20);
      D_802724F0 = 0;
      BrSfxFadeTo(1.0f, 0.2f);
      BrMusicFadeTo(1.0f, 0.2f);
      D_80272554 = 1;
    }
    break;
  case 16:                      /* done */
    D_80316423 = 0;
    BrTextAlignCentre();
    BrTextHighlightOff();
#if TGR_REGION_EUR
    BrTextSetFont(12);
#else
    BrTextSetFont(14);
#endif
    switch (items[D_80272550].kind) {
    case 0:
    case 6:
      if (D_802724F4 == 0) {
        h = 0xD9;
      } else {
        h = 0xCE;
      }
      BrBevelPanel(0xE2, h, 0xBC, 0x4A, 3, 0, 0, 0x80, 0x80, 0x80);
#if TGR_REGION_EUR
      BrTextPrint("%ywSEASON DATA", 159, (h + 32) >> 1);
      BrTextPrint("%ywLOADED OK!", 159, ((h + 32) >> 1) + 14);
#else
      BrTextPrint("%yw%j\xC8(\xCB%e OK!", 159, ((h + 32) >> 1) + 7);
#endif
      break;
    case 2:
      if (D_802724F4 == 0) {
        h = 0xD9;
      } else {
        h = 0xCE;
      }
      BrBevelPanel(0xE2, h, 0xBC, 0x4A, 3, 0, 0, 0x80, 0x80, 0x80);
#if TGR_REGION_EUR
      BrTextPrint("%ywGHOST DATA", 159, (h + 32) >> 1);
      BrTextPrint("%ywLOADED OK!", 159, ((h + 32) >> 1) + 14);
#else
      BrTextPrint("%yw%j\xC8(\xCB%e OK!", 159, ((h + 32) >> 1) + 7);
#endif
      break;
    case 1:
    case 7:
      if (D_802724F4 == 0) {
        h = 0xD9;
      } else {
        h = 0xCE;
      }
      BrBevelPanel(0xE2, h, 0xBC, 0x4A, 3, 0, 0, 0x80, 0x80, 0x80);
#if TGR_REGION_EUR
      BrTextPrint("%ywSEASON DATA", 159, (h + 32) >> 1);
      BrTextPrint("%ywSAVED OK!", 159, ((h + 32) >> 1) + 14);
#else
      BrTextPrint("%yw%j\xB2(\xAC%e OK!", 159, ((h + 32) >> 1) + 7);
#endif
      break;
    case 3:
      if (D_802724F4 == 0) {
        h = 0xD9;
      } else {
        h = 0xCE;
      }
      BrBevelPanel(0xD8, h, 0xD0, 0x4A, 3, 0, 0, 0x80, 0x80, 0x80);
#if TGR_REGION_EUR
      BrTextPrint("%ywGHOST CAR DATA", 159, (h + 32) >> 1);
      BrTextPrint("%ywSAVED OK!", 159, ((h + 32) >> 1) + 14);
#else
      BrTextPrint("%yw%j\xB2(\xAC%e OK!", 159, ((h + 32) >> 1) + 7);
#endif
      break;
    case 4:
    case 8:
      BrBevelPanel(0xC3, 0xE7, 0xFA, 0x2E, 3, 0, 0, 0x80, 0x80, 0x80);
#if TGR_REGION_EUR
      BrTextPrint("%ywOPTIONS LOADED OK!", 159, 131);
#else
      BrTextPrint("%yw%j\xC8(\xCB%e OK!", 159, 131);
#endif
      break;
    case 5:
    case 9:
      BrBevelPanel(0xC3, 0xE7, 0xFA, 0x2E, 3, 0, 0, 0x80, 0x80, 0x80);
#if TGR_REGION_EUR
      BrTextPrint("%ywOPTIONS SAVED OK!", 159, 131);
#else
      BrTextPrint("%yw%j\xB2(\xAC%e OK!", 159, 131);
#endif
      break;
    case 10:
      BrBevelPanel(0xC3, 0xE7, 0xFA, 0x2E, 3, 0, 0, 0x80, 0x80, 0x80);
      BrTextPrint("%ywDONE!", 159, 131);
      break;
    default:
      D_80272554 = 1;
      break;
    }
    if (PAD->pressed & 0x8030) {
      BrPadConsume(PAD, 0x8030);
      if (D_802724FC != 0) {
        D_80272554 = 17;
      } else {
        BrSfxFadeTo(1.0f, 0.2f);
        BrMusicFadeTo(1.0f, 0.2f);
        D_80272554 = 1;
      }
    }
    break;
  case 17:                      /* put the Rumble Pak back */
    D_80316423 = 0;
    BrTextHighlightOff();
    BrTextAlignLeft();
    BrTextSetColours(0xFF, 0xFF, 0xFF, 0xFF, 0xF5, 0);
#if TGR_REGION_EUR
    BrBevelPanel(0x73, 0xB2, 0x19A, 0x98, 3, 0, 0, 0x80, 0x80, 0x80);
    BrTextSetFont(12);
    BrTextPrint("IF THE RUMBLE PAK IS TO BE USED,", 65, 105);
    BrTextPrint("REMOVE THE CONTROLLER PAK AND", 65, 118);
    BrTextPrint("INSERT THE RUMBLE PAK INTO THE", 65, 131);
    BrTextPrint("CONTROLLER.  PRESS THE A BUTTON", 65, 144);
    BrTextPrint("TO CONTINUE.", 65, 157);
#else
    BrBevelPanel(0xAA, 0xCC, 0x12C, 0x64, 3, 0, 0, 0x80, 0x80, 0x80);
    BrTextSetFont(14);
    BrTextPrint("%j\xD2\xD3\x8D\x8F\xA1w\xDEPsA5'", 93, 118);
    BrTextPrint("%j\xC1\xCF\xC3\xC8(\x88\x8D\x8F\xA1s", 93, 131);
    BrTextPrint("%j\xD2\xD3\x8D\x8F\xA1w\xD4\xD5" "Bc\xDB" "2@&", 93, 144);
#endif
    if (PAD->pressed & 0x10) {
      BrPadConsume(PAD, 0x10);
      if (items[D_80272550].kind == 0 || items[D_80272550].kind == 2) {
        D_80272554 = 18;
      } else {
        BrSfxFadeTo(1.0f, 0.2f);
        BrMusicFadeTo(1.0f, 0.2f);
        D_80272554 = 1;
      }
      D_802724FC = 0;
      if (tgr_rumble_open(D_80272D48, &D_8031A3F8[D_80271FA8], D_80271FA8) == 0) {
        D_8031B1E8[D_80271FA8] = 1;
      }
    }
    break;
  case 18:
    BrFadeTo(0.0f, 0.2f);
    D_80272554 = 19;
    break;
  case 19:
    BrSfxFadeTo(1.0f, 0.2f);
    BrMusicFadeTo(1.0f, 0.2f);
    break;
  }
  {
    Gfx *g = D_8028A858++;
    SET32(g->words.w1, 0);
    SET32(g->words.w0, 0xE7000000);
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
    if ((items[D_80272550].kind == 0 || items[D_80272550].kind == 6) && D_80316422 != 0) {
      D_80316422 = 0;
      BrModeSet(BrChampionshipStart);
    } else if (items[D_80272550].kind == 2 && D_80316422 != 0) {
      D_80316422 = 0;
      BrModeSet(BrTimeAttackStart);
    } else {
      D_80272550 = 0;
      if (D_802724F4 == 0) {
        BrModeSet(BrMainMenu);
      } else if ((mode = D_802724F4) == 1) {
        BrModeSet(BrTrackSelectScreen);
      } else if ((mode | 0) == 2) {
        BrModeSet(BrMainMenu);
      }
    }
  }
  D_802A4BE8 = 1;
}
#else
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
  extern unsigned char D_80272550;  /* the row: load/save season, ghost, options */
  extern unsigned char D_80272554;  /* the screen's state */
  extern unsigned char D_80316422;      /* data was loaded */
  extern unsigned char D_80316423;      /* the menu is live (not behind a message) */
  extern int D_80316424;                /* the open file */
  extern unsigned char D_80316428;      /* frames the fade has been done */

  D_802A4BE8 = 0;
  if (D_802724F8 == 0) {
    D_802724F8 = 1;
    D_80316422 = 0;
    D_80316423 = 1;
    D_802724F0 = 0;
    D_80316428 = 0;
    BrIfaceMemReset();
    BrFrameBeginLayout1();
    BrFrameEnd();
    BrFrameBeginLayout1();
    BrFrameEnd();
    BrAllocPaintShopGfxMem(&D_8028CB40);
    BrAllocPaintShopGfxMem(&D_8028CB70);
    BrAllocPaintShopGfxMem(&D_8028D0B0);
    BrAllocPaintShopGfxMem(&D_8028D0E0);
    D_80272500 = TGR_PTR(unsigned char *, BrIfaceMemAlloc(0x3A00));
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
      BrPadStickToButtons(tgr_addr32(PAD));
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
    tgr_pak_present(D_80272D48, &pakBits);
    if ((pakBits & (1 << D_80271FA8)) == 0) {
      D_802724F0 = 1;
      D_80272554 = 12;
    } else {
      tgr_print("\nInitializing controller pak...\n");
      D_802724F0 = tgr_pak_open_new(D_80272D48, PFS, D_80271FA8);
      if (D_802724F0 != 0) {
        if (D_802724F0 == 10) {
          if (tgr_rumble_open(D_80272D48, &D_8031A3F8[D_80271FA8], D_80271FA8) == 0) {
            tgr_rumble_off(&D_8031A3F8[D_80271FA8]);
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
    D_802724F0 = tgr_pak_repair(PFS);
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
    BrBevelPanel(0xAF, 0xB9, 0x122, 0x89, 3, 0, 0, 0x80, 0x80, 0x80);
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
    D_802724F0 = tgr_pak_find(PFS, COMPANY, GAME, name, ext, &D_80316424);
    if (D_802724F0 == 3) {
      tgr_print("File system is corrupted, attempting repair...\n");
      tgr_pak_check(PFS);
      D_802724F0 = tgr_pak_find(PFS, COMPANY, GAME, name, ext, &D_80316424);
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
        tgr_print("Loading season data...\n");
      } else if (D_80272550 == 2) {
        tgr_print("Loading compressed ghost data...\n");
      } else if (D_80272550 == 4) {
        tgr_print("Loading Options...\n");
      }
      if (D_80272550 != 4) {
        D_802724F0 = tgr_pak_io(PFS, D_80316424, 0, 0, size, D_80272500);
      } else {
        D_802724F0 = tgr_pak_io(PFS, D_80316424, 0, 0, 0x80, D_80272500);
      }
      if (D_802724F0 != 0) {
        D_80272554 = 12;
      } else {
        if (D_80272550 == 0) {
          if (D_802724F4 == 0) {
            opts[0] = TGR_PTR(BrSeason *, D_8031B760[D_80271FA8 ^ 1].season)->xd4;
            opts[1] = TGR_PTR(BrSeason *, D_8031B760[D_80271FA8 ^ 1].season)->xd8;
            opts[2] = TGR_PTR(BrSeason *, D_8031B760[D_80271FA8 ^ 1].season)->xdc;
            opts[3] = TGR_PTR(BrSeason *, D_8031B760[D_80271FA8 ^ 1].season)->xe0;
            opts[4] = TGR_PTR(BrSeason *, D_8031B760[D_80271FA8 ^ 1].season)->xe4;
          }
          memcpy(TGR_PTR(BrSeason *, D_8031B760[0].season), D_80272500, 0x128);
          memcpy(TGR_PTR(BrSeason *, D_8031B760[1].season), D_80272500, 0x128);
          if (D_802724F4 == 0) {
            TGR_PTR(BrSeason *, D_8031B760[D_80271FA8 ^ 1].season)->xd4 = opts[0];
            TGR_PTR(BrSeason *, D_8031B760[D_80271FA8 ^ 1].season)->xd8 = opts[1];
            TGR_PTR(BrSeason *, D_8031B760[D_80271FA8 ^ 1].season)->xdc = opts[2];
            TGR_PTR(BrSeason *, D_8031B760[D_80271FA8 ^ 1].season)->xe0 = opts[3];
            TGR_PTR(BrSeason *, D_8031B760[D_80271FA8 ^ 1].season)->xe4 = opts[4];
          }
          tgr_print("Done!\n");
          if (D_802724FC == 0) {
            D_80272554 = 15;
          } else {
            D_80272554 = 13;
          }
          D_80316422 = 1;
        } else if (D_80272550 == 2) {
          tgr_print("Extracting ghost data...\n");
          D_80270784 = BrRlePlanesUnpack(D_80307F00, 0xDE5C, D_80272500, 2);
          D_8028B940 = D_80307F00[0];
          TGR_PTR(BrSeason *, D_8031B760[0].season)->xce |= 1 << D_80307F00[0];
          TGR_PTR(BrSeason *, D_8031B760[0].season)->unlocked |= 1 << D_80307F00[1];
          tgr_print("Done!\n");
          if (D_802724FC == 0) {
            D_80272554 = 15;
          } else {
            D_80272554 = 13;
          }
          D_80316422 = 1;
        } else if (D_80272550 == 4) {
          memcpy(D_802723D0, D_80272500, 12);
          D_802724F0 = tgr_pak_io(PFS, D_80316424, 0, 0x80, 0x80, D_80272500);
          if (D_802724F0 == 0) {
            memcpy(opts, D_80272500, 20);
            TGR_PTR(BrSeason *, D_8031B760[D_80271FA8].season)->xd4 = opts[0];
            TGR_PTR(BrSeason *, D_8031B760[D_80271FA8].season)->xd8 = opts[1];
            TGR_PTR(BrSeason *, D_8031B760[D_80271FA8].season)->xdc = opts[2];
            TGR_PTR(BrSeason *, D_8031B760[D_80271FA8].season)->xe0 = opts[3];
            TGR_PTR(BrSeason *, D_8031B760[D_80271FA8].season)->xe4 = opts[4];
            tgr_print("Done!\n");
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
    D_802724F0 = tgr_pak_free(PFS, &freeBytes);
    if (D_802724F0 == 0) {
      if (freeBytes < size) {
        D_802724F0 = 7;
        D_80272554 = 12;
      } else {
        if (D_80272550 == 1) {
          tgr_print("Allocating %d bytes for season data...\n", size);
        } else if (D_80272550 == 3) {
          tgr_print("Allocating %d bytes for ghost data...\n", size);
        } else if (D_80272550 == 5) {
          tgr_print("Allocating %d bytes for options...\n", size);
        }
        D_802724F0 = tgr_pak_create(PFS, COMPANY, GAME, name, ext, size, &D_80316424);
        if (D_802724F0 == 3) {
          tgr_pak_check(PFS);
          D_802724F0 = tgr_pak_create(PFS, COMPANY, GAME, name, ext, size, &D_80316424);
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
    BrBevelPanel(0xD5, h, 0xD6, 0x6A, 3, 0, 0, 0x80, 0x80, 0x80);
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
      tgr_print("Saving season data...\n");
      memcpy(D_80272500, TGR_PTR(BrSeason *, D_8031B760[0].season), 0x128);
      D_802724F0 = tgr_pak_io(PFS, D_80316424, 1, 0, 0x200, D_80272500);
      if (D_802724F0 == 0) {
        tgr_print("Done!\n");
        D_80272554 = 13;
      } else {
        D_80272554 = 12;
      }
    } else if (D_80272550 == 3) {
      tgr_print("Compressing ghost data...\n");
      newSize = BrRleEncode(D_80272500, 0x3A00, D_80307F00, D_80270784, 2);
      tgr_print("new size = %d\n", newSize);
      if (newSize != -1) {
        tgr_print("Saving ghost data to controller pak...\n");
        D_802724F0 = tgr_pak_io(PFS, D_80316424, 1, 0, 0x3A00, D_80272500);
        if (D_802724F0 == 0) {
          tgr_print("Done!\n");
          D_80272554 = 13;
        } else {
          D_80272554 = 12;
        }
      } else {
        D_802724F0 = -99999;
        D_80272554 = 12;
      }
    } else if (D_80272550 == 5) {
      tgr_print("Saving options...\n");
      memcpy(D_80272500, D_802723D0, 12);
      D_802724F0 = tgr_pak_io(PFS, D_80316424, 1, 0, 0x80, D_80272500);
      if (D_802724F0 != 0) {
        D_80272554 = 12;
      } else {
        opts[0] = TGR_PTR(BrSeason *, D_8031B760[D_80271FA8].season)->xd4;
        opts[1] = TGR_PTR(BrSeason *, D_8031B760[D_80271FA8].season)->xd8;
        opts[2] = TGR_PTR(BrSeason *, D_8031B760[D_80271FA8].season)->xdc;
        opts[3] = TGR_PTR(BrSeason *, D_8031B760[D_80271FA8].season)->xe0;
        opts[4] = TGR_PTR(BrSeason *, D_8031B760[D_80271FA8].season)->xe4;
        memcpy(D_80272500, opts, 20);
        D_802724F0 = tgr_pak_io(PFS, D_80316424, 1, 0x80, 0x80, D_80272500);
        if (D_802724F0 == 0) {
          tgr_print("Done!\n");
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
      BrBevelPanel(0xBD, 0xD9, 0x106, 0x4A, 3, 0, 0, 0x80, 0x80, 0x80);
      BrTextPrint("THE CONTROLLER PAK", 102, 124);
      BrTextPrint("IS NONFUNCTIONAL.", 102, 138);
      break;
    case 1:
    case 11:
      BrBevelPanel(0x94, 0xD8, 0x158, 0x4C, 3, 0, 0, 0x80, 0x80, 0x80);
      BrTextPrint("CONTROLLER PAK NOT FOUND.", 82, 124);
      if ((unusedD0 = D_80272550) == 0 || D_80272550 == 2 || D_80272550 == 4) {
        BrTextPrint("DATA CANNOT BE LOADED.", 82, 138);
      } else if (unusedD0 == 1 || unusedD0 == 3 || unusedD0 == 5) {
        BrTextPrint("DATA CANNOT BE SAVED.", 82, 138);
      }
      break;
    case 9999:
      y = 0x146 - D_8028D0B0.w;
      BrBevelPanel(0x79, 0xAE, 0x18D, 0xA0, 3, 0, 0, 0x80, 0x80, 0x80);
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
      BrBevelPanel(0xA5, 0x9F, 0x136, 0xBD, 3, 0, 0, 0x80, 0x80, 0x80);
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
      BrBevelPanel(0x75, 0xBD, 0x196, 0x82, 3, 0, 0, 0x80, 0x80, 0x80);
      BrTextPrint("UNABLE TO SAVE GHOST CAR DATA.", 66, 110);
      BrTextPrint("GHOST CAR DATA IS LARGER THAN", 66, 124);
      BrTextPrint("THE MAXIMUM ALLOCATED SPACE", 66, 138);
      BrTextPrint("IN THE CONTROLLER PAK.", 66, 152);
      break;
    case 4:
      BrBevelPanel(0xBF, 0xD9, 0x102, 0x4A, 3, 0, 0, 0x80, 0x80, 0x80);
      BrTextPrint("CONTROLLER ERROR", 103, 124);
      BrTextPrint("HAS BEEN DETECTED!", 103, 138);
      break;
    case 5:
      if ((unusedCC = D_80272550) == 0 || D_80272550 == 2 || D_80272550 == 4) {
        BrBevelPanel(0xAD, 0xD9, 0x125, 0x4A, 3, 0, 0, 0x80, 0x80, 0x80);
        BrTextPrint("SAVED DATA NOT FOUND", 94, 124);
        BrTextPrint("IN CONTROLLER PAK.", 94, 138);
      } else if (unusedCC == 1 || unusedCC == 3 || unusedCC == 5) {
        BrBevelPanel(0xB6, 0xD9, 0x114, 0x4A, 3, 0, 0, 0x80, 0x80, 0x80);
        BrTextPrint("UNABLE TO SAVE DATA", 99, 124);
        BrTextPrint("TO CONTROLLER PAK.", 99, 138);
      }
      break;
    case 2:
      BrBevelPanel(0xAF, 0xBB, 0x122, 0x89, 3, 0, 0, 0x80, 0x80, 0x80);
      BrTextPrint("A NEW CONTROLLER PAK", 95, 109);
      BrTextPrint("WAS INSERTED.", 95, 122);
      BrTextPrint("THIS CONTROLLER PAK", 95, 141);
      BrTextPrint("WILL BE USED.", 95, 154);
      break;
    default:
      BrBevelPanel(0xB1, 0xD9, 0x11E, 0x4A, 3, 0, 0, 0x80, 0x80, 0x80);
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
      BrBevelPanel(0xE2, h, 0xBC, 0x4A, 3, 0, 0, 0x80, 0x80, 0x80);
      BrTextPrint("%ywSEASON DATA", 159, (h + 32) >> 1);
      BrTextPrint("%ywLOADED OK!", 159, ((h + 32) >> 1) + 14);
      break;
    case 2:
      if (D_802724F4 == 0) {
        h = 0xD9;
      } else {
        h = 0xCE;
      }
      BrBevelPanel(0xE2, h, 0xBC, 0x4A, 3, 0, 0, 0x80, 0x80, 0x80);
      BrTextPrint("%ywGHOST DATA", 159, (h + 32) >> 1);
      BrTextPrint("%ywLOADED OK!", 159, ((h + 32) >> 1) + 14);
      break;
    case 1:
      if (D_802724F4 == 0) {
        h = 0xD9;
      } else {
        h = 0xCE;
      }
      BrBevelPanel(0xE2, h, 0xBC, 0x4A, 3, 0, 0, 0x80, 0x80, 0x80);
      BrTextPrint("%ywSEASON DATA", 159, (h + 32) >> 1);
      BrTextPrint("%ywSAVED OK!", 159, ((h + 32) >> 1) + 14);
      break;
    case 3:
      if (D_802724F4 == 0) {
        h = 0xD9;
      } else {
        h = 0xCE;
      }
      BrBevelPanel(0xD8, h, 0xD0, 0x4A, 3, 0, 0, 0x80, 0x80, 0x80);
      BrTextPrint("%ywGHOST CAR DATA", 159, (h + 32) >> 1);
      BrTextPrint("%ywSAVED OK!", 159, ((h + 32) >> 1) + 14);
      break;
    case 4:
      BrBevelPanel(0xC3, 0xE7, 0xFA, 0x2E, 3, 0, 0, 0x80, 0x80, 0x80);
      BrTextPrint("%ywOPTIONS LOADED OK!", 159, 131);
      break;
    case 5:
      BrBevelPanel(0xC3, 0xE7, 0xFA, 0x2E, 3, 0, 0, 0x80, 0x80, 0x80);
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
    BrBevelPanel(0x72, 0xB2, 0x19C, 0x98, 3, 0, 0, 0x80, 0x80, 0x80);
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
      if (tgr_rumble_open(D_80272D48, &D_8031A3F8[D_80271FA8], D_80271FA8) == 0) {
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
    tgr_wr32(&g->words.w1, 0);
    tgr_wr32(&g->words.w0, 0xE7000000);
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
      BrModeSet(BrChampionshipStart);
    } else if (D_80272550 == 2 && D_80316422 != 0) {
      D_80316422 = 0;
      D_802A4BE8 = 1;
      BrModeSet(BrTimeAttackStart);
    } else {
      D_80272550 = 0;
      if (D_802724F4 == 0) {
        D_802A4BE8 = 1;
        BrModeSet(BrMainMenu);
      } else {
        mode = D_802724F4;
        if (D_802724F4 == 1) {
          D_802A4BE8 = 1;
          BrModeSet(BrTrackSelectScreen);
        } else if ((mode | 0) == 2) {
          D_802A4BE8 = 1;
          BrModeSet(BrMainMenu);
        }
      }
    }
  }
  D_802A4BE8 = 0;
}
#endif
