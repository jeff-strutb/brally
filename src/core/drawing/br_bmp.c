/* br_bmp.c -- the platform seam between a BMP file and the game's decoders.
 *
 * See br_bmp.h for the split and br_surf.h for the address chain.  In one
 * line: the file parsing below is a recreation of USER32!LoadImageA plus
 * GDI32!GetObjectA, because that is what both originals call and there is no
 * BMP parser anywhere in BRGlide.dll to decompile; everything after the
 * `BITMAP` is decompiled and lives in br_surf.c.
 *
 * BMP FORMAT NOTES, all read from the shipped files rather than assumed:
 *   - BITMAPINFOHEADER (40 bytes), biCompression 0, 24bpp, bfOffBits 54.
 *     All 172 files in IMAGES\ are identical in all four respects.
 *   - Rows are BOTTOM-UP (biHeight positive).  This code does NOT flip them:
 *     it hands GDI's own convention to BrSurfBlt24, which starts at the last
 *     stored row and walks backwards, and that is where the flip happens --
 *     in the game's code, where the original put it.
 *   - Each row is padded to a 4-byte boundary, which is also GDI's DIB
 *     stride.  The padding is not optional and a loader that ignores it
 *     shears every image whose width is not a multiple of 4.  127*3 == 381,
 *     so but-sav.bmp shears immediately if it is got wrong -- which makes it
 *     a good canary, and port/tests/test_br_bmp.c uses it as one.
 *   - Pixels are stored B,G,R.
 */
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include "br_bmp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t rd32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static uint16_t rd16(const uint8_t *p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

/* ======================================================================
 * RECREATION -- LoadImageA(LR_LOADFROMFILE|LR_CREATEDIBSECTION) + GetObjectA
 *
 * No original address: the original is in USER32.DLL and GDI32.DLL.
 * ====================================================================== */
int BrBmpGdiLoad(BrGdiBitmapMem *pOut, const char *pszPath)
{
    FILE    *fh;
    uint8_t  hdr[54];
    uint32_t off, dib, cb;
    int32_t  w, h;
    uint16_t planes, bpp;
    size_t   stride, need;

    if (!pOut || !pszPath) return -1;
    memset(pOut, 0, sizeof(*pOut));

    fh = fopen(pszPath, "rb");
    if (!fh) return -1;
    if (fread(hdr, 1, sizeof hdr, fh) != sizeof hdr) { fclose(fh); return -1; }
    if (hdr[0] != 'B' || hdr[1] != 'M')              { fclose(fh); return -1; }

    off    = rd32(hdr + 10);
    dib    = rd32(hdr + 14);
    w      = (int32_t)rd32(hdr + 18);
    h      = (int32_t)rd32(hdr + 22);
    planes = rd16(hdr + 26);
    bpp    = rd16(hdr + 28);

    /* BITMAPINFOHEADER or later, and BI_RGB.  The bpp is NOT judged here --
     * see the header: refusing it is the game's job, and it does it twice. */
    if (dib < 40 || rd32(hdr + 30) != 0)            { fclose(fh); return -1; }
    if (bpp == 0 || bpp > 32)                       { fclose(fh); return -1; }
    if (w <= 0 || w > 4096 || h == 0 || h < -4096 || h > 4096) {
        fclose(fh); return -1;
    }

    /* GDI's DIB stride: the row rounded up to a DWORD.  For 24bpp this is the
     * same as the BMP file's own row padding, which is why the file image can
     * be handed straight over as if it were a DIB section's bits. */
    stride = (((size_t)w * bpp + 31u) / 32u) * 4u;
    need   = stride * (size_t)(h < 0 ? -h : h);

    if (fseek(fh, 0, SEEK_END) != 0)                { fclose(fh); return -1; }
    cb = (uint32_t)ftell(fh);
    if (cb < off || (size_t)(cb - off) < need)      { fclose(fh); return -1; }

    pOut->pAlloc = (uint8_t *)malloc(need);
    if (!pOut->pAlloc) { fclose(fh); return -1; }
    if (fseek(fh, (long)off, SEEK_SET) != 0 ||
        fread(pOut->pAlloc, 1, need, fh) != need) {
        free(pOut->pAlloc); pOut->pAlloc = NULL; fclose(fh); return -1;
    }
    fclose(fh);

    /* GetObjectA's view.  bmHeight is always positive; a top-down file simply
     * has its first stored row at the top, and nothing here records which --
     * which is exactly GDI's behaviour, and exactly why a top-down BMP would
     * come out flipped.  Nothing on the disc is top-down. */
    pOut->bm.type         = 0;
    pOut->bm.cx           = w;
    pOut->bm.cy           = (h < 0 ? -h : h);
    pOut->bm.cbWidthBytes = (int32_t)stride;
    pOut->bm.cPlanes      = (planes ? planes : 1);
    pOut->bm.cBitsPixel   = bpp;
    pOut->bm.pBits        = pOut->pAlloc;
    return 0;
}

void BrBmpGdiFree(BrGdiBitmapMem *pMem)
{
    if (!pMem) return;
    free(pMem->pAlloc);
    pMem->pAlloc = NULL;
    pMem->bm.pBits = NULL;
}

/* ======================================================================
 * 0x10001290 -- the UI sprite loader
 *
 * The original's shape, kept:
 *
 *     hbm = LoadImageA(GetModuleHandleA(NULL), name, IMAGE_BITMAP, cx, cy,
 *                      LR_CREATEDIBSECTION);            // resources
 *     if (!hbm)
 *         hbm = LoadImageA(NULL, name, IMAGE_BITMAP, cx, cy,
 *                          LR_LOADFROMFILE|LR_CREATEDIBSECTION);
 *     if (!hbm) return NULL;
 *     GetObjectA(hbm, 24, &bm);
 *     s = 0x10001240(&bm);
 *     DeleteObject(hbm);
 *     if (s) s->key = 0x07E0;
 *     return s;
 *
 * The resource arm has no host equivalent and always failed in the original
 * anyway (the names are relative paths and the DLL carries no bitmap
 * resources), so only the file arm survives.  The literal key store does not
 * depend on it and is kept exactly where it was: AFTER the conversion, and
 * only when the conversion succeeded.
 * ====================================================================== */
/* WHAT IT DOES: loads a Windows bitmap file from disk and turns it into a
 * drawing surface in the game's 16-bit colour format, tagging it with the
 * transparency key. Only 24-bit-colour bitmaps are accepted. The original
 * could also load a bitmap out of its own resources, but the DLL carries
 * none, so that path always failed and is not transcribed. */
/* @implements 0x10001290 glide BrBmpLoadSurface */
BrSurf *BrBmpLoadSurface(const char *pszPath, int32_t cx, int32_t cy)
{
    BrGdiBitmapMem mem;
    BrSurf        *pSurf;

    /* LoadImageA's stretch parameters. Every call site in BRGlide.dll passes
     * 0, 0 -- 0x1005841D and 0x100584CB, the only two -- so no host scaler is
     * needed to be faithful. Asserted rather than silently ignored. */
    if (cx != 0 || cy != 0) return NULL;

    if (BrBmpGdiLoad(&mem, pszPath) != 0) return NULL;

    pSurf = BrSurfFromBitmap(&mem.bm);      /* 0x10001240: the 24bpp gate */
    BrBmpGdiFree(&mem);                     /* DeleteObject */

    if (pSurf) pSurf->key = (uint16_t)BR_SURF_KEY_565;   /* 0x10001308 */
    return pSurf;
}

/* ======================================================================
 * 0x1005A210 / 0x10059F10 / 0x10059F70 -- the RGBA8888 texture loader
 *
 * This is the function br_bmp.c used to be, before anyone looked for it.
 * ====================================================================== */
/* WHAT IT DOES: loads a Windows bitmap file and hands back its pixels as
 * full-colour four-byte pixels ready for the 3D card, along with the
 * picture's size. It flips the image the right way up on the way through,
 * because bitmap files are stored bottom row first. Only 24-bit-colour
 * bitmaps are accepted. */
/* @implements 0x1005A210 glide BrBmpLoadRgba */
uint8_t *BrBmpLoadRgba(const char *pszPath, int32_t *pcx, int32_t *pcy)
{
    BrGdiBitmapMem mem;
    uint8_t       *pOut;
    const uint8_t *pRow;
    int32_t        y;

    if (BrBmpGdiLoad(&mem, pszPath) != 0) return NULL;

    /* 0x1005A247 -- the same gate as 0x1000124B, at the caller this time.
     * The original returns here without DeleteObject; the port has to free
     * the buffer or it would leak for real rather than reproducing a leak. */
    if (mem.bm.cBitsPixel != 24) { BrBmpGdiFree(&mem); return NULL; }

    /* 0x10059F10: malloc(cx*cy*4). */
    pOut = (uint8_t *)malloc((size_t)mem.bm.cx * (size_t)mem.bm.cy * 4u);
    if (!pOut) { BrBmpGdiFree(&mem); return NULL; }

    /* 0x10059F70: the same bottom-up walk as BrSurfBlt24, widening instead of
     * packing.  B is read first, then G, then R, and R,G,B,0xFF are stored. */
    pRow = mem.bm.pBits + (size_t)(mem.bm.cy - 1) * (size_t)mem.bm.cbWidthBytes;
    {
        uint8_t *pDst = pOut;
        for (y = mem.bm.cy; y != 0; y--) {
            const uint8_t *pSrc = pRow;
            int32_t        x;
            for (x = mem.bm.cx; x != 0; x--) {
                uint8_t b = pSrc[0], g = pSrc[1], r = pSrc[2];
                pSrc   += 3;
                *pDst++ = r;
                *pDst++ = g;
                *pDst++ = b;
                *pDst++ = 0xFF;
            }
            pRow -= mem.bm.cbWidthBytes;
        }
    }

    /* 0x10059F57/0x10059F60 publish these through globals; here they are out
     * parameters.  Written only on success, as the original writes them. */
    if (pcx) *pcx = mem.bm.cx;
    if (pcy) *pcy = mem.bm.cy;

    BrBmpGdiFree(&mem);
    return pOut;
}

/* ======================================================================
 * The host adaptor -- 0x10001290 then 565 -> RGBA8888
 * ====================================================================== */
int BrBmpLoad(BrBmp *pOut, const char *pszPath)
{
    BrSurf  *pSurf;
    size_t   n, i;

    if (!pOut || !pszPath) return -1;
    memset(pOut, 0, sizeof(*pOut));

    pSurf = BrBmpLoadSurface(pszPath, 0, 0);
    if (!pSurf) return -1;

    pOut->w = (uint32_t)pSurf->cx;
    pOut->h = (uint32_t)pSurf->cy;
    n = (size_t)pOut->w * pOut->h;

    pOut->pRgba = (uint8_t *)malloc(n * 4u);
    if (!pOut->pRgba) { BrSurfFree(pSurf); pOut->w = pOut->h = 0; return -1; }

    for (i = 0; i < n; i++) {
        /* DEVIATION: the original stops at 16 bits and blits them.  The host
         * needs 8888, so the surface is widened by bit replication -- see
         * br_surf.h for why replication rather than a shift is required for
         * the colour key to keep working. */
        uint32_t v = BrSurf565ToRgb(pSurf->pPix[i]);
        pOut->pRgba[i * 4 + 0] = (uint8_t)(v >> 16);
        pOut->pRgba[i * 4 + 1] = (uint8_t)(v >> 8);
        pOut->pRgba[i * 4 + 2] = (uint8_t)(v);
        pOut->pRgba[i * 4 + 3] = 0xFF;
    }

    BrSurfFree(pSurf);
    return 0;
}

void BrBmpApplyKey(BrBmp *pBmp, uint32_t rgb)
{
    size_t n, i;
    if (!pBmp || !pBmp->pRgba) return;
    n = (size_t)pBmp->w * pBmp->h;
    for (i = 0; i < n; i++) {
        uint8_t *p = pBmp->pRgba + i * 4u;
        uint32_t v = ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | p[2];
        if (v == rgb) p[3] = 0;
    }
}

void BrBmpFree(BrBmp *pBmp)
{
    if (!pBmp) return;
    free(pBmp->pRgba);
    pBmp->pRgba = NULL;
    pBmp->w = pBmp->h = 0;
}

/* ── Ghidra-matched functions ─────────────────────────── */
#ifdef BR_MATCHING_BUILD
extern char DAT_100ad7ec;
extern char DAT_100ad7f0;
extern char DAT_100ad7f4;
extern char DAT_100ad7f8;
int FUN_10059f70();
extern int DAT_10ac67c4;
extern int DAT_10ac67c8;
extern int DAT_100b22d8;

/* WHAT IT DOES: return the current BMP surface handle. */
/* @implements 0x1005A070 glide BrBmpGetHandle */

int BrBmpGetHandle(void)

{
  return DAT_100b22d8;
}

/* WHAT IT DOES: for a 24bpp bitmap header, malloc a cx*cy*4 RGBA buffer, convert the
 * pixels via 0x10059F70, and publish cx/cy in the two globals. NULL if not 24bpp. */
/* @implements 0x10059F10 glide BrBmpToRgba32 */

void * BrBmpToRgba32(int param_1)

{
  void *pvVar1;
  
  if (*(short *)(param_1 + 0x12) != 0x18) {
    return (void *)0x0;
  }
  pvVar1 = malloc(*(int *)(param_1 + 4) * *(int *)(param_1 + 8) * 4);
  if (pvVar1 != (void *)0x0) {
    FUN_10059f70(pvVar1,*(int *)(param_1 + 0x14),*(int *)(param_1 + 4),
                 *(int *)(param_1 + 8),*(int *)(param_1 + 0xc));
    DAT_10ac67c4 = *(int *)(param_1 + 4);
    DAT_10ac67c8 = *(int *)(param_1 + 8);
  }
  return pvVar1;
}

/* WHAT IT DOES: read the four words at +0/+4/+8/+0xC of record [param_1][param_2] from
 * the 30-per-row, 0x28-stride table at 0x100AD7EC into the four out-pointers. */
/* @implements 0x1005A020 glide BrBmpRect4Get */

void BrBmpRect4Get(int param_1,int param_2,int *param_3,int *param_4,
                 int *param_5,int *param_6)

{
  int iVar1;
  
  iVar1 = (param_2 + param_1 * 0x1e) * 0x28;
  *param_3 = *(int *)(&DAT_100ad7ec + iVar1);
  *param_4 = *(int *)(&DAT_100ad7f0 + iVar1);
  *param_5 = *(int *)(&DAT_100ad7f4 + iVar1);
  *param_6 = *(int *)(&DAT_100ad7f8 + iVar1);
  return;
}

#endif /* BR_MATCHING_BUILD */
