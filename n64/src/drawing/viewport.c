/* viewport.c -- the viewport ring (32 Vp at 0x80318CD0, one taken per view)
 * and the depth scale it implies
 */
#include "tgr/common.h"
#include "tgr/gbi.h"

/* -- declarations -- */
extern Gfx *D_8028A858;
extern int D_8028AB60;
extern int D_8028AB64;
extern int D_8028AB68;
extern Vp D_80318CD0[32];
extern int D_8028A850;
extern int D_8028A8A8;
extern int D_8028A8AC;
void BrScissorSet(int x, int y, int w, int h);
/* -- end declarations -- */

/* WHAT IT DOES: Take the next viewport in the ring and set it to a w by h
 * view at (x, y) (320-wide coordinates, doubled on a hi-res screen),
 * optionally scissoring to it.  A negative width mirrors the view (the
 * mirror flag is kept, and a second flag flips the sign again); load it and
 * keep its depth scale and offset. */
/* @implements 0x80219BF0 tgr BrViewportSet */
void BrViewportSet(int x, int y, int w, int h, int scissor)
{
  D_8028AB60 = (D_8028AB60 + 1) & 0x1f;
  if (scissor != 0) {
    BrScissorSet(x, y, w < 0 ? -w : w, h);
  }
  if (D_8028A850 != 0) {
    x *= 2;
    y *= 2;
    w *= 2;
    h *= 2;
  }
  if (w < 0) {
    if (D_8028A8AC != 0) {
      D_80318CD0[D_8028AB60].vp.vscale[0] = w * -2;
    } else {
      D_80318CD0[D_8028AB60].vp.vscale[0] = w * 2;
    }
    w = -w;
    D_8028A8A8 = 1;
  } else {
    if (D_8028A8AC != 0) {
      D_80318CD0[D_8028AB60].vp.vscale[0] = w * -2;
    } else {
      D_80318CD0[D_8028AB60].vp.vscale[0] = w * 2;
    }
    D_8028A8A8 = 0;
  }
  D_80318CD0[D_8028AB60].vp.vscale[1] = h * 2;
  D_80318CD0[D_8028AB60].vp.vscale[2] = 0x1ff;
  D_80318CD0[D_8028AB60].vp.vscale[3] = 0;
  D_80318CD0[D_8028AB60].vp.vtrans[0] = (x * 2 + w) * 2;
  D_80318CD0[D_8028AB60].vp.vtrans[1] = (y * 2 + h) * 2;
  D_80318CD0[D_8028AB60].vp.vtrans[2] = 0x1ff;
  D_80318CD0[D_8028AB60].vp.vtrans[3] = 0;
  gSPViewport(D_8028A858++, &D_80318CD0[D_8028AB60]);
  D_8028AB64 = D_80318CD0[D_8028AB60].vp.vscale[2];
  D_8028AB68 = D_80318CD0[D_8028AB60].vp.vtrans[2];
}

/* WHAT IT DOES: Take the next viewport in the ring and set it to the
 * whole 320x240 screen (optionally syncing and scissoring to x, y, w, h
 * first); load it, clear the mirror flag and keep its depth scale and
 * offset. */
/* @implements 0x80219DF0 tgr BrViewportFull */
void BrViewportFull(int x, int y, int w, int h, int scissor)
{
  D_8028AB60 = (D_8028AB60 + 1) & 0x1f;
  if (scissor != 0) {
    gDPPipeSync(D_8028A858++);
    BrScissorSet(x, y, w < 0 ? -w : w, h);
  }
  D_8028A8A8 = 0;
  D_80318CD0[D_8028AB60].vp.vscale[0] = 0x280;
  D_80318CD0[D_8028AB60].vp.vscale[1] = 0x1e0;
  D_80318CD0[D_8028AB60].vp.vscale[2] = 0x1ff;
  D_80318CD0[D_8028AB60].vp.vscale[3] = 0;
  D_80318CD0[D_8028AB60].vp.vtrans[0] = 0x280;
  D_80318CD0[D_8028AB60].vp.vtrans[1] = 0x1e0;
  D_80318CD0[D_8028AB60].vp.vtrans[2] = 0x1ff;
  D_80318CD0[D_8028AB60].vp.vtrans[3] = 0;
  gSPViewport(D_8028A858++, &D_80318CD0[D_8028AB60]);
  D_8028AB64 = D_80318CD0[D_8028AB60].vp.vscale[2];
  D_8028AB68 = D_80318CD0[D_8028AB60].vp.vtrans[2];
}

/* WHAT IT DOES: Load the current viewport into the display list and keep
 * its depth scale and offset for turning depths into z-buffer values. */
/* @implements 0x80219F04 tgr BrViewportApply */
void BrViewportApply(void)
{
  gSPViewport(D_8028A858++, &D_80318CD0[D_8028AB60]);
  D_8028AB64 = D_80318CD0[D_8028AB60].vp.vscale[2];
  D_8028AB68 = D_80318CD0[D_8028AB60].vp.vtrans[2];
}

/* WHAT IT DOES: Draw a one-pixel outline round the w by h rectangle at
 * (x, y): one-cycle mode, the fog colour set to 0xFF, then the top, bottom,
 * left and right edges as fill rectangles.  The three constant commands
 * are written out over several lines (the one-line macros store w1 first). */
/* @implements 0x80219F6C tgr BrViewOutline */
void BrViewOutline(int x, int y, int w, int h)
{
  gDPPipeSync(D_8028A858++);
  {
    Gfx *_g = D_8028A858++;
    _g->words.w0 = 0xb900031d;         /* render mode */
    _g->words.w1 = 0x55004240;
  }
  {
    Gfx *_g = D_8028A858++;
    _g->words.w0 = 0xba001402;         /* cycle type: one cycle */
    _g->words.w1 = 0;
  }
  {
    Gfx *_g = D_8028A858++;
    _g->words.w0 = 0xf8000000;         /* fog colour */
    _g->words.w1 = 0xff;
  }
  gDPFillRectangle(D_8028A858++, x, y, x + w, y + 1);
  gDPFillRectangle(D_8028A858++, x, y + h - 1, x + w, y + h);
  gDPFillRectangle(D_8028A858++, x, y, x + 1, y + h);
  gDPFillRectangle(D_8028A858++, x + w - 1, y, x + w, y + h);
}

/* WHAT IT DOES: Frame a view (the rear-view mirror) in three rings: a black
 * two-pixel border in fill mode just outside it, the one-pixel outline on
 * its edge (as BrViewOutline) and a one-pixel line two further out. */
/* @implements 0x8021A0F8 tgr BrViewFrame */
void BrViewFrame(int x, int y, int w, int h)
{
  Gfx *g;

  gDPPipeSync(D_8028A858++);
  {
    Gfx *_g = D_8028A858++;
    _g->words.w0 = 0xb900031d;         /* render mode */
    _g->words.w1 = 0x0f0a4000;
  }
  {
    Gfx *_g = D_8028A858++;
    _g->words.w0 = 0xba001402;         /* cycle type: fill */
    _g->words.w1 = 0x300000;
  }
  {
    Gfx *_g = D_8028A858++;
    _g->words.w0 = 0xf7000000;         /* fill colour */
    _g->words.w1 = 0x10001;
  }
  {
    Gfx *_g = D_8028A858++;
    _g->words.w0 = 0xf8000000;         /* fog colour */
    _g->words.w1 = 0xff;
  }
  gDPFillRectangle(D_8028A858++, x - 1, y - 2, x + w, y - 1);
  gDPFillRectangle(D_8028A858++, x - 1, y + h, x + w, y + h + 1);
  {
    Gfx *_g = D_8028A858++;
    _g->words.w0 = (_SHIFTL(G_FILLRECT, 24, 8) | _SHIFTL(x - 1, 14, 10) |
                    _SHIFTL(y + h, 2, 10));
    _g->words.w1 = (_SHIFTL(x - 2, 14, 10) | _SHIFTL(y - 1, 2, 10));
  }
  gDPFillRectangle(D_8028A858++, x + w, y - 1, x + w + 1, y + h);
  gDPPipeSync(D_8028A858++);
  {
    Gfx *_g = D_8028A858++;
    _g->words.w0 = 0xb900031d;         /* render mode */
    _g->words.w1 = 0x55004240;
  }
  {
    Gfx *_g = D_8028A858++;
    _g->words.w0 = 0xba001402;         /* cycle type: one cycle */
    _g->words.w1 = 0;
  }
  {
    Gfx *_g = D_8028A858++;
    _g->words.w0 = 0xf8000000;         /* fog colour */
    _g->words.w1 = 0xff;
  }
  gDPFillRectangle(D_8028A858++, x, y, x + w, y + 1);
  gDPFillRectangle(D_8028A858++, x, y + h - 1, x + w, y + h);
  gDPFillRectangle(D_8028A858++, x, y, x + 1, y + h);
  gDPFillRectangle(D_8028A858++, x + w - 1, y, x + w, y + h);
  {
    Gfx *_g = D_8028A858++;
    _g->words.w0 = (_SHIFTL(G_FILLRECT, 24, 8) | _SHIFTL(x + w + 3, 14, 10) |
                    _SHIFTL(y - 2, 2, 10));
    _g->words.w1 = (_SHIFTL(x - 3, 14, 10) | _SHIFTL(y - 3, 2, 10));
  }
  {
    Gfx *_g = D_8028A858++;
    _g->words.w0 = (_SHIFTL(G_FILLRECT, 24, 8) | _SHIFTL(x + w + 3, 14, 10) |
                    _SHIFTL(y + h + 3, 2, 10));
    _g->words.w1 = (_SHIFTL(x - 3, 14, 10) | _SHIFTL(y + h + 2, 2, 10));
  }
  {
    Gfx *_g = D_8028A858++;
    _g->words.w0 = (_SHIFTL(G_FILLRECT, 24, 8) | _SHIFTL(x - 2, 14, 10) |
                    _SHIFTL(y + h + 3, 2, 10));
    _g->words.w1 = (_SHIFTL(x - 3, 14, 10) | _SHIFTL(y - 3, 2, 10));
  }
  {
    Gfx *_g = D_8028A858++;
    _g->words.w0 = (_SHIFTL(G_FILLRECT, 24, 8) | _SHIFTL(x + w + 3, 14, 10) |
                    _SHIFTL(y + h + 3, 2, 10));
    _g->words.w1 = (_SHIFTL(x + w + 2, 14, 10) | _SHIFTL(y - 3, 2, 10));
  }
}

/* WHAT IT DOES: Turn a depth ratio (a / b) into the z-buffer's fixed-point
 * value through the current viewport's depth scale and offset. Nothing in
 * the ROM calls it. */
/* @implements 0x8023B130 tgr BrViewportDepthZ */
int BrViewportDepthZ(float a, float b)
{
  return ((a / b) * (float)D_8028AB64 + (float)D_8028AB68) * 32.0f;
}
