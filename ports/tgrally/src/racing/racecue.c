/* racecue.c -- the scrolling text lines (y, font, gap, text) laid out from
 * the bottom of the screen and scrolled up a line of pixels a frame
 */
#include "tgr/common.h"

/* -- declarations -- */
typedef struct BrRaceCue {      /* one scrolling text line, 0x10 bytes; the table ends at a 0 text */
  int start;                    /* 0x00  its y on screen */
  int len;                      /* 0x04  its font size */
  int x8;                       /* 0x08  the gap after it */
  TgrAddr next;                   /* char * -- 0x0C  its text */
} BrRaceCue;
extern BrRaceCue D_80270194[95];       /* 94 lines and the end */
extern TgrAddr D_802701A0;          /* (char *) the first line's text: 0 when the table is empty */
typedef struct BrViewRect { int x; int y; int w; int h; int x10; } BrViewRect;
extern BrViewRect D_8031B2C8[];
void BrTextSetColours(int a, int b, int c, int d, int e, int f);
void BrTextHighlightOff(void);
void BrTextAlignCentre(void);
void BrScissorSet(int x, int y, int w, int h);
void BrTextSetFont(int font);
void BrTextPrint(char *s, int x, int y);
/* -- end declarations -- */

/* WHAT IT DOES: Lay the race-start cues out in time from frame 240: each
 * cue starts three quarters of the way through the previous one's length
 * plus its own gap. */
/* @implements 0x802006C8 tgr BrRaceCueLayout */
void BrRaceCueLayout(void)
{
  BrRaceCue *p;
  int t;
  int d;
  int n;

  t = 240;
  if (D_802701A0 != 0) {
    p = D_80270194;
    do {
      n = p->len;
      d = n * 3 / 4;
      t += d;
      p->start = t;
      t = t + n - d + p->x8;
      p++;
    } while (p->next != 0);
  }
}

/* WHAT IT DOES: Step every race-start cue's start time back one frame.  The
 * PC twin (br_racebegin.c) has the same shape. */
/* @implements 0x8020072C tgr BrRaceCueRewind */
void BrRaceCueRewind(void)
{
  BrRaceCue *p;

  if (D_802701A0 == 0) {
    return;
  }
  p = D_80270194;
  do {
    p->start--;
    p++;
  } while (p->next != 0);
}

/* WHAT IT DOES: Draw the scrolling text lines that are on screen (y from
 * -79 to 279), white and centred, clipped to the first view. */
/* @implements 0x80200764 tgr BrRaceCueDraw */
void BrRaceCueDraw(void)
{
  BrRaceCue *p;

  BrTextSetColours(0xff, 0xff, 0xff, 0xff, 0xff, 0xff);
  BrTextHighlightOff();
  BrTextAlignCentre();
  BrScissorSet(D_8031B2C8[0].x, D_8031B2C8[0].y, D_8031B2C8[0].w, D_8031B2C8[0].h);
  if (D_802701A0 != 0) {
    p = D_80270194;
    do {
      if (p->start >= -0x4f && p->start < 0x118) {
        BrTextSetFont(p->len);
        BrTextPrint(TGR_PTR(char *, p->next), 0xa0, p->start);
      }
      p++;
    } while (p->next != 0);
  }
}
