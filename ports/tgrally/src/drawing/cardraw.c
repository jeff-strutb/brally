/* cardraw.c -- drawing the cars
 */
#include "tgr/common.h"
#include "tgr/car.h"
#include "tgr/track.h"
#include "tgr/gbi.h"

/* -- declarations -- */
void BrMat4TransformPoint4(float out[4], float v[3], float m[4][4]);
extern BrCarModel *D_8028AB08;
float BrVec3Dist(BrVec3 *a, BrVec3 *b);
float BrFogAmount(float v[3]);
int BrGridSpanHasPoint(float x, float z);
void BrVec3MulAdd(BrVec3 *pOut, BrVec3 *pA, BrVec3 *pB, float s);
extern BrCar *D_8028AAF0;             /* the player's car */
extern BrCar *D_8028AAF4;             /* the car the camera follows */
extern int D_8028AA70;
extern int D_8028AA80;
extern int D_8028AA8C;
extern int D_80351C70[];              /* per slot: draw the car body */
extern int D_80351C80[];              /* per slot: draw the car at all */
extern Gfx *D_8028A858;
extern int D_8028C300;                  /* the fog render-mode bits */
extern int D_80351D00;                  /* the ghost car's alpha */
extern int D_8028AA84;                  /* use the low-detail wheels */
extern float D_8031AB50[4][4];
extern float D_80351CC0[4][4];
extern float D_8031AA50[4][4];
extern float D_8031AB10[4][4];
void guScaleF(float mf[4][4], float x, float y, float z);
void guMtxCatF(float m[4][4], float n[4][4], float r[4][4]);
void guMtxF2L(float mf[4][4], void *m);
void *BrMtxAlloc(void);
void BrMat4FitRange(float m[4][4]);
/* -- end declarations -- */

/* -- declarations: BrSkyDraw -- */
extern int D_8028AA5C;                  /* no sky this frame */
extern int D_8028AA78;                  /* the weather has fog */
extern int D_8028C818;                  /* the lightning timer */
extern int D_8028AAEC;                  /* the view being drawn */
extern unsigned char D_8028AB20, D_8028AB24, D_8028AB28, D_8028AB2C;   /* the fog colour */
extern unsigned int D_8028A898;         /* the texture filter */
extern int D_8028A8A8;
extern int D_8028A8AC;
extern Mtx *D_8028A878;                 /* the projection matrix */
#define D_80025C50 BE32(D_80025C00.x50)  /* the track's sky display list (an original address) */
typedef struct BrSkyView { int x; int y; int w; int h; int car; } BrSkyView;
extern BrSkyView D_8031B2C8[2];
int BrRaceSplitScreen(void);
void guTranslateF(float mf[4][4], float x, float y, float z);
void BrGfxFillRect(int x, int y, int w, int h, int r, int g, int b);
/* -- end declarations -- */

/* WHAT IT DOES: Draw the sky: set the clip ratio, then, unless the frame
 * has no sky, either the track's sky display list around the camera
 * (lowered a little, fogged in fog), or in a split screen a fill of the
 * view in the fog colour -- brightened by a lightning flash.  The unused
 * array is the ROM frame's 0x40.
 * RESIDUE (116): the two cull-mode ternaries -- the ROM materialises the 0x1000
 * arm first and fills the branch's delay slot from the 0x2000 arm (4 bytes
 * longer); everything after is shifted by that. */
/* @t4-pass 0x8022F968 1 2026-09-29 compiles 97 best 116 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x8022F968 2 2026-09-29 compiles 97 best 116 moved 0  (n64/tools/n64permute.py) */
/* @t3 0x8022F968 */
/* @implements 0x8022F968 tgr BrSkyDraw */
void BrSkyDraw(void)
{
  int u[4];
  Mtx *m;

  gSPClipRatio1(D_8028A858++);
  if (D_8028AA5C != 0) {
    return;
  }
  if (BrRaceSplitScreen()) {
    if (D_8028C818 > 0 && (D_8028C818 & 1)) {
      BrGfxFillRect(D_8031B2C8[D_8028AAEC].x, D_8031B2C8[D_8028AAEC].y, D_8031B2C8[D_8028AAEC].w,
                    D_8031B2C8[D_8028AAEC].h, (D_8028AB20 * 3 + 240) >> 2, (D_8028AB24 * 3 + 248) >> 2,
                    (D_8028AB28 * 3 + 255) >> 2);
    } else {
      BrGfxFillRect(D_8031B2C8[D_8028AAEC].x, D_8031B2C8[D_8028AAEC].y, D_8031B2C8[D_8028AAEC].w,
                    D_8031B2C8[D_8028AAEC].h, D_8028AB20, D_8028AB24, D_8028AB28);
    }
    return;
  }
  guTranslateF(D_8031AB10, D_8028AAF4->mtx0[3][0], D_8028AAF4->mtx0[3][1], D_8028AAF4->mtx0[3][2] * 0.99f);
  m = BrMtxAlloc();
  guMtxF2L(D_8031AB10, m);
  gSPMatrix(D_8028A858++, tgr_addr32(D_8028A878) + 0x80000000u, G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
  gSPMatrix(D_8028A858++, tgr_addr32(m) + 0x80000000u, G_MTX_MODELVIEW | G_MTX_MUL | G_MTX_PUSH);
  gDPPipeSync(D_8028A858++);
  gDPSetCycleType(D_8028A858++, G_CYC_1CYCLE);
  if (D_8028AA78 != 0) {
    gDPSetCombine(D_8028A858++, 0x167E2C, 0x55FEF379);
    gDPSetEnvColor(D_8028A858++, D_8028AB20, D_8028AB24, D_8028AB28, D_8028AB2C);
  } else {
    gDPSetCombine(D_8028A858++, 0xFFFFFF, 0xFFFCF87C);
  }
  gDPSetRenderMode(D_8028A858++, 0x0F0A4200, 0);
  gDPSetTextureFilter(D_8028A858++, D_8028A898);
  gSPClearGeometryMode(D_8028A858++, 0xF0205);
  gSPSetGeometryMode(D_8028A858++, D_8028A8A8 == D_8028A8AC ? 0x2000 : 0x1000);
  gSPClearGeometryMode(D_8028A858++, D_8028A8A8 == D_8028A8AC ? 0x1000 : 0x2000);
  gDPSetTextureLOD(D_8028A858++, 0);
  gSPTexture(D_8028A858++, 0xFFFF, 0xFFFF, 0, 0, 1);
  gSPClearGeometryMode(D_8028A858++, 0xC0000);
  gDPTileSync(D_8028A858++);
  gRaw(D_8028A858++, 0xF5100000, 0x07000000);
  gRaw(D_8028A858++, 0xF50001F0, 0x06000000);
  gRaw(D_8028A858++, 0xF5000100, 0x05000000);
  gSPDisplayList(D_8028A858++, D_80025C50);
  gDPPipeSync(D_8028A858++);
  gDPSetCycleType(D_8028A858++, G_CYC_1CYCLE);
  gSPSetGeometryMode(D_8028A858++, 0x20205);
  gSPPopMatrix(D_8028A858++, 0);
}

/* WHAT IT DOES: Place car n's four wheels: each wheel's matrix takes the
 * body's rotation and, as its translation, the model's wheel position
 * carried through the body matrix.  Makes the car's model the current one;
 * a model without wheels is left alone. */
/* @implements 0x8022FEF0 tgr BrCarPlaceWheels */
void BrCarPlaceWheels(int n)
{
  BrCar *car = &D_8031B760[n];
  int i;
  float w[3];

  D_8028AB08 = (BrCarModel *)TGR_PTR(char *, car->model);
  if (BE32(D_8028AB08->dl2[0][0]) != 0) {
    for (i = 0; i < 4; i++) {
      memcpy(car->wheelMtx[i], car->mtx0, 0x30);
      br_vec3_from(w, (const BrVec3be *)D_8028AB08->wheel[i]);   /* the model's wheel position, natively */
      BrMat4TransformPoint4(car->wheelMtx[i][3], w, car->mtx0);
    }
  }
}

/* WHAT IT DOES: Draw the current model's four wheels on a car: for each,
 * set the ghost (translucent, alpha from D_80351D00) or normal combine and
 * fog render mode, scale the wheel's matrix down from the model's 1/255
 * units, load it, load its product with the camera as the lighting matrix,
 * set texturing and tiles, and call the wheel display list (the low-detail
 * one when asked).
 * RESIDUE (20): saved-register naming only -- the ROM gives s3-s5 to the
 * two matrix buffers and the DL command word and s6/s7 to the loop; ours
 * the reverse.  Register-blind exact; loop forms and named locals do not
 * move it. */
/* @t4-pass 0x8022FFB4 1 2026-10-03 compiles 118 best 20 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x8022FFB4 2 2026-10-03 compiles 117 best 20 moved 0  (n64/tools/n64permute.py) */
/* @t3 0x8022FFB4 */
/* @implements 0x8022FFB4 tgr BrCarDrawWheels */
void BrCarDrawWheels(BrCar *car)
{
  int i;
  unsigned int m;

  if (BE32(D_8028AB08->dl2[0][0]) != 0) {
    for (i = 0; i < 4; i++) {
      gDPPipeSync(D_8028A858++);
      gRaw(D_8028A858++, 0xba001402, 0x100000);
      if (car->colour[3] == 2) {
        gRaw(D_8028A858++, 0xb900031d, D_8028C300 | 0x104a50);
        gRaw(D_8028A858++, 0xfb000000, D_80351D00 & 0xff);
        gRaw(D_8028A858++, 0xfc127fff, 0xff17f23f);
      } else {
        gRaw(D_8028A858++, 0xfc127fff, 0xfffff238);
        gRaw(D_8028A858++, 0xb900031d, D_8028C300 | 0x112230);
      }
      guScaleF(D_8031AB50, 0.003921569f, 0.003921569f, 0.003921569f);
      guMtxCatF(D_8031AB50, car->wheelMtx[i], D_80351CC0);
      m = tgr_addr32(BrMtxAlloc());
      guMtxF2L(D_80351CC0, TGR_PTR(void *, m));
      gRaw(D_8028A858++, 0x1060040, m);
      guMtxCatF(D_80351CC0, D_8031AA50, D_8031AB10);
      BrMat4FitRange(D_8031AB10);
      m = tgr_addr32(BrMtxAlloc());
      guMtxF2L(D_8031AB10, TGR_PTR(void *, m));
      gRaw(D_8028A858++, 0x39e0010, m + 0x80000000);
      gRaw(D_8028A858++, 0x3980010, m + 0x80000010);
      gRaw(D_8028A858++, 0x39a0010, m + 0x80000020);
      gRaw(D_8028A858++, 0x39c0010, m + 0x80000030);
      gRaw(D_8028A858++, 0xbb000001, 0xffffffff);
      gRaw(D_8028A858++, 0xb6000000, 0xc0000);
      gRaw(D_8028A858++, 0xe8000000, 0);
      gRaw(D_8028A858++, 0xf5100000, 0x7000000);
      gRaw(D_8028A858++, 0xf50001f0, 0x6000000);
      gRaw(D_8028A858++, 0xf5000100, 0x5000000);
      if (D_8028AA84 != 0) {
        if (BE32(D_8028AB08->dl2[0][2]) != 0) {
          gRaw(D_8028A858++, 0x6000000, BE32(D_8028AB08->dl2[0][2]));
        }
      } else if (BE32(D_8028AB08->dl2[0][0]) != 0) {
        gRaw(D_8028A858++, 0x6000000, BE32(D_8028AB08->dl2[0][0]));
      }
      gRaw(D_8028A858++, 0xbd000000, 0);
    }
  }
}

/* WHAT IT DOES: Decide whether a car is drawn this frame and set its fog
 * amount: other cars only when their position (or, with either of two
 * flags set, a point 6 units ahead) lies on the track grid; the player's
 * car skips its body when seen from its in-car views; a car with colour
 * byte 3 == 2 never draws its body. */
/* @implements 0x802303D4 tgr BrCarVisibility */
void BrCarVisibility(BrCar *car)
{
  float dist;
  float v[3];

  dist = BrVec3Dist((BrVec3 *)car->mtx0[3], (BrVec3 *)D_8028AAF4->mtx0[3]);
  D_80351C70[car->slot] = 0;
  D_80351C80[car->slot] = 0;
  car->fog = BrFogAmount(car->mtx0[3]);
  if (car != D_8028AAF0) {
    if (D_8028AA80 != 0 || D_8028AA8C != 0) {
      BrVec3MulAdd((BrVec3 *)v, (BrVec3 *)car->mtx0[3], (BrVec3 *)car->mtx0[0], 6.0f);
      if (!BrGridSpanHasPoint(v[0], v[1]) && !BrGridSpanHasPoint(car->mtx0[3][0], car->mtx0[3][1])) {
        return;
      }
    } else if (!BrGridSpanHasPoint(car->mtx0[3][0], car->mtx0[3][1])) {
      return;
    }
  }
  if (car == D_8028AAF0 && (TGR_PTR(BrCarCam *, car->cam) == &car->cams[0] || TGR_PTR(BrCarCam *, car->cam) == &car->cam4) && D_8028AA70 == 0) {
    D_80351C80[car->slot] = 1;
    return;
  }
  if (car->colour[3] != 2) {
    D_80351C70[car->slot] = 1;
  }
  D_80351C80[car->slot] = 1;
}

/* -- declarations: BrCarDrawBody -- */
extern unsigned int D_80351CA0[];       /* per slot: the car's model matrix */
extern unsigned int D_80351CB0[];       /* per slot: the car's lighting matrix */
extern Gfx D_8028A9C8[];                /* the car setup display list */
extern char D_8028A9F0[];               /* the two lights */
extern char D_8028A9F8[];
extern unsigned int D_8028A8A0;
extern float D_8028B750;                /* the screen flash level */
void BrTexLoad(int tile, void *parts);
void BrVec3Add(BrVec3 *out, BrVec3 *a, BrVec3 *b);
void BrVec3Sub(BrVec3 *out, BrVec3 *a, BrVec3 *b);
float BrVec3Length(BrVec3 *v);
void BrVec3DivBy(BrVec3 *v, float s);
float BrVec3Dot(BrVec3 *a, BrVec3 *b);
void BrVec3Scale(BrVec3 *out, BrVec3 *v, float s);
void BrVec3MulAddTo(BrVec3 *out, BrVec3 *v, float s);
/* -- end declarations -- */

/* -- declarations: BrCarDraw -- */
#include "tgr/pad.h"
typedef struct BrTrackObjFl {  /* a track object, as far as the car draw reads it (0x54 bytes) */
  char pad00[0x4C];
  unsigned short flags;         /* 0x4C  0x10: a tunnel */
  char pad4e[0x54 - 0x4E];
} BrTrackObjFl;
void BrVec3Normalise(BrVec3 *v);
#define TXL2WORDS_4b(txls)      MAX(1, ((txls) / 16))
#define CALC_DXT_4b(width)      (((1 << 11) + TXL2WORDS_4b(width) - 1) / TXL2WORDS_4b(width))
#define gDPLoadTextureBlock_4b(pkt, timg, fmt, width, height, pal, cms, cmt, masks, maskt, shifts, shiftt) \
{                                                                       \
    gDPSetTextureImage(pkt, fmt, 2, 1, timg);                           \
    gDPSetTile(pkt, fmt, 2, 0, 0, 7, 0, cmt, maskt, shiftt, cms, masks, shifts); \
    gDPLoadSync(pkt);                                                   \
    gDPLoadBlock(pkt, 7, 0, 0, (((width) * (height) + 3) >> 2) - 1, CALC_DXT_4b(width)); \
    gDPPipeSync(pkt);                                                   \
    gDPSetTile(pkt, fmt, 0, ((((width) >> 1) + 7) >> 3), 0, 0, pal, cmt, maskt, shiftt, cms, masks, shifts); \
    gDPSetTileSize(pkt, 0, 0, 0, ((width) - 1) << 2, ((height) - 1) << 2); \
}
#define gDPLoadTLUT_pal16(pkt, pal, dram)                               \
{                                                                       \
    gDPSetTextureImage(pkt, 0, 2, 1, dram);                             \
    gDPTileSync(pkt);                                                   \
    gDPSetTile(pkt, 0, 0, 0, (256 + (((pal) & 0xf) * 16)), 7, 0, 0, 0, 0, 0, 0, 0); \
    gDPLoadSync(pkt);                                                   \
    gDPLoadTLUTCmd(pkt, 7, 15);                                         \
    gDPPipeSync(pkt);                                                   \
}
typedef struct BrHilite { int x1, y1, x2, y2; } BrHilite;
#define gDPSetHilite1Tile(pkt, tile, hilite, width, height)            \
    gDPSetTileSize(pkt, tile, (hilite)->x1 & 0xfff, (hilite)->y1 & 0xfff, \
                   ((((width) - 1) * 4) + (hilite)->x1) & 0xfff,       \
                   ((((height) - 1) * 4) + (hilite)->y1) & 0xfff)
#define VEC3_COPY(d, s)                                                 \
{                                                                       \
  float *_d = (float *)(d);                                             \
  float *_s = (float *)(s);                                             \
                                                                        \
  _d[0] = _s[0];                                                        \
  _d[1] = _s[1];                                                        \
  _d[2] = _s[2];                                                        \
}
#define G(a, b) { Gfx *g_ = D_8028A858++; tgr_wr32(&g_->words.w0, (unsigned int)(a)); tgr_wr32(&g_->words.w1, TGR_W1(b)); }
extern unsigned int D_80351C90[];       /* per slot: the level of detail drawn */
extern int D_8028AA9C;                  /* the least detail to draw */
extern int D_8028AB0C;                  /* views on screen */
extern int D_8028AA78;                  /* fog */
extern int D_8028A8A8;
extern int D_8028A8AC;                  /* the track is mirrored */
extern int D_8028C304;                  /* the body render-mode bits */
extern int D_8028C334;
extern int D_8028DDC8;
extern int D_8028DDCC;
extern int D_8028AA3C;                  /* polygons drawn */
extern unsigned char D_8028AB40, D_8028AB44, D_8028AB48;  /* the light colours */
extern unsigned char D_8028AB4C, D_8028AB50, D_8028AB54;
extern float D_8031B338[3];             /* the sun's direction */
extern BrVec3 D_80351D08;               /* the light direction for this car */
extern BrVec3 D_80351D18;               /* the highlight direction */
extern char D_8028A900[];               /* the car setup display lists: near, */
extern unsigned char D_80351C10[];      /* per slot: two lights, 0x18 bytes each */
typedef struct BrLights1 {              /* an ambient and one directional light */
  int amb[2];
  int col[2];                           /* 0x08  the light's colour (copied twice) */
  signed char dir[3];                   /* 0x10 */
  char pad13;
  int pad14;
} BrLights1;
extern int D_8028C340;                  /* the environment-map palette */
extern int D_802A1968;                  /* the highlight textures: day, */
extern int D_802A2170;                  /* and wet */
extern int D_8028C308[];                /* the chrome textures per class, */
extern int D_8028C318[];                /* and their palettes */
extern unsigned char D_8028C32C[];      /* the class per car kind and tunnel */
extern int D_8028C328;                  /* the car kind (paint scheme) */
extern float D_802A9ED4;                /* 1/255 */
extern float D_802A9ED8;
extern float D_802A9EDC;
extern float D_802A9EE0;
void * BrVpAlloc(void);
void *BrLightAlloc(void);
void BrVec3Negate(BrVec3 *out, void *v);
void BrVec3Midpoint(BrVec3 *out, BrVec3 *a, BrVec3 *b);
float BrVec3Length(BrVec3 *v);
void guLookAtReflectF(float mf[4][4], void *l, float xEye, float yEye, float zEye, float xAt,
                      float yAt, float zAt, float xUp, float yUp, float zUp);
void guLookAtHiliteF(float mf[4][4], void *l, void *h, float xEye, float yEye, float zEye,
                     float xAt, float yAt, float zAt, float xUp, float yUp, float zUp,
                     float xl1, float yl1, float zl1, float xl2, float yl2, float zl2, int twidth,
                     int theight);
/* -- end declarations -- */

/* WHAT IT DOES: Draw one car (lod added to the detail level): the ghost's
 * alpha, the tunnel flag from the trigger it last passed, the level of detail
 * from the view setting (by distance with two views), the model matrix and
 * the lighting matrix, nothing more for the player's own car seen from its
 * in-car views; the light colours (dimmed in tunnels, faded with distance in
 * rain); the light direction (the sun, or the camera's at night) and the
 * highlight halfway to the camera; the reflection and highlight look-ats; the
 * near or far setup list; the lights (a per-car copy pointing back along the
 * player's heading at night); the render modes; then the body (with the
 * wheels for a ghost), the brake lamps' and the indicators' colours patched
 * into their palettes, the near details, the shadow decal when the light is
 * right, the chrome pass with its scrolling environment map, the windows, and
 * the wheels.  PC twin: BrCarDrawVehicle.
 * The view-mode flag is read into a local and tested once more by an empty
 * if after the lights flag is set; the empty test costs no code but splits
 * the block, which is what puts the flag in v0. */
/* @implements 0x80230554 tgr BrCarDraw */
void BrCarDraw(BrCar *car, int lodBias)
{
  int lod;
  int tunnel;
  float dist;
  float nearPad;
  void *hilite;
  int reflect;
  int look;
  unsigned int colA;
  unsigned int colB;
  int mode;
  unsigned char *cls;
  float div;
  int tpal;
  int lights;
  int k;
  unsigned char c[4];
  int tw;
  int th;
  int cw;
  int ch;
  unsigned short *pal;
  int unused344[4];
  BrVec3 toCam;
  float x;
  float y;
  float z;
  float atZ;
  float atX;
  float eye[3];

  tw = 64;
  th = 64;
  tunnel = 0;
  if (D_80351C80[car->slot] == 0) {
    return;
  }
  dist = BrVec3Dist((BrVec3 *)car->mtx0[3], (BrVec3 *)D_8028AAF4->mtx0[3]);
  if (car->colour[3] == 2) {
    D_80351D00 = car->x2064 * 255.0f;
    gDPSetFogColor(D_8028A858++, D_8028AB20, D_8028AB24, D_8028AB28, D_80351D00);
  }
  *(int *)car->pad1dc8 = 0;
  if (car->x2000 != 0 && (BE16(BR_TRACKOBJS()[car->x1fc0[0]].flags) & 0x10) != 0) {
    *(int *)car->pad1dc8 = 1;
    tunnel = 1;
  }
  D_8028AB08 = (BrCarModel *)TGR_PTR(char *, car->model);
  if (D_8028AB0C == 2) {
    if (dist < 40.0f) {
      lod = 0;
    } else if (dist < 80.0f) {
      lod = 1;
    } else {
      lod = 2;
    }
    if (lod < D_8028AA9C) {
      lod = D_8028AA9C;
    }
  } else {
    lod = D_8028AA9C;
  }
  lod += lodBias;
  if (lod > 2) {
    lod = 2;
  }
  guScaleF(D_8031AB50, D_802A9ED4, D_802A9ED4, D_802A9ED4);
  guMtxCatF(D_8031AB50, car->mtx0, D_80351CC0);
  D_80351CA0[car->slot] = tgr_addr32(BrMtxAlloc());
  guMtxF2L(D_80351CC0, TGR_PTR(void *, D_80351CA0[car->slot]));
  guMtxCatF(D_80351CC0, D_8031AA50, D_8031AB10);
  BrMat4FitRange(D_8031AB10);
  D_80351CB0[car->slot] = tgr_addr32(BrMtxAlloc());
  guMtxF2L(D_8031AB10, TGR_PTR(void *, D_80351CB0[car->slot]));
  if (car == D_8028AAF0 && (TGR_PTR(BrCarCam *, car->cam) == &car->cams[0] || TGR_PTR(BrCarCam *, car->cam) == &car->cam4) && D_8028AA70 == 0) {
    return;
  }
  D_80351C90[car->slot] = lod;
  if (D_8028AA80 != 0) {
    div = dist / 10.0f;
    if (div < 1.0f) {
      div = 1.0f;
    }
    c[0] = tgr_f2u((unsigned int)D_8028AB40 / div);
    c[1] = tgr_f2u((unsigned int)D_8028AB44 / div);
    c[2] = tgr_f2u((unsigned int)D_8028AB48 / div);
    colA = c[0] << 24 | c[1] << 16 | c[2] << 8;
    c[0] = D_8028AB4C;
    c[1] = D_8028AB50;
    c[2] = D_8028AB54;
    colB = c[0] << 24 | c[1] << 16 | c[2] << 8;
  } else if (tunnel != 0) {
    c[0] = 0;
    c[1] = 0;
    c[2] = 0;
    colA = c[0] << 24 | c[1] << 16 | c[2] << 8;
    c[0] = (D_8028AB4C << 2) / 5;
    c[1] = (D_8028AB50 << 2) / 5;
    c[2] = (D_8028AB54 << 2) / 5;
    colB = c[0] << 24 | c[1] << 16 | c[2] << 8;
  } else {
    c[0] = D_8028AB40;
    c[1] = D_8028AB44;
    c[2] = D_8028AB48;
    colA = c[0] << 24 | c[1] << 16 | c[2] << 8;
    c[0] = D_8028AB4C;
    c[1] = D_8028AB50;
    c[2] = D_8028AB54;
    colB = c[0] << 24 | c[1] << 16 | c[2] << 8;
  }
  gSPMatrix(D_8028A858++, D_80351CA0[car->slot], 6);
  gSPMatrix(D_8028A858++, D_8028A878, 3);
  if (D_8028AA80 != 0) {
    if (&D_8028AAF0->cams[3] == (BrCarCam *)D_8028AAF4) {
      BrVec3Negate(&D_80351D08, D_8028AAF4);
    } else {
      BrVec3Negate(&D_80351D08, D_8028AAF0);
    }
  } else {
    x = D_8031B338[0];
    y = D_8031B338[1];
    z = D_8031B338[2];
    D_80351D08.x = x;
    D_80351D08.y = y;
    D_80351D08.z = z;
  }
  BrVec3Normalise(&D_80351D08);
  D_80351D18.x = D_80351D08.x;
  D_80351D18.y = D_80351D08.y;
  D_80351D18.z = D_80351D08.z;
  BrVec3Sub(&toCam, (BrVec3 *)D_8028AAF4->mtx0[3], (BrVec3 *)car->mtx0[3]);
  div = BrVec3Length(&toCam);
  if (div == 0) {
    BrVec3Negate(&toCam, D_8028AAF4);
  } else {
    BrVec3DivBy(&toCam, div);
  }
  BrVec3Midpoint(&D_80351D18, &toCam, &D_80351D18);
  div = BrVec3Length(&D_80351D18);
  if (div == 0) {
    VEC3_COPY(&D_80351D18, D_8028AAF4->mtx0[2]);
  } else {
    BrVec3DivBy(&D_80351D18, div);
  }
  BrMtxAlloc();
  hilite = BrLightAlloc();
  reflect = tgr_addr32(BrVpAlloc());
  look = tgr_addr32(BrVpAlloc());
  atZ = 0.0f;
  atX = 0.0f;
  if (D_8028AAF4->mtx0[3][0] == car->mtx0[3][0] && D_8028AAF4->mtx0[3][1] == car->mtx0[3][1]) {
    if (D_8028AAF4->mtx0[3][2] == car->mtx0[3][2]) {
      atZ = 1.0f;
    } else {
      atX = D_802A9ED8;
    }
  }
  eye[0] = D_8028AAF4->mtx0[0][0];
  eye[1] = D_8028AAF4->mtx0[0][1];
  eye[2] = D_8028AAF4->mtx0[0][2];
  if (eye[0] == 0.0f && eye[1] == 0.0f) {
    eye[0] = D_802A9EDC;
  }
  guLookAtReflectF(D_8031AB10, TGR_PTR(void *, reflect), eye[0], eye[1], 0.0f, 0, 0, 0, 0, 0, 1.0f);
  guLookAtHiliteF(D_8031AB10, TGR_PTR(void *, look), hilite, D_8028AAF4->mtx0[3][0], D_8028AAF4->mtx0[3][1],
                  D_8028AAF4->mtx0[3][2], car->mtx0[3][0] + atX, car->mtx0[3][1], car->mtx0[3][2] + atZ,
                  0.0f, 0.0f, 1.0f, D_80351D18.x, D_80351D18.y, D_80351D18.z, D_80351D18.x, D_80351D18.y,
                  D_80351D18.z, 64, 64);
  if (10.0f < dist) {
    gSPDisplayList(D_8028A858++, D_8028A9C8);
  } else {
    gSPDisplayList(D_8028A858++, D_8028A900);
  }
  if (D_8028AA80 != 0 || D_8028AA8C != 0) {
    ((BrLights1 *)D_80351C10)[car->slot] = *(BrLights1 *)D_8028A9F0;
    ((BrLights1 *)D_80351C10)[car->slot].dir[0] = -(signed char)(int)(D_8028AAF0->mtx0[0][0] * 120.0f);
    ((BrLights1 *)D_80351C10)[car->slot].dir[1] = -(signed char)(int)(D_8028AAF0->mtx0[0][1] * 120.0f);
    ((BrLights1 *)D_80351C10)[car->slot].dir[2] = -(signed char)(int)(D_8028AAF0->mtx0[0][2] * 120.0f);
    gSPNumLights(D_8028A858++, 1);
    gSPLight(D_8028A858++, ((BrLights1 *)D_80351C10)[car->slot].col, 1);
    gSPLight(D_8028A858++, ((BrLights1 *)D_80351C10)[car->slot].amb, 2);
  } else {
    gSPNumLights(D_8028A858++, 1);
    gSPLight(D_8028A858++, D_8028A9F8, 1);
    gSPLight(D_8028A858++, D_8028A9F0, 2);
  }
  gDPPipeSync(D_8028A858++);
  gDPSetTextureLOD(D_8028A858++, 0x10000);
  {
    Gfx *_g = (Gfx *)(D_8028A858++);

    tgr_wr32(&_g->words.w0, 0xB7000000);
    tgr_wr32(&_g->words.w1, 0x20205);
  }
  if (D_8028AA78 != 0) {
    gSPSetGeometryMode(D_8028A858++, 0x10000);
    if (car->colour[3] == 2) {
      D_8028C300 = 0x0C080000;
    } else {
      D_8028C300 = 0xC8000000;
    }
  } else {
    D_8028C300 = 0x0C080000;
  }
  gSPSetGeometryMode(D_8028A858++, D_8028A8AC - D_8028A8A8 ? 0x1000 : 0x2000);
  gSPClearGeometryMode(D_8028A858++, D_8028A8AC - D_8028A8A8 ? 0x2000 : 0x1000);
  if (car->colour[3] == 2) {
    D_8028C304 = 0x011049D8;
  } else if ((dist < 100.0f) != 0) {
    if (car == D_8028AAF0) {
      D_8028C304 = 0x00112078;
    } else {
      D_8028C304 = 0x00112038;
    }
  } else {
    D_8028C304 = 0x00112230;
  }
  gDPSetCycleType(D_8028A858++, 0x100000);
  gDPSetRenderMode(D_8028A858++, D_8028C300, D_8028C304);
  gDPSetCombine(D_8028A858++, 0x127FFF, 0xFFFFF238);
  gDPSetTextureFilter(D_8028A858++, D_8028A898);
  gDPSetTextureLOD(D_8028A858++, 0);
  gSPClearGeometryMode(D_8028A858++, 0xC0000);
  gMoveWd(D_8028A858++, 0x0A, 0x00, colA);
  gMoveWd(D_8028A858++, 0x0A, 0x04, colA);
  gMoveWd(D_8028A858++, 0x0A, 0x20, colB);
  gMoveWd(D_8028A858++, 0x0A, 0x24, colB);
  if (car->colour[3] == 2) {
    BrCarDrawWheels(car);
  }
  gDma1p(D_8028A858++, 0x03, D_80351CB0[car->slot], 0x10, 0x9E);
  gDma1p(D_8028A858++, 0x03, D_80351CB0[car->slot] + 0x10, 0x10, 0x98);
  gDma1p(D_8028A858++, 0x03, D_80351CB0[car->slot] + 0x20, 0x10, 0x9A);
  gDma1p(D_8028A858++, 0x03, D_80351CB0[car->slot] + 0x30, 0x10, 0x9C);
  gDPSetTextureImage(D_8028A858++, 0, 2, 1, &D_8028C340);
  gDPTileSync(D_8028A858++);
  gDPSetTile(D_8028A858++, 0, 0, 0, 0x1E0, 7, 0, 0, 0, 0, 0, 0, 0);
  gDPLoadSync(D_8028A858++);
  gDPLoadTLUTCmd(D_8028A858++, 7, 15);
  gDPPipeSync(D_8028A858++);
  gSPLookAtX(D_8028A858++, look);
  gSPLookAtY(D_8028A858++, look + 0x10);
  if (D_8028DDC8 == 0 && car->x2068 == 0) {
    gSPTexture(D_8028A858++, 0xFFFF, 0xFFFF, 0, 0, 1);
    gSPClearGeometryMode(D_8028A858++, 0xC0000);
    gDPTileSync(D_8028A858++);
    gDPSetTile(D_8028A858++, 0, 2, 0, 0x0, 7, 0, 0, 0, 0, 0, 0, 0);
    gDPSetTile(D_8028A858++, 0, 0, 0, 0x1F0, 6, 0, 0, 0, 0, 0, 0, 0);
    gDPSetTile(D_8028A858++, 0, 0, 0, 0x100, 5, 0, 0, 0, 0, 0, 0, 0);
    gDPSetRenderMode(D_8028A858++, D_8028C300, D_8028C304);
    gDPSetCombine(D_8028A858++, 0x127FFF, 0xFFFFF238);
    gDPSetTextureLUT(D_8028A858++, 0);
    gDPSetCombine(D_8028A858++, 0x1219FF, 0xFFFFFE38);
    gSPClearGeometryMode(D_8028A858++, 0x40000);
    gMoveWd(D_8028A858++, 0x0A, 0x00, colA);
    gMoveWd(D_8028A858++, 0x0A, 0x04, colA);
    gMoveWd(D_8028A858++, 0x0A, 0x20, colB);
    gMoveWd(D_8028A858++, 0x0A, 0x24, colB);
    gDPSetTextureFilter(D_8028A858++, D_8028A898);
    if (BE32(D_8028AB08->dl[lod][8]) != 0) {
      gSPDisplayList(D_8028A858++, BE32(D_8028AB08->dl[lod][8]));
    }
    gSPTexture(D_8028A858++, 0xFFFF, 0xFFFF, 0, 0, 1);
    gSPClearGeometryMode(D_8028A858++, 0xC0000);
    gDPTileSync(D_8028A858++);
    gDPSetTile(D_8028A858++, 0, 2, 0, 0x0, 7, 0, 0, 0, 0, 0, 0, 0);
    gDPSetTile(D_8028A858++, 0, 0, 0, 0x1F0, 6, 0, 0, 0, 0, 0, 0, 0);
    gDPSetTile(D_8028A858++, 0, 0, 0, 0x100, 5, 0, 0, 0, 0, 0, 0, 0);
  }
  mode = D_8028AA80;
  lights = mode != 0 || D_8028AA8C != 0;
  if (mode != 0) {
  }
  pal = BEPTR(unsigned short *, BR_CARPARTS(D_8028AB08)[D_8028AB08->decalPart[10]].b);
  if (pal != 0) {
    for (k = 12; k < 16; k++) {
      if (lights) {
        tgr_wr16(&pal[k], tgr_rd16(&pal[k]) & (~1));
      } else {
        tgr_wr16(&pal[k], tgr_rd16(&pal[k]) | (1));
      }
    }
  }
  pal = BEPTR(unsigned short *, BR_CARPARTS(D_8028AB08)[D_8028AB08->decalPart[11]].b);
  if (pal != 0) {
    if (lights) {
      tgr_wr16(&pal[15], 0x7000);
      tgr_wr16(&pal[10], 0x9082);
    } else {
      tgr_wr16(&pal[15], 0x9001);
      tgr_wr16(&pal[10], 0xA001);
    }
    if ((*TGR_PTR(unsigned int *, car->pad) & 0xC0000) != 0) {
      tgr_wr16(&pal[14], 0xC000);
      tgr_wr16(&pal[13], 0xC000);
      tgr_wr16(&pal[9], 0xF904);
      tgr_wr16(&pal[8], 0xF904);
    } else {
      tgr_wr16(&pal[14], 0x9001);
      tgr_wr16(&pal[13], tgr_rd16(&pal[15]));
      tgr_wr16(&pal[9], 0xA001);
      tgr_wr16(&pal[8], tgr_rd16(&pal[10]));
    }
    tgr_wr16(&pal[12], 0x7981);
    tgr_wr16(&pal[7], 0x9241);
    if (car->xe38 < 0.0f) {
      tgr_wr16(&pal[11], 0xB5AC);
      tgr_wr16(&pal[6], 0xFFFE);
    } else {
      tgr_wr16(&pal[11], 0xAD6B);
      tgr_wr16(&pal[6], 0xC631);
    }
  }
  gDPSetTextureLOD(D_8028A858++, 0x10000);
  gSPTexture(D_8028A858++, 0xFFFF, 0xFFFF, 0, 0, 1);
  gDPSetTile(D_8028A858++, 0, 2, 0, 0x0, 7, 0, 0, 0, 0, 0, 0, 0);
  gDPSetTile(D_8028A858++, 0, 0, 0, 0x1F0, 6, 0, 0, 0, 0, 0, 0, 0);
  gDPSetTile(D_8028A858++, 0, 0, 0, 0x100, 5, 0, 0, 0, 0, 0, 0, 0);
  {
    BrVec3 rel;

    BrVec3Sub(&rel, (BrVec3 *)car->mtx0[3], (BrVec3 *)D_8028AAF4->mtx0[3]);
    if (0.0 < BrVec3Dot((BrVec3 *)car->mtx0[2], &rel)) {
      BrTexLoad(6, BEPTR(void *, D_8028AB08->parts));
      gDPPipeSync(D_8028A858++);
      {
        Gfx *_g = (Gfx *)(D_8028A858++);

        tgr_wr32(&_g->words.w0, 0xBA001402);
        tgr_wr32(&_g->words.w1, 0x100000);
      }
      gDPSetRenderMode(D_8028A858++, D_8028C300, D_8028C304);
      gDPSetCombine(D_8028A858++, 0x127FFF, 0xFFFFF838);
      gMoveWd(D_8028A858++, 0x0A, 0x00, colA);
      gMoveWd(D_8028A858++, 0x0A, 0x04, colA);
      gMoveWd(D_8028A858++, 0x0A, 0x20, colB);
      gMoveWd(D_8028A858++, 0x0A, 0x24, colB);
      gSPTexture(D_8028A858++, 0xFFFF, 0xFFFF, 0, 0, 1);
      gDPSetTile(D_8028A858++, 0, 2, 0, 0x0, 7, 0, 0, 0, 0, 0, 0, 0);
      gDPSetTile(D_8028A858++, 0, 0, 0, 0x1F0, 6, 0, 0, 0, 0, 0, 0, 0);
      gDPSetTile(D_8028A858++, 0, 0, 0, 0x100, 5, 0, 0, 0, 0, 0, 0, 0);
      if (BE32(D_8028AB08->dl[lod][6]) != 0) {
        gSPDisplayList(D_8028A858++, BE32(D_8028AB08->dl[lod][6]));
      }
    }
  }
  {
    float near;
    int scroll;

  if (((BrPadRec *)TGR_PTR(unsigned int *, D_8028AAF0->pad))->ghost != 0 && &D_8028AAF0->cams[3] == (BrCarCam *)D_8028AAF4) {
    near = dist < 120.0f;
  } else {
    near = dist < 50.0f;
  }
  if (D_8028A8A8 == 0 && near && car->colour[3] != 2 && lod < 1) {
    BrTexLoad(3, BEPTR(void *, D_8028AB08->parts));
    gDPPipeSync(D_8028A858++);
    {
      Gfx *_g = (Gfx *)(D_8028A858++);

      tgr_wr32(&_g->words.w0, 0xBA001402);
      tgr_wr32(&_g->words.w1, 0x100000);
    }
    gDPSetRenderMode(D_8028A858++, D_8028C300, D_8028C304);
    gDPSetCombine(D_8028A858++, 0x127FFF, 0xFFFFF838);
    gMoveWd(D_8028A858++, 0x0A, 0x00, colA);
    gMoveWd(D_8028A858++, 0x0A, 0x04, colA);
    gMoveWd(D_8028A858++, 0x0A, 0x20, colB);
    gMoveWd(D_8028A858++, 0x0A, 0x24, colB);
    gSPTexture(D_8028A858++, 0xFFFF, 0xFFFF, 0, 0, 1);
    gDPSetTile(D_8028A858++, 0, 2, 0, 0x0, 7, 0, 0, 0, 0, 0, 0, 0);
    gDPSetTile(D_8028A858++, 0, 0, 0, 0x1F0, 6, 0, 0, 0, 0, 0, 0, 0);
    gDPSetTile(D_8028A858++, 0, 0, 0, 0x100, 5, 0, 0, 0, 0, 0, 0, 0);
    if (BE32(D_8028AB08->dl[lod][3]) != 0) {
      gSPDisplayList(D_8028A858++, BE32(D_8028AB08->dl[lod][3]));
    }
  }
  if (D_8028AB0C == 1 && D_8028C334 == 0 && D_8028A8A8 == 0 && D_8028AA84 == 0 && D_8028AA8C == 0 &&
      (tunnel == 0 || D_8028AA80 != 0) && near && (D_8028AA80 == 0 || car != D_8028AAF0) &&
      D_8028DDC8 == 0 && car->x2068 == 0) {
    gDPPipeSync(D_8028A858++);
    gDPSetCycleType(D_8028A858++, 0);
    gSPSetGeometryMode(D_8028A858++, 0x40000);
    gSPTexture(D_8028A858++, 0x0F80, 0x0F80, 0, 0, 1);
    gDPSetTextureFilter(D_8028A858++, D_8028A898);
    gDPSetPrimColor(D_8028A858++, 0, 0, 0xFF, 0xFF, 0xCC, 0xFF);
    gDPSetCombine(D_8028A858++, 0xFFFFFF, 0xFFFDF2F9);
    {
      Gfx *_g = (Gfx *)(D_8028A858++);

      tgr_wr32(&_g->words.w0, 0xB900031D);
      tgr_wr32(&_g->words.w1, 0x00504F50);
    }
    gDPTileSync(D_8028A858++);
    gDPSetTextureLUT(D_8028A858++, 0);
    gDPSetTextureImage(D_8028A858++, 4, 2, 1, D_8028AA80 ? &D_802A2170 : &D_802A1968);
    {
      Gfx *_g = (Gfx *)(D_8028A858++);

      tgr_wr32(&_g->words.w0, 0xF5900000);
      tgr_wr32(&_g->words.w1, 0x07018060);
    }
    gDPLoadSync(D_8028A858++);
    gDPLoadBlock(D_8028A858++, 7, 0, 0, (((tw) * (th) + 3) >> 2) - 1, CALC_DXT_4b(tw));
    gDPPipeSync(D_8028A858++);
    {
      Gfx *_g = (Gfx *)(D_8028A858++);

      tgr_wr32(&_g->words.w0, _SHIFTL(G_SETTILE, 24, 8) | _SHIFTL(4, 21, 3) | _SHIFTL(0, 19, 2) |
                     _SHIFTL((((tw) >> 1) + 7) >> 3, 9, 9) | _SHIFTL(0, 0, 9));
      tgr_wr32(&_g->words.w1, _SHIFTL(0, 24, 3) | _SHIFTL(0, 20, 4) | _SHIFTL(0, 18, 2) | _SHIFTL(6, 14, 4) |
                     _SHIFTL(0, 10, 4) | _SHIFTL(0, 8, 2) | _SHIFTL(6, 4, 4) | _SHIFTL(0, 0, 4));
    }
    {
      Gfx *_g = (Gfx *)(D_8028A858++); tgr_wr32(&_g->words.w0, _SHIFTL(G_SETTILESIZE, 24, 8) | _SHIFTL(0, 12, 12) | _SHIFTL(0, 0, 12));
      tgr_wr32(&_g->words.w1, _SHIFTL(0, 24, 3) | _SHIFTL(((tw) - 1) << 2, 12, 12) | _SHIFTL(((th) - 1) << 2, 0, 12));
    }
    gDPSetColorDither(D_8028A858++, 0xC0);
    gDPSetHilite1Tile(D_8028A858++, 0, (BrHilite *)hilite, 64, 64);
    if (BE32(D_8028AB08->dl[lod][9]) != 0) {
      gSPDisplayList(D_8028A858++, BE32(D_8028AB08->dl[lod][9]));
    }
    gDPSetColorDither(D_8028A858++, D_8028A8A0);
  }
  gDPPipeSync(D_8028A858++);
  gDPSetCycleType(D_8028A858++, 0x100000);
  gSPSetGeometryMode(D_8028A858++, (D_8028DDCC != 0 ? 0x80000 : 0) | 0x40000);
  {
    Gfx *_g = (Gfx *)(D_8028A858++);

    tgr_wr32(&_g->words.w0, 0xBB000001);
    tgr_wr32(&_g->words.w1, 0x08001000);
  }
  gDPSetTextureFilter(D_8028A858++, D_8028A898);
  gDPSetCombine(D_8028A858++, 0x167E2C, 0x55FEF379);
  cls = D_8028C32C;
  if (D_8028AA78 != 0) {
    if (tunnel != 0) {
      gDPSetEnvColor(D_8028A858++, D_8028AB20, D_8028AB24, D_8028AB28, (D_8028AB2C >> 3) + 0xDF);
    } else {
      gDPSetEnvColor(D_8028A858++, D_8028AB20, D_8028AB24, D_8028AB28, (D_8028AB2C >> 1) + 0x7F);
    }
  } else {
    gDPSetEnvColor(D_8028A858++, D_8028AB20, D_8028AB24, D_8028AB28, 0xFF);
  }
  gDPSetRenderMode(D_8028A858++, D_8028C300, D_8028C304);
  gDPTileSync(D_8028A858++);
  gDPSetTextureLUT(D_8028A858++, 0);
  cw = 32;
  tpal = 0;
  ch = 128;
  gDPSetTextureImage(D_8028A858++, 2, 2, 1, D_8028C308[((unsigned char (*)[2])cls)[D_8028C328][*(int *)car->pad1dc8]]);
  {
    Gfx *_g = (Gfx *)(D_8028A858++);

    tgr_wr32(&_g->words.w0, 0xF5500000);
    tgr_wr32(&_g->words.w1, 0x0701FC5F);
  }
  gDPLoadSync(D_8028A858++);
  gDPLoadBlock(D_8028A858++, 7, 0, 0, (((tw) * (th) + 3) >> 2) - 1, CALC_DXT_4b(cw));
  gDPPipeSync(D_8028A858++);
  {
    Gfx *_g = (Gfx *)(D_8028A858++);

    tgr_wr32(&_g->words.w0, _SHIFTL(G_SETTILE, 24, 8) | _SHIFTL(2, 21, 3) | _SHIFTL(0, 19, 2) |
                   _SHIFTL((((cw) >> 1) + 7) >> 3, 9, 9) | _SHIFTL(0, 0, 9));
    tgr_wr32(&_g->words.w1, _SHIFTL(0, 24, 3) | _SHIFTL(tpal, 20, 4) | _SHIFTL(0, 18, 2) | _SHIFTL(7, 14, 4) |
                   _SHIFTL(15, 10, 4) | _SHIFTL(0, 8, 2) | _SHIFTL(5, 4, 4) | _SHIFTL(15, 0, 4));
  }
  {
    Gfx *_g = (Gfx *)(D_8028A858++); tgr_wr32(&_g->words.w0, _SHIFTL(G_SETTILESIZE, 24, 8) | _SHIFTL(0, 12, 12) | _SHIFTL(0, 0, 12));
    tgr_wr32(&_g->words.w1, _SHIFTL(0, 24, 3) | _SHIFTL(((cw) - 1) << 2, 12, 12) | _SHIFTL(((ch) - 1) << 2, 0, 12));
  }
  gDPLoadSync(D_8028A858++);
  gDPSetTextureImage(D_8028A858++, 0, 2, 1, D_8028C318[((unsigned char (*)[2])cls)[D_8028C328][*(int *)car->pad1dc8]]);
  gDPTileSync(D_8028A858++);
  gDPSetTile(D_8028A858++, 0, 0, 0, (256 + (((tpal) & 0xf) * 16)), 7, 0, 0, 0, 0, 0, 0, 0);
  gDPLoadSync(D_8028A858++);
  gDPLoadTLUTCmd(D_8028A858++, 7, 15);
  gDPPipeSync(D_8028A858++);
  gDPSetTextureLUT(D_8028A858++, 0x8000);
  scroll = tw - (int)(D_8028AAF0->heading * tw * D_802A9EE0);
  scroll = 0x1F - scroll;
  gDPSetTileSize(D_8028A858++, 0, scroll + 2, 2, scroll + 0x7E, 0x1FE);
  gDPPipeSync(D_8028A858++);
  gMoveWd(D_8028A858++, 0x0A, 0x00, 0);
  gMoveWd(D_8028A858++, 0x0A, 0x04, 0);
  gMoveWd(D_8028A858++, 0x0A, 0x20, 0);
  gMoveWd(D_8028A858++, 0x0A, 0x24, 0);
  gSPLookAtX(D_8028A858++, reflect);
  gSPLookAtY(D_8028A858++, reflect + 0x10);
  if (BE32(D_8028AB08->dl[lod][4]) != 0) {
    gSPDisplayList(D_8028A858++, BE32(D_8028AB08->dl[lod][4]));
  }
  if ((D_8028DDC8 != 0 || car->x2068 != 0) && BE32(D_8028AB08->dl[lod][9]) != 0) {
    gSPDisplayList(D_8028A858++, BE32(D_8028AB08->dl[lod][9]));
  }
  gSPPopMatrix(D_8028A858++, 0);
  gSPClearGeometryMode(D_8028A858++, 0x40000);
  gMoveWd(D_8028A858++, 0x0A, 0x00, colA);
  gMoveWd(D_8028A858++, 0x0A, 0x04, colA);
  gMoveWd(D_8028A858++, 0x0A, 0x20, colB);
  gMoveWd(D_8028A858++, 0x0A, 0x24, colB);
  gDPSetTextureFilter(D_8028A858++, D_8028A898);
  gDPSetTextureLUT(D_8028A858++, 0);
  if (car->colour[3] != 2) {
    BrCarDrawWheels(car);
  }
  gDPPipeSync(D_8028A858++);
  gDPSetCycleType(D_8028A858++, 0);
  gDPSetCombine(D_8028A858++, 0x121824, 0xFF33FFFF);
  D_8028AA3C += (int)tgr_rd32(D_8028AB08);     /* the model's first word, big-endian */
  }
}


/* WHAT IT DOES: Draw a car's body, when the frame draws cars and the
 * visibility pass marked its body: not the player's own car seen from its
 * third camera, nor a ghost.  Loads the car's model and lighting matrices,
 * the setup list, the model's textures, the body render state and the body
 * display list; for another car whose headlights face the camera, adds to
 * the screen flash by how squarely they point (above 0.95) over the squared
 * distance; then restores the state the track draw expects. */
/* @implements 0x80232ED4 tgr BrCarDrawBody */
void BrCarDrawBody(BrCar *car)
{
  if ((D_8028AA80 != 0 || D_8028AA8C != 0) && D_80351C70[car->slot] != 0) {
    if (car == D_8028AAF0 && D_8028AAF4 == (BrCar *)&D_8028AAF0->cams[2]) {
      return;
    }
    if (car->colour[3] == 2) {
      return;
    }
    D_8028AB08 = (BrCarModel *)TGR_PTR(char *, car->model);
    gRaw(D_8028A858++, 0x1060040, D_80351CA0[car->slot]);
    gRaw(D_8028A858++, 0x1030040, D_8028A878);
    gRaw(D_8028A858++, 0x39e0010, D_80351CB0[car->slot]);
    gRaw(D_8028A858++, 0x3980010, D_80351CB0[car->slot] + 0x10);
    gRaw(D_8028A858++, 0x39a0010, D_80351CB0[car->slot] + 0x20);
    gRaw(D_8028A858++, 0x39c0010, D_80351CB0[car->slot] + 0x30);
    gSPDisplayList(D_8028A858++, D_8028A9C8);
    BrTexLoad(5, BEPTR(void *, D_8028AB08->parts));
    gDPPipeSync(D_8028A858++);
    gRaw(D_8028A858++, 0xba001402, 0);
    gRaw(D_8028A858++, 0xbc00000a, 0);
    gRaw(D_8028A858++, 0xbc00040a, 0);
    gRaw(D_8028A858++, 0xbc00200a, 0xffffff00);
    gRaw(D_8028A858++, 0xbc00240a, 0xffffff00);
    gRaw(D_8028A858++, 0xfcffffff, 0xffff73b9);
    {
      Gfx *_g = D_8028A858++;

      tgr_wr32(&_g->words.w0, 0xb900031d);
      tgr_wr32(&_g->words.w1, 0x4049d8);
    }
    gRaw(D_8028A858++, 0xba000602, 0x80);
    if (BE32(D_8028AB08->dl[0][5]) != 0) {
      gSPDisplayList(D_8028A858++, BE32(D_8028AB08->dl[0][5]));
    }
    gDPPipeSync(D_8028A858++);
    gRaw(D_8028A858++, 0xba000602, D_8028A8A0);
    if (car != D_8028AAF0) {
      int unused;
      BrVec3 dir;
      BrVec3 ahead;
      float d;
      float len;

      BrVec3Add(&dir, (BrVec3 *)car->mtx0[3], (BrVec3 *)car->mtx0[0]);
      BrVec3Sub(&dir, (BrVec3 *)D_8028AAF4->mtx0[3], &dir);
      len = BrVec3Length(&dir);
      if (len != 0.0f) {
        BrVec3DivBy(&dir, len);
        if (BrVec3Dot(&dir, (BrVec3 *)D_8028AAF4->mtx0[0]) < 0.0f) {
          BrVec3Scale(&ahead, (BrVec3 *)car->mtx0[0], 1.0f);
          BrVec3MulAddTo(&ahead, (BrVec3 *)car->mtx0[2], -0.0f);
          d = BrVec3Dot(&dir, &ahead) * -BrVec3Dot(&dir, (BrVec3 *)D_8028AAF4->mtx0[0]);
          if (d > 0.95f) {
            len = len * len;
            D_8028B750 += 750.0f * (d - 0.95f) / len;
          }
        }
      }
    }
    gDPPipeSync(D_8028A858++);
    gRaw(D_8028A858++, 0xba001402, 0);
    gRaw(D_8028A858++, 0xbd000000, 0);
    gRaw(D_8028A858++, 0xb6000000, 0x40000);
    gRaw(D_8028A858++, 0xbc000002, 0x80000040);
    gRaw(D_8028A858++, 0x3860010, D_8028A9F8);
    gRaw(D_8028A858++, 0x3880010, D_8028A9F0);
    gRaw(D_8028A858++, 0xba000c02, D_8028A898);
    gRaw(D_8028A858++, 0xba000e02, 0);
    gRaw(D_8028A858++, 0xfc121824, 0xff33ffff);
  }
}
