/* brr_skymask.h: the painted backdrop's sky, found by its structure (port
 * code, plain C; brr_metal.m makes one per backdrop texture, the lighttest
 * suite runs the same code on synthetic and real panoramas).
 *
 * A backdrop texture is a horizontal panorama strip: painted sky at the top,
 * the scenery's silhouette below.  Going down each column from the top edge,
 * the sky is the region in which the colour changes smoothly (painted
 * gradients, soft clouds, haze, a sun's glow: no single step between
 * neighbouring rows is large); the scenery starts at the first strong edge
 * that persists (the rows below stay apart from the sky just above it).  A
 * transparent run at the top is sky too, and the first opaque texel under it
 * starts the scenery.  A column with no such edge is all sky.  The skyline
 * is then median filtered across 5 columns (a lone tree or a cloud's crisp
 * rim does not notch it).  The mask is 255 above the skyline, 0 at and below
 * it; the shader samples it bilinearly for an anti-aliased silhouette. */
#ifndef BRR_SKYMASK_H
#define BRR_SKYMASK_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define BRR_SKY_EDGE 0.07f      /* one row's step, max over the channels, that may start scenery */
#define BRR_SKY_KEEP 0.05f      /* how far the next 3 rows must stay from the last 3 sky rows */
#define BRR_SKY_SOFT 0.03f      /* a soft edge (one 5-bit step a row: haze going down into the sea) ... */
#define BRR_SKY_SOFTKEEP 0.06f  /* ... starts scenery when the next 3 rows stay this far, darker, from the last 3 */
#define BRR_SKY_FLAT 0.03f      /* a texture with no edge whose row means vary less than this is a flat panel (the sea) */
#define BRR_SKY_LEVEL 0.035f    /* the most a channel may vary down the rows under a soft edge (a 5-bit step, a glint) */
#define BRR_SKY_RUN 10          /* how many rows under a soft edge must stay level */
#define BRR_SKY_MED  5          /* the skyline's median filter, columns */

static float brr_sky_px(const float *f, int w, int x, int y, int c) { return f[(y * w + x) * 3 + c]; }

/* the skyline per column (row where the scenery starts, h for none) into sky[w];
 * the mask into m[w*h] when m is not NULL.  Returns the fraction of texels that are sky. */
static float brr_skymask_half(const uint8_t *rgba, int w, int h, int *sky, uint8_t *m)
{
    float *f = (float *)malloc((size_t)w * (size_t)h * 3 * sizeof(float));
    int *raw = (int *)malloc((size_t)w * sizeof(int));
    int x, y, c, i, nsky = 0;
    if (!f || !raw) {
        free(f);
        free(raw);
        for (x = 0; x < w; x++)
            sky[x] = 0;
        if (m)
            memset(m, 0, (size_t)w * (size_t)h);
        return 0.0f;
    }
    /* a [1 2 1] blur along the row: steadies dithered paint, moves no horizontal edge */
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++)
            for (c = 0; c < 3; c++) {
                int xl = x > 0 ? x - 1 : 0, xr = x < w - 1 ? x + 1 : w - 1;
                const uint8_t *r = rgba + (size_t)y * w * 4;
                f[(y * w + x) * 3 + c] = (r[xl * 4 + c] + 2.0f * r[x * 4 + c] + r[xr * 4 + c]) / (4.0f * 255.0f);
            }
    for (x = 0; x < w; x++) {
        int s = h;
        for (y = 1; y < h && s == h; y++) {
            int a0 = rgba[((size_t)(y - 1) * w + x) * 4 + 3], a1 = rgba[((size_t)y * w + x) * 4 + 3];
            float step = 0.0f, keep = 0.0f;
            int dark = 1;           /* every channel of the next 3 rows below the last 3 sky rows */
            if (a1 < 128)
                continue;
            if (a0 < 128) {         /* the first opaque texel under the transparent sky */
                s = y;
                break;
            }
            for (c = 0; c < 3; c++) {
                float d = brr_sky_px(f, w, x, y, c) - brr_sky_px(f, w, x, y - 1, c), above = 0.0f, below = 0.0f;
                int n0 = 0, n1 = 0, k;
                if (d < 0.0f)
                    d = -d;
                if (d > step)
                    step = d;
                for (k = 1; k <= 3 && y - k >= 0; k++, n0++)
                    above += brr_sky_px(f, w, x, y - k, c);
                for (k = 0; k < 3 && y + k < h; k++, n1++)
                    below += brr_sky_px(f, w, x, y + k, c);
                d = below / n1 - above / n0;
                if (d >= 0.0f)
                    dark = 0;
                if (d < 0.0f)
                    d = -d;
                if (d > keep)
                    keep = d;
            }
            if (step >= BRR_SKY_EDGE && keep >= BRR_SKY_KEEP)
                s = y;
            else if (step >= BRR_SKY_SOFT && keep >= BRR_SKY_SOFTKEEP && dark && y + BRR_SKY_RUN <= h) {
                int flat = 1, k;    /* and the BRR_SKY_RUN rows below it are level: a sea, not a cloud's shaded base */
                for (c = 0; c < 3 && flat; c++) {
                    float lo = 1e9f, hi = -1e9f;
                    for (k = 1; k < BRR_SKY_RUN; k++) {
                        float v = brr_sky_px(f, w, x, y + k, c);
                        lo = v < lo ? v : lo;
                        hi = v > hi ? v : hi;
                    }
                    flat = hi - lo < BRR_SKY_LEVEL;
                }
                if (flat)
                    s = y;
            }
        }
        raw[x] = s;
    }
    {   /* no edge anywhere and no gradient down it: a flat panel, the sea under the horizon */
        float lo = 1e9f, hi = -1e9f;
        int edge = 0;
        for (x = 0; x < w; x++)
            edge |= raw[x] < h;
        for (y = 0; y < h && !edge; y++) {
            float r = 0.0f;
            for (x = 0; x < w; x++)
                r += brr_sky_px(f, w, x, y, 0) + brr_sky_px(f, w, x, y, 1) + brr_sky_px(f, w, x, y, 2);
            r /= 3.0f * w;
            lo = r < lo ? r : lo;
            hi = r > hi ? r : hi;
        }
        if (!edge && hi - lo < BRR_SKY_FLAT)
            for (x = 0; x < w; x++)
                raw[x] = 0;
    }
    for (x = 0; x < w; x++) {      /* the median across BRR_SKY_MED columns, clamped at the ends */
        int v[BRR_SKY_MED], n = 0, a, b;
        for (i = -BRR_SKY_MED / 2; i <= BRR_SKY_MED / 2; i++) {
            int xx = x + i < 0 ? 0 : x + i >= w ? w - 1 : x + i;
            v[n++] = raw[xx];
        }
        for (a = 1; a < n; a++)
            for (b = a; b > 0 && v[b - 1] > v[b]; b--) {
                int t = v[b];
                v[b] = v[b - 1];
                v[b - 1] = t;
            }
        sky[x] = v[n / 2];
        nsky += sky[x];
    }
    if (m)
        for (y = 0; y < h; y++)
            for (x = 0; x < w; x++)
                m[(size_t)y * w + x] = (uint8_t)(y < sky[x] ? 255 : 0);
    free(f);
    free(raw);
    return (float)nsky / ((float)w * (float)h);
}

/* The game's backdrop textures are stored as the picture over its mirror
 * image (each row y equals row h-1-y; the lower half is the picture upright:
 * its first row is the sky's zenith fill colour, the hills sit on the sea
 * there).  Such a texture is analysed by its lower half, top down, and the
 * mask mirrored into the upper half; sky[x] is the lower half's skyline. */
static int brr_sky_mirrored(const uint8_t *rgba, int w, int h)
{
    int x, y, c;
    if (h < 8 || h & 1)
        return 0;
    for (y = 0; y < h / 2; y++)
        for (x = 0; x < w; x++)
            for (c = 0; c < 4; c++)
                if (rgba[((size_t)y * w + x) * 4 + c] != rgba[((size_t)(h - 1 - y) * w + x) * 4 + c])
                    return 0;
    return 1;
}

static float brr_skymask(const uint8_t *rgba, int w, int h, int *sky, uint8_t *m)
{
    int x, y, h2 = h / 2;
    float fr;
    if (!brr_sky_mirrored(rgba, w, h))
        return brr_skymask_half(rgba, w, h, sky, m);
    fr = brr_skymask_half(rgba + (size_t)h2 * w * 4, w, h2, sky, m ? m + (size_t)h2 * w : NULL);
    for (x = 0; x < w; x++)
        sky[x] += h2;
    if (m)
        for (y = 0; y < h2; y++)
            memcpy(m + (size_t)y * w, m + (size_t)(h - 1 - y) * w, (size_t)w);
    return fr;
}

#endif
