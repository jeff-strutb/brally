/* slice2_15.c -- pass-15 packet, 0x10016A60-0x1001CCA0. See slice2_15.h.
 *
 * ---------------------------------------------------------------------------
 * FLOAT CONSTANTS
 * ---------------------------------------------------------------------------
 * Every constant below was read out of BRD3D.dll's .rdata at the address in
 * the comment, not inferred from context. Two of them are QWORDS (doubles);
 * the original uses `fcomp qword` / `fmul qword` on those, so they are declared
 * double here.
 *
 * ---------------------------------------------------------------------------
 * DEVIATIONS (all of them, collected)
 * ---------------------------------------------------------------------------
 *  - x87 intermediates: the original computes at 53-bit precision, per the
 *    CRT control word 0x027F (CONVENTIONS.md), NOT the 80-bit extended this
 *    entry used to claim. This port uses `double`, which therefore models
 *    those registers exactly -- so the old rider that "results differ in the
 *    last bits of long chains" does not apply and has been dropped. Where the
 *    original round-trips a value through a 32-bit memory slot the port stores
 *    to `float` so that rounding step is preserved. That pairing -- double for
 *    registers, float at the spills -- is the whole model, and this file
 *    already had it right.
 *  - display-list addresses: command words are 32 bits, host pointers may be
 *    64. BrGfxAddr() takes the low 32 bits and is marked at each use.
 *  - sprintf -> snprintf, with the original's buffer sizes.
 *  - __ftol (0x1007C8A0) is reproduced by BrFtol(); out-of-range doubles are
 *    undefined behaviour in C, so it returns 0x80000000 there, which is what
 *    the x87 indefinite-integer store produces.
 */
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include <math.h>
#include <stdint.h>
#include <stdio.h>

#include "slice2_15.h"

/* ---- 0x1008F300 .. 0x1008F3C0 ------------------------------------------ */
static const float  kF300 = 0.0f;                    /* 0x1008F300 */
static const float  kF304 = 1000.0f;                 /* 0x1008F304 */
static const float  kF308 = 0.0001428571413271129f;  /* 0x1008F308  1/7000 */
static const float  kF30C = 0.5f;                    /* 0x1008F30C */
static const float  kF314 = 5.0f;                    /* 0x1008F314 */
static const float  kF31C = 7.0f;                    /* 0x1008F31C */
static const float  kF320 = 0.0001250000059371814f;  /* 0x1008F320  1/8000 */
static const float  kF324 = 0.05000000074505806f;    /* 0x1008F324 */
static const float  kF328 = -0.05000000074505806f;   /* 0x1008F328 */
static const float  kF32C = -0.30000001192092896f;   /* 0x1008F32C */
static const float  kF330 = 0.30000001192092896f;    /* 0x1008F330 */
static const float  kF334 = 100.0f;                  /* 0x1008F334 */
static const float  kF338 = 0.4464285671710968f;     /* 0x1008F338  1/2.24 */
static const float  kF33C = 25.0f;                   /* 0x1008F33C */
static const double kF340 = 0.0;                     /* 0x1008F340 (qword) */
static const float  kF348 = 0.6213712096214294f;     /* 0x1008F348 */
static const float  kF34C = 0.9900000095367432f;     /* 0x1008F34C */
static const float  kF350 = 3.051804378628731e-05f;  /* 0x1008F350  1/32768 */
static const float  kF354 = 1.0f;                    /* 0x1008F354 */
static const float  kF358 = 6.2831854820251465f;     /* 0x1008F358  2*pi  */
static const float  kF35C = 0.0f;                    /* 0x1008F35C */
static const float  kF360 = -6.2831854820251465f;    /* 0x1008F360 -2*pi  */
static const float  kF364 = 0.5f;                    /* 0x1008F364 */
static const float  kF368 = -0.5f;                   /* 0x1008F368 */
static const float  kF36C = -343.0f;                 /* 0x1008F36C */
static const double kF370 = 2048.0;                  /* 0x1008F370 (qword) */
static const float  kF378 = 0.2777777910232544f;     /* 0x1008F378  1/3.6 */
static const float  kF37C = 3.5999999046325684f;     /* 0x1008F37C  3.6   */
static const float  kF384 = 0.25f;                   /* 0x1008F384 */
static const double kF388 = 16383.5;                 /* 0x1008F388 (qword) */
static const float  kF3BC = 255.0f;                  /* 0x1008F3BC */
static const float  kF3C0 = 0.003921568859368563f;   /* 0x1008F3C0  1/255 */

/* ---- state blocks ------------------------------------------------------ */
static BrGfxOut     g_out;
static BrScreenInfo g_screen;
static BrHudEnv     g_hud;
static BrSceneEnv   g_scene;
static BrWeather    g_weather;
static BrRdpRegs    g_regs;

BrGfxOut     *BrGfxGetOut(void)   { return &g_out; }
BrScreenInfo *BrScreenGet(void)   { return &g_screen; }
BrHudEnv     *BrHudGetEnv(void)   { return &g_hud; }
BrSceneEnv   *BrSceneGetEnv(void) { return &g_scene; }
BrWeather    *BrWeatherGet(void)  { return &g_weather; }
BrRdpRegs    *BrRdpGetRegs(void)  { return &g_regs; }

/* ---- per-frame scene accumulators -------------------------------------- *
 * A pair of float accumulators the per-frame race render maintains.  Their
 * TRUE linkage is global: they are written from a different object (the geometry
 * pass at 0x1000BEB0 == BrCarDrawBody, br_drawcar.c) and read again inside the
 * frame builder at 0x10011FA0, so they are NOT static -- see slice2_15.h for the
 * shared declarations.
 *
 * Named by their Glide .data address (the reference target).  The geometry pass
 * ADDS into each of them a BrVec3Dot-derived, epsilon-clamped, squared term; the
 * frame builder later reads the pair back as `g_4B16AC - g_4B16A0*k` and
 * `g_4B16A0 + g_4B16AC`, each handed to BrFadeDrawSprite (0x10017F80) as its
 * alpha.  Their exact geometric meaning is not yet pinned down, so they keep
 * address names rather than a guessed role.  BrSceneAccumReset (below) zeroes
 * both at the top of every frame. */
float g_4B16A0 = 0.0f;   /* 0x104B16A0 */
float g_4B16AC = 0.0f;   /* 0x104B16AC */

/* ---- helpers ----------------------------------------------------------- */

/* 0x1007C8A0 __ftol: x87 truncation toward zero, then the LOW DWORD of the
 * 64-bit result (before any clamp). DEVIATION: see the file header. */
static int32_t BrFtol(double v)
{
    if (v > -9.2233720368547758e18 && v < 9.2233720368547758e18) {
        int64_t t = (int64_t)v;
        return (int32_t)(uint32_t)(uint64_t)t;
    }
    return (int32_t)0x80000000u;
}

/* DEVIATION: 32-bit command word from a possibly-64-bit host pointer. */
static __inline uint32_t BrGfxAddr(const void *p)
{
    return (uint32_t)(uintptr_t)p;
}

/* The original's allocation idiom: read the cursor, bump it by 8, write. */
static __inline BrGfxCmd *BrGfxAlloc(void)
{
    BrGfxCmd *p = g_out.pCur;
    g_out.pCur = p + 1;
    return p;
}

static __inline void BrGfxEmit(uint32_t w0, uint32_t w1)
{
    BrGfxCmd *p = BrGfxAlloc();
    p->w0 = w0;
    p->w1 = w1;
}

/* `11 * i` scaled by 8 == a 0x58-byte stride. */
static __inline BrHudView *BrHudViewAt(BrHudView *aViews, int32_t i)
{
    return &aViews[i];
}

extern const uint8_t g_hudSpriteTable[];   /* 0x100BCDD0 -- an ARRAY */
static __inline const BrHudSprite *BrHudSpriteAt(int32_t i)
{
    return (const BrHudSprite *)(g_hudSpriteTable
                                 + (size_t)(uint32_t)i * BR_HUDSPRITE_STRIDE);
}

/* =====================================================================
 * 0x10016A60
 * ===================================================================== */
/* WHAT IT DOES: draws one rectangular picture on screen at a given position
 * and size -- the workhorse behind every dashboard graphic and menu image. It
 * points the hardware at the picture, tells it how much of it to use, and then
 * asks for the rectangle. */
/* @implements 0x10016A60 d3d BrGfxDrawTexRect */
void BrGfxDrawTexRect(uint32_t dlAddr, int x, int y, int w, int h)
{
    BrGfxCmd *p;

    /* 10016A60: w1 is written before w0; the order does not matter. */
    p = BrGfxAlloc();
    p->w1 = 1u;
    p->w0 = 0xDC000000u | (dlAddr & 0x00FFFFFFu);

    /* 10016AC5: tile size. uls = ult = 0x002 (0.5 in 10.2); lrs/lrt are the
     * extents less half a texel. */
    p = BrGfxAlloc();
    p->w0 = 0xF2002002u;
    p->w1 = ((uint32_t)((h * 4) - 2) & 0xFFFu)
          | ((uint32_t)(w * 0x4000 - 0x2000) & 0xFFF000u);

    /* 10016B02: the rect.  The *4 / mask / >>2 fixed-point (10.2) dance is
     * value-preserving but NOT byte-preserving: VC5 emits every step
     * (shl 2 / and / [or] / sar 2 / shl 12), so it must be transcribed, not
     * simplified.  The command byte rides along as 0x38C000, which the
     * >>2 << 12 turns into 0xE3000000.  x is spelled `* 4` (lea), y is
     * spelled `<< 2` (shl) -- the original mixes them.
     * RESIDUE (0+1 regnorm, T3a): the original composes the tile word in
     * the h-term's register and copies (x+w)*4 aside (`mov edi,edx`)
     * because it reloads y into edx; ours frees eax and skips the copy --
     * one whole-register rotation, 5 bytes short.  Probed and failed:
     * h-first OR order, Ghidra-literal `w*0x4000-0x2000` w-term (both
     * canonicalize back). */
    p = BrGfxAlloc();
    p->w0 = (uint32_t)((int32_t)(((uint32_t)((x + w) * 4) & 0x3FFCu)
                                 | 0x38C000u) >> 2) << 12
          | ((uint32_t)((y + h) * 4 >> 2) & 0xFFFu);
    p->w1 = (((uint32_t)(x * 4 >> 2) & 0xFFFu) << 12)
          | ((uint32_t)((y << 2) >> 2) & 0xFFFu);
}

/* =====================================================================
 * 0x10016B40
 * ===================================================================== */
/* WHAT IT DOES: draws the rev counter on the dashboard -- the dial face and
 * the needle sweeping round it. The needle angle comes from the engine, with a
 * small random shake added so it never sits perfectly still, and the dial face
 * itself is one of sixteen pictures chosen the same way. In split screen the
 * face is skipped and only the needle is drawn, and even then the artwork is
 * taken from the first player's record rather than the current one. */
/* @implements 0x10016B40 d3d BrHudDrawDial */
void BrHudDrawDial(BrHudView *aViews)
{
    const BrHudSprite *pSpr;
    BrHudQuad *pQuad;
    BrGfxCmd *p;
    int32_t x, y, iSeq;
    float tip, base;
    float t;

    if (g_hud.f0BD3F4 == 0)              /* 10016B4D */
        return;
    if (g_hud.f22AF1C != 0)              /* 10016B56 */
        return;

    /* The sprite record is resolved BEFORE the call: the original loads
     * g_screen.iView, aViews[iView].iSprite and the sprite address at
     * 100140D2-10014109, all ahead of the call at 10014110. */
    pSpr = BrHudSpriteAt(BrHudViewAt(aViews, g_screen.iView)->iSprite);

    /* 10016B68-10016BA0: the two globals are converted to float and handed to
     * 0x1003407D, whose result is discarded -- it is called for effect. */
    BrSub_1003407D((float)g_hud.f6C0684, (float)g_hud.f6C299C);

    x = g_screen.cx - pSpr->e4 - 0x10;               /* 10016BB1 */
    y = BrHudViewAt(aViews, g_screen.iView)->y
      + BrHudViewAt(aViews, g_screen.iView)->h - pSpr->e5 - 4;  /* 10016BC6 */

    /* 10016BE9: NaN takes the zero path (C0 is set for unordered). */
    if (g_hud.pRace->f0E68 >= kF300)
        iSeq = g_hud.pRace->f0E70 + 1;
    else
        iSeq = 0;

    if (g_screen.cViews == 1) {                      /* 10016C0D */
        int32_t v, iFrame;

        p = BrGfxAlloc();
        BrSub_1002F900(p, 0, 0, 0, 0x3EB, 0, 0, 0, 0x3EB,
                          0, 0, 0, 0x3EB, 0, 0, 0, 0x3EB);
        BrGfxEmit(0xB900031Du, 0x0C184240u);
        BrGfxEmit(0xB900031Du, 0x0C193078u);
        BrGfxEmit(0xB6000000u, 0x00000001u);
        BrGfxEmit(0xDE000000u, 0x3F800000u);   /* +1.0f as a payload */
        BrGfxEmit(0xDF000000u, 0xBF800000u);   /* -1.0f as a payload */

        /* 10014258: both tests are `je` into the dial path, so the source
         * puts the non-dial arm first -- `mode != 1 && mode != 2`.  Each arm
         * carries its OWN BrGfxDrawTexRect call (10014266-1001427F is a
         * constant-folded copy reading aDial[0] at +0x14 directly); the two
         * are cross-jumped at the last push, 10014311. */
        if (pSpr->mode != 1 && pSpr->mode != 2) {
            /* 10016CF6: the non-dial path always uses byte offset +0x14,
             * which is aDial[0] -- the same slot iFrame == 15 selects. */
            BrGfxDrawTexRect(aViews[0].aDial[0], x, y,
                             pSpr->e4, pSpr->e5);
        } else {
            /* 10016D14: with f6909B4 set the jitter is pinned to 0x40. */
            if (g_hud.f6909B4 != 0)
                v = 0x40;
            else
                v = BrRandom() & 0x7F;

            /* 100142A2-100142DA: every operand is a dword -- float, not
             * double -- and __ftol takes the value off the x87 stack. */
            iFrame = (int32_t)(((float)v + g_hud.pRace->f0E24 - kF304)
                               * kF308 * (float)(pSpr->ea + 1) - kF30C);

            if (iFrame < 0)         iFrame = 0;
            else if (iFrame > 0xF)  iFrame = 0xF;

            /* GOTCHA: aDial and dlOverlay come from RECORD 0, not iView. */
            BrGfxDrawTexRect(aViews[0].aDial[15 - iFrame], x, y,
                             pSpr->e4, pSpr->e5);
        }

        /* 10016DB4: one cached sequence id per view. */
        if (iSeq != g_hud.aLastSeq[g_screen.iView]) {
            p = BrGfxAlloc();
            p->w0 = 0xDD000000u | (aViews[0].dlOverlay & 0x00FFFFFFu);
            /* DEVIATION: 32-bit truncation of a host pointer. */
            p->w1 = BrGfxAddr((const uint8_t *)pSpr + BR_HUDSPRITE_DATAOFF)
                  + (uint32_t)pSpr->fFC * (uint32_t)iSeq;
            g_hud.aLastSeq[g_screen.iView] = iSeq;
        }

        BrGfxDrawTexRect(aViews[0].dlOverlay,
                         x + pSpr->e6, y + pSpr->e7,
                         pSpr->e8, pSpr->e9);
    }

    /* ---- 10016E3C: reached for every cViews ---- */
    {
        int32_t v;
        if (g_hud.f6909B4 != 0)
            v = 0x40;
        else
            v = BrRandom() & 0x7F;
        /* Stored through a 32-bit slot, so the float32 rounding is real. */
        t = (float)v + g_hud.pRace->f0E24;
    }

    if (pSpr->mode != 0)                             /* 10016E7C */
        return;

    pQuad = &g_hud.aQuads[g_screen.iView + 2 * g_hud.f6C65EC];

    if (g_screen.cViews == 2) {                      /* 10016E98 */
        base = kF314;          /* 5.0f  */
        tip  = 15.0f;          /* the immediate 0x41700000 */
        x += (pSpr->ea * 3) / 4;
        y += (pSpr->eb * 3) / 4;
    } else {
        base = kF31C;          /* 7.0f  */
        tip  = 20.0f;          /* the immediate 0x41A00000 */
        x += pSpr->ea;
        y += pSpr->eb;
    }

    {
        float A  = pSpr->fF0;
        float  ang;
        float fx;
        int32_t dy = g_screen.cy - y;

        /* 10016F22: NaN takes the "no interpolation" path. */
        if (t > kF300) {
            /* 100144AF is `fsubp st(1)`: A is updated IN PLACE on the x87
             * stack -- the source subtracts FROM A. */
            A -= (pSpr->fF0 - pSpr->fEC) * t * kF320;
        }

        /* The tip pair uses the 15/20 radius; the base pair uses 5/7. The
         * original loses the tip radius (its stack slot is reused) and reaches
         * for the deep x87 copy of the base radius instead -- these two really
         * are different numbers.
         *
         * Every trig result goes through the reused temp `t` -- that is what
         * keeps the scalar on the x87 stack and the tip/base on the fmul
         * side (`fmul [esp+S]` / `fmul st(N)`); inline cos()*tip emits
         * fld+fmulp pairs instead.  `fx` is a statement local (converted
         * once, memory-homed, adds read the slot); `dy` stays an int and is
         * converted by the CSE'd inline cast at its first use inside v[0].y,
         * matching the original's sunk fild/fstp.
         *
         * RESIDUE (~4 insns, T3a): at the v[1].x and v[2].x transitions the
         * original computes the next angle (`fld st(0); fsub k`) BEFORE
         * storing the previous y and juggles with 2 fxch each; we store
         * first, no fxch.  Probed and failed: hoisting the ang assignment
         * above the v[k].y statement (extends ang's live range, changes the
         * frame, 9+2), fresh per-vertex angle variables (no change).
         * Scheduler-internal pipelining depth.
         *
         * CONFIRMED 2026-09-03 that this is the WHOLE residue: the four
         * missing `fxch` are exactly the -8 bytes and -4 instructions, and
         * nothing else in 1703 B differs. fn.py also reports an extra
         * `lea R,[R*I]` against a missing `lea R,[R*I + I]` -- that pair is
         * a PHANTOM. It is the `lea edi,[edx*8 + <global>]` at 0x10014109,
         * whose displacement is a reloc and therefore reads 0 in the .obj;
         * both encodings are the same seven bytes and mask equal. Do not
         * chase it. */
        fx = (float)x;
        ang = A - kF324;
        t = (float)cos(ang);
        pQuad->v[0].x = (float)(int16_t)(int32_t)(t * tip  + fx);
        t = (float)sin(ang);
        pQuad->v[0].y = (float)(int16_t)(int32_t)(t * tip  + (float)dy);
        pQuad->v[0].z = 0.0f;
        ang = A - kF328;
        t = (float)cos(ang);
        pQuad->v[1].x = (float)(int16_t)(int32_t)(t * tip  + fx);
        t = (float)sin(ang);
        pQuad->v[1].y = (float)(int16_t)(int32_t)(t * tip  + (float)dy);
        pQuad->v[1].z = 0.0f;
        ang = A - kF32C;
        t = (float)cos(ang);
        pQuad->v[2].x = (float)(int16_t)(int32_t)(t * base + fx);
        t = (float)sin(ang);
        pQuad->v[2].y = (float)(int16_t)(int32_t)(t * base + (float)dy);
        pQuad->v[2].z = 0.0f;
        ang = A - kF330;
        t = (float)cos(ang);
        pQuad->v[3].x = (float)(int16_t)(int32_t)(t * base + fx);
        t = (float)sin(ang);
        pQuad->v[3].y = (float)(int16_t)(int32_t)(t * base + (float)dy);
        pQuad->v[3].z = 0.0f;
    }

    /* 100145F2-10014619: twelve straight-line stores, 255.0f materialised
     * once as the immediate 0x437F0000 -- not a loop, not a named const. */
    pQuad->v[0].f14 = 0.0f;
    pQuad->v[0].f18 = 255.0f;
    pQuad->v[0].f1C = 0.0f;
    pQuad->v[1].f14 = 0.0f;
    pQuad->v[1].f18 = 255.0f;
    pQuad->v[1].f1C = 0.0f;
    pQuad->v[2].f14 = 0.0f;
    pQuad->v[2].f18 = 255.0f;
    pQuad->v[2].f1C = 0.0f;
    pQuad->v[3].f14 = 0.0f;
    pQuad->v[3].f18 = 255.0f;
    pQuad->v[3].f1C = 0.0f;

    /* 100170A9: three slots are taken in this order -- the third goes to
     * 0x1002F900, so the interleaving matters. */
    p = BrGfxAlloc();
    p->w0 = 0xE7000000u;
    p->w1 = 0u;
    p = BrGfxAlloc();
    p->w0 = 0xBA001402u;
    p->w1 = 0u;
    p = BrGfxAlloc();
    BrSub_1002F900(p, 0, 0, 0, 0x3EC, 0, 0, 0, 0x3EC,
                      0, 0, 0, 0x3EC, 0, 0, 0, 0x3EC);

    BrGfxEmit(0xB900031Du, 0x00552048u);
    BrGfxEmit(0xB6000000u, 0x00033000u);
    BrGfxEmit(0xB7000000u, 0x00000004u);
    /* G_VTX: F3DEX packs n in bits[15:10], so 0x107F is n = 4. */
    BrGfxEmit(0x0400107Fu, BrGfxAddr(pQuad));
    BrGfxEmit(0xB1000102u, 0x00000203u);   /* G_TRI2 */
    BrGfxEmit(0xB7000000u, 0x00000001u);
    BrGfxEmit(0xE7000000u, 0x00000000u);
}

/* =====================================================================
 * 0x10017690
 * ===================================================================== */
/* WHAT IT DOES: draws the game's own banner message across the middle of the
 * player's view -- things like PAUSED or WRONG WAY. It shows nothing when there
 * is no message set, or while the game is in the state that suppresses banner
 * text. */
/* @implements 0x10017690 d3d BrHudDrawViewCentreText */
void BrHudDrawViewCentreText(const BrHudView *aViews)
{
    const BrHudView *pView;
    int32_t big, small_, x, yy;

    if (BrSub_1002B2A0() != 0)
        return;

    if (g_screen.cViews == 1) { big = 0x1E; small_ = 0x14; }
    else                      { big = 0x14; small_ = 0x0F; }

    pView = &aViews[g_screen.iView];

    x  = pView->x + pView->w / 2;       /* cdq/sub/sar 1: truncates toward 0 */
    yy = pView->y + pView->h / 3 + 0x18;/* magic 0x55555556: signed /3       */

    BrSub_10019270();

    if (g_hud.pszCentre == NULL)
        return;

    BrSub_100192F0(small_);

    /* (big*3)/16, signed, truncating -- 5 for 0x1E, 3 for 0x14. */
    BrTextDraw(g_hud.pszCentre, x, yy + (big * 3) / 16);
}

/* =====================================================================
 * 0x10017790
 * ===================================================================== */
/* WHAT IT DOES: draws the race's current announcement in the middle of the
 * player's view -- lap records, finishing places and the like. There are two
 * message slots and the first takes priority; the first is also drawn half
 * again as large as the second, so which slot a message lands in changes how
 * big it appears. Two separate suppression flags can silence it entirely. */
/* @implements 0x10017790 d3d BrHudDrawViewMessage */
void BrHudDrawViewMessage(const BrHudView *aViews)
{
    const BrHudView *pView;
    int32_t big, small_, x, yy;

    if (g_hud.f6909B4 != 0)
        return;
    if (BrSub_1002B2A0() != 0)
        return;

    if (g_screen.cViews == 1) { big = 0x1E; small_ = 0x14; }
    else                      { big = 0x14; small_ = 0x0F; }

    pView = &aViews[g_screen.iView];

    x  = pView->x + pView->w / 2;
    yy = pView->y + pView->h / 3;

    BrSub_10019270();

    if (g_hud.pRace->psz0FFC != NULL) {
        /* GOTCHA: this branch sizes the text with `big`, the other with
         * `small_`; they are not the same number. */
        BrSub_100192F0(big);
        BrTextDraw(g_hud.pRace->psz0FFC, x, yy + big / 4);
        return;
    }
    if (g_hud.pRace->psz1004 == NULL)
        return;

    BrSub_100192F0(small_);
    BrTextDraw(g_hud.pRace->psz1004, x, yy + (big * 3) / 16);
}

/* =====================================================================
 * 0x10017CD0
 * ===================================================================== */
/* WHAT IT DOES: works out how many seconds behind the leader this car is, by
 * taking the gap between them along the track and dividing by the car's own
 * speed. A stopped or barely-moving car has its speed treated as a minimum so
 * the gap does not blow up, but there is no upper limit, and a car that is not
 * behind anyone gets zero. */
/* @implements 0x10017CD0 d3d BrHudGapSeconds */
float BrHudGapSeconds(const BrCar *aCars, int iCar)
{
    int32_t best = 0xFF;              /* 10017CE8 */
    float   f    = kF300;             /* 10017CD9 */
    float   v;
    int     i;

    for (i = 0; i < g_hud.cCars; ++i) {
        if (best > aCars[i].f0FF8) {
            best = aCars[i].f0FF8;
            f = aCars[i].f0FF4 - aCars[iCar].f0FF4;
        }
    }

    /* 10017D26: `fcom kF300 / test ah,0x40` -- C3 covers equal AND unordered,
     * so a NaN f (or v) takes the equal path. All-float spellings: the
     * original compares `fcom dword ptr` against the float globals and keeps
     * f and v on the x87 stack. The nesting (returns falling out the bottom,
     * not early-outs) is what places both sentinel blocks out-of-line after
     * the division tail, sharing the final epilogue. */
    if (f != kF300) {
        v = aCars[iCar].f1030 * kF338;

        if (v != kF300) {
            /* Asymmetric: v is raised to 25.0f but never lowered. */
            if (v < kF33C)
                v = kF33C;

            return f / v;
        }
        return kF304;   /* NaN/zero v yields the 1000.0f sentinel */
    }
    return f;
}

/* =====================================================================
 * 0x10017C80
 * ===================================================================== */
/* WHAT IT DOES: turns the gap to the leader into the "+12.34" text shown on
 * the dashboard. A car that is not behind gets an empty string rather than a
 * zero, so nothing is drawn for the leader. */
/* @implements 0x10017C80 d3d BrHudFormatGapString */
const char *BrHudFormatGapString(const BrCar *aCars, int iCar)
{
    float f = BrHudGapSeconds(aCars, iCar);

    /* 10017C9E: C0|C3 -- f <= 0 or unordered -> the empty string. */
    if (f > kF300) {
        g_hud.szGap[0] = '+';
        BrSub_100020D0(&g_hud.szGap[1], f);
    } else {
        g_hud.szGap[0] = '\0';
    }
    return g_hud.szGap;
}

/* =====================================================================
 * 0x10017FE0
 * ===================================================================== */
/* WHAT IT DOES: draws one line of the split-time list -- a position number and
 * a time as minutes, seconds and hundredths -- at the given spot. */
/* @implements 0x10017FE0 d3d BrHudDrawSplitLine */
void BrHudDrawSplitLine(const char *pszPrefix, int rank, float fSeconds,
                        int x, int y)
{
    char szBuf[0x20];                 /* the original's stack buffer */
    int32_t total, hundredths, minutes, seconds;

#ifdef BR_MATCHING_BUILD
    /* 0x10015550..0x1001555E: `fld dword [esp+0xC]; fmul dword [kF334];
     * call __ftol`. A FLOAT multiply and the CRT helper -- the BrFtol
     * wrapper takes a double, so it pushes eight bytes and calls itself. */
    total      = (int32_t)(fSeconds * kF334);
#else
    total      = BrFtol((double)fSeconds * (double)kF334);
#endif
    /* RESIDUE (2 regnorm, -4 bytes, emitter-level): both builds factor
     * 100 the same way (x5 lea, x5 lea, x4), but the original NEGATES the
     * quotient first -- mov/neg/shl/sub/lea/lea giving -100q, then folds
     * the add into `lea esi,[esi + edx*4]`. Ours builds +100q and
     * subtracts, which is two instructions shorter. Probed and dead:
     * `total %% 100` (our cl emits a real idiv and stops CSEing the
     * quotient -- worse), `100 * (total/100)`, `(total/100) * -100`,
     * `total + -100 * (total/100)`, and an explicitly negated quotient
     * `(-(total/100)) * 100`. All four multiply spellings canonicalise to
     * the same +100q form. The seconds below use that same +form in the
     * original, so it is the hundredths that are the odd one out. */
    hundredths = total - (total / 100) * 100;   /* signed, truncating */
    total     /= 100;
    minutes    = total / 60;
    seconds    = total - minutes * 60;

    /* sprintf, not snprintf: the original calls the /MD CRT import and the
     * extra size argument is a whole push. The buffer is the original's own
     * 0x20 and the widest result fits. */
    sprintf(szBuf, "%s%d. %d:%02d.%02d",
            pszPrefix, rank, minutes, seconds, hundredths);

    BrTextDraw(szBuf, x, y);
}

/* =====================================================================
 * 0x10017F30
 * ===================================================================== */
void BrHudDrawSplitList(const BrHudView *aViews)
{
    const BrHudView *pView;
    int32_t x, y, bias, i;

    if (g_hud.f0BD3F0 == 0)
        return;

    x = g_screen.cx - 0x10;

    /* 10017F4B: dec/neg/sbb/and 0xFFFFFFE2/add 0x1E collapses to this. */
    bias = (g_screen.cViews == 1) ? 0x1E : 0;

    pView = &aViews[g_screen.iView];
    y = bias + (pView->y + 0x14) + 0x25;

    BrSub_10019260();
    BrSub_10019290();
    BrSub_100192F0(0x0F);

    /* The count is re-read from the race block on every iteration. */
    for (i = 0; i < g_hud.pRace->cSplits; ++i) {
        BrHudDrawSplitLine(g_hud.pszSplitPrefix, i + 1,
                           g_hud.pRace->aSplits[i], x, y);
        y += 0x0F;
    }
}

/* =====================================================================
 * 0x10017D90
 * ===================================================================== */
/* WHAT IT DOES: draws the whole in-race dashboard for one player's view: the
 * rev counter, the lap and position readouts, the split times, the race
 * messages, and the speed with its unit. Speed is shown in miles or kilometres
 * per hour depending on the player's setting, and a negative speed is shown as
 * zero. */
extern const char *BrStrGet(int id);   /* slice4_52.c, Glide 0x1006D280 */

/* @implements 0x10017D90 d3d BrHudDraw */
void BrHudDraw(BrHudView *aViews, int a2)
{
    const BrHudView *pView;
    const BrHudSprite *pSpr;
    float speed;
    int32_t x, y;

    /* speed FIRST: assigning it while the aViews arg slot is still unread
     * keeps VC5 from parking speed in the dead arg slot -- the original
     * gives it a real `push ecx` frame slot. */
    speed = g_hud.pRace->f1030;
    pView = &aViews[g_screen.iView];

    BrSub_1003289F(0, pView->y, g_screen.cx, pView->h);

    /* 10017DCF: negative (and NaN) speeds are pinned to zero.  Bare kF340:
     * the original is `fld dword [speed]; fcomp qword [kF340]` -- speed on
     * the fld side, the double constant as the fcomp operand.  A (float)
     * cast on kF340 flips the pair. */
    if (!(speed >= kF340))
        speed = 0.0f;

    BrHudDrawDial(aViews);
    BrSub_10017290(aViews);
    BrHudDrawSplitList(aViews);
    BrSub_100173F0(aViews, a2);
    BrHudDrawViewMessage(aViews);
    BrSub_10019260();
    BrSub_10019290();

    /* f0ADF60 selects miles: the number is scaled by 0.6213712 and the unit
     * string id changes from 0xEC to 0xEB.  TWO sprintf calls, not a
     * ternary: the compiler tail-merges them from `sub esp,8` on, leaving
     * each arm its own fld head (`fld;fmul;jmp` / `fld`) -- a ternary (or a
     * temp) head-merges the fld and loses the jmp.  sprintf itself (not
     * snprintf): the original calls the /MD CRT import, and the extra size
     * argument reshapes the whole push sequence.  "%%yw" is a text-markup
     * escape BrTextDraw consumes, not a printf directive. */
    if (g_hud.f0ADF60 != 0)
        sprintf(g_hud.szText, "%%yw%.0f", speed * kF348);
    else
        sprintf(g_hud.szText, "%%yw%.0f", speed);

    pView = &aViews[g_screen.iView];
    x = g_screen.cx - 0x10;
    y = pView->h + pView->y - 4;

    if (g_hud.f22AF1C != 0)
        return;
    if (g_hud.f0BD3F4 == 0)
        return;

    pSpr = BrHudSpriteAt(pView->iSprite);
    x -= 0x1E;
    y -= pSpr->e5;

    BrSub_100192F0(0x14);

    /* 10015433: `y -= 3` sits in BOTH arms (hoisting it above the if merges
     * the two into one pre-branch sub, -2 insns), and the km arm passes
     * `x - 3` as an EXPRESSION (`lea ecx,[esi-3]`) without touching x.
     * RESIDUE (2+2 regnorm, T3a-encoding): the original spells these two
     * mutations `add r,-3` where we emit `sub r,3` -- same length, same op.
     * Probed and failed: `+= -3`, `y = y - 3`, in-arg `y -= 3`, unsigned
     * x/y, hoisted common statement (regresses).  The RAW gap is the
     * esi/edi rotation downstream of the same two bytes. */
    if (g_hud.f0ADF60 != 0) {
        y -= 3;
        BrTextDraw(g_hud.szText, x, y);
    } else {
        y -= 3;
        BrTextDraw(g_hud.szText, x - 3, y);
    }

    BrSub_100192F0(0x0F);
    BrSub_10019280();

    /* 10015462: the unit string comes from BrStrGet (the one-argument
     * bounds-checked table lookup), and only the km arm mutates x. */
    if (g_hud.f0ADF60 != 0) {
        BrTextDraw(BrStrGet(0xEB), x, y);
    } else {
        x -= 3;
        BrTextDraw(BrStrGet(0xEC), x, y);
    }
}

/* =====================================================================
 * 0x10018070
 * ===================================================================== */
/* WHAT IT DOES: decides whether this frame's background is just a flat colour
 * rather than the sky. Five separate conditions each force the flat fill --
 * among them having no sky picture loaded at all. */
/* @implements 0x10018070 d3d BrSceneUsePlainClear */
#ifdef BR_MATCHING_BUILD
/* Loose globals, goto-shared return-1 tail with the last test inverted
 * (the fade-family shape). */
extern int DAT_106ed6ac, DAT_106ed6b0, DAT_106ed6b4;
extern int DAT_106eed28, DAT_100b3858;

int BrSceneUsePlainClear(void)
{
    if (DAT_106ed6ac != 0)
        goto yes;
    if (DAT_106ed6b0 != 0)
        goto yes;
    if (DAT_106ed6b4 != 0)
        goto yes;
    if (DAT_106eed28 == 0)
        goto yes;
    if (DAT_100b3858 != 2)
        return 0;
yes:
    return 1;
}
#else
int BrSceneUsePlainClear(void)
{
    if (g_scene.f6C661C == 0) {
        if (g_scene.f6C6620 == 0) {
            if (g_scene.f6C6624 == 0) {
                if (g_scene.f6C7C98 != 0) {
                    if (g_scene.f0B4050 != 2) {
                        return 0;
                    }
                }
            }
        }
    }
    return 1;
}
#endif

/* =====================================================================
 * 0x10017F60
 * ===================================================================== */
/* WHAT IT DOES: clears the two per-frame scene accumulators (g_4B16A0 and
 * g_4B16AC) back to zero.  The per-frame race render calls it once at the very
 * top of the frame -- right after BrSceneSetupFrame lays the background and just
 * before BrSpanBuildHull -- so the geometry pass (0x1000BEB0) accumulates into a
 * clean slate every frame.  Body is exactly two stores of 0.0f and a return.
 *
 * SOURCE: transcribed from the Glide build (asm/10010000.asm).  The D3D twin is
 * 0x1002AEF0 (shared, same 21 bytes) but its body sits in a run the D3D dump
 * folded into padding, so it is not the transcription source. */
/* @implements 0x10017F60 glide BrSceneAccumReset */
/* @n64 0x802237B4 located */
void BrSceneAccumReset(void)
{
    g_4B16AC = 0.0f;
    g_4B16A0 = 0.0f;
}

/* =====================================================================
 * 0x100180B0
 * ===================================================================== */
/* WHAT IT DOES: lays down the background before anything else in the frame is
 * drawn. That is either a flat colour wash over the player's view -- brightened
 * for one frame on alternate lightning flashes, which is what makes a storm
 * flicker -- or the sky drawn through the camera's current view. */
/* @implements 0x100180B0 d3d BrSceneSetupFrame */
void BrSceneSetupFrame(const BrHudView *aViews)
{
    const BrHudView *pView;
    BrGfxCmd *p;
    BrMat4 *pDst;
    uint32_t bitsB7, bitsB6;

    BrGfxEmit(0xBC000404u, 0x00000001u);
    BrGfxEmit(0xBC000C04u, 0x00000001u);
    BrGfxEmit(0xBC001404u, 0x0000FFFFu);
    BrGfxEmit(0xBC001C04u, 0x0000FFFFu);

    if (g_scene.f6C6608 != 0)
        return;

    if (BrSceneUsePlainClear()) {
        int32_t c0, c1, c2;

        pView = &aViews[g_screen.iView];

        /* SHIFT, NOT DIVIDE.  The original ends each of the three brightened
         * components with a bare `sar reg,2`; a signed `/ 4` makes MSVC emit
         * the round-toward-zero correction (`cdq; and edx,3; add; sub`) --
         * three `cdq`s, and 11 of the register-blind gap.  Same value for the
         * non-negative inputs these always have.
         *
         * 1001814B: `test al,1` -- the flat fill brightens on ODD lightning
         * counts only, and only while the counter is still positive. */
        if (g_scene.f0A79CC > 0 && (g_scene.f0A79CC & 1) != 0) {
            c0 = (((int32_t)g_scene.c6C0200 + 0x55) * 3) >> 2;
            c1 = ((int32_t)g_scene.c6C1614 * 3 + 0xF8) >> 2;
            c2 = (((int32_t)g_scene.c6C0260 + 0x50) * 3) >> 2;
        } else {
            c0 = g_scene.c6C0200;
            c1 = g_scene.c6C1614;
            c2 = g_scene.c6C0260;
        }
        BrSub_10031688(pView->x, pView->y, pView->w, pView->h, c2, c1, c0);
        return;
    }

    BrSub_10031140(&g_scene.mtx, g_scene.pCam->f30, g_scene.pCam->f34,
                   (float)((double)g_scene.pCam->f38 * (double)kF34C));

    pDst = BrSub_10069490();
    BrMat4Copy(&g_scene.mtx, pDst);      /* source first -- see br_mat.h */

    BrGfxEmit(0x01030040u, (uint32_t)g_scene.f6C32D0);
    BrGfxEmit(0x01040040u, BrGfxAddr(pDst));   /* DEVIATION: 32-bit address */
    BrGfxEmit(0xE7000000u, 0u);
    BrGfxEmit(0xBA001402u, 0u);

    /* THE 0x1002F900 BLOCK IS WRITTEN OUT IN BOTH ARMS, not hoisted above the
     * if.  Behaviourally the two are the same -- which is what the old note
     * here said, and why it was hoisted -- but the original calls the 17-arg
     * emitter TWICE (two `add esp,0x44` sites, at +0x213 and +0x288) and a
     * hoisted call can only ever be one of them.  Tell for this class: count
     * `call`s and stack adjusts in the original before trusting a "both
     * branches do X" comment -- the original has SEVEN calls and TWO
     * `add esp,0x44`; the hoisted version had six and one.
     *
     * MEASURED AT THIS TU'S OWN VARIANT (/O2 /Op -- fn.py compiles /O2 only,
     * so its numbers here are partly phantom): register-blind gap 37+40 ->
     * 26+18 for this fix together with the shift below.  Note the raw byte
     * diff moved the WRONG way, 819 -> 821, size went from 38 short to 7
     * short, and the instruction count from 3 under to 8 over.  That is the
     * documented pattern -- rank by the register-blind multiset, never by
     * size -- but it does mean the two blocks are not yet shaped the way the
     * original shares them.
     *
     * RESIDUE: recomp EXTRA is 4 dword global reads where the original
     * byte-loads (it reads 0x106e7290 / 0x106e86a4 / 0x106e72f0 three times
     * each and 0x106b7c78 once, always `xor r,r; mov rl,[g]`), plus a
     * `mov R,R` / `mov B,B` / `and R,I` cluster -- the byte-slot idiom.  DEAD
     * probe, do not re-run: nesting the 0xFB word's shifts so two bytes pack
     * into one register before the shl pair -- VC5 canonicalises the `|`
     * chain and it compiles byte-identical. */
    if (g_scene.f6C6618 != 0) {
        p = BrGfxAlloc();
        BrSub_1002F900(p, 0, 0, 0, 0x3E9, 0, 0, 0, 0x3EC,
                          0, 0, 0, 0x3E9, 0, 0, 0, 0x3EC);
        p = BrGfxAlloc();
        p->w0 = 0xFB000000u;
        p->w1 = ((uint32_t)g_scene.c6C0260 << 24)
              | ((uint32_t)g_scene.c6C1614 << 16)
              | ((uint32_t)g_scene.c6C0200 << 8)
              |  (uint32_t)g_scene.c690BE8;
    } else {
        p = BrGfxAlloc();
        BrSub_1002F900(p, 0, 0, 0, 0x3E9, 0, 0, 0, 0x3EC,
                          0, 0, 0, 0x3E9, 0, 0, 0, 0x3EC);
    }

    BrGfxEmit(0xB900031Du, 0x0F0A4200u);
    BrGfxEmit(0xBA000C02u, (uint32_t)g_scene.f6C0258);
    BrGfxEmit(0xB6000000u, 0x000F0205u);

    if (g_scene.f6C6618 != 0)
        BrGfxEmit(0xB7000000u, 0x00010000u);

    /* neg/sbb turns "the two globals differ" into a mask. */
    bitsB7 = (g_scene.f6C3364 != g_scene.f6C1174) ? 0x1000u : 0x2000u;
    BrGfxEmit(0xB7000000u, bitsB7);
    bitsB6 = (g_scene.f6C3364 != g_scene.f6C1174) ? 0x2000u : 0x1000u;
    BrGfxEmit(0xB6000000u, bitsB6);

    BrGfxEmit(0xBA001001u, 0u);
    BrGfxEmit(0xBB000001u, 0xFFFFFFFFu);
    BrGfxEmit(0xB6000000u, 0x000C0000u);
    BrGfxEmit(0xE8000000u, 0u);
    BrGfxEmit(0xF5100000u, 0x07000000u);
    BrGfxEmit(0xF50001F0u, 0x06000000u);
    BrGfxEmit(0xF5000100u, 0x05000000u);
    BrGfxEmit(0x06000000u, (uint32_t)g_scene.f6C7C98);
    BrGfxEmit(0xE7000000u, 0u);
    BrGfxEmit(0xBA001402u, 0u);
    BrGfxEmit(0xB7000000u, 0x00020205u);
    BrGfxEmit(0xBD000000u, 0u);
}

/* =====================================================================
 * 0x10019490
 * ===================================================================== */
/* WHAT IT DOES: scatters the rain or snow to random positions, which is what
 * gets the weather started and what resets it when the view jumps. Each layer
 * is filled with 512 particles; the handful of spare slots past that are left
 * as they were. */
/* @implements 0x10019490 d3d BrWeatherRandomiseParticles */
/* @n64 0x80239CF0 located */
void BrWeatherRandomiseParticles(void)
{
    int layer, i;

    g_weather.cParticles = 0x200;

    for (layer = 0; layer < BR_PARTICLE_LAYERS; ++layer) {
        /* Init p to [layer][0][2]: orig uses [p-4],[p-2],[p+0] writes,
         * generating the store sequence 0xFC,0xFE,0x06 that the binary has. */
        int16_t *p = &g_weather.aParticles[layer][0][2];
        for (i = 0; i < BR_PARTICLES_PER_LAYER; ++i) {
            p[-2] = (int16_t)BrRandom();
            p[-1] = (int16_t)BrRandom();
            p[0]  = (int16_t)BrRandom();
            p += 3;
        }
    }
}

/* =====================================================================
 * 0x100194E0
 * ===================================================================== */
/* WHAT IT DOES: drifts the wind on by one frame. Both its direction and its
 * strength wander randomly rather than being set anywhere -- the direction
 * wraps round the compass and the strength is held between half and full -- and
 * the result is turned into the horizontal push the snow is blown by. */
/* @implements 0x10016AA0 glide BrWeatherStepWind */
/* @implements 0x100194E0 d3d BrWeatherStepWind */
void BrWeatherStepWind(void)
{
    float r, a;

    /* orig: fild; fmul kF350; fsub 1; fmul dt; fadd angle; fst; fcomp 2pi.
     * All memory operands are float -- double casts emit fld+fmulp. */
    r = (float)(BrRandom() & 0xFFFF) * kF350 - kF354;
    g_weather.windAngle = r * g_weather.dt + g_weather.windAngle;
    if (g_weather.windAngle >= kF358)
        g_weather.windAngle -= kF358;
    else if (g_weather.windAngle < kF35C)
        g_weather.windAngle -= kF360;   /* -(-2pi) = +2pi */

    r = (float)(BrRandom() & 0xFFFF) * kF350 - kF354;
    g_weather.windGain = r * g_weather.dt + g_weather.windGain;
    /* Then-arms are integer stores of 1.0f / 0.5f (mov imm32), not x87. */
    if (g_weather.windGain > kF354)
        g_weather.windGain = 1.0f;
    else if (g_weather.windGain < kF364)
        g_weather.windGain = 0.5f;

    /* Integer-home of the angle (mov eax; mov [esp], eax; fld [esp]),
     * windZ zeroed between the copy and the two flds, fcos of the global
     * and fsin of the slot copy, then scale both by gain then dt.
     *
     * WALL: orig then `fxch; fst [esp]; fxch; fst [esp]; fxch` (round BOTH
     * fcos/fsin results through the one slot) before the interleaved
     * gain/dt scale-out.  /O2 emits only the sin fst; /Op on the function
     * also rounds the fild path (`fstp; fld`) which orig does not. */
    {
        int ia = *(int *)&g_weather.windAngle;
        g_weather.windZ = 0.0f;
        *(int *)&a = ia;
        g_weather.windX = (float)cos(g_weather.windAngle) * g_weather.windGain * g_weather.dt;
        g_weather.windY = (float)sin(a) * g_weather.windGain * g_weather.dt;
    }
}

/* =====================================================================
 * 0x10019620
 * ===================================================================== */
/* WHAT IT DOES: runs a lightning strike. Most frames it just rolls the dice --
 * about one chance in five hundred -- and on a hit it picks a random spot in
 * the sky and lights it for three frames. After the flash it tracks the thunder
 * travelling outward at the speed of sound, and once that has gone far enough
 * it starts watching for the next strike. */
/* @implements 0x10019620 d3d BrWeatherStepLightning */
#ifdef BR_MATCHING_BUILD
extern int    DAT_100a718c;
extern float  DAT_104add3c;
/* DECLARATION ORDER IS LOAD-BEARING: in a both-memory fmul, the
 * LATER-declared symbol takes the fld side. 31c first so dt gets fld. */
extern float  DAT_1007731c;
extern float  DAT_106e9d8c;
extern double DAT_10077320;
extern float  DAT_104abb60, DAT_104abb64;
extern int    DAT_104abb68, DAT_106eed10;

void BrWeatherStepLightning(void)
{
    int n = DAT_100a718c;

    if (n >= 0) {
        DAT_104add3c = DAT_104add3c - DAT_106e9d8c * DAT_1007731c;
        if (n > 0) {
            DAT_100a718c = n - 1;
            return;
        }
        if (DAT_104add3c > DAT_10077320)
            DAT_100a718c = -1;
        return;
    }

    if ((int)(BrRandom() & 0xFFFF) < 0x80) {
        DAT_104add3c = 0;
        DAT_100a718c = 3;
        DAT_104abb60 = (float)(BrRandom() & 0x7FF);
        DAT_104abb64 = (float)(BrRandom() & 0x7FF);
        DAT_104abb68 = DAT_106eed10;
    }
}
#else
void BrWeatherStepLightning(void)
{
    if (g_weather.lightning < 0) {
        /* 1001966D: 0x80 in 0x10000 per call. */
        if ((BrRandom() & 0xFFFF) >= 0x80)
            return;
        g_weather.thunderDist = 0.0f;
        g_weather.lightning   = 3;
        g_weather.flashX = (float)(BrRandom() & 0x7FF);
        g_weather.flashZ = g_weather.f6C7C80;   /* latched before flashY */
        g_weather.flashY = (float)(BrRandom() & 0x7FF);
        return;
    }

    /* fsubr against -343 * dt, i.e. the distance the thunder has travelled. */
    g_weather.thunderDist = (float)((double)g_weather.thunderDist
                                    - (double)g_weather.dt * (double)kF36C);

    if (g_weather.lightning > 0) {
        g_weather.lightning -= 1;
        return;
    }

    /* 1001964E: C0|C3 -- <= 2048 (or unordered) keeps the strike alive. */
    if ((double)g_weather.thunderDist > kF370)
        g_weather.lightning = -1;
}
#endif

/* =====================================================================
 * 0x100196D0
 * ===================================================================== */
/* WHAT IT DOES: moves the rain or snow on by one frame for each player's view.
 * The particles are carried along with the camera, so what the player sees is
 * weather falling past them rather than weather they drive through; rain falls
 * straight down while snow is pushed sideways by the wind. Very fast camera
 * movement is damped so the weather does not streak. If neither rain nor snow
 * is on it abandons the whole loop, not just the current view. */
/* @implements 0x100196D0 d3d BrWeatherStepParticles */
void BrWeatherStepParticles(void)
{
    int32_t iView;

    BrWeatherStepWind();

    if (g_weather.storm != 0)
        BrWeatherStepLightning();

    /* 0x200 split between the views.
     * DEVIATION: the original divides BEFORE testing cViews, so cViews == 0
     * faults on the idiv. Guarded here. */
    g_weather.cParticles = (g_screen.cViews != 0)
                         ? (int32_t)(0x200 / g_screen.cViews)
                         : 0;
    if (g_screen.cViews <= 0)
        return;

    for (iView = 0; iView < g_screen.cViews; ++iView) {
        const BrCamBlock *pBlk = g_weather.pfnGetBlock(iView);
        BrVec3 pos;
        BrVec3 d;
        float dx, dy, dz;
        /* DEVIATION: the original walks a raw cursor, so cViews > 2 would run
         * off the end of the two-layer particle field. Clamped. */
        int32_t layer = (iView < BR_PARTICLE_LAYERS)
                      ? iView : (BR_PARTICLE_LAYERS - 1);

        /* out = block->v30 + block->v00 * 3.0f */
        BrVec3MulAdd(&pos, &pBlk->v30, &pBlk->v00, 3.0f);

        if (g_weather.fInit == 0) {
            BrWeatherRandomiseParticles();
            g_weather.aPrev[0] = pos;
            g_weather.aPrev[1] = pos;
            g_weather.fInit = 1;
        }

        /* Both off -> the whole loop is abandoned, not just this view. */
        if (g_weather.rain == 0 && g_weather.storm == 0)
            return;

        dx = pos.x - g_weather.aPrev[iView].x;
        dy = pos.y - g_weather.aPrev[iView].y;
        dz = pos.z - g_weather.aPrev[iView].z;

        g_weather.speed = (float)(sqrt((double)dx * dx
                                     + (double)dy * dy
                                     + (double)dz * dz)
                                  / (double)g_weather.dt);

        if (g_weather.speed > kF378) {
            double k = sqrt((double)g_weather.speed * (double)kF37C)
                     * (double)kF378 / (double)g_weather.speed;
            g_weather.k = (float)k;
            g_weather.speed = (float)(k * (double)g_weather.speed);
            dx = (float)((double)dx * (double)g_weather.k);
            dy = (float)((double)dy * (double)g_weather.k);
            dz = (float)((double)dz * (double)g_weather.k);
        } else {
            g_weather.k = 1.0f;
        }

        if (g_weather.rain != 0) {
            /* Rain falls straight: dz picks up -0.5 * dt and nothing else. */
            dz = (float)((double)dz - (double)g_weather.dt * (double)kF368);
        } else {
            /* Snow drifts on the wind. */
            double a  = (double)g_weather.windAngle;
            double cw = cos(a) * (double)g_weather.windGain
                                * (double)g_weather.dt;
            double sw = sin(a) * (double)g_weather.windGain
                                * (double)g_weather.dt;
            double dt2 = (double)g_weather.dt + (double)g_weather.dt;

            d.x = dx;
            d.y = dy;
            d.z = dz;

            g_weather.aDrift[iView].x = (float)cw;
            g_weather.aDrift[iView].y = (float)sw;
            g_weather.aDrift[iView].z = (float)dt2;

            dx = (float)((double)dx + (double)g_weather.aDrift[iView].x);
            dy = (float)((double)dy + (double)g_weather.aDrift[iView].y);
            dz = (float)((double)dz + dt2);

            BrVec3ScaleBy(&d, (float)((double)g_weather.k * (double)kF364));
            BrVec3AddTo(&g_weather.aDrift[iView], &d);
        }

        {
            float j0, j1;
            int32_t D0, D1, D2, R1, R2;
            int32_t i, cx, cy;

            if (g_weather.rain != 0) {
                double r;
                r = (double)(BrRandom() & 0xFFFF) * (double)kF350
                    - (double)kF354;
                j0 = (float)(r * (double)g_weather.dt * (double)kF384);
                r = (double)(BrRandom() & 0xFFFF) * (double)kF350
                    - (double)kF354;
                j1 = (float)(r * (double)g_weather.dt * (double)kF384);
            } else {
                j0 = 0.0f;
                j1 = 0.0f;
            }

            g_weather.aPrev[iView] = pos;

            cx = BrRandom() & 0xF;
            cy = BrRandom() & 0xF;

            D0 = BrFtol((double)dx * kF388);
            D1 = BrFtol((double)dy * kF388);
            D2 = BrFtol((double)dz * kF388);
            R1 = BrFtol((double)j0 * kF388);
            R2 = BrFtol((double)j1 * kF388);

            for (i = 0; i < g_weather.cParticles; ++i) {
                int16_t *pRec = g_weather.aParticles[layer][i];

                /* Every cx'th particle gets the jitter folded in and the
                 * jitter's SIGN flipped, then a fresh countdown. */
                if (cx != 0) {
                    pRec[0] = (int16_t)((uint16_t)pRec[0] + (uint16_t)D0);
                    --cx;
                } else {
                    pRec[0] = (int16_t)((uint16_t)pRec[0]
                                        + (uint16_t)(R1 + D0));
                    R1 = -R1;
                    cx = BrRandom() & 0xF;
                }

                if (cy != 0) {
                    pRec[1] = (int16_t)((uint16_t)pRec[1] + (uint16_t)D1);
                    --cy;
                } else {
                    pRec[1] = (int16_t)((uint16_t)pRec[1]
                                        + (uint16_t)(R2 + D1));
                    R2 = -R2;
                    cy = BrRandom() & 0xF;
                }

                pRec[2] = (int16_t)((uint16_t)pRec[2] + (uint16_t)D2);
            }
        }
    }
}

/* =====================================================================
 * 0x1001A4B0
 * ===================================================================== */
/* WHAT IT DOES: purpose unclear. Observably it hands one entry of a table,
 * chosen by number, plus two fixed globals, to another routine and does nothing
 * else. What that routine does with them is not established here. */
/* @d3donly 0x1001A4B0 BrForward1001A4B0 -- absent from BRGlide (D3D-only / dynamically-imported CRT); no Glide twin exists */
void BrForward1001A4B0(int i)
{
    BrSub_100290A0(&g_weather.f2554, &g_weather.f2558,
                   g_weather.apTable[i]);
}

/* =====================================================================
 * 0x1001BB80 .. 0x1001BC50
 * ===================================================================== */
void BrRdpCacheScreenWidth(void)  { g_regs.f4C5164 = g_screen.cx; }
void BrRdpCacheScreenHeight(void) { g_regs.f4C01A0 = g_screen.cy; }
void BrRdpCacheHalfWidthA(void)   { g_regs.f4BBF08 = (float)(g_screen.cx / 2); }
void BrRdpCacheHalfWidthB(void)   { g_regs.f4C0BB0 = (float)(g_screen.cx / 2); }
void BrRdpCacheHalfHeight(void)   { g_regs.f4C0BB8 = (float)(g_screen.cy / 2); }

/* =====================================================================
 * Command handlers
 * ===================================================================== */

const BrGfxCmd *BrCmdDispatchIndirect(const BrGfxCmd *pCmd)
{
    g_regs.pfn18AA0B8(pCmd->w0 & 0x00FFFFFFu, pCmd->w1);
    return pCmd + 1;
}

/* Sign-extend the low `bits` of v, arithmetically. */
static int32_t BrSext(uint32_t v, int bits)
{
    uint32_t m = 1u << (bits - 1);
    return (int32_t)((v & (m + m - 1u)) ^ m) - (int32_t)m;
}

const BrGfxCmd *BrCmdRectFixed(const BrGfxCmd *pCmd)
{
    /* 10.2 fixed point: sign-extend 12 bits, arithmetic >>2, mask to 10. */
    int32_t y1 = (BrSext(pCmd->w1, 12)       >> 2) & 0x3FF;
    int32_t x1 = (BrSext(pCmd->w1 >> 12, 12) >> 2) & 0x3FF;
    int32_t y2 = (BrSext(pCmd->w0, 12)       >> 2) & 0x3FF;
    int32_t x2 = (BrSext(pCmd->w0 >> 12, 12) >> 2) & 0x3FF;

    BrSub_1001BE90(x1, g_screen.cy - y2 - 1, x2 + 1, g_screen.cy - y1);
    return pCmd + 1;
}

const BrGfxCmd *BrCmdRectInt(const BrGfxCmd *pCmd)
{
    /* Same fields, but plain signed integers -- no >>2 and no mask. */
    int32_t y1 = BrSext(pCmd->w1, 12);
    int32_t x1 = BrSext(pCmd->w1 >> 12, 12);
    int32_t y2 = BrSext(pCmd->w0, 12);
    int32_t x2 = BrSext(pCmd->w0 >> 12, 12);

    BrSub_1001BE90(x1, g_screen.cy - y2 - 1, x2 + 1, g_screen.cy - y1);
    return pCmd + 1;
}

const BrGfxCmd *BrCmdLatchPair(const BrGfxCmd *pCmd)
{
    g_regs.f4C5158 = pCmd->w0;
    g_regs.f4C515C = pCmd->w1;
    BrSub_1001C820(pCmd->w0, pCmd->w1);
    return pCmd + 1;
}

/* The two colour handlers share this shape: four bytes of w1, high first,
 * each through a float32 store/reload and then scaled by 1/255. */
static void BrCmdUnpackColor(uint32_t w1, float *pa, float *pb,
                             float *pc, float *pd)
{
    *pa = (float)((double)(float)(int32_t)( w1 >> 24)          * (double)kF3C0);
    *pb = (float)((double)(float)(int32_t)((w1 >> 16) & 0xFFu) * (double)kF3C0);
    *pc = (float)((double)(float)(int32_t)((w1 >>  8) & 0xFFu) * (double)kF3C0);
    *pd = (float)((double)(float)(int32_t)( w1        & 0xFFu) * (double)kF3C0);
}

/* @n64 0x8026C5C0 located */
const BrGfxCmd *BrCmdSetColorA(const BrGfxCmd *pCmd)
{
    BrCmdUnpackColor(pCmd->w1, &g_regs.f4BBF04, &g_regs.f4C0BAC,
                     &g_regs.f4BBEB8, &g_regs.f4BBE2C);
    return pCmd + 1;
}

const BrGfxCmd *BrCmdSetColorB(const BrGfxCmd *pCmd)
{
    BrCmdUnpackColor(pCmd->w1, &g_regs.f4C5154, &g_regs.f4C5160,
                     &g_regs.f4C1690, &g_regs.f4C0BA8);
    return pCmd + 1;
}

const BrGfxCmd *BrCmdUnpackModeBits(const BrGfxCmd *pCmd)
{
    uint32_t w1 = pCmd->w1;
    uint8_t a, b;

    /* xor/and 7/xor: splice the low three bits of one shift into another. */
    a = (uint8_t)(w1 >> 8);
    b = (uint8_t)(w1 >> 13);
    g_regs.c4BBF00 = (uint8_t)(a ^ ((uint8_t)((b ^ a) & 7u)));

    a = (uint8_t)(w1 >> 3);
    b = (uint8_t)(w1 >> 8);
    g_regs.c4BC194 = (uint8_t)(a ^ ((uint8_t)((b ^ a) & 7u)));

    /* The shift is 8-bit, so w1 bits 6 and 7 fall off before the or. */
    g_regs.c4C5150 = (uint8_t)((uint8_t)((uint8_t)(w1 & 0xFEu) << 2)
                               | (uint8_t)((w1 >> 3) & 7u));

    g_regs.c4C15CC = (uint8_t)((w1 & 1u) ? 0xFFu : 0x00u);
    return pCmd + 1;
}

/* ── Ghidra-matched functions ─────────────────────────── */
#ifdef BR_MATCHING_BUILD
extern int DAT_104ab4f0;
extern int DAT_104ab504;

/* WHAT IT DOES: clear a one-shot flag (zeroes it if set, idempotent). */
/* @implements 0x10013F00 glide BrClearFlag_AB504 */

int BrClearFlag_AB504(void)

{
  if (DAT_104ab504 != 0) {
    DAT_104ab504 = 0;
  }
  return;
}

/* WHAT IT DOES: return the value of a global flag at 0x104AB4F0. */
/* @implements 0x10013FC0 glide BrGetFlag_AB4F0 */

int BrGetFlag_AB4F0(void)

{
  return DAT_104ab4f0;
}

extern int DAT_100a5eb0;
extern int DAT_100a5ebc;
extern int DAT_106e7714;
extern int DAT_106e9a2c;
int BrSetGlobal_ABB30();
int BrSet_10019270();
int BrTextFlag358Clear();
int BrTextSetColors();

/* WHAT IT DOES: draw the on-screen text list at 0x100A5EB0 (16-byte records:
 * y, color-arg, ?, text): white colors, viewport from the caller's rect,
 * then centre-draw each record whose y sits inside (-0x50, height+0x28). */
/* @implements 0x10013140 glide BrHudTextListDraw */

void BrHudTextListDraw(int *param_1)

{
  int iVar1;
  int *piVar2;

  BrTextSetColors(0xff,0xff,0xff,0xff,0xff,0xff);
  BrTextFlag358Clear();
  BrSet_10019270();
  BrSub_1003289F(*param_1,param_1[1],param_1[2],param_1[3]);
  if (DAT_100a5ebc != 0) {
    piVar2 = &DAT_100a5eb0;
    do {
      if ((*piVar2 > -0x50) && (*piVar2 < DAT_106e9a2c + 0x28)) {
        BrSetGlobal_ABB30(piVar2[1]);
        BrTextDraw((const char *)piVar2[3],DAT_106e7714 / 2,*piVar2);
      }
      iVar1 = piVar2[7];
      piVar2 = piVar2 + 4;
    } while (iVar1 != 0);
  }
  return;
}


extern int DAT_104abb20;
extern float DAT_106e9d8c;
extern float _DAT_104abb24;
extern float kF300_S_S537;

/* WHAT IT DOES: fade a level down by one step per call and, once it reaches
 * the floor, snap it to zero and drop the pointer that was driving it. The
 * tail end of a fade-out. */
/* @implements 0x10014CB0 glide FUN_10014cb0 */
/* @n64 0x8021F1A4 located */
/* auto-filed from ghidra --refine; transforms: kF300_S_S537:float */

void FUN_10014cb0(void)

{
  if ((_DAT_104abb24 != kF300_S_S537) &&
     (_DAT_104abb24 = _DAT_104abb24 - DAT_106e9d8c, _DAT_104abb24 <= kF300_S_S537)) {
    _DAT_104abb24 = 0.0;
    DAT_104abb20 = 0;
  }
  return;
}

#endif /* BR_MATCHING_BUILD */
