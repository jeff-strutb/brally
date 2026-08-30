/* br_tex3d.c -- see br_tex3d.h.  The load-time texture pass, transcribed
 * from BRGlide.dll 0x10028820 and the eleven routines it reaches. */

#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
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
 * which 0x10028EF0 sets to 1 and nothing else writes.
 *
 * The cursor form and `i ^ 4` agree for EVERY row width, not just multiples
 * of eight, which is why this one line can stand in for the four-way
 * unrolled loop.  The unrolled form is
 *
 *     L1: p += 4; run four (or until i == w); p -= 8;
 *         run four (or until i == w);        p += 4; repeat
 *
 * so a run that stops short leaves the cursor wherever it stopped, and the
 * NEXT group is entered only when i < w -- which re-establishes
 * p == pRow + (i & ~7).  Worked at w = 3, 6 and 10 against the listing. */
static size_t br_tex3d_swiz(size_t i, int fOdd)
{
    return fOdd ? (i ^ 4u) : i;
}

/* ==================================================================== */
/* 0x100250D0 -- the expander                                           */
/* ==================================================================== */

/* WHICH BUILD THIS IS, BECAUSE THE SLOT HOLDS TWO DIFFERENT FUNCTIONS.
 *
 * config/shared.csv classes 0x100250D0 (Glide) / 0x10025AB0 (D3D) as
 * `renderer`, matched by SLOT: crossdiff paired them by their call sites,
 * not by their bodies.  The bodies really do differ, and the difference is
 * the destination pixel format, measured rather than assumed:
 *
 *   - the Glide body stores 47 sixteen-bit words to the output cursor and
 *     no dwords; the D3D body stores 67 DWORDS and no words.
 *   - the per-texel helper each calls is a different function.  Glide's
 *     0x100271F0 is 44 bytes and returns ARGB1555 (br_tex3d_texel above);
 *     D3D's 0x10027B10 is 116 bytes and expands the same RGBA5551 source
 *     into ARGB8888 with each 5-bit channel replicated as (c<<3)|(c&7) and
 *     alpha as 0x00 or 0xFF.
 *
 * The two are otherwise the same shape -- 22 cdecl arguments, 14 calls to
 * their respective helper, 8480 against 8288 bytes -- so this is one
 * algorithm compiled against two backends, not two algorithms.  THIS FILE
 * TRANSCRIBES THE GLIDE BODY.  Do not fold the D3D one in.
 *
 * WHAT THE ARGUMENTS ARE, read off the real caller 0x10027B60, which passes
 * the 0x2B4-byte texture descriptor field by field:
 *
 *   1  pOut     0x1186C988, the staging buffer      2  cbOut    desc +0x29C
 *   3  siz      tile[base].siz                      4  pTexels  desc +0x48
 *   5  pPal     desc +0x4C                          6  fmt      tile[base].fmt
 *   7  fMirrorS desc +0x50                          8  fMirrorT desc +0x54
 *   9  lod      desc +0x58                         10  lodEnd   desc +0x5C
 *  11  aTile    desc +0x60 (eight 0x40-byte tiles) 12  flags    desc +0x260
 *  13  mode     desc +0x264                    14..17  desc +0x290..+0x293
 *  18..21       desc +0x294..+0x297            22  maskOdd  desc +0x298
 *
 * -- so arguments 14..21 are the dedup's eight state bytes: the G_SETENVCOLOR
 * RGBA (0xFB, +0x290..+0x293) as the HIGH colour and the G_SETPRIMCOLOR pair
 * (0xFA, +0x294/+0x295) plus TWO BYTES NOTHING EVER WRITES (+0x296/+0x297)
 * as the LOW colour.  br_tex3d_find above already records that defect from
 * the other side; here is what it costs -- the blue and alpha of the low
 * colour in the two blend arms are uninitialised stack in the original.  The
 * port zeroes them, as it does in the dedup.
 *
 * The other caller, 0x10024E60, builds a one-tile descriptor on its own
 * stack and zeroes all eight bytes explicitly. */

typedef struct BrTexExpSink {
    uint8_t *p;         /* esi */
    int32_t  cb;        /* edi -- output bytes so far */
    int32_t  cbMax;     /* ebp -- argument 2          */
} BrTexExpSink;

/* Every emit in the function has the same three steps in the same order:
 * store, advance by the element size, then `cmp edi,ebp / jge` straight out
 * of the whole function.  The budget test is AFTER the store, so the first
 * element is written even when cbOut is zero -- preserved.  Returns 1 when
 * the caller must unwind. */
static int br_te_put(BrTexExpSink *pS, uint32_t v, int elem)
{
    if (elem == 1) {
        pS->p[0] = (uint8_t)v;
    } else {
        uint16_t h = (uint16_t)v;
        memcpy(pS->p, &h, sizeof h);
    }
    pS->p  += elem;
    pS->cb += elem;
    return pS->cb >= pS->cbMax;
}

static uint32_t br_te_get(const uint8_t *p, int elem)
{
    uint16_t h;
    if (elem == 1)
        return (uint32_t)p[0];
    memcpy(&h, p, sizeof h);
    return (uint32_t)h;
}

/* The arms, named for the (siz, fmt, mode) triple that selects them. */
enum {
    BR_TE_CI4,        /* siz 0 fmt 2            palette   -> ARGB1555 */
    BR_TE_IDX4,       /* siz 0 fmt 2, flags&2   raw index -> u16      */
    BR_TE_I4BLEND,    /* siz 0 fmt 4 mode 1     blend     -> ARGB1555 */
    BR_TE_I4,         /* siz 0 fmt 4            i*17      -> u8       */
    BR_TE_CI8,        /* siz 1 fmt 2            palette   -> ARGB1555 */
    BR_TE_IA8BLEND,   /* siz 1 fmt 3 mode 1     blend     -> ARGB4444 */
    BR_TE_AI44,       /* siz 1 fmt 3            swap      -> u8       */
    BR_TE_I8,         /* siz 1 fmt 4            copy      -> u8       */
    BR_TE_RGBA16,     /* siz 2 fmt 0            0x100271F0-> ARGB1555 */
    BR_TE_NONE
};

/* 0x10025809.  Four channels, each `lo + i*(hi-lo)/255` at 32 bits, each
 * then SPILLED TO A BYTE -- the three colour channels shifted right by 3 and
 * the fourth by 7, which is what makes the fourth exactly one bit.  The
 * division is the SIGNED magic sequence (imul 0x80808081, sar 7, add sign),
 * i.e. C's `/ 255` truncating toward zero.
 *
 * The 20 bits assembled here are stored through a 16-bit `mov`, so the top
 * four are discarded and the result is ARGB1555 with argument 14/18 as red. */
static uint16_t br_te_blend1555(int32_t i, const int32_t *aHi,
                                const int32_t *aLo)
{
    int32_t ch[4];
    uint32_t v;
    int k;

    for (k = 0; k < 4; k++) {
        int32_t lo = aLo[k] & 0xFF;
        int32_t d  = (aHi[k] & 0xFF) - lo;
        ch[k] = lo + (i * d) / 255;
    }
    v = (uint32_t)((ch[3] >> 7) & 0xFF);
    v = (v << 5) | (uint32_t)((ch[0] >> 3) & 0xFF);
    v = (v << 5) | (uint32_t)((ch[1] >> 3) & 0xFF);
    v = (v << 5) | (uint32_t)((ch[2] >> 3) & 0xFF);
    return (uint16_t)v;
}

/* 0x10026705.  THREE channels, not four: the alpha of an IA8 texel is the
 * source's own low nibble and no colour is blended into it.
 *
 * DIFFERENT ARITHMETIC FROM THE ARM ABOVE, and this is not tidiness to be
 * harmonised away: 0x10026747 is `mul` + `shr edx,7` -- the UNSIGNED magic
 * divide -- where 0x1002585B is `imul` + `sar` + sign fixup.  So a high
 * colour darker than the low one wraps here and clamps toward zero there.
 * Nothing masks the shifted channels either; the four nibbles are assembled
 * in a register and truncated only by the 16-bit store. */
static uint16_t br_te_blend4444(uint32_t b, const int32_t *aHi,
                                const int32_t *aLo)
{
    uint32_t i = (b >> 4) | (b & 0xF0u);        /* 0x10026720 */
    uint32_t v = b & 0x0Fu;                     /* 0x10026728 */
    int k;

    for (k = 0; k < 3; k++) {
        uint32_t lo   = (uint32_t)aLo[k] & 0xFFu;
        uint32_t prod = (uint32_t)((int32_t)(((uint32_t)aHi[k] & 0xFFu) - lo)
                                   * (int32_t)i);
        v = (v << 4) | ((lo + prod / 255u) >> 4);
    }
    return (uint16_t)v;
}

/* One SOURCE unit -- one byte for the 4bpp and 8bpp arms, one halfword for
 * RGBA16 -- turned into one or two output elements. */
static int br_te_unit(BrTexExpSink *pS, int kind, const uint8_t *pu,
                      const uint8_t *pPal, const int32_t *aHi,
                      const int32_t *aLo)
{
    uint32_t b = pu[0];

    switch (kind) {
    case BR_TE_CI4:
        /* 0x10025467: the HIGH nibble first, both times. */
        if (br_te_put(pS, br_tex3d_texel(pPal + (b >> 4) * 2u), 2)) return 1;
        return br_te_put(pS, br_tex3d_texel(pPal + (b & 15u) * 2u), 2);
    case BR_TE_IDX4:
        /* 0x100251E3: `movzx dx,dl` of the bare nibble -- no palette. */
        if (br_te_put(pS, b >> 4, 2)) return 1;
        return br_te_put(pS, b & 15u, 2);
    case BR_TE_I4BLEND:
        if (br_te_put(pS, br_te_blend1555((int32_t)((b >> 4) | (b & 0xF0u)),
                                          aHi, aLo), 2)) return 1;
        return br_te_put(pS, br_te_blend1555(
            (int32_t)(((b << 4) | (b & 15u)) & 0xFFu), aHi, aLo), 2);
    case BR_TE_I4:
        if (br_te_put(pS, (b & 0xF0u) | (b >> 4), 1)) return 1;
        return br_te_put(pS, ((b << 4) | (b & 15u)) & 0xFFu, 1);
    case BR_TE_CI8:
        return br_te_put(pS, br_tex3d_texel(pPal + b * 2u), 2);
    case BR_TE_IA8BLEND:
        return br_te_put(pS, br_te_blend4444(b, aHi, aLo), 2);
    case BR_TE_AI44:
        /* 0x10026C04: the nibbles swap.  This is the whole of the IA8 ->
         * AI44 difference CONVENTIONS.md records for the Glide font. */
        return br_te_put(pS, ((b << 4) | (b >> 4)) & 0xFFu, 1);
    case BR_TE_I8:
        return br_te_put(pS, b, 1);
    default:
        return br_te_put(pS, br_tex3d_texel(pu), 2);
    }
}

/* WHAT IT DOES: turns the N64's packed texture data into the pixels the
 * Glide card can hold. It walks each level of detail a texture has, reads
 * one row at a time, undoes the console's habit of swapping every other
 * row's halves, and writes out one colour per texel -- looking colours up in
 * a palette, mixing between two colours by brightness, or passing the
 * colour straight through, whichever the texture's format calls for. It also
 * duplicates the picture left-right and top-bottom when the texture is
 * marked as mirrored, and it stops the moment the destination buffer is
 * full. */
/* Port reconstruction (not byte-exact). Matching twin is BrTex3dExpand at
 * 0x100250D0, transcribed separately. */
#ifndef BR_MATCHING_BUILD
void BrTex3dExpand(uint8_t *pOut, int32_t cbOut, int32_t siz,
                   const uint8_t *pTexels, const uint8_t *pPal, int32_t fmt,
                   int32_t fMirrorS, int32_t fMirrorT,
                   int32_t lod, int32_t lodEnd,
                   const BrTex3dTile *aTile, int32_t flags, int32_t mode,
                   int32_t hi0, int32_t hi1, int32_t hi2, int32_t hi3,
                   int32_t lo0, int32_t lo1, int32_t lo2, int32_t lo3,
                   int32_t maskOdd)
{
    BrTexExpSink s;
    int32_t aHi[4], aLo[4];

    aHi[0] = hi0; aHi[1] = hi1; aHi[2] = hi2; aHi[3] = hi3;
    aLo[0] = lo0; aLo[1] = lo1; aLo[2] = lo2; aLo[3] = lo3;

    s.p = pOut; s.cb = 0; s.cbMax = cbOut;

    /* 0x100250F5.  The guard is on the FIRST iteration's condition, so an
     * empty LOD range leaves the buffer untouched. */
    for (; lod < lodEnd; lod++) {
        /* 0x1002510D: the tile array is indexed by the ABSOLUTE tile number
         * and the cursor restarts from the texel base every level. */
        const BrTex3dTile *pT = &aTile[lod];
        const uint8_t *pRow = pTexels + (size_t)pT->tmem * 8;
        int32_t w, h, y, rowElems;
        int kind, elem, nPer, srcUnit;

        if (siz == 0) {
            if (fmt == 2) {
                /* 0x10025148.  Both halves of the test matter: the raw-index
                 * arm needs bit 1 of the descriptor's +0x260 flags AND the
                 * level to be exactly 1.  It reads its width, height and
                 * pitch from tile[1] by a hard-coded displacement rather
                 * than from tile[lod] -- the same tile, because of the
                 * guard, so the two spellings cannot disagree. */
                kind = ((flags & 2) != 0 && lod == 1) ? BR_TE_IDX4 : BR_TE_CI4;
            } else if (fmt == 4) {
                kind = (mode == 1) ? BR_TE_I4BLEND : BR_TE_I4;
            } else {
                continue;
            }
            /* 0x10025169: maskS MINUS ONE, because a 4bpp row is half as
             * many bytes as it is texels, and the loops below count BYTES. */
            w = (int32_t)((uint32_t)1 << ((uint32_t)(pT->maskS - 1) & 31u));
        } else if (siz == 1) {
            if (fmt == 2)      kind = BR_TE_CI8;
            else if (fmt == 3) kind = (mode == 1) ? BR_TE_IA8BLEND : BR_TE_AI44;
            else if (fmt == 4) kind = BR_TE_I8;
            else               continue;
            w = (int32_t)((uint32_t)1 << ((uint32_t)pT->maskS & 31u));
        } else if (siz == 2 && fmt == 0) {
            kind = BR_TE_RGBA16;
            w = (int32_t)((uint32_t)1 << ((uint32_t)pT->maskS & 31u));
        } else {
            continue;
        }
        h = (int32_t)((uint32_t)1 << ((uint32_t)pT->maskT & 31u));

        elem    = (kind == BR_TE_I4 || kind == BR_TE_AI44 ||
                   kind == BR_TE_I8) ? 1 : 2;
        nPer    = (kind == BR_TE_CI4 || kind == BR_TE_IDX4 ||
                   kind == BR_TE_I4BLEND || kind == BR_TE_I4) ? 2 : 1;
        srcUnit = (kind == BR_TE_RGBA16) ? 2 : 1;

        for (y = 0; y < h; y++) {
            int fOdd = (y & maskOdd) != 0;
            int32_t i;

            for (i = 0; i < w; i++) {
                size_t off = br_tex3d_swiz((size_t)i * (size_t)srcUnit, fOdd);
                if (br_te_unit(&s, kind, pRow + off, pPal, aHi, aLo)) {
                    /* 0x10026FEE.  The RGBA16 SWIZZLED loop -- and only that
                     * one -- tests the row bound BEFORE the budget, so the
                     * last texel of a row does not abandon the row's mirror
                     * tail.  Every other arm tests the budget first. */
                    if (!(kind == BR_TE_RGBA16 && fOdd && i + 1 >= w))
                        return;
                }
            }

            /* 0x10025680: the S mirror re-emits the row just written,
             * backwards, doubling its width. */
            if (fMirrorS) {
                const uint8_t *q = s.p - elem;
                int32_t k, n = w * nPer;
                for (k = 0; k < n; k++) {
                    uint32_t v = br_te_get(q, elem);
                    q -= elem;
                    if (br_te_put(&s, v, elem))
                        return;
                }
            }

            /* 0x100256BC: the source pitch is the tile's `line`, added once
             * per row and NOT clamped up to the row width -- a `line`
             * narrower than the mask makes consecutive rows overlap, which
             * is the N64 idiom for a non-power-of-two image. */
            pRow += (size_t)pT->line;
        }

        /* 0x100256E8: and the T mirror does the same to whole rows, walking
         * the finished image backwards a row at a time. */
        if (fMirrorT && h > 0) {
            uint8_t *q = s.p;
            rowElems = (fMirrorS ? 2 : 1) * w * nPer;
            for (y = 0; y < h; y++) {
                const uint8_t *p;
                int32_t k;
                q -= (size_t)rowElems * (size_t)elem;
                p = q;
                for (k = 0; k < rowElems; k++) {
                    uint32_t v = br_te_get(p, elem);
                    p += elem;
                    if (br_te_put(&s, v, elem))
                        return;
                }
            }
        }
    }
}
#endif /* BR_MATCHING_BUILD */

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
    int32_t texW, texH;
    int bpp;
    size_t row, pitch, need;

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

    /* THE PIXEL LOOPS ARE NOT REPEATED HERE.  They used to be, and that made
     * the tree hold two host models of one original function -- exactly the
     * hazard CONVENTIONS.md's "aliased storage" section describes, one level
     * up.  BrTex3dExpand above IS 0x100250D0; this routine is the validated
     * wrapper the port's own callers want, and nothing else.
     *
     * DEVIATION, and it is this wrapper's rather than the original's: the
     * expander adds `line` to the row cursor unconditionally, so a tile with
     * line == 0 reads the same source row every time.  A record built from a
     * real G_SETTILE never has that, but a hand-built fixture can, so the
     * fallback pitch computed above is handed over in a tile COPY.  The
     * expander itself is untouched. */
    {
        BrTex3dTile t = *pT;
        int32_t outW = texW * (pT->mirrorS ? 2 : 1);
        int32_t outH = texH * (pT->mirrorT ? 2 : 1);
        int32_t siz  = (bpp == 4) ? 0 : (bpp == 8) ? 1 : 2;
        int32_t fmt  = (bpp == 16) ? 0 : 2;

        t.line = (int32_t)pitch;
        BrTex3dExpand((uint8_t *)pOut, outW * outH * 2, siz,
                      pTexels, pPal, fmt,
                      pT->mirrorS, pT->mirrorT,
                      0, 1, &t,
                      /* flags */ 0, /* mode */ 0,
                      0, 0, 0, 0, 0, 0, 0, 0,
                      /* maskOdd */ 1);
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

/* ── Ghidra-matched functions ─────────────────────────── */
#ifdef BR_MATCHING_BUILD
extern int DAT_105ccbd0;
extern int _DAT_106b7aa4;
extern int _DAT_106b7aa8;
extern int DAT_118ed1a0;
extern int DAT_118ed1b4;
extern int DAT_1186c988;
extern char s_Out_of_tex_mem__100a9e5c[];
int FUN_10027290();
int FUN_100242e0();
int FUN_10027220();
int FUN_10024df0();
int FUN_100275c0();
int FUN_10027710();
int FUN_10023d70();
int FUN_10028200(int, unsigned char, int, int, int, int, int, int, int, int, int, int, int, int, int);
int FUN_100283c0();
int FUN_10027850();
void FUN_1006ff50(char *);
typedef struct BrTexReq272 {
    unsigned int fTmu2;         /* 0x000 */
    unsigned char lod;          /* 0x004 */
    int w;                      /* 0x008 */
    int h;                      /* 0x00C */
    int fmt;                    /* 0x010 */
    int f14;                    /* 0x014 */
    int aspect0;                /* 0x018 */
    int aspect1;                /* 0x01C */
    int f20;                    /* 0x020 */
    unsigned int fClampS;       /* 0x024 */
    unsigned int fClampT;       /* 0x028 */
    int f2c;                    /* 0x02C */
    int f30;                    /* 0x030 */
    int f34;                    /* 0x034 */
    int f38;                    /* 0x038 */
    int cbTotal;                /* 0x03C */
    int wPow;                   /* 0x040 */
    int hPow;                   /* 0x044 */
    int p1;                     /* 0x048 */
    int p2;                     /* 0x04C */
    int p8;                     /* 0x050 */
    int p9;                     /* 0x054 */
    int iLevel;                 /* 0x058 */
    int f5c;                    /* 0x05C */
    int lv[8][16];              /* 0x060..0x25F */
    int f260;                   /* 0x260 */
    int f264;                   /* 0x264 */
    int f268;                   /* 0x268 */
    int f26c;                   /* 0x26C */
    int f270;                   /* 0x270 */
    int f274;                   /* 0x274 */
    int f278;                   /* 0x278 */
    char pad27c[0x14];          /* 0x27C */
    char b290, b291, b292, b293;/* 0x290 */
    char b294, b295, b296, b297;/* 0x294 */
    int f298;                   /* 0x298 */
    int cb29c;                  /* 0x29C */
    int w2a0;                   /* 0x2A0 */
    int h2a4;                   /* 0x2A4 */
} BrTexReq272;
extern int _DAT_10697a48;
extern int _DAT_10697a50;
extern int DAT_106b7aa0;
extern int DAT_10697a4c;
#ifndef BR_FUNCPTR_DEFINED
#define BR_FUNCPTR_DEFINED
typedef int (*funcptr)();
#endif
extern funcptr DAT_118ed1d0;
int FUN_10027b60();
void BrTex3dDownloadAt();
int FUN_10027a70(BrTexReq272 *);
int FUN_10029290();
int FUN_10030fd0();
int FUN_1005a070();
int FUN_10030710();
int FUN_100306d0();
int FUN_10001000();
int __stdcall grTexCalcMemRequired(int,int,int,int);
extern int DAT_106b7ab0;
extern int DAT_106b7a94;
extern int DAT_100b8498;
extern int DAT_106b7aac;
extern int DAT_105d17f0;
extern int DAT_106b7a98;
extern unsigned char DAT_105e17f8;
extern unsigned char DAT_105e1800;
extern unsigned char DAT_1066182c;
extern unsigned char DAT_106b7a78;
extern unsigned char DAT_105d17e8;
extern unsigned char DAT_10697a68;
extern unsigned char DAT_10697a40;
extern unsigned char DAT_10661828;
extern int DAT_1186c968[];
/* The 0x40-byte tile record array at 0x10697840 (see BrTex3dTile in
 * br_tex3d.h); the registrar indexes it with a 0x40 stride. */
typedef struct BrTexTile40 {
    int fmt, siz, line, tmem;
    int mirrorS, clampS, mirrorT, clampT;
    int maskS, maskT, shiftS, shiftT;
    int uls, ult, lrs, lrt;
} BrTexTile40;
extern BrTexTile40 DAT_10697840[];

/* WHAT IT DOES: extract two 3-bit tile indices from a packed command word. */
/* @implements 0x100293F0 glide BrTexTileUnpack */

int BrTexTileUnpack(unsigned int *param_1)

{
  _DAT_10697a50 = *param_1 >> 8 & 7;
  _DAT_10697a48 = *param_1 >> 0xb & 7;
  return;
}

/* WHAT IT DOES: copy the first two words of texture record `src` into record `dst`
 * (the 0x2B4-stride array at 0x106B7AA0). Installed in hook slot 0x118ED1BC. */
/* @implements 0x10023D20 glide BrTex3dRecCopyHead */

void BrTex3dRecCopyHead(int param_1,int param_2)

{
  *(int *)(DAT_106b7aa0 + param_1 * 0x2b4) = *(int *)(DAT_106b7aa0 + param_2 * 0x2b4);
  *(int *)(DAT_106b7aa0 + 4 + param_1 * 0x2b4) =
       *(int *)(DAT_106b7aa0 + 4 + param_2 * 0x2b4);
  return;
}

/* WHAT IT DOES: store a word at +0x278 of texture record `idx`. */
/* @implements 0x10024E30 glide BrTex3dRecSet278 */

void BrTex3dRecSet278(int param_1,int param_2)

{
  *(int *)(DAT_106b7aa0 + 0x278 + param_1 * 0x2b4) = param_2;
  return;
}

int FUN_10024490();

/* WHAT IT DOES: download a full mip chain: fix the record's minor aspect
 * field (+0x40/+0x44) from the aspect ratio, then per level from the global
 * start LOD copy/convert (0x10024490) and advance both cursors by that
 * level's byte size; returns the source bytes consumed. */
/* @implements 0x10027E10 glide BrTex3dMipChainLoad */

int BrTex3dMipChainLoad(int param_1,int param_2,int param_3)

{
  int iVar1;
  int iVar2;
  int iVar3;
  int iVar4;
  int iVar5;
  int iVar6;
  int local_c;
  int local_8;

  local_c = 0;
  iVar1 = FUN_10024df0(*(int *)(param_3 + 0x10));
  iVar5 = *(int *)(param_3 + 0x2a0);
  iVar6 = *(int *)(param_3 + 0x2a4);
  iVar3 = *(int *)(param_3 + 8);
  iVar4 = *(int *)(param_3 + 0xc);
  /* RESIDUE (4B): the original's imul copies the DIMENSION into eax
   * (mov eax,ebx / mov eax,ebp); every probed spelling copies the aspect --
   * commutative-mult canonicalization, structure exact. */
  if (iVar5 >= iVar6) {
    *(int *)(param_3 + 0x44) = (iVar3 * iVar6) / iVar4;
  }
  else {
    *(int *)(param_3 + 0x40) = (iVar4 * iVar5) / iVar3;
  }
  local_8 = DAT_106b7ab0;
  if (DAT_106b7ab0 < *(int *)(param_3 + 0x5c)) {
    do {
      FUN_10024490(param_1,iVar3,iVar4,param_2,iVar5,iVar6,*(int *)(param_3 + 0x10));
      iVar2 = iVar4 * iVar3 * iVar1;
      local_c = local_c + iVar2;
      param_1 = param_1 + iVar2;
      param_2 = param_2 + iVar6 * iVar5 * iVar1;
      iVar3 = iVar3 >> 1;
      iVar4 = iVar4 >> 1;
      iVar5 = iVar5 >> 1;
      iVar6 = iVar6 >> 1;
      local_8 = local_8 + 1;
    } while (local_8 < *(int *)(param_3 + 0x5c));
  }
  return local_c;
}

void __stdcall grTexDownloadMipMap(int tmu, int startAddress, int evenOdd,
                                   void *pInfo);
extern int DAT_105d17ec;
extern int DAT_10661844;
extern char DAT_10661854;
extern char DAT_10661884;
extern char DAT_10661888;
extern char DAT_1066188c;
extern char DAT_10661904;
extern char DAT_10661914;

/* WHAT IT DOES: for a live record in the 0xD8-stride texture table at
 * 0x10661844, store the new start address (+0xD0 and +0x10) and hand the
 * record's GrTexInfo (+0xC0) back to grTexDownloadMipMap.  Callers push a
 * third argument the function never reads.
 * RESIDUE (4B): the original keeps the [+0x48] arg load BELOW the two
 * address stores; every probed spelling (char-offset, int-indexed, chained
 * assignment) hoists it one slot -- scheduling residue, structure exact. */
/* @implements 0x100283C0 glide BrTex3dDownloadAt */

void BrTex3dDownloadAt(unsigned int param_1,int param_2)

{
  int iVar1;

  if ((param_1 < (unsigned int)DAT_105d17ec) &&
     (iVar1 = param_1 * 0xd8, (&DAT_10661844)[param_1 * 0x36] != 0)) {
    *(int *)(&DAT_10661914 + iVar1) = param_2;
    *(int *)(&DAT_10661854 + iVar1) = param_2;
    grTexDownloadMipMap(*(int *)(&DAT_10661884 + iVar1),
                        *(int *)(&DAT_1066188c + iVar1),
                        *(int *)(&DAT_10661888 + iVar1),&DAT_10661904 + iVar1);
  }
  return;
}

/* WHAT IT DOES: re-download texture `id` from a new texel address, unless it is the
 * currently bound record. What hook slot 0x118ED1D0 holds (opcode 0xDD). */
/* @implements 0x100285E0 glide BrTex3dReDownload */

void BrTex3dReDownload(int param_1,int param_2)

{
  if (param_1 != DAT_10697a4c) {
    BrTex3dDownloadAt(*(int *)(DAT_106b7aa0 + param_1 * 0x2b4),param_2,0);
  }
  return;
}

/* WHAT IT DOES: reconvert texture `id`'s texels and re-download through [0x118ED1D0].
 * Installed in hook slot 0x118ED1D8. */
/* @implements 0x100287E0 glide BrTex3dReconvert */

void BrTex3dReconvert(int param_1)

{
  int uVar1;
  
  uVar1 = FUN_10027b60(DAT_106b7aa0 + 4 + param_1 * 0x2b4);
  (*DAT_118ed1d0)(param_1,uVar1);
  return;
}

/* WHAT IT DOES: make the Glide texture from a filled BrTexReq272 -- skip
 * dedup when DAT_118ed1b4 is set, else reuse 0x10027A70's hit; BMP-substitute
 * through 0x10023D70; allocate a TMEM slot via 0x10028200 (retry TMU0 if the
 * first TMU misses); download; then the texel-scale helper. Out-of-memory
 * fills a dummy 32-byte block with 0x800F800F and returns the bound id.
 * lod at +4 is unsigned char (the CONCAT in the Ghidra dump is leftover
 * high bits of fmt in eax after `mov al,[esi+4]`). */
/* @implements 0x10027710 glide FUN_10027710 */

int FUN_10027710(int *param_1,int *param_2)

{
  int *piVar1;
  int iVar2;
  int *puVar3;
  struct {
  char local_24 [4];
  int local_20 [8];
  } _fr;

  piVar1 = param_1;
  if ((DAT_118ed1b4 != 0) || (iVar2 = FUN_10027a70(param_1), iVar2 == -1)) {
    iVar2 = FUN_10023d70(_fr.local_24,&param_1,piVar1);
    puVar3 = &DAT_1186c988;
    if (iVar2 == 0) {
      puVar3 = param_2;
    }
    iVar2 = FUN_10028200(*piVar1,*(unsigned char *)(piVar1 + 1),piVar1[2],
                         piVar1[3],piVar1[4],piVar1[5],piVar1[6],piVar1[7],piVar1[8],piVar1[9],
                         piVar1[10],piVar1[0xb],piVar1[0xc],piVar1[0xd],piVar1[0xe]);
    while (iVar2 == -1) {
      if (*piVar1 != 1) {
        puVar3 = _fr.local_20;
        for (iVar2 = 8; iVar2 != 0; iVar2 = iVar2 + -1) {
          *puVar3 = 0x800f800f;
          puVar3 = puVar3 + 1;
        }
        FUN_100283c0(DAT_10697a4c,_fr.local_20,0);
        FUN_1006ff50(s_Out_of_tex_mem__100a9e5c);
        return DAT_10697a4c;
      }
      *piVar1 = 0;
      iVar2 = FUN_10028200(0,*(unsigned char *)(piVar1 + 1),piVar1[2],
                           piVar1[3],piVar1[4],piVar1[5],piVar1[6],piVar1[7],piVar1[8],piVar1[9],
                           piVar1[10],piVar1[0xb],piVar1[0xc],piVar1[0xd],piVar1[0xe]);
    }
    FUN_100283c0(iVar2,puVar3,0);
    iVar2 = FUN_10027850(piVar1,iVar2);
  }
  return iVar2;
}

/* WHAT IT DOES: build a texture-creation request on the stack -- the 0x2A8-byte record
 * the convert (0x10027B60) and make-Glide-texture (0x10027710) pair consume -- from the
 * fifteen parameters: sizes to powers of two, format code, clamp flags, one mip level
 * with GBI shifts, total texel bytes. Installed in hook slot 0x118ED1C4. Ghidra shredded
 * the record into ~60 independent locals, which /O2 then dead-stored away; the struct
 * spelling below is what makes every store live. */
/* @implements 0x100272F0 glide BrTex3dCreate */

void BrTex3dCreate(int param_1,int param_2,int param_3,int param_4,int param_5,
                 int param_6,int param_7,int param_8,int param_9,
                 int param_10,int param_11,int param_12,int param_13,int param_14,
                 int param_15)
{
  int iVar2;
  int uVar3;
  int iVar4;
  int iVar5;
  BrTexReq272 r;
  
  iVar4 = 1 << FUN_10027290(param_3);
  iVar5 = 1 << FUN_10027290(param_4);
  r.fTmu2 = (unsigned int)(1 < DAT_105ccbd0);
  r.h2a4 = param_4;
  r.h = param_4;
  r.lod = 3;
  r.w2a0 = param_3;
  r.w = param_3;
  r.wPow = iVar4;
  r.hPow = iVar5;
  r.f14 = 0;
  FUN_100242e0(&r.aspect0,iVar4,iVar5);
  r.aspect1 = r.aspect0;
  r.iLevel = 0;
  r.lv[0][3] = 0;
  iVar2 = FUN_10027290(param_3);
  r.lv[r.iLevel][8] = iVar2;
  iVar2 = FUN_10027290(param_4);
  r.lv[r.iLevel][9] = iVar2;
  r.lv[r.iLevel][2] = param_5;
  r.lv[r.iLevel][1] = param_6;
  r.lv[r.iLevel][0] = param_7;
  r.lv[r.iLevel][10] = param_12;
  r.lv[r.iLevel][11] = param_13;
  r.lv[r.iLevel][12] = 2;
  r.lv[r.iLevel][13] = 2;
  r.lv[r.iLevel][14] = param_3 * 4 + -2;
  r.lv[r.iLevel][15] = param_4 * 4 + -2;
  r.f5c = 1;
  r.f298 = param_15;
  r.f264 = 0;
  r.f268 = 0;
  r.b297 = 0;
  r.b296 = 0;
  r.b295 = 0;
  r.b294 = 0;
  r.b293 = 0;
  r.b292 = 0;
  r.b291 = 0;
  r.b290 = 0;
  _DAT_106b7aa8 = 0;
  _DAT_106b7aa4 = 0;
  r.fmt = FUN_10027220(r.lv[r.iLevel][1],r.lv[r.iLevel][0],0);
  r.cbTotal = FUN_10024df0(r.fmt) * iVar5 * iVar4;
  r.p1 = param_1;
  r.p2 = param_2;
  r.p8 = param_8;
  r.p9 = param_9;
  r.fClampS = (unsigned int)(param_10 != 0);
  r.fClampT = (unsigned int)(param_11 != 0);
  FUN_100275c0(&r.f20,iVar4,iVar5);
  r.f2c = 1;
  r.f30 = 1;
  r.f34 = 0xc0000000;
  r.f38 = 0;
  r.f260 = DAT_118ed1a0;
  r.cb29c = r.cbTotal;
  uVar3 = FUN_10027b60(&r);
  FUN_10027710(&r,uVar3);
  return;
}

/* WHAT IT DOES: build a texture-creation request for a BLANK texture of the given size
 * and format code (format 2/1 level fields, no source data, clamp both axes, priority
 * 0x10) and hand it to the convert + make-Glide-texture pair. Same 0x2A8-byte record as
 * BrTex3dCreate. */
/* @implements 0x10027FB0 glide BrTex3dCreateBlank */

void BrTex3dCreateBlank(int param_1,int param_2,int param_3,int param_4)
{
  int iVar2;
  int uVar3;
  int iVar4;
  int iVar5;
  BrTexReq272 r;
  
  iVar4 = 1 << FUN_10027290(param_2);
  iVar5 = 1 << FUN_10027290(param_3);
  r.fTmu2 = (unsigned int)(1 < DAT_105ccbd0);
  r.h2a4 = param_3;
  r.h = param_3;
  r.lod = 3;
  r.w2a0 = param_2;
  r.w = param_2;
  r.wPow = iVar4;
  r.hPow = iVar5;
  r.f14 = 0;
  FUN_100242e0(&r.aspect0,iVar4,iVar5);
  r.aspect1 = r.aspect0;
  r.iLevel = 0;
  r.lv[0][3] = 0;
  iVar2 = FUN_10027290(param_2);
  r.lv[r.iLevel][8] = iVar2;
  iVar2 = FUN_10027290(param_3);
  r.lv[r.iLevel][9] = iVar2;
  r.lv[r.iLevel][2] = param_2;
  r.lv[r.iLevel][1] = 1;
  r.lv[r.iLevel][0] = 2;
  r.lv[r.iLevel][10] = 0;
  r.lv[r.iLevel][11] = 0;
  r.lv[r.iLevel][12] = 2;
  r.lv[r.iLevel][13] = 2;
  r.lv[r.iLevel][14] = param_2 * 4 + -2;
  r.lv[r.iLevel][15] = param_3 * 4 + -2;
  r.f5c = 1;
  r.fmt = param_4;
  r.cbTotal = FUN_10024df0(param_4) * iVar5 * iVar4;
  r.p1 = param_1;
  r.p2 = 0;
  r.p8 = 0;
  r.p9 = 0;
  r.fClampS = 1;
  r.fClampT = 1;
  FUN_100275c0(&r.f20,iVar4,iVar5);
  r.f2c = 1;
  r.f30 = 1;
  r.f34 = 0xc0000000;
  r.f38 = 0;
  r.f260 = 0x10;
  r.f264 = 0;
  r.f268 = 0;
  uVar3 = FUN_10027b60(&r);
  FUN_10027710(&r,uVar3);
  return;
}

/* WHAT IT DOES: build a one-tile texture descriptor on the stack (same BrTexReq272
 * record) for texels+palette at (param_2, param_3) sized (param_4, param_5), zero the
 * eight dedup state bytes, and run the expander (0x100250D0) straight into `param_1`.
 * Returns the output byte count. Installed in hook slot 0x118ED1C0. The eight state
 * bytes are CHAR parameters -- VC5 pushes each as an unaligned dword window over the
 * record, which is what Ghidra renders as overlapping CONCATs. */
/* @implements 0x10024E60 glide BrTex3dExpandInto */

void FUN_100250d0(int,int,int,int,int,int,int,int,int,int,int*,int,int,
                  char,char,char,char,char,char,char,char,int);

int BrTex3dExpandInto(int param_1,int param_2,int param_3,int param_4,
                int param_5,int param_6,int param_7,int param_8)
{
  int uVar2;
  int iVar3;
  int iVar4;
  int iVar5;
  BrTexReq272 r;
  
  iVar4 = 1 << FUN_10027290(param_4);
  iVar5 = 1 << FUN_10027290(param_5);
  r.fTmu2 = (unsigned int)(1 < DAT_105ccbd0);
  r.h = param_5;
  r.hPow = param_5;
  r.lod = 3;
  r.w = param_4;
  r.wPow = param_4;
  r.f14 = 0;
  FUN_100242e0(&r.aspect0,iVar4,iVar5);
  r.aspect1 = r.aspect0;
  r.iLevel = 0;
  r.lv[0][3] = 0;
  uVar2 = FUN_10027290(param_4);
  r.lv[r.iLevel][8] = uVar2;
  uVar2 = FUN_10027290(param_5);
  _DAT_106b7aa8 = 0;
  _DAT_106b7aa4 = 0;
  r.lv[r.iLevel][9] = uVar2;
  r.lv[r.iLevel][2] = param_6;
  r.lv[r.iLevel][1] = param_7;
  r.lv[r.iLevel][0] = param_8;
  r.f5c = 1;
  r.f298 = 0;
  r.f264 = 0;
  r.f268 = 0;
  r.b297 = 0;
  r.b296 = 0;
  r.b295 = 0;
  r.b294 = 0;
  r.b293 = 0;
  r.b292 = 0;
  r.b291 = 0;
  r.b290 = 0;
  r.fmt = FUN_10027220(r.lv[r.iLevel][1],r.lv[r.iLevel][0],0);
  r.cbTotal = FUN_10024df0(r.fmt) * iVar5 * iVar4;
  r.p1 = param_2;
  r.p2 = param_3;
  r.f260 = DAT_118ed1a0;
  r.p8 = 0;
  r.p9 = 0;
  FUN_100250d0(param_1,r.cbTotal,r.lv[r.iLevel][1],param_2,param_3,
               r.lv[r.iLevel][0],0,0,r.iLevel,r.f5c,&r.lv[0][0],DAT_118ed1a0,
               r.f264,r.b290,r.b291,r.b292,r.b293,r.b294,r.b295,r.b296,r.b297,
               r.f298);
  return r.cbTotal;
}

/* WHAT IT DOES: THE REGISTRAR (br_tex3d.h's table) -- build the full 0x2A8-byte
 * descriptor for the tile run the scan just closed, from the tile records at
 * 0x10697840 and the source/state globals; dedup through 0x10027A70; size the
 * Glide texture (LOD/aspect codes, grTexCalcMemRequired, halving retries when
 * the aspect is unrepresentable); convert and append via 0x10027B60/0x10027710.
 * Returns the record index the 0xDC command will carry. */
/* @implements 0x10028BB0 glide BrTex3dRegister */

int BrTex3dRegister(void)

{
  unsigned short a;
  unsigned short b;
  int id;
  int sMask;
  int h;
  int tMask;
  int w;
  int wReal;
  int hReal;
  int d;
  int cb;
  int slot;
  int j;
  int wCur;
  int hCur;
  BrTexReq272 r;

  /* Opening copies: orig stores p1, p2, then f264, then b290..b295 in
   * field-offset order.  Ghidra's f264-first / b292-first order rotates
   * the first three stores. */
  r.p1 = DAT_105d17f0;
  r.p2 = DAT_106b7a98;
  r.f264 = DAT_106b7aac;
  r.b290 = DAT_105e1800;
  r.b291 = DAT_1066182c;
  r.b292 = DAT_105e17f8;
  r.b293 = DAT_105d17e8;
  r.b294 = DAT_10697a68;
  r.b295 = DAT_106b7a78;
  r.f268 = 0;
  r.b296 = DAT_10697a40;
  r.b297 = DAT_10661828;
  id = FUN_10027a70(&r);
  if (id != -1) {
    return id;
  }
  /* r.iLevel is a store of DAT_106b7ab0, not a cache: first-region
   * indexing re-derefs the global so the CSE can die in the LOD walk
   * (orig edx).  1 and h are one web (orig ebp): materialize 1 first so
   * fTmu2 is cmp-reg not cmp-imm. */
  r.iLevel = DAT_106b7ab0;
  if (DAT_106b7a94 < DAT_106b7ab0) {
    DAT_106b7a94 = DAT_106b7ab0;
  }
  h = 1;
  r.fTmu2 = (unsigned int)(DAT_105ccbd0 > h);
  r.lod = 3;
  sMask = DAT_10697840[DAT_106b7ab0].maskS;
  w = h << sMask;
  r.w = w;
  r.wPow = w;
  tMask = DAT_10697840[DAT_106b7ab0].maskT;
  h = h << tMask;
  r.h = h;
  r.hPow = h;
  if (DAT_106b7a94 > DAT_106b7ab0) {
    w = tMask;
    j = DAT_106b7ab0 + 1;
    for (; j <= DAT_106b7a94; j++) {
      sMask = sMask - 1;
      if ((DAT_10697840[j].maskS != sMask) ||
          (w = w - 1, DAT_10697840[j].maskT != w)) {
        DAT_106b7a94 = j - 1;
        break;
      }
    }
  }
  w = DAT_10697840[DAT_106b7ab0].mirrorS;
  hCur = DAT_10697840[DAT_106b7ab0].mirrorT;
  if (w) {
    r.w = r.w * 2;
  }
  if (hCur) {
    r.h = h * 2;
  }
  r.fmt = FUN_10027220(DAT_10697840[DAT_106b7ab0].siz,
                       DAT_10697840[DAT_106b7ab0].fmt, DAT_106b7aac);
  r.aspect0 = 8;
  r.aspect1 = 8;
  r.f14 = 2;
  r.fClampS = (unsigned int)(DAT_10697840[r.iLevel].clampS != 0);
  r.f20 = 3;
  r.fClampT = (unsigned int)(DAT_10697840[r.iLevel].clampT != 0);
  wReal = (DAT_10697840[r.iLevel].lrs - DAT_10697840[r.iLevel].uls + 4) >> 2;
  hReal = (DAT_10697840[r.iLevel].lrt - DAT_10697840[r.iLevel].ult + 4) >> 2;
  if (wReal != r.wPow) {
    if (wReal == r.w) {
      r.wPow = wReal;
    }
    else if ((wReal > r.w) && ((wReal / r.w) * r.w == wReal)) {
      r.fClampS = 0;
    }
  }
  if (hReal != r.hPow) {
    if (hReal == r.h) {
      r.hPow = hReal;
    }
    else if ((hReal > r.h) && ((hReal / r.h) * r.h == hReal)) {
      r.fClampT = 0;
    }
  }
  FUN_100242e0(&r.aspect1,r.w,r.h);
  h = (FUN_100275c0(&r.f20,r.w,r.h) == 0);
  d = DAT_106b7a94 - DAT_106b7ab0;
  FUN_100242e0(&r.aspect0,r.w >> d,r.h >> d);
  if (DAT_100b8498 > 1) {
    r.aspect0 = r.aspect1;
    DAT_106b7a94 = DAT_106b7ab0;
  }
  r.f2c = 1;
  r.f30 = 1;
  r.f34 = 0xc0000000;
  r.f38 = 0;
  cb = grTexCalcMemRequired(r.aspect0,r.aspect1,r.f20,r.fmt);
  if (cb > 0x80000) {
    cb = 0x80000;
  }
  r.cbTotal = cb;
  memcpy(r.lv,DAT_10697840,0x200);
  r.f5c = DAT_106b7a94 + 1;
  r.p8 = w;
  r.p9 = hCur;
  r.f298 = 1;
  r.w2a0 = r.w;
  r.h2a4 = r.h;
  if (r.w >= r.h) {
    r.wPow = r.w;
    r.hPow = r.w;
  }
  else {
    r.wPow = r.h;
    r.hPow = r.h;
  }
  if (h || w || hCur) {
    wCur = r.w;
    hCur = r.h;
    if (w && (DAT_100b8498 > 1)) {
      wCur = wCur / 2;
      FUN_100242e0(&r.aspect1,wCur,hCur);
      h = (FUN_100275c0(&r.f20,wCur,hCur) == 0);
      d = r.aspect1 - DAT_106b7ab0;
      r.aspect0 = d + DAT_106b7a94;
    }
    if (r.p9 && (DAT_100b8498 > 1)) {
      hCur = hCur / 2;
      FUN_100242e0(&r.aspect1,wCur,hCur);
      h = (FUN_100275c0(&r.f20,wCur,hCur) == 0);
      d = r.aspect1 - DAT_106b7ab0;
      r.aspect0 = d + DAT_106b7a94;
    }
    if (h) {
      id = r.w;
      sMask = r.h;
      if (r.w >= r.h) {
        if (r.w > 1) {
          id = r.w / 2;
          FUN_100242e0(&r.aspect1,id,sMask);
          FUN_100275c0(&r.f20,id,sMask);
          d = r.aspect1 - DAT_106b7ab0;
          r.aspect0 = d + DAT_106b7a94;
        }
      }
      else if (r.h > 1) {
        sMask = r.h / 2;
        FUN_100242e0(&r.aspect1,id,sMask);
        FUN_100275c0(&r.f20,id,sMask);
        d = r.aspect1 - DAT_106b7ab0;
        r.aspect0 = d + DAT_106b7a94;
      }
      FUN_10029290(&wCur,&hCur,r.aspect1,r.f20);
    }
    r.w = wCur;
    r.h = hCur;
  }
  if ((DAT_100b8498 > 1) && FUN_10030fd0(r.p1,&r.w,&r.h)) {
    FUN_100242e0(&r.aspect1,r.w,r.h);
    FUN_100275c0(&r.f20,r.w,r.h);
    d = r.aspect1 - DAT_106b7ab0;
      r.aspect0 = d + DAT_106b7a94;
  }
  r.f260 = DAT_118ed1a0;
  r.cb29c = r.cbTotal;
  if ((r.f260 & 2) && (r.f5c == 2)) {
    r.f260 = DAT_118ed1a0 | 0x80;
  }
  sMask = FUN_10027b60(&r);
  id = FUN_10027710(&r,sMask);
  if (*(int *)(DAT_106b7aa0 + 0x26c + id * 0x2b4)) {
    slot = FUN_1005a070();
    if ((slot >= 0) && (slot < 8)) {
      DAT_1186c968[slot] = id;
    }
  }
  if (r.f260 & 2) {
    if (r.f268) {
      if (r.f278) {
        FUN_100306d0(id);
      }
    }
    else if (FUN_10030710(r.p2)) {
      a = *(unsigned short *)r.p2;
      b = ((unsigned short *)r.p2)[1];
      *(unsigned short *)r.p2 = 0xffff;
      ((unsigned short *)r.p2)[1] = 0xffff;
      sMask = FUN_10027b60(&r);
      (*DAT_118ed1d0)(id,sMask);
      hCur = FUN_10001000(FUN_10001000(0,0,0),sMask,r.cbTotal);
      *(unsigned short *)r.p2 = a;
      ((unsigned short *)r.p2)[1] = b;
      sMask = FUN_10027b60(&r);
      (*DAT_118ed1d0)(id,sMask);
      if (FUN_10001000(FUN_10001000(0,0,0),sMask,r.cbTotal) != hCur) {
        FUN_100306d0(id);
      }
    }
  }
  return id;
}

#endif /* BR_MATCHING_BUILD */
