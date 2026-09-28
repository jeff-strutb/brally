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

#define OS_K0_TO_PHYSICAL(x) ((unsigned int)((char *)(x) - 0x80000000))

/* WHAT IT DOES: Work out the race camera's view frustum for culling: the
 * far plane's centre (far along the camera's first axis) and its four
 * corners (sin(fov) * far across the side axis, scaled by h / w up the
 * third, halved for a split screen), each corner then moved three quarters
 * of the way back toward the eye; the field of view and depth are kept. */
/* @implements 0x8021B0C4 tgr BrFrustumSet */
void BrFrustumSet(float m[4][4], float fov, float far, float w, float h)
{
  float halfW;
  float halfH;

  halfW = sinf(fov) * far;
  halfH = halfW * h / w;
  if (D_8028AB0C == 2) {
    halfH *= 0.5f;
  }
  D_8031B1F0.eye[0] = m[3][0];
  D_8031B1F0.eye[1] = m[3][1];
  D_8031B1F0.eye[2] = m[3][2];
  BrVec3MulAdd(D_8031ABF0, D_8031B1F0.eye, m[0], far);
  BrVec3Scale(D_8031ABD0, m[1], halfW);
  BrVec3Scale(D_8031ABE0, m[2], halfH);
  D_8031B1F0.centre[0] = D_8031ABF0[0];
  D_8031B1F0.centre[1] = D_8031ABF0[1];
  D_8031B1F0.centre[2] = D_8031ABF0[2];
  BrVec3Add(D_8031B1F0.corner[0], D_8031ABF0, D_8031ABD0);
  BrVec3AddTo(D_8031B1F0.corner[0], D_8031ABE0);
  BrVec3Add(D_8031B1F0.corner[3], D_8031ABF0, D_8031ABD0);
  BrVec3SubFrom(D_8031B1F0.corner[3], D_8031ABE0);
  BrVec3Sub(D_8031B1F0.corner[1], D_8031ABF0, D_8031ABD0);
  BrVec3AddTo(D_8031B1F0.corner[1], D_8031ABE0);
  BrVec3Sub(D_8031B1F0.corner[2], D_8031ABF0, D_8031ABD0);
  BrVec3SubFrom(D_8031B1F0.corner[2], D_8031ABE0);
  BrVec3Lerp(D_8031B1F0.corner[1], D_8031B1F0.corner[1], D_8031B1F0.eye, 0.75f);
  BrVec3Lerp(D_8031B1F0.corner[2], D_8031B1F0.corner[2], D_8031B1F0.eye, 0.75f);
  BrVec3Lerp(D_8031B1F0.corner[0], D_8031B1F0.corner[0], D_8031B1F0.eye, 0.75f);
  BrVec3Lerp(D_8031B1F0.corner[3], D_8031B1F0.corner[3], D_8031B1F0.eye, 0.75f);
  D_8028AACC = far;
  D_8028AAC4 = fov;
}

/* WHAT IT DOES: Set the race camera from a camera matrix: look from its
 * position along its first axis with its third as up, and project with the
 * given field of view (scaled for the viewport's shape, in radians) and far
 * plane, near plane 2.4; the combined matrix goes to a fresh Mtx. */
/* @implements 0x8021B2F8 tgr BrCameraSet */
void BrCameraSet(float m[4][4], float fov, float far, float w, float h)
{
  guLookAtF(D_8031AAD0, m[3][0], m[3][1], m[3][2], m[3][0] + m[0][0], m[3][1] + m[0][1],
            m[3][2] + m[0][2], m[2][0], m[2][1], m[2][2]);
  D_8028A870 = far;
  D_8028A86C = 2.4f;
  guPerspectiveF(D_8031AA90, &D_8028A874, fov * 1.3333334f * (h / w) * 57.295776f, w / h,
                 D_8028A86C, D_8028A870, 1.0f);
  guMtxCatF(D_8031AAD0, D_8031AA90, D_8031AA50);
  D_8028A878 = BrMtxAlloc();
  guMtxF2L(D_8031AA50, D_8028A878);
}

/* WHAT IT DOES: Set the front end's 3D camera: looking straight into a
 * 1024x768 plane from z 1000, 45 degree field of view, 4:3; load it as the
 * projection. */
/* @implements 0x8021B458 tgr BrMenuCameraSet */
void BrMenuCameraSet(float w, float h)
{
  guLookAtF(D_8031AAD0, 512.0f, 384.0f, 1000.0f, 512.0f, 384.0f, 0.0f, 0.0f, 1.0f, 0.0f);
  guPerspectiveF(D_8031AB50, &D_8028A874, 45.0f, 1.3333334f, 10.0f, 2000.0f, 1.0f);
  guMtxCatF(D_8031AAD0, D_8031AB50, D_8031AA50);
  gSPPerspNormalize(D_8028A858++, D_8028A874);
  D_8028A878 = BrMtxAlloc();
  guMtxF2L(D_8031AA50, D_8028A878);
  gSPMatrix(D_8028A858++, OS_K0_TO_PHYSICAL(D_8028A878), G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
}

/* WHAT IT DOES: Set a 320x240 screen-space camera (from z 290, 45 degrees,
 * 4:3), near 1 and far 500 unless a caller set its own planes for this one
 * frame; load it as the projection. */
/* @implements 0x8021B5A4 tgr BrScreenCameraSet */
void BrScreenCameraSet(float w, float h)
{
  guLookAtF(D_8031AAD0, 160.0f, 120.0f, 290.0f, 160.0f, 120.0f, 0.0f, 0.0f, 1.0f, 0.0f);
  if (D_8028A868 != 0) {
    D_8028A868 = 0;
  } else {
    D_8028A86C = 1.0;
    D_8028A870 = 500.0f;
  }
  guPerspectiveF(D_8031AA90, &D_8028A874, 45.0f, 1.3333334f, D_8028A86C, D_8028A870, 1.0f);
  guMtxCatF(D_8031AAD0, D_8031AA90, D_8031AA50);
  gSPPerspNormalize(D_8028A858++, D_8028A874);
  D_8028A878 = BrMtxAlloc();
  guMtxF2L(D_8031AA50, D_8028A878);
  gSPMatrix(D_8028A858++, OS_K0_TO_PHYSICAL(D_8028A878), G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
}

/* WHAT IT DOES: The screen rectangle a ball of radius r at pos covers in the
 * current view: project the centre through the camera, and unless it is on
 * the eye plane (|w| <= 0.001) put the corners r / w either side of it
 * (x flipped for a mirrored view), in pixels from the view's centre (y up);
 * lo gets the lower corner, hi the upper.
 * RESIDUE (28, same 114 instructions): the ROM spills the half-sizes to
 * 0x24/0x20, two words below where ours land (0x2C/0x28), with nothing
 * stored in between; and its temporaries are numbered differently from the
 * first load.  The unused float[4] reproduces the 16-byte hole above cx. */
/* @implements 0x80233E10 tgr BrProjectExtent */
void BrProjectExtent(float pos[3], int r, short *lo, short *hi)
{
  float p[4];
  float q[4];
  float spare[4];
  int cx;
  int cy;
  int hw;
  int hh;
  float inv;
  float s;

  hw = D_8031B2C8[D_8028AAEC].w >> 1;
  hh = D_8031B2C8[D_8028AAEC].h >> 1;
  cx = D_8031B2C8[D_8028AAEC].x + hw;
  cy = D_8031B2C8[D_8028AAEC].y + hh;
  BrMat4TransformPoint4(p, pos, D_8031AA50);
  if (!(p[3] <= 0.001f && -0.001f <= p[3])) {
    inv = 1.0f / p[3];
    if (D_8028A8A8 != D_8028A8AC) {
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
