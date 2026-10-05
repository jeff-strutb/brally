/* modeldraw.c -- drawing a lit, textured model from its parts list (the
 * front end's icons and cars, the title screen's model)
 */
#include "tgr/common.h"
#include "tgr/gbi.h"

/* -- declarations -- */
#include "tgr/model.h"

extern Gfx *D_8028A858;
extern int D_80271D98;                  /* set: draw the next model with the opaque surface mode */
extern unsigned int D_80271D9C[4];      /* the lit colour for flags & 3 */
extern unsigned int D_8028A898;         /* the texture filter */
extern unsigned int D_8028A89C;         /* the alpha dither */
extern unsigned int D_8028A8A0;         /* the colour dither */
extern int D_8028A8A8;
extern int D_8028A8AC;
extern int D_8028AA48;                  /* fog is on */
extern float D_8031AB10[4][4];
extern float D_8031AB50[4][4];
void *BrVpAlloc(void);
Mtx *BrMtxAlloc(void);
void guLookAtReflectF(float mf[4][4], void *l, float xEye, float yEye, float zEye, float xAt, float yAt,
                      float zAt, float xUp, float yUp, float zUp);
void guTranslateF(float mf[4][4], float x, float y, float z);
void guMtxF2L(float mf[4][4], Mtx *m);
/* -- end declarations -- */

/* WHAT IT DOES: Draw a model under the matrix m: set up two-cycle
 * textured, z-buffered, lit rendering with a fixed look-at for the
 * reflections, push m, then draw every part that has a display list in two
 * passes (the opaque parts, then the blended ones), each under its own
 * translation; a part can switch its light colour, cull front faces or turn
 * fog off for itself, and restores them after.  Pops m at the end. */
/* The pass counter is a register variable spilled at the start of each pass
 * (0xBC), as the ROM has it: the pass test spelled !(flags & 8) - !pass
 * keeps it out of memory (the difference takes the temporary the ROM uses),
 * and the display list tested around the body (not a continue) is what
 * reloads the part count there.  A homed counter behaved the same per call
 * but left different state behind (whole-image run, rain and night
 * scripts, frame 2983).  The mirror tests are subtractions, and the
 * clear-geometry and light-colour commands are written one word per line,
 * w0 first, as the ROM stores them. */
/* @implements 0x80209D70 tgr BrModelDraw */
void BrModelDraw(BrModel *model, float m[4][4])
{
  void *la;
  Mtx *mtx;
  unsigned int i;

  gDPPipeSync(D_8028A858++);
  gDPSetCycleType(D_8028A858++, 1 << G_MDSFT_CYCLETYPE);
  if (D_80271D98 != 0) {
    gDPSetRenderMode(D_8028A858++, 0x0C192008, 0);
    D_80271D98 = 0;
  } else {
    gDPSetRenderMode(D_8028A858++, 0x0C192038, 0);
  }
  gDPSetCombine(D_8028A858++, 0x26A004, 0x1FFC93F8);
  gDPSetBlendColor(D_8028A858++, 0, 0, 0, 0);
  gDPSetTextureDetail(D_8028A858++, 0);
  gDPSetTextureLOD(D_8028A858++, 0);
  gDPSetTextureLUT(D_8028A858++, 0);
  gDPSetTextureFilter(D_8028A858++, D_8028A898);
  gDPSetColorDither(D_8028A858++, D_8028A8A0);
  gDPSetAlphaDither(D_8028A858++, D_8028A89C);
  gSPSetGeometryMode(D_8028A858++, 1);
  gDPSetAlphaCompare(D_8028A858++, 1);
  gDPSetTextureDetail(D_8028A858++, 0);
  gDPSetTextureLOD(D_8028A858++, 1 << G_MDSFT_TEXTLOD);
  gDPSetTextureLUT(D_8028A858++, 0);
  gDPSetTextureFilter(D_8028A858++, D_8028A898);
  gSPClearGeometryMode(D_8028A858++, 0x853200);
  gSPSetGeometryMode(D_8028A858++, (D_8028A8A8 - D_8028A8AC ? 0x1000 : 0x2000) | 0xA0005 | 0x200);
  la = BrVpAlloc();
  guLookAtReflectF(D_8031AB50, la, 0.0f, -1.0f, 15.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
  gSPLookAtX(D_8028A858++, la);
  gSPLookAtY(D_8028A858++, (char *)la + 16);
  mtx = BrMtxAlloc();
  guMtxF2L(m, mtx);
  gSPMatrix(D_8028A858++, mtx, G_MTX_MODELVIEW | G_MTX_MUL | G_MTX_PUSH);
  gSPTexture(D_8028A858++, 0xFFFF, 0xFFFF, 0, 0, 1);
  gSPClearGeometryMode(D_8028A858++, 0xC0000);
  gDPTileSync(D_8028A858++);
  gRaw(D_8028A858++, 0xF5100000, 0x07000000);
  gRaw(D_8028A858++, 0xF50001F0, 0x06000000);
  gRaw(D_8028A858++, 0xF5000100, 0x05000000);
  {
  int pass;

  for (pass = 0; pass != 2; pass++) {
    for (i = 0; i < BE32(model->nDl); i++) {
      if (!(BES16(model->dls[i].flags) & 8) != !pass) {
        continue;
      }
      if (BES32(model->dls[i].dl) != 0) {
        if (BES16(model->dls[i].flags) & 0x400) {
          gSPLightColor(D_8028A858++, 1, 0);
          gSPLightColor(D_8028A858++, 2, D_80271D9C[BE16(model->dls[i].flags) & 3]);
        }
        if (BES16(model->dls[i].flags) & 4) {
          gSPClearGeometryMode(D_8028A858++, 0x3000);
        }
        if ((BES16(model->dls[i].flags) & 0x80) && D_8028AA48 != 0) {
          gSPClearGeometryMode(D_8028A858++, 0x200);
        }
        gSPTexture(D_8028A858++, 0xFFFF, 0xFFFF, 0, 0, 1);
        gDPTileSync(D_8028A858++);
        mtx = BrMtxAlloc();
        guTranslateF(D_8031AB10, BEF(model->dls[i].pos[0]), BEF(model->dls[i].pos[1]), BEF(model->dls[i].pos[2]));
        guMtxF2L(D_8031AB10, mtx);
        gSPMatrix(D_8028A858++, mtx, G_MTX_MODELVIEW | G_MTX_MUL | G_MTX_PUSH);
        gSPDisplayList(D_8028A858++, BE32(model->dls[i].dl));
        gSPPopMatrix(D_8028A858++, 0);
        if ((BES16(model->dls[i].flags) & 0x80) && D_8028AA48 != 0) {
          gSPSetGeometryMode(D_8028A858++, 0x200);
        }
        if (BES16(model->dls[i].flags) & 4) {
          gSPSetGeometryMode(D_8028A858++, D_8028A8A8 - D_8028A8AC ? 0x1000 : 0x2000);
        }
        if (BES16(model->dls[i].flags) & 0x400) {
          gSPLightColor(D_8028A858++, 1, 0xFFFFFF00);
          gSPLightColor(D_8028A858++, 2, 0x40404000);
        }
      }
    }
  }
  }
  gSPPopMatrix(D_8028A858++, 0);
}
