/* br_tex3d.h -- the LOAD-TIME texture pass, and the texel expander.
 *
 * WHAT THIS IS
 * ----------------------------------------------------------------------
 * br_dl.h's "HOW A TEXTURE REACHES THE BIND OPCODE" describes a chain that
 * runs once per display list at LOAD time and rewrites a run of RDP setup
 * commands into a single 0xDC bind.  Nothing in the port ran it, so every
 * model reported `binds=0` and the whole texture path was dead code.  This
 * module is that chain, transcribed:
 *
 *   0x10028820  BrTex3dScan      the nine-state walk, and its two tables
 *   0x10029420  G_SETTIMG        record siz + image address, open the run
 *   0x10029480  G_LOADTLUT       publish the palette address
 *   0x10029510  G_LOADBLOCK      publish the texel address
 *   0x100295B0  G_SETTILE        fill one of eight 0x40-byte tile records
 *   0x100296B0  G_SETTILESIZE    ... and its uls/ult/lrs/lrt
 *   0x100293D0  the run END      first unrecognised command wins
 *   0x10028B50  the SEAM         plant 0xDC over the run's first command
 *   0x10028BB0  the registrar     build the descriptor, dedup, append
 *   0x10027A70  the dedup        texel source + palette source + mode + 8B
 *   0x10027A10  AppendTexture    the growable array 0xDC indexes
 *   0x10027220  BrTexFormatCode  (slice1_04.c) -- everything but I4/I8/IA8
 *                                maps to 11 == ARGB1555; see br_dl.h
 *   0x100250D0  BrTex3dExpand    the expander, WHOLE -- all nine arms
 *   0x100271F0  the per-texel conversion
 *
 * THE THREE ARMS WERE NOT ENOUGH, AND THE REASON THEY LOOKED LIKE ENOUGH WAS
 * A CLAIM NOBODY DECODED.  br_dl.h used to say CI4/CI8/RGBA16 was "everything
 * the models actually use"; a census of every G_SETTILE in the canonical
 * load idiom across all nineteen shipped assets says otherwise -- RGBA16
 * 1155, CI4 277, I4 32, IA8 16, and CI8 ZERO.  testdata/tracks/desert.trk
 * carries five real textures the three arms could not touch (IA8 64x32 and
 * 64x64, I4 16x16 and two 32x8).
 *
 * ALL NINE ARMS ARE NOW TRANSCRIBED, under BrTex3dExpand.  What remains is
 * narrower and is a property of THIS FILE's wrapper rather than of the
 * original: BrTex3dDecode hands back 16-bit texels, and five of the nine
 * arms produce EIGHT-bit output (I4, AI44, I8 and the two the mode flag
 * gates), which that buffer cannot carry.  So BrTex3dDecode still answers
 * BR_TEX3D_UNSUPPORTED for them, and a caller that wants those formats calls
 * BrTex3dExpand.  `br_tex3d_bpp` is the wrapper's format gate, not the
 * expander's.
 *
 * THE STATE MACHINE, as the two tables at 0x10028A48 / 0x100289FC give it.
 * The byte table maps opcode-4 onto a state, the dword table maps the state
 * onto an arm; nine arms do real work and the rest fall through:
 *
 *   0x04 0xB1 0xBF  G_VTX / G_TRI2 / G_TRI1 -> the SEAM (the run has ended)
 *   0xB8            G_ENDDL                 -> stop
 *   0xB9            SETOTHERMODE_L          -> 0x10029710 then the run END
 *   0xBA            SETOTHERMODE_H          -> 0x10029780 (base LOD tile)
 *   0xE6 0xE7 0xE8  LOAD/PIPE/TILE sync     -> pure state transitions
 *   0xF0 0xF2 0xF3 0xF5   the four setup commands
 *   0xFA 0xFB       SETPRIM/SETENVCOLOR     -> eight dedup bytes
 *   0xFC            SETCOMBINE              -> the mode flag, and ONLY for
 *                   the single pair FC50FE04/3FFDF3F8; everything else
 *                   CLEARS it.  This is why 0x106B7AAC is 0 for retail car
 *                   models and why the dedup's eight-byte arm never runs.
 *   everything else -> the run END
 *
 * WHAT THE RUN IS.  The run OPENS at a G_SETTIMG seen in state 0 and CLOSES
 * at the first command that is not part of the set above -- or, if there is
 * none, at the G_VTX/G_TRI that triggers the seam.  0x10028B50 then writes
 *
 *     pRunStart->w0 = 0xDC000000 | (id & 0x00FFFFFF);
 *     pRunStart->w1 = (pRunEnd - pRunStart) / 8;
 *
 * which is exactly why the 0xDC handler returns `p + 8*w1`.
 *
 * MEASURED IN THE RETAIL DATA (testdata/ce.rca), the run is
 *
 *     FD SETTIMG(texels)  E6 LOADSYNC  F3 LOADBLOCK
 *     FD SETTIMG(tlut)    F0 LOADTLUT  E7 PIPESYNC
 *     [B9 SETOTHERMODE_L] E8 TILESYNC  F5 SETTILE  F2 SETTILESIZE
 *
 * -- note the SECOND SETTIMG.  G_SETTIMG only acts in states 0, 3 and 6, so
 * the texel address is captured by LOADBLOCK (state 2 -> 3) before the TLUT
 * address overwrites it, and the palette address is captured by LOADTLUT.
 * Get the order backwards and the palette becomes the texture.
 *
 * THE FORMAT IS READ, NOT INFERRED (br_dl.h says three earlier attempts in
 * this project failed by inferring it from byte statistics).  0x10027220
 * sends CI4, CI8 and RGBA16 to GrTextureFormat 11 == GR_TEXFMT_ARGB_1555,
 * and 0x100271F0 is
 *
 *     v = bswap16(v);  return (v >> 1) | ((v & 1) << 15);
 *
 * so every texel leaves the loader as 16-bit ARGB1555, little-endian, with
 * the palette ALREADY APPLIED and no palette state at draw time.
 *
 * THREE FACTS ABOUT THE SOURCE LAYOUT, all read off 0x100250D0:
 *   - the row PITCH is the tile's `line` field (already shifted left 3, so
 *     bytes), NOT the texel width.  0x100256BC adds it once per row.
 *   - ODD ROWS ARE WORD-SWAPPED.  The expander walks a row with a
 *     4-forward/8-back cursor whenever (y & mask) is set, and the registrar
 *     sets that mask (descriptor +0x298) to 1 unconditionally.  In byte
 *     terms the swizzle is exactly `i ^ 4`, and it applies to CI4, CI8 and
 *     RGBA16 alike -- it is the N64's TMEM interleave.
 *   - the texel cursor starts at texelSource + tile.tmem * 8.
 *
 * NOT MODELLED, and each says so where it matters: the registrar's LOD-count
 * trim (0x106B7AB0..0x106B7A94; retail car models have one level -- and note
 * that 0x10027B60 hands the expander descriptor +0x58 as the FIRST level and
 * +0x5C as the bound, while 0x10028F07 writes the SAME value to both, so
 * something after 0x10028F19 must raise +0x5C and this file does not model
 * what), the Glide TMEM allocator 0x10028200/0x100283C0, and the
 * aspect/shift encoding the registrar computes for grTexDownloadMipMap.
 *
 * TWO ARMS ARE REACHED BY SHIPPED DATA AND ARE STILL REFUSED BY THE WRAPPER:
 * IA8 and I4, both in testdata/tracks/desert.trk (see the census above and
 * in br_dl.h).  That paragraph once named them "IA4/I8/IA8", which was
 * inherited from br_dl.h and wrong twice -- IA4 is a catch-all case in
 * 0x10027220, and I8 occurs in no shipped asset at all.  Both are
 * transcribed in BrTex3dExpand now; test_br_tex3d.c's
 * `test_unsupported_is_refused` pins that the 16-bit ENTRY POINT keeps
 * saying no rather than truncating them.
 */
#ifndef BR_TEX3D_H
#define BR_TEX3D_H

#include <stdint.h>
#include <stddef.h>

#define BR_TEX3D_TILES 8

/* One 0x40-byte tile record, 0x10697840 + tile*0x40.  Field order and
 * offsets are 0x100295B0's and 0x100296B0's, in their order. */
typedef struct BrTex3dTile {
    int32_t fmt, siz;            /* +0x00 +0x04  (w0>>21)&7, (w0>>19)&3     */
    int32_t line, tmem;          /* +0x08 +0x0C  ((w0>>9)&0x1FF)<<3, w0&0x1FF */
    int32_t mirrorS, clampS;     /* +0x10 +0x14  (w1>>8)&1, (w1>>9)&1      */
    int32_t mirrorT, clampT;     /* +0x18 +0x1C  (w1>>18)&1, (w1>>19)&1    */
    int32_t maskS, maskT;        /* +0x20 +0x24  (w1>>4)&0xF, (w1>>14)&0xF */
    int32_t shiftS, shiftT;      /* +0x28 +0x2C  w1&0xF, (w1>>10)&0xF      */
    int32_t uls, ult, lrs, lrt;  /* +0x30..+0x3C 10.2, from G_SETTILESIZE   */
} BrTex3dTile;

/* One entry of the array 0xDC's low 24 bits index (0x106B7AA0, stride
 * 0x2B4).  Only the part of the 0x2B0-byte descriptor a portable backend
 * can use is kept; the descriptor offset each field came from is quoted. */
typedef struct BrTex3dRec {
    int32_t     w, h;            /* +0x08 +0x0C  1<<mask, doubled to mirror */
    int32_t     fmtCode;         /* +0x10  BrTexFormatCode; 11 == ARGB1555  */
    int32_t     clampS, clampT;  /* +0x24 +0x28                             */
    int32_t     divW, divH;      /* +0x40 +0x44  the texel-scale divisors   */
    uint32_t    texelSrc;        /* +0x48  a 32-bit display-list address    */
    uint32_t    palSrc;          /* +0x4C  0 when the tile carries no TLUT  */
    int32_t     baseLod, cLods;  /* +0x58 +0x5C                             */
    int32_t     mode;            /* +0x264 (0x106B7AAC)                     */
    uint8_t     aKey[8];         /* +0x290..+0x297, the dedup's state bytes */
    BrTex3dTile tile;            /* +0x60 + baseLod*0x40, verbatim          */
} BrTex3dRec;

typedef struct BrTex3d {
    /* 0x10697840.  PERSISTENT ACROSS SCANS: 0x10028820 clears its state
     * machine on entry but not the tile records, so a list that issues no
     * G_SETTILE inherits the previous list's tile.  Preserved. */
    BrTex3dTile aTile[BR_TEX3D_TILES];

    int32_t  state;              /* 0x105E17FC, the nine states             */
    int32_t  maxTile;            /* 0x106B7A94                              */
    int32_t  baseTile;           /* 0x106B7AB0                              */
    int32_t  mode;               /* 0x106B7AAC                              */
    int32_t  imgSiz;             /* 0x105E1804                              */
    uint32_t imgAddr;            /* 0x105E180C                              */
    uint32_t texelSrc;           /* 0x105D17F0                              */
    uint32_t palSrc;             /* 0x106B7A98                              */
    uint8_t  aKey[8];            /* the SETPRIM/SETENVCOLOR bytes           */

    BrTex3dRec *aRec;            /* 0x106B7AA0                              */
    uint32_t    cRec, cRecMax;   /* 0x10697A58, 0x10697A5C                  */

    /* counters -- not in the original, the port's only way to assert */
    uint32_t cPlanted;           /* 0xDC commands written                   */
    uint32_t cRuns;              /* runs that reached the seam              */
    uint32_t cMultiLod;          /* records whose LOD chain is deeper than 1 */
} BrTex3d;

void BrTex3dInit(BrTex3d *pTex);
void BrTex3dFree(BrTex3d *pTex);

/* 0x10028820.  Walk ONE host-order display list (BrDlPatch has already run
 * over it) to G_ENDDL, planting a 0xDC over every texture-setup run.
 * Returns the number of commands walked.  `cbMax` bounds the walk; the
 * original has no such bound.  DEVIATION, for the same reason BrDlRun has
 * one: a test must not be able to run off the end of a malformed list. */
size_t BrTex3dScan(BrTex3d *pTex, uint8_t *pList, size_t cbMax);

/* BrTex3dDecode's return codes.  A caller that gets anything but OK has a
 * texture the expander's ported arms do not cover, and should say so rather
 * than draw something plausible. */
enum {
    BR_TEX3D_OK = 0,
    BR_TEX3D_BADID,
    BR_TEX3D_UNSUPPORTED,   /* an (fmt,siz) pair only the untranscribed arms
                             * of 0x100250D0 handle -- IA4, I8, IA8, RGBA32 */
    BR_TEX3D_NOSRC,         /* texels (or, for CI, the palette) unresolved  */
    BR_TEX3D_DEGENERATE     /* w or h is not positive                       */
};

/* 0x100250D0, THE EXPANDER, whole -- all nine arms and both mirrors.
 *
 * This is the GLIDE body.  The same dispatch slot holds a DIFFERENT function
 * in BRD3D.dll (0x10025AB0) and config/shared.csv classes the pair
 * `renderer`; the two differ in the destination pixel format -- 16-bit
 * stores and an ARGB1555 helper here, 32-bit stores and an ARGB8888 helper
 * there.  See the banner at the definition for the measurement.
 *
 * The nine arms, by the (siz, fmt, mode) triple that selects them:
 *
 *   siz 0  fmt 2            CI4  -> ARGB1555 through the palette
 *   siz 0  fmt 2  flags&2   CI4  -> the bare 4-bit index, widened to u16,
 *                                   and only for level 1
 *   siz 0  fmt 4  mode 1    I4   -> ARGB1555, brightness mixing two colours
 *   siz 0  fmt 4            I4   -> u8, the nibble replicated (i * 17)
 *   siz 1  fmt 2            CI8  -> ARGB1555 through the palette
 *   siz 1  fmt 3  mode 1    IA8  -> ARGB4444, mixed colour + the low nibble
 *   siz 1  fmt 3            IA8  -> u8 with the nibbles SWAPPED, i.e. AI44
 *   siz 1  fmt 4            I8   -> u8, copied
 *   siz 2  fmt 0            RGBA16 -> ARGB1555 through 0x100271F0
 *
 * Anything else is a no-op for that level.  `cbOut` is a byte budget, tested
 * AFTER each element is stored, so one element is always written; `maskOdd`
 * is the row-swizzle mask (descriptor +0x298, always 1 in shipped data);
 * `hi0..hi3` / `lo0..lo3` are the eight dedup state bytes, and lo2/lo3 are
 * the two the registrar never writes.  Arguments are in the original's
 * order. */
void BrTex3dExpand(uint8_t *pOut, int32_t cbOut, int32_t siz,
                   const uint8_t *pTexels, const uint8_t *pPal, int32_t fmt,
                   int32_t fMirrorS, int32_t fMirrorT,
                   int32_t lod, int32_t lodEnd,
                   const BrTex3dTile *aTile, int32_t flags, int32_t mode,
                   int32_t hi0, int32_t hi1, int32_t hi2, int32_t hi3,
                   int32_t lo0, int32_t lo1, int32_t lo2, int32_t lo3,
                   int32_t maskOdd);

/* The validated wrapper over BrTex3dExpand for the three arms whose output
 * is 16 bits wide, for the BASE LOD.  `pTexels` must address at least the
 * whole source rectangle and `pPal` 32 bytes when the format is CI; `pOut`
 * receives w*h ARGB1555 halfwords in host order.  The five arms whose output
 * is 8 bits wide, and the raw-index one, are reachable only through
 * BrTex3dExpand -- this entry point's buffer contract cannot express them,
 * which is why it still answers BR_TEX3D_UNSUPPORTED for those formats. */
int BrTex3dDecode(const BrTex3d *pTex, uint32_t id,
                  const uint8_t *pTexels, size_t cbTexels,
                  const uint8_t *pPal, size_t cbPal,
                  uint16_t *pOut);

/* How many source bytes the base LOD reads, so a caller can bounds-check
 * before resolving.  Zero when the record is degenerate. */
size_t BrTex3dSrcBytes(const BrTex3d *pTex, uint32_t id);

/* THE TEXTURE COORDINATE SCALE, and it is not a detail.
 *
 * 0x10027850 computes two floats per record (record +0x2AC/+0x2B0) and the
 * bind handler 0x100284E0 fstp's them into 0x118ED1A4 / 0x118ED1A8, which
 * br_dl.h notes br_dl_finish_vtx currently holds at 1.0.  Read off the
 * instructions and the three constants they multiply by -- 0x10077450 ==
 * 0.03125, 0x10077454 == 256.0, 0x10077458 == 0.0009765625 -- the value is
 *
 *     scale = (1/32) * 256 / divW * 2^-shiftS      (shiftS <= 10)
 *     scale = (1/32) * 256 / divW * 2^(16-shiftS)  (shiftS > 10)
 *
 * The 1/32 is the S10.5 fixed point in the N64 Vtx; the 256 is Glide's
 * texture-coordinate space, which runs 0..256 across a texture whatever its
 * size.  A backend that samples with NORMALISED coordinates therefore wants
 * this divided by 256 -- which is what BrTex3dTexScaleNorm returns:
 *
 *     u = s_vertex * scaleNorm
 *
 * Note the divisor is the descriptor's +0x40, not the uploaded width: with
 * the mirror bit the uploaded texture is twice as wide and u == 1 lands in
 * the middle of it, which is exactly how the mirror is meant to work. */
void BrTex3dTexScaleNorm(const BrTex3d *pTex, uint32_t id,
                         float *pScaleS, float *pScaleT);

/* ARGB1555 (alpha in bit 15) -> RGBA8, four bytes per texel.  NOT in the
 * original: Glide takes ARGB1555 directly and Metal's packed 16-bit formats
 * do not agree with it on channel order, so the port expands on upload --
 * which br_dl.h names as the expected shape for a Metal backend. */
void BrTex3dToRgba8(const uint16_t *pArgb1555, uint32_t count, uint8_t *pRgba);

#endif /* BR_TEX3D_H */
