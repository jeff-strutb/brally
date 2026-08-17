/* br_drawcar.c -- see br_drawcar.h.  The vehicle's display list, from
 * BRGlide.dll.
 *
 * Read off the GLIDE build, which is this project's reference.  Both
 * functions here are classed `shared` in config/shared.csv, so the D3D
 * twins (0x1000C6E0 and 0x1000CBE0) are the same code under other numbers.
 */
#include "br_drawcar.h"
#include "slice1_05.h"   /* BrGfxWords, BrRdpSetCombineLERP, BrMat4Mul   */
#include "slice2_18.h"   /* BrG_6C0680 -- the display-list write cursor  */

#include <string.h>

/* ------------------------------------------------------------------ *
 * Storage.  Every address below was grepped across port/include and
 * port/src before a name was coined; none of them has another host model.
 * ------------------------------------------------------------------ */
uint32_t g_BrDrawRenderMode;   /* 0x10273644 */
int32_t  g_BrDrawFogAlpha;     /* 0x102735FC */
int32_t  g_BrDrawWheelAlt;     /* 0x106ED6B0 */
BrMat4   g_BrDrawScale;        /* 0x106E7930 */
BrMat4   g_BrDrawWorld;        /* 0x10273570 */
BrMat4   g_BrDrawView;         /* 0x106E9A38 */
BrMat4   g_BrDrawCombined;     /* 0x106E78F0 */

static BrDrawCarHooks s_hooks;
static int32_t        s_cFrontier;

void BrDrawCarSetHooks(const BrDrawCarHooks *pHooks)
{
    if (pHooks) s_hooks = *pHooks;
    else        memset(&s_hooks, 0, sizeof(s_hooks));
}
int32_t BrDrawCarFrontierHits(void) { return s_cFrontier; }
void    BrDrawCarFrontierReset(void) { s_cFrontier = 0; }

/* ------------------------------------------------------------------ *
 * The eight-byte append.
 *
 * This is not a function in the original -- it is inlined at all 177 sites
 * across these two routines, always as the same five instructions:
 *
 *     mov ecx, [0x106E7710]      ; ... [0x106C0680] in the D3D build
 *     mov eax, ecx
 *     add ecx, 8
 *     mov [0x106E7710], ecx
 *     mov [eax], w0  /  mov [eax+4], w1
 *
 * so it carries no @implements line.  The cursor itself is slice2_18's
 * BrG_6C0680, deliberately: 0x106C0680 and 0x106E7710 are one object under
 * the two builds' numbers, and a second host model of it would be the
 * aliased-storage bug CONVENTIONS.md documents.
 * ------------------------------------------------------------------ */
static void put(uint32_t w0, uint32_t w1)
{
    uint32_t *p = BrG_6C0680;
    BrG_6C0680 += 2;
    p[0] = w0;
    p[1] = w1;
}

/* The command the combiner builder writes into.  The original bumps the
 * cursor BEFORE the call and hands the routine the old slot, so a caller
 * that inspects the cursor mid-flight sees it already advanced. */
static BrGfxWords *put_slot(void)
{
    BrGfxWords *p = (BrGfxWords *)BrG_6C0680;
    BrG_6C0680 += 2;
    return p;
}

/* Combiner tokens, as the game spells them.  0x1001D180/0x1001D150 map
 * 0x3E8 + n to n, and 0 to G_CCMUX_0 / G_ACMUX_0. */
#define TK_ZERO       0
#define TK_COMBINED   0x3E8
#define TK_TEXEL0     0x3E9
#define TK_TEXEL1_A   0x3EB
#define TK_PRIMITIVE  0x3EC
#define TK_SHADE      0x3ED

/* ------------------------------------------------------------------ *
 * 0x1002A9F2 -- five bytes: push ebp / mov ebp,esp / pop ebp / ret.
 *
 * It is EMPTY in this build, and that is a fact about the shipped image,
 * not a gap in this port: the bytes are `55 8B EC 5D C3` and there is no
 * body to transcribe.  0x1000A110 calls it once, with the combined matrix,
 * and ignores the result.  Kept as a real function because deleting the
 * call would silently change what 0x1000A110's transcription looks like
 * next to the disassembly.
 * ------------------------------------------------------------------ */
/* WHAT IT DOES: nothing.  It is a hook the shipped build left empty -- the
 * game hands it a finished transform and it returns immediately. */
/* @implements 0x1002A9F2 glide BrGuMtxHookNop */
void BrGuMtxHookNop(const BrMat4 *pM)
{
    (void)pM;
}

/* ------------------------------------------------------------------ *
 * 0x10029E50 -- copy a 4x4 matrix, sixteen dwords.
 *
 *     esi = arg2 (DESTINATION), edi = arg1 (SOURCE), ecx = edi - esi
 *     outer x4:  ecx is recomputed from edi - esi every pass
 *       inner x4:  [eax] = [ecx + eax]; eax += 4
 *
 * eax is NOT reset by the outer loop, so the two nested counts are only a
 * 4x4 spelling of sixteen consecutive dwords -- exactly the trap
 * CONVENTIONS.md records for 0x10074B20, checked the same way: the outer
 * loop's jump target is 0x10029E63, which is AFTER `mov eax, esi`.
 *
 * The argument order is (source, destination), the opposite of memcpy and
 * of every routine in br_vec.h.  Preserved.
 * ------------------------------------------------------------------ */
/* WHAT IT DOES: copies one transform into a slot the graphics list will
 * point at, so the list keeps its own snapshot of where a thing was. */
/* @implements 0x10029E50 glide BrGuMtxStore */
void BrGuMtxStore(const void *pSrc, void *pDst)
{
    const uint32_t *s = (const uint32_t *)pSrc;
    uint32_t       *d = (uint32_t *)pDst;
    int i;
    for (i = 0; i < 16; ++i)
        d[i] = s[i];
}

/* A pooled matrix, as the display list must name it.  The pool itself is
 * already transcribed under its D3D address in slice5_62.c; this is the
 * bridge from that module's host pointer to the 32-bit number the command
 * word carries.  With no hook installed it reports "no matrix" and counts
 * the reach -- it does not invent an address. */
static BrMat4 *mtx_alloc(uint32_t *pAddr)
{
    BrMat4 *pM;
    if (!s_hooks.pfnMtxAlloc || !s_hooks.pfnDlAddr) {
        ++s_cFrontier;
        *pAddr = 0;
        return 0;
    }
    pM = s_hooks.pfnMtxAlloc();
    *pAddr = pM ? s_hooks.pfnDlAddr(pM) : 0;
    return pM;
}

/* ==================================================================== *
 * 0x10009C10 -- the four wheels.
 *
 * Four identical passes over the four 0x40-byte matrices at car+0x40,
 * car+0x80, car+0xC0 and car+0x100.  Each pass:
 *
 *   - resets the pipe and puts the RDP in two-cycle mode;
 *   - picks a colour combiner and a render mode from the car's draw class
 *     at +0x29AF -- class 2 (the translucent pass) gets an extra
 *     G_SETENVCOLOR carrying the car's alpha and the render-mode word
 *     0x104A50, everything else gets 0x112230 and no env colour;
 *   - scales the wheel's transform by 1/255 and multiplies it into the
 *     view, pushing BOTH the model matrix and the four 16-byte blocks of
 *     the combined one (0x9E/0x98/0x9A/0x9C) into the list;
 *   - turns texturing on, clears the two texture-generate bits, loads
 *     three tiles, calls the model's wheel display list, and pops.
 *
 * The class-2 arm emits its render mode BEFORE the combiner and the other
 * arm emits it AFTER; that is not a tidying opportunity, it is the order
 * the bytes are in (0x10009C9C vs 0x10009D60).
 *
 * The whole function is gated on the model's +0x80BC being non-zero, and
 * that gate is read ONCE, before the loop, from the global 0x106EA398 --
 * which 0x1000A110's prologue sets from the car's +0x29C4.  The port takes
 * the model as an argument instead of reading a global that 0x1000A110
 * would own; that is the only departure and it is visible in the
 * signature.
 * ==================================================================== */
/* WHAT IT DOES: draws a car's four wheels.  Each wheel has its own
 * position and spin, so each gets its own turn through the same wheel
 * shape, and a see-through car gets its transparency applied to them too. */
/* @implements 0x10009C10 glide BrCarDrawWheels */
void BrCarDrawWheels(const BrCarView *pCar, const BrModelView *pModel)
{
    int pass;

    /* 0x10009C19 -- the gate, read once and from the model, not per pass */
    if (pModel->dlWheel == 0)
        return;

    for (pass = 0; pass < 4; ++pass) {
        const BrMat4 *pWheel = &pCar->aWheel[pass];
        BrMat4   *pSlot;
        uint32_t  addr;

        put(0xE7000000u, 0);                    /* pipe sync            */
        put(0xBA001402u, 0x00100000u);          /* two-cycle            */

        if (pCar->bKind == 2) {
            put(0xB900031Du, g_BrDrawRenderMode | 0x00104A50u);
            put(0xFB000000u, (uint32_t)g_BrDrawFogAlpha & 0xFFu);
            BrRdpSetCombineLERP(put_slot(),
                TK_TEXEL0, TK_ZERO, TK_PRIMITIVE, TK_ZERO,
                TK_ZERO,   TK_ZERO, TK_ZERO,      TK_TEXEL0,
                TK_ZERO,   TK_ZERO, TK_ZERO,      TK_COMBINED,
                TK_COMBINED, TK_ZERO, TK_SHADE,   TK_ZERO);
        } else {
            BrRdpSetCombineLERP(put_slot(),
                TK_TEXEL0, TK_ZERO, TK_PRIMITIVE, TK_ZERO,
                TK_ZERO,   TK_ZERO, TK_ZERO,      TK_TEXEL0,
                TK_ZERO,   TK_ZERO, TK_ZERO,      TK_COMBINED,
                TK_ZERO,   TK_ZERO, TK_ZERO,      TK_COMBINED);
            put(0xB900031Du, g_BrDrawRenderMode | 0x00112230u);
        }

        /* 0x10009D75 -- the shared tail.  0x3B808081 is 1/255, read out of
         * the binary rather than assumed; the same constant is pushed
         * three times at 0x10009D75/7A/7F. */
        BrMat4Scale(&g_BrDrawScale, 0.003921569f, 0.003921569f, 0.003921569f);
        BrMat4Mul(&g_BrDrawScale, pWheel, &g_BrDrawWorld);

        pSlot = mtx_alloc(&addr);
        if (pSlot) BrGuMtxStore(&g_BrDrawWorld, pSlot);
        put(0x01060040u, addr);                 /* gsSPMatrix, PUSH|LOAD */

        BrMat4Mul(&g_BrDrawWorld, &g_BrDrawView, &g_BrDrawCombined);

        pSlot = mtx_alloc(&addr);
        if (pSlot) BrGuMtxStore(&g_BrDrawCombined, pSlot);
        /* DEVIATION, frontier only: the original computes addr+0x10/0x20/
         * 0x30 unconditionally because its allocator cannot fail.  With no
         * pool hook installed `addr` is the frontier's zero, and offsetting
         * it would put 0x10/0x20/0x30 into the list -- three plausible-
         * looking addresses that are not addresses.  Zero stays zero. */
        put(0x039E0010u, addr);
        put(0x03980010u, addr ? addr + 0x10u : 0);
        put(0x039A0010u, addr ? addr + 0x20u : 0);
        put(0x039C0010u, addr ? addr + 0x30u : 0);

        put(0xBB000001u, 0xFFFFFFFFu);          /* texture on            */
        put(0xB6000000u, 0x000C0000u);          /* clear both texgen bits*/
        put(0xE8000000u, 0);                    /* tile sync             */
        put(0xF5100000u, 0x07000000u);
        put(0xF50001F0u, 0x06000000u);
        put(0xF5000100u, 0x05000000u);

        if (g_BrDrawWheelAlt != 0) {
            if (pModel->dlWheelAlt != 0)
                put(0x06000000u, pModel->dlWheelAlt);
        } else {
            if (pModel->dlWheel != 0)
                put(0x06000000u, pModel->dlWheel);
        }

        put(0xBD000000u, 0);                    /* pop matrix            */
    }
}

/* ==================================================================== *
 * 0x1000A110 -- draw one vehicle.  7,577 bytes.  MAPPED, NOT CLAIMED.
 * ====================================================================
 *
 * Read in full for this pass.  It is recorded here rather than
 * transcribed, because a whole-function `@implements` over a partial body
 * is the failure CONVENTIONS.md names, and 7,577 bytes of branchy x87 plus
 * forty-five globals is not something this pass could finish and
 * mutation-test honestly.  Everything below is read off the disassembly.
 *
 * SIGNATURE.  `void f(void *pCar, int32_t lod)` -- cdecl, two arguments.
 * `lod` is IN/OUT on the caller's stack: 0x1000A27F adds the distance-
 * derived level to it, clamps at 2, and writes it back to [esp+0x64].
 * 0x10011FA0 always passes 0.
 *
 * GUARDS (0x1000A11B .. 0x1000A16E).  Returns without emitting anything if
 * any of +0xF08, +0x168, +0x170, +0x16C, +0x174 is NULL, or if
 * 0x10273350[car->+0x140] is zero.  A sixth exit at 0x1000A37A: for the
 * PLAYER's car, if +0x2734 points at either self+0x273C or self+0x2890 and
 * 0x106ED6A4 is zero.
 *
 * DETAIL LEVEL (0x1000A174 .. 0x1000A295).  dist = BrVec3Dist(car+0x30,
 * cam+0x30) -- 0x10034760, whose result STAYS IN st0 across the next four
 * comparisons; `fst [esp+0x20]` spills a float copy without popping.  When
 * 0x100AA044 == 2 the level is 0 below 40, 1 below 80, else 2, then raised
 * to at least 0x106ED6C0; otherwise it is 0x106ED6C0 outright.  A separate
 * flag records dist < 100.  All four tests are `test ah,1` after `fcom`,
 * so NaN takes the LOW side every time -- write them as !(d >= k), never
 * as (d < k).
 *
 * FOG (0x1000A198).  Draw class 2 only: 0x102735FC = __ftol(alpha*255) and
 * a G_SETFOGCOLOR whose RGB is (0x106E72F0, 0x106E86A4, 0x106E7290) and
 * whose alpha is that byte.
 *
 * MATRICES (0x1000A2AA .. 0x1000A34F).  Scale(1/255) * car -> 0x10273570,
 * stored into a pooled matrix recorded in 0x102735B0[iCar]; then
 * 0x10273570 * 0x106E9A38 -> 0x106E78F0, stored into a second pooled
 * matrix recorded in 0x10273600[iCar].  0x1002A9F2 is called on the
 * combined matrix and does nothing (see above).
 *
 * TWO LIGHT COLOURS (0x1000A393 .. 0x1000A556).  Three arms produce the
 * pair of G_MW_LIGHTCOL words:
 *   - 0x106ED6AC set: each channel is __ftol(byte / max(dist*0.1f, 1.0f)),
 *     three divisions sharing one x87 register;
 *   - else, if the +0x290C record's +0x4C carries bit 0x10: colour A is 0
 *     and colour B is each byte scaled by 4/5 (an 0x66666667 magic divide);
 *   - else: the bytes unscaled.
 * Both words are RGBA with A == 0 in every arm.
 *
 * LIGHTS (0x1000A86A .. 0x1000A9CE).  With 0x106ED6AC and 0x106ED6B4 both
 * clear it emits the static Lights1 at 0x100A9FF0 (gsSPLight(&l[0],1) to
 * 0x100A9FF8 and gsSPLight(&a,2) to 0x100A9FF0 -- textbook F3D).
 * Otherwise it copies that Lights1 into 0x102733A0 + 24*iCar and overwrites
 * its direction with __ftol(player->m00/m01/m02 * -120.0f), then points the
 * two G_MOVEMEMs at the copy.  The 24-byte stride is Ambient(8)+Light(16).
 *
 * CULLING (0x1000AA83 .. 0x1000AADB).  G_CULL_FRONT and G_CULL_BACK are
 * SWAPPED when 0x106EA3F4 != 0x106E8204 -- set gets 0x1000/0x2000 and
 * clear gets the other.  Both are built with the `neg/sbb` idiom.
 *
 * RENDER MODE (0x1000AADE .. 0x1000AB7E).  0x10273640 is 0x011049D8 for
 * class 2, else 0x00112078 for the player / 0x00112038 for anyone else
 * when dist < 100, else 0x00112230.  One extra arm: session kind 2, a
 * non-NULL +0xF00 whose +0x64 is set, and an alpha of exactly 0x3EC00000
 * force 0x10273640 = 2 and 0x10273644 = 0.
 *
 * THE FIVE DRAW PASSES, in emission order:
 *   1. 0x1000AB7E  body        -> model->aLod[lod] + 0x1C
 *   2. 0x1000AE72  underside   -> model->aLod[lod] + 0x18, only when
 *                                 0x10273304 == 0 and car->+0x29B4 == 0
 *   3. 0x1000B2CB  glass       -> model->aLod[lod] + 0x10, only when
 *                                 dot(car row 2, carPos - camPos) > 0
 *                                 (0x10034310, compared against a DOUBLE
 *                                 zero at 0x100771C0)
 *   4. 0x1000B4AA  detail      -> model->aLod[lod] + 0x04
 *   5. 0x1000B6E6  reflection  -> gated on 0x10B7153C set, 0x106ED6B0 and
 *                                 0x106ED6B4 clear, 0x10273304 clear,
 *                                 car->+0x29B4 clear, and a two-armed test
 *                                 on 0x106ED6AC against the player
 * then 0x1000BC7B emits aLod[lod] + 0x08 and aLod[lod] + 0x1C again, pops
 * the matrix, and calls 0x10009C10 for the wheels -- EARLY (0x1000ACCA)
 * for draw class 2 and LATE (0x1000BDE8) for everything else, so the
 * wheels are drawn exactly once either way.
 *
 * THE SPECULAR HIGHLIGHT (0x1000A6F6 .. 0x1000A81B).  Three pool
 * allocations, then 0x1002A4D0 with eleven arguments and 0x1002A200 with
 * twenty.  Those two are guLookAtReflectF and guLookAtHiliteF: the twenty
 * are exactly (mf, l, h, eye[3], at[3], up[3], light1[3], light2[3],
 * twidth, theight), and the 16-byte record 0x1002A200 fills is the Hilite
 * whose two dwords 0x1000B882 unpacks into a G_SETTILESIZE.  Neither is
 * transcribed anywhere in this tree.
 *
 * THE ONE INDIRECT CALL.  0x1000B1EF calls through 0x118ED1BC with two
 * display-list addresses: always model->+0x80, and one of +0x84/+0x88/
 * +0x8C/+0x90 chosen by (aux flags & 0xC0000) crossed with the sign of
 * car->+0xE68.
 *
 * THE STACK TRAP, recorded because it is live here.  The two argument
 * slots are reused as locals, and the SAME displacement names different
 * things at different points:
 *   [esp+0x64] is `lod` at 0x1000A27F (esp = E-0x5C) and a saved command
 *   pointer after 0x1000B876; and at 0x1000A655 esp is E-0x60 because a
 *   `push` is outstanding, so that `fst [esp+0x64]` writes the ARG1 slot,
 *   which 0x1000A67C then reads back as [esp+0x60].  Three displacements,
 *   two slots, one function.
 * ==================================================================== */
