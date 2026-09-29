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
#include <stdlib.h>
#include "w2c_native.h"

void hglide_tri_h(const float *a, const float *b, const float *c, int noz);

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
}

/* Draw pool entries ia, ib, ic; `flat` gives every corner ia's colour; `noz`
 * for the leaves the no-Z vertex routines pair with (their 1/w is 1/65535). */
static void tri(u32 ia, u32 ib, u32 ic, int flat, int noz)
{
    u32 a = POOL + ia * VSZ, b = POOL + ib * VSZ, c = POOL + ic * VSZ;
    float va[10], vb[10], vc[10];
    if (W_LD(s32, a + V_OUTCODE, 0) & W_LD(s32, b + V_OUTCODE, 0) & W_LD(s32, c + V_OUTCODE, 0))
        return;                          /* all outside one plane */
    corner(va, a, a);
    corner(vb, b, flat ? a : b);
    corner(vc, c, flat ? a : c);
    hglide_tri_h(va, vb, vc, noz);
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
