/* br_tex3d.c -- see br_tex3d.h.  The load-time texture pass, transcribed
 * from BRGlide.dll 0x10028820 and the eleven routines it reaches. */

#include "br_tex3d.h"
#include "slice1_04.h"      /* BrTexFormatCode (0x10027220 == 0x10027B90) */

#include <stdlib.h>
#include <string.h>

/* Host-order word access, byte-wise.  Identical to br_dl.c's, and it has to
 * be: BrDlPatch leaves the list in this layout and the interpreter reads the
 * opcode out of byte 3. */
static uint32_t br_tex3d_w(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void br_tex3d_putw(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;         p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

void BrTex3dInit(BrTex3d *pTex)
{
    if (pTex == NULL)
        return;
    memset(pTex, 0, sizeof(*pTex));
}

void BrTex3dFree(BrTex3d *pTex)
{
    if (pTex == NULL)
        return;
    free(pTex->aRec);
    memset(pTex, 0, sizeof(*pTex));
}

/* ==================================================================== */
/* 0x10027A70 -- the dedup                                              */
/* ==================================================================== */

/* Keys on the texel source, the palette source and -- ONLY when BOTH the
 * candidate and the probe have mode == 1 -- eight render-state bytes.
 *
 * The `mode != 1` short-circuit ACCEPTS the record; it is not a skip, and
 * inverting it is the easy mistake (slice1_04.h flags the same asymmetry in
 * the D3D twin).  Since 0x100289AD sets mode for exactly one G_SETCOMBINE
 * pair and clears it for every other, retail data never reaches the byte
 * comparison at all.
 *
 * DEVIATION, recorded because it is a real defect in the original: the
 * registrar writes descriptor bytes +0x290..+0x295 and +0x29A..+0x29B but
 * NOT +0x296/+0x297, and the dedup compares +0x290..+0x297 -- so two of the
 * eight bytes are uninitialised stack.  The port zeroes them. */
static int32_t br_tex3d_find(const BrTex3d *pTex, const BrTex3dRec *pProbe)
{
    uint32_t i;

    for (i = 0; i < pTex->cRec; i++) {
        const BrTex3dRec *pR = &pTex->aRec[i];
        if (pR->texelSrc != pProbe->texelSrc)  continue;
        if (pR->palSrc   != pProbe->palSrc)    continue;
        if (pR->mode != 1)                     return (int32_t)i;
        if (pProbe->mode != 1)                 return (int32_t)i;
        if (memcmp(pR->aKey, pProbe->aKey, sizeof(pR->aKey)) == 0)
            return (int32_t)i;
    }
    return -1;
}

/* 0x10027A10 -- AppendTexture.  Returns the OLD count, which is the index
 * the 0xDC command carries. */
/* WHAT IT DOES: adds a newly recognised texture to the game's texture table
 * and reports the slot it went into, growing the table in blocks of 256 when
 * it fills up. The slot number it gives back is the number that gets written
 * into the drawing list in place of the texture-load commands. */
/* @implements 0x10027A10 glide br_tex3d_append */
static int32_t br_tex3d_append(BrTex3d *pTex, const BrTex3dRec *pRec)
{
    if (pTex->cRec >= pTex->cRecMax) {
        uint32_t n = pTex->cRecMax + 0x100u;    /* grown 0x100 at a time */
        BrTex3dRec *p = (BrTex3dRec *)realloc(pTex->aRec,
                                              (size_t)n * sizeof(*p));
        if (p == NULL)
            return -1;
        pTex->aRec = p;
        pTex->cRecMax = n;
    }
    pTex->aRec[pTex->cRec] = *pRec;
    return (int32_t)pTex->cRec++;
}

/* ==================================================================== */
/* 0x10028BB0 -- the registrar                                          */
/* ==================================================================== */

/* Builds the descriptor, looks for an identical one, appends when there is
 * none.  Returns the index or -1.
 *
 * Only the fields a portable backend can act on are built.  What is left
 * out is the Glide side: the TMEM slot, the shift/aspect encoding
 * (0x100242E0 / 0x100275C0, == slice1_04's BrTexShiftFromSize and
 * BrTexAspectFromSize), the LOD-count trim that walks the tile array
 * looking for each successive halving, and the 0x80000-byte TMEM clamp.
 * None of them changes which pixels the texture has. */
static int32_t br_tex3d_register(BrTex3d *pTex)
{
    BrTex3dRec rec;
    const BrTex3dTile *pT;
    int32_t base, last, w, h, tileW, tileH;

    memset(&rec, 0, sizeof(rec));

    /* 0x10028C5C: the base LOD is 0x106B7AB0 and the last is 0x106B7A94,
     * raised to the base when it is below it. */
    base = pTex->baseTile;
    if (base < 0) base = 0;
    if (base >= BR_TEX3D_TILES) base = BR_TEX3D_TILES - 1;
    last = pTex->maxTile;
    if (last < base) {
        last = base;
        pTex->maxTile = base;
    }
    if (last >= BR_TEX3D_TILES) last = BR_TEX3D_TILES - 1;

    pT = &pTex->aTile[base];

    /* 0x10028C9E / 0x10028CF4: the dimensions are 1<<mask, doubled on the
     * mirror bit.  Nothing later changes them -- the G_SETTILESIZE
     * correction below touches only the clamp flags and a side copy. */
    w = (int32_t)1 << (pT->maskS & 31);
    h = (int32_t)1 << (pT->maskT & 31);
    if (pT->mirrorS) w *= 2;
    if (pT->mirrorT) h *= 2;

    rec.w = w;
    rec.h = h;
    rec.clampS = (pT->clampS != 0);
    rec.clampT = (pT->clampT != 0);
    rec.fmtCode = BrTexFormatCode(pT->siz, pT->fmt, pTex->mode);
    rec.texelSrc = pTex->texelSrc;
    rec.palSrc   = pTex->palSrc;
    rec.baseLod  = base;
    rec.cLods    = last - base + 1;
    rec.mode     = pTex->mode;
    rec.tile     = *pT;
    memcpy(rec.aKey, pTex->aKey, sizeof(rec.aKey));

    /* 0x10028DA5: (lr - ul + 4) >> 2, the same formula G_SETTILESIZE's
     * consumer uses, against the two SIDE COPIES of the dimensions at
     * +0x40/+0x44 -- which are the divisors the texel scale later uses, and
     * which the mirror doubling does NOT touch.  A tile SIZE wider than the
     * mask and an exact multiple of it means the texture repeats, so the
     * clamp goes off; anything else leaves the state alone. */
    rec.divW = (int32_t)1 << (pT->maskS & 31);
    rec.divH = (int32_t)1 << (pT->maskT & 31);
    tileW = (pT->lrs - pT->uls + 4) >> 2;
    tileH = (pT->lrt - pT->ult + 4) >> 2;
    if (tileW != rec.divW) {
        if (tileW == w)
            rec.divW = tileW;
        else if (tileW > w && w > 0 && (tileW / w) * w == tileW)
            rec.clampS = 0;
    }
    if (tileH != rec.divH) {
        if (tileH == h)
            rec.divH = tileH;
        else if (tileH > h && h > 0 && (tileH / h) * h == tileH)
            rec.clampT = 0;
    }

    {
        int32_t iFound = br_tex3d_find(pTex, &rec);
        if (iFound >= 0)
            return iFound;
    }
    if (rec.cLods > 1)
        pTex->cMultiLod++;
    return br_tex3d_append(pTex, &rec);
}

/* ==================================================================== */
/* 0x10028820 -- the scan                                               */
/* ==================================================================== */

/* 0x100293D0 == BRD3D 0x10029E60, which slice2_16.c ports as
 * BrGbiTexScanMark. Nineteen bytes, byte-identical in both images.  TWO HOST
 * BODIES REMAIN for this and for its caller below, and the reason is stated
 * rather than left to be found: the two modules disagree about the
 * REPRESENTATION of a display-list command -- this file writes words
 * byte-wise into a possibly-unaligned `uint8_t *` (which is what
 * CONVENTIONS.md requires of a foreign buffer) and slice2_16.c stores through
 * a `BrGfxWords` overlay.  A shared body would have to pick one, and picking
 * the byte-wise one changes slice2_16's stores on a big-endian host while
 * picking the overlay puts an unaligned 32-bit store in this one.
 *
 * What HAS been removed is the disagreement: this file had a NULL guard on
 * the run start that the original does not have and slice2_16.c did not have
 * either.  See the seam below.
 *
 * The run ends at the FIRST command that is not part of the setup grammar;
 * later ones do not move it. */
/* WHAT IT DOES: notes where a run of texture-setup commands stops. Only the
 * first command that ends the run counts; later ones do not move it. */
/* @implements 0x100293D0 glide br_tex3d_end */
static void br_tex3d_end(BrTex3d *pTex, uint8_t *p, uint8_t **ppEnd)
{
    (void)pTex;
    if (*ppEnd == NULL)
        *ppEnd = p;
}

/* 0x10028B50 == BRD3D 0x10029410 == slice2_16.c's BrGbiTexScanFlush.  Ninety-
 * two bytes, byte-identical in both images.  THE SEAM. */
/* WHAT IT DOES: closes off a run of texture-setup commands and replaces it
 * with a single one. It registers the texture the run describes, and if that
 * works, overwrites the run's first command with "use texture number N, and
 * skip this many commands", so the several commands the N64 needed to load a
 * texture become one on the PC. This is the point where an N64 drawing list
 * is rewritten into a PC one. */
/* @implements 0x10028B50 glide br_tex3d_seam */
static void br_tex3d_seam(BrTex3d *pTex, uint8_t *p,
                          uint8_t **ppStart, uint8_t **ppEnd)
{
    int32_t id;

    if (pTex->state == 0)
        return;
    br_tex3d_end(pTex, p, ppEnd);
    pTex->cRuns++;

    id = br_tex3d_register(pTex);
    /* THE `*ppStart != NULL` TEST THAT USED TO BE HERE IS GONE.  The original
     * dereferences the run-start global unguarded (0x10028B7D loads it,
     * 0x10028B8D stores through it, no test between), and slice2_16.c's
     * transcription of the same 92 bytes -- BrGbiTexScanFlush, BRD3D
     * 0x10029410 -- did not have it either. So the port had one function
     * written twice and disagreeing.
     *
     * It was also DEAD: the run start is written on the same state 0 -> 1
     * transition that makes `state` non-zero, and the early return above
     * covers state 0, so it cannot be NULL when this line runs. A guard that
     * cannot fire still costs, because it reads as evidence that it can. */
    if (id >= 0) {
        br_tex3d_putw(*ppStart, 0xDC000000u | ((uint32_t)id & 0x00FFFFFFu));
        /* `sar edx,3` at 0x10028B9B -- an arithmetic SHIFT, not a divide.
         * The two round the same way for the non-negative lengths this can
         * produce, and the shift is what the bytes say. */
        br_tex3d_putw(*ppStart + 4,
                      (uint32_t)((*ppEnd - *ppStart) >> 3));
        pTex->cPlanted++;
    }
    pTex->state = 0;
}

size_t BrTex3dScan(BrTex3d *pTex, uint8_t *pList, size_t cbMax)
{
    size_t off = 0, n = 0;
    uint8_t *pStart = NULL, *pEnd = NULL;

    if (pTex == NULL || pList == NULL)
        return 0;

    /* 0x10028832: five globals cleared on entry.  The tile records are NOT
     * among them -- see br_tex3d.h. */
    pTex->baseTile = 0;
    pTex->maxTile  = 0;
    pTex->mode     = 0;
    pTex->state    = 0;

    while (off + 8 <= cbMax) {
        uint8_t *p = pList + off;
        uint32_t w0 = br_tex3d_w(p), w1 = br_tex3d_w(p + 4);
        unsigned op = (w0 >> 24) & 0xFFu;
        int32_t tile;

        n++;
        /* 0x10028867: `add ecx,-4 / cmp ecx,0xF9 / ja` -- outside
         * 0x04..0xFD the byte table is not even consulted. */
        if (op < 0x04u || op > 0xFDu) {
            br_tex3d_end(pTex, p, &pEnd);
            off += 8;
            continue;
        }

        switch (op) {
        case 0xB8:                                  /* G_ENDDL: stop      */
            return n;

        case 0x04:                                  /* G_VTX              */
        case 0xB1:                                  /* G_TRI2             */
        case 0xBF:                                  /* G_TRI1             */
            br_tex3d_seam(pTex, p, &pStart, &pEnd);
            break;

        case 0xFD:                                  /* 0x10029420 SETTIMG */
            if (pTex->state == 0 || pTex->state == 3 || pTex->state == 6) {
                pTex->imgSiz  = (int32_t)((w0 >> 19) & 3u);
                pTex->imgAddr = w1;
                pTex->palSrc  = 0;
                if (pTex->state == 0) {
                    pStart = p;                     /* 0x10697A64         */
                    pEnd   = NULL;                  /* 0x106B7A9C = 0     */
                }
                pTex->state = 1;
            }
            break;

        case 0xE6:                                  /* 0x100294F0 LOADSYNC */
            if (pTex->state == 1)
                pTex->state = 2;
            break;

        case 0xE7:                                  /* 0x10029570 PIPESYNC */
            /* The original tests `state != 0` and then `state == 7`; the
             * first test cannot change the outcome and is not repeated. */
            if (pTex->state == 7)
                pTex->state = 8;
            break;

        case 0xE8:                                  /* 0x10029590 TILESYNC */
            if (pTex->state == 3 || pTex->state == 7)
                pTex->state = 4;
            break;

        case 0xF3:                                  /* 0x10029510 LOADBLOCK */
            if (pTex->state == 2) {
                pTex->texelSrc = pTex->imgAddr;     /* 0x105D17F0          */
                pTex->state = 3;
            }
            break;

        case 0xF0:                                  /* 0x10029480 LOADTLUT  */
            if (pTex->state == 1) {
                pTex->palSrc = pTex->imgAddr;       /* 0x106B7A98          */
                pTex->state = 7;
            }
            break;

        case 0xF5: {                                /* 0x100295B0 SETTILE   */
            BrTex3dTile *pT;
            tile = (int32_t)((w1 >> 24) & 7u);
            pT = &pTex->aTile[tile];
            pT->fmt     = (int32_t)((w0 >> 21) & 7u);
            pT->siz     = (int32_t)((w0 >> 19) & 3u);
            pT->line    = (int32_t)(((w0 >> 9) & 0x1FFu) << 3);
            pT->tmem    = (int32_t)(w0 & 0x1FFu);
            pT->mirrorS = (int32_t)((w1 >> 8) & 1u);
            pT->clampS  = (int32_t)((w1 >> 9) & 1u);
            pT->mirrorT = (int32_t)((w1 >> 18) & 1u);
            pT->clampT  = (int32_t)((w1 >> 19) & 1u);
            pT->maskS   = (int32_t)((w1 >> 4) & 0xFu);
            pT->maskT   = (int32_t)((w1 >> 14) & 0xFu);
            pT->shiftS  = (int32_t)(w1 & 0xFu);
            pT->shiftT  = (int32_t)((w1 >> 10) & 0xFu);
            /* 0x10029676: a tile issued straight after a load STARTS the
             * chain; otherwise it only ever extends it. */
            if (pTex->state == 3 || pTex->state == 4 || pTex->state == 7)
                pTex->maxTile = tile;
            else if (tile > pTex->maxTile)
                pTex->maxTile = tile;
            pTex->state = 5;
            break;
        }

        case 0xF2: {                                /* 0x100296B0 TILESIZE  */
            BrTex3dTile *pT;
            tile = (int32_t)((w1 >> 24) & 7u);
            pT = &pTex->aTile[tile];
            pT->uls = (int32_t)((w0 >> 12) & 0xFFFu);
            pT->ult = (int32_t)(w0 & 0xFFFu);
            pT->lrs = (int32_t)((w1 >> 12) & 0xFFFu);
            pT->lrt = (int32_t)(w1 & 0xFFFu);
            pTex->state = 6;
            break;
        }

        case 0xFA:                                  /* 0x1002893A SETPRIM   */
            pTex->aKey[4] = (uint8_t)(w1 >> 24);    /* +0x294 (0x10697A68) */
            pTex->aKey[5] = (uint8_t)(w1 >> 16);    /* +0x295 (0x106B7A78) */
            /* +0x29A / +0x29B in the original; outside the dedup's window,
             * which stops at +0x297.  Kept for the record's sake. */
            break;

        case 0xFB:                                  /* 0x10028973 SETENV    */
            pTex->aKey[0] = (uint8_t)(w1 >> 24);    /* +0x290 (0x105E1800) */
            pTex->aKey[1] = (uint8_t)(w1 >> 16);    /* +0x291 (0x1066182C) */
            pTex->aKey[2] = (uint8_t)(w1 >> 8);     /* +0x292 (0x105E17F8) */
            pTex->aKey[3] = (uint8_t)w1;            /* +0x293 (0x105D17E8) */
            break;

        case 0xFC:                                  /* 0x100289AD SETCOMBINE */
            /* ONE pair sets the mode flag; every other pair clears it. */
            pTex->mode = (w0 == 0xFC50FE04u && w1 == 0x3FFDF3F8u) ? 1 : 0;
            break;

        case 0xB9:                                  /* SETOTHERMODE_L        */
            /* 0x10029710 latches an XLU flag the backend already models,
             * then 0x100293D0 closes the run. */
            br_tex3d_end(pTex, p, &pEnd);
            break;

        case 0xBA:                                  /* SETOTHERMODE_H        */
            /* 0x10029780: only the shift-0x11 form is read, and it sets the
             * BASE LOD tile from `w1 == 0x40000`. */
            if ((w0 & 0xFF00u) == 0x1100u)
                pTex->baseTile = (w1 == 0x00040000u) ? 1 : 0;
            break;

        case 0xBB:                                  /* G_TEXTURE             */
            /* 0x100293F0 records two tile indices the registrar does not
             * read.  No state change, and -- note -- no run END either. */
            break;

        default:
            br_tex3d_end(pTex, p, &pEnd);
            break;
        }
        off += 8;
    }
    return n;
}

/* ==================================================================== */
/* 0x100250D0 -- the expander, and 0x100271F0                           */
/* ==================================================================== */

/* 0x100271F0, forty-four bytes: an x86 little-endian halfword load followed
 * by a byte swap, which is a BIG-endian read of the N64 halfword, then a
 * rotate right by one.  Reading the two bytes big-endian here is the same
 * value on every host (CONVENTIONS.md: decode byte-wise, never by overlay). */
/* WHAT IT DOES: reads one 16-bit colour out of N64 texture data. As well as
 * taking the two bytes the N64's way round, it rotates the value by one bit,
 * which moves the transparency bit from the bottom of the N64's layout to
 * the top of the layout the rest of this code uses. */
/* @implements 0x100271F0 glide br_tex3d_texel */
static uint16_t br_tex3d_texel(const uint8_t *p)
{
    uint32_t v = ((uint32_t)p[0] << 8) | (uint32_t)p[1];
    return (uint16_t)((v >> 1) | ((v & 1u) << 15));
}

/* The odd-row swizzle.  0x1002543E walks a row with a 4-forward/8-back
 * cursor -- bytes 4,5,6,7, 0,1,2,3, 12,13,14,15, 8,9,10,11 ... -- which is
 * `i ^ 4`, and the 16-bit arm at 0x10026FBD does the same thing two bytes at
 * a time.  The mask the row index is tested against is descriptor +0x298,
 * which 0x10028EF0 sets to 1 and nothing else writes. */
static size_t br_tex3d_swiz(size_t i, int fOdd)
{
    return fOdd ? (i ^ 4u) : i;
}

static int br_tex3d_bpp(const BrTex3dTile *pT)
{
    if (pT->fmt == 2 && pT->siz == 0) return 4;     /* CI4    */
    if (pT->fmt == 2 && pT->siz == 1) return 8;     /* CI8    */
    if (pT->fmt == 0 && pT->siz == 2) return 16;    /* RGBA16 */
    return 0;
}

size_t BrTex3dSrcBytes(const BrTex3d *pTex, uint32_t id)
{
    const BrTex3dRec *pR;
    int32_t texW, texH;
    int bpp;
    size_t row, pitch;

    if (pTex == NULL || id >= pTex->cRec)
        return 0;
    pR = &pTex->aRec[id];
    bpp = br_tex3d_bpp(&pR->tile);
    if (bpp == 0)
        return 0;
    texW = (int32_t)1 << (pR->tile.maskS & 31);
    texH = (int32_t)1 << (pR->tile.maskT & 31);
    if (texW <= 0 || texH <= 0)
        return 0;
    row = ((size_t)texW * (size_t)bpp) / 8u;
    pitch = (pR->tile.line > 0) ? (size_t)pR->tile.line : row;
    return (size_t)pR->tile.tmem * 8u + pitch * (size_t)(texH - 1) + row;
}

/* 0x10027850, the two floats at record +0x2AC/+0x2B0, divided by Glide's
 * 256-unit texture-coordinate space so the result multiplies a raw N64 Vtx
 * texture coordinate straight into [0,1].  See br_tex3d.h. */
/* WHAT IT DOES: turns a texture-coordinate shift code into the multiplier it
 * stands for -- a halving for each of the first ten codes, and a doubling
 * for each step of the remainder. This is how a texture ends up sampled at
 * the right scale. */
/* @implements 0x10027850 glide br_tex3d_shift */
static float br_tex3d_shift(int32_t shift)
{
    float f = 1.0f;
    int32_t i;
    if (shift == 0)
        return 1.0f;
    if (shift <= 10) {
        /* (0x400 >> shift) * 0.0009765625 == 2^-shift */
        for (i = 0; i < shift; i++)
            f *= 0.5f;
    } else {
        for (i = 0; i < 16 - shift; i++)
            f *= 2.0f;
    }
    return f;
}

void BrTex3dTexScaleNorm(const BrTex3d *pTex, uint32_t id,
                         float *pScaleS, float *pScaleT)
{
    const BrTex3dRec *pR;
    float s = 1.0f, t = 1.0f;

    if (pTex != NULL && id < pTex->cRec) {
        pR = &pTex->aRec[id];
        /* (1/32) / div * 256, then /256 for normalised sampling. */
        if (pR->divW > 0)
            s = (1.0f / 32.0f) / (float)pR->divW * br_tex3d_shift(pR->tile.shiftS);
        if (pR->divH > 0)
            t = (1.0f / 32.0f) / (float)pR->divH * br_tex3d_shift(pR->tile.shiftT);
    }
    if (pScaleS != NULL) *pScaleS = s;
    if (pScaleT != NULL) *pScaleT = t;
}

int BrTex3dDecode(const BrTex3d *pTex, uint32_t id,
                  const uint8_t *pTexels, size_t cbTexels,
                  const uint8_t *pPal, size_t cbPal,
                  uint16_t *pOut)
{
    const BrTex3dRec *pR;
    const BrTex3dTile *pT;
    const uint8_t *pRow;
    int32_t texW, texH, x, y;
    int bpp;
    size_t row, pitch, need;
    uint16_t *o = pOut;

    if (pTex == NULL || pOut == NULL || id >= pTex->cRec)
        return BR_TEX3D_BADID;
    pR = &pTex->aRec[id];
    pT = &pR->tile;

    bpp = br_tex3d_bpp(pT);
    if (bpp == 0)
        return BR_TEX3D_UNSUPPORTED;

    texW = (int32_t)1 << (pT->maskS & 31);
    texH = (int32_t)1 << (pT->maskT & 31);
    if (texW <= 0 || texH <= 0 || pR->w <= 0 || pR->h <= 0)
        return BR_TEX3D_DEGENERATE;

    /* THE ROW PITCH IS `line`, NOT THE TEXEL WIDTH, and it is allowed to be
     * SMALLER: two of testdata/ce.rca's textures have maskS = 7 (128 texels)
     * and line = 40 bytes, i.e. 80 texels.  That is the ordinary N64 idiom
     * for a non-power-of-two image -- the mask is rounded up so that
     * wrapping works and the geometry simply never addresses the columns
     * past 80, which on the RDP read the NEXT row.  0x100256BC adds `line`
     * once per row and the inner loop still reads a full mask-width row, so
     * the rows overlap; clamping the pitch up to the width instead would
     * produce a different (and prettier) image than the original's, which
     * is not what a transcription is for. */
    row   = ((size_t)texW * (size_t)bpp) / 8u;
    pitch = (pT->line > 0) ? (size_t)pT->line : row;
    need = (size_t)pT->tmem * 8u + pitch * (size_t)(texH - 1) + row;

    if (pTexels == NULL || cbTexels < need)
        return BR_TEX3D_NOSRC;
    if (pT->fmt == 2 && (pPal == NULL || cbPal < ((pT->siz == 0) ? 32u : 512u)))
        return BR_TEX3D_NOSRC;

    /* 0x1002511F: the cursor starts at the texel source plus the tile's
     * TMEM offset, which is in 64-bit words. */
    pRow = pTexels + (size_t)pT->tmem * 8u;

    for (y = 0; y < texH; y++) {
        int fOdd = (y & 1) != 0;
        uint16_t *pRowOut = o;

        for (x = 0; x < texW; x++) {
            uint16_t v;
            if (bpp == 4) {
                size_t i = (size_t)(x >> 1);
                uint8_t b = pRow[br_tex3d_swiz(i, fOdd)];
                /* 0x10025467: the HIGH nibble first. */
                unsigned idx = (x & 1) ? (b & 0x0Fu) : (unsigned)(b >> 4);
                v = br_tex3d_texel(pPal + idx * 2u);
            } else if (bpp == 8) {
                uint8_t b = pRow[br_tex3d_swiz((size_t)x, fOdd)];
                v = br_tex3d_texel(pPal + (size_t)b * 2u);
            } else {
                size_t i = (size_t)x * 2u;
                v = br_tex3d_texel(pRow + br_tex3d_swiz(i, fOdd));
            }
            *o++ = v;
        }
        /* 0x10025680: the mirror emits the row it has just written,
         * backwards, doubling the width. */
        if (pT->mirrorS) {
            for (x = 0; x < texW; x++)
                *o++ = pRowOut[texW - 1 - x];
        }
        pRow += pitch;                              /* 0x100256BC */
    }

    /* 0x100256E8: and the T mirror does the same to whole rows. */
    if (pT->mirrorT) {
        int32_t stride = pR->w;
        for (y = 0; y < texH; y++) {
            const uint16_t *pSrc = pOut + (size_t)(texH - 1 - y) * (size_t)stride;
            for (x = 0; x < stride; x++)
                *o++ = pSrc[x];
        }
    }
    return BR_TEX3D_OK;
}

void BrTex3dToRgba8(const uint16_t *pArgb1555, uint32_t count, uint8_t *pRgba)
{
    uint32_t i;
    if (pArgb1555 == NULL || pRgba == NULL)
        return;
    for (i = 0; i < count; i++) {
        uint16_t v = pArgb1555[i];
        unsigned r = (v >> 10) & 0x1Fu;
        unsigned g = (v >> 5) & 0x1Fu;
        unsigned b = v & 0x1Fu;
        /* 5 -> 8 bits by bit replication, so 0x1F maps to 0xFF exactly. */
        pRgba[i * 4 + 0] = (uint8_t)((r << 3) | (r >> 2));
        pRgba[i * 4 + 1] = (uint8_t)((g << 3) | (g >> 2));
        pRgba[i * 4 + 2] = (uint8_t)((b << 3) | (b >> 2));
        pRgba[i * 4 + 3] = (uint8_t)((v & 0x8000u) ? 255u : 0u);
    }
}
