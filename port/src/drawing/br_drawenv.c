/* br_drawenv.c -- see br_drawenv.h.  The track/environment display-list
 * emitter, from BRGlide.dll.
 *
 * NOT CLAIMED: two FP-dense inner loops are deferred as TODO pending
 * x87emu verification.  The mechanical put() sections and the guard
 * logic are transcribed from the Glide asm at 10010000.asm line 6936.
 */
#include "br_drawenv.h"
#include "br_drawcar.h"     /* g_BrDrawWheelAlt, g_BrDrawTrackFlags,
                             * g_BrDrawCombined, g_BrDrawScale, TK_* */
#include "slice1_05.h"      /* BrRdpSetCombineLERP, BrGfxWords        */
#include "slice2_14.h"      /* g_BrFpsScreenW/H                       */
#include "slice2_18.h"      /* BrG_6C0258, BrG_6C6624, BrG_6C1174    */
#include "slice2_21.h"      /* BrMtxInvert, BrMtxMul                  */
#include "br_racebegin.h"   /* g_brRaceBeginDifficulty                */
#include "br_mat.h"         /* BrMat4, BrVec3Project                  */
#include "br_vec.h"
#include "slice3_39.h"      /* BrMemFill                              */

#include <string.h>

/* ---- storage --------------------------------------------------------- */

int32_t   g_BrEnvSection;            /* 0x106EC798 */
int32_t   g_BrEnvFlagCount;          /* 0x106E8A18 */
uint16_t *g_BrEnvFlagIndices;        /* 0x106ED528 */
void     *g_BrEnvCamPtr;             /* 0x106ED520 */
uint32_t  g_BrEnvOthermode;          /* 0x106E72E8 */
void     *g_BrEnvLightDirs;          /* 0x104B15D0 */
uint32_t *g_BrEnvTexLookup;          /* 0x1184C460 */
uint32_t  g_BrEnvTexDefault;         /* 0x1184C478 */
int32_t   g_BrEnvSegCount;           /* 0x104ADD38 */
void     *g_BrEnvSegBase;            /* computed */
uint8_t  *g_BrEnvBitmap;             /* 0x104AF5C8 */
BrVec3    g_BrEnvFogDir;             /* 0x106E72A8 */

/* The put()/put_slot() helpers and the DL cursor live in br_drawcar.c;
 * they are file-static there.  This file cannot call them directly.
 * Instead, the function below uses the same inline cursor pattern. */
extern uint32_t *BrG_6C0680;         /* the DL cursor, slice2_18 */

static void env_put(uint32_t w0, uint32_t w1)
{
    uint32_t *p = BrG_6C0680;
    BrG_6C0680 += 2;
    p[0] = w0;
    p[1] = w1;
}

static BrGfxWords *env_put_slot(void)
{
    BrGfxWords *p = (BrGfxWords *)BrG_6C0680;
    BrG_6C0680 += 2;
    return p;
}

/* 0x10008D60 -- 1-byte trace function (a nop in both builds). */
static void env_trace(void) { }

/* ==================================================================== *
 * 0x10017110 -- the track/environment emitter.  2,039 bytes.
 *
 * NOT CLAIMED (@implements withheld).  Two FP-dense inner loops
 * (per-car projection at 0x10017461-0x100175F2, per-segment tile
 * computation at 0x10017699-0x100178BD) are deferred as TODO.
 *
 * Structure:
 *   1. Guard: return if both mode flags clear, or if any track flag
 *      in the g_BrEnvFlagIndices array has bit 0x10 set at offset 0x4C.
 *   2. Header: pipe sync, two-cycle, texture on, othermode, combiner,
 *      render mode, conditional DD/DC texture opcode, settile, clear
 *      geom, prim colour.
 *   3. Matrix setup: invert the combined matrix, zero its translation
 *      row, multiply by a projection scale, optionally clear the
 *      per-section bitmap.
 *   4. Per-car loop (0..16): project each car's light direction through
 *      the projection matrix, write a cross-shaped stamp into the
 *      visibility bitmap.  [TODO: x87emu]
 *   5. Scene colour + matrix multiply for fog direction.
 *   6. Per-segment loop: for each segment, transform its 3-component
 *      position through the projected matrix, compute tile coordinates,
 *      and emit a single 8-byte DL command.  [TODO: x87emu]
 *   7. Tail: pipe sync + othermode restore.
 * ==================================================================== */
void BrEnvEmit(void)
{
    int32_t i;

    /* 0x17110 -- guard: both mode flags must not be zero. */
    if (g_BrDrawWheelAlt == 0 && BrG_6C6624 == 0)
        return;

    /* 0x1712E -- track-flag guard: if any indexed record has bit 0x10
     * at offset 0x4C, return immediately. */
    if (g_BrEnvFlagCount > 0) {
        const unsigned char *flags = (const unsigned char *)g_BrDrawTrackFlags;
        for (i = 0; i < g_BrEnvFlagCount; ++i) {
            uint32_t k = g_BrEnvFlagIndices[i];
            if (flags[k * 84 + 0x4C] & 0x10)
                return;
        }
    }

    /* 0x17167 -- compute per-section base pointer (stride 3132). */
    {
        int32_t idx = g_BrEnvSection;
        uint32_t off = (uint32_t)idx * 261u;
        off = off * 3u;
        g_BrEnvSegBase = (void *)((unsigned char *)g_BrEnvBitmap
                         - 0x78u + off * 4u);
    }

    /* 0x17188 -- trace call, conditional on g_6ED6B4. */
    if (BrG_6C6624 != 0)
        env_trace();

    /* 0x1718F -- header DL commands. */
    env_put(0xE7000000u, 0);                    /* pipe sync             */
    env_put(0xBA001402u, 0);                    /* two-cycle             */
    env_put(0xBB000001u, 0xFFFFFFFFu);          /* texture on            */
    env_put(0xBA000C02u, g_BrEnvOthermode);     /* othermode             */

    BrRdpSetCombineLERP(env_put_slot(),
        TK_TEXEL0,   TK_ZERO,     TK_ZERO, TK_ZERO,
        TK_TEXEL1_A, TK_ZERO,     TK_ZERO, TK_ZERO,
        TK_TEXEL0,   TK_ZERO,     TK_ZERO, TK_ZERO,
        TK_TEXEL1_A, TK_ZERO,     TK_ZERO, TK_ZERO);

    env_put(0xB900031Du, 0x00504240u);          /* render mode           */

    /* 0x17257 -- conditional texture opcode. */
    if (BrG_6C6624 != 0) {
        uint32_t texVal = g_BrEnvTexLookup[g_BrEnvSection];
        uint32_t w0dd = (texVal & 0x00FFFFFFu) | 0xDD000000u;
        uint32_t w1   = (uint32_t)g_BrEnvSection * 0x1000u
                        + (uint32_t)(uintptr_t)g_BrEnvBitmap;
        env_put(w0dd, w1);

        uint32_t w0dc = (texVal & 0x00FFFFFFu) | 0xDC000000u;
        env_put(w0dc, 1);
    } else {
        uint32_t w0dc = (g_BrEnvTexDefault & 0x00FFFFFFu) | 0xDC000000u;
        env_put(w0dc, 1);
    }

    env_put(0xF2002002u, 0x000FE0FEu);          /* settile              */
    env_put(0xB6000000u, 0x00003000u);           /* clear geom           */

    /* 0x17331 -- prim colour (different payload per mode flag). */
    if (g_BrDrawWheelAlt != 0)
        env_put(0xFA00FFFFu, 0xE0E0FFFFu);
    else
        env_put(0xFA00FFFFu, 0x788088FFu);

    /* 0x1736B -- matrix setup: invert the combined matrix, zero
     * translation, multiply by a projection scale. */
    BrMtxInvert(&g_BrDrawCombined, g_BrEnvCamPtr);
    g_BrDrawCombined.m[3][0] = 0.0f;
    g_BrDrawCombined.m[3][1] = 0.0f;
    g_BrDrawCombined.m[3][2] = 0.0f;

    /* The projection scale matrix (values read from the asm immediates):
     * a permuted diagonal with s ≈ 6.1e-5 (0x38800100 as float). */
    {
        union { uint32_t u; float f; } scale;
        scale.u = 0x38800100u;
        memset(&g_BrDrawScale, 0, sizeof(g_BrDrawScale));
        g_BrDrawScale.m[0][2] = scale.f;
        g_BrDrawScale.m[1][0] = scale.f;
        g_BrDrawScale.m[2][1] = scale.f;
        g_BrDrawScale.m[3][3] = 1.0f;
    }
    BrMtxMul(&g_BrDrawCombined, &g_BrDrawCombined, &g_BrDrawScale);

    /* 0x17451 -- per-car loop: project, clamp, stamp the bitmap. */
    if (BrG_6C6624 != 0) {
        uint8_t *pBmp = g_BrEnvBitmap
                        + (uint32_t)g_BrEnvSection * 0x1000u;
        BrMemFill(pBmp, 0x1000, 0);

        /* TODO 0x17461-0x175F2: per-car projection loop (16 iterations).
         *
         * For each car i (0..15):
         *   1. Load the car's 3-component light direction from
         *      g_BrEnvLightDirs + i * 12.
         *   2. BrVec3NormaliseGuard it.
         *   3. If g_brRaceBeginDifficulty != BrG_6C1174, negate it.
         *   4. Multiply by .rdata constants (0x10077344..0x10077354),
         *      subtract from 0x10077348, producing two projected coords.
         *   5. Add g_BrDrawCombined.m[3] offsets (translation).
         *   6. __ftol both coords; if both in [2, 61], stamp a
         *      cross-shaped pattern (21 bytes) into the bitmap at
         *      computed offsets.
         *
         * The x87 chain is 40+ instructions with 6 fxch shuffles and
         * needs emulator verification. */
    }

    /* 0x175F4 -- scene colour + fog direction matrix multiply. */
    env_put(0xFA00FFFFu, 0x788088FFu);  /* scene-pass prim colour */

    /* 0x17612 -- BrMtxMul(g_BrDrawCombined, g_BrEnvFogDir, g_BrDrawCombined).
     * The asm passes 0x106E72A8 as a BrMat4*; the 12 bytes of BrVec3 are
     * read as the first row of the matrix argument. */
    BrMtxMul(&g_BrDrawCombined, (const BrMat4 *)(void *)&g_BrEnvFogDir,
             &g_BrDrawCombined);

    /* 0x17626 -- per-segment scale factors (conditional on mode flag). */
    {
        float scaleA, scaleB;
        if (g_BrDrawWheelAlt != 0) {
            scaleA = 96.0f;    /* 0x42C00000 */
            scaleB = 144.0f;   /* 0x43100000 */
        } else {
            scaleA = 128.0f;   /* 0x43000000 */
            scaleB = 128.0f;   /* 0x43000000 */
        }

        /* 0x17654 -- viewW/viewH: screenW or -screenW depending on the
         * difficulty != cull_ref flag, then doubled. */
        float viewW, viewH;
        if (g_brRaceBeginDifficulty != BrG_6C1174)
            viewW = (float)(-g_BrFpsScreenW) * 2.0f;
        else
            viewW = (float)(g_BrFpsScreenW) * 2.0f;
        viewH = (float)(g_BrFpsScreenH) * 2.0f;

        /* TODO 0x17699-0x178BD: per-segment tile loop.
         *
         * For each segment s (0..g_BrEnvSegCount-1):
         *   1. Read 3 int16 from g_BrEnvSegBase + s*6.
         *   2. Convert to float, multiply by projected matrix elements
         *      from g_BrDrawCombined (rows 0-2, cols 0-3).
         *   3. fcomp against .rdata threshold 0x10077364; if the
         *      projected w <= 0, skip.
         *   4. Compute 1/w, multiply x and y by it; range-check both
         *      against 0x10077368 (upper) and 0x10077304 (lower).
         *   5. Multiply by scaleA/scaleB and viewW/viewH, __ftol to
         *      get integer tile coords (u0, v0, u1, v1).
         *   6. Pack into one 8-byte DL command:
         *      w0 = ((u0+u1) & 0x3FFC | 0x38C000) >> 2, shifted << 12
         *           | (v1 & 0xFFF)
         *      w1 = (u0 & 0xFFF) << 12 | (v0 & 0xFFF)
         *   7. env_put(w0, w1)
         *
         * The x87 chain is ~70 instructions with matrix element reads
         * from g_BrDrawCombined and needs emulator verification. */
        (void)scaleA;
        (void)scaleB;
        (void)viewW;
        (void)viewH;
    }

    /* 0x178C3 -- tail: sync + othermode restore. */
    env_put(0xE7000000u, 0);
    env_put(0xBA001301u, 0x00080000u);
}
