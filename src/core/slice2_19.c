/* slice2_19.c -- decompiled from BRD3D.dll, range 0x10033CB1 .. 0x10036C00.
 *
 * See slice2_19.h for the recovered layouts, the DEVIATION list, the skipped
 * functions and the gotchas. Everything here was traced from
 * work/slice2/agent19.asm.
 *
 * x87 note: every fcomp/fnstsw pair in this range was decoded through the
 * flag mapping C0 = ah bit 0, C2 = ah bit 2, C3 = ah bit 6, so
 *      test ah,0x01 / jne  -> ST0 <  mem, or unordered
 *      test ah,0x41 / jne  -> ST0 <= mem, or unordered
 * and the C below uses the negated-comparison forms that reproduce the
 * unordered case as well, not just the ordered one.
 */
#ifdef BR_MATCHING_BUILD
/* Header prototype is cdecl (this, r, g, b).  Original is thiscall with
 * ret 0xC; hide that prototype so the definition can take the struct-arg
 * __fastcall shape that reproduces it. */
#define BrRgbSinkSet BrRgbSinkSet_hdr
#endif
#ifdef BR_MATCHING_BUILD
/* slice2_19.h / br_seg.h declare these cdecl with a leading state pointer the
 * originals do not have.  Hide those prototypes so BrModelLoad can call them
 * with the shapes the bytes show. */
#define BrSub100088B0 BrSub100088B0_cdecl
#define BrSegSetBases BrSegSetBases_cdecl
#endif
#include "slice2_19.h"
#ifdef BR_MATCHING_BUILD
#undef BrSub100088B0
#undef BrSegSetBases
typedef struct { void *p; } BrModelLoadArg;
extern int g_brModelMgr;                        /* 0x10AC0810 */
void * __fastcall BrSub100088B0(void *pThis, BrModelLoadArg a,
                                BrModelLoadArg b);
void BrSegSetBases(uint32_t n64Base, uint32_t hostBase);
#endif
#ifdef BR_MATCHING_BUILD
#undef BrRgbSinkSet
#endif

#include <string.h>

/* ================================================================== */
/* Globals the original reaches by absolute address                    */
/* ================================================================== */

float g_BrK08F514 = 2.0f;          /* DERIVED   */
/* MEASURED, not assumed any more.  Both constants were read straight out of
 * the two images' .rdata: BRD3D.dll 0x1008F518 / 0x1008F51C and BRGlide.dll
 * 0x100774E0 / 0x100774E4 (the same two floats the Glide twin 0x1002D5CF /
 * 0x1002D5DD multiplies by) hold the byte patterns ABAAAA3F and E02E6542,
 * i.e. 1.3333334f and 57.2957764f -- 4/3 and 180/pi.  BrCamMatrixSetup's
 * fovy line is therefore
 *      fovy_degrees = a2 * (4/3) * (a5/a4) * (180/pi)
 * with a2 in RADIANS; at a 4:3 viewport the middle two factors cancel.  With
 * the assumed 1.0f pair the field of view came out around 0.75 DEGREES, so
 * this was not a cosmetic gap: nothing rendered through this camera could
 * have looked right. */
float g_BrK08F518 = 1.3333333730697632f;   /* MEASURED  4/3    */
float g_BrK08F51C = 57.2957763671875f;     /* MEASURED  180/pi */
/* All eight below are now MEASURED out of BRD3D.dll .rdata, byte pattern in
 * the comment.  Six of the eight confirmed the guess exactly; 0x1008F518 and
 * 0x1008F548 did not.  Reading them cost one script. */
float g_BrK08F520 = 2.5f;          /* MEASURED 40200000 */
float g_BrK08F524 = 5.0f;          /* MEASURED 40A00000 */
float g_BrK08F52C = 4096.0f;       /* MEASURED 45800000 */
float g_BrK08F530 = 1.0f / 128.0f; /* MEASURED 3C000000 == 0.0078125 exactly */
float g_BrK08F534 = 0.5f;          /* MEASURED 3F000000 */
/* MEASURED, was ASSUMED 1/80 and WRONG BY 12.5%.  0x1008F548 holds 3C6A0EA1
 * == 0.0142857144f == 1/70, and it scales EVERY analog axis of EVERY frame
 * (0x10035EE1 / 0x10035EF5 / 0x10035F13, the three fmul sites in
 * BrPadTranslate).  The old reading's evidence was that the digital arm
 * synthesises +/-0x50 (+/-80), so 1/80 lands exactly on +/-1.  The bytes
 * refute the inference rather than the observation: 80 * (1/70) is
 * 1.14285719f, and the +/-1 clamp two instructions later (0x1008F54C /
 * 0x1008F550) cuts it back to exactly +/-1.  The digital path was designed to
 * SATURATE, so it produces the same +/-1 under either constant and could
 * never have discriminated between them.  What it does discriminate is the
 * ANALOG path, which is the one that runs while driving. */
float g_BrK08F548 = 0.0142857144f; /* MEASURED 3C6A0EA1 == 1/70 */
float g_BrK08F54C =  1.0f;         /* MEASURED 3F800000 */
float g_BrK08F550 = -1.0f;         /* MEASURED BF800000 */

BrVec3 g_BrCamEye;
BrVec3 g_BrCamCentre;
BrVec3 g_BrCamExtentR;
BrVec3 g_BrCamExtentU;
BrVec3 g_BrCamCentreCopy;
BrVec3 g_BrCamCorner0;
BrVec3 g_BrCamCorner1;
BrVec3 g_BrCamCorner2;
BrVec3 g_BrCamCorner3;
float  g_BrCamDist;
float  g_BrCamFovIn;
/* 0x100AA8B4, 0x100AC300, 0x106C661C, 0x106C6624 and 0x106C2CFC are defined
 * ONCE, in port/src/br_data.c -- see the ALIAS RESOLVED notes in slice2_19.h.
 * Three of them carry non-zero initialisers this module never had. */

BrMat4    g_BrViewMat;
BrMat4    g_BrProjMat;
BrMat4    g_BrProjMatFixed;
BrMat4    g_BrCurMat;
uint16_t  g_BrPerspNorm;
float     g_BrCamFar;
float     g_BrCamNear;
void     *g_BrMtxSlot;
uint32_t *g_BrGfxPtr;
BrPool   *g_BrPool;

int32_t     g_Br0B380C;
int32_t     g_Br6C666C;
/* g_BrDlTableA is an incomplete extern array (see slice2_19.h): the object
 * at 0x100AA8D8 is the table, so there is nothing to define here. */

int32_t g_BrCarCount;
void  (*g_BrGfxSubmit)(uint32_t dl);
void  (*g_BrGfxSubmitB)(uint32_t p);


const unsigned char *g_BrPadModeBytes;
int32_t              g_Br6909B4;
const void          *g_BrPadHookFn;

/* 0x10019A70 is the (unclaimed, 11 KB) race step.  The original passes its
 * address as an IMMEDIATE, so the matching build needs a function symbol,
 * not a pointer variable.  The port keeps the variable. */
#ifdef BR_MATCHING_BUILD
extern void BrRaceStep_10019A70(void);
#define BR_PAD_RACE_STEP ((const void *)BrRaceStep_10019A70)
#else
#define BR_PAD_RACE_STEP g_BrPadHookFn
#endif
int32_t g_br5CCB5C;   /* 0x105CCB5C -- used only by this module; defined here
                       * so the port links (matching pins the address via
                       * config/globals.csv, unaffected by this BSS def). */

void  (*g_BrModelFixup)(uint32_t *pSlot);
void *(*g_BrModelDeref)(uint32_t slot);
BrSegMap *g_BrSegMap;

void *g_BrLogArg;

/* ================================================================== */
/* 1. Camera / matrix set-up                                          */
/* ================================================================== */


/* 0x10033E83 */
/* 0x10033F7E  Both parameters are dead; see the header. */
/* 0x1003407D */
/* ================================================================== */
/* 2. Display-list segment fixup                                      */
/* ================================================================== */

/* 0x10035060 */
/* 0x10035089 */
/* 0x1003445A */
/* ================================================================== */
/* 3. Per-car RDP mode words                                          */
/* ================================================================== */

/* The original writes both halfwords byte-swapped (big-endian, for the RDP).
 * Transcribed as the same shift/mask pair it uses, not as a memory swap. */
static uint16_t BrSwapHalf(uint16_t v)
{
    return (uint16_t)(((uint32_t)v << 8 & 0xFF00u) | ((uint32_t)v >> 8 & 0xFFu));
}

/* 0x10035CA0  __thiscall, ret 0xC. Only the low byte of each argument. */
/* 0x100350EE */
/* 0x10035452 */
/* ================================================================== */
/* 4. Keyframe vertex animation                                       */
/* ================================================================== */

/* 0x10035585 */
/* 0x1007C8A0 __ftol -- truncate toward zero, low dword before any clamp.
 *
 * DEVIATION: C's (int) cast is undefined for values outside int range and
 * for NaN, and BrAnimUpdate's three documented divide-by-zero paths do
 * produce those. The original's x87 FISTP stores the integer indefinite
 * 0x80000000 there, so the port does the same explicitly. In every one of
 * those paths the two brackets are the SAME keyframe, so the resulting
 * garbage frac is multiplied by a zero delta and never reaches the output. */
static int BrFtol(float f)
{
    if (!(f > -2147483649.0f && f < 2147483648.0f))
        return (int)0x80000000L;
    return (int)f;
}

/* lo + (((hi - lo) * frac) >> 12), truncated back to the source width. The
 * truncation is a `movsx ax` / `movsx al` in the original and does wrap. */
static int BrAnimLerp16(int lo, int hi, int frac)
{
    return (int)(int16_t)((((hi - lo) * frac) >> 12) + lo);
}

static int BrAnimLerp8(int lo, int hi, int frac)
{
    return (int)(int8_t)((((hi - lo) * frac) >> 12) + lo);
}

/* 0x1003563A */
/* WHAT IT DOES: advances every animation in a set by one frame's worth of
 * time and works out the shape of the model in between its stored key poses,
 * blending each corner point and its surface direction between the pose
 * before and the pose after. Animations that have run off the end either stop,
 * jump back to the start or turn round and play backwards, according to how
 * they were set up. Several of the stopping cases end up interpolating
 * between a pose and itself, which divides by zero -- harmless because the
 * result is then multiplied by no difference at all, and preserved. */
/* PROGRESS NOTE (2026-09-03): this is an /Od function and was written in the
 * /O2 idiom; it was parked as a wall, wrongly -- the park predates the
 * "diff stranded in an /Od run" screen in docs/VC5-IDIOMS.md. Two source
 * facts fixed so far and the first 0x19 bytes now match exactly under /Od:
 * the guard is a WRAPPED body, not an early return, and there is NO pList
 * local -- the original re-derefs pSet->pList at every use.
 *
 * WHAT IS LEFT is slot homing: `sub esp,0x60` against the original's 0x64,
 * so one local short, and the ones that exist are in the wrong slots (the
 * original puts the count at ebp-0x2c and the loop counter at ebp-8).  /Od
 * homes locals by an internal NAME hash rather than declaration order -- see
 * BrCarGfxReadColour below, where the single-letter names were found
 * empirically -- so this needs a naming pass over ~24 locals and is its own
 * session. Do NOT re-park it as a coloring wall; it is not one. */
/* @implements 0x1003563A d3d BrAnimUpdate */
void BrAnimUpdate(BrAnimSet *pSet)
{
    int32_t i, n;

    /* Wrapped, not an early return: the original's guard is a single near
     * `je` to the epilogue (0x1002ECF8 -> 0x1002F230), where `return` emits a
     * short branch over a jump. Same lever as BrCarGfxSetColour.
     *
     * NO pList local: the original re-derefs pSet->pList at every use, which
     * is what /Od does with a member expression. Caching it costs a slot and
     * shifts every displacement. */
    if (pSet->pList != NULL) {

    n = pSet->pList->n;

    for (i = 0; i < n; i++) {
        BrAnimTrack     *pT = pSet->pList->a[i];
        const BrAnimKey *pLo;
        const BrAnimKey *pHi;
        const int16_t   *pS16;
        const int16_t   *pE16;
        const int8_t    *pS8;
        const int8_t    *pE8;
        BrAnimVtx       *pOut;
        float t, u, span, lim;
        int32_t k, m, cVerts;
        int frac;

        if ((pT->flags & 4u) != 0) {
            /* ---- playing in reverse (0x1003569A) ---- */
            pT->t -= g_BrAnimDt;
            t = pT->t;

            if (!(t >= pT->tLo)) {
                /* 0x1003595E -- reflect off the low end, or stop */
                if ((pT->flags & 1u) == 0)
                    continue;
                t = g_BrK08F514 * pT->tLo - t;
                pT->t    = t;
                pT->flags = (uint16_t)(pT->flags & 0xFFFBu);
                pT->iKey  = 0;
                goto search;
            }
            if (t >= pT->tHi)
                continue;
            goto search;
        }

        /* ---- playing forward (0x100359AE) ---- */
        pT->t += g_BrAnimDt;
        t = pT->t;

        if (!(t >= pT->tLo)) {
            /* GOTCHA: both brackets become aKeys[0], so the interpolation
             * below divides by zero. Original behaviour. */
            pHi = pT->aKeys[0];
            pLo = pT->aKeys[0];
            t = 0.0f;
            goto interp;
        }

        if (!(t >= pT->tHi)) {
            if (!(t >= pT->tLo))
                continue;
            goto search;
        }

        /* 0x10035A21 -- past the end */
        if ((pT->flags & 1u) == 0) {
            /* GOTCHA: same degenerate bracket as above. The original indexes
             * +0x1C + cKeys*4, i.e. the LAST key; with cKeys == 0 it would
             * read the `t` field as a pointer. */
            pHi = pT->aKeys[pT->cKeys - 1];
            pLo = pT->aKeys[pT->cKeys - 1];
            t = 0.0f;
            goto interp;
        }

        span = pT->tHi - pT->tLo;
        lim  = pT->tHi + span;

        if ((pT->flags & 2u) != 0) {
            span = (pT->tHi - pT->tLo) * g_BrK08F514;
            lim  = pT->tHi + span;
            while (t > lim)
                t -= span;
            span = span * g_BrK08F534;
            lim  = lim - span;
            /* GOTCHA: this falls into the PLAIN wrap loop, which then also
             * runs the plain tail -- the reverse bit is never set. */
            if (t > lim)
                goto wrap_plain;
            t = g_BrK08F514 * pT->tHi - t;
            pT->t = t;
            pT->flags = (uint16_t)(pT->flags | 4u);
            goto reset_key;
        }

        span = pT->tHi - pT->tLo;
        lim  = pT->tHi + span;

    wrap_plain:
        while (t > lim)
            t -= span;
        t = t - (pT->tHi - pT->tLo);
        pT->t = t;

    reset_key:
        pT->iKey = 0;

    search:
        /* GOTCHA: k is not re-tested against cKeys before the load, so a
         * track whose last key time is <= t reads aKeys[cKeys]. */
        k = pT->iKey;
        while (k < pT->cKeys) {
            if (pT->aKeys[k]->t > t)
                break;
            k++;
        }
        pHi = pT->aKeys[k];
        k--;
        pLo = pT->aKeys[k];

    interp:
        u    = (t - pLo->t) / (pHi->t - pLo->t);
        frac = BrFtol(u * g_BrK08F52C);     /* 0x1007C8A0 */

        cVerts = (int32_t)pT->cVerts;
        pS16 = (const int16_t *)((const char *)pLo + 4);
        pE16 = (const int16_t *)((const char *)pHi + 4);
        pS8  = (const int8_t  *)(pS16 + (size_t)cVerts * 3);
        pE8  = (const int8_t  *)(pE16 + (size_t)cVerts * 3);
        pOut = pT->pOut;

        for (m = 0; m < cVerts; m++) {
            pOut[m].x = (float)BrAnimLerp16(pS16[0], pE16[0], frac);
            pOut[m].y = (float)BrAnimLerp16(pS16[1], pE16[1], frac);
            pOut[m].z = (float)BrAnimLerp16(pS16[2], pE16[2], frac);

            pOut[m].nx = (float)BrAnimLerp8(pS8[0], pE8[0], frac) * g_BrK08F530;
            pOut[m].ny = (float)BrAnimLerp8(pS8[1], pE8[1], frac) * g_BrK08F530;
            pOut[m].nz = (float)BrAnimLerp8(pS8[2], pE8[2], frac) * g_BrK08F530;

            pS16 += 3;
            pE16 += 3;
            pS8  += 3;
            pE8  += 3;
        }
    }
    }
}

/* ================================================================== */
/* 5. Controller translation                                          */
/* ================================================================== */

/* One of the two identical ramp steps at 0x10035E9C. */
static void BrPadRamp(const int32_t *pEnable, int32_t *pCur, const int32_t *pLim)
{
    if (*pEnable == 0)
        return;
    if (*pCur < *pLim && g_Br6909B4 == 0)
        *pCur = *pCur + 2;
}

/* Reproduces `if (v > hi) v = 1; else if (v < lo) v = -1;` including the
 * unordered case, which the original routes to the LOW assignment. */
/* The two comparisons are 0x10035EFD / 0x10035F2B and their siblings.  The
 * upper one is `fcomp ; test ah,0x41 ; je <clamp>`, so the clamping arm is
 * taken only when NEITHER C0 nor C3 is set -- ordered and strictly greater.
 * NaN sets both and takes the other arm, which the positive `v > hi` also
 * does, so the positive form is exact here.  The lower one is
 * `test ah,1 ; jne <clamp>`, where NaN DOES clamp, hence the negated form.
 *
 * Mutation note: rewriting the upper test as `v >= g_BrK08F54C` survives the
 * suite, and that is an equivalent mutation rather than missing coverage.
 * The two differ only at v == g_BrK08F54C, and there the clamp returns 1.0f
 * while falling through returns v, which IS 1.0f.  A threshold equal to its
 * own clamp target cannot distinguish `>` from `>=`. */
static float BrPadClamp(float v)
{
    if (v > g_BrK08F54C)
        return 1.0f;
    if (!(v >= g_BrK08F550))
        return -1.0f;
    return v;
}

/* 0x10035FC0  __thiscall */
/* WHAT IT DOES: splits a set of pressed buttons into "newly pressed this
 * frame" and "still held from last frame", which is how the game tells a tap
 * from a hold. */
/* @implements 0x10035FC0 d3d BrBitEdgeSplit */
/* @n64 0x80255934 located */
/* Both members are loaded into registers up front, b BEFORE a
 * (`mov edx,[ecx+4]; mov eax,[ecx]`), so both are locals and b is declared
 * first; the earlier spelling re-dereferenced pPair->b twice and cost 11
 * bytes.
 *
 * RESIDUE 4 bytes, 25 against 21: the original ANDs into its copy of ~b
 * (`mov esi,edx; not esi; and esi,eax`, three registers), while VC5
 * canonicalises `~b & a` to put the plain operand on the left and so copies
 * a as well (`mov esi,edx; mov edi,eax; not esi; and edi,esi`), paying a
 * push/pop of edi.  Probed and ruled out, do not re-run: `a & ~b`, naming
 * `~b` as a local, compounding it (`nb &= a`), hoisting both results into
 * temps before the stores, and re-dereferencing one member.  Writing the
 * b-store FIRST does come out 21 bytes exactly -- but it swaps which store
 * leads, and the original stores [ecx] first, so that is a lower byte count
 * for a less faithful source, not a match. T3a. */
void BR_THISCALL1 BrBitEdgeSplit(BrBitPair *pPair)
{
    uint32_t b = pPair->b;
    uint32_t a = pPair->a;

    pPair->a = ~b & a;
    pPair->b = a & b;
}

/* ================================================================== */
/* 6. Big-endian model fixup                                          */
/* ================================================================== */

/* `swap byte n with byte n+3, byte n+1 with byte n+2` -- what the original
 * spells out for every 32-bit slot it is about to hand to the fixup. */
static void BrRev4(void *pv)
{
    unsigned char *p = (unsigned char *)pv;
    unsigned char t;

    t = p[0]; p[0] = p[3]; p[3] = t;
    t = p[1]; p[1] = p[2]; p[2] = t;
}

static void BrRev2(void *pv)
{
    unsigned char *p = (unsigned char *)pv;
    unsigned char t = p[0];

    p[0] = p[1];
    p[1] = t;
}

/* The other form the original uses for 32-bit fields: compose the value
 * byte-wise MSB-first and store it natively. Identical to BrRev4 on the
 * little-endian host the original ran on; kept distinct because the two are
 * genuinely different instruction sequences. */
static void BrRdBe32(void *pv)
{
    const unsigned char *p = (const unsigned char *)pv;
    uint32_t v = ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16)
               | ((uint32_t)p[2] <<  8) | (uint32_t)p[3];

    memcpy(pv, &v, 4);
}

static uint32_t BrLd32(const void *pv)
{
    uint32_t v;

    memcpy(&v, pv, 4);
    return v;
}

static uint16_t BrLd16(const void *pv)
{
    uint16_t v;

    memcpy(&v, pv, 2);
    return v;
}

/* 0x10036C00 */
/* WHAT IT DOES: makes a model file usable after loading. The game's art was
 * authored for a machine that stores numbers the other way round, so every
 * number in the file has to be turned back to front, and every address in it
 * corrected to where the data now sits -- header, geometry, animation frames
 * and all. Each finished piece is then handed to the renderer. */
/* @implements 0x10036C00 d3d BrModelSwap */
#ifdef BR_MATCHING_BUILD
/* RESIDUE 1062 vs 1053 bytes, 371 vs 368 instructions, register-blind 8+11
 * (from 149+285 when this was first opened, and 13+23 before the leaf-loop
 * step below -- see the git log).  The 2-byte reversal being a halfword
 * COMPOSE AND ONE 16-BIT STORE, rather than two byte stores, was one big one:
 * the original has seven `mov word ptr` stores, exactly one per BrRev2 site.
 *
 * The other was the LEAF LOOP's bound.  Hoisting `3 * item->m` into an
 * `nHalf` local turned the loop into a count-DOWN (`dec`/`jne`) walking a
 * negative displacement, and freed a register so `j` never spilled.  The
 * original RE-READS the bound every pass -- it reloads PITEM from the slot,
 * loads item->m, `lea edx,[edx+edx*2]` and compares -- which is what puts `j`
 * in the THIRD stack slot and makes the frame `sub esp,0xc` rather than 8.
 * Spelling the bound in the for-condition closed the frame, the loop rotation
 * and the whole body: the leaf loop is now instruction-for-instruction exact.
 *
 * WHAT IS LEFT, all measured against this baseline:
 *  - 2 insns in the RECORD loop's guard.  The original walks that loop on a
 *    pointer biased +2 (`lea esi,[ebp+0xa]`) and rematerialises pRec each
 *    pass (`mov eax,[esi-2]` / `lea edi,[esi-2]` / `test eax,eax`), where we
 *    fold to `cmp dword ptr [esi],0`.  It then uses edi for offsets 0..3 and
 *    the two tail reloads, esi for 4..0x13.
 *  - 1 insn: the fixup argument, orig `mov ecx,edi` + `add ecx,edx` against
 *    our `mov ecx,[esi]` + `add ecx,edi`.  A register copy.
 *  - the `off = 0x20` init: orig emits `mov ebx,0x20` in the leaf loop's
 *    PREHEADER (after the `k <= 0` guard), we emit it before.  Same count.
 *  - BrRev4's two stores per pair come out in the opposite order at 2 of the
 *    sites -- and the sites disagree with each other, so it is scheduling.
 *  - ~30 instructions differ only as `[edi+eax]` vs `[eax+edi]` (SIB base and
 *    index exchanged).  Register-blind-invisible, byte-visible.
 * PROBED AND DEAD, do not re-run: single-temp and load-both-first spellings
 * of the byte swap (two byte stores never merge, however the temps are
 * arranged); the same through a `p_` pointer temp (better RAW, worse size and
 * instruction count); giving the leaf loop's doubled subscript its own local
 * stepped by 2 (register-blind 36 -> 49); the high-byte-down spelling of
 * BrRev4 (`t=p[3]; p[3]=p[0]; p[0]=t;` -- fixes the head site, 8+11 -> 16+21
 * overall); flipping BrRev2's `|` operands, `off` moved into the for-init,
 * writing PSLOT offset-first as `4 + 4*iItem + PBLOCK`, and giving the record
 * loop its own `unsigned char *p = pRec` with the guard through a local --
 * all four are INERT, VC5 canonicalises them to the identical bytes.
 *
 * MACROS, not statics -- MSVC5 will not inline a static with more than one
 * caller, so every BrRev/BrLd here was a `call` the original does not have.
 * Scoped to BrModelSwap with #undef below so the other users of these
 * helpers keep whatever shape they already match with. */
#define BrRev4(pv) do { unsigned char t_; \
    t_ = ((unsigned char *)(pv))[0]; ((unsigned char *)(pv))[0] = ((unsigned char *)(pv))[3]; ((unsigned char *)(pv))[3] = t_; \
    t_ = ((unsigned char *)(pv))[1]; ((unsigned char *)(pv))[1] = ((unsigned char *)(pv))[2]; ((unsigned char *)(pv))[2] = t_; } while (0)
#define BrRev2(pv) (*(uint16_t *)(void *)(pv) = (uint16_t)( \
    ((uint16_t)((unsigned char *)(pv))[0] << 8) | (uint16_t)((unsigned char *)(pv))[1] ))
#define BrRdBe32(pv) do { uint32_t v_ = \
      ((uint32_t)((unsigned char *)(pv))[0] << 24) | ((uint32_t)((unsigned char *)(pv))[1] << 16) \
    | ((uint32_t)((unsigned char *)(pv))[2] << 8)  | (uint32_t)((unsigned char *)(pv))[3]; \
    *(uint32_t *)(void *)(pv) = v_; } while (0)
#define BrLd32(pv) (*(const uint32_t *)(const void *)(pv))
#define BrLd16(pv) (*(const uint16_t *)(const void *)(pv))
/* DIRECT calls, not indirect: the original has nine `call rel32` and one
 * `call [mem]`; the two fixup/deref hooks are ordinary functions here. */
void  BrModelFixupDirect(uint32_t *pSlot);
void *BrModelDerefDirect(uint32_t slot);
#define g_BrModelFixup BrModelFixupDirect
#define g_BrModelDeref BrModelDerefDirect
#endif
void BrModelSwap(void *pImage)
{
    unsigned char *pHdr = (unsigned char *)pImage;
    unsigned char *pRec;
    uint32_t iRec;

    /* Header +0x00 and +0x02: two independent big-endian halfwords, and the
     * original does the SECOND one first -- its word store to +2 precedes the
     * one to +0. */
    BrRev2(pHdr + 2);
    BrRev2(pHdr + 0);

    /* GOTCHA: tested BEFORE the byte reversal. Only works because zero is a
     * palindrome. */
    if (BrLd32(pHdr + 4) != 0) {
        int32_t iItem;
#define PBLOCK (*(unsigned char **)(void *)(pHdr + 4))
#define PSLOT  (PBLOCK + 4 + 4 * (size_t)iItem)
#define PITEM  (*(unsigned char **)(void *)PSLOT)

        BrRev4(pHdr + 4);
        g_BrModelFixup((uint32_t *)(pHdr + 4));

        BrRdBe32(PBLOCK);                  /* block->n */

        /* The original re-reads the count from the block on every pass. */
        for (iItem = 0; iItem < (int32_t)BrLd32(PBLOCK); iItem++) {
            int32_t iLeaf;
            size_t off;

            BrRev4(PSLOT);
            g_BrModelFixup((uint32_t *)PSLOT);

            BrRdBe32(PITEM + 0x00);        /* item->m */
            BrRev4  (PITEM + 0x04);
            g_BrModelFixup((uint32_t *)(PITEM + 0x04));

            /* The vertex-cache resolve is handed the SLOT, not the value. */
            BrModelVtxResolve((uint32_t *)(PITEM + 0x04),
                              (int)BrLd32(PITEM + 0x00));

            BrRev4(PITEM + 0x08);
            g_BrModelFixup((uint32_t *)(PITEM + 0x08));

            BrRdBe32(PITEM + 0x0C);        /* item->k */
            BrRev2  (PITEM + 0x10);
            BrRev2  (PITEM + 0x12);
            /* These three are plain in-place byte reversals, not the
             * compose-and-store form: the original has exactly three
             * shl/or composes (block->n, item->m at +0x00 and item->k at
             * +0x0C) and six shl total, where the compose spelling here
             * gave twelve. */
            BrRev4(PITEM + 0x14);
            BrRev4(PITEM + 0x18);
            BrRev4(PITEM + 0x1C);

            /* The leaf count is likewise re-read from the item every pass;
             * the original's `if (k <= 0) skip` guard is the same test. */
            off = 0x20;
            for (iLeaf = 0;
                 iLeaf < (int32_t)BrLd32(PITEM + 0x0C);
                 iLeaf++, off += 4) {
                int32_t j;
#define PLEAF (*(unsigned char **)(void *)(PITEM + off))

                BrRev4(PITEM + off);
                g_BrModelFixup((uint32_t *)(PITEM + off));
                BrRev4(PLEAF + 0);

                /* GOTCHA: the halfword count comes from the ITEM's first
                 * dword, not the leaf's -- and it is re-read on every pass,
                 * like every other count in this function. */
                for (j = 0; j < 3 * (int32_t)BrLd32(PITEM + 0x00); j++)
                    BrRev2(PLEAF + 4 + 2 * (size_t)j);
#undef PLEAF
            }
        }
#undef PITEM
#undef PSLOT
#undef PBLOCK
    }

    /* ---- the record array at +0x08, stride 0x14 ----
     * The count is re-read from the header on every pass, and the compare
     * is unsigned. */
    pRec = pHdr + 8;

    for (iRec = 0; iRec < (uint32_t)BrLd16(pHdr + 2); iRec++, pRec += 0x14) {
        uint32_t v;

        if (BrLd32(pRec) == 0)
            continue;

        BrRev4(pRec + 0x00);
        g_BrModelFixup((uint32_t *)(pRec + 0x00));
        BrRev2(pRec + 0x04);
        BrRev2(pRec + 0x06);
        BrRev4(pRec + 0x08);
        BrRev4(pRec + 0x0C);
        BrRev4(pRec + 0x10);

        v = BrLd32(pRec);
        BrSub1002BF80(v);
        BrSub10074DC0(8);
        g_BrGfxSubmitB(BrLd32(pRec));
    }
}
#ifdef BR_MATCHING_BUILD
#undef BrRev4
#undef BrRev2
#undef BrRdBe32
#undef BrLd32
#undef BrLd16
#undef g_BrModelFixup
#undef g_BrModelDeref
#endif

/* 0x10036BD0 */
/* ================================================================== */
/* 7. Odds and ends                                                   */
/* ================================================================== */

/* 0x10035059 */
/* WHAT IT DOES: always answers "no". It exists to be installed where the game
 * needs a handler that declines everything; what it is installed as was not
 * established. */
/* @d3donly 0x10035059 BrRet0_10035059 -- exists in BRGlide only as folded/duplicated stubs; no unique twin locatable by bytes */
int BrRet0_10035059(void) { return 0; }
/* 0x1003557B */
/* 0x10035B87 */
/* WHAT IT DOES: a second, separate routine that also always answers "yes".
 * Two identical bodies at different addresses, so callers of one are not
 * callers of the other; what either is installed as was not established. */
/* @d3donly 0x10035B87 BrRet1_10035B87 -- glide twin 0x1002EC2C COMDAT-folded onto BrRet1_1003557B */
int BrRet1_10035B87(void) { return 1; }


/* 0x10035BBA */
/* ==================================================================
 * NOTES ON THE SKIPPED FUNCTIONS -- recorded so the analysis is not lost.
 * ==================================================================
 *
 * 0x100341B3 (packet starts at 0x100341E2, 47 bytes in)
 *   Walks an 8-byte-command display list until a null command pointer,
 *   dispatching on (w0 >> 24) - 0xB8 through a 0x45-entry byte index at
 *   0x10034415 into a jump table at 0x100343FD. Four handled cases:
 *     * match w0/w1 against six 32-byte records at arg2 and, on a hit,
 *       replace the command with the pair at (record + [ebp-4]*8); a hit at
 *       record index >= 3 sets the return value to 1;
 *     * with [ebp-0xC] != 0, match against one 16-byte record at 0x100AA8B8
 *       and substitute from +0x08/+0x0C;
 *     * scan two 8-byte entries at 0x100AA8C8 and set a local flag;
 *     * two near-identical tails that force w1 to 0x60789000 or 0x8C9CA800
 *       when that flag and g_6C6620 are both set.
 *   The prologue would tell us how [ebp-0x18] (which selects the +1 or +2
 *   column via `sete`) and the return slot [ebp-0x14] are initialised.
 *   Without it the function cannot be written down honestly.
 *
 * 0x10034F37 (packet starts mid-function)
 *   A plane-interleaved RLE decoder. For each of arg3 planes it reads a
 *   4-byte little-endian chunk length through 0x1007ED60 (memcpy), then
 *   consumes control bytes:
 *     c  < 0 : copy -c literal bytes, each written stride-arg3 apart;
 *     c >= 0 : repeat the next byte (c + BIAS) times, same stride.
 *   The destination pointer advances by ONE between planes, which is what
 *   makes the output interleaved. It returns the final destination offset.
 *   BIAS lives in [ebp-8] and is only ever written by the missing prologue,
 *   and it changes every decoded length, so the function is unusable
 *   without it.
 */

/* ── Ghidra-matched functions ─────────────────────────── */
#ifdef BR_MATCHING_BUILD
extern int DAT_106b8090;
extern int DAT_106ec778;
extern int DAT_106ed630;
extern unsigned short _DAT_100b5598;
int FUN_10059e70();
extern int DAT_100a7514;
extern int DAT_100a7518;
extern int DAT_106ed6e4;
extern int DAT_106ea3f4;
extern int DAT_106e8204;
extern int DAT_106ed674;
extern char DAT_106e8818;
extern char DAT_106e881a;
extern char DAT_106e881c;
extern char DAT_106e881e;
extern char DAT_106e8820;
extern char DAT_106e8822;
extern char DAT_106e8824;
extern char DAT_106e8826;
extern int *DAT_106e7710;
extern int _DAT_106ed368;
extern int _DAT_106ed648;
int FUN_1002bf50();
extern int DAT_106ed6fc;
extern int DAT_100b2f04;
extern unsigned char DAT_10af3bb7;
extern char DAT_10af3bcc;
extern int DAT_100aa128;
extern int DAT_100aa1e8;
extern int DAT_100aa068;
#ifndef BR_FUNCPTR_DEFINED
#define BR_FUNCPTR_DEFINED
typedef int (*funcptr)();
#endif
extern funcptr DAT_10b73534;
int FUN_1002d864();
extern char DAT_106ed708;
void BrPadFrameBegin(void);
extern int DAT_106e7738;
extern int DAT_106e79d0;
extern int DAT_106ea430;
extern int DAT_106ed650;
extern int DAT_106ea388;
extern int DAT_106ea410;
extern int DAT_106ecb48;
extern int DAT_106e9a30;
extern int DAT_106ec6a8;
extern int DAT_106ed700;
int BrPodNop();
extern int DAT_106e8a1c;
extern int DAT_106e8698;
extern int DAT_106ed5d0;
int BrStubTrue();

/* 0x1002E136, 0x1002E2DE and 0x1002E2E3 (nops) and the two setters
 * 0x1002E2E8 / 0x1002E2F5 now live in src/core/startup/br_stubs.c. */

/* 0x1002EBCC BrNop_1002EBCC now lives in src/core/startup/br_stubs.c. */


extern int DAT_106ec740;
extern int DAT_106ec744;
extern int DAT_106e7294;
extern int DAT_106ec768;
extern int DAT_106ed588;
extern int DAT_106b7ac0;
extern int DAT_106e9d8c;



#ifdef BR_MATCHING_BUILD
#include <windows.h>
#endif
extern int DAT_106b7ac8;
extern int DAT_106b8090;
extern int DAT_106b80a8;
extern char DAT_106e7730;
extern int DAT_106e7738;
extern char DAT_106e79b8;
extern unsigned char DAT_106e79ba;
extern unsigned char DAT_106e79bb;
extern int DAT_106e79d4;
extern int DAT_106e8200;
extern int DAT_106e869c;
extern int DAT_106ea1a0;
extern int DAT_106ea358;
extern int DAT_106ea410;
extern int DAT_106ea430;
extern char DAT_106ec508;
extern int DAT_106ec6c0;
extern int DAT_106ec794;
extern int DAT_106ed368;
extern int DAT_106ed370;
extern int DAT_106ed570;
extern int DAT_106ed5d0;
extern int DAT_106ed6e0;
extern int DAT_10b25794;
extern int _DAT_106ec770;
int BrPodNop();
int BrStubFalse();
int BrStubTrue();



#endif /* BR_MATCHING_BUILD */
