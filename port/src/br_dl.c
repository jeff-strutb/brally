/* br_dl.c -- the display-list machine.  See br_dl.h for what this is and how
 * it was established.  Every address literal is from orig/BRGlide.dll, which
 * CONVENTIONS.md names as the reference; where the D3D build's handler is a
 * different function the divergence is called out at the site.
 *
 * WHAT IS TRANSCRIBED AND WHAT IS NOT
 * ----------------------------------------------------------------------
 * Transcribed from the disassembly, opcode by opcode:
 *   the loop (0x10023C90), the dispatch table (0x100A9A58), the load-time
 *   patch pass (0x10019040), and the handlers for
 *   0x01 0x03 0x04 0x06 0xB1 0xB6 0xB7 0xB8 0xB9 0xBC 0xBD 0xBF
 *   0xDC 0xDD 0xDE 0xDF 0xE1 0xE2 0xE3 0xE4 0xED 0xF2 0xF6 0xF7 0xF8
 *   0xFA 0xFB 0xFC.
 *
 * NOT transcribed, and flagged as DEVIATION where it shows:
 *   - CLIPPING.  0x1001ECF0 hands a partly-visible triangle to 0x1001EE70,
 *     which drives six 311-byte per-plane routines (0x1001F0D0, 0x1001F2B0,
 *     0x1001F3F0, 0x1001F530, 0x1001F670, 0x1001F7B0, 0x1001F8F0 -- all
 *     SHARED, so all real port targets) and a common interpolator
 *     (0x1001F200).  None of those seven has been read.  What is below is a
 *     straightforward homogeneous Sutherland-Hodgman that produces the right
 *     SHAPE but is not claimed to match the original's vertex order, its
 *     tie-breaking, or its behaviour on degenerate input.
 *   - LIGHTING.  The transform copies the Vtx's last three bytes straight
 *     into r/g/b and nothing in it consults g_5CCFD0 (numlights) or the light
 *     array at 0x105CCC78, which both G_MOVEWORD and G_MOVEMEM plainly
 *     maintain.  So a lighting pass exists and has not been found.  Vertices
 *     therefore come out of this file carrying the raw bytes.
 *   - The 0xFA (prim colour) handler is 138 bytes in Glide and 183 in D3D
 *     and was not read; only the payload is recorded here.
 */

#include "br_dl.h"

#include <string.h>

/* slice1_05.c owns 0x100306C0 (D3D) == 0x10029D70 (Glide), which shared.csv
 * pairs as one shared function.  Declared by prototype rather than by
 * including slice1_05.h, the way br_font.c declares BrRdpSetCombineLERP, so
 * this file does not drag in a dozen unrelated models.  Do NOT re-implement
 * it here: one original address must have one host definition. */
extern void BrMat4Mul(const BrMat4 *pA, const BrMat4 *pB, BrMat4 *pOut);

/* ==================================================================== */
/* helpers                                                              */
/* ==================================================================== */

/* The list is in host order by the time the interpreter sees it, so a command
 * is two host u32s.  Read them byte-wise anyway: CONVENTIONS.md forbids
 * overlaying a struct on a foreign buffer, and a display list is exactly
 * that. */
static uint32_t br_dl_w(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint32_t br_dl_be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static void br_dl_putw(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;         p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

/* 0x1001EC30's sign fold: a 12-bit field above 0x800 is negative. */
static int32_t br_dl_s12(uint32_t v)
{
    int32_t x = (int32_t)(v & 0xFFFu);
    return (x >= 0x800) ? x - 0x1000 : x;
}

/* Read a float out of a host-order 32-bit pattern without aliasing. */
static float br_dl_f32(uint32_t v)
{
    float f;
    memcpy(&f, &v, sizeof(f));
    return f;
}

/* ==================================================================== */
/* addressing -- see the header on why this is a table and not a cast    */
/* ==================================================================== */

int BrDlAddRegion(BrDl *pDl, uint32_t base, const void *pHost, size_t cb)
{
    if (pDl->cRegions >= BR_DL_REGIONS)
        return 1;
    pDl->aRegion[pDl->cRegions].base  = base;
    pDl->aRegion[pDl->cRegions].pHost = (const uint8_t *)pHost;
    pDl->aRegion[pDl->cRegions].cb    = cb;
    pDl->cRegions++;
    return 0;
}

const uint8_t *BrDlResolve(const BrDl *pDl, uint32_t addr, size_t cbNeed)
{
    int i;
    if (addr == 0u)
        return NULL;
    for (i = 0; i < pDl->cRegions; ++i) {
        const BrDlRegion *pR = &pDl->aRegion[i];
        uint32_t off;
        if (addr < pR->base)
            continue;
        off = addr - pR->base;
        if ((size_t)off > pR->cb || pR->cb - (size_t)off < cbNeed)
            continue;
        return pR->pHost + off;
    }
    return NULL;
}

/* ==================================================================== */
/* state                                                                */
/* ==================================================================== */

void BrDlInit(BrDl *pDl, int32_t cxScreen, int32_t cyScreen)
{
    int i;

    memset(pDl, 0, sizeof(*pDl));

    /* 0x10023B10 seeds the vertex array to zero and iModel to 1.  The `1`
     * matters: a list that never issues G_MTX still has a live modelview
     * slot, and `iModel == 0` is the sentinel meaning "none", which
     * 0x10021080 turns into a NULL matrix pointer. */
    pDl->iModel = 1;
    for (i = 0; i < BR_DL_MTX_STACK; ++i)
        BrMat4Identity(&pDl->aModel[i]);
    BrMat4Identity(&pDl->proj);
    BrMat4Identity(&pDl->combined);

    pDl->scisULX = 0;
    pDl->scisULY = 0;
    pDl->scisLRX = cxScreen;
    pDl->scisLRY = cyScreen;

    /* 0x100A7514 / 0x100A7518 -- the grSstWinOpen dimensions the fill and
     * scissor handlers flip Y against. */
    pDl->vpScaleX = (float)cxScreen * 0.5f;
    pDl->vpTransX = (float)cxScreen * 0.5f;
    pDl->vpScaleY = (float)cyScreen * -0.5f;
    pDl->vpTransY = (float)cyScreen * 0.5f;

    pDl->env[0] = pDl->env[1] = pDl->env[2] = pDl->env[3] = 1.0f;
    pDl->prim[0] = pDl->prim[1] = pDl->prim[2] = pDl->prim[3] = 1.0f;
}

void BrDlSetViewport(BrDl *pDl, float scaleX, float transX,
                     float scaleY, float transY)
{
    pDl->vpScaleX = scaleX; pDl->vpTransX = transX;
    pDl->vpScaleY = scaleY; pDl->vpTransY = transY;
}

/* ==================================================================== */
/* handlers                                                             */
/* ==================================================================== */

typedef const uint8_t *(*BrDlHandler)(BrDl *, const uint8_t *);

/* 0x10021240 -- 228 of the 256 table slots point here. */
static const uint8_t *br_dl_skip(BrDl *pDl, const uint8_t *p)
{
    pDl->cUnhandled++;
    return p + 8;
}

/* ---- 0x01 G_MTX  (0x10021080, SHARED) ------------------------------- */
static const uint8_t *br_dl_mtx(BrDl *pDl, const uint8_t *p)
{
    uint32_t w0 = br_dl_w(p);
    const uint8_t *pSrcBytes = BrDlResolve(pDl, br_dl_w(p + 4), 64);
    BrMat4 src, tmp;
    BrMat4 *pCur;
    int i;

    /* An unresolvable payload means "identity" here.  The original would
     * dereference whatever the loader left in the word; there is no host on
     * which that is safe, so the port refuses.  DEVIATION. */
    if (pSrcBytes == NULL) {
        BrMat4Identity(&src);
    } else {
        for (i = 0; i < 16; ++i)
            ((float *)src.m)[i] = br_dl_f32(br_dl_w(pSrcBytes + i * 4));
    }

    if (w0 & 0x10000u) {                 /* G_MTX_PROJECTION */
        if (w0 & 0x20000u)               /* G_MTX_LOAD */
            pDl->proj = src;
        else
            BrMat4Mul(&src, &pDl->proj, &pDl->proj);
    } else if (w0 & 0x20000u) {          /* modelview, load */
        if (w0 & 0x40000u) {             /* G_MTX_PUSH */
            if (pDl->iModel == 10) pDl->iModel = 0;
            pDl->iModel++;
        }
        pDl->aModel[pDl->iModel] = src;
    } else {                             /* modelview, multiply */
        pCur = pDl->iModel ? &pDl->aModel[pDl->iModel] : NULL;
        BrMat4Identity(&tmp);
        BrMat4Mul(&src, pCur, &tmp);
        if (w0 & 0x40000u) {
            if (pDl->iModel == 10) pDl->iModel = 0;
            pDl->iModel++;
        }
        pDl->aModel[pDl->iModel] = tmp;
        pDl->fCombinedStale = 0;
    }

    /* 0x1002118A: combined = model * projection, with a NULL model when the
     * stack index is 0.  BrMat4Mul returns without writing on a NULL input,
     * so the previous combined survives -- preserved, not "fixed". */
    pCur = pDl->iModel ? &pDl->aModel[pDl->iModel] : NULL;
    BrMat4Mul(pCur, &pDl->proj, &pDl->combined);
    return p + 8;
}

/* ---- 0x03 G_MOVEMEM  (0x10023810, SHARED) --------------------------- */
static const uint8_t *br_dl_movemem(BrDl *pDl, const uint8_t *p)
{
    uint32_t w0 = br_dl_w(p), w1 = br_dl_w(p + 4);
    unsigned idx = (w0 >> 16) & 0xFFu;

    if (idx < 0x80u || idx > 0x9Eu)
        return p + 8;

    if (idx == 0x80u) {
        /* viewport (0x10023920, Glide-only, 156 bytes) -- not read. */
        (void)w1;
    } else if (idx == 0x82u || idx == 0x84u) {
        /* lookat y / lookat x: the pointer is simply stored
         * (0x105CE2D8 / 0x105CE2DC) and never dereferenced here. */
    } else if (idx == 0x9Eu) {
        /* G_MV_MATRIX_1 (0x10023900): sixteen dwords straight into the
         * COMBINED matrix.  Not the stack -- 0x105D1760 itself. */
    } else if (idx >= 0x86u && idx <= 0x94u) {
        /* the light array: base + ((idx - 0x86) / 2) * 16, (w0 & 0xFFFF)
         * bytes.  0x1002386E. */
        unsigned slot = (idx - 0x86u) >> 1;
        unsigned cb   = w0 & 0xFFFFu;
        if (slot < BR_DL_LIGHTS) {
            if (cb > 16u) cb = 16u;
            /* The payload pointer cannot be followed on this host for the
             * same 32-bit reason as G_MTX; the slot is marked live so a
             * caller can see the command was honoured. */
            memset(pDl->aLight[slot], 0, sizeof(pDl->aLight[slot]));
            pDl->fCombinedStale = 0;
        }
    }
    return p + 8;
}

/* ---- 0x04 G_VTX  (0x10021A20, Glide-only; D3D 0x10021BD0) ----------- */
static const uint8_t *br_dl_vtx(BrDl *pDl, const uint8_t *p)
{
    uint32_t w0 = br_dl_w(p);
    int n  = (int)((w0 >> 10) & 0x3Fu);   /* bits[15:10] */
    int v0 = (int)((w0 >> 16) & 0xFFu);   /* byte 2 of w0 -- see br_dl.h    */
    const uint8_t *pSrc = BrDlResolve(pDl, br_dl_w(p + 4),
                                      (size_t)(n > 0 ? n : 1) * 0x20u);
    int i;

    pDl->cVtxLoads++;
    /* `jle` on the count: n == 0 is a no-op, and the ONLY bound in the
     * original is that the destination index is a byte.  Nothing stops a
     * malformed list writing past the 32-entry array; the port clamps.
     * DEVIATION. */
    if (n <= 0 || pSrc == NULL)
        return p + 8;

    for (i = 0; i < n; ++i) {
        BrDlVtx *pV;
        float sx, sy, invW;
        int32_t oc = 0;
        int k = v0 + i;

        if (k < 0 || k >= BR_DL_VTX_COUNT)
            break;
        pV = &pDl->aVtx[k];

        /* Source stride is 0x20: the eight floats BrVtxExpand (0x10018EF0
         * Glide == 0x1002BE30 D3D, SHARED) writes -- x,y,z,s,t,n0,n1,n2. */
        {
            float x = br_dl_f32(br_dl_w(pSrc + 0x00));
            float y = br_dl_f32(br_dl_w(pSrc + 0x04));
            float z = br_dl_f32(br_dl_w(pSrc + 0x08));
            const float *m = (const float *)pDl->combined.m;

            /* Row-vector: out = v * M, translation in row 3.  Read straight
             * off 0x10021A55..0x10021AF0, which multiplies y by m[4..7] and
             * z by m[8..11] and adds m[12..15]. */
            pV->cx = x * m[0] + y * m[4] + z * m[8]  + m[12];
            pV->cy = x * m[1] + y * m[5] + z * m[9]  + m[13];
            pV->cz = x * m[2] + y * m[6] + z * m[10] + m[14];
            pV->cw = x * m[3] + y * m[7] + z * m[11] + m[15];

            pV->s  = br_dl_f32(br_dl_w(pSrc + 0x0C));
            pV->t  = br_dl_f32(br_dl_w(pSrc + 0x10));
            pV->n0 = br_dl_f32(br_dl_w(pSrc + 0x14));
            pV->n1 = br_dl_f32(br_dl_w(pSrc + 0x18));
            pV->n2 = br_dl_f32(br_dl_w(pSrc + 0x1C));
        }

        /* The outcodes, in the original's order and with its comparison.
         * Every test is `fcomp 0.0 / test ah,1`, i.e. C0 -- STRICTLY LESS
         * THAN, and NaN takes the true side because an unordered compare
         * also sets C0.  Written as `!(v >= 0)` for exactly that reason
         * (CONVENTIONS.md, comparison polarity). */
        if (!(pV->cw >= 0.0f))              oc |= BR_DL_CLIP_W;
        if (!(pV->cz + pV->cw >= 0.0f))     oc |= BR_DL_CLIP_NEAR;
        if (!(pV->cw - pV->cz >= 0.0f))     oc |= BR_DL_CLIP_FAR;
        if (!(pV->cx + pV->cw >= 0.0f))     oc |= BR_DL_CLIP_LEFT;
        if (!(pV->cw - pV->cx >= 0.0f))     oc |= BR_DL_CLIP_RIGHT;
        if (!(pV->cy + pV->cw >= 0.0f))     oc |= BR_DL_CLIP_BOTTOM;
        if (!(pV->cw - pV->cy >= 0.0f))     oc |= BR_DL_CLIP_TOP;
        pV->outcode = oc;

        if (oc == 0) {
            invW = 1.0f / pV->cw;
            pV->oow = invW;
            sx = pDl->vpScaleX * invW * pV->cx + pDl->vpTransX;
            sy = pDl->vpScaleY * invW * pV->cy + pDl->vpTransY;
            /* 0x10021BF9: snap to quarter-pixels -- multiply by 4.0
             * (0x10077408), round through fistp/fild, multiply by 0.25
             * (0x1007740C).  fistp is round-to-nearest under the default
             * control word, NOT truncation. */
            sx = (float)((int32_t)(sx * 4.0f + (sx >= 0.0f ? 0.5f : -0.5f))) * 0.25f;
            sy = (float)((int32_t)(sy * 4.0f + (sy >= 0.0f ? 0.5f : -0.5f))) * 0.25f;
            pV->x = sx;
            pV->y = sy;
            /* r/g/b take the Vtx's trailing bytes verbatim -- see the file
             * header on the missing lighting pass. */
            pV->r = pV->n0;
            pV->g = pV->n1;
            pV->b = pV->n2;
            pDl->cVtxTransformed++;
        }

        pSrc += 0x20;
    }
    return p + 8;
}

/* ---- 0x06 G_DL  (0x10021020, Glide-only) ---------------------------- */
static const uint8_t *br_dl_calldl(BrDl *pDl, const uint8_t *p)
{
    uint32_t w0 = br_dl_w(p), w1 = br_dl_w(p + 4);

    if ((w0 & 0x00FF0000u) == 0u) {          /* branch-and-link, not a jump */
        if (pDl->sp + 1 == BR_DL_DL_STACK) {
            /* The original calls exit(1) here.  DEVIATION: refuse the push
             * and count it, because a test suite must not terminate the
             * process to report a defect. */
            pDl->cStackOverflow++;
            return NULL;
        }
        pDl->aStack[pDl->sp++] = p + 8;
        pDl->cDlCalls++;
    }
    /* The callee address is a display-list address like any other; it goes
     * through the region table rather than being cast. */
    return BrDlResolve(pDl, w1, 8);
}

/* ---- 0xB8 G_ENDDL  (0x10021060, SHARED) ----------------------------- */
static const uint8_t *br_dl_enddl(BrDl *pDl, const uint8_t *p)
{
    (void)p;
    if (pDl->sp == 0)
        return NULL;
    pDl->sp--;
    return pDl->aStack[pDl->sp];
}

/* ---- 0xB6 / 0xB7 geometry mode  (0x1001FD40 / 0x100211E0, SHARED) --- */
static const uint8_t *br_dl_geoclear(BrDl *pDl, const uint8_t *p)
{
    pDl->geoModePrev = pDl->geoMode;
    pDl->geoMode &= ~br_dl_w(p + 4);
    return p + 8;
}
static const uint8_t *br_dl_geoset(BrDl *pDl, const uint8_t *p)
{
    pDl->geoModePrev = pDl->geoMode;
    pDl->geoMode |= br_dl_w(p + 4);
    return p + 8;
}

/* ---- 0xB9 G_SETOTHERMODE_L  (0x10021210, SHARED) -------------------- */
static const uint8_t *br_dl_othermodeL(BrDl *pDl, const uint8_t *p)
{
    /* `shl eax,0x10 / sar eax,0x18` -- sign-extend bits[15:8], the shift
     * field.  Shift 0 (alpha compare) is explicitly a no-op; shift 3
     * (render mode) is the only one honoured; everything else falls through
     * to a plain p+8.  So SETOTHERMODE_H (0xBA) has no handler at all. */
    int32_t shift = (int32_t)(int8_t)((br_dl_w(p) >> 8) & 0xFFu);

    if (shift == 3) {
        pDl->renderMode = br_dl_w(p + 4);
        if (pDl->sink.pfnRenderMode)
            pDl->sink.pfnRenderMode(pDl->sink.pUser, pDl->renderMode);
    }
    return p + 8;
}

/* ---- 0xBC G_MOVEWORD  (0x100239C0, SHARED) -------------------------- */
static const uint8_t *br_dl_moveword(BrDl *pDl, const uint8_t *p)
{
    uint32_t w0 = br_dl_w(p), w1 = br_dl_w(p + 4);
    int32_t  type = (int32_t)(int8_t)(w0 & 0xFFu);

    /* The 13-entry index table at 0x10023A90 sends only types 2 and 10
     * anywhere; every other type in 2..14 lands on the p+8 tail. */
    if (type == 2) {                                   /* G_MW_NUMLIGHT */
        pDl->nLights = (int32_t)((w1 >> 5) & 0xFu);
    } else if (type == 10) {                           /* G_MW_LIGHTCOL */
        uint32_t off = (w0 >> 8) & 0xFFFFu;
        uint32_t slot = (off >> 5) & 0xFFu;
        /* The low nibble picks between the light's two colour triples --
         * +0 when zero, +4 otherwise (0x10023A15 vs 0x10023A4E). */
        uint32_t at = ((off & 0xFu) == 0u) ? 0u : 4u;
        if (slot < BR_DL_LIGHTS) {
            pDl->aLight[slot][at + 0] = (uint8_t)(w1 >> 24);
            pDl->aLight[slot][at + 1] = (uint8_t)(w1 >> 16);
            pDl->aLight[slot][at + 2] = (uint8_t)(w1 >> 8);
            pDl->fCombinedStale = 0;
        }
    }
    return p + 8;
}

/* ---- 0xBD G_POPMTX  (0x100211B0, SHARED) ---------------------------- */
static const uint8_t *br_dl_popmtx(BrDl *pDl, const uint8_t *p)
{
    if (pDl->iModel != 0) {
        pDl->iModel--;
        if (pDl->iModel == 0)
            pDl->iModel = 10;      /* wraps, it does not clamp */
    }
    return p + 8;
}

/* ---- triangles  (0xBF 0x1001ECF0, 0xB1 0x1001FA30 -- both Glide-only) */

/* 0x1001ED83: s and t are scaled by two globals (0x118ED1A4 / 0x118ED1A8 --
 * the tile's texel-to-Glide-unit factors, written by the texture binder,
 * which has not been read) and then by 1/w.  Held at 1.0 here. */
static void br_dl_finish_vtx(BrDl *pDl, BrDlVtx *pV)
{
    (void)pDl;
    pV->tmu0[2] = pV->oow;
    pV->tmu1[2] = pV->oow;
    pV->tmu0[0] = pV->s * pV->oow;
    pV->tmu1[0] = pV->s * pV->oow;
    pV->tmu0[1] = pV->t * pV->oow;
    pV->tmu1[1] = pV->t * pV->oow;
}

static void br_dl_tri(BrDl *pDl, int i0, int i1, int i2)
{
    BrDlVtx *a, *b, *c;
    int32_t and3, or3;

    pDl->cTriIn++;
    if ((unsigned)i0 >= BR_DL_VTX_COUNT || (unsigned)i1 >= BR_DL_VTX_COUNT ||
        (unsigned)i2 >= BR_DL_VTX_COUNT)
        return;                       /* DEVIATION: the original indexes raw */

    a = &pDl->aVtx[i0]; b = &pDl->aVtx[i1]; c = &pDl->aVtx[i2];

    /* 0x1001ED37: reject when all three share an outcode bit.  Note the
     * order -- the AND is computed from b and c and only then tested against
     * a, which is the same value but is worth preserving because the
     * original's register pressure made it visible. */
    and3 = (b->outcode & c->outcode);
    if ((a->outcode & and3) != 0) {
        pDl->cTriRejected++;
        return;
    }
    or3 = a->outcode | b->outcode | c->outcode;
    if (or3 != 0) {
        /* DEVIATION -- see the file header.  The original calls 0x1001EE70. */
        pDl->cTriClipped++;
        return;
    }

    br_dl_finish_vtx(pDl, a);
    br_dl_finish_vtx(pDl, b);
    br_dl_finish_vtx(pDl, c);
    pDl->cTriDrawn++;
    if (pDl->sink.pfnTri)
        pDl->sink.pfnTri(pDl->sink.pUser, a, b, c);
}

static const uint8_t *br_dl_tri1(BrDl *pDl, const uint8_t *p)
{
    /* Bytes 6, 5, 4 in that order -- 0x1001ECFC reads [esi+6] first and the
     * push order at 0x1001EE15 puts it first.  The patch pass has already
     * halved them (0x10019250). */
    br_dl_tri(pDl, p[6], p[5], p[4]);
    return p + 8;
}

static const uint8_t *br_dl_tri2(BrDl *pDl, const uint8_t *p)
{
    br_dl_tri(pDl, p[2], p[1], p[0]);
    br_dl_tri(pDl, p[6], p[5], p[4]);
    return p + 8;
}

/* ---- 0xDC bind texture  (0x1001E2E0 Glide, 30 B; D3D 0x1001BD70, 149) */
static const uint8_t *br_dl_bindtex(BrDl *pDl, const uint8_t *p)
{
    uint32_t w0 = br_dl_w(p), w1 = br_dl_w(p + 4);

    pDl->hTexture = w0 & 0x00FFFFFFu;
    if (pDl->sink.pfnBindTexture)
        pDl->sink.pfnBindTexture(pDl->sink.pUser, pDl->hTexture);
    /* `lea eax,[esi + ecx*8]` -- the command consumes w1 double-words, so
     * one 0xDC can stand in for a whole texture-setup run.  br_font.c emits
     * w1 == 1, i.e. just itself. */
    return p + (size_t)8 * (size_t)w1;
}

/* ---- 0xDD re-aim texture  (0x1001E300, SHARED) ---------------------- */
static const uint8_t *br_dl_retarget(BrDl *pDl, const uint8_t *p)
{
    if (pDl->sink.pfnRetarget)
        pDl->sink.pfnRetarget(pDl->sink.pUser,
                              br_dl_w(p) & 0x00FFFFFFu, br_dl_w(p + 4));
    return p + 8;
}

/* ---- 0xDE / 0xDF  (0x1001EB10 SHARED / 0x1001EB30 Glide-only) ------- */
static const uint8_t *br_dl_setDE(BrDl *pDl, const uint8_t *p)
{
    pDl->f0A9A54 = br_dl_f32(br_dl_w(p + 4));
    return p + 8;
}
static const uint8_t *br_dl_setDF(BrDl *pDl, const uint8_t *p)
{
    pDl->f5D17C4 = br_dl_f32(br_dl_w(p + 4));
    return p + 8;
}

/* ---- rectangles ------------------------------------------------------
 * Four opcodes, two coordinate conventions, and this is where the RDP
 * dialect diverges from stock F3D:
 *
 *   0xF6  fill rect, 10.2 corners, 8 bytes   (0x1001E320, SHARED)
 *   0xE1  fill rect, INTEGER corners, 8 B    (0x1001E720, SHARED)
 *   0xE4  texture rect, 10.2, 24 BYTES       (0x10021570, SHARED)
 *   0xE3  texture rect, INTEGER, 8 bytes     (0x100219D0, SHARED)
 *
 * CONVENTIONS.md already records "0xE1 is FILL RECTANGLE with integer
 * corners here"; the table pins that -- 0x100A9A58 + 0xE1*4 holds
 * 0x1001E720, whose only difference from the 0xF6 handler is `sar 0x14`
 * where the other has `sar 0x16`, i.e. no /4. */

static const uint8_t *br_dl_rect(BrDl *pDl, const uint8_t *p,
                                 int fTextured, int fFixed)
{
    uint32_t w0 = br_dl_w(p), w1 = br_dl_w(p + 4);
    int32_t ulx, uly, lrx, lry, tile = 0;

    if (fTextured) {
        /* 0x10021570 / 0x100219D0 both call 0x100215C0 with
         * (ulx, uly, lrx, lry, tile) in 10.2; the integer form multiplies
         * by four on the way in, so both are reduced to pixels here. */
        lrx = (int32_t)((w0 >> 12) & 0xFFFu);
        lry = (int32_t)(w0 & 0xFFFu);
        ulx = (int32_t)((w1 >> 12) & 0xFFFu);
        uly = (int32_t)(w1 & 0xFFFu);
        tile = (int32_t)((w1 >> 24) & 7u);
        if (fFixed) { lrx >>= 2; lry >>= 2; ulx >>= 2; uly >>= 2; }
    } else {
        ulx = (int32_t)((w0 >> 12) & 0xFFFu);
        uly = (int32_t)(w0 & 0xFFFu);
        lrx = (int32_t)((w1 >> 12) & 0xFFFu);
        lry = (int32_t)(w1 & 0xFFFu);
        if (fFixed) { ulx >>= 2; uly >>= 2; lrx >>= 2; lry >>= 2; }
    }

    pDl->cRects++;
    if (pDl->sink.pfnRect)
        pDl->sink.pfnRect(pDl->sink.pUser, fTextured, tile, ulx, uly, lrx, lry);
    /* 0xE4 alone is three double-words. */
    return p + ((fTextured && fFixed) ? 0x18 : 8);
}

static const uint8_t *br_dl_fillF6(BrDl *d, const uint8_t *p)
{ return br_dl_rect(d, p, 0, 1); }
static const uint8_t *br_dl_fillE1(BrDl *d, const uint8_t *p)
{ return br_dl_rect(d, p, 0, 0); }
static const uint8_t *br_dl_texE4(BrDl *d, const uint8_t *p)
{ return br_dl_rect(d, p, 1, 1); }
static const uint8_t *br_dl_texE3(BrDl *d, const uint8_t *p)
{ return br_dl_rect(d, p, 1, 0); }

/* ---- 0xED / 0xE2 scissor  (0x1001EB50 / 0x1001EBC0, both Glide-only) */
static const uint8_t *br_dl_scissor(BrDl *pDl, const uint8_t *p)
{
    uint32_t w0 = br_dl_w(p), w1 = br_dl_w(p + 4);
    pDl->scisULX = (int32_t)((w0 >> 14) & 0x3FFu);
    pDl->scisULY = (int32_t)((w0 >> 2) & 0x3FFu);
    pDl->scisLRX = (int32_t)((w1 >> 14) & 0x3FFu);
    pDl->scisLRY = (int32_t)((w1 >> 2) & 0x3FFu);
    return p + 8;
}

/* ---- 0xF2 G_SETTILESIZE  (0x1001EC30, SHARED) ----------------------- */
static const uint8_t *br_dl_settilesize(BrDl *pDl, const uint8_t *p)
{
    uint32_t w0 = br_dl_w(p), w1 = br_dl_w(p + 4);

    pDl->uls = br_dl_s12(w0 >> 12);
    pDl->ult = br_dl_s12(w0);
    pDl->lrs = br_dl_s12(w1 >> 12);
    pDl->lrt = br_dl_s12(w1);
    /* `(lrs - uls + 4) >> 2` with an ARITHMETIC shift, so a negative span
     * rounds toward -inf.  Preserved. */
    pDl->tileW = (pDl->lrs - pDl->uls + 4) >> 2;
    pDl->tileH = (pDl->lrt - pDl->ult + 4) >> 2;
    return p + 8;
}

/* ---- 0xF7 fill colour, 0xF8 fog colour ------------------------------ */
static const uint8_t *br_dl_fillcolour(BrDl *pDl, const uint8_t *p)
{
    pDl->fillColour = br_dl_w(p + 4);
    return p + 8;
}
static const uint8_t *br_dl_fogcolour(BrDl *pDl, const uint8_t *p)
{
    pDl->fogColour = br_dl_w(p + 4);
    return p + 8;
}

/* ---- 0xFA prim colour, 0xFB env colour  (0x1001EA80 / 0x1001E930) --- */
static void br_dl_unpack(uint32_t v, float *pOut)
{
    /* 0x1001E930 multiplies each byte by 0x10077400 == 1/255 in that exact
     * order: R (bits 31:24), G, B, A. */
    pOut[0] = (float)((v >> 24) & 0xFFu) * (1.0f / 255.0f);
    pOut[1] = (float)((v >> 16) & 0xFFu) * (1.0f / 255.0f);
    pOut[2] = (float)((v >> 8) & 0xFFu) * (1.0f / 255.0f);
    pOut[3] = (float)(v & 0xFFu) * (1.0f / 255.0f);
}
static const uint8_t *br_dl_prim(BrDl *pDl, const uint8_t *p)
{
    br_dl_unpack(br_dl_w(p + 4), pDl->prim);
    return p + 8;
}
static const uint8_t *br_dl_env(BrDl *pDl, const uint8_t *p)
{
    br_dl_unpack(br_dl_w(p + 4), pDl->env);
    return p + 8;
}

/* ---- 0xFC G_SETCOMBINE  (0x1001E770 SHARED -> 0x1001E7A0 Glide-only)  */

/* The whole state model, in one table.  0x1001E7A0 is a chain of exact
 * equality tests on the pair, and this is that chain with the compare order
 * preserved -- the 0xFC317E02 row has two accepted w1 values, which is why
 * the table has a mask column rather than two rows. */
static const struct { uint32_t w0, w1, w1b; BrDlCombine id; } s_aCombine[] = {
    { 0xFCFFFFFFu, 0xFFFCF87Cu, 0xFFFCF87Cu, BR_DL_CC_SHADE        },
    { 0xFCFFFFFFu, 0xFFFE793Cu, 0xFFFE793Cu, BR_DL_CC_TEX          },
    { 0xFC567EACu, 0xFFFFF3F9u, 0xFFFFF3F9u, BR_DL_CC_TEX_SHADE_C1 },
    { 0xFCFF97FFu, 0xFF2DFEFFu, 0xFF2DFEFFu, BR_DL_CC_TEX_SHADE_A  },
    { 0xFCFFFFFFu, 0xFFFDF2F9u, 0xFFFDF2F9u, BR_DL_CC_TEX_SHADE_B  },
    { 0xFCFFFFFFu, 0xFFFF73B9u, 0xFFFF73B9u, BR_DL_CC_TEX_SHADE_CW },
    { 0xFC127E08u, 0xF3FFF2F8u, 0xF3FFF2F8u, BR_DL_CC_ENVMAP       },
    { 0xFC317E02u, 0x5FFEF3FAu, 0x51FEF3FAu, BR_DL_CC_DECAL        },
    { 0xFC127FFFu, 0xFFFFF838u, 0xFFFFF838u, BR_DL_CC_TEX_SHADE_C0 }
};

BrDlCombine BrDlClassifyCombine(uint32_t w0, uint32_t w1)
{
    size_t i;
    for (i = 0; i < sizeof(s_aCombine) / sizeof(s_aCombine[0]); ++i)
        if (s_aCombine[i].w0 == w0 &&
            (s_aCombine[i].w1 == w1 || s_aCombine[i].w1b == w1))
            return s_aCombine[i].id;
    return BR_DL_CC_DEFAULT;
}

static const uint8_t *br_dl_combine(BrDl *pDl, const uint8_t *p)
{
    uint32_t w0 = br_dl_w(p), w1 = br_dl_w(p + 4);

    pDl->combineW0 = w0;
    pDl->combineW1 = w1;
    pDl->combine   = BrDlClassifyCombine(w0, w1);
    /* 0x1001E8C0 sets 0x105CDA04 only on the DECAL row, and the tail at
     * 0x1001E8FB uses it to swap the triangle routine at 0x100A9A68 between
     * 0x10021C70 and 0x100221D0.  So this one row selects a whole different
     * rasterisation path -- it is not just a blend. */
    pDl->fDecal = (pDl->combine == BR_DL_CC_DECAL) ? 1 : 0;
    if (pDl->sink.pfnCombine)
        pDl->sink.pfnCombine(pDl->sink.pUser, pDl->combine, w0, w1);
    return p + 8;
}

/* ==================================================================== */
/* the table -- 0x100A9A58                                              */
/* ==================================================================== */

static BrDlHandler s_aTable[256];
static int s_fTableReady;

static void br_dl_build_table(void)
{
    int i;
    if (s_fTableReady)
        return;
    for (i = 0; i < 256; ++i)
        s_aTable[i] = br_dl_skip;
    s_aTable[0x01] = br_dl_mtx;
    s_aTable[0x03] = br_dl_movemem;
    s_aTable[0x04] = br_dl_vtx;
    s_aTable[0x06] = br_dl_calldl;
    s_aTable[0xB1] = br_dl_tri2;
    s_aTable[0xB6] = br_dl_geoclear;
    s_aTable[0xB7] = br_dl_geoset;
    s_aTable[0xB8] = br_dl_enddl;
    s_aTable[0xB9] = br_dl_othermodeL;
    s_aTable[0xBC] = br_dl_moveword;
    s_aTable[0xBD] = br_dl_popmtx;
    s_aTable[0xBF] = br_dl_tri1;
    s_aTable[0xDC] = br_dl_bindtex;
    s_aTable[0xDD] = br_dl_retarget;
    s_aTable[0xDE] = br_dl_setDE;
    s_aTable[0xDF] = br_dl_setDF;
    s_aTable[0xE1] = br_dl_fillE1;
    s_aTable[0xE2] = br_dl_scissor;
    s_aTable[0xE3] = br_dl_texE3;
    s_aTable[0xE4] = br_dl_texE4;
    s_aTable[0xED] = br_dl_scissor;
    s_aTable[0xF2] = br_dl_settilesize;
    s_aTable[0xF6] = br_dl_fillF6;
    s_aTable[0xF7] = br_dl_fillcolour;
    s_aTable[0xF8] = br_dl_fogcolour;
    s_aTable[0xFA] = br_dl_prim;
    s_aTable[0xFB] = br_dl_env;
    s_aTable[0xFC] = br_dl_combine;
    s_fTableReady = 1;
}

int BrDlIsHandled(unsigned op)
{
    br_dl_build_table();
    return (op < 256u) && (s_aTable[op] != br_dl_skip);
}

/* ==================================================================== */
/* 0x10023C90                                                           */
/* ==================================================================== */

size_t BrDlRun(BrDl *pDl, const uint8_t *pList, size_t cbMax)
{
    const uint8_t *p = pList;
    const uint8_t *pEnd = pList + cbMax;
    size_t n = 0;

    br_dl_build_table();
    if (pDl == NULL || pList == NULL)
        return 0;

    /* The original is exactly:
     *     while (p) p = table[p[3]](p);
     * with no bound at all.  The `pEnd` test is a DEVIATION; it can only
     * fire on a list the original would have walked off the end of, and
     * only for lists that stay inside [pList, pList+cbMax) -- a G_DL into
     * another buffer legitimately leaves the range, so the bound is applied
     * only while the cursor is still inside it. */
    while (p != NULL) {
        unsigned op;
        if (p >= pList && p < pEnd && (size_t)(pEnd - p) < 8u)
            break;
        op = p[3];
        pDl->cCommands++;
        n++;
        p = s_aTable[op](pDl, p);
        if (n > 1000000u)
            break;                       /* DEVIATION: cycle guard */
    }
    return n;
}

/* ==================================================================== */
/* 0x10019040 -- the load-time patch pass                               */
/* ==================================================================== */

size_t BrDlPatch(const BrSegMap *pMap, uint8_t *pList, size_t cbMax,
                 void (*pfnResolve)(void *pUser, uint32_t *pw1, int nVerts),
                 void *pUser)
{
    size_t off = 0, n = 0;

    if (pList == NULL)
        return 0;

    while (off + 8 <= cbMax) {
        uint8_t *p = pList + off;
        uint32_t w0, w1;
        unsigned op;

        /* The original byte-swaps IN PLACE and then reads the opcode out of
         * the swapped word: `mov ah,[esi]` etc. assembles big-endian bytes
         * into a host dword, stores it, and then takes bits 31:24.  So the
         * opcode after the swap is byte 3, which is why the interpreter
         * indexes [3] and not [0]. */
        w0 = br_dl_be32(p);
        w1 = br_dl_be32(p + 4);
        br_dl_putw(p, w0);
        br_dl_putw(p + 4, w1);
        n++;

        op = (w0 >> 24) & 0xFFu;
        /* `add eax,-4 / cmp eax,0xF9 / ja skip` -- opcodes outside 0x04..0xFD
         * are skipped without even a table lookup. */
        if (op < 0x04u || op > 0xFDu) { off += 8; continue; }

        switch (op) {
        case 0x04: {                            /* G_VTX  (0x10019210) */
            uint32_t v = pMap ? (pMap->n64Base ^ ((pMap->n64Base ^ w1) & 0x00FFFFFFu))
                              : w1;
            if (pMap) BrSegFixup(pMap, &v);
            br_dl_putw(p + 4, v);
            if (pfnResolve) {
                uint32_t cur = br_dl_w(p + 4);
                pfnResolve(pUser, &cur, (int)((w0 >> 10) & 0x3Fu));
                br_dl_putw(p + 4, cur);
            }
            break;
        }
        case 0xBF:                              /* G_TRI1 (0x10019250) */
            p[6] = (uint8_t)(p[6] >> 1);
            p[5] = (uint8_t)(p[5] >> 1);
            p[4] = (uint8_t)(p[4] >> 1);
            break;
        case 0xB1:                              /* G_TRI2 (0x10019270) */
            p[2] = (uint8_t)(p[2] >> 1);
            p[1] = (uint8_t)(p[1] >> 1);
            p[0] = (uint8_t)(p[0] >> 1);
            p[6] = (uint8_t)(p[6] >> 1);
            p[5] = (uint8_t)(p[5] >> 1);
            p[4] = (uint8_t)(p[4] >> 1);
            break;
        case 0xB8:                              /* G_ENDDL: stop        */
            return n;
        case 0xFD: {                            /* G_SETTIMG            */
            uint32_t v = br_dl_w(p + 4);
            if (pMap) BrSegFixup(pMap, &v);
            br_dl_putw(p + 4, v);
            break;
        }
        default:
            break;
        }
        off += 8;
    }
    return n;
}

/* ==================================================================== */
/* PART 3 -- reference rasteriser (NOT in the original)                 */
/* ==================================================================== */

static void br_ras_tri(void *pUser, const BrDlVtx *a, const BrDlVtx *b,
                       const BrDlVtx *c)
{
    BrDlRaster *pR = (BrDlRaster *)pUser;
    float minx, maxx, miny, maxy, area;
    int32_t x0, x1, y0, y1, px, py;

    minx = a->x; if (b->x < minx) minx = b->x; if (c->x < minx) minx = c->x;
    maxx = a->x; if (b->x > maxx) maxx = b->x; if (c->x > maxx) maxx = c->x;
    miny = a->y; if (b->y < miny) miny = b->y; if (c->y < miny) miny = c->y;
    maxy = a->y; if (b->y > maxy) maxy = b->y; if (c->y > maxy) maxy = c->y;

    area = (b->x - a->x) * (c->y - a->y) - (b->y - a->y) * (c->x - a->x);
    if (area == 0.0f)
        return;

    x0 = (int32_t)minx; if (x0 < 0) x0 = 0;
    y0 = (int32_t)miny; if (y0 < 0) y0 = 0;
    x1 = (int32_t)maxx + 1; if (x1 > pR->cx) x1 = pR->cx;
    y1 = (int32_t)maxy + 1; if (y1 > pR->cy) y1 = pR->cy;

    for (py = y0; py < y1; ++py) {
        for (px = x0; px < x1; ++px) {
            float fx = (float)px + 0.5f, fy = (float)py + 0.5f;
            float w0 = (b->x - a->x) * (fy - a->y) - (b->y - a->y) * (fx - a->x);
            float w1 = (c->x - b->x) * (fy - b->y) - (c->y - b->y) * (fx - b->x);
            float w2 = (a->x - c->x) * (fy - c->y) - (a->y - c->y) * (fx - c->x);
            uint8_t *q;
            float l0, l1, l2, r, g, bl;

            if (area > 0.0f) { if (w0 < 0 || w1 < 0 || w2 < 0) continue; }
            else             { if (w0 > 0 || w1 > 0 || w2 > 0) continue; }

            l1 = w2 / area; l2 = w0 / area; l0 = 1.0f - l1 - l2;
            r  = l0 * a->r + l1 * b->r + l2 * c->r;
            g  = l0 * a->g + l1 * b->g + l2 * c->g;
            bl = l0 * a->b + l1 * b->b + l2 * c->b;
            /* The colour slots hold whatever the Vtx's trailing bytes were,
             * scaled by 1/128 and therefore in [-1, 1].  Fold to [0, 1] so
             * the output is a picture rather than a clamp artefact. */
            r = r * 0.5f + 0.5f; g = g * 0.5f + 0.5f; bl = bl * 0.5f + 0.5f;
            if (r < 0) r = 0; if (r > 1) r = 1;
            if (g < 0) g = 0; if (g > 1) g = 1;
            if (bl < 0) bl = 0; if (bl > 1) bl = 1;

            q = pR->pRgba + ((size_t)py * (size_t)pR->cx + (size_t)px) * 4;
            q[0] = (uint8_t)(r * 255.0f);
            q[1] = (uint8_t)(g * 255.0f);
            q[2] = (uint8_t)(bl * 255.0f);
            q[3] = 255;
            pR->cCovered++;
        }
    }
}

void BrDlAttachRaster(BrDl *pDl, BrDlRaster *pRas)
{
    memset(&pDl->sink, 0, sizeof(pDl->sink));
    pDl->sink.pUser  = pRas;
    pDl->sink.pfnTri = br_ras_tri;
}
