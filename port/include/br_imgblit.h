/* br_imgblit.h -- RESPONSIBILITY: turn images into pixels.  Glide 0x1006C990,
 * the full-screen `.img` blitter, and the only consumer of the `.img` format
 * in the Glide build.
 *
 * WHERE IT IS CALLED FROM, and why both call sites matter
 *
 *   0x1003255D   0x1006C990("splash.img", 0x2AC7E58B)   cold init, state 0
 *   0x1001CDFB   0x1006C990("loading.img", 0)           state 3, the loading
 *                                                       screen
 *
 * WHAT 0x2AC7E58B IS.  It is not a flag and not a colour: it is the **zlib
 * Adler-32 of splash.img's pixel payload** -- the file from byte 12 to EOF,
 * 131072 bytes, seeded the way the original asks for a seed
 * (`BrAdler32(0, NULL, 0)`, which returns 1).  Measured against the shipped
 * `testdata/splash.img`, it matches exactly.  The second argument is
 * therefore an *expected checksum*, and zero means "do not check":
 *
 *      0x1006CA09   test edi,edi ; je (skip)
 *      0x1006CA13   BrAdler32(0, NULL, 0)              -> the seed, 1
 *      0x1006CA22   BrAdler32(seed, 0x1184C488, cb)    -> the sum
 *      0x1006CA2A   cmp eax, edi ; je (ok)
 *      0x1006CA30   exit(1)
 *
 * `loading.img`'s sum is 0x440E8E2C, and state 3 passes 0, so only the splash
 * is protected.  A mismatch is fatal -- `exit(1)`, not a return.
 *
 * THIS IS NOT A DECODER, AND br_img.c IS NOT WHAT IT USES.
 *
 * `port/src/drawing/br_img.c` expands `.img` to RGBA8888 on the host.  The
 * original does no such thing: it `fread`s the raw 16-bit payload into one
 * fixed scratch buffer and hands that buffer straight to Glide as texture
 * data.  There is no pixel loop anywhere in these 994 bytes.  So br_img.c is
 * not a decoder this module can wire -- there is nothing to decode.
 *
 * What br_img.c CAN now be told is the thing its own comment says it is
 * guessing.  br_img.c detects the channel order from pixel statistics and
 * carries a TODO asking for "the flag the original passes".  There is no such
 * flag.  The original declares the texture ARGB1555 unconditionally:
 *
 *      0x1006CA54   push 0xb   ->  0x10028200's `format` argument, stored at
 *                   GrTexInfo+0x0C by 0x10028254
 *
 * and 0xB is `GR_TEXFMT_ARGB_1555` in the same Glide enum CONVENTIONS.md
 * already uses for the font (format 4 == `GR_TEXFMT_ALPHA_INTENSITY_44`).
 * So `.img` is ARGB1555 -- bit 15 alpha -- for BOTH files, and br_img.h's
 * header text is right while its run-time sniffing is not needed.
 *
 * NOT TO BE CONFUSED WITH THE 0x07E0 COLOUR KEY.  The RGB565 / colour-key
 * 0x07E0 note recorded in this tree belongs to `br_surf.h` / `br_bmp.h` -- the
 * DirectDraw software sprite path that loads `images\*.bmp`.  It says nothing
 * about `.img`, which never goes near that blitter.  Two formats, two paths.
 *
 * THE FORMAT WORD IS READ AND IMMEDIATELY DESTROYED.  The header is three
 * dwords and the original reads them in two steps:
 *
 *      0x1006C9B1   BrChkFRead(&w, 4, 1, f)          -> local
 *      0x1006C9C3   BrChkFRead(&h, 4, 1, f)          -> local
 *      0x1006C9E0   BrChkFRead(0x1184C488, 4, 1, f)  -> the PIXEL BUFFER
 *      0x1006C9F1   BrChkFRead(0x1184C488, w*h*2, 1, f)
 *
 * The third read lands the format word at the START of the pixel buffer, and
 * the fourth overwrites it.  The format is consumed and discarded; nothing
 * ever tests it.  That overlap is deliberate here and is modelled, not
 * tidied.
 *
 * `w*h*2` is computed as `imul` then `shl esi,1` -- the *2 is bytes per
 * pixel, not a dword count.
 *
 * WHAT THE TEXTURE IS DECLARED AS, versus what the file holds
 *
 * 0x10028200's GrTexInfo fields come from four of its fifteen arguments and
 * are all literals here (0x1006CA3E..0x1006CA5D):
 *
 *      smallLodLog2 = 0    GR_LOD_256      arg7
 *      largeLodLog2 = 0    GR_LOD_256      arg8
 *      aspectRatio  = 3    GR_ASPECT_1x1   arg9
 *      format       = 0xB  ARGB_1555       arg5
 *
 * i.e. a 256x256 texture, regardless of the file.  `loading.img` is 256x200,
 * so the download reads 28,672 bytes past the payload.  It is harmless in the
 * original -- the scratch buffer is 0x1184C488, deep inside a 24 MB `.data`
 * -- and the quad only samples rows 0..h-1, but it is real and it is why this
 * header states the declared size separately from the file's.
 *
 * THE PLACEMENT IS HALF FIXED AND HALF FROM THE FILE, which is the single
 * easiest thing here to get wrong:
 *
 *      x0 = (screenW - 256) / 2      <- the literal 0x100, NOT the file width
 *      y0 = (screenH - 256) / 2      <- likewise
 *      x1 = fileW + x0 - 1
 *      y1 = fileH + y0 - 1
 *
 * For the 256x256 splash the two agree and the quad is centred.  For the
 * 256x200 loading screen they do not: at 640x480 it is placed at y=112 and is
 * 200 tall, so it sits ABOVE centre by 28 pixels.  That asymmetry is the
 * original's.
 *
 * The clamp is an x87 unordered compare against 0.0f at 0x10077C10:
 * `fcomp ; fnstsw ; test ah,1 ; je` stores 0 when C0 is set, and C0 is also
 * set for unordered -- so it is written `!(v >= 0.0f)`, per CONVENTIONS.md.
 * The `- 1` is the literal 1.0f at 0x10077C14, applied to x1/y1 only.
 *
 * The integer halving is `lea eax,[ecx-0x100] ; cdq ; sub eax,edx ; sar eax,1`
 * -- signed division truncating toward zero, which differs from `>> 1` for a
 * screen narrower than 256.  Reproduced.
 *
 * THE QUAD.  Four `GrVertex` on the stack at stride 0x3C, then two
 * `grDrawTriangle` calls.  Established by tracking every push between the
 * `lea`s -- `push esi` at 0x1006CB59 and `push ecx` at 0x1006CBBC sit in the
 * middle of the vertex stores, so displacements before and after them name
 * different slots (CONVENTIONS.md, "a stack displacement means nothing
 * without the ESP it is relative to").  Both draw blocks resolve to the same
 * four slots by two different displacement sets, which is the cross-check.
 *
 *      v0 = (x1, y1)   s = fileW-1   t = 0
 *      v1 = (x0, y0)   s = 0         t = fileH-1
 *      v2 = (x0, y1)   s = 0         t = 0
 *      v3 = (x1, y0)   s = fileW-1   t = fileH-1
 *
 *      triangles: (v2, v0, v1) then (v0, v3, v1)
 *
 * so t runs OPPOSITE to y.  Every vertex gets r=g=b=a=255.0f and oow=1.0f;
 * z, ooz and the two TMU `oow` slots are never written and go to Glide as
 * whatever was on the stack.  This module writes the same nine fields and
 * leaves the caller's buffer alone elsewhere, rather than zeroing.
 *
 * IT DRAWS THE QUAD TWICE, ONCE PER BUFFER.  Clip, clear, two triangles,
 * present -- then the identical sequence again.  So the splash lands in both
 * buffers and survives the next flip.  The two blocks are byte-for-byte the
 * same work.
 *
 * IT ALSO WIPES THE TEXTURE MANAGER, TWICE.  0x100281C0 runs at 0x1006CA39
 * (before the allocation) and again at 0x1006CD64 (last thing before the
 * `ret`).  It zeroes all 1024 0xD8-byte texture records at 0x10661844 and
 * resets both TMU allocation cursors to `grTexMinAddress`, so every texture
 * the game had resident is dropped.  Calling this function is destructive to
 * anything already uploaded; that is why the boot uses it and nothing else
 * does.
 *
 * THERE IS NO D3D COUNTERPART.  `config/shared.csv` has no row for
 * 0x1006C990, and the D3D build reaches its splash through a __thiscall on a
 * C++ object instead -- 0x1002F73D pushes "loading.img" and 0 and calls
 * 0x10008E30 with `ecx = [0x118ABE08]`.  Different function, different shape.
 * This one is Glide-only, and that is expected: its body is Glide texture and
 * triangle calls from top to bottom.
 */
#ifndef BR_IMGBLIT_H
#define BR_IMGBLIT_H

#include <stddef.h>
#include <stdint.h>

#include "br_dl.h"      /* BrDlVtx -- its first 0x3C bytes ARE a GrVertex */

/* ------------------------------------------------------------------ *
 * The two literals the call sites pass, so a caller never re-types them.
 * 0x100AA3C8 and 0x100A9924 are the string addresses.
 * ------------------------------------------------------------------ */
#define BR_IMGBLIT_SPLASH_NAME   "splash.img"
#define BR_IMGBLIT_LOADING_NAME  "loading.img"

/* 0x10032553.  The expected Adler-32 of splash.img's payload. */
#define BR_IMGBLIT_SPLASH_ADLER  0x2AC7E58Bu

/* Not passed by anything in the original, and recorded because it is the
 * measured value for the other file: state 3 passes 0 instead. */
#define BR_IMGBLIT_LOADING_ADLER 0x440E8E2Cu

/* 0x1006CAAA / 0x1006CADC -- the literal 0x100 the centring uses, which is
 * NOT the image's own width or height. */
#define BR_IMGBLIT_CENTRE_SPAN   256

/* The GrTexInfo the original declares, from 0x10028200's arguments. */
#define BR_IMGBLIT_TEX_SMALLLOD  0     /* GR_LOD_256                        */
#define BR_IMGBLIT_TEX_LARGELOD  0     /* GR_LOD_256                        */
#define BR_IMGBLIT_TEX_ASPECT    3     /* GR_ASPECT_1x1                     */
#define BR_IMGBLIT_TEX_FORMAT    0xB   /* GR_TEXFMT_ARGB_1555               */
#define BR_IMGBLIT_TEX_EVENODD   3     /* GR_MIPMAPLEVELMASK_BOTH, arg2     */

/* The declared texture is 256x256 ARGB1555 whatever the file holds. */
#define BR_IMGBLIT_TEX_BYTES     (256 * 256 * 2)

/* ------------------------------------------------------------------ *
 * The header, as three little-endian dwords read one at a time.
 * ------------------------------------------------------------------ */
typedef struct BrImgBlitHdr {
    int32_t  cx;       /* [esp+0x18] -- first dword                        */
    int32_t  cy;       /* [esp+0x1C] -- second dword                       */
    uint32_t fmt;      /* the third dword, read into the pixel buffer and
                        * then overwritten.  Captured here because it is
                        * observable in the buffer between the two reads,
                        * and never used for anything.                     */
    int32_t  cbPixels; /* cx * cy * 2, the `imul` + `shl 1`                */
} BrImgBlitHdr;

/* BrImgBlitLoad's result. */
enum {
    BR_IMGBLIT_OK = 0,
    BR_IMGBLIT_NOFILE,    /* BrChkFReadOpen returned NULL                  */
    BR_IMGBLIT_TOOBIG,    /* DEVIATION -- see BrImgBlitLoad                */
    BR_IMGBLIT_BADSUM     /* the Adler-32 did not match; the original
                           * exit(1)s at this point                        */
};

/* ------------------------------------------------------------------ *
 * The load half: 0x1006C990 .. 0x1006CA36.
 *
 * Reads the header and the payload into `pvBuf`, replicating the format
 * word's landing in the buffer and being overwritten.  When `uAdler` is
 * non-zero it computes BrAdler32 over exactly `cbPixels` bytes of the buffer
 * and compares.
 *
 * DEVIATION, and it is the only one: the original has no bound check at all
 * -- `cbPixels` comes from the file and the destination is a fixed global.
 * A hostile or corrupt header would write anywhere in `.data`.  This returns
 * BR_IMGBLIT_TOOBIG instead of smashing the caller's buffer, and does not
 * read the payload.  The shipped files need exactly 131072 and 102400 bytes.
 *
 * DEVIATION: the original's `exit(1)` on a checksum mismatch is NOT here; it
 * is at its own site inside BrImgBlitFullScreen.  This function reports
 * BR_IMGBLIT_BADSUM and fills *puAdler so the value is inspectable.  Nothing
 * observable moves: the caller exits at the same point in the same sequence.
 *
 * `puAdler` may be NULL.  On BR_IMGBLIT_NOFILE nothing else is written.
 * ------------------------------------------------------------------ */
int BrImgBlitLoad(const char *pszName, uint32_t uAdler,
                  void *pvBuf, size_t cbBuf,
                  BrImgBlitHdr *pHdr, uint32_t *puAdler);

/* ------------------------------------------------------------------ *
 * The placement half: 0x1006CAA4 .. 0x1006CB34.
 *
 * `screenW`/`screenH` are the globals 0x100A7514 / 0x100A7518, which
 * br_boot.h already owns as g_brAppModeW / g_brAppModeH -- the pair
 * CreateWindowExA reads, i.e. the mode in the operative sense.
 * ------------------------------------------------------------------ */
void BrImgBlitPlace(int32_t cx, int32_t cy, int32_t screenW, int32_t screenH,
                    float *px0, float *py0, float *px1, float *py1);

/* 0x1006CB08 .. 0x1006CCC5.  Fills the four vertices in the original's own
 * order -- av[0] is the (x1,y1) corner.  Writes x, y, r, g, b, a, oow and
 * tmu0[0..1] only; every other field of `av` is left exactly as it was, which
 * is what the original does with its uninitialised stack. */
void BrImgBlitQuad(BrDlVtx av[4], int32_t cx, int32_t cy,
                   float x0, float y0, float x1, float y1);

/* ------------------------------------------------------------------ *
 * The Glide half.  Every one of these is an unported callee -- the frontier.
 * A NULL entry is SKIPPED and COUNTED (BrImgBlitSkipped), never faked, and
 * pfnTexAlloc's absence yields handle 0 rather than an invented one.
 * ------------------------------------------------------------------ */
typedef enum BrImgBlitStep {
    BR_IMGBLIT_TEXRESET = 0,  /* 0x100281C0, called twice                  */
    BR_IMGBLIT_TEXALLOC,      /* 0x10028200, 15 cdecl args                 */
    BR_IMGBLIT_TEXDOWNLOAD,   /* 0x100283C0                                */
    BR_IMGBLIT_TEXSOURCE,     /* 0x10028420                                */
    BR_IMGBLIT_TEXCOMBINE,    /* 0x1007298A -> glide2x _grTexCombine@28    */
    BR_IMGBLIT_COLORCOMBINE,  /* 0x10072990 -> glide2x _grColorCombine@20  */
    BR_IMGBLIT_CLIPWINDOW,    /* 0x100729D2 -> glide2x _grClipWindow@16    */
    BR_IMGBLIT_BUFFERCLEAR,   /* 0x100729BA -> glide2x _grBufferClear@12   */
    BR_IMGBLIT_DRAWTRIANGLE,  /* 0x100729EA -> glide2x _grDrawTriangle@12  */
    BR_IMGBLIT_PRESENT,       /* call [0x106B7AB8], set to 0x1001DD50 by
                               * 0x1001E080 -- the installed present hook  */
    BR_IMGBLIT_NSTEPS
} BrImgBlitStep;

/* The fifteen dwords 0x1006CA3E..0x1006CA5D pushes, in argument order.
 * Kept as an array because their meaning is positional and only five are
 * decoded; BR_IMGBLIT_TEXARG_* index the ones that are. */
#define BR_IMGBLIT_TEXARGS  15
#define BR_IMGBLIT_TEXARG_EVENODD   1   /* arg2  == BR_IMGBLIT_TEX_EVENODD */
#define BR_IMGBLIT_TEXARG_FORMAT    4   /* arg5  == BR_IMGBLIT_TEX_FORMAT  */
#define BR_IMGBLIT_TEXARG_SMALLLOD  6   /* arg7                            */
#define BR_IMGBLIT_TEXARG_LARGELOD  7   /* arg8                            */
#define BR_IMGBLIT_TEXARG_ASPECT    8   /* arg9                            */
extern const int32_t g_aBrImgBlitTexArgs[BR_IMGBLIT_TEXARGS];

/* grColorCombine(3, 8, 1, 1, 0):
 *   function SCALE_OTHER, factor ONE, local ITERATED, other TEXTURE, no
 *   invert -- the pixel is the texel, unmodulated. */
#define BR_IMGBLIT_CC_FUNCTION  3
#define BR_IMGBLIT_CC_FACTOR    8
#define BR_IMGBLIT_CC_LOCAL     1
#define BR_IMGBLIT_CC_OTHER     1
#define BR_IMGBLIT_CC_INVERT    0

/* grBufferClear(0, 0, 0xFFFF) -- colour 0, alpha 0, depth 0xFFFF. */
#define BR_IMGBLIT_CLEAR_DEPTH  0xFFFF

typedef struct BrImgBlitOps {
    void    (*pfnTexReset)(void *pUser);
    int32_t (*pfnTexAlloc)(void *pUser, const int32_t *paArgs);
    void    (*pfnTexDownload)(void *pUser, int32_t hTex, const void *pvData,
                              int32_t nLevel);
    void    (*pfnTexSource)(void *pUser, int32_t hTex);
    void    (*pfnTexCombine)(void *pUser, int32_t tmu,
                             int32_t rgbFunc, int32_t rgbFactor,
                             int32_t aFunc, int32_t aFactor,
                             int32_t rgbInvert, int32_t aInvert);
    void    (*pfnColorCombine)(void *pUser, int32_t func, int32_t factor,
                               int32_t local, int32_t other, int32_t invert);
    void    (*pfnClipWindow)(void *pUser, int32_t minx, int32_t miny,
                             int32_t maxx, int32_t maxy);
    void    (*pfnBufferClear)(void *pUser, uint32_t colour, uint32_t alpha,
                              uint32_t depth);
    void    (*pfnDrawTriangle)(void *pUser, const BrDlVtx *pA,
                               const BrDlVtx *pB, const BrDlVtx *pC);
    void    (*pfnPresent)(void *pUser);
    void     *pUser;
} BrImgBlitOps;

/* ------------------------------------------------------------------ *
 * Glide 0x1006C990 entire.  No D3D counterpart -- see the banner.
 *
 * `pvBuf`/`cbBuf` stand for the fixed scratch at 0x1184C488 (slice1_01.h's
 * house rule: a fixed global the original bakes in becomes a parameter).
 * `screenW`/`screenH` stand for 0x100A7514 / 0x100A7518 for the same reason.
 *
 * Returns nothing, exactly as the original does.  On a checksum mismatch it
 * calls exit(1) at the original's site and does not return.
 * ------------------------------------------------------------------ */
void BrImgBlitFullScreen(const char *pszName, uint32_t uAdler,
                         void *pvBuf, size_t cbBuf,
                         int32_t screenW, int32_t screenH,
                         const BrImgBlitOps *pOps);

/* How many times a step was reached with no hook installed.  Zero for a fully
 * wired run.  Not in the original. */
int32_t BrImgBlitSkipped(BrImgBlitStep step);
void    BrImgBlitResetForTest(void);

#endif /* BR_IMGBLIT_H */
