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
/* -- end declarations -- */

#define OS_K0_TO_PHYSICAL(x) ((unsigned int)((char *)(x) - 0x80000000))

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
