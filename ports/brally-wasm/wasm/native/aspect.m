/* aspect.m -- any window shape (port code).
 *
 * The game draws for a 4:3 screen: its camera's lens (BrCamMatrixSetup)
 * and the wedge of the world it culls against (BrCamFrustumBuild) are made
 * from each view's 640x480 rectangle.  host_glide.m fills a window of any
 * shape: in a race frame it stretches the game's 640x480 over the whole
 * target, so the camera here is widened by the same factor and nothing is
 * distorted -- the view keeps the game's vertical angle and sees more to
 * the sides in a wide window (Hor+), and keeps its horizontal angle and
 * sees more above and below in a tall one (Vert+).  The culling wedge grows
 * with it, so the edges of a wide view are drawn, not left empty.
 *
 * Only the race's own frames (BrFrameDraw) are widened; menus, and any 3D
 * in them, are the 4:3 picture centred.  The rear-view mirror keeps its own
 * shape (host_glide.m draws it that size, centred), so its camera is left
 * as the game made it.
 */
#include <math.h>
#include "w2c_native.h"

void hglide_set_wide(int on);
void hframe_race_frame(void);
void hglide_view_scale(float *kx, float *ky);

#define G_CAR         0x106E9D88u   /* the car whose view is being drawn     */
#define CAR_MIRRORCAM 0x2890u       /* its rear-view mirror camera            */
#define K_FOV1        0x100774E0u   /* the lens constants BrCamMatrixSetup    */
#define K_FOV2        0x100774E4u   /*   turns the view angle into fovy with  */
#define CAM_EYE       0x106EA3A0u   /* BrCamFrustumBuild's results (br_camera.c) */
#define CAM_CENTRE    0x106E9A20u
#define CAM_EXT_R     0x106EC788u
#define CAM_EXT_U     0x106E7720u
#define CAM_CORNER0   0x106EA3ACu
#define CAM_CORNER1   0x106EA3B8u
#define CAM_CORNER2   0x106EA3C4u
#define CAM_CORNER3   0x106EA3D0u

static int g_race;                  /* inside BrFrameDraw */

static int widen(u32 cam, float *kx, float *ky)
{
    if (!g_race || cam == W_LD(u32, G_CAR, 0) + CAR_MIRRORCAM) return 0;
    hglide_view_scale(kx, ky);
    return *kx > 1.0f || *ky > 1.0f;
}

/* WHY: a race frame fills the window (host_glide.m's screen map); this
 * tells the renderer which frames are the race's. */
/* @replaces 0x10011FA0 BrFrameDraw */
void n_BrFrameDraw(u32 slot)
{
    W_TRACE("n_BrFrameDraw");
    hglide_set_wide(1);
    hframe_race_frame();
    g_race = 1;
    W_ORIG_BrFrameDraw(slot);
    g_race = 0;
}

/* WHY: the camera's lens for a window wider (or taller) than 4:3.  The game
 * makes fovy = angle * K1 * (h / w) * K2 and aspect = w / h from the view's
 * rectangle; the view is given the rectangle it fills on the target and the
 * angle that keeps fovy (Hor+), or the fovy that keeps the horizontal angle
 * (Vert+). */
/* @replaces 0x1002D534 BrCamMatrixSetup */
void n_BrCamMatrixSetup(u32 cam, f32 angle, f32 far_, f32 w, f32 h)
{
    float kx, ky;
    double k, fovy, fovy2, w2, h2;
    W_TRACE("n_BrCamMatrixSetup");
    if (!widen(cam, &kx, &ky) || w <= 0 || h <= 0) { W_ORIG_BrCamMatrixSetup(cam, angle, far_, w, h); return; }
    k = (double)W_LD(f32, K_FOV1, 0) * W_LD(f32, K_FOV2, 0);
    fovy = angle * k * (h / w);                          /* degrees */
    w2 = w * kx; h2 = h * ky;
    fovy2 = ky > 1.0f ? 2.0 * atan(ky * tan(fovy * M_PI / 360.0)) * 180.0 / M_PI : fovy;
    W_ORIG_BrCamMatrixSetup(cam, (f32)(fovy2 / (k * (h2 / w2))), far_, (f32)w2, (f32)h2);
}

/* WHY: the culling wedge for the same widened view: the game's, with its
 * half-width grown by kx and half-height by ky, the four corners rebuilt as
 * the game builds them (centre +- right +- up, pulled three-quarters of the
 * way back toward the eye). */
/* @replaces 0x1002D362 BrCamFrustumBuild */
void n_BrCamFrustumBuild(u32 cam, f32 angle, f32 dist, f32 w, f32 h)
{
    static const u32 CORNER[4] = { CAM_CORNER0, CAM_CORNER1, CAM_CORNER2, CAM_CORNER3 };
    static const float SR[4] = { 1, -1, -1, 1 }, SU[4] = { 1, 1, -1, -1 };
    float kx, ky, e[3], c[3], r[3], u[3];
    int i, j;
    W_TRACE("n_BrCamFrustumBuild");
    W_ORIG_BrCamFrustumBuild(cam, angle, dist, w, h);
    if (!widen(cam, &kx, &ky)) return;
    for (j = 0; j < 3; j++) {
        e[j] = W_LD(f32, CAM_EYE, 4 * j);
        c[j] = W_LD(f32, CAM_CENTRE, 4 * j);
        r[j] = W_LD(f32, CAM_EXT_R, 4 * j) * kx;
        u[j] = W_LD(f32, CAM_EXT_U, 4 * j) * ky;
        W_ST(f32, CAM_EXT_R, 4 * j, r[j]);
        W_ST(f32, CAM_EXT_U, 4 * j, u[j]);
    }
    for (i = 0; i < 4; i++)
        for (j = 0; j < 3; j++) {
            float v = c[j] + SR[i] * r[j] + SU[i] * u[j];
            W_ST(f32, CORNER[i], 4 * j, (v - e[j]) * 0.75f + e[j]);
        }
}
