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
extern float D_802A78F4;
extern float D_802A78F8;
extern float D_802A7908;
extern int D_8028A8A8;
extern int D_8028A8AC;
extern int D_8028AAB4;
extern Mtx D_803161E0;
extern Gfx *D_8028A858;
extern Gfx D_80271F28[];
/* -- end declarations -- */

/* WHAT IT DOES: Draw a menu item's 3-D icon at (x, y, z), spun by the given
 * angle about the view axis and scaled to 1/32; while 0x8028A8A8 differs
 * from 0x8028A8AC it is mirrored and turns the other way. */
/* @implements 0x8020A524 tgr BrMenuIconDraw */
void BrMenuIconDraw(int icon, float x, float y, float z, float spin)
{
  if (D_8028A8AC != D_8028A8A8) {
    guRotateF(D_8031AB10, -spin * D_802A78F4, 0.0f, 0.0f, 1.0f);
    guScaleF(D_8031AB50, -0.03125f, 0.03125f, 0.03125f);
  } else {
    guRotateF(D_8031AB10, spin * D_802A78F8, 0.0f, 0.0f, 1.0f);
    guScaleF(D_8031AB50, 0.03125f, 0.03125f, 0.03125f);
  }
  guMtxCatF(D_8031AB10, D_8031AB50, D_8031AB10);
  guRotateF(D_8031AB50, -90.0f, 1.0f, 0.0f, 0.0f);
  guMtxCatF(D_8031AB10, D_8031AB50, D_8031AB10);
  guTranslateF(D_8031AB50, x, y, z);
  guMtxCatF(D_8031AB10, D_8031AB50, D_8031AB10);
  func_80209D70(icon, D_8031AB10);
}

/* WHAT IT DOES: Build the menu backdrop's matrix (its scale, tipped back 90
 * degrees, centred and set just above the middle of the screen) and draw
 * its display list. */
/* @implements 0x8020A9D0 tgr BrMenuBackdropDraw */
void BrMenuBackdropDraw(void)
{
  guScaleF(D_8031AB10, D_802A7908, D_802A7908, D_802A7908);
  guRotateF(D_8031AB50, -90.0f, 1.0f, 0.0f, 0.0f);
  guMtxCatF(D_8031AB10, D_8031AB50, D_8031AB10);
  guTranslateF(D_8031AB50, 160.0f, D_8028AAB4 * 6 / 16 - 4, 0.0f);
  guMtxCatF(D_8031AB10, D_8031AB50, D_8031AB10);
  guMtxF2L(D_8031AB10, &D_803161E0);
  gSPDisplayList(D_8028A858++, D_80271F28);
}
