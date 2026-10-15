/* brr_shadow.h: the sun's cascaded shadows, the parts that are plain
 * arithmetic (brr_metal.m; ports/brally/tools/lighttest checks them against
 * brute force): the cascades' fit, the static cache's bookkeeping, and the
 * camera's map from world to the lit target's pixels.  Plain C, no Metal. */
#ifndef BRR_SHADOW_H
#define BRR_SHADOW_H

#include <math.h>
#include <string.h>
#include <stddef.h>

#define SHADOW_SIZE 2048
#define CASCADES 4
#define SHADOW_FAR 300.0f       /* metres: the cascades cover the view this far */
#define SHADOW_SNAP 8           /* a cascade's window moves in steps of an eighth of itself */
#define SHADOW_LOSTEP 16.0f     /* metres: the casters' reach toward the sun moves in these steps */
#define SUN_HALF_DEG 0.2675f    /* the sun's angular radius (its disc is 0.535 degrees across) */
#define PCSS_SEARCH_MAX 16.0f   /* texels: the blocker search's widest radius */
#define PCSS_FILTER_MAX 16.0f   /* texels: the penumbra filter's widest radius */
#define CONTACT_LEN 0.5f        /* metres: the screen-space contact shadow's march toward the sun */
#define SHADOW_NOFS 0.3f        /* texels: the receiver's offset along its normal (brr_metal_lit.h casvis) */
#define SHADOW_NOFS_SIN 1.0f    /* texels: more of it times the sine of its angle from the sun */
#define SHADOW_ZBIAS 0.5f       /* texels of depth: the comparison's bias for the map's rounding */

/* a cascade: its matrix (world to light clip, column-major: x, y in -1..1,
 * depth 0..1 from the casters' reach toward the sun), the view depth it ends
 * at, metres per texel, one texel in its depth units, and metres per unit of
 * its depth */
typedef struct BrrCascade { float m[16]; float split, texel, bias, depth; } BrrCascade;

/* the cascades over the view: the camera's view out to SHADOW_FAR split at
 * the practical-scheme distances (lambda 0.7, Zhang 2006), each split's
 * slice fitted by its bounding sphere (the same at any turn of the view) in
 * an orthographic window looking along the light.  The window is a little
 * wider than the sphere (SNAP/(SNAP-1)) and moves in whole steps of an
 * SNAP-th of itself, a whole number of texels: anywhere within a step the
 * sphere stays inside, so the matrix holds still, to the bit, until the
 * slice crosses a step, which is what lets the static casters' depth be kept
 * from frame to frame (brr_shc_plan); and a texel's place in the world never
 * moves, so shadow edges do not crawl.  Its depth reaches back toward the sun
 * to lo (the farthest caster along the light: the caller's track extent and
 * moving objects), taken down to a SHADOW_LOSTEP step so it moves rarely.
 * ray[4]: the view's corner rays, fwd its forward axis (unit). */
static void brr_cascade_fit(const float sun[3], const float eye[3], const float ray[4][3], const float fwd[3],
                            float lo, int size, BrrCascade out[CASCADES])
{
    float f[3] = { -sun[0], -sun[1], -sun[2] }, up[3] = { 0, 0, 1 }, r[3], v[3], len;
    float near = 0.5f, far = SHADOW_FAR, lambda = 0.7f;
    int k, c, j;
    r[0] = f[1] * up[2] - f[2] * up[1]; r[1] = f[2] * up[0] - f[0] * up[2]; r[2] = f[0] * up[1] - f[1] * up[0];
    len = sqrtf(r[0] * r[0] + r[1] * r[1] + r[2] * r[2]);
    if (len < 1e-4f) {                                  /* the sun overhead: any horizontal axis will do */
        r[0] = 1; r[1] = 0; r[2] = 0; len = 1;
    }
    for (k = 0; k < 3; k++) r[k] /= len;
    v[0] = r[1] * f[2] - r[2] * f[1]; v[1] = r[2] * f[0] - r[0] * f[2]; v[2] = r[0] * f[1] - r[1] * f[0];
    lo = floorf(lo / SHADOW_LOSTEP) * SHADOW_LOSTEP;
    for (c = 0; c < CASCADES; c++) {
        float d0 = c ? out[c - 1].split : near, t = (float)(c + 1) / CASCADES;
        float d1 = lambda * near * powf(far / near, t) + (1.0f - lambda) * (near + (far - near) * t);
        float pt[8][3], ctr[3] = { 0, 0, 0 }, rad = 0, R, q[3], texel, step, sx, sz, zlo, zhi;
        float *m = out[c].m;
        for (j = 0; j < 8; j++) {
            float d = j < 4 ? d0 : d1;
            const float *rr = ray[j & 3];
            float tt = d / (rr[0] * fwd[0] + rr[1] * fwd[1] + rr[2] * fwd[2]);
            for (k = 0; k < 3; k++) {
                pt[j][k] = eye[k] + rr[k] * tt;
                ctr[k] += pt[j][k] / 8.0f;
            }
        }
        for (j = 0; j < 8; j++) {
            float dd[3] = { pt[j][0] - ctr[0], pt[j][1] - ctr[1], pt[j][2] - ctr[2] };
            rad = fmaxf(rad, sqrtf(dd[0] * dd[0] + dd[1] * dd[1] + dd[2] * dd[2]));
        }
        rad = ceilf(rad * 16.0f) / 16.0f;               /* a radius that does not flicker with the view's turn */
        R = ceilf(rad * (float)SHADOW_SNAP / (float)(SHADOW_SNAP - 1) * 16.0f) / 16.0f;
        texel = 2.0f * R / (float)size;
        step = texel * (float)(size / SHADOW_SNAP);     /* 2R / SNAP: |centre - snapped| <= R / SNAP, rad + R / SNAP <= R */
        q[0] = roundf((ctr[0] * r[0] + ctr[1] * r[1] + ctr[2] * r[2]) / step) * step;
        q[1] = roundf((ctr[0] * v[0] + ctr[1] * v[1] + ctr[2] * v[2]) / step) * step;
        q[2] = roundf((ctr[0] * f[0] + ctr[1] * f[1] + ctr[2] * f[2]) / step) * step;
        zhi = q[2] + R + 1.0f;
        zlo = fminf(lo, q[2] - R) - 1.0f;
        sx = 1.0f / R;
        sz = 1.0f / (zhi - zlo);
        m[0] = r[0] * sx; m[4] = r[1] * sx; m[8]  = r[2] * sx; m[12] = -q[0] * sx;
        m[1] = v[0] * sx; m[5] = v[1] * sx; m[9]  = v[2] * sx; m[13] = -q[1] * sx;
        m[2] = f[0] * sz; m[6] = f[1] * sz; m[10] = f[2] * sz; m[14] = -zlo * sz;
        m[3] = 0;         m[7] = 0;         m[11] = 0;         m[15] = 1;
        out[c].split = d1;
        out[c].texel = texel;
        out[c].bias = texel * sz;                       /* one texel, in this cascade's depth */
        out[c].depth = zhi - zlo;
    }
}

/* where in a cascade's texels a set of triangles can draw: the bounding
 * rectangle (x0, y0, x1, y1, ends exclusive, clamped to the map; empty when
 * x0 >= x1) of their corners, a texel wider each side.  A triangle covers
 * only texels inside its corners' bounds, so this holds every texel it can
 * write. */
static void brr_shc_rect(const float m[16], const float *v, size_t nv, int size, int r[4])
{
    float lo[2] = { 1e30f, 1e30f }, hi[2] = { -1e30f, -1e30f };
    size_t i;
    for (i = 0; i < nv; i++) {
        const float *p = &v[3 * i];
        float x = m[0] * p[0] + m[4] * p[1] + m[8] * p[2] + m[12];
        float y = m[1] * p[0] + m[5] * p[1] + m[9] * p[2] + m[13];
        float s = (x * 0.5f + 0.5f) * (float)size, t = (0.5f - y * 0.5f) * (float)size;
        lo[0] = fminf(lo[0], s); hi[0] = fmaxf(hi[0], s);
        lo[1] = fminf(lo[1], t); hi[1] = fmaxf(hi[1], t);
    }
    if (!nv || !(hi[0] >= 0.0f) || !(hi[1] >= 0.0f) || !(lo[0] <= (float)size) || !(lo[1] <= (float)size)) {
        r[0] = r[1] = r[2] = r[3] = 0;
        return;
    }
    r[0] = lo[0] < 1.0f ? 0 : (int)floorf(lo[0]) - 1;
    r[1] = lo[1] < 1.0f ? 0 : (int)floorf(lo[1]) - 1;
    r[2] = hi[0] > (float)(size - 2) ? size : (int)ceilf(hi[0]) + 1;
    r[3] = hi[1] > (float)(size - 2) ? size : (int)ceilf(hi[1]) + 1;
}

/* a cascade's static cache.  Two depth arrays: the cache holds the static
 * casters alone, drawn with the matrix m; the live map, the one the lighting
 * reads, is the cache with this frame's moving casters drawn over it.  The
 * live map differs from the cache only inside dirty (the moving casters'
 * rectangle last frame).  Each frame, per cascade: when the matrix or the
 * track changed the static casters are drawn again into the cache (redraw)
 * and all of the live map is restored from it; otherwise only dirty is.  The
 * moving casters are then drawn over the live map, and their rectangle is the
 * next frame's dirty. */
typedef struct BrrShCache { float m[16]; unsigned gen; int valid; int dirty[4]; } BrrShCache;

/* this frame's plan for a cascade: returns 1 when the static casters must be
 * drawn again into the cache; restore[] the live texels to copy back from the
 * cache before the moving casters (whose rectangle is dyn) are drawn */
static int brr_shc_plan(BrrShCache *c, const float m[16], unsigned gen, const int dyn[4], int size, int restore[4])
{
    int redraw = !c->valid || c->gen != gen || memcmp(c->m, m, sizeof c->m) != 0;
    if (redraw) {
        memcpy(c->m, m, sizeof c->m);
        c->gen = gen;
        c->valid = 1;
        restore[0] = restore[1] = 0;
        restore[2] = restore[3] = size;
    } else {
        memcpy(restore, c->dirty, sizeof c->dirty);
    }
    memcpy(c->dirty, dyn, sizeof c->dirty);
    return redraw;
}

/* the moving casters are composited in a scratch tile, not over the whole
 * live map: a depth pass costs its whole attachment's store whatever it
 * draws (lighttest measures it), a blit only the texels it moves.  The tile
 * is the smallest square power of two from 64 that holds the rectangle w x
 * h; brr_shc_crop is the cascade's matrix m with the tile's texel (0, 0) at
 * the map's texel (x0, y0): the same texel grid, b texels across. */
static int brr_shc_tile(int w, int h, int size)
{
    int b = 64;
    while (b < size && (b < w || b < h))
        b *= 2;
    return b;
}
static void brr_shc_crop(const float m[16], int size, int x0, int y0, int b, float out[16])
{
    float a = (float)size / (float)b, bx = (float)(size - 2 * x0) / (float)b - 1.0f, by = 1.0f - (float)(size - 2 * y0) / (float)b;
    int i;
    memcpy(out, m, 16 * sizeof(float));
    for (i = 0; i < 4; i++) {
        out[4 * i + 0] = a * m[4 * i + 0] + bx * m[4 * i + 3];
        out[4 * i + 1] = a * m[4 * i + 1] + by * m[4 * i + 3];
    }
}

/* world to the lit target's pixels, h = S x [p 1] (S column-major), the pixel
 * h.xy / h.w (continuous: a pixel's centre at +0.5): the game's view x
 * projection (row vectors: clip = [p 1] vp), its viewport (x = cx / cw * sx +
 * ox), Glide's rows (flip: the game's rows run up from the lower left of a
 * screen this high, else 0), the frame's screen map entry (ndc = g * xft.x +
 * xft.y, g.y * xft.z + xft.w) and the target's size.  The inverse of the
 * sky's camray (brr_metal_sky.h). */
static void brr_screen_matrix(const float vp[16], float sx, float ox, float sy, float oy, float flip, const float xft[4],
                              float tw, float th, float s[16])
{
    float a = flip > 0.0f ? -sy : sy, b = flip > 0.0f ? flip - oy : oy;
    float A = 0.5f * tw * xft[0] * sx, B = 0.5f * tw * (1.0f + xft[0] * ox + xft[1]);
    float C = -0.5f * th * xft[2] * a, D = 0.5f * th * (1.0f - xft[2] * b - xft[3]);
    int i;
    for (i = 0; i < 4; i++) {
        s[4 * i + 0] = A * vp[4 * i + 0] + B * vp[4 * i + 3];
        s[4 * i + 1] = C * vp[4 * i + 1] + D * vp[4 * i + 3];
        s[4 * i + 2] = 0.0f;
        s[4 * i + 3] = vp[4 * i + 3];
    }
}

#endif
