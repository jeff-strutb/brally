/* menudraw.c -- drawing the generic menu screen's 3-D parts: the backdrop
 * and the spinning item icons
 */
#include "tgr/common.h"
#include "tgr/gbi.h"

/* -- declarations -- */
void guScaleF(float mf[4][4], float x, float y, float z);
void guRotateF(float mf[4][4], float a, float x, float y, float z);
void guTranslateF(float mf[4][4], float x, float y, float z);
void guMtxCatF(float m[4][4], float n[4][4], float r[4][4]);
void guMtxF2L(float mf[4][4], Mtx *m);
void func_80209D70(int icon, float m[4][4]);
extern float D_8031AB10[4][4];
extern float D_8031AB50[4][4];
extern int D_8028A8A8;
extern int D_8028A8AC;
extern int D_8028AAB4;
extern Mtx D_803161E0;
extern Gfx *D_8028A858;
extern Gfx D_80271F28[];
extern int D_80271DAC;
extern Vtx D_80315FF0[31];
extern int D_8028AAB0;
extern Gfx D_80271DB0[];
float sinf(float x);
float cosf(float x);
/* -- end declarations -- */

/* WHAT IT DOES: Draw a menu item's 3-D icon at (x, y, z), spun by the given
 * angle about the view axis and scaled to 1/32; while 0x8028A8A8 differs
 * from 0x8028A8AC it is mirrored and turns the other way. */
/* @implements 0x8020A524 tgr BrMenuIconDraw */
void BrMenuIconDraw(int icon, float x, float y, float z, float spin)
{
  if (D_8028A8AC != D_8028A8A8) {
    guRotateF(D_8031AB10, -spin * 57.2957763671875f, 0.0f, 0.0f, 1.0f);
    guScaleF(D_8031AB50, -0.03125f, 0.03125f, 0.03125f);
  } else {
    guRotateF(D_8031AB10, spin * 57.2957763671875f, 0.0f, 0.0f, 1.0f);
    guScaleF(D_8031AB50, 0.03125f, 0.03125f, 0.03125f);
  }
  guMtxCatF(D_8031AB10, D_8031AB50, D_8031AB10);
  guRotateF(D_8031AB50, -90.0f, 1.0f, 0.0f, 0.0f);
  guMtxCatF(D_8031AB10, D_8031AB50, D_8031AB10);
  guTranslateF(D_8031AB50, x, y, z);
  guMtxCatF(D_8031AB10, D_8031AB50, D_8031AB10);
  func_80209D70(icon, D_8031AB10);
}

/* WHAT IT DOES: The same as BrMenuIconDraw with the scale given instead of
 * the fixed 1/32. */
/* @implements 0x8020A6A4 tgr BrMenuIconDrawScaled */
void BrMenuIconDrawScaled(int icon, float x, float y, float z, float spin, float scale)
{
  if (D_8028A8AC != D_8028A8A8) {
    guRotateF(D_8031AB10, -spin * 57.2957763671875f, 0.0f, 0.0f, 1.0f);
    guScaleF(D_8031AB50, -scale, scale, scale);
  } else {
    guRotateF(D_8031AB10, spin * 57.2957763671875f, 0.0f, 0.0f, 1.0f);
    guScaleF(D_8031AB50, scale, scale, scale);
  }
  guMtxCatF(D_8031AB10, D_8031AB50, D_8031AB10);
  guRotateF(D_8031AB50, -90.0f, 1.0f, 0.0f, 0.0f);
  guMtxCatF(D_8031AB10, D_8031AB50, D_8031AB10);
  guTranslateF(D_8031AB50, x, y, z);
  guMtxCatF(D_8031AB10, D_8031AB50, D_8031AB10);
  func_80209D70(icon, D_8031AB10);
}

/* WHAT IT DOES: Draw the menu's ring: the first time, lay out its 31
 * vertices (a centre point and a circle of radius 64 in 12-degree steps,
 * white, centred on the screen's width); then run its display list. */
/* @implements 0x8020A820 tgr BrMenuRingDraw */
void BrMenuRingDraw(void)
{
  int i;
  float a;

  if (D_80271DAC == 0) {
    for (i = 0; i < 31; i++) {
      a = i * 0.20943952f;
      D_80315FF0[i].v.ob[0] = (short)(sinf(a) * 64.0f) + D_8028AAB0 / 2;
      D_80315FF0[i].v.ob[1] = D_8028AAB4 * 6 / 16;
      D_80315FF0[i].v.ob[2] = cosf(a) * 64.0f;
      D_80315FF0[i].v.cn[0] = 0xff;
      D_80315FF0[i].v.cn[1] = 0xff;
      D_80315FF0[i].v.cn[2] = 0xff;
      D_80315FF0[i].v.cn[3] = 0xff;
    }
    D_80315FF0[0].v.ob[0] = D_8028AAB0 / 2;
    D_80315FF0[0].v.ob[1] = D_8028AAB4 * 5 / 16;
    D_80315FF0[0].v.ob[2] = 0;
  }
  gSPDisplayList(D_8028A858++, D_80271DB0);
}

/* WHAT IT DOES: Build the menu backdrop's matrix (its scale, tipped back 90
 * degrees, centred and set just above the middle of the screen) and draw
 * its display list. */
/* @implements 0x8020A9D0 tgr BrMenuBackdropDraw */
void BrMenuBackdropDraw(void)
{
  guScaleF(D_8031AB10, 0.26f, 0.26f, 0.26f);
  guRotateF(D_8031AB50, -90.0f, 1.0f, 0.0f, 0.0f);
  guMtxCatF(D_8031AB10, D_8031AB50, D_8031AB10);
  guTranslateF(D_8031AB50, 160.0f, D_8028AAB4 * 6 / 16 - 4, 0.0f);
  guMtxCatF(D_8031AB10, D_8031AB50, D_8031AB10);
  guMtxF2L(D_8031AB10, &D_803161E0);
  gSPDisplayList(D_8028A858++, D_80271F28);
}
