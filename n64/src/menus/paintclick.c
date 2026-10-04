/* paintclick.c -- the paint shop's click handling
 *
 * Its own object here: its strings and switch tables sit apart from the
 * other paint-shop functions'.
 */
#include "tgr/common.h"

/* -- declarations -- */
typedef unsigned long long u64;
u32 osGetCount(void);
extern u64 osClockRate;
float sqrtf(float x);
void osSyncPrintf(char *fmt, ...);
typedef struct BrPadRec {       /* as tgr/pad.h (0x15C bytes) */
  unsigned int pressed;
  unsigned int held;
  int repeat[4];
  float axis[2];
  char pad20[0x15c - 0x20];
} BrPadRec;
extern char D_8036A8E0;
#define PADS ((BrPadRec *)&D_8036A8E0)
typedef struct BrPaintRect { int x, y, w, h; } BrPaintRect;
typedef struct BrPaintState {   /* 0x8028D110 */
  char pad00[0x1c];
  int x;                        /* 0x1C  cursor */
  int y;                        /* 0x20 */
} BrPaintState;
typedef struct BrPaintBrush {   /* 0x8028D290 */
  char pad00[0x24];
  int w;                        /* 0x24 */
  int h;                        /* 0x28 */
} BrPaintBrush;
typedef struct BrGlyph {        /* a keyboard key (0x20) */
  int x0;
  int x4;
  char pad08[0x20 - 8];
} BrGlyph;
typedef struct BrClickSwatch {  /* a palette entry (0x14) */
  int x, y, w, h;
  unsigned char c[3];           /* 0x10 */
} BrClickSwatch;
typedef struct BrDecalPart {    /* a model part, as the decal code reads it (0x24) */
  unsigned char *tex;
  char pad04[8];
  unsigned short w;             /* 0x0C */
  unsigned short h;             /* 0x0E */
  char pad10[0x24 - 0x10];
} BrDecalPart;
typedef struct BrDecalModel {
  char pad00[0x14];
  BrDecalPart *parts;           /* 0x14 */
  char pad18[0x110 - 0x18];
  unsigned char decalPart[12];  /* 0x110  each decal slot's part */
  void **masks;                 /* 0x11C  each slot's paint mask */
} BrDecalModel;
extern BrDecalModel *D_8028AB08;        /* the current car's model */
extern BrPaintState D_8028D110;
extern BrPaintBrush D_8028D290;
extern BrGlyph D_8028D540[50];          /* the keyboard */
extern BrPaintRect D_8028D470;          /* the paint area's frame */
extern BrPaintRect D_8028D480;          /* the two scroll arrows */
extern BrPaintRect D_8028D490;
extern BrPaintRect D_8028D4A0[5];       /* the brush sizes */
extern BrPaintRect D_8028D4F0[5];       /* their buttons */
extern BrPaintRect D_8028DB94;          /* the paint area on screen */
extern BrPaintRect D_80369CD8[13];      /* the tool buttons */
extern BrPaintRect D_80369DB0[10];      /* the decal slot buttons */
extern BrClickSwatch D_80369B98[16];    /* the palette */
extern unsigned char D_80369DA8[3];     /* the colour before the mixer opened */
extern int D_80369B80;                  /* the drag anchor */
extern int D_80369B84;
extern unsigned int D_80369B88;         /* the last click, in milliseconds */
extern int D_80369B8C;                  /* the anchor has been set */
extern int D_80369B90;
typedef struct BrImage BrImage;
extern BrImage *D_8028DB08;             /* the slot's preview image */
extern BrImage *D_8028DB0C[10];
extern unsigned char D_8028CE9C;        /* the brush shape: 0 round */
extern unsigned char D_8028CF5C;        /* the rectangle style */
extern unsigned char D_8028CF8C;        /* the oval style */
extern unsigned char D_8028CFEC;
extern unsigned char D_8028DAC0;        /* the chosen brush size */
extern unsigned char D_8028DB54;        /* the palette colour before */
extern unsigned char D_8028DB58;        /* the chosen palette colour */
extern unsigned char D_8028DB60;        /* the tool */
extern unsigned char D_8028DB64;        /* the tool before */
extern unsigned char D_8028DB68;        /* the decal being painted */
extern unsigned char D_8028DB6C;        /* the decal before */
extern unsigned char *D_8028DB78;       /* the decal texture */
extern void *D_8028DB7C;                /* its paint mask */
extern unsigned char D_8028DB88;        /* texture width */
extern unsigned char D_8028DB8C;        /* texture height */
extern unsigned char D_8028DBA8;        /* letters typed */
extern unsigned char D_8028DBAC;
extern unsigned short D_8028DBB0;
extern unsigned char D_8028DBBC;        /* the controller */
extern unsigned char D_8028DBC0;        /* a stroke is under way */
extern unsigned char D_8028DBC4;        /* the keyboard is up */
extern unsigned char D_8028DBC8;        /* the palette is live */
extern unsigned char D_8028DBCC;        /* the decal has a mirror side */
extern unsigned char D_8028DBD0;        /* the view is turning */
extern unsigned char D_8028DBD4;        /* the colour mixer is open */
extern unsigned char D_8028DBE0;        /* a popup is open */
extern unsigned char D_8028DBE4;
extern unsigned char D_8028DBE8;
int BrPaintCursorInRect(BrPaintRect *r);
int BrPaintCursorInRect2(BrClickSwatch *r);
void BrPadConsume(unsigned int *pad, unsigned int bits);
void BrPaintDecalCommit(void);
void BrPaintDecalApply(void);
void BrPaintKeyboard(void);
void BrPaintTextDraw(int x, int y);
void BrPaintTextStamp(int x, int y);
void BrPaintPlot(int x, int y, unsigned char c);
void BrPaintDisc(int x, int y, int r, unsigned char screen);
void BrPaintCircle(int x, int y, int r);
void BrPaintFloodFill(int x, int y);
void BrPaintLine(int x0, int y0, int x1, int y1);
void BrPaintFillRect(int x0, int y0, int x1, int y1);
void BrPaintFrameRect(int x0, int y0, int x1, int y1);
void BrPaintFillRoundRect(int x0, int y0, int x1, int y1);
void BrPaintFrameRoundRect(int x0, int y0, int x1, int y1);
void BrPaintFillOval(int x0, int y0, int x1, int y1);
void BrPaintFrameOval(int x0, int y0, int x1, int y1);
void BrPaintDashLine(int x0, int y0, int x1, int y1);
void BrPaintDashRect(int x0, int y0, int x1, int y1);
void BrPaintDashRoundRect(int x0, int y0, int x1, int y1);
void BrPaintDashOval(int x0, int y0, int x1, int y1);
void BrPaintDashCircle(int x, int y, int r);
#define BR_MSEC() ((unsigned int)((((u64)osGetCount() * 1000000) / osClockRate) / 1000))
/* -- end declarations -- */

/* WHAT IT DOES: The paint shop's click handling, A or Z on the cursor.
 * A decal slot button picks the decal to paint (setting up its texture,
 * mask, size and place on screen, and turning the view to it); a palette
 * swatch picks the colour, and a second click within 350 ms opens the
 * mixer on it; a tool button picks the tool (a second quick click on the
 * brush, line, rectangle or oval tool opens its options popup), and the
 * others undo, clear, mirror or pick a brush size.  In the paint area the
 * tool acts: the brush paints a dot or a square, the fill floods, the line,
 * rectangle and oval take a first click as the anchor and a second to
 * draw, the text tool opens the keyboard and then stamps the text; between
 * the clicks a dashed outline follows the cursor.
 * RESIDUE (gap 85, same length): the ROM loads and stores the two anchor
 * flags without keeping their addresses in v0, tests the palette loop with
 * slti where ours gets bne, and its loop pointer temporaries sit one word
 * higher (0x1C/0x20). */
/* @t4-pass 0x8024C184 1 2026-10-03 compiles 26 best 525 moved 1  (n64/tools/n64permute.py) */
/* @t4-pass 0x8024C184 2 2026-10-03 compiles 26 best 525 moved 0  (n64/tools/n64permute.py) */
/* @implements 0x8024C184 tgr BrPaintClick */
void BrPaintClick(void)
{
  int j;
  int i;
  int w;
  int h;
  int r;
  unsigned int t;
  int k;
  int u1[5];                    /* unused: the frame has them */
  char clicked;

  if (D_80369B8C != 0) {
  } else {
    D_80369B8C = 1;
    D_80369B80 = D_8028D110.x;
  }
  if (D_80369B90 == 0) {
    D_80369B90 = 1;
    D_80369B84 = D_8028D110.y;
  }
  if (D_8028DBE8 != 0) {
    return;
  }
  if (D_8028DBC4 == 0 && D_8028DBD0 == 0 && (*(unsigned int *)(&D_8036A8E0 + D_8028DBBC * 0x15c) & 0x8010)) {
    for (i = 0; i < 10; i++) {
      if (BrPaintCursorInRect(&D_80369DB0[i])) {
        BrPadConsume((unsigned int *)(&D_8036A8E0 + D_8028DBBC * 0x15c), 0x8010);
        D_8028DB6C = D_8028DB68;
        D_8028DB68 = i;
        D_8028DB08 = D_8028DB0C[i];
        if (D_8028DB68 != D_8028DB6C) {
          D_8028DBD0 = 1;
          D_8028DBB0 = 0;
          D_8028DB78 = D_8028AB08->parts[D_8028AB08->decalPart[i]].tex;
          D_8028DB7C = D_8028AB08->masks[D_8028DB68];
          D_8028DB88 = D_8028AB08->parts[D_8028AB08->decalPart[i]].w;
          D_8028DB8C = D_8028AB08->parts[D_8028AB08->decalPart[i]].h;
          osSyncPrintf("decal_width = %d\n", D_8028DB88);
          osSyncPrintf("decal_height = %d\n", D_8028DB8C);
          w = D_8028DB88;
          h = D_8028DB8C;
          D_8028DB94.x = D_8028D470.x + ((D_8028D470.w - w * 4) >> 1);
          D_8028DB94.y = D_8028D470.y + ((D_8028D470.h - h * 4) >> 1);
          D_8028DB94.w = w * 4;
          D_8028DB94.h = h * 4;
        }
        if (D_8028DB68 == 4 || D_8028DB68 == 5 || D_8028DB68 == 8 || D_8028DB68 == 9) {
          D_8028DBCC = 0;
        } else {
          D_8028DBCC = 1;
        }
        return;
      }
    }
  }
  if ((BrPaintCursorInRect(&D_8028D480) || BrPaintCursorInRect(&D_8028D490)) && D_8028DBC8 != 0 &&
      (*(unsigned int *)(&D_8036A8E0 + D_8028DBBC * 0x15c) & 0x8010)) {
    for (j = 0; j < 16; j++) {
      if (BrPaintCursorInRect2(&D_80369B98[j])) {
        BrPadConsume((unsigned int *)(&D_8036A8E0 + D_8028DBBC * 0x15c), 0x8010);
        D_8028DB54 = D_8028DB58;
        D_8028DB58 = j;
        if (D_8028DB54 != j) {
          D_80369B88 = BR_MSEC();
          return;
        }
        t = BR_MSEC();
        if (t - D_80369B88 < 350) {
          for (k = 0; k < 3; k++) {
            D_80369DA8[k] = D_80369B98[D_8028DB58].c[k];
          }
          D_8028DBD4 = 1;
          return;
        }
        D_80369B88 = t;
        return;
      }
    }
  }
  if (D_8028DBC4 == 0) {
    for (i = 0; i < 13; i++) {
      if (BrPaintCursorInRect(&D_80369CD8[i]) && (*(unsigned int *)(&D_8036A8E0 + D_8028DBBC * 0x15c) & 0x8010)) {
        BrPadConsume((unsigned int *)(&D_8036A8E0 + D_8028DBBC * 0x15c), 0x8010);
        D_8028DBC0 = 0;
        if (i != 0) {
          D_8028DB64 = D_8028DB60;
          D_8028DB60 = i;
          if (D_8028DB60 != D_8028DB64) {
            if (D_8028DB60 == 5 || D_8028DB60 == 6 || D_8028DB60 == 1 || D_8028DB60 == 4) {
              D_80369B88 = BR_MSEC();
            }
          } else if (D_8028DB60 == 5 || D_8028DB60 == 6 || D_8028DB60 == 1 || D_8028DB60 == 4) {
            t = BR_MSEC();
            if (t - D_80369B88 < 350) {
              D_8028DBC0 = 1;
              D_8028DBE0 = 1;
              return;
            }
            D_80369B88 = t;
          }
        }
        switch (i) {
        case 0:
          for (j = 0; j < 5; j++) {
            if (BrPaintCursorInRect(&D_8028D4F0[j])) {
              D_8028DAC0 = j;
              return;
            }
          }
          return;
        case 7:
          D_8028DBC0 = 1;
          return;
        case 11:
          BrPaintDecalApply();
          D_8028DB60 = D_8028DB64;
          return;
        case 8:
          D_8028CFEC = 0;
          return;
        case 9:
        case 10:
          D_8028DBC0 = 0;
          return;
        case 12:
          return;
        }
        return;
      }
    }
  }
  if (D_8028DB60 == 4) {
    clicked = 0;
    if (BrPaintCursorInRect(&D_8028DB94) && D_8028DBC4 == 0 && (*(unsigned int *)(&D_8036A8E0 + D_8028DBBC * 0x15c) & 0x8010)) {
      BrPadConsume((unsigned int *)(&D_8036A8E0 + D_8028DBBC * 0x15c), 0x8010);
      D_80369B80 = D_8028D110.x;
      D_80369B84 = D_8028D110.y;
      clicked = 1;
      D_8028DBC4 = 1;
    }
    if (D_8028DBC4 != 0 && D_8028DBC0 == 0) {
      if (clicked) {
        D_8028D110.x = D_8028D540[21].x0 + (D_8028D290.w >> 1);
        D_8028D110.y = D_8028D540[21].x4 + (D_8028D290.h >> 1);
        D_8028DBB0 = 0;
      }
      BrPaintKeyboard();
      return;
    }
    if (D_8028DBC0 != 0 && !BrPaintCursorInRect(&D_8028D480) && !BrPaintCursorInRect(&D_8028D490) &&
        !(*(unsigned int *)(&D_8036A8E0 + D_8028DBBC * 0x15c) & 0x8010)) {
      if (D_8028DBA8 > 0) {
        if (D_8028DBE4 != 0) {
          D_8028D110.x = D_80369B80;
          D_8028DBE4 = 0;
          D_8028D110.y = D_80369B84;
        }
        BrPaintTextDraw(D_8028D110.x, D_8028D110.y + 1);
        return;
      }
      if (D_8028DBA8 == 0) {
        D_8028DBC0 = 0;
        D_8028DBC4 = 0;
      }
      return;
    }
    if (D_8028DBC0 != 0 && (*(unsigned int *)(&D_8036A8E0 + D_8028DBBC * 0x15c) & 0x8010)) {
      BrPadConsume((unsigned int *)(&D_8036A8E0 + D_8028DBBC * 0x15c), 0x8010);
      BrPaintDecalCommit();
      BrPaintTextStamp(D_8028D110.x, D_8028D110.y + 1);
      D_8028DBA8 = 0;
      D_8028DBAC = 0;
      D_8028DBC0 = 0;
      D_8028DBC4 = 0;
    }
  } else if (D_8028DB60 != 4 && BrPaintCursorInRect(&D_8028DB94) && (*(unsigned int *)(&D_8036A8E0 + D_8028DBBC * 0x15c) & 0x8010)) {
    switch (D_8028DB60) {
    case 1:
      if (D_8028DBC0 == 0) {
        BrPaintDecalCommit();
        D_8028DBC0 = 1;
      }
      if (D_8028DAC0 == 0) {
        BrPaintPlot((D_8028D110.x - D_8028DB94.x) >> 2, (D_8028DB94.y + D_8028DB94.h - D_8028D110.y) >> 2,
                    D_8028DB58);
      } else if (D_8028CE9C == 0) {
        BrPaintDisc(D_8028D110.x, D_8028D110.y, D_8028D4A0[D_8028DAC0].w >> 1, 1);
      } else {
        BrPaintFillRect(D_8028D110.x - D_8028D4A0[D_8028DAC0].w * 2, D_8028D110.y - D_8028D4A0[D_8028DAC0].w * 2,
                        D_8028D110.x + D_8028D4A0[D_8028DAC0].w * 2, D_8028D110.y + D_8028D4A0[D_8028DAC0].w * 2);
      }
      return;
    case 3:
      if (D_8028DBC0 == 0) {
        D_80369B80 = D_8028D110.x;
        D_80369B84 = D_8028D110.y;
        D_8028DBC0 = 1;
      } else {
        BrPaintDecalCommit();
        BrPaintLine(D_80369B80, D_80369B84, D_8028D110.x, D_8028D110.y);
        D_8028DBC0 = 0;
      }
      BrPadConsume((unsigned int *)(&D_8036A8E0 + D_8028DBBC * 0x15c), 0x8010);
      return;
    case 2:
      BrPaintDecalCommit();
      BrPaintFloodFill(D_8028D110.x, D_8028D110.y);
      BrPadConsume((unsigned int *)(&D_8036A8E0 + D_8028DBBC * 0x15c), 0x8010);
      return;
    case 5:
      if (D_8028DBC0 == 0) {
        D_80369B80 = D_8028D110.x;
        D_80369B84 = D_8028D110.y;
        D_8028DBC0 = 1;
      } else {
        BrPaintDecalCommit();
        if (D_8028CF5C == 0) {
          BrPaintFillRect(D_80369B80, D_80369B84, D_8028D110.x, D_8028D110.y);
        } else if (D_8028CF5C == 1) {
          BrPaintFrameRect(D_80369B80, D_80369B84, D_8028D110.x, D_8028D110.y);
        } else if (D_8028CF5C == 2) {
          BrPaintFillRoundRect(D_80369B80, D_80369B84, D_8028D110.x, D_8028D110.y);
        } else if (D_8028CF5C == 3) {
          BrPaintFrameRoundRect(D_80369B80, D_80369B84, D_8028D110.x, D_8028D110.y);
        }
        D_8028DBC0 = 0;
      }
      BrPadConsume((unsigned int *)(&D_8036A8E0 + D_8028DBBC * 0x15c), 0x8010);
      return;
    case 6:
      if (0 == D_8028DBC0) {
        D_80369B80 = D_8028D110.x;
        D_80369B84 = D_8028D110.y;
        D_8028DBC0 = 1;
      } else {
        BrPaintDecalCommit();
        if (D_8028CF8C == 2 || D_8028CF8C == 3) {
          r = (int)sqrtf((float)((D_8028D110.x - D_80369B80) * (D_8028D110.x - D_80369B80) +
                                 (D_8028D110.y - D_80369B84) * (D_8028D110.y - D_80369B84))) >> 2;
        }
        if (D_8028CF8C == 0) {
          BrPaintFillOval(D_80369B80, D_80369B84, D_8028D110.x, D_8028D110.y);
        } else if (D_8028CF8C == 1) {
          BrPaintFrameOval(D_80369B80, D_80369B84, D_8028D110.x, D_8028D110.y);
        } else if (D_8028CF8C == 2) {
          BrPaintDisc(D_80369B80, D_80369B84, r, 1);
        } else if (D_8028CF8C == 3) {
          BrPaintCircle(D_80369B80, D_80369B84, r);
        }
        D_8028DBC0 = 0;
      }
      BrPadConsume((unsigned int *)(&D_8036A8E0 + D_8028DBBC * 0x15c), 0x8010);
      return;
    }
  } else if (D_8028DB60 != 4 && D_8028DBC0 != 0 && BrPaintCursorInRect(&D_8028DB94) &&
             !(*(unsigned int *)(&D_8036A8E0 + D_8028DBBC * 0x15c) & 0x8010)) {
    switch (D_8028DB60) {
    case 1:
      D_8028DBC0 = 0;
      return;
    case 3:
      BrPaintDashLine(D_80369B80, D_80369B84, D_8028D110.x, D_8028D110.y);
      return;
    case 5:
      if (D_8028CF5C == 0 || D_8028CF5C == 1) {
        BrPaintDashRect(D_80369B80, D_80369B84, D_8028D110.x, D_8028D110.y);
        return;
      }
      BrPaintDashRoundRect(D_80369B80, D_80369B84, D_8028D110.x, D_8028D110.y);
      return;
    case 6:
      if (D_8028CF8C == 0 || D_8028CF8C == 1) {
        BrPaintDashOval(D_80369B80, D_80369B84, D_8028D110.x, D_8028D110.y);
        return;
      }
      BrPaintDashCircle(D_80369B80, D_80369B84,
                        (int)sqrtf((float)((D_8028D110.x - D_80369B80) * (D_8028D110.x - D_80369B80) +
                                           (D_8028D110.y - D_80369B84) * (D_8028D110.y - D_80369B84))));
      return;
    }
  }
}
