/* decalpak.c -- saving and loading the paint shop's decals and colour
 * palette on the Controller Pak
 */
#include "tgr/common.h"
#include "tgr/pad.h"

/* -- declarations -- */
#include "tgr/image.h"
extern BrImage D_8028D0B0;      /* the A button */
extern BrImage D_8028D0E0;      /* the B button */

typedef struct BrPaintPart {    /* a model part the paint shop edits (0x24 bytes) */
  be32_t tex;           /* unsigned char * -- 0x00  its texture, 4 bits a pixel */
  be32_t pal;          /* unsigned short * -- 0x04  its palette */
  char pad08[4];
  be16_t w;             /* 0x0C */
  be16_t h;             /* 0x0E */
  char pad10[0x24 - 0x10];
} BrPaintPart;
typedef struct BrPaintModel {   /* the car model being painted */
  char pad00[0x14];
  be32_t parts;           /* BrPaintPart * -- 0x14 */
  char pad18[0x110 - 0x18];
  unsigned char decal[10];      /* 0x110  the parts holding the ten decals */
  unsigned char paint[2];       /* 0x11A  the parts that share the palette */
  be32_t mask;         /* unsigned char ** -- 0x11C  per decal: the pixels it keeps, 0 for all */
} BrPaintModel;

extern OSPfs D_80369EC0[4];
extern OSPfs D_8031A3F8[4];
extern unsigned char D_803163E0[2][32];
extern unsigned char D_80316420[4];
extern unsigned char D_8031B1E8[4];
extern char D_80272D48[];
extern int D_80271FA8;

extern short D_802A4BE8;
extern unsigned char D_8028DCBC;  /* started */
extern unsigned char D_8028DCC0;  /* the file still has to be allocated */
extern int D_8028DCC4;          /* bytes transferred so far */
extern unsigned char D_80369E50;  /* the Rumble Pak was taken out for this */
extern unsigned char D_80369E51;  /* a new pak turned up mid-transfer */
extern int D_80369E54;          /* the last Controller Pak status */
extern int D_80369E58;          /* the file */
extern int D_80369E5C;          /* the size of the decal being moved */
extern unsigned char D_80369E60;  /* the state */
extern unsigned char D_80369E61;  /* the decal being moved */
extern unsigned char D_80369E62;  /* a frame counter */
extern unsigned char D_8028DB60;
extern unsigned char D_8028DB64;
extern unsigned char *D_8028DB80;  /* the transfer buffer */
extern unsigned char D_8028DBB4;

void BrSfxFadeTo(float level, float seconds);
void BrMusicFadeTo(float level, float seconds);
int BrSfxFadeDone(void);
void BrTextHighlightOff(void);
void BrTextAlignLeft(void);
void BrTextSetColours(int a, int b, int c, int d, int e, int f);
void BrTextSetFont(int font);
void BrTextPrint(char *s, int x, int y);
void BrImageDrawAt(BrImage *img, int x, int y);
void BrFillRect(int x, int y, int w, int h, unsigned char r, unsigned char g, unsigned char b);
void BrBevelPanel(int, int, int, int, int, char, char, unsigned char, unsigned char, unsigned char);
void BrPaintPaletteLoad(void);
void BrPaintDecalCommit(void);
void BrPadConsume(BrPadRec *pad, unsigned int bits);
/* -- end declarations -- */

#define COMPANY 0x3544          /* "5D" */
#define GAME 0x4E475245         /* "NGRE" */
#define OP_LOAD 9
#define PAD (&D_8036A8E0[port])
#define PFS (&D_80369EC0[port])

/* WHAT IT DOES: One frame of moving the paint shop's ten decals and its
 * colour palette to or from the Controller Pak in `port` (op 9 loads, any
 * other saves; fromMenu 1 when called from the car-select menus, which skip
 * the confirmation).  A state machine: confirm; fade the sound; probe the
 * pak (swap a Rumble Pak, remember a new pak's id); find the decal file (on a
 * save, confirm an overwrite or allocate it); move one decal a frame, each
 * masked by its keep-mask on a load, with a progress bar; move the palette
 * and share it with the other painted parts; offer the Rumble Pak back; then
 * report.  Sets *done when the caller may leave; returns the pak status on
 * an error.  (Run by the box with an empty pak: save, load, overwrite; the
 * pak errors, a new pak and the Rumble Pak swap are not reached.) */
/* @t4-pass 0x80248F88 1 2026-09-28 compiles 21 best 1862 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x80248F88 2 2026-09-28 compiles 21 best 1862 moved 0  (n64/tools/n64permute.py) */
/* @t3 0x80248F88 */
/* @implements 0x80248F88 tgr BrDecalPakTransfer */
int BrDecalPakTransfer(BrPaintModel *m, unsigned char port, char op, char fromMenu,
                       unsigned char *done)
{
  unsigned char name[16] = {
    0x2D, 0x28, 0x29, 0x0F, 0x20, 0x1E, 0x1A, 0x2B, 0x0F, 0x1D, 0x1E, 0x1C, 0x1A, 0x25, 0x2C, 0x00
  };
  unsigned char ext[4] = { 0, 0, 0, 0 };
  unsigned char pakBits;
  unsigned int w;
  unsigned int h;
  int freeBytes;
  int y;
  int ty;
  int top;
  unsigned int row;
  unsigned int col;
  unsigned char *tex;
  unsigned char *mask;
  int stride;
  int half;
  int k;
  int flip;
  BrPaintPart *part;
  OSPfs *pfs;

  D_802A4BE8 = 0;
  if (D_8028DCBC == 0) {
    D_8028DCBC = 1;
    D_80369E50 = 0;
    D_80369E51 = 0;
    D_80369E54 = 0;
    D_80369E61 = 0;
    D_8028DCC4 = 0;
    D_80369E5C = 0;
    D_80369E62 = 0;
    if (fromMenu == 1) {
      D_80369E60 = 0;
    } else {
      D_80369E60 = 2;
    }
  }
  *done = 0;
  switch (D_80369E60) {
  case 0:
    BrSfxFadeTo(0.0f, 0.2f);
    BrMusicFadeTo(0.0f, 0.2f);
    D_80369E60 = 1;
    break;
  case 1:
    if (BrSfxFadeDone() != 0 && D_80369E62++ == 3) {
      D_80369E60 = 3;
      D_80369E62 = 0;
    }
    break;
  case 2:                       /* load/save? */
    y = 0x110 - D_8028D0B0.w;
    BrBevelPanel(0xDC, 200, 200, 0x50, 3, 0, 0, 0x80, 0x80, 0x80);
    BrTextAlignLeft();
    BrTextHighlightOff();
    BrTextSetFont(14);
    BrTextSetColours(0xFF, 0xFF, 0xFF, 0xFF, 0xCA, 0);
    if (op == OP_LOAD) {
      BrTextPrint("LOAD DECALS?", 119, 116);
    } else {
      BrTextPrint("SAVE DECALS?", 119, 116);
    }
    BrTextSetFont(10);
    BrTextPrint("%wwOK", (D_8028D0B0.w + 0xF2U) >> 1, (y + 18) >> 1);
    BrTextPrint("%wwCANCEL", (D_8028D0E0.w + 0x136U) >> 1, (y + 18) >> 1);
    BrImageDrawAt(&D_8028D0B0, 0xEC, y);
    BrImageDrawAt(&D_8028D0E0, 0x130, y);
    if (PAD->pressed & 0x10) {
      BrPadConsume(PAD, 0x10);
      D_80369E60 = 0;
    } else {
      if (PAD->pressed & 0x20) {
        BrPadConsume(PAD, 0x20);
        *done = 1;
        D_8028DB60 = D_8028DB64;
      }
      return 0;
    }
    break;
  case 3:                       /* probe the pak */
    osPfsIsPlug(D_80272D48, &pakBits);
    if ((pakBits & (1 << port)) == 0) {
      D_80369E54 = 1;
      D_80369E60 = 12;
    } else {
      osSyncPrintf("\nInitializing controller pak...\n");
      D_80369E54 = osPfsInitPak(D_80272D48, PFS, port);
      if (D_80369E54 == 0) {
        D_8031B1E8[D_80271FA8] = 0;
        if (D_80316420[port] == 0) {
          D_80316420[port] = 1;
          memcpy(D_803163E0[port], D_80369EC0[port].id, 32);
          D_80369E60 = 6;
        } else if (bcmp(D_803163E0[port], D_80369EC0[port].id, 32) == 0) {
          D_80369E60 = 6;
        } else {
          memcpy(D_803163E0[port], D_80369EC0[port].id, 32);
          D_80369E60 = 9;
        }
      } else if (D_80369E54 == 10) {
        if (osMotorInit(D_80272D48, &D_8031A3F8[port], port) == 0) {
          osMotorStop(&D_8031A3F8[port]);
          D_80369E54 = 9999;
          D_80369E50 = 1;
          D_80369E60 = 10;
        } else {
          D_8031B1E8[D_80271FA8] = 0;
          D_80369E54 = 0;
          D_80369E60 = 4;
        }
      } else {
        D_8031B1E8[D_80271FA8] = 0;
        D_80369E60 = 12;
      }
    }
    break;
  case 4:
    D_80369E54 = osPfsRepairId(PFS);
    if (D_80369E54 == 0) {
      D_80369E60 = 3;
    } else {
      D_80369E60 = 10;
    }
    break;
  case 5:                       /* overwrite? */
    y = 0x11D - D_8028D0B0.w;
    BrBevelPanel(0xD6, 0xBB, 0xD4, 0x6A, 3, 0, 0, 0x80, 0x80, 0x80);
    BrTextAlignLeft();
    BrTextHighlightOff();
    BrTextSetFont(13);
    BrTextSetColours(0xFF, 0xFF, 0xFF, 0xFF, 0xCA, 0);
    BrTextPrint("OK TO OVERWRITE", 114, 108);
    BrTextPrint("SAVED DECALS?", 114, 122);
    BrTextSetFont(10);
    BrTextPrint("%wwOK", (D_8028D0B0.w + 0xEEU) >> 1, (y + 18) >> 1);
    BrTextPrint("%wwCANCEL", (D_8028D0E0.h + 0x13AU) >> 1, (y + 18) >> 1);
    BrImageDrawAt(&D_8028D0B0, 0xE8, y);
    BrImageDrawAt(&D_8028D0E0, 0x134, y);
    if (PAD->pressed & 0x10) {
      BrPadConsume(PAD, 0x10);
      D_80369E60 = 7;
    } else {
      if ((PAD->pressed & 0x20) == 0) {
        return 0;
      }
      BrPadConsume(PAD, 0x20);
      D_80369E60 = 12;
    }
    break;
  case 6:                       /* find the file */
    D_8028DCC4 = 0;
    D_80369E5C = 0;
    D_80369E61 = 0;
    D_8028DCC0 = 0;
    pfs = PFS;
    D_80369E62 = 0;
    D_80369E54 = osPfsFindFile(pfs, COMPANY, GAME, name, ext, &D_80369E58);
    if (D_80369E54 == 0) {
      if (op == OP_LOAD) {
        top = 0xC5;
        if (fromMenu == 1) {
          top = 0xDB;
        }
        BrBevelPanel(0xD4, top, 0xD8, 0x56, 3, 0, 0, 0x80, 0x80, 0x80);
        BrTextAlignLeft();
        BrTextHighlightOff();
        BrTextSetFont(13);
        BrTextSetColours(0xFF, 0xFF, 0xFF, 0xFF, 0xCA, 0);
        BrTextPrint("LOADING DECALS...", 114, (top + 30) >> 1);
        BrBevelPanel(0xE6, top + 0x34, 0xB4, 0x14, 1, 1, 1, 0x80, 0x80, 0x80);
        D_80369E60 = 7;
      } else {
        D_80369E60 = 5;
      }
    } else if (D_80369E54 == 5) {
      if (op == OP_LOAD) {
        D_80369E60 = 12;
      } else if (osPfsFreeBlocks(pfs, &freeBytes) == 0) {
        if (freeBytes < 0x3E00) {
          D_80369E54 = 7;
          D_80369E60 = 12;
        } else {
          BrBevelPanel(0xD4, 0xC5, 0xD8, 0x56, 3, 0, 0, 0x80, 0x80, 0x80);
          BrTextAlignLeft();
          BrTextHighlightOff();
          BrTextSetFont(13);
          BrTextSetColours(0xFF, 0xFF, 0xFF, 0xFF, 0xCA, 0);
          BrTextPrint("SAVING DECALS...", 115, 113);
          BrBevelPanel(0xE6, 0xF9, 0xB4, 0x14, 1, 1, 1, 0x80, 0x80, 0x80);
          D_8028DCC0 = 1;
          D_80369E60 = 7;
          D_80369E62 = 0;
        }
      } else {
        D_80369E54 = 4;
        D_80369E60 = 12;
      }
    } else if (D_80369E54 == 3) {
      D_80369E54 = osPfsChecker(pfs);
      if (D_80369E54 == 0) {
        D_80369E60 = 6;
      } else {
        D_80369E60 = 12;
      }
    } else if (D_80369E54 == 2) {
      D_80369E60 = 3;
      D_80369E54 = 0;
    } else {
      D_80369E60 = 12;
    }
    break;
  case 7:                       /* one decal a frame */
    top = 0xC5;
    ty = 0x71;
    if (fromMenu == 1) {
      top = 0xDB;
      ty = 0x7C;
    }
    BrBevelPanel(0xD4, top, 0xD8, 0x56, 3, 0, 0, 0x80, 0x80, 0x80);
    BrTextAlignLeft();
    BrTextHighlightOff();
    BrTextSetFont(13);
    BrTextSetColours(0xFF, 0xFF, 0xFF, 0xFF, 0xCA, 0);
    if (op == OP_LOAD) {
      BrTextPrint("LOADING DECALS...", 114, ty);
    } else {
      BrTextPrint("SAVING DECALS...", 115, ty);
    }
    BrBevelPanel(0xE6, top + 0x34, 0xB4, 0x14, 1, 1, 1, 0x80, 0x80, 0x80);
    if (D_80369E62++ > 1) {
      if (D_8028DCC0 != 0) {
        pfs = PFS;
        D_8028DCC0 = 0;
        D_80369E54 = osPfsAllocateFile(pfs, COMPANY, GAME, name, ext, 0x3E00, &D_80369E58);
        switch (D_80369E54) {
        case 2:
          D_80369E60 = 3;
          break;
        case 3:
          D_80369E54 = osPfsChecker(pfs);
          if (D_80369E54 != 0) {
            D_80369E60 = 2;
            return D_80369E54;
          }
          D_80369E60 = 3;
          return 0;
        case 5:
        case 9:
          D_80369E54 = 3;
          break;
        }
        if (D_80369E54 != 0) {
          if (D_80369E54 != 2) {
            D_80369E60 = 12;
            return 0;
          }
          D_80369E54 = 0;
          D_80369E60 = 3;
          *done = 0;
          return 0;
        }
      }
      if (D_80369E61 < 10) {
        part = &BEPTR(BrPaintPart *, m->parts)[m->decal[D_80369E61]];
        w = BE16(part->w);
        h = BE16(part->h);
        pfs = PFS;
        if (D_80369E61 == 0 || D_80369E61 == 1) {
          h++;
        }
        D_80369E5C = (int)(w * h) >> 1;
        if (op == OP_LOAD) {
          D_80369E54 = osPfsReadWriteFile(pfs, D_80369E58, 0, D_8028DCC4, D_80369E5C, D_8028DB80);
        } else {
          D_80369E54 = osPfsReadWriteFile(pfs, D_80369E58, 1, D_8028DCC4, D_80369E5C, BEPTR(unsigned char *, part->tex));
        }
        if (D_80369E54 == 0) {
          if (op == OP_LOAD) {
            osSyncPrintf("Loading decal %d...\n", D_80369E61);
            tex = BEPTR(unsigned char *, BEPTR(BrPaintPart *, m->parts)[m->decal[D_80369E61]].tex);
            mask = TGR_PTR(unsigned char *, tgr_rd32(BEPTR(be32_t *, m->mask) + D_80369E61));  /* a table of
                                                   original addresses, big-endian */
            if (mask == 0) {
              memcpy(tex, D_8028DB80, D_80369E5C);
            } else {
              /* keep the pixels the mask marks, take the rest from the pak */
              for (row = 0; row != h; row++) {
                stride = (int)(w + 7) >> 3;
                half = (int)w >> 1;
                flip = (row & 1) << 3;
                for (col = 0; col != w; col++) {
                  if ((mask[row * stride + ((int)col >> 3)] & (1 << ((col ^ 7) & 7))) == 0) {
                    k = ((int)(flip ^ col) >> 1) + row * half;
                    tex[k] = D_8028DB80[k];
                  }
                }
              }
            }
          } else {
            osSyncPrintf("Saving decal %d...\n", D_80369E61);
          }
          D_8028DCC4 += D_80369E5C;
        } else if (D_80369E54 == 3) {
          D_80369E54 = osPfsChecker(pfs);
          if (D_80369E54 == 1 || D_80369E54 == 4 || D_80369E54 == 3) {
            D_80369E61 = 0;
            D_8028DCC4 = 0;
            D_80369E5C = 0;
            D_80369E60 = 12;
          } else {
            D_80369E61--;
            D_8028DCC4 -= D_80369E5C;
            D_80369E54 = 0;
          }
        } else if (D_80369E54 == 5) {
          D_80369E54 = 99999;
        }
        if (D_80369E54 == 0) {
          BrFillRect(0xE7, top + 0x35, D_8028DCC4 * 0xB2 / 0x3D00, 0x12, 0, 0, 0xC0);
          D_80369E61++;
        } else {
          if (D_80369E54 == 2) {
            D_80369E54 = 0;
            D_80369E60 = 9;
            D_80369E51 = 1;
            *done = 0;
            return 0;
          }
          D_80369E60 = 12;
        }
      } else {
        D_80369E61 = 0;
        BrFillRect(0xE7, top + 0x35, 0xB2, 0x12, 0, 0, 0xC0);
        D_80369E60 = 8;
      }
    }
    break;
  case 8:                       /* the palette */
    if (op == OP_LOAD) {
      osSyncPrintf("Loading color palette...\n");
      pfs = PFS;
      D_80369E54 = osPfsReadWriteFile(pfs, D_80369E58, 0, D_8028DCC4, 0x20,
                                      (unsigned char *)BEPTR(unsigned short *, BEPTR(BrPaintPart *, m->parts)[m->decal[2]].pal));
      if (D_80369E54 == 0) {
        BEPTR(unsigned short *, BEPTR(BrPaintPart *, m->parts)[m->paint[0]].pal)[0] = BEPTR(unsigned short *, BEPTR(BrPaintPart *, m->parts)[m->decal[2]].pal)[0];
        BEPTR(unsigned short *, BEPTR(BrPaintPart *, m->parts)[m->paint[0]].pal)[1] = BEPTR(unsigned short *, BEPTR(BrPaintPart *, m->parts)[m->decal[2]].pal)[1];
        BEPTR(unsigned short *, BEPTR(BrPaintPart *, m->parts)[m->paint[1]].pal)[0] = BEPTR(unsigned short *, BEPTR(BrPaintPart *, m->parts)[m->decal[2]].pal)[0];
        BEPTR(unsigned short *, BEPTR(BrPaintPart *, m->parts)[m->paint[1]].pal)[1] = BEPTR(unsigned short *, BEPTR(BrPaintPart *, m->parts)[m->decal[2]].pal)[1];
        if (fromMenu == 0) {
          BrPaintPaletteLoad();
          BrPaintDecalCommit();
        }
      }
    } else {
      osSyncPrintf("Saving color palette...\n");
      pfs = PFS;
      D_80369E54 = osPfsReadWriteFile(pfs, D_80369E58, 1, D_8028DCC4, 0x20,
                                      (unsigned char *)BEPTR(unsigned short *, BEPTR(BrPaintPart *, m->parts)[m->decal[2]].pal));
    }
    if (D_80369E54 == 0) {
      if (op == OP_LOAD) {
        osSyncPrintf("Color palette loaded successfully!\n\n");
      } else {
        osSyncPrintf("Color palette saved successfully!\n\n");
      }
      D_80369E60 = 11;
      if (fromMenu == 0) {
        D_8028DBB4 = 0;
      }
    } else if (D_80369E54 == 3) {
      D_80369E54 = osPfsChecker(pfs);
      if (D_80369E54 == 1 || D_80369E54 == 4 || D_80369E54 == 3) {
        D_80369E60 = 12;
      } else {
        D_8028DCC4 -= D_80369E5C;
        D_80369E54 = 0;
        D_80369E60 = 8;
      }
    }
    if (D_80369E54 == 0) {
      return 0;
    }
    osSyncPrintf("err_code = %d\n\n", D_80369E54);
    if (D_80369E54 == 2) {
      D_80369E60 = 9;
      D_80369E51 = 1;
      *done = 0;
      D_80369E54 = 0;
      return 0;
    }
    D_80369E60 = 12;
    *done = 0;
    break;
  case 9:                       /* a different pak */
    top = 0xAB;
    if (fromMenu == 1) {
      top = 0xD3;
    }
    BrTextHighlightOff();
    BrTextAlignLeft();
    BrTextSetFont(12);
    BrBevelPanel(0xAF, top, 0x122, 0x89, 3, 0, 0, 0x80, 0x80, 0x80);
    if (fromMenu == 1) {
      BrTextSetColours(0xFF, 0xFF, 0xFF, 0xFF, 0xF5, 0);
    } else {
      BrTextSetColours(0xFF, 0xFF, 0xFF, 0xFF, 0xCA, 0);
    }
    BrTextPrint("A NEW CONTROLLER PAK", 95, (top + 32) >> 1);
    BrTextPrint("WAS INSERTED.", 95, ((top + 32) >> 1) + 13);
    BrTextPrint("THIS CONTROLLER PAK", 95, ((top + 32) >> 1) + 32);
    BrTextPrint("WILL BE USED.", 95, ((top + 32) >> 1) + 45);
    if ((PAD->pressed & 0x8030) == 0) {
      return 0;
    }
    BrPadConsume(PAD, 0x8030);
    if (D_80369E51 == 0) {
      D_80369E60 = 6;
    } else {
      D_80369E51 = 0;
      D_80369E60 = 3;
    }
    break;
  case 10:                      /* a Rumble Pak is in the way, or an error */
    BrTextHighlightOff();
    BrTextAlignLeft();
    BrTextSetFont(12);
    if (fromMenu == 1) {
      BrTextSetColours(0xFF, 0xFF, 0xFF, 0xFF, 0xF5, 0);
    } else {
      BrTextSetColours(0xFF, 0xFF, 0xFF, 0xFF, 0xCA, 0);
    }
    if (D_80369E54 == 9999) {
      top = 0xA0;
      if (fromMenu == 1) {
        top = 200;
      }
      y = top - D_8028D0B0.w + 0x98;
      BrBevelPanel(0x7A, top, 0x18C, 0xA0, 3, 0, 0, 0x80, 0x80, 0x80);
      BrTextPrint("PLEASE REMOVE THE RUMBLE PAK", 69, (top + 32) >> 1);
      BrTextPrint("AND INSERT THE CONTROLLER PAK", 69, ((top + 32) >> 1) + 13);
      BrTextPrint("INTO CONTROLLER.  PRESS THE A", 69, ((top + 32) >> 1) + 26);
      BrTextPrint("BUTTON WHEN READY.", 69, ((top + 32) >> 1) + 39);
      BrTextSetFont(10);
      BrTextPrint("%wwOK", (D_8028D0B0.w + 0xDEU) >> 1, (y + 18) >> 1);
      BrTextPrint("%wwCANCEL", (D_8028D0E0.w + 0x14AU) >> 1, (y + 18) >> 1);
      BrImageDrawAt(&D_8028D0B0, 0xD8, y);
      BrImageDrawAt(&D_8028D0E0, 0x144, y);
    }
    if (D_80369E54 == 9999) {
      if (PAD->pressed & 0x10) {
        BrPadConsume(PAD, 0x10);
        D_80369E54 = 0;
        D_80369E60 = 3;
      } else {
        if ((PAD->pressed & 0x20) == 0) {
          return 0;
        }
        BrPadConsume(PAD, 0x20);
        D_80369E54 = 0;
        D_80369E60 = 12;
        if (fromMenu == 1) {
          D_80369E54 = 999;
        }
      }
    } else {
      if ((PAD->pressed & 0x8030) == 0) {
        return 0;
      }
      BrPadConsume(PAD, 0x8030);
      D_80369E60 = 12;
    }
    break;
  case 11:                      /* put the Rumble Pak back */
    D_80369E61 = 0;
    D_8028DCC4 = 0;
    D_80369E5C = 0;
    D_80369E62 = 0;
    if (fromMenu == 0) {
      D_80369E60 = 12;
    } else if (D_80369E50 == 0) {
      D_80369E60 = 12;
    } else {
      BrBevelPanel(0x73, 0xCC, 0x19A, 0x98, 3, 0, 0, 0x80, 0x80, 0x80);
      BrTextHighlightOff();
      BrTextAlignLeft();
      BrTextSetFont(12);
      BrTextSetColours(0xFF, 0xFF, 0xFF, 0xFF, 0xF5, 0);
      BrTextPrint("IF THE RUMBLE PAK IS TO BE USED,", 65, 118);
      BrTextPrint("REMOVE THE CONTROLLER PAK AND", 65, 131);
      BrTextPrint("INSERT THE RUMBLE PAK INTO THE", 65, 144);
      BrTextPrint("CONTROLLER.  PRESS THE A BUTTON", 65, 157);
      BrTextPrint("TO CONTINUE.", 65, 170);
      if ((PAD->pressed & 0x8010) == 0) {
        return 0;
      }
      BrPadConsume(PAD, 0x8010);
      D_80369E50 = 0;
      D_80369E54 = 0;
      if (osMotorInit(D_80272D48, &D_8031A3F8[port], port) == 0) {
        D_8031B1E8[port] = 1;
      }
      D_80369E60 = 12;
    }
    break;
  case 12:                      /* done */
    D_8028DCBC = 0;
    BrSfxFadeTo(1.0f, 0.2f);
    BrMusicFadeTo(1.0f, 0.2f);
    D_802A4BE8 = 1;
    if (D_80369E54 == 0) {
      *done = 1;
      if (fromMenu == 0) {
        D_8028DB60 = D_8028DB64;
      }
      return 0;
    }
    if (fromMenu == 0 && (PAD->pressed & 0x8030) != 0) {
      BrPadConsume(PAD, 0x8030);
      D_8028DB60 = D_8028DB64;
    }
    *done = 0;
    return D_80369E54;
  }
  return 0;
}
