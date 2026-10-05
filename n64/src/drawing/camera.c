/* camera.c -- the view and projection matrices for the race and the menus
 */
#include "tgr/common.h"
#include "tgr/gbi.h"

/* -- declarations -- */
void guLookAtF(float mf[4][4], float xEye, float yEye, float zEye, float xAt, float yAt, float zAt,
               float xUp, float yUp, float zUp);
void guPerspectiveF(float mf[4][4], unsigned short *perspNorm, float fovy, float aspect, float near,
                    float far, float scale);
void guMtxCatF(float m[4][4], float n[4][4], float r[4][4]);
void guMtxF2L(float mf[4][4], Mtx *m);
Mtx *BrMtxAlloc(void);
extern Gfx *D_8028A858;
extern int D_8028A868;
extern float D_8028A86C;
extern float D_8028A870;
extern unsigned short D_8028A874;
extern Mtx *D_8028A878;
extern float D_8031AA50[4][4];
extern float D_8031AA90[4][4];
extern float D_8031AAD0[4][4];
extern float D_8031AB50[4][4];
float sinf(float x);
void BrVec3MulAdd(float out[3], float a[3], float b[3], float s);
void BrVec3Scale(float out[3], float v[3], float s);
void BrVec3Add(float out[3], float a[3], float b[3]);
void BrVec3AddTo(float v[3], float d[3]);
void BrVec3Sub(float out[3], float a[3], float b[3]);
void BrVec3SubFrom(float v[3], float d[3]);
void BrVec3Lerp(float out[3], float a[3], float b[3], float t);
extern int D_8028AB0C;                  /* number of players */
extern float D_8028AAC4;                /* the view frustum's field of view */
extern float D_8028AACC;                /* and its depth */
typedef struct BrFrustum {
  float eye[3];                         /* 0x00 */
  float corner[4][3];                   /* 0x0C  pulled three quarters of the way to the eye */
  float centre[3];                      /* 0x3C  the far plane's centre */
} BrFrustum;
extern BrFrustum D_8031B1F0;
extern float D_8031ABD0[3];             /* half the far plane's width, along the side axis */
extern float D_8031ABE0[3];             /* half its height, along the up axis */
extern float D_8031ABF0[3];             /* its centre */
void BrMat4TransformPoint4(float out[4], float v[3], float m[4][4]);
typedef struct BrViewRect { int x; int y; int w; int h; int car; } BrViewRect;
extern BrViewRect D_8031B2C8[2];        /* the players' views */
extern int D_8028AAEC;                  /* the view being drawn */
extern int D_8028A8A8;                  /* mirror flags: they differ when the view is mirrored */
extern int D_8028A8AC;
/* -- end declarations -- */

/* WHAT IT DOES: The screen rectangle a ball of radius r at pos covers in the
 * current view: project the centre through the camera, and unless it is on
 * the eye plane (|w| <= 0.001) put the corners r / w either side of it
 * (x flipped for a mirrored view), in pixels from the view's centre (y up);
 * lo gets the lower corner, hi the upper.
 * The view's width and height are locals of their own (with inv and s
 * they fill the four words above cx), the centre is offset by the halved
 * size before the half-sizes are kept, and the mirror test is a
 * subtraction. */
/* @implements 0x80233E10 tgr BrProjectExtent */
void BrProjectExtent(float pos[3], int r, short *lo, short *hi)
{
  float p[4];
  float q[4];
  int w;
  int h;
  float inv;
  float s;
  int cx;
  int cy;
  int hw;
  int hh;

  w = D_8031B2C8[D_8028AAEC].w;
  h = D_8031B2C8[D_8028AAEC].h;
  cx = D_8031B2C8[D_8028AAEC].x + (w >> 1);
  cy = D_8031B2C8[D_8028AAEC].y + (h >> 1);
  hw = w >> 1;
  hh = h >> 1;
  BrMat4TransformPoint4(p, pos, D_8031AA50);
  if (!(p[3] <= 0.001f && -0.001f <= p[3])) {
    inv = 1.0f / p[3];
    if (D_8028A8AC - D_8028A8A8) {
      p[0] *= -inv;
    } else {
      p[0] *= inv;
    }
    p[1] *= inv;
    s = r * inv;
    q[0] = p[0] + s;
    p[0] = p[0] - s;
    q[1] = p[1] + s;
    p[1] = p[1] - s;
    lo[0] = (int)(p[0] * hw) + cx;
    lo[1] = cy - (int)(p[1] * hh);
    hi[0] = (int)(q[0] * hw) + cx;
    hi[1] = cy - (int)(q[1] * hh);
  }
}
