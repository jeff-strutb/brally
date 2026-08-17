/* br_imgblit.c -- see br_imgblit.h.
 *
 * RESPONSIBILITY: turn images into pixels.  Glide 0x1006C990, the full-screen
 * `.img` blitter -- the boot splash and the loading screen.
 */
#include "br_imgblit.h"

#include <stdlib.h>     /* exit -- the original's 0x1006CA30 */
#include <string.h>

#include "slice1_01.h"  /* BrAdler32 0x10001000, BrChkFRead Glide 0x100034C0 */
#include "slice6_78.h"  /* BrChkFReadOpen Glide 0x10003320, BrChkFClose      */

/* 0x1006CA3E .. 0x1006CA5D, in argument order (the pushes reversed). */
const int32_t g_aBrImgBlitTexArgs[BR_IMGBLIT_TEXARGS] = {
    0,                          /*  1                                      */
    BR_IMGBLIT_TEX_EVENODD,     /*  2  evenOdd, 3 == both mipmap levels    */
    0x80,                       /*  3                                      */
    0x20,                       /*  4                                      */
    BR_IMGBLIT_TEX_FORMAT,      /*  5  GrTexInfo.format   -> +0x0C         */
    2,                          /*  6                                      */
    BR_IMGBLIT_TEX_SMALLLOD,    /*  7  GrTexInfo.smallLod -> +0x00         */
    BR_IMGBLIT_TEX_LARGELOD,    /*  8  GrTexInfo.largeLod -> +0x04         */
    BR_IMGBLIT_TEX_ASPECT,      /*  9  GrTexInfo.aspect   -> +0x08         */
    0, 0, 0, 0, 0, 0            /* 10..15                                  */
};

static int32_t s_aSkipped[BR_IMGBLIT_NSTEPS];

/* `fHooked` is the caller's `pfn != NULL`, passed as an int for the reason
 * br_bootinit.c gives: C99 does not define converting a function pointer to
 * an object pointer. */
static int hooked(BrImgBlitStep step, int fHooked)
{
    if (fHooked) {
        return 1;
    }
    ++s_aSkipped[step];
    return 0;
}

/* ==========================================================================
 * The load half -- 0x1006C990 .. 0x1006CA36
 * ========================================================================== */

int BrImgBlitLoad(const char *pszName, uint32_t uAdler,
                  void *pvBuf, size_t cbBuf,
                  BrImgBlitHdr *pHdr, uint32_t *puAdler)
{
    FILE        **ppFile;
    int32_t       cx = 0, cy = 0;
    int32_t       cbPixels;
    unsigned char *pb = (unsigned char *)pvBuf;

    memset(pHdr, 0, sizeof(*pHdr));

    /* 0x1006C99D.  CHK_FReadOpen: Glide 0x10003320 == D3D 0x10002FE0 ==
     * slice6_78.c's BrChkFReadOpen.  NOT BrChkFileExists, which is Glide
     * 0x10003680 -- see the isported note in the report. */
    ppFile = BrChkFReadOpen(pszName);
    if (ppFile == NULL) {
        return BR_IMGBLIT_NOFILE;
    }

    /* 0x1006C9B1 and 0x1006C9C3.  Two four-byte reads into two DIFFERENT
     * stack slots ([esp+0x18] then [esp+0x1C]); the second is picked up at
     * 0x1006C9C8 through [esp+0x2C] while esp is still 0x10 low from the
     * call's arguments, which is the same slot. */
    BrChkFRead(&cx, 4, 1, ppFile);
    BrChkFRead(&cy, 4, 1, ppFile);

    /* 0x1006C9CF / 0x1006C9DE.  imul then shl 1: BYTES, not dwords. */
    cbPixels = cx * cy * 2;
    pHdr->cx = cx;
    pHdr->cy = cy;
    pHdr->cbPixels = cbPixels;

    if (cx <= 0 || cy <= 0 || (size_t)cbPixels > cbBuf) {
        /* DEVIATION, stated in the header: the original has no check here. */
        BrChkFClose(ppFile);
        return BR_IMGBLIT_TOOBIG;
    }

    /* 0x1006C9E0.  The format word is read into the START of the pixel
     * buffer and is then overwritten by the payload read below.  Modelled
     * because it is exactly the aliasing shape CONVENTIONS.md warns about --
     * a fill and a later store landing on the same bytes -- and because the
     * value is observable in the buffer in between.  Nothing reads it. */
    BrChkFRead(pb, 4, 1, ppFile);
    pHdr->fmt = (uint32_t)pb[0]
              | ((uint32_t)pb[1] << 8)
              | ((uint32_t)pb[2] << 16)
              | ((uint32_t)pb[3] << 24);

    /* 0x1006C9F1.  size = cbPixels, count = 1 -- one big read. */
    BrChkFRead(pb, (size_t)cbPixels, 1, ppFile);

    /* 0x1006C9FA. */
    BrChkFClose(ppFile);

    /* 0x1006CA09.  The second argument, reloaded from [esp+0x124] while esp
     * is 4 low from `push edi` -- i.e. R+8, the second cdecl argument, not a
     * local.  Zero skips the check entirely. */
    if (uAdler != 0u) {
        /* 0x1006CA13: BrAdler32(0, NULL, 0) is how the original asks for the
         * seed; slice1_01.h records that a NULL buffer returns 1 and ignores
         * the first argument.  0x1006CA22 then runs it over the payload. */
        unsigned long seed = BrAdler32(0, NULL, 0);
        unsigned long sum  = BrAdler32(seed, pb, (unsigned int)cbPixels);

        if (puAdler != NULL) {
            *puAdler = (uint32_t)sum;
        }
        if ((uint32_t)sum != uAdler) {
            return BR_IMGBLIT_BADSUM;
        }
        return BR_IMGBLIT_OK;
    }

    if (puAdler != NULL) {
        *puAdler = 0u;
    }
    return BR_IMGBLIT_OK;
}

/* ==========================================================================
 * The placement half -- 0x1006CAA4 .. 0x1006CB34
 * ========================================================================== */

void BrImgBlitPlace(int32_t cx, int32_t cy, int32_t screenW, int32_t screenH,
                    float *px0, float *py0, float *px1, float *py1)
{
    float x0, y0;

    /* 0x1006CAAA and 0x1006CADC.  `lea eax,[reg-0x100] ; cdq ; sub eax,edx ;
     * sar eax,1` is a SIGNED divide by two truncating toward zero, which is
     * not `>> 1` once the numerator goes negative.  C99's `/` truncates
     * toward zero, so it is the same operator. */
    x0 = (float)((screenW - BR_IMGBLIT_CENTRE_SPAN) / 2);
    y0 = (float)((screenH - BR_IMGBLIT_CENTRE_SPAN) / 2);

    /* 0x1006CAC1 / 0x1006CAF3.  `fcomp [0x10077C10]` against 0.0f, then
     * `test ah,1 ; je` -- the store happens when C0 is set, and C0 is also
     * set for an unordered compare, so NaN takes the store.  Written as the
     * negated form per CONVENTIONS.md; never `x0 < 0.0f`. */
    if (!(x0 >= 0.0f)) {
        x0 = 0.0f;
    }
    if (!(y0 >= 0.0f)) {
        y0 = 0.0f;
    }

    /* 0x1006CB1A .. 0x1006CB2E.  The far corner uses the FILE's extent, not
     * the 256 the near corner centred on, and subtracts the 1.0f at
     * 0x10077C14. */
    *px0 = x0;
    *py0 = y0;
    *px1 = (float)cx + x0 - 1.0f;
    *py1 = (float)cy + y0 - 1.0f;
}

/* ==========================================================================
 * The quad -- 0x1006CB08 .. 0x1006CCC5
 * ========================================================================== */

/* The six constant fields every vertex gets.  0x437F0000 is 255.0f and
 * 0x3F800000 is 1.0f, read out of the instruction stream rather than
 * assumed. */
static void vtx(BrDlVtx *pV, float x, float y, float s, float t)
{
    pV->x = x;
    pV->y = y;
    pV->r = 255.0f;
    pV->g = 255.0f;
    pV->b = 255.0f;
    pV->a = 255.0f;
    pV->oow = 1.0f;
    pV->tmu0[0] = s;
    pV->tmu0[1] = t;
    /* z, ooz, tmu0[2], tmu1[] are NOT written -- the original leaves them as
     * whatever was on its stack and hands that to grDrawTriangle. */
}

void BrImgBlitQuad(BrDlVtx av[4], int32_t cx, int32_t cy,
                   float x0, float y0, float x1, float y1)
{
    /* 0x1006CB18 / 0x1006CB19: `dec eax` / `dec edx` -- the texture extents
     * are one less than the file's, and they are integers converted with
     * `fild`, so a 0-wide image would give -1 rather than wrapping. */
    const float s1 = (float)(cx - 1);
    const float t1 = (float)(cy - 1);

    vtx(&av[0], x1, y1, s1, 0.0f);   /* frame slot [esp+0x28] */
    vtx(&av[1], x0, y0, 0.0f, t1);   /* frame slot [esp+0x64] */
    vtx(&av[2], x0, y1, 0.0f, 0.0f); /* frame slot [esp+0xA0] */
    vtx(&av[3], x1, y0, s1, t1);     /* frame slot [esp+0xDC] */
}

/* ==========================================================================
 * The whole function -- Glide 0x1006C990
 * ========================================================================== */

/* One clip / clear / two triangles / present pass.  0x1006CCC5..0x1006CD06
 * and 0x1006CD0C..0x1006CD5E are the same work written twice; the second
 * block reaches the same four slots through a different set of displacements
 * because its pushes fall between different `lea`s, and that agreement is
 * what confirms the slot assignment. */
static void pass(const BrImgBlitOps *pOps, const BrDlVtx av[4],
                 int32_t screenW, int32_t screenH)
{
    if (hooked(BR_IMGBLIT_CLIPWINDOW, pOps->pfnClipWindow != NULL)) {
        pOps->pfnClipWindow(pOps->pUser, 0, 0, screenW, screenH);
    }
    if (hooked(BR_IMGBLIT_BUFFERCLEAR, pOps->pfnBufferClear != NULL)) {
        pOps->pfnBufferClear(pOps->pUser, 0u, 0u, BR_IMGBLIT_CLEAR_DEPTH);
    }
    /* 0x1006CCEA: (v2, v0, v1).  0x1006CD01: (v0, v3, v1). */
    if (hooked(BR_IMGBLIT_DRAWTRIANGLE, pOps->pfnDrawTriangle != NULL)) {
        pOps->pfnDrawTriangle(pOps->pUser, &av[2], &av[0], &av[1]);
    }
    if (hooked(BR_IMGBLIT_DRAWTRIANGLE, pOps->pfnDrawTriangle != NULL)) {
        pOps->pfnDrawTriangle(pOps->pUser, &av[0], &av[3], &av[1]);
    }
    /* 0x1006CD06 / 0x1006CD5E: `call [0x106B7AB8]`. */
    if (hooked(BR_IMGBLIT_PRESENT, pOps->pfnPresent != NULL)) {
        pOps->pfnPresent(pOps->pUser);
    }
}

void BrImgBlitFullScreen(const char *pszName, uint32_t uAdler,
                         void *pvBuf, size_t cbBuf,
                         int32_t screenW, int32_t screenH,
                         const BrImgBlitOps *pOps)
{
    BrImgBlitHdr hdr;
    BrDlVtx      av[4];
    float        x0, y0, x1, y1;
    int32_t      hTex = 0;
    int          rc;

    if (pOps == NULL) {
        return;
    }

    rc = BrImgBlitLoad(pszName, uAdler, pvBuf, cbBuf, &hdr, NULL);

    /* 0x1006CA2E.  The original's only failure exit, and it is not
     * recoverable.  slice1_01.h documents the same shape for the CHK_*
     * wrappers. */
    if (rc == BR_IMGBLIT_BADSUM) {
        exit(1);
    }
    if (rc != BR_IMGBLIT_OK) {
        /* BrChkFReadOpen failing is not a path the original has -- it exits
         * inside the helper -- and an over-long payload is this port's own
         * refusal.  Neither invents a picture: nothing is drawn. */
        return;
    }

    /* 0x1006CA39.  Wipes all 1024 texture records and both TMU cursors. */
    if (hooked(BR_IMGBLIT_TEXRESET, pOps->pfnTexReset != NULL)) {
        pOps->pfnTexReset(pOps->pUser);
    }

    /* 0x1006CA5F.  Fifteen cdecl arguments, `add esp,0x3c`. */
    if (hooked(BR_IMGBLIT_TEXALLOC, pOps->pfnTexAlloc != NULL)) {
        hTex = pOps->pfnTexAlloc(pOps->pUser, g_aBrImgBlitTexArgs);
    }

    /* 0x1006CA71: (handle, buffer, 0). */
    if (hooked(BR_IMGBLIT_TEXDOWNLOAD, pOps->pfnTexDownload != NULL)) {
        pOps->pfnTexDownload(pOps->pUser, hTex, pvBuf, 0);
    }
    /* 0x1006CA7A. */
    if (hooked(BR_IMGBLIT_TEXSOURCE, pOps->pfnTexSource != NULL)) {
        pOps->pfnTexSource(pOps->pUser, hTex);
    }

    /* 0x1006CA90: grTexCombine(GR_TMU0, LOCAL, ZERO, LOCAL, ZERO, 0, 0). */
    if (hooked(BR_IMGBLIT_TEXCOMBINE, pOps->pfnTexCombine != NULL)) {
        pOps->pfnTexCombine(pOps->pUser, 0, 1, 0, 1, 0, 0, 0);
    }
    /* 0x1006CA9F: the pixel is the texel. */
    if (hooked(BR_IMGBLIT_COLORCOMBINE, pOps->pfnColorCombine != NULL)) {
        pOps->pfnColorCombine(pOps->pUser,
                              BR_IMGBLIT_CC_FUNCTION, BR_IMGBLIT_CC_FACTOR,
                              BR_IMGBLIT_CC_LOCAL, BR_IMGBLIT_CC_OTHER,
                              BR_IMGBLIT_CC_INVERT);
    }

    BrImgBlitPlace(hdr.cx, hdr.cy, screenW, screenH, &x0, &y0, &x1, &y1);

    /* The original's four vertices are uninitialised stack.  Nothing here
     * depends on their contents, and zeroing would be a claim the original
     * does not make -- but a host `grDrawTriangle` shim reading z would
     * otherwise see an indeterminate value, which C99 does not permit us to
     * pass around.  Zero, and say so. */
    memset(av, 0, sizeof av);
    BrImgBlitQuad(av, hdr.cx, hdr.cy, x0, y0, x1, y1);

    /* 0x1006CCC5 and 0x1006CD0C -- once per buffer. */
    pass(pOps, av, screenW, screenH);
    pass(pOps, av, screenW, screenH);

    /* 0x1006CD64.  The manager is wiped again on the way out. */
    if (hooked(BR_IMGBLIT_TEXRESET, pOps->pfnTexReset != NULL)) {
        pOps->pfnTexReset(pOps->pUser);
    }
}

int32_t BrImgBlitSkipped(BrImgBlitStep step)
{
    if (step < 0 || step >= BR_IMGBLIT_NSTEPS) {
        return 0;
    }
    return s_aSkipped[step];
}

void BrImgBlitResetForTest(void)
{
    int i;
    for (i = 0; i < BR_IMGBLIT_NSTEPS; i++) {
        s_aSkipped[i] = 0;
    }
}
