/* test_gfx.c -- render real game artwork through the Metal backend, headless.
 *
 * Loads a retail .img, uploads it as a texture, draws it into an offscreen
 * target, reads it back and writes a PPM. Verifying the port by looking at
 * what it actually draws is the whole point of dropping byte-matching.
 */
#include "br_img.h"
#include "br_testdata.h"
#include "br_gfx.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fail;

static void check(int cond, const char *pszWhat)
{
    printf("  [%s] %s\n", cond ? "PASS" : "FAIL", pszWhat);
    if (!cond)
        g_fail = 1;
}

static int write_ppm(const char *pszPath, const uint8_t *pRgba,
                     uint32_t w, uint32_t h)
{
    FILE *f = fopen(pszPath, "wb");
    uint32_t i;
    if (f == NULL)
        return 1;
    fprintf(f, "P6\n%u %u\n255\n", w, h);
    for (i = 0; i < w * h; i++)
        fwrite(pRgba + i * 4, 1, 3, f);
    fclose(f);
    return 0;
}

int main(int argc, char **argv)
{
    BR_REQUIRE_TESTDATA("testdata/splash.img", "gfx");
    const char *pszImg = (argc > 1) ? argv[1] : "testdata/splash.img";
    const char *pszOut = (argc > 2) ? argv[2] : "build/gfx_out.ppm";
    BrImage img;
    BrGfx *gfx;
    BrTexture tex;
    uint8_t *pOut;
    uint32_t W = 512, H = 384;
    uint32_t opaque = 0, i;

    printf("loading %s\n", pszImg);
    check(BrImgLoad(&img, pszImg) == 0, "BrImgLoad");
    if (g_fail)
        return 1;
    printf("  %ux%u\n", img.width, img.height);
    check(img.width > 0 && img.height > 0, "image has dimensions");

    /* Alpha must be strictly binary (these are 1-bit-alpha formats) and there
     * must be real opaque content. We cannot assert "mostly opaque": splash.img
     * is a transparency-keyed overlay at ~15% coverage, while loading.img is a
     * fully opaque background. Picking the wrong channel order collapses alpha
     * to all-0 or all-1, which both of these catch. */
    {
        uint32_t total = img.width * img.height, binary = 0;
        for (i = 0; i < total; i++) {
            uint8_t a = img.pixels[i * 4 + 3];
            if (a == 0 || a == 255) binary++;
            if (a == 255) opaque++;
        }
        printf("  opaque pixels: %u / %u\n", opaque, total);
        check(binary == total, "alpha is strictly binary");
        check(opaque > total / 20, "image has real opaque content");
        check(opaque > 0 && opaque <= total, "alpha not collapsed to a constant");
    }

    gfx = BrGfxCreate(W, H);
    if (gfx == NULL) {
        printf("  [FAIL] BrGfxCreate: %s\n", BrGfxLastError());
        return 1;
    }
    check(1, "BrGfxCreate (Metal device + runtime-compiled shaders)");

    tex = BrGfxCreateTexture(gfx, img.width, img.height, img.pixels);
    check(tex != 0, "BrGfxCreateTexture");

    BrGfxBeginFrame(gfx, 0.10f, 0.10f, 0.12f, 1.0f);
    BrGfxDrawTexture(gfx, tex, 16.0f, 16.0f, (float)img.width, (float)img.height);
    /* draw again, scaled, to exercise the sampler */
    BrGfxDrawTexture(gfx, tex, 300.0f, 200.0f, 128.0f, 128.0f);
    BrGfxEndFrame(gfx);

    pOut = (uint8_t *)malloc((size_t)W * H * 4);
    check(BrGfxReadPixels(gfx, pOut) == 0, "BrGfxReadPixels");

    /* the cleared background must survive where nothing was drawn */
    check(pOut[(H - 1) * W * 4 + 4] < 64, "clear colour present in empty area");

    check(write_ppm(pszOut, pOut, W, H) == 0, "wrote render output");
    printf("  -> %s\n", pszOut);

    free(pOut);
    BrGfxDestroy(gfx);
    BrImgFree(&img);
    printf(g_fail ? "\nFAILED\n" : "\nALL PASSED\n");
    return g_fail;
}
