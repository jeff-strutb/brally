/* brview.c -- minimal asset viewer: real game artwork in a Metal window. */
#include "br_img.h"
#include "br_gfx.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    BrGfx *gfx;
    BrImage img;
    BrTexture tex[8];
    int cTex = 0, i, frames = 0;
    int maxFrames = (argc > 1 && argv[1][0] == '-') ? atoi(argv[1] + 1) : 0;

    gfx = BrGfxCreate(640, 480);
    if (!gfx) { printf("gfx init failed: %s\n", BrGfxLastError()); return 1; }
    if (BrGfxOpenWindow(gfx, "Boss Rally - asset viewer") != 0) return 1;

    const char *paths[] = { "testdata/loading.img", "testdata/splash.img" };
    for (i = 0; i < 2; i++) {
        if (BrImgLoad(&img, paths[i]) == 0) {
            tex[cTex++] = BrGfxCreateTexture(gfx, img.width, img.height, img.pixels);
            printf("loaded %s (%ux%u)\n", paths[i], img.width, img.height);
            BrImgFree(&img);
        }
    }

    while (BrGfxPumpEvents(gfx)) {
        BrGfxBeginFrame(gfx, 0.06f, 0.06f, 0.09f, 1.0f);
        if (cTex > 0) BrGfxDrawTexture(gfx, tex[0],  16.0f,  16.0f, 256.0f, 200.0f);
        if (cTex > 1) BrGfxDrawTexture(gfx, tex[1], 300.0f,  16.0f, 320.0f, 320.0f);
        if (cTex > 0) BrGfxDrawTexture(gfx, tex[0],  16.0f, 240.0f, 128.0f, 100.0f);
        BrGfxEndFrame(gfx);
        BrGfxPresent(gfx);
        if (maxFrames && ++frames >= maxFrames) break;
        usleep(16000);
    }
    BrGfxDestroy(gfx);
    return 0;
}
