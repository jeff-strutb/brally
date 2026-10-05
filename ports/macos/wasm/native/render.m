/* render.m -- the native 3D renderer's triangle leaves
 * (ports/macos/NATIVE_RENDERER.md 4.3).
 *
 * The display-list machine stays game code: the command loop, the matrix
 * stack, the lighting and texture-generation vertex routines all run as the
 * original's, and each G_VTX leaves its vertices in the pool at 0x105CE318
 * with their clip-space position (cx, cy, cz, cw), their final colour in the
 * three slots after it (the lit routines write the lit colour there, the
 * unlit ones the source bytes) and their texture coordinates (s, t).
 *
 * The original's triangle leaves then did the rest on the CPU: the divide by
 * w, the viewport transform, a quarter-pixel snap, a seven-plane polygon
 * clipper, and a Glide call per triangle at 640x480.  These replacements
 * hand the three pool vertices to the host in clip space instead
 * (hglide_tri_h), and the GPU projects, clips and rasterises them at the
 * target's size.  What each original leaf decided stays decided the same way:
 * which pool entries make a triangle and in what order, the trivial reject
 * (every corner outside one plane), flat shading (every corner takes the
 * first corner's colour), and the texture scale of the bound texture.
 *
 * BR_GLIDE3D=1 runs the original leaves instead, for side-by-side checks.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "w2c_native.h"

void hglide_tri_h(const float *a, const float *b, const float *c, int noz, int view);
void hglide_tri_shadow(const float *a, const float *b, const float *c);
void hfx_set_cam(const float *P, float sx, float tx, float sy, float ty);
void hter_seam(void);   /* host_terrain.m */
int hfx_on(void);
unsigned hglide_swaps(void);

/* the vertex pool (br_dl.h, BrDlVtx) */
#define POOL        0x105CE318u
#define VSZ         0x68u
#define V_R         0x0C
#define V_A         0x1C
#define V_OUTCODE   0x3C
#define V_CX        0x44
#define V_CY        0x48
#define V_CZ        0x4C
#define V_S         0x50
#define V_T         0x54
#define V_CW        0x58
#define V_N0        0x5C

/* viewport (screen = trans + scale * clip / w) and the bound texture's scale */
#define VP_SCALE_X  0x105CCD48u
#define VP_TRANS_X  0x105CD9F8u
#define VP_SCALE_Y  0x105CCFDCu
#define VP_TRANS_Y  0x105CD9FCu
#define TEX_SCALE_S 0x118ED1A4u
#define TEX_SCALE_T 0x118ED1A8u

static int glide3d(void)
{
    static int v = -1;
    if (v < 0) v = getenv("BR_GLIDE3D") && atoi(getenv("BR_GLIDE3D"));
    return v;
}

#define F(a) W_LD(f32, (a), 0)

/* The fx G-buffer (host_fx.m) wants each corner's world position.  The
 * display-list machine's projection slot 0x105CCD00 holds the camera's view
 * x projection (BrCamMatrixSetup's g_BrCurMat), so a clip-space corner times
 * its inverse is the corner in the world.  The first depth-buffered triangle
 * of a frame names the main camera; triangles drawn through another (the
 * rear-view mirror) are marked and left unlit. */
#define PROJ 0x105CCD00u
static float g_P[16], g_mainP[16];
static double g_IP[16];
static int g_have_main, g_cur_main;
static unsigned g_serial = ~0u;

static void inv4(const double *m, double *o)
{
    double a[4][8];
    int i, j, k;
    for (i = 0; i < 4; i++) for (j = 0; j < 8; j++) a[i][j] = j < 4 ? m[i * 4 + j] : (j - 4 == i);
    for (i = 0; i < 4; i++) {
        int p = i; double t;
        for (k = i + 1; k < 4; k++) if (fabs(a[k][i]) > fabs(a[p][i])) p = k;
        for (j = 0; j < 8; j++) { t = a[i][j]; a[i][j] = a[p][j]; a[p][j] = t; }
        t = a[i][i]; if (t == 0) t = 1e-30;
        for (j = 0; j < 8; j++) a[i][j] /= t;
        for (k = 0; k < 4; k++) if (k != i) { t = a[k][i]; for (j = 0; j < 8; j++) a[k][j] -= t * a[i][j]; }
    }
    for (i = 0; i < 4; i++) for (j = 0; j < 4; j++) o[i * 4 + j] = a[i][j + 4];
}

/* Which view a triangle belongs to, from the projection and viewport it is
 * drawn through: 1 flat 2D (an orthographic projection, whose w does not
 * depend on the position: the HUD and text), 2 the rear-view mirror, 0 the
 * camera.  host_glide.m places each kind on the screen its own way (any
 * window shape).
 *
 * The mirror is told by its HEIGHT, not by the sign of its width.
 * BrFrameDraw (0x10011FA0) draws it into a strip w/4..3w/8+2 wide and a
 * quarter of that tall: at most 62 pixels of the 480, a viewport half-height
 * (VP_SCALE_Y) of at most 31.  The camera's view is never under 120 pixels
 * (split screen halves it).  A negative width is not the mirror's alone: the
 * mirrored tracks (the track selector's second round, indices 6..11) flip
 * the camera's own view with one, and the old test took the whole race for
 * the mirror -- no Remastered lighting, sky or ground on a mirrored track. */
static int view_now(void)
{
    if (F(PROJ + 12) == 0 && F(PROJ + 28) == 0 && F(PROJ + 44) == 0) return 1;
    return fabsf(F(VP_SCALE_Y)) < 48.0f ? 2 : 0;
}
int hrender_view(void) { return view_now(); }

static void fx_camera(int noz, int view)
{
    float P[16];
    int i;
    unsigned sw = hglide_swaps();
    for (i = 0; i < 16; i++) P[i] = F(PROJ + 4 * i);
    if (sw != g_serial) { g_serial = sw; g_have_main = 0; }
    if (memcmp(P, g_P, sizeof P)) {
        double d[16];
        memcpy(g_P, P, sizeof P);
        for (i = 0; i < 16; i++) d[i] = P[i];
        inv4(d, g_IP);
    }
    if (!g_have_main && !noz && view == 0) {
        memcpy(g_mainP, P, sizeof P);
        g_have_main = 1;
        hfx_set_cam(P, F(VP_SCALE_X), F(VP_TRANS_X), F(VP_SCALE_Y), F(VP_TRANS_Y));
    }
    /* the backdrop is drawn before the frame's first depth-buffered
     * triangle: it belongs to the main view */
    g_cur_main = g_have_main ? !memcmp(P, g_mainP, sizeof P) : noz;
}

/* One corner for the host: Glide screen space, homogeneous. */
static void corner(float *o, u32 v, u32 colour_from)
{
    float cx = F(v + V_CX), cy = F(v + V_CY), cw = F(v + V_CW);
    o[0] = F(VP_SCALE_X) * cx + F(VP_TRANS_X) * cw;
    o[1] = F(VP_SCALE_Y) * cy + F(VP_TRANS_Y) * cw;
    o[2] = F(v + V_CZ);
    o[3] = cw;
    o[4] = F(colour_from + V_N0);
    o[5] = F(colour_from + V_N0 + 4);
    o[6] = F(colour_from + V_N0 + 8);
    o[7] = F(v + V_A);
    o[8] = F(v + V_S) * F(TEX_SCALE_S);
    o[9] = F(v + V_T) * F(TEX_SCALE_T);
    {
        double c[4] = { F(v + V_CX), F(v + V_CY), F(v + V_CZ), F(v + V_CW) }, h[4];
        int j;
        for (j = 0; j < 4; j++) h[j] = c[0] * g_IP[j] + c[1] * g_IP[4 + j] + c[2] * g_IP[8 + j] + c[3] * g_IP[12 + j];
        if (h[3] == 0) h[3] = 1e-30;
        o[10] = (float)(h[0] / h[3]); o[11] = (float)(h[1] / h[3]); o[12] = (float)(h[2] / h[3]);
        o[13] = g_cur_main ? 1.0f : 3.0f;
    }
}

/* Draw pool entries ia, ib, ic; `flat` gives every corner ia's colour; `noz`
 * for the leaves the no-Z vertex routines pair with (their 1/w is 1/65535). */
static void tri(u32 ia, u32 ib, u32 ic, int flat, int noz)
{
    u32 a = POOL + ia * VSZ, b = POOL + ib * VSZ, c = POOL + ic * VSZ;
    float va[14], vb[14], vc[14];
    int out = W_LD(s32, a + V_OUTCODE, 0) & W_LD(s32, b + V_OUTCODE, 0) & W_LD(s32, c + V_OUTCODE, 0);
    int view = view_now();
    if (out && (noz || view == 1 || !hfx_on()))
        return;                          /* all outside one plane */
    if (hfx_on()) fx_camera(noz, view); else g_cur_main = 0;
    /* the Remastered landscape goes in once per view, before that view's
     * first depth-buffered triangle: after the backdrop, under everything */
    if (!noz && view != 1 && hfx_on()) hter_seam();
    corner(va, a, a);
    corner(vb, b, flat ? a : b);
    corner(vc, c, flat ? a : c);
    if (getenv("BR_VIEWLOG")) {          /* debug: the views a frame draws through */
        static unsigned last = ~0u, n[3]; static float sx[3];
        unsigned sw = hglide_swaps();
        if (sw != last) {
            if (last != ~0u && sw % 120 == 0)
                fprintf(stderr, "views: swap %u camera %u (vp %.1f) 2d %u (vp %.1f) mirror %u (vp %.1f)\n",
                        sw, n[0], sx[0], n[1], sx[1], n[2], sx[2]);
            last = sw; n[0] = n[1] = n[2] = 0;
        }
        n[view]++; sx[view] = F(VP_SCALE_X);
    }
    if (out) hglide_tri_shadow(va, vb, vc);   /* off screen, but it may shade what is on it */
    else hglide_tri_h(va, vb, vc, noz, view);
}

#define B(p, i) W_LD(u8, (p) + (i), 0)

/* WHY: the G_TRI1 leaf; the GPU projects and clips (see the header). */
/* @replaces 0x1001ECF0 BrDlCmdTri1 */
u32 n_BrDlCmdTri1(u32 p)
{
    if (glide3d()) return W_ORIG_BrDlCmdTri1(p);
    tri(B(p, 6), B(p, 5), B(p, 4), 0, 0);
    return p + 8;
}

/* WHY: the G_TRI2 leaf, two triangles per command. */
/* @replaces 0x1001FA30 BrDlCmdTri2 */
u32 n_BrDlCmdTri2(u32 p)
{
    if (glide3d()) return W_ORIG_BrDlCmdTri2(p);
    tri(B(p, 2), B(p, 1), B(p, 0), 0, 0);
    tri(B(p, 6), B(p, 5), B(p, 4), 0, 0);
    return p + 8;
}

/* WHY: G_TRI1 with the depth buffer off; the depth state already says so. */
/* @replaces 0x10020900 BrDlCmdTri1NoZ */
u32 n_BrDlCmdTri1NoZ(u32 p)
{
    if (glide3d()) return W_ORIG_BrDlCmdTri1NoZ(p);
    tri(B(p, 6), B(p, 5), B(p, 4), 0, 1);
    return p + 8;
}

/* WHY: G_TRI2 with the depth buffer off. */
/* @replaces 0x10020D70 BrDlCmdTri2NoZ */
u32 n_BrDlCmdTri2NoZ(u32 p)
{
    if (glide3d()) return W_ORIG_BrDlCmdTri2NoZ(p);
    tri(B(p, 2), B(p, 1), B(p, 0), 0, 1);
    tri(B(p, 6), B(p, 5), B(p, 4), 0, 1);
    return p + 8;
}

/* WHY: the flat-shaded emitter the four G_TRI*_FLAT commands share. */
/* @replaces 0x1001FF60 BrDlTriFlatZ */
void n_BrDlTriFlatZ(u32 i0, u32 i1, u32 i2)
{
    if (glide3d()) { W_ORIG_BrDlTriFlatZ(i0, i1, i2); return; }
    tri(i0, i1, i2, 1, 0);
}

/* WHY: the same, depth buffer off. */
/* @replaces 0x10020460 BrDlTriFlatNoZ */
void n_BrDlTriFlatNoZ(u32 i0, u32 i1, u32 i2)
{
    if (glide3d()) { W_ORIG_BrDlTriFlatNoZ(i0, i1, i2); return; }
    tri(i0, i1, i2, 1, 1);
}
