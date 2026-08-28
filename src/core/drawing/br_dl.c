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
 * CLIPPING has since been read.  PART 2 below is the DRIVER 0x1001EE70 only;
 * the seven per-plane routines, the interpolator 0x1001F200 and the 64-node
 * pool are slice1_03.c's, under their D3D addresses -- grepping the Glide
 * ones finds nothing.  See the section header for the mapping.
 *
 * LIGHTING has since been read; it is PART 4 below, and the note that used to
 * stand here -- "a lighting pass exists and has not been found" -- is
 * withdrawn.  It was not found because it is not a pass: it is a second
 * G_VTX HANDLER, installed over the first.
 *
 * NOT transcribed, and flagged as DEVIATION where it shows:
 *   - TEXTURE_GEN's s/t generation.  0x10022600 and 0x10022BF0 are the two
 *     lit transforms the geometry mode selects when G_TEXTURE_GEN is set;
 *     they do the same lighting as 0x10021C70 (they call the same
 *     0x10022AC0) and additionally derive s/t from the normal.  The lighting
 *     is here; the s/t derivation is not, and pDl->cVtxTexGen counts the
 *     vertices that would have had it so the gap is visible rather than
 *     silent.  Nothing in this port sets G_TEXTURE_GEN today.
 *   - 0x1001E380, the 914-byte rectangle emitter both untextured fill-rect
 *     opcodes call.  Its four opening CLAMPS are read (they are what pin
 *     which scissor global is min and which is max) and its emission is not.
 *     The four arguments 0xE1 and 0xF6 compute for it are recorded in
 *     pDl->rectMinX..rectMaxY; the clamp and the two Glide triangles are not
 *     performed.
 *
 * FOURTEEN OPCODES ARE TRANSCRIBED TWICE IN THIS TREE, stated here rather
 * than left to be found.  br_dlglide.c independently ports 0xDC 0xDD 0xDF
 * 0xE1 0xE2 0xED and br_dlcmd.c independently ports 0x04 0xB1 0xBF 0xF6 0xF7
 * 0xFA 0xFB 0xFC, all under the same original addresses this file uses.  Two
 * host definitions of one original address is the hazard CONVENTIONS.md's
 * "Aliased storage" section names.  They now AGREE -- the seven places where
 * they did not are the subject of the opcode audit this pass acted on -- but
 * agreeing is not the same as being one object, and the end state is that one
 * of each pair goes.
 */

#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include "br_dl.h"

/* The clip planes, the interpolator and the node pool are slice1_03's --
 * see PART 2 on why this file must not own a second copy of them. */
#include "slice1_03.h"

/* The routines this file and slice2_16.c BOTH used to transcribe.  Same
 * original function, one host body -- see br_dlshared.h. */
#include "br_dlshared.h"

#include <string.h>

/* slice1_05.c owns 0x100306C0 (D3D) == 0x10029D70 (Glide), which shared.csv
 * pairs as one shared function.  Declared by prototype rather than by
 * including slice1_05.h, the way br_font.c declares BrRdpSetCombineLERP, so
 * this file does not drag in a dozen unrelated models.  Do NOT re-implement
 * it here: one original address must have one host definition. */
extern void BrMat4Mul(const BrMat4 *pA, const BrMat4 *pB, BrMat4 *pOut);

#include "br_vec.h"      /* BrVec3 -- see br_dl_normalise below */

#include <math.h>

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

/* 0x1001EC30's sign fold: a 12-bit field above 0x800 is negative.  0x1001E720
 * does the same thing with `shl 20 / sar 20` on all four of its corner fields;
 * written as the explicit fold because C99 leaves a signed right shift
 * implementation-defined. */
/* NOT 0x1001EC30 -- an annotation pass attached that address here because
 * the comment above opens with it.  0x1001EC30 is the 178-byte 0xF2
 * handler, which is br_dl_settilesize below; this is a six-line helper it
 * and 0x1001E720 both use. */
static int32_t br_dl_s12(uint32_t v)
{
    int32_t x = (int32_t)(v & 0xFFFu);
    return (x >= 0x800) ? x - 0x1000 : x;
}

/* `fistp dword ptr [0x105CE310]` under MSVC's startup control word: round to
 * NEAREST, TIES TO EVEN.  Out of range -- and NaN -- stores the x87 integer
 * indefinite 0x80000000.
 *
 * This is NOT __ftol (0x1007C8A0 / MSVCRT `_ftol` at 0x10074560), which
 * truncates toward zero and yields 0 out of range.  Both appear in this
 * binary, a few hundred bytes apart, and the quarter-pixel snap uses the
 * FIRST: 0x100220D4 is a bare `fistp`, with no control-word change anywhere
 * in 0x10022070 or its callers.  The port used to add +/-0.5 and truncate,
 * which is ties-AWAY-from-zero and differs from the original on every exact
 * half -- e.g. 0.125 * 4 == 0.5 snaps to 0 here and to 1 that way.
 *
 * rint() honours the current rounding mode, which is the same ties-to-even.
 * br_dlcmd.c's br_dlcmd_fistp is the identical helper for the same `fistp`;
 * the two files are separate translation units transcribing separate original
 * functions, so this is a duplicated LEAF, not a duplicated address. */
static int32_t br_dl_fistp(double v)
{
    double r = rint(v);
    /* Negated conjunction so NaN takes the indefinite side, as x87 does;
     * `r < min || r > max` would let NaN through. */
    if (!(r >= -2147483648.0 && r <= 2147483647.0))
        return (int32_t)0x80000000;
    return (int32_t)r;
}

/* Read a float out of a host-order 32-bit pattern without aliasing. */
static float br_dl_f32(uint32_t v)
{
    float f;
    memcpy(&f, &v, sizeof(f));
    return f;
}

/* 0x10023B10's free-list threading; defined with the rest of the clipper. */
static void br_dl_clip_reset(BrDl *pDl);

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

    /* 0x10023B10 is the whole of this: it threads the clip-vertex pool into a
     * free list (see br_dl_clip_reset), zeroes the display-list stack pointer
     * 0x105CCFE8, sets iModel (0x100A9A50) to 1 and clears 0x105D17D4.  It
     * does NOT touch the vertex array -- an earlier note here said it did.
     *
     * The `1` matters: a list that never issues G_MTX still has a live
     * modelview slot, and `iModel == 0` is the sentinel meaning "none", which
     * 0x10021080 turns into a NULL matrix pointer. */
    pDl->iModel = 1;
    br_dl_clip_reset(pDl);
    for (i = 0; i < BR_DL_MTX_STACK; ++i)
        BrMat4Identity(&pDl->aModel[i]);
    BrMat4Identity(&pDl->proj);
    BrMat4Identity(&pDl->combined);

    /* 0x100A7514 / 0x100A7518 -- the grSstWinOpen dimensions the fill and
     * scissor handlers flip Y against.  Latched because both 0x1001EB50 and
     * 0x1001E720 read 0x100A7518 directly. */
    pDl->cxScreen = cxScreen;
    pDl->cyScreen = cyScreen;

    /* The clip window as 0x1001E1E0 / 0x1001E200 leave it for a full-screen
     * view, in the BOTTOM-UP min/max form the two scissor handlers write.
     * Those writers are outside this file, so this is a starting value rather
     * than a transcription and is stated as such. */
    pDl->scisMinX = 0;
    pDl->scisMinY = 0;
    pDl->scisMaxX = cxScreen;
    pDl->scisMaxY = cyScreen;

    pDl->vpScaleX = (float)cxScreen * 0.5f;
    pDl->vpTransX = (float)cxScreen * 0.5f;
    pDl->vpScaleY = (float)cyScreen * -0.5f;
    pDl->vpTransY = (float)cyScreen * 0.5f;

    /* Both colour globals are zero-initialised in the original (they are past
     * the end of .data's raw bytes).  This port has always started them at
     * WHITE instead, so a list that draws before its first 0xFA/0xFB is not
     * black; that is a port default and not a transcription, and it is kept.
     * The number differs between the two because the UNITS differ: env is
     * 0..1 (0xFB multiplies by 1/255) and prim is 0..255 (0xFA does not). */
    pDl->env[0] = pDl->env[1] = pDl->env[2] = pDl->env[3] = 1.0f;
    pDl->prim[0] = pDl->prim[1] = pDl->prim[2] = pDl->prim[3] =
        BR_DL_COLOUR_MAX;
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

/* 0x10021240 -- 228 of the 256 table slots point here.  BRD3D's copy is
 * 0x100243D0 and slice5_60.c ports it as BrGbiCall100243D0; the whole
 * function is `add eax,8`, so the shared part is the step, and it is named
 * once in br_dlshared.h rather than written as a bare 8 in both files. The
 * counter is this port's, not the original's. */
/* WHAT IT DOES: the do-nothing handler that most of the drawing-command
 * table points at: it counts the command as unhandled and steps over it. The
 * N64 command set is far larger than the game actually uses, so the great
 * majority of the 256 slots land here. */
/* @implements 0x10021240 glide br_dl_skip */
#ifdef BR_MATCHING_BUILD
static const uint8_t *br_dl_skip(const uint8_t *p)
{
    return p + 8;
}
#endif

/* Table-compatible do-nothing handler for the PORT dispatch, which calls every
 * slot as (pDl, p). The byte-exact original (br_dl_skip above) takes one arg;
 * this two-arg wrapper does the same step (p + 8) so the 256-slot table is
 * type-uniform. Called as s_aTable[op](pDl, p) — br_dl_skip's 1-arg form would
 * mis-read pDl as p, so the table must hold this version. */
static const uint8_t *br_dl_skip_h(BrDl *pDl, const uint8_t *p)
{
    /* Every unmapped opcode routes here (handled opcodes use real handlers),
     * so this is where the port's unhandled-command stat is counted.  Port-only
     * -- the matching build uses the byte-exact br_dl_skip above, which cannot
     * touch pDl. */
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
        /* 0x1002116E, reached from BOTH modelview arms (0x100210F7 and
         * 0x10021107 both jump to the same rep movsd + store).  An earlier
         * revision of this file cleared the flag only on the multiply arm,
         * which left the derived light direction stale after a G_MTX LOAD --
         * silently, because nothing read the flag at all then. */
        pDl->fLightCached = 0;
    } else {                             /* modelview, multiply */
        pCur = pDl->iModel ? &pDl->aModel[pDl->iModel] : NULL;
        BrMat4Identity(&tmp);
        BrMat4Mul(&src, pCur, &tmp);
        if (w0 & 0x40000u) {
            if (pDl->iModel == 10) pDl->iModel = 0;
            pDl->iModel++;
        }
        pDl->aModel[pDl->iModel] = tmp;
        pDl->fLightCached = 0;
    }
    /* Note what does NOT clear it: the two PROJECTION arms jump straight to
     * 0x10021174, past the store.  The light direction is transformed by the
     * modelview alone, so that is correct and not an oversight. */

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
         * bytes.  0x1002386E, a `rep movsd` + `rep movsb` pair. */
        unsigned slot = (idx - 0x86u) >> 1;
        unsigned cb   = w0 & 0xFFFFu;
        if (slot < BR_DL_LIGHTS) {
            const uint8_t *pSrc;
            if (cb > 16u) cb = 16u;
            /* The payload IS followed now -- through the region table, the
             * same way G_VTX and G_DL follow theirs.  It used to be zeroed
             * with a note that a 32-bit address could not be dereferenced on
             * this host, which is true of a CAST and not of a lookup; the
             * consequence was that every light was black. */
            pSrc = BrDlResolve(pDl, w1, cb);
            if (pSrc != NULL)
                memcpy(pDl->aLight[slot], pSrc, cb);
            else
                memset(pDl->aLight[slot], 0, cb);
            /* 0x10023898 -- and note it is unconditional in the original,
             * outside the `slot` test, because there is no slot test. */
            pDl->fLightCached = 0;
        }
    }
    return p + 8;
}

/* ==================================================================== */
/* PART 4 -- LIGHTING                                                   */
/* ==================================================================== */
/* WHERE IT IS, AND WHY IT WAS NOT FOUND BY LOOKING FOR A PASS
 * ----------------------------------------------------------------------
 * There is no lighting pass.  There is a second G_VTX HANDLER, and the
 * geometry mode installs it over the first:
 *
 *   0x1001FD40 (0xB6 clear) and 0x100211E0 (0xB7 set) both end in
 *   `call 0x1001FD70`, and 0x1001FD70 does nothing but push renderer state
 *   and then WRITE THE DISPATCH TABLE:
 *
 *       0x100A9A68 == 0x100A9A58 + 4 * 4   -- slot 0x04, G_VTX
 *       0x100A9D1C == 0x100A9A58 + 0xB1*4  -- slot 0xB1, G_TRI2
 *       0x100A9D54 == 0x100A9A58 + 0xBF*4  -- slot 0xBF, G_TRI1
 *
 *   Those three addresses were the whole mystery.  0x100A9A68 had been read
 *   as a free-standing "triangle routine pointer" (br_dl.h says so, in the
 *   BR_DL_CC_DECAL note); it is a table slot, and the routines it holds are
 *   vertex transforms.  The static image of the table has 0x10021A20 in it,
 *   which is why the unlit transform is the one everybody found.
 *
 *   The selection, transcribed from 0x1001FE0D:
 *
 *     ZBUFFER     LIGHTING  TEXGEN  TEXGEN_LIN    slot 0x04
 *     -------     --------  ------  ----------    ---------
 *        1           0         0        -         0x10021A20   unlit
 *        1           1         0        -         0x10021C70   lit
 *        1           1         0        -         0x100221D0   lit, DECAL
 *        1           -         1        0         0x10022600   lit + texgen
 *        1           -         1        1         0x10022BF0   lit + texgen
 *        0           0         -        -         0x10023110   unlit
 *        0           1         -        -         0x10023360   lit
 *
 *   and SHADING_SMOOTH picks the triangle pair (0x1001ECF0/0x1001FA30 vs
 *   0x1001FEF0/0x10020CF0 with a z-buffer, 0x10020900/0x10020D70 vs
 *   0x100203F0/0x10020D30 without).  0x1001E8FB additionally swaps 0x10021C70
 *   and 0x100221D0 when the DECAL combiner row is selected -- and ONLY those
 *   two, so it can never install a lit routine over an unlit one.
 *
 *   0x10021C70, 0x100221D0 and 0x10023360 are the same 1019/1059/1019 bytes
 *   with different x87 scheduling: identical constant use (three 0x10077418,
 *   three 0x10077420, the same nine 0x105CE2xx slots), so ONE transcription
 *   covers all three.  0x10022600 and 0x10022BF0 call the lighting out of
 *   line at 0x10022AC0 instead of inlining it, and add texgen.
 *
 * THE ARITHMETIC IS ALSO ALREADY HALF-PORTED, UNDER D3D ADDRESSES.
 *   BRD3D 0x10022350 is slice2_16.c's BrGbiLightVertex and is byte-for-byte
 *   the same 301-byte routine as Glide 0x10022AC0, down to the "lights off"
 *   fallback reading three NON-CONTIGUOUS globals.  Its BrGbiLightState maps
 *   one-for-one onto the three float triples below.  The two differ in
 *   exactly one thing: D3D clamps at 1.0f (0x1008F3C4) and divides by 255 in
 *   its setup, Glide clamps at 255.0f (0x10077418) and does not divide,
 *   because a Glide iterated colour is 0..255.  Glide is the reference, so
 *   this file carries 0..255 and BrDlColourScale() says so out loud.
 *
 * WHAT THE INPUT ACTUALLY IS.  Measured, not assumed: over every vertex the
 * two retail models reach -- 864 in bb.rca, 931 in ce.rca -- the magnitude of
 * the Vtx's trailing three signed bytes is 127.0 +/- 0.6, every single one.
 * They are unit normals.  The colours this port has been rendering are
 * normals painted as colour; the file does NOT hold lit colours. */

/* --- 0x100344D0, and a deliberate second copy -------------------------
 * The normalise 0x10021DB8 calls is BRD3D 0x1003AE50 -- config/shared.csv
 * pairs the two as ONE shared 141-byte function -- and slice2_21.c ALREADY
 * PORTS IT, as BrVec3NormaliseGuard (not BrVec3Normalise: slice1_09 owns that
 * name for the unguarded 0x10074180).  CONVENTIONS.md says to reuse, and the
 * first attempt here did: `extern void BrVec3NormaliseGuard(BrVec3 *)`.
 *
 * It does not link.  slice2_21.c needs BrSqrtF, whose only definition is in
 * slice4_53.c, which needs roughly the whole game; adding it to
 * build.d/test_br_dl.deps turns a five-object test into an unlinkable one.
 * So this is a SECOND HOST COPY of one original function, on purpose, and the
 * mitigation is that it is stated here rather than discovered later.  It is
 * arithmetically identical, including the two things that are easy to get
 * wrong and that br_dl_light_setup depends on:
 *
 *   - the sum order is y*y + z*z + x*x (the original spills y and z and
 *     squares them from the stack first);
 *   - a length of EXACTLY zero yields (0, 0, 1), not a zero vector and not
 *     the input.  0x1003450D compares against 0x100775F4 == 0.0f and tests
 *     C3, which an unordered compare also sets, so a NaN length takes the
 *     same arm.  `!(len != 0.0f)` for that reason.
 *
 * br_dl.c already carries one other such copy for the same kind of reason:
 * br_dl_outcode is 0x10022120, which slice2_16.c ports as BrGbiClipCodes.
 * Those are the only two, and both are pure leaf arithmetic with no state. */
/* WHAT IT DOES: scales a direction so it is exactly one unit long, with one
 * important safety valve: a direction of zero length -- or one that is not a
 * number -- comes out pointing straight along the third axis rather than
 * staying zero. The lighting setup relies on that. */
/* @implements 0x100344D0 glide br_dl_normalise */
static void br_dl_normalise(BrVec3 *pV)
{
    float len = (float)sqrt((double)(pV->y * pV->y + pV->z * pV->z +
                                     pV->x * pV->x));
    if (!(len != 0.0f)) {
        pV->x = 0.0f;
        pV->y = 0.0f;
        pV->z = 1.0f;
        return;
    }
    len = 1.0f / len;                     /* fdivr 0x100775F0 == 1.0f */
    pV->x = len * pV->x;
    pV->y = len * pV->y;
    pV->z = len * pV->z;
}

/* --- 0x10021C70's prologue (0x10021C70..0x10021E0F) -------------------
 * Rebuild the derived light state.  Guarded by 0x105D17D0, which G_MTX
 * (modelview only), G_MOVEMEM light and G_MOVEWORD LIGHTCOL all clear. */
/* WHAT IT DOES: works out the light the next batch of vertices will be
 * shaded by: the light's colour, its direction pulled back into the space
 * the model's surface normals live in, and the ambient colour. It caches the
 * answer and only redoes the work when something that could change it -- a
 * new matrix, a new light, a new light colour -- has cleared the cache. Note
 * it also latches the cache when there are no lights at all, having computed
 * nothing. */
/* @implements 0x10021C70 glide br_dl_light_setup */
static void br_dl_light_setup(BrDl *pDl)
{
    const uint8_t *pL = pDl->aLight[BR_DL_LIGHT_DIFFUSE];   /* 0x105CCC78 */
    const uint8_t *pA = pDl->aLight[BR_DL_LIGHT_AMBIENT];   /* 0x105CCC88 */
    const float *m;
    BrVec3 d;
    float dx, dy, dz;

    if (pDl->fLightCached)                /* 0x10021C7E */
        return;
    pDl->cLightSetup++;

    /* 0x10021C8B jumps straight to the `= 1` store, so the flag is latched
     * even though nothing was computed.  Preserved: it means a list that
     * sets numlights AFTER a G_MTX still rebuilds, because G_MOVEWORD
     * clears the flag again. */
    if (pDl->nLights == 0) {
        pDl->fLightCached = 1;
        return;
    }

    /* 0x10021C91: the MODELVIEW, not the combined matrix -- the direction is
     * being pulled back into the space the normals live in. */
    if (pDl->iModel != 0) {
        m = (const float *)pDl->aModel[pDl->iModel].m;
    } else {
        /* 0x10021CA5 leaves esi == 0 and 0x10021D22 then dereferences it.
         * The original faults.  BrDlInit sets iModel to 1 so it cannot
         * happen through this port's front door; identity here, counted.
         * DEVIATION. */
        static const BrMat4 s_ident = { { { 1, 0, 0, 0 }, { 0, 1, 0, 0 },
                                          { 0, 0, 1, 0 }, { 0, 0, 0, 1 } } };
        m = (const float *)s_ident.m;
        pDl->cLightNoMtx++;
    }

    /* The colour: three UNSIGNED bytes, `fild`ed and NOT scaled.  0..255 is
     * the Glide iterated-colour range, so no division is wanted or present. */
    pDl->lightScale[0] = (float)(unsigned)pL[BR_DL_LIGHT_COL + 0];
    pDl->lightScale[1] = (float)(unsigned)pL[BR_DL_LIGHT_COL + 1];
    pDl->lightScale[2] = (float)(unsigned)pL[BR_DL_LIGHT_COL + 2];

    /* The direction: three SIGNED bytes (`movsx`, 0x10021CAE/CC5/CE6). */
    dx = (float)(int)(int8_t)pL[BR_DL_LIGHT_DIR + 0];
    dy = (float)(int)(int8_t)pL[BR_DL_LIGHT_DIR + 1];
    dz = (float)(int)(int8_t)pL[BR_DL_LIGHT_DIR + 2];

    /* Row i of the modelview dotted with the direction, i.e. M applied as a
     * COLUMN-vector matrix -- which for a rotation is M's inverse under this
     * file's row-vector convention, so the light lands in model space.  The
     * grouping is the original's: (row[1]*dy + row[0]*dx) + row[2]*dz, then
     * `fdiv [0x10077420]` with 0x10077420 == 128.0f -- written as the divide
     * it is, not as a multiply by BR_DL_BYTE_SCALE, so the instruction and
     * the line still read the same way. */
    d.x = ((m[1] * dy + m[0] * dx) + m[2] * dz) / 128.0f;
    d.y = ((m[5] * dy + m[4] * dx) + m[6] * dz) / 128.0f;
    d.z = ((m[9] * dy + m[8] * dx) + m[10] * dz) / 128.0f;

    /* 0x10021DB8.  The /128 above is arithmetically redundant in front of a
     * normalise and is kept because the original does it -- it changes which
     * inputs underflow to a zero-length vector, and a zero-length vector
     * becomes (0, 0, 1) rather than staying zero. */
    br_dl_normalise(&d);
    pDl->lightDir[0] = d.x;
    pDl->lightDir[1] = d.y;
    pDl->lightDir[2] = d.z;

    /* 0x10021DBD: the ambient, three unsigned bytes of SLOT 1 at a fixed
     * address.  `numlights` never selects which slot -- see br_dl.h. */
    pDl->lightAmb[0] = (float)(unsigned)pA[BR_DL_LIGHT_COL + 0];
    pDl->lightAmb[1] = (float)(unsigned)pA[BR_DL_LIGHT_COL + 1];
    pDl->lightAmb[2] = (float)(unsigned)pA[BR_DL_LIGHT_COL + 2];

    pDl->fLightCached = 1;                /* 0x10021E05 */
}

/* --- 0x10022AC0 (== BRD3D 0x10022350 == slice2_16's BrGbiLightVertex) --
 * One vertex.  `pN` is the normal (source floats +0x14/+0x18/+0x1C), `pOut`
 * the three colour floats the caller writes to the vertex's +0x5C/+0x60/+0x64
 * -- the clip node's +0x1C/+0x20/+0x24, which is why the clipper carries the
 * LIT colour and not the normal across a cut edge. */
/* WHAT IT DOES: shades one vertex. With no lights it just copies the current
 * primitive colour. Otherwise it measures how squarely the surface faces the
 * light; surfaces facing away get plain ambient, and the rest get ambient
 * plus a share of the light's colour, capped at full brightness. Colours
 * here run 0 to 255, not 0 to 1. */
/* @implements 0x10022AC0 glide br_dl_light_vertex */
static void br_dl_light_vertex(BrDl *pDl, const float *pN, float *pOut)
{
    float t;
    int i;

    /* 0x10022AC6 / 0x10022BCC.  Note this arm does NOT use the ambient -- it
     * copies 0x105D17A4, 0x105D17B4 and 0x105CE2D0, which are exactly 0xFA's
     * first three destinations.  There was a separate `lightOff[3]` here with
     * no writer anywhere in the port, so this arm always produced (0,0,0);
     * one original object had two host names and only one of them was ever
     * assigned.  See br_dl.h on `prim`. */
    if (pDl->nLights == 0) {
        pOut[0] = pDl->prim[0];
        pOut[1] = pDl->prim[1];
        pOut[2] = pDl->prim[2];
        pDl->cVtxLitOff++;
        return;
    }

    /* n . L, grouped as the original groups it. */
    t = (pN[1] * pDl->lightDir[1] + pN[2] * pDl->lightDir[2])
        + pN[0] * pDl->lightDir[0];

    /* 0x10022B01: `fcomp 0.0 / test ah,1`, i.e. C0 -- strictly less than,
     * and an unordered compare sets C0 as well, so a NaN dot takes the
     * ambient-only arm.  Negated form for exactly that reason
     * (CONVENTIONS.md, comparison polarity). */
    if (!(t >= 0.0f)) {
        pOut[0] = pDl->lightAmb[0];
        pOut[1] = pDl->lightAmb[1];
        pOut[2] = pDl->lightAmb[2];
        pDl->cVtxLitAmbient++;
        return;
    }

    for (i = 0; i < 3; ++i) {
        float v = t * pDl->lightScale[i] + pDl->lightAmb[i];
        /* 0x10022B2A: `fcomp 255.0 / test ah,0x41`, i.e. C0|C3, and the
         * literal 255.0f is taken only when the test is ZERO -- an ORDERED
         * GREATER-THAN.  Unordered sets both bits, so a NaN is NOT clamped.
         * Written in the POSITIVE form here, which is the rare case where
         * that is the faithful one: C's `v > 255.0f` is also false for NaN.
         * `!(v <= 255.0f)` would clamp NaN and be wrong. */
        pOut[i] = (v > BR_DL_COLOUR_MAX) ? BR_DL_COLOUR_MAX : v;
    }
}

/* --- 0x1001FD70's table write, as a query -----------------------------
 * See the section header for the transcription. */
/* WHAT IT DOES: picks which of the six vertex-processing routines the next
 * batch of vertices goes through, based on the geometry switches currently
 * set -- whether the depth buffer is on, whether lighting is on, whether
 * texture coordinates are being generated, and whether decal mode is in
 * force. The port reports the original's address rather than installing a
 * function, so the choice stays checkable. */
/* @implements 0x1001FD70 glide BrDlVtxRoutine */
uint32_t BrDlVtxRoutine(const BrDl *pDl)
{
    uint32_t geo = pDl->geoMode;
    uint32_t vtx;

    if (geo & BR_DL_GEO_ZBUFFER) {                       /* 0x1001FE0D */
        if (geo & BR_DL_GEO_LIGHTING)                    /* 0x1001FE15 */
            vtx = pDl->fDecal ? 0x100221D0u : 0x10021C70u;
        else
            vtx = 0x10021A20u;
        if (geo & BR_DL_GEO_TEXTURE_GEN) {               /* 0x1001FE48 */
            vtx = 0x10022BF0u;
            if (!(geo & BR_DL_GEO_TEXTURE_GEN_LIN))
                vtx = 0x10022600u;
        }
    } else {                                             /* 0x1001FE99 */
        vtx = (geo & BR_DL_GEO_LIGHTING) ? 0x10023360u : 0x10023110u;
    }
    return vtx;
}

int BrDlIsLit(const BrDl *pDl)
{
    uint32_t v = BrDlVtxRoutine(pDl);
    return (v != 0x10021A20u && v != 0x10023110u);
}

float BrDlColourScale(const BrDl *pDl)
{
    return pDl->fVtxLit ? (1.0f / BR_DL_COLOUR_MAX) : 1.0f;
}

/* --- 0x10022120, and it is now ONE body -------------------------------
 * 0x10021A20 inlines these seven tests; 0x10021C70 calls them.  The tests
 * themselves used to be written out here AND again in slice2_16.c as
 * BrGbiClipCodes (BRD3D 0x10022DC0), and the two disagreed about NaN for as
 * long as both existed.  Both are gone; br_dlshared.c holds the one copy and
 * carries both addresses.  This is now nothing but the BrDlVtx field
 * ordering, which is this file's own business. */
static int32_t br_dl_outcode(const BrDlVtx *pV)
{
    return BrDlsClipCodes(pV->cx, pV->cy, pV->cz, pV->cw);
}

/* --- 0x10022070, and the identical tail 0x10021BAD..0x10021C48 ---------
 * Perspective divide, viewport, quarter-pixel snap, colour store.  The lit
 * transforms call it with the colour in three registers; the unlit one
 * inlines it and uses n0/n1/n2. */
/* WHAT IT DOES: turns a transformed vertex into a screen position: divides
 * through by depth to get perspective, applies the viewport scale and
 * offset, stores the vertex's colour, and snaps the result to the nearest
 * quarter of a pixel -- the resolution the hardware rasteriser works at. */
/* @implements 0x10022070 glide br_dl_project */
static void br_dl_project(BrDl *pDl, BrDlVtx *pV, float r, float g, float b)
{
    float invW, sx, sy;

    invW = 1.0f / pV->cw;                 /* fld 1.0 (0x10077404) / fdiv cw */
    pV->oow = invW;
    sx = pDl->vpScaleX * invW * pV->cx + pDl->vpTransX;
    sy = pDl->vpScaleY * pV->oow * pV->cy + pDl->vpTransY;
    pV->r = r;
    pV->g = g;
    pV->b = b;
    /* 0x100220C6..0x10022115: snap to quarter-pixels.
     *     fld x; fmul 4.0 (0x10077408); fstp [ebp-0xC]   <- rounds to float
     *     fld [ebp-0xC]; fistp [0x105CE310]              <- TIES TO EVEN
     *     fild [0x105CE310]; fstp [ebp-0xC]              <- back to float
     *     fld [ebp-0xC]; fmul 0.25 (0x1007740C); fstp x
     * The spill through [ebp-0xC] is a real rounding to float and is
     * reproduced.  See br_dl_fistp on why this is ties-to-even and not the
     * +/-0.5-and-truncate this line used to carry. */
    sx = (float)((float)br_dl_fistp((double)(float)(sx * 4.0f)) * 0.25f);
    sy = (float)((float)br_dl_fistp((double)(float)(sy * 4.0f)) * 0.25f);
    pV->x = sx;
    pV->y = sy;
}

/* ---- 0x04 G_VTX ----------------------------------------------------
 * Both transforms, selected exactly as 0x1001FD70 selects them.  The unlit
 * body is 0x10021A20 (Glide-only; D3D 0x10021BD0); the lit body is
 * 0x10021C70 and its three equivalents. */
static const uint8_t *br_dl_vtx(BrDl *pDl, const uint8_t *p)
{
    uint32_t w0 = br_dl_w(p);
    int n  = (int)((w0 >> 10) & 0x3Fu);   /* bits[15:10] */
    int v0 = (int)((w0 >> 16) & 0xFFu);   /* byte 2 of w0 -- see br_dl.h    */
    const uint8_t *pSrc = BrDlResolve(pDl, br_dl_w(p + 4),
                                      (size_t)(n > 0 ? n : 1) * 0x20u);
    int fLit = BrDlIsLit(pDl);
    int fTexGen = (pDl->geoMode & BR_DL_GEO_TEXTURE_GEN) != 0 &&
                  (pDl->geoMode & BR_DL_GEO_ZBUFFER) != 0;
    int i;

    pDl->cVtxLoads++;
    pDl->fVtxLit = fLit;

    /* 0x10021C70 does its light setup ONCE, before the count test and before
     * the source pointer is even loaded -- so a G_VTX with n == 0 still
     * rebuilds the derived state.  Preserved. */
    if (fLit)
        br_dl_light_setup(pDl);

    /* `jle` on the count: n == 0 is a no-op, and the ONLY bound in the
     * original is that the destination index is a byte.  Nothing stops a
     * malformed list writing past the 32-entry array; the port clamps.
     * DEVIATION. */
    if (n <= 0 || pSrc == NULL)
        return p + 8;

    for (i = 0; i < n; ++i) {
        BrDlVtx *pV;
        int k = v0 + i;

        if (k < 0 || k >= BR_DL_VTX_COUNT)
            break;
        pV = &pDl->aVtx[k];

        /* Source stride is 0x20: the eight floats BrVtxExpand (0x10018EF0
         * Glide == 0x1002BE30 D3D, SHARED) writes -- x,y,z,s,t,n0,n1,n2.
         * The last three are the trailing bytes scaled by 1/128
         * (0x100773A0), i.e. a unit normal for lit geometry. */
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

            if (fLit) {
                /* 0x10021F09.  The normal is read from the SOURCE and the
                 * result written over the vertex's +0x5C/+0x60/+0x64 -- the
                 * normal does not survive into the vertex record at all on
                 * this path, which is the whole reason the clipper's nine
                 * interpolated floats end up carrying a colour. */
                float aN[3], aC[3];
                aN[0] = br_dl_f32(br_dl_w(pSrc + 0x14));
                aN[1] = br_dl_f32(br_dl_w(pSrc + 0x18));
                aN[2] = br_dl_f32(br_dl_w(pSrc + 0x1C));
                br_dl_light_vertex(pDl, aN, aC);
                pV->n0 = aC[0];
                pV->n1 = aC[1];
                pV->n2 = aC[2];
                pDl->cVtxLit++;
                if (fTexGen)
                    pDl->cVtxTexGen++;   /* s/t not derived -- see header */
            } else {
                pV->n0 = br_dl_f32(br_dl_w(pSrc + 0x14));
                pV->n1 = br_dl_f32(br_dl_w(pSrc + 0x18));
                pV->n2 = br_dl_f32(br_dl_w(pSrc + 0x1C));
            }
        }

        pV->outcode = br_dl_outcode(pV);

        if (pV->outcode == 0) {
            br_dl_project(pDl, pV, pV->n0, pV->n1, pV->n2);
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
            pDl->fLightCached = 0;      /* 0x10023A33 / 0x10023A6C */
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

/* ==================================================================== */
/* PART 2 -- the clipper                                                */
/* ==================================================================== */
/* 0x1001EE70 (607 B) is the triangle submitter that drives seven per-plane
 * routines and a shared interpolator.  Only the DRIVER is here.  The plane
 * routines, the interpolator and the 64-node pool are slice1_03's -- see
 * below on why that matters -- so this section is the part that had no host
 * definition, and nothing else.
 *
 * WHERE THE REST OF IT ALREADY LIVED
 * ----------------------------------------------------------------------
 * BRGlide 0x1001F0D0 / 0x1001F2B0 / 0x1001F3F0 / 0x1001F530 are BRD3D
 * 0x1001D810 / 0x1001D9F0 / 0x1001DB30 / 0x1001DC70, which slice1_03.c ports
 * as BrClipPlaneW / WPlusF04 / WMinusF04 / WPlusF08, with 0x1001F200 ==
 * 0x1001D940 == BrClipLerpVert and the pool as BrClipPoolInit.  Grepping the
 * GLIDE addresses finds none of that; grepping the D3D ones finds all of it.
 * slice1_04.h even wrote down the three-line integration for the remaining
 * three planes and the reason -- forking the pool would be "a correctness
 * hazard, not just duplication".  Those three are now in slice1_03.c.
 *
 * THE LIST.  0x1001EE70's prologue takes three BrDlVtx*, adds 0x40 to each,
 * and links them a->b->c->a, keeping `{ head, count }` in two stack slots at
 * ebp-8 / ebp-4 which it passes to every plane routine by address -- exactly
 * slice1_03's BrClipList.  So a clip NODE is `&vtx->f40`, and that is what
 * pins slice1_03's positional field names: f04/f08/f0C = clip x/y/z,
 * f10/f14 = s/t, f18 = clip w, f1C/f20/f24 = the Vtx's trailing bytes.
 *
 * THE POOL.  0x10023B10 threads 0x105CCFF0..0x105CD9C8 downward in steps of
 * 0x28 -- 64 nodes -- and leaves the LOWEST as the head of the free list at
 * 0x105CDA00.  Every free site tests `0x105CCFF0 <= p < 0x105CD9F0` first, so
 * only pool nodes are recycled and the three vertex-resident seeds are
 * silently dropped.  The storage is here because this file is what makes the
 * pool exist at all in the port; the LIST is slice1_03's, one object.
 *
 * THE PLANES, and the order, which is observable.  The seven bodies are
 * identical apart from the two-instruction distance expression:
 *
 *   0x1001F7B0  fld cz ; fadd cw   ->  cz + cw   NEAR    called 1st
 *   0x1001F2B0  fld cw ; fadd cx   ->  cw + cx   LEFT    called 2nd
 *   0x1001F3F0  fld cw ; fsub cx   ->  cw - cx   RIGHT   called 3rd
 *   0x1001F670  fld cw ; fsub cy   ->  cw - cy   TOP     called 4th
 *   0x1001F8F0  fld cw ; fsub cz   ->  cw - cz   FAR     called 5th
 *   0x1001F530  fld cy ; fadd cw   ->  cy + cw   BOTTOM  called 6th
 *   0x1001F0D0  fld cw            ->  cw        W       called 7th
 *
 * Same seven half-spaces as br_dl_vtx's outcode bits, in a DIFFERENT order --
 * and Sutherland-Hodgman's output vertex order depends on it, so the order is
 * preserved rather than tidied.
 *
 * THE POLARITY.  Each routine does `fcomp [0x10077410]`, and 0x10077410 reads
 * 0x00000000, so the threshold is plain zero; then `fnstsw ax / test ah,1`,
 * i.e. C0, and the jump on C0 goes to the "outside" arm.  C0 is set for
 * unordered as well as less-than, so a NaN distance is OUTSIDE.  slice1_03
 * keeps that by writing the INSIDE test as `d >= 0.0f`. */

/* The pool storage.  Static, not per-BrDl, because the original's is one
 * global block and slice1_03's free list is one global list: a per-instance
 * copy would be the aliased-storage bug CONVENTIONS.md describes.  A second
 * live BrDl shares it, which is what the original does too. */
static BrClipVert s_aClipPool[BR_DL_CLIP_POOL];   /* 0x105CCFF0 */
static BrClipVert s_aClipSeed[3];                 /* the three &vtx->f40 */

/* 0x10023B10's free-list threading, delegated. */
/* WHAT IT DOES: empties the pool of spare vertices the triangle clipper
 * borrows from, putting every one of them back on the free list ready for
 * the next frame. */
/* @implements 0x10023B10 glide br_dl_clip_reset */
static void br_dl_clip_reset(BrDl *pDl)
{
    (void)pDl;
    BrClipPoolInit(s_aClipPool, BR_DL_CLIP_POOL);
}

/* The seven planes in 0x1001EE70's CALL order. */
typedef void (*BrDlClipPlaneFn)(BrClipList *);
static const BrDlClipPlaneFn s_apClipPlane[7] = {
    BrClipPlaneWPlusF0C,    /* 0x1001F7B0  NEAR   */
    BrClipPlaneWPlusF04,    /* 0x1001F2B0  LEFT   */
    BrClipPlaneWMinusF04,   /* 0x1001F3F0  RIGHT  */
    BrClipPlaneWMinusF08,   /* 0x1001F670  TOP    */
    BrClipPlaneWMinusF0C,   /* 0x1001F8F0  FAR    */
    BrClipPlaneWPlusF08,    /* 0x1001F530  BOTTOM */
    BrClipPlaneW            /* 0x1001F0D0  W      */
};

/* --- 0x1001EE70's output stage ---------------------------------------
 * Identical arithmetic to the tail of br_dl_vtx plus the s/t scaling of
 * br_dl_finish_vtx, written out here because the original writes it out here
 * too (0x1001EF82..0x1001F065) rather than calling either. */
/* WHAT IT DOES: turns one vertex produced by clipping into a finished screen
 * vertex, ready to be handed to the rasteriser: perspective divide,
 * viewport, quarter-pixel snap, colour, and texture coordinates divided
 * through by depth for both texture units. This is why a triangle cut by the
 * edge of the screen keeps its shading -- the clipper carries the already-
 * lit colour across the cut, not the surface normal. */
/* @implements 0x1001EE70 glide br_dl_clip_emit */
static void br_dl_clip_emit(BrDl *pDl, const BrClipVert *pN, BrDlVtx *pOut)
{
    float invW, sx, sy;

    /* DEVIATION: the original leaves the frame's ooz (+0x18) and a (+0x1C)
     * untouched, so they carry stack garbage into grDrawTriangle.  Zeroed
     * here; nothing downstream of this port reads them. */
    memset(pOut, 0, sizeof(*pOut));

    invW = 1.0f / pN->f18;                      /* fld 1.0 / fdiv cw */
    pOut->oow = invW;

    sx = pDl->vpScaleX * invW * pN->f04 + pDl->vpTransX;
    sy = pDl->vpScaleY * invW * pN->f08 + pDl->vpTransY;
    /* The same fmul 4 / fistp / fild / fmul 0.25 as br_dl_project, written out
     * again because 0x1001EFxx writes it out again.  Ties to even. */
    sx = (float)((float)br_dl_fistp((double)(float)(sx * 4.0f)) * 0.25f);
    sy = (float)((float)br_dl_fistp((double)(float)(sy * 4.0f)) * 0.25f);
    pOut->x = sx;
    pOut->y = sy;

    pOut->r = pN->f1C;
    pOut->g = pN->f20;
    pOut->b = pN->f24;

    pOut->cx = pN->f04; pOut->cy = pN->f08; pOut->cz = pN->f0C;
    pOut->cw = pN->f18;
    pOut->s  = pN->f10; pOut->t  = pN->f14;
    pOut->n0 = pN->f1C; pOut->n1 = pN->f20; pOut->n2 = pN->f24;
    pOut->outcode = 0;

    /* 0x1001F038 / 0x1001F050: the same two texel-scale globals
     * (0x118ED1A4 / 0x118ED1A8) br_dl_finish_vtx holds at 1.0. */
    pOut->tmu0[2] = invW;          pOut->tmu1[2] = invW;
    pOut->tmu0[0] = pN->f10 * invW; pOut->tmu1[0] = pOut->tmu0[0];
    pOut->tmu0[1] = pN->f14 * invW; pOut->tmu1[1] = pOut->tmu0[1];
}

/* --- 0x1001EE70 ------------------------------------------------------- */
static void br_dl_clip_tri(BrDl *pDl, const BrDlVtx *a, const BrDlVtx *b,
                           const BrDlVtx *c)
{
    const BrDlVtx *aIn[3];
    BrClipList list;
    BrDlVtx out[BR_DL_CLIP_MAX];
    int i, n;

    aIn[0] = a; aIn[1] = b; aIn[2] = c;
    for (i = 0; i < 3; ++i) {
        BrClipVert *pS = &s_aClipSeed[i];
        pS->f04 = aIn[i]->cx; pS->f08 = aIn[i]->cy; pS->f0C = aIn[i]->cz;
        pS->f10 = aIn[i]->s;  pS->f14 = aIn[i]->t;
        pS->f18 = aIn[i]->cw;
        pS->f1C = aIn[i]->n0; pS->f20 = aIn[i]->n1; pS->f24 = aIn[i]->n2;
    }
    /* a -> b -> c -> a, with the head on a.  The original writes c->next
     * twice: NULL first, then back to a.  The NULL is dead.  The seeds are
     * NOT pool nodes, so BrClipPoolFree will refuse them -- which is the
     * original's range test doing its job, not an omission. */
    s_aClipSeed[0].pNext = &s_aClipSeed[1];
    s_aClipSeed[1].pNext = &s_aClipSeed[2];
    s_aClipSeed[2].pNext = &s_aClipSeed[0];
    list.pHead  = &s_aClipSeed[0];
    list.cVerts = 3;

    /* Each call is followed by `cmp ecx,3 / jl` -- the chain stops the moment
     * the polygon cannot be a polygon any more. */
    for (i = 0; i < 7; ++i) {
        s_apClipPlane[i](&list);
        if (list.cVerts < 3)
            break;
    }

    /* Pool starvation: slice1_03's BrClipLerpVert returns NULL where the
     * original faults, and BrClipPlane then quietly leaves the polygon a
     * vertex short.  There is no return value to see that through, so it is
     * inferred here -- an empty free list at the end of the chain, before
     * anything is given back.  A triangle can borrow at most seven nodes, so
     * on a 64-node pool this cannot fire unless something has leaked. */
    if (BrClipPoolCount() == 0)
        pDl->cClipStarved++;

    if (list.cVerts < 3) {
        /* 0x1001EF30: walk exactly cVerts nodes from the head returning the
         * pool ones, then give up. */
        BrClipVert *p = list.pHead;
        int k = list.cVerts;
        while (k-- > 0 && p != NULL) {
            BrClipVert *pN = p->pNext;
            BrClipPoolFree(p);
            p = pN;
        }
        pDl->cTriClipKilled++;
        return;
    }

    n = list.cVerts;
    if ((uint32_t)n > pDl->cClipVtxMax)
        pDl->cClipVtxMax = (uint32_t)n;
    if (n > BR_DL_CLIP_MAX) {
        /* DEVIATION: the original writes past its 0x224-byte frame.  See the
         * BR_DL_CLIP_MAX note in br_dl.h. */
        pDl->cClipOverflow++;
        n = BR_DL_CLIP_MAX;
    }

    /* 0x1001EF82: emit and free in one pass, walking the list from the head. */
    {
        BrClipVert *p = list.pHead;
        for (i = 0; i < n && p != NULL; ++i) {
            BrClipVert *pN = p->pNext;
            br_dl_clip_emit(pDl, p, &out[i]);
            BrClipPoolFree(p);
            p = pN;
        }
        while (i < list.cVerts && p != NULL) {   /* the clamped tail, if any */
            BrClipVert *pN = p->pNext;
            BrClipPoolFree(p);
            p = pN;
            ++i;
        }
    }

    /* 0x1001F095: exactly three vertices go to grDrawTriangle (0x100729EA),
     * anything else to grDrawPolygonVertexList (0x100729FC), which takes the
     * count and the base of the contiguous 0x3C-stride array.  BrDlSink has
     * only pfnTri, so the polygon becomes a fan -- which is what a Glide
     * convex-polygon call decomposes to anyway.  DEVIATION in form only. */
    if (pDl->sink.pfnTri) {
        for (i = 1; i + 1 < n; ++i)
            pDl->sink.pfnTri(pDl->sink.pUser, &out[0], &out[i], &out[i + 1]);
    }
    pDl->cTriClipOut += (uint32_t)(n - 2);
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
        /* 0x1001ED45: not trivially rejectable and not wholly inside, so the
         * triangle goes to the clipper -- and note the ARGUMENT ORDER, which
         * is the same (i0, i1, i2) the untouched path uses (0x1001ED53 pushes
         * &v[i2], &v[i1], &v[i0]; cdecl reverses that back to i0 first). */
        pDl->cTriClipped++;
        br_dl_clip_tri(pDl, a, b, c);
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
 * where the other has `sar 0x16` and an `and 0x3FF`.
 *
 * ALL FOUR PUT THE LOWER-RIGHT CORNER IN w0 AND THE UPPER-LEFT IN w1.  This
 * is stock RDP packing and the file used to get it backwards on the
 * untextured pair while getting it right on the textured pair, which is the
 * strongest evidence available that the untextured arm was the wrong one.
 * Read off the disassembly:
 *
 *   0x10021570 (0xE4) pushes, last-first,  (w1>>12, w1&0xFFF, w0>>12,
 *     w0&0xFFF, tile) into 0x100215C0 -- so w1 is the FIRST corner.
 *   0x1001E320 (0xF6) and 0x1001E720 (0xE1) push, last-first,
 *     (w1 hi, H - (w0 lo) - 1, (w0 hi) + 1, H - (w1 lo)) into 0x1001E380,
 *     whose four opening clamps are max, max, min, min -- so the w1 fields
 *     are the MINIMA (upper-left) and the w0 fields the maxima.
 *
 * THE THREE FIELD DECODES, each verbatim:
 *   0xE1  shl 20 / sar 20            signed 12-bit integer, no mask.  A -8
 *                                    corner is -8; masking it with 0xFFF
 *                                    makes it 4088.
 *   0xF6  shl 20 / sar 22 / and 0x3FF   net (w >> 2) & 0x3FF -- the sign
 *                                    extension is masked straight off again,
 *                                    so 0xF6's corners are UNSIGNED.
 *   0xE3/0xE4  and 0xFFF, and 0xE3 additionally shifts each field left two
 *                                    on the way in, so both reach 0x100215C0
 *                                    in 10.2 and both are unsigned.
 *
 * AND THE UNTEXTURED PAIR DO NOT PASS THE CORNERS ON RAW.  They flip Y
 * against 0x100A7518 and adjust both maxima by one:
 *     0x1001E380(ulx, H - lry - 1, lrx + 1, H - uly)
 * (`inc edi` at 0x1001E74E / 0x1001E363, `dec edx` at 0x1001E753 /
 * 0x1001E368).  That window is recorded in pDl->rectMinX..rectMaxY. */

static const uint8_t *br_dl_rect(BrDl *pDl, const uint8_t *p,
                                 int fTextured, int fFixed)
{
    uint32_t w0 = br_dl_w(p), w1 = br_dl_w(p + 4);
    int32_t ulx, uly, lrx, lry, tile = 0;

    if (fTextured) {
        /* 0x10021570 (0xE4) and 0x100219D0 (0xE3) are br_dlshared.c's -- ONE
         * body, both builds' addresses, and slice2_16.c's BrGbiTileRect /
         * BrGbiTileRectS now go through the same one.
         *
         * CORRECTED HERE.  This file used to decode both forms itself, and it
         * had the scaling BACKWARDS: it shifted 0xE4's corners RIGHT by two
         * and left 0xE3's alone.  The bytes say the opposite -- 0xE4 shifts
         * nothing (`and 0xFFF`, 0x1002157E onward) and 0xE3 shifts LEFT by
         * two (`shl edx,2` at 0x100219EE, `shr ecx,0xA / and 0x3FFC` at
         * 0x100219EB).  The comment that stood here even SAID "the integer
         * form multiplies by four", and the code below it did neither.
         *
         * The two mistakes cancelled at the sink, because a right shift of
         * the quarter-pixel form and no shift of the whole-pixel form both
         * land on whole pixels -- so nothing downstream could see it.  The
         * decode is now the original's, in quarter-pixels, and the
         * conversion to the pixels this file's sink deals in is done once,
         * below, where it is visible.
         *
         * DEVIATION: the sink is a port-level observer, not a transcription
         * of 0x100215C0, and it takes whole pixels because 0xE1 and 0xF6
         * deliver whole pixels.  The `>> 2` here is that adaptation and
         * nothing else; it discards the sub-pixel bits, which the original
         * keeps and passes on. */
        BrDlsTileRect r;
        BrDlsTileRectDecode(w0, w1, !fFixed, &r);
        tile = r.tile;
        ulx = r.ulx >> 2;
        uly = r.uly >> 2;
        lrx = r.lrx >> 2;
        lry = r.lry >> 2;
    } else if (fFixed) {
        /* 0xF6, 0x1001E320.  Unsigned 10.2. */
        lrx = (int32_t)((w0 >> 14) & 0x3FFu);
        lry = (int32_t)((w0 >> 2) & 0x3FFu);
        ulx = (int32_t)((w1 >> 14) & 0x3FFu);
        uly = (int32_t)((w1 >> 2) & 0x3FFu);
    } else {
        /* 0xE1, 0x1001E720.  Signed 12-bit integer. */
        lrx = br_dl_s12(w0 >> 12);
        lry = br_dl_s12(w0);
        ulx = br_dl_s12(w1 >> 12);
        uly = br_dl_s12(w1);
    }

    if (!fTextured) {
        pDl->rectMinX = ulx;
        pDl->rectMinY = pDl->cyScreen - lry - 1;
        pDl->rectMaxX = lrx + 1;
        pDl->rectMaxY = pDl->cyScreen - uly;
    }

    pDl->cRects++;
    if (pDl->sink.pfnRect)
        pDl->sink.pfnRect(pDl->sink.pUser, fTextured, tile, ulx, uly, lrx, lry);
    /* 0xE4 alone is three double-words. */
    return p + ((fTextured && fFixed) ? BR_DLS_TILERECT_E4_BYTES
                                      : BR_DLS_SKIP_BYTES);
}

/* WHAT IT DOES: draws a solid-colour rectangle whose corners were given in
 * quarter-pixel units. The corners are unsigned in this form. The port
 * records the resulting screen window but does not itself paint the pixels. */
/* @implements 0x1001E320 glide br_dl_fillF6 */
static const uint8_t *br_dl_fillF6(BrDl *d, const uint8_t *p)
{ return br_dl_rect(d, p, 0, 1); }
/* WHAT IT DOES: draws a solid-colour rectangle whose corners were given as
 * whole pixels. Unlike its quarter-pixel twin these corners are signed, so a
 * corner off the left of the screen really is negative. The port records the
 * window rather than painting it. */
/* @implements 0x1001E720 glide br_dl_fillE1 */
static const uint8_t *br_dl_fillE1(BrDl *d, const uint8_t *p)
{ return br_dl_rect(d, p, 0, 0); }
/* WHAT IT DOES: draws a textured rectangle straight onto the screen -- the
 * command behind heads-up display panels and menu artwork -- with its
 * corners given in quarter-pixel units. It swallows three commands' worth of
 * data, because the texture coordinates follow it. */
/* The decode is br_dlshared.c's BrDlsTileRectDecode, which carries this
 * address and BRD3D's 0x10021510. */
static const uint8_t *br_dl_texE4(BrDl *d, const uint8_t *p)
{ return br_dl_rect(d, p, 1, 1); }
/* WHAT IT DOES: the same textured screen rectangle with its corners given as
 * whole pixels, scaled up to quarter-pixels on the way through. */
/* Likewise 0x100219D0 / BRD3D 0x10021B80. */
static const uint8_t *br_dl_texE3(BrDl *d, const uint8_t *p)
{ return br_dl_rect(d, p, 1, 0); }

/* ---- 0xED / 0xE2 scissor  (0x1001EB50 / 0x1001EBC0, both Glide-only) --
 * TWO FUNCTIONS, TWO CONVENTIONS.  This file used to route both slots to one
 * handler using the 0xED decode, which silently divided 0xE2's corners by
 * four.  They are 103 and 97 bytes and differ in exactly the four shift/mask
 * pairs:
 *
 *   0x1001EB61  shr eax,0xE ; and eax,0x3FF   |  0x1001EBD1  shr eax,0xC
 *   0x1001EB70  shr ecx,2   ; and ecx,0x3FF   |              ; and eax,0xFFF
 *   0x1001EB84  shr ecx,0xE ; and ecx,0x3FF   |  0x1001EBE0  and ecx,0xFFF
 *   0x1001EB97  shr ebx,2   ; and ebx,0x3FF   |  ...
 *
 * Confirmed independently in BRD3D.dll, where the same two slots hold
 * 0x1001CDA0 (0xED: `shr 0xE / and 0x3FF` at 0x1001CDBB) and 0x1001CE70
 * (0xE2: `shr 0xC / and 0xFFF` at 0x1001CE8B).
 *
 * THE SHIFTS ARE `shr`, NOT `sar`: unlike 0xE1 the scissor corners are
 * UNSIGNED in both forms.  Preserved.
 *
 * THE Y FLIP IS THE POINT, and the roles of the two Y corners swap with it:
 *     0x105D17BC = ulx      = minX        0x105D17B8 = lrx      = maxX
 *     0x105D17C0 = H - lry  = minY        0x105CCFE0 = H - uly  = maxY
 * Established from the CONSUMER, not from the names: 0x1001E380 opens with
 * four clamps -- `jge` against 0x105D17BC, `jge` against 0x105D17C0, `jle`
 * against 0x105D17B8, `jle` against 0x105CCFE0 -- i.e. max, max, min, min.
 * The tail (0x1001EBAB) passes the same four to grClipWindow through the
 * thunk 0x100729D2 in the order (minx, miny, maxx, maxy).
 *
 * DUPLICATION, stated rather than left to be found: br_dlglide.c transcribes
 * these two addresses independently and correctly as BrDlGlScissorInt /
 * BrDlGlScissorFrac.  Two host definitions of one original address is what
 * CONVENTIONS.md's aliased-storage section is about; they no longer DISAGREE,
 * which is the part that was actively harmful, but the duplication is real
 * and outlives this pass.  Same for 0xE1 (BrDlGlFillRect), 0xDC, 0xDD and
 * 0xDF here, and for 0xF6/0xF7/0xFA/0xFB in br_dlcmd.c. */
static const uint8_t *br_dl_scissor(BrDl *pDl, const uint8_t *p, int fFrac)
{
    uint32_t w0 = br_dl_w(p), w1 = br_dl_w(p + 4);
    int32_t  H  = pDl->cyScreen;
    int32_t  ulx, uly, lrx, lry;

    if (fFrac) {                        /* 0xED, 0x1001EB50: 10.2 */
        ulx = (int32_t)((w0 >> 14) & 0x3FFu);
        uly = (int32_t)((w0 >> 2) & 0x3FFu);
        lrx = (int32_t)((w1 >> 14) & 0x3FFu);
        lry = (int32_t)((w1 >> 2) & 0x3FFu);
    } else {                            /* 0xE2, 0x1001EBC0: integer */
        ulx = (int32_t)((w0 >> 12) & 0xFFFu);
        uly = (int32_t)(w0 & 0xFFFu);
        lrx = (int32_t)((w1 >> 12) & 0xFFFu);
        lry = (int32_t)(w1 & 0xFFFu);
    }

    pDl->scisMinX = ulx;                /* 0x105D17BC */
    pDl->scisMaxY = H - uly;            /* 0x105CCFE0 */
    pDl->scisMaxX = lrx;                /* 0x105D17B8 */
    pDl->scisMinY = H - lry;            /* 0x105D17C0 */
    return p + 8;
}

/* 0x1001EBC0 -- opcode 0xE2, 97 bytes.  Integer fields. */
/* WHAT IT DOES: sets the clipping window -- the region of the screen
 * anything drawn afterwards is confined to -- from whole-pixel corners,
 * flipping the vertical axis because the game's display list counts down the
 * screen and the renderer counts up. */
/* @implements 0x1001EBC0 glide br_dl_scissorE2 */
static const uint8_t *br_dl_scissorE2(BrDl *pDl, const uint8_t *p)
{ return br_dl_scissor(pDl, p, 0); }

/* 0x1001EB50 -- opcode 0xED, 103 bytes.  10.2 fields. */
/* WHAT IT DOES: the same clipping-window setter for the quarter-pixel form
 * of the command. These really are two separate routines in the original,
 * and routing both through one decode quietly divides one form's corners by
 * four. */
/* @implements 0x1001EB50 glide br_dl_scissorED */
static const uint8_t *br_dl_scissorED(BrDl *pDl, const uint8_t *p)
{ return br_dl_scissor(pDl, p, 1); }

/* ---- 0xF2 G_SETTILESIZE  (0x1001EC30, SHARED) -----------------------
 * The D3D twin is 0x1001CF30, which slice2_16.c ports as BrGbiSetTileSize
 * -- it was called BrGbiSetScissor until this pass.  Same 178 bytes, same
 * slot 0xF2 in both builds' tables; one function under two addresses, and
 * ONE BODY: br_dlshared.c holds the decode and carries both addresses. */
/* WHAT IT DOES: tells the renderer which rectangle of a texture the next
 * drawings will use, and works out that rectangle's width and height in
 * texture pixels. Sign is preserved throughout, so a negative span stays
 * negative. */
static const uint8_t *br_dl_settilesize(BrDl *pDl, const uint8_t *p)
{
    BrDlsTileSize t;

    BrDlsTileSizeDecode(br_dl_w(p), br_dl_w(p + 4), &t);
    pDl->uls   = t.uls;
    pDl->ult   = t.ult;
    pDl->lrs   = t.lrs;
    pDl->lrt   = t.lrt;
    pDl->tileW = t.tileW;
    pDl->tileH = t.tileH;
    return p + BR_DLS_SKIP_BYTES;
}

/* ---- 0xF7 fill colour, 0xF8 fog colour ------------------------------
 * 0x1001E9F0 IS NOT A RAW STORE.  110 bytes, and every one of them is a
 * decode: it expands the LOW RGBA5551 half of w1 into four separate BYTE
 * globals, which the rectangle emitter 0x1001E380 reads at 0x1001E441 --
 * `mov bl,[0x105CCD40] / mov al,[0x105CCFD8] / mov cl,[0x105D17A0] /
 * mov dl,[0x105CE208]` -- on the arm taken whenever the latched combiner is
 * not the prim-colour row.  So 0xF7 is the colour 0xF6 fills with, and
 * keeping the word verbatim leaves that decode unwritten.
 *
 * The three colour channels use one idiom three times, e.g. for red at
 * 0x1001E9F8..0x1001EA09:
 *     mov ecx,w1 ; shr ecx,8      cl = bits 15:8
 *     mov edx,w1 ; shr edx,0xD    dl = bits 15:13
 *     xor dl,cl ; and dl,7 ; xor dl,cl
 * The three-instruction tail is the standard bitfield merge
 * `a ^ ((a ^ b) & mask)` == `(a & ~7) | (b & 7)`, i.e. the 5->8 widening
 * (v << 3) | (v >> 2) assembled out of one register pair.  Green is the same
 * with shifts 3 and 8; blue is `and cl,0xFE / shl cl,2` -- an EIGHT-BIT
 * shift, so bits 6 and 7 fall off the end, which is what leaves room for the
 * low three from `shr edx,3`.
 *
 * Alpha is `and cl,1 / neg cl / sbb ecx,ecx / and ecx,0xFF`: bit 0 spread to
 * all eight, giving 0 or 255, never 0 or 1.
 *
 * br_dlcmd.c transcribes this address independently as BrDlCmdFillColour; see
 * the duplication note on the scissor above. */
/* WHAT IT DOES: sets the colour that solid-colour rectangles are filled
 * with. The command carries the colour packed into fifteen bits plus one bit
 * of transparency, and this expands it back out to four full bytes -- the
 * transparency bit becoming either fully solid or fully clear, never
 * anything between. */
/* @implements 0x1001E9F0 glide br_dl_fillcolour */
static const uint8_t *br_dl_fillcolour(BrDl *pDl, const uint8_t *p)
{
    uint32_t w1 = br_dl_w(p + 4);
    uint8_t  hi, lo;

    hi = (uint8_t)(w1 >> 8);            /* bits 15:8  -- carries R << 3 */
    lo = (uint8_t)(w1 >> 13);           /* bits 15:13 -- carries R >> 2 */
    pDl->fillR = (uint8_t)((hi & 0xF8u) | (lo & 0x07u));

    hi = (uint8_t)(w1 >> 3);            /* bits 10:3  -- carries G << 3 */
    lo = (uint8_t)(w1 >> 8);            /* bits 10:8  -- carries G >> 2 */
    pDl->fillG = (uint8_t)((hi & 0xF8u) | (lo & 0x07u));

    hi = (uint8_t)((uint8_t)(w1 & 0xFEu) << 2);
    lo = (uint8_t)(w1 >> 3);            /* bits 5:3   -- carries B >> 2 */
    pDl->fillB = (uint8_t)(hi | (lo & 0x07u));

    pDl->fillA = (uint8_t)((w1 & 1u) ? 0xFFu : 0x00u);

    /* Port bookkeeping, not a global -- see br_dl.h. */
    pDl->fillColour = w1;
    return p + 8;                       /* 0x1001EA4E `add eax,8` */
}
static const uint8_t *br_dl_fogcolour(BrDl *pDl, const uint8_t *p)
{
    pDl->fogColour = br_dl_w(p + 4);
    return p + 8;
}

/* ---- 0xFA prim colour, 0xFB env colour  (0x1001EA80 / 0x1001E930) ---
 * ONE UNPACK IS WRONG FOR ONE OF THEM.  Both take the same four bytes in the
 * same order -- R (bits 31:24), G, B, A -- and both `fild` each into a float.
 * There the two part company, and this file used to divide both by 255:
 *
 *   0x1001EA80 (0xFA)  fild qword [esp+4] ; fstp dword [dest]
 *                      -- four times, at 0x1001EAA0 / EABE / EADC / EAF3.
 *                      NO fmul anywhere in the 138 bytes.  Prim is 0..255.
 *   0x1001E930 (0xFB)  fild ; fstp scratch ; fld scratch ;
 *                      fmul [0x10077400] ; fstp dest
 *                      -- four times, and 0x10077400 is 0x3B808081, the float
 *                      nearest 1/255.  Env is 0..1.
 *
 * Three independent corroborations that 0..255 is the reading and not an
 * oversight: BR_DL_COLOUR_MAX (0x10077418) is 255.0f and is what the lit
 * colour is clamped to two functions away; 0x1001E380 passes all four of
 * 0xFA's destinations through _ftol (0x10074560) into single BYTES at
 * 0x1001E412..0x1001E43B, which only makes sense for 0..255; and the
 * "lights off" fallback at 0x10022BCC copies three of them straight into a
 * vertex colour in a pipeline whose ceiling is 255.
 *
 * The spill through a stack slot between 0xFB's fild and its fmul is a real
 * rounding to float.  Reproduced rather than folded, as br_dlcmd.c does.
 *
 * br_dlcmd.c transcribes both addresses independently as BrDlCmdPrimColour /
 * BrDlCmdEnvColour; see the duplication note on the scissor above. */
/* WHAT IT DOES: sets the primitive colour, the flat colour used where a
 * drawing is not taking its colour from a texture or from vertex shading.
 * The four channels are kept on a 0-to-255 scale, unlike the environment
 * colour below. */
/* @implements 0x1001EA80 glide br_dl_prim */
static const uint8_t *br_dl_prim(BrDl *pDl, const uint8_t *p)
{
    uint32_t v = br_dl_w(p + 4);

    /* 0x105D17A4, 0x105D17B4, 0x105CE2D0, 0x105CD9F0.  The first three are
     * also br_dl_light_vertex's numlights==0 fallback -- one object. */
    pDl->prim[0] = (float)(int32_t)((v >> 24) & 0xFFu);
    pDl->prim[1] = (float)(int32_t)((v >> 16) & 0xFFu);
    pDl->prim[2] = (float)(int32_t)((v >> 8) & 0xFFu);
    pDl->prim[3] = (float)(int32_t)(v & 0xFFu);
    return p + 8;
}
/* WHAT IT DOES: sets the environment colour, the second flat colour the
 * pixel combiner can mix in. Unlike the primitive colour this one is scaled
 * down to a 0-to-1 range as it is stored, which is a real difference between
 * the two commands and not an oversight. */
/* @implements 0x1001E930 glide br_dl_env */
static const uint8_t *br_dl_env(BrDl *pDl, const uint8_t *p)
{
    uint32_t v = br_dl_w(p + 4);
    const float k = 1.0f / 255.0f;    /* 0x10077400 == 0x3B808081 exactly */

    pDl->env[0] = (float)(int32_t)((v >> 24) & 0xFFu) * k;
    pDl->env[1] = (float)(int32_t)((v >> 16) & 0xFFu) * k;
    pDl->env[2] = (float)(int32_t)((v >> 8) & 0xFFu) * k;
    pDl->env[3] = (float)(int32_t)(v & 0xFFu) * k;
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
     * 0x1001E8FB uses it to swap dispatch-table slot 0x04 (0x100A9A68)
     * between the two LIT VERTEX TRANSFORMS 0x10021C70 and 0x100221D0 -- and
     * only ever between those two, so it can never install a lit transform
     * over an unlit one.  BrDlVtxRoutine reads pDl->fDecal for exactly this.
     * PART 4 has the rest. */
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
        s_aTable[i] = br_dl_skip_h;
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
    s_aTable[0xE2] = br_dl_scissorE2;    /* 0x1001EBC0 -- integer */
    s_aTable[0xE3] = br_dl_texE3;
    s_aTable[0xE4] = br_dl_texE4;
    s_aTable[0xED] = br_dl_scissorED;    /* 0x1001EB50 -- 10.2    */
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
    return (op < 256u) && (s_aTable[op] != br_dl_skip_h);
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
     * only for lists that stay inside [pList, pList+cbMax] -- a G_DL into
     * another buffer legitimately leaves the range, so the bound is applied
     * only while the cursor is still inside it.
     *
     * THE END OF THE RANGE IS INCLUSIVE, and it has to be.  This test used to
     * read `p < pEnd`, which is FALSE at exactly the address the last
     * in-range handler leaves the cursor at -- so the one position the guard
     * exists for was the one position it skipped, and `p[3]` then read a
     * fourth byte past the buffer.  Landing on pEnd is the ordinary way a
     * well-formed list without a G_ENDDL runs out (a 0xE4 advances 0x18, so
     * it is easy to step over a terminator and finish exactly on the end).
     * ASan found it; `<= pEnd` is the whole fix, and it does not change the
     * G_DL case at all, because a cursor in another buffer is either below
     * pList or above pEnd and skips the guard either way. */
    while (p != NULL) {
        unsigned op;
        if (p >= pList && p <= pEnd && (size_t)(pEnd - p) < 8u)
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
    int   fLit  = (pR->pDl != NULL) && pR->pDl->fVtxLit;
    float scale = (pR->pDl != NULL) ? BrDlColourScale(pR->pDl) : 1.0f;

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
            if (fLit) {
                /* A lighting transform ran: the slots are Glide iterated
                 * colours, 0..255. */
                r *= scale; g *= scale; bl *= scale;
            } else {
                /* Nothing lit these, so the slots hold the Vtx's trailing
                 * bytes scaled by 1/128 and are in [-1, 1] -- a NORMAL, for
                 * every model this port has (br_dl.h records the
                 * measurement).  Fold to [0, 1] so the output is a picture
                 * rather than a clamp artefact.  This is a property of the
                 * reference rasteriser, not of the original: the original
                 * would hand these to Glide as 0..255 and get black. */
                r = r * 0.5f + 0.5f; g = g * 0.5f + 0.5f; bl = bl * 0.5f + 0.5f;
            }
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
    pRas->pDl        = pDl;
    pDl->sink.pUser  = pRas;
    pDl->sink.pfnTri = br_ras_tri;
}

/* ── Ghidra-matched functions ─────────────────────────── */
#ifdef BR_MATCHING_BUILD
typedef int (*funcptr)();
extern funcptr DAT_118ed1cc;
extern funcptr DAT_118ed1d0;

/* WHAT IT DOES: display-list opcode: call a function pointer with the low 24 bits of the command word, advance by the stride. */
/* @implements 0x1001E2E0 glide BrDlOpDispatch1 */

unsigned int * BrDlOpDispatch1(unsigned int *param_1)

{
  (*DAT_118ed1cc)(*param_1 & 0xffffff);
  return param_1 + param_1[1] * 2;
}

/* WHAT IT DOES: display-list opcode: call a function pointer with the command word and its argument, advance by 2. */
/* @implements 0x1001E300 glide BrDlOpDispatch2 */

unsigned int * BrDlOpDispatch2(unsigned int *param_1)

{
  (*DAT_118ed1d0)(*param_1 & 0xffffff,param_1[1]);
  return param_1 + 2;
}

#endif /* BR_MATCHING_BUILD */
