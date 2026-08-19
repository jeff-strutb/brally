/* br_drawcar.c -- see br_drawcar.h.  The vehicle's display list, from
 * BRGlide.dll.
 *
 * Read off the GLIDE build, which is this project's reference.  Both
 * functions here are classed `shared` in config/shared.csv, so the D3D
 * twins (0x1000C6E0 and 0x1000CBE0) are the same code under other numbers.
 */
#include "br_drawcar.h"
#include "slice1_05.h"   /* BrGfxWords, BrRdpSetCombineLERP, BrMat4Mul   */
#include "slice2_15.h"   /* g_4B16A0 / g_4B16AC scene accumulators       */
#include "slice2_17.h"   /* BrGfxEmitTexCmd, BrS17GetState               */
#include "slice2_18.h"   /* BrG_6C0680 cursor; BrFogFactorAtPoint; car globals */
#include "slice2_19.h"   /* g_BrMtxSlot current projection slot          */
#include "slice2_21.h"   /* BrSpanVolume, BrSpanTestPoint                */
#include "br_racebegin.h" /* g_brRaceBeginDifficulty, g_brRaceBeginNTexSet */
#include "br_appstart.h"  /* g_brCfgGameMode                             */
#include "br_bootfrontier.h" /* BrBootGlobal_ABAA0                       */
#include "slice3_41.h"   /* BrPool16Alloc, BrPool32Alloc                 */
#include "br_vec.h"      /* BrVec3Dist, BrVec3MulAdd, the glow cluster   */

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
int32_t  g_BrDrawClass[BR_CAR_MAX];        /* 0x10273520  per-car LOD     */
uint8_t  g_BrDrawLights[24 * BR_CAR_MAX];  /* 0x102733A0  light copies    */
uint32_t g_BrDrawModeBase;                  /* 0x10273640  render mode base*/
int32_t  g_BrDrawSuppress;                  /* 0x10273304  suppress under  */
int32_t  g_BrDrawLodFloor;                  /* 0x106ED6C0  minimum LOD     */
BrVec3   g_BrDrawDir0;                      /* 0x10273390  light dir 0     */
BrVec3   g_BrDrawDir1;                      /* 0x10273560  light dir 1     */
int32_t  g_BrDrawReflectEnable;             /* 0x10B7153C  BSS 0           */
int32_t  g_BrDrawReflectFlag = 1;           /* 0x100A5B40  .data = 1       */
void    *g_BrDrawTrackFlags;                /* 0x106EED38  84-byte recs    */
const void *g_BrDrawTexBlob;                /* 0x100A5C88  palette blob    */
void   (*g_BrDrawModelDlHook)(uint32_t, uint32_t);  /* 0x118ED1BC  BSS    */
uint8_t  g_BrDrawByte80;                    /* 0x106B7C80  light colour    */
uint8_t  g_BrDrawByte78;                    /* 0x106B7C78  env colour      */
int32_t  g_BrDrawRefIndex;                  /* 0x10273688  ref colour idx  */
const int8_t  *g_BrDrawRefTbl;              /* 0x100A5C78  ref table       */
const uint32_t *g_BrDrawRefColors;          /* 0x100A5C58  ref colours     */

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

/* Combiner tokens TK_* are in br_drawcar.h. */

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
 * 0x10009FC0 -- the per-car visibility pass.
 *
 * Runs once per car before the draw passes.  It computes the car's fog
 * factor (stored back into the record at +0x2730) and decides whether the
 * car is on screen this frame, recording the answer in the two flag arrays
 * that 0x1000A110's opaque and translucent passes gate on.
 *
 * The frame's on-screen coverage is a global span hull (g_BrFrameHull,
 * the original's grid at Glide 0x10AC2C54 / D3D 0x10A9BBC4).  The original's
 * span test read that grid implicitly; the port's BrSpanTestPoint takes the
 * volume explicitly, so the hull is passed in.  Nothing builds the hull yet
 * (the frame setup that calls BrSpanBuildHull is unported), so in the live
 * host it is empty and every non-player car culls -- faithful to the code,
 * inert until that setup lands.  Verified against the byte-identical D3D
 * twin 0x1000CA90.
 * ==================================================================== */

/* The per-frame coverage hull.  See slice2_21's BrSpanBuildHull. */
BrSpanVolume g_BrFrameHull;

int32_t g_BrCarVisOpaque[BR_CAR_MAX];   /* 0x10273648 (d3d 0x10277E60) */
int32_t g_BrCarVisAny[BR_CAR_MAX];      /* 0x10273350 (d3d 0x10277B68) */

/* Per-car pooled-matrix DL addresses -- 0x1000A110 fills these, this pass
 * reads them.  Zero until it runs. */
uint32_t g_BrCarMtxSlot[BR_CAR_MAX];    /* 0x102735B0 */
uint32_t g_BrCarLightSlot[BR_CAR_MAX];  /* 0x10273600 */

/* @implements 0x10009FC0 glide BrCarVisibilityUpdate */
void BrCarVisibilityUpdate(void *pCar)
{
    unsigned char *car = (unsigned char *)pCar;
    const BrVec3  *pPos;
    int32_t        iCar;

    /* +0xF08 is one of the record's guard pointers; nothing happens without it. */
    if (*(void *const *)(car + BR_CAR_OFF_GUARD) == NULL) {
        return;
    }

    pPos = (const BrVec3 *)(car + BR_CAR_OFF_POS);

    /* A distance from the car to BrG_6C6490's +0x30 that the original computes
     * and then discards (fstp st(0)).  Kept for fidelity; it has no effect. */
    (void)BrVec3Dist(pPos,
                     (const BrVec3 *)((const unsigned char *)BrG_6C6490 + 0x30));

    iCar = *(const int32_t *)(car + BR_CAR_OFF_ICAR);
    g_BrCarVisOpaque[iCar] = 0;
    g_BrCarVisAny[iCar]    = 0;

    *(float *)(car + BR_CAR_OFF_FOG) = BrFogFactorAtPoint(pPos);

    /* The player's own car is never span-culled; jump straight to the
     * self/active-camera test.  Otherwise a car is visible if its position --
     * or, when a mode flag forces it, a point 6 units to its side -- lands in
     * the hull. */
    if ((void *)car != BrG_6C2CF8) {
        if (BrG_6C661C != 0 || BrG_6C6624 != 0) {
            BrVec3 probe;
            BrVec3MulAdd(&probe, pPos,
                         (const BrVec3 *)(car + BR_CAR_OFF_MTX), 6.0f);
            if (BrSpanTestPoint(&g_BrFrameHull, probe.x, probe.y) == 0 &&
                BrSpanTestPoint(&g_BrFrameHull, pPos->x, pPos->y) == 0) {
                return;                         /* culled */
            }
        } else if (BrSpanTestPoint(&g_BrFrameHull, pPos->x, pPos->y) == 0) {
            return;                             /* culled */
        }

        /* Visible.  The player-car branch below only applies to the player,
         * so a non-player car falls straight through to the flag set. */
        if ((void *)car != BrG_6C2CF8) {
            goto set_flags;
        }
    }

    /* Player car (reached directly, or as a visible car that is the player):
     * if its active camera points at one of its own two cam frames and the
     * override flag is clear, it counts only for the translucent pass. */
    {
        const void *pActiveCam = *(const void *const *)(car + BR_CAR_OFF_ACTIVECAM);
        if (pActiveCam == (const void *)(car + BR_CAR_OFF_CAMA) ||
            pActiveCam == (const void *)(car + BR_CAR_OFF_CAMB)) {
            if (BrG_6C6614 == 0) {
                g_BrCarVisAny[iCar] = 1;
                return;
            }
        }
    }

set_flags:
    /* Opaque cars (class != 2) show in both passes; class-2 cars only in the
     * translucent pass. */
    if (*(const unsigned char *)(car + BR_CAR_OFF_KIND) != 2) {
        g_BrCarVisOpaque[iCar] = 1;
    }
    g_BrCarVisAny[iCar] = 1;
}

/* ==================================================================== *
 * 0x1000BEB0 -- emit ONE opaque car's body.  1,576 bytes.
 *
 * The sibling of 0x10009C10 (the wheels): where that one walks the four
 * wheel matrices, this one lays down the body pass -- two matrices, the four
 * light-matrix blocks, a canned setup list, the texture command, two colour
 * combiners around the body geometry, and the static Lights1.  Run once per
 * opaque car by the frame driver, after the visibility pass has set the
 * flags.  Verified against the byte-identical D3D twin 0x1000E950.
 *
 * IT ALSO ACCUMULATES.  For every car that is NOT the player it folds a
 * headlight-glare term into the two per-frame scene accumulators
 * slice2_15's BrSceneAccumReset (0x10017F60) zeroes at frame top -- so this
 * is the "geometry pass" that header names as their writer.  g_4B16AC takes
 * the FRONT-facing glare (the view direction opposes the camera basis) and
 * g_4B16A0 the BACK-facing one; each term is (align - 0.95) * 750 / dist^2
 * above a 0.95 alignment threshold, and the two are mutually exclusive per
 * car.  The glow arithmetic sits in the file's stack-aliasing region and was
 * transcribed against tools/x87emu.py, not hand-derived (see the golden
 * vectors in test_br_drawcar.c).
 *
 * The record and its model are reached by raw byte offset, the write-back
 * convention the visibility pass established: this pass writes gfx state and
 * the shared accumulators, which the read-only BrCarView cannot model.
 *
 * Consts read out of the binary: g_0771A8 = 0.0 (the divide-by-zero guard on
 * the distance), g_0771CC = 0.95 (the alignment threshold), g_0771D0 = 750.0
 * (the glare gain).  1.0f / 0.0f scale factors on the basis vector are the
 * shipped build's -- BrVec3Scale(row0, 1.0) then BrVec3MulAddTo(row2, 0.0)
 * reduces to a copy of row0, emitted as written.
 * ==================================================================== */
/* WHAT IT DOES: draws the solid shell of one car and, for the other racers,
 * adds a little bloom wherever their headlights point roughly at or away from
 * the camera, so oncoming and receding cars glow. */
/* @implements 0x1000BEB0 glide BrCarDrawBody */
void BrCarDrawBody(void *pCar)
{
    unsigned char       *car = (unsigned char *)pCar;
    const unsigned char *model;
    int32_t              iCar;
    uint32_t             slotL;

    /* 0x1000BEB0 -- nothing draws unless one of the two mode flags is set. */
    if (BrG_6C661C == 0 && BrG_6C6624 == 0) {
        return;
    }
    /* 0x1000BED0 -- only cars the visibility pass marked for the opaque pass. */
    iCar = *(const int32_t *)(car + BR_CAR_OFF_ICAR);
    if (g_BrCarVisOpaque[iCar] == 0) {
        return;
    }
    /* 0x1000BEE3 -- the player's own car, when its active camera object is the
     * record's own +0x27C4 slot, is left to the other passes. */
    if ((void *)car == BrG_6C2CF8 &&
        BrG_6C6490 == (void *)(car + BR_CAR_OFF_CAMSLOT)) {
        return;
    }
    /* 0x1000BEFF -- class 2 is the translucent pass, not this one. */
    if (*(const unsigned char *)(car + BR_CAR_OFF_KIND) == 2) {
        return;
    }

    /* 0x1000BF0C -- publish the model to the scratch global the tail (and
     * 0x1000A110) read, then work from it. */
    BrG_6C3308 = *(void *const *)(car + BR_CAR_OFF_MODEL);
    model      = (const unsigned char *)BrG_6C3308;

    /* 0x1000BF18 -- the two matrices: the car's pooled model matrix pushed as
     * the modelview, the shared projection slot loaded after it. */
    put(0x01060040u, g_BrCarMtxSlot[iCar]);
    put(0x01030040u, (uint32_t)(uintptr_t)g_BrMtxSlot);

    /* 0x1000BF5F -- the four 16-byte blocks of the car's lighting matrix
     * (0x9E/0x98/0x9A/0x9C at +0/+0x10/+0x20/+0x30 of one pooled slot). */
    slotL = g_BrCarLightSlot[iCar];
    put(0x039E0010u, slotL);
    put(0x03980010u, slotL ? slotL + 0x10u : 0);
    put(0x039A0010u, slotL ? slotL + 0x20u : 0);
    put(0x039C0010u, slotL ? slotL + 0x30u : 0);

    /* 0x1000C004 -- the canned setup list, then the model's texture command. */
    put(0x06000000u, (uint32_t)(uintptr_t)BrG_0AA838);
    BrGfxEmitTexCmd(5, *(const void *const *)(model + BR_MODEL_OFF_TEXRECS));

    /* 0x1000C035 -- pipe sync, two-cycle, and the move-word run that primes
     * the primitive colour (0x200A/0x240A carry 0xFFFFFF00). */
    put(0xE7000000u, 0);
    put(0xBA001402u, 0);
    put(0xBC00000Au, 0);
    put(0xBC00040Au, 0);
    put(0xBC00200Au, 0xFFFFFF00u);
    put(0xBC00240Au, 0xFFFFFF00u);

    /* 0x1000C0FD -- combiner #1: shade the body from TEXEL0 with a solid
     * "1" in the d slot (colour and alpha both). */
    BrRdpSetCombineLERP(put_slot(),
        TK_ZERO, TK_ZERO, TK_ZERO, TK_ONE,
        TK_ZERO, TK_ZERO, TK_ZERO, TK_TEXEL0,
        TK_ZERO, TK_ZERO, TK_ZERO, TK_ONE,
        TK_ZERO, TK_ZERO, TK_ZERO, TK_TEXEL0);

    /* 0x1000C108 -- render mode, othermode. */
    put(0xB900031Du, 0x004049D8u);
    put(0xBA000602u, 0x00000080u);

    /* 0x1000C147 -- the body geometry, only if the model carries one. */
    if (*(const uint32_t *)(model + BR_MODEL_OFF_BODYDL) != 0) {
        put(0x06000000u, *(const uint32_t *)(model + BR_MODEL_OFF_BODYDL));
    }
    put(0xE7000000u, 0);
    put(0xBA000602u, BrG_6C0688);

    /* 0x1000C1B5 -- headlight glare, non-player cars only. */
    if ((void *)car != BrG_6C2CF8) {
        const BrVec3 *pRow0     = (const BrVec3 *)(car + BR_CAR_OFF_MTX);
        const BrVec3 *pRow2     = (const BrVec3 *)(car + BR_CAR_OFF_ROW2);
        const BrVec3 *pPos      = (const BrVec3 *)(car + BR_CAR_OFF_POS);
        const BrVec3 *pCamBasis = (const BrVec3 *)BrG_6C6490;
        const BrVec3 *pCamPos   =
            (const BrVec3 *)((const unsigned char *)BrG_6C6490 + 0x30);
        BrVec3 dir, basis;
        float  len, dot2, dot1, val;

        /* dir = camPos - (pos + row0); its length is the car-to-camera
         * distance measured from a point one basis unit ahead. */
        BrVec3Add(&dir, pPos, pRow0);
        BrVec3Sub(&dir, pCamPos, &dir);
        len = BrVec3Length(&dir);

        if (len != 0.0f) {                       /* g_0771A8 == 0.0 */
            BrVec3DivBy(&dir, len);              /* dir -> unit direction */
            dot2 = BrVec3Dot(&dir, pCamBasis);

            /* 0x1000C243 -- FRONT glare: the view opposes the camera basis. */
            if (dot2 < 0.0f) {
                BrVec3Scale(&basis, pRow0, 1.0f);
                BrVec3MulAddTo(&basis, pRow2, 0.0f);   /* basis = row0 */
                dot1 = BrVec3Dot(&dir, &basis);
                val  = -(dot2 * dot1);
                if (val > 0.95f) {               /* g_0771CC == 0.95 */
                    g_4B16AC += ((val - 0.95f) * 750.0f) / (len * len);
                }
            }

            /* 0x1000C2FA -- BACK glare: the view runs with the camera basis. */
            dot2 = BrVec3Dot(&dir, pCamBasis);
            if (dot2 > 0.95f) {
                BrVec3Scale(&basis, pRow0, 1.0f);
                BrVec3MulAddTo(&basis, pRow2, 0.0f);
                dot1 = BrVec3Dot(&dir, &basis);
                val  = dot2 * dot1;
                if (val > 0.95f) {
                    g_4B16A0 += ((val - 0.95f) * 750.0f) / (len * len);
                }
            }
        }
    }

    /* 0x1000C38A -- the tail: sync, pop the model matrix, clear the shade
     * geom-mode bit, then the two static Lights1 blocks and combiner #2. */
    put(0xE7000000u, 0);
    put(0xBA001402u, 0);
    put(0xBD000000u, 0);
    put(0xB6000000u, 0x00040000u);
    put(0xBC000002u, 0x80000040u);
    put(0x03860010u, (uint32_t)(uintptr_t)BrG_0AA868);
    put(0x03880010u, (uint32_t)(uintptr_t)BrG_0AA860);
    put(0xBA000C02u, BrG_6C0258);
    put(0xBA000E02u, 0);

    /* 0x1000C4CA -- combiner #2: sample TEXEL0 modulated by the primitive. */
    BrRdpSetCombineLERP(put_slot(),
        TK_TEXEL0, TK_ZERO, TK_PRIMITIVE, TK_ZERO,
        TK_TEXEL0, TK_ZERO, TK_PRIMITIVE, TK_ZERO,
        TK_TEXEL0, TK_ZERO, TK_PRIMITIVE, TK_ZERO,
        TK_TEXEL0, TK_ZERO, TK_PRIMITIVE, TK_ZERO);
}

/* ------------------------------------------------------------------ *
 * wheel_call -- bridge from the raw 0x2B68 car record into the repacked
 * BrCarView / BrModelView that BrCarDrawWheels takes.  0x1000A110 calls
 * 0x10009C10 with the raw pointer; BrCarDrawWheels takes the view pair.
 * Only the fields BrCarDrawWheels actually reads are populated.
 * ------------------------------------------------------------------ */
static void wheel_call(unsigned char *car)
{
    const unsigned char *mdl = (const unsigned char *)BrG_6C3308;
    BrCarView   cv;
    BrModelView mv;
    memset(&cv, 0, sizeof(cv));
    memset(&mv, 0, sizeof(mv));
    memcpy(cv.aWheel, car + BR_CAR_OFF_AWHEEL, sizeof(cv.aWheel));
    cv.bKind      = *(car + BR_CAR_OFF_KIND);
    mv.dlWheel    = *(const uint32_t *)(mdl + 0x80BC);
    mv.dlWheelAlt = *(const uint32_t *)(mdl + 0x80C4);
    BrCarDrawWheels(&cv, &mv);
}

/* ==================================================================== *
 * 0x1000A110 -- draw one vehicle: body, underside, glass, detail,
 * reflection and wheels.  7,577 bytes.
 *
 * NOT CLAIMED (@implements withheld).  Three interleaved-x87 blocks are
 * deferred (marked TODO below) and the command stream has not been
 * validated against an independent source.  The body below is recovered
 * from the analysis at git 66dbe21 (which was reverted at 2838aad) and
 * is treated as raw material, not a finished draft.  See the re-land
 * checklist in memory (implements-requires-execution.md).
 *
 * THE STACK TRAP, recorded because it is live here.  The two argument
 * slots are reused as locals, and the SAME displacement names different
 * things at different points:
 *   [esp+0x64] is `lod` at 0x1000A27F (esp = E-0x5C) and a saved command
 *   pointer after 0x1000B876; and at 0x1000A655 esp is E-0x60 because a
 *   `push` is outstanding, so that `fst [esp+0x64]` writes the ARG1 slot,
 *   which 0x1000A67C then reads back as [esp+0x60].  Three displacements,
 *   two slots, one function.
 *
 * Deferred TODO blocks:
 *   0xA5A1-0xA6F3  light-direction computation (g_BrDrawDir0/Dir1)
 *   0xA6F6-0xA81B  specular setup (pool + guLookAtReflectF/HiliteF)
 *   0xB925-0xBC7B  post-detail setup block (~854 bytes, F2 settile)
 * The reflection pass (0xB685-0xB925) is also TODO but gated on
 * g_BrDrawReflectEnable which is BSS 0 (dead path).
 * ==================================================================== */
void BrCarDrawVehicle(void *pCar, int32_t lodBias)
{
    unsigned char *car = (unsigned char *)pCar;
    const unsigned char *model;
    int32_t  iCar, lod, distNear, flag290C;
    float    dist;
    uint32_t colourA, colourB;
    uint32_t lodOff;
    uint32_t specMem = 0;
    BrMat4  *pSlot;
    uint8_t  bKind;

    /* 0xA11B -- six guard tests. */
    if (*(void **)(car + BR_CAR_OFF_GUARD) == 0) return;
    if (*(void **)(car + BR_CAR_OFF_P0168) == 0) return;
    if (*(void **)(car + BR_CAR_OFF_P0170) == 0) return;
    if (*(void **)(car + BR_CAR_OFF_P016C) == 0) return;
    if (*(void **)(car + BR_CAR_OFF_P0174) == 0) return;
    iCar    = *(int32_t *)(car + BR_CAR_OFF_ICAR);
    flag290C = 0;
    if (g_BrCarVisAny[iCar] == 0) return;

    /* 0xA174 -- distance + LOD. */
    dist = BrVec3Dist(
        (const BrVec3 *)(car + BR_CAR_OFF_POS),
        (const BrVec3 *)((const unsigned char *)BrG_6C6490 + 0x30));

    bKind = *(car + BR_CAR_OFF_KIND);

    /* 0xA198 -- fog (class 2 only). */
    if (bKind == 2) {
        g_BrDrawFogAlpha =
            (int32_t)(*(const float *)(car + BR_CAR_OFF_ALPHA) * 255.0f);
        put(0xF8000000u,
            ((uint32_t)BrG_6C0260 << 24) |
            ((uint32_t)BrG_6C1614 << 16) |
            ((uint32_t)BrG_6C0200 << 8)  |
            ((uint32_t)g_BrDrawFogAlpha & 0xFFu));
    }

    /* 0xA1F1 -- flag290C gate from track records. */
    *(int32_t *)(car + BR_CAR_OFF_I2714) = 0;
    if (*(void *const *)(car + BR_CAR_OFF_P294C) != 0) {
        uint32_t k = *(const uint16_t *)(car + BR_CAR_OFF_U290C);
        const unsigned char *flags = (const unsigned char *)g_BrDrawTrackFlags;
        if (flags[k * 84 + 0x4C] & 0x10) {
            flag290C = 1;
            *(int32_t *)(car + BR_CAR_OFF_I2714) = 1;
        }
    }

    /* 0xA232 -- model setup. */
    BrG_6C3308 = *(void *const *)(car + BR_CAR_OFF_MODEL);
    model = (const unsigned char *)BrG_6C3308;

    /* 0xA23D -- LOD computation. */
    if (g_brRaceBeginNTexSet == 2) {
        if      (!(dist >= 40.0f))  lod = 0;
        else if (!(dist >= 80.0f))  lod = 1;
        else                        lod = 2;
        if (lod < g_BrDrawLodFloor)
            lod = g_BrDrawLodFloor;
    } else {
        lod = g_BrDrawLodFloor;
    }
    lod += lodBias;
    if (lod > 2) lod = 2;

    /* 0xA295 -- near-distance flag. */
    distNear = !(dist >= 100.0f);

    /* 0xA2AA -- matrices: scale(1/255) * car -> world, world * view -> combined. */
    BrMat4Scale(&g_BrDrawScale, 0.003921569f, 0.003921569f, 0.003921569f);
    BrMat4Mul(&g_BrDrawScale, (const BrMat4 *)car, &g_BrDrawWorld);

    pSlot = mtx_alloc(&g_BrCarMtxSlot[iCar]);
    if (pSlot) BrGuMtxStore(&g_BrDrawWorld, pSlot);

    BrMat4Mul(&g_BrDrawWorld, &g_BrDrawView, &g_BrDrawCombined);
    BrGuMtxHookNop(&g_BrDrawCombined);

    pSlot = mtx_alloc(&g_BrCarLightSlot[iCar]);
    if (pSlot) BrGuMtxStore(&g_BrDrawCombined, pSlot);

    /* 0xA354 -- player self-view guard. */
    if ((void *)car == BrG_6C2CF8) {
        void *activeCam = *(void *const *)(car + BR_CAR_OFF_ACTIVECAM);
        if (activeCam == (void *)(car + BR_CAR_OFF_CAMA) ||
            activeCam == (void *)(car + BR_CAR_OFF_CAMB)) {
            if (BrG_6C6614 == 0)
                return;
        }
    }

    /* 0xA386 -- record LOD class for this car. */
    g_BrDrawClass[iCar] = lod;

    /* 0xA393 -- three-arm light colour computation. */
    if (BrG_6C661C != 0) {
        float div = dist * 0.1f;
        if (!(div >= 1.0f)) div = 1.0f;
        {
            uint32_t r = (uint32_t)(int32_t)((float)(int32_t)BrG_6C1580 / div);
            uint32_t g = (uint32_t)(int32_t)((float)(int32_t)BrG_6C335C / div);
            uint32_t b = (uint32_t)(int32_t)((float)(int32_t)BrG_6C0968 / div);
            colourA = ((r & 0xFF) << 24) | ((g & 0xFF) << 16) | ((b & 0xFF) << 8);
        }
        colourB = ((uint32_t)g_BrDrawByte80 << 24) |
                  ((uint32_t)BrG_6C0960 << 16) |
                  ((uint32_t)BrG_6C65BC << 8);
    } else if (flag290C != 0) {
        colourA = 0;
        colourB = ((uint32_t)((int32_t)(g_BrDrawByte80 * 4) / 5) << 24) |
                  ((uint32_t)((int32_t)(BrG_6C0960   * 4) / 5) << 16) |
                  ((uint32_t)((int32_t)(BrG_6C65BC   * 4) / 5) << 8);
    } else {
        colourA = ((uint32_t)BrG_6C1580 << 24) |
                  ((uint32_t)BrG_6C335C << 16) |
                  ((uint32_t)BrG_6C0968 << 8);
        colourB = ((uint32_t)g_BrDrawByte80 << 24) |
                  ((uint32_t)BrG_6C0960 << 16) |
                  ((uint32_t)BrG_6C65BC << 8);
    }

    /* 0xA556 -- two G_MTX pushes: model and projection. */
    put(0x01060040u, g_BrCarMtxSlot[iCar]);
    put(0x01030040u, (uint32_t)(uintptr_t)g_BrMtxSlot);

    /* 0xA5A1 -- light-direction computation: build g_BrDrawDir0 and
     * g_BrDrawDir1 from camera, player, and car positions. */
    if (BrG_6C661C != 0) {
        if (BrG_6C6490 == (void *)((unsigned char *)BrG_6C2CF8 + 0x2808))
            BrVec3Negate(&g_BrDrawDir0, (const BrVec3 *)BrG_6C6490);
        else
            BrVec3Negate(&g_BrDrawDir0, (const BrVec3 *)BrG_6C2CF8);
    } else {
        g_BrDrawDir0 = BrG_6C0670;
    }
    BrVec3NormaliseGuard(&g_BrDrawDir0);

    g_BrDrawDir1 = g_BrDrawDir0;

    {
        BrVec3 tmp;
        float  len;
        BrVec3Sub(&tmp,
            (const BrVec3 *)((const unsigned char *)BrG_6C6490 + 0x30),
            (const BrVec3 *)(car + BR_CAR_OFF_POS));
        len = BrVec3Length(&tmp);
        if (len == 0.0f)
            BrVec3Negate(&tmp, (const BrVec3 *)BrG_6C6490);
        else
            BrVec3DivBy(&tmp, len);

        BrVec3Midpoint(&g_BrDrawDir1, &tmp, &g_BrDrawDir1);

        len = BrVec3Length(&g_BrDrawDir1);
        if (len == 0.0f) {
            const float *pCam = (const float *)BrG_6C6490;
            g_BrDrawDir1.x = pCam[8];
            g_BrDrawDir1.y = pCam[9];
            g_BrDrawDir1.z = pCam[10];
        } else {
            BrVec3DivBy(&g_BrDrawDir1, len);
        }
    }

    /* 0xA6F6 -- specular highlight setup: three pool allocations, then
     * BrLightDirsFromLookAt and BrLightDirsAndAngles. */
    {
        void          *pDiscard;
        BrLightPair   *pLights;
        BrSkyAngles   *pAngles;
        float          eyeX, eyeY, atOffset, eyeScale;
        const float   *pCam = (const float *)BrG_6C6490;
        const float   *pCarF = (const float *)car;

        pDiscard = BrPool16Alloc();
        (void)pDiscard;
        pLights  = (BrLightPair *)BrPool32Alloc();
        pAngles  = (BrSkyAngles *)BrPool32Alloc();
        specMem  = pLights
                   ? (uint32_t)(uintptr_t)pLights : 0;

        atOffset = 0.0f;
        eyeScale = 0.0f;

        if (pCam[12] == pCarF[12] &&
            pCam[13] == pCarF[13] &&
            pCam[14] != pCarF[14]) {
            eyeScale = 0.1f;
        } else if (pCam[12] == pCarF[12] &&
                   pCam[13] == pCarF[13] &&
                   pCam[14] == pCarF[14]) {
            atOffset = 1.0f;
        }

        eyeX = pCam[0];
        eyeY = pCam[1];
        if (eyeX == 0.0f && eyeY == 0.0f) {
            union { uint32_t u; float f; } fix;
            fix.u = 0x38D1B717u;
            eyeX = fix.f;
        }

        BrLightDirsFromLookAt(&g_BrDrawCombined, pLights,
            eyeX, eyeY, 0.0f,
            0.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f);

        BrLightDirsAndAngles(&g_BrDrawCombined, pLights, pAngles,
            pCam[12], pCam[13], pCam[14],
            pCarF[12] + eyeScale, pCarF[13],
            pCarF[14] + atOffset,
            0.0f, 0.0f, 1.0f,
            g_BrDrawDir1.x, g_BrDrawDir1.y, g_BrDrawDir1.z,
            g_BrDrawDir1.x, g_BrDrawDir1.y, g_BrDrawDir1.z,
            64, 64);
    }

    /* 0xA86A -- Lights1 emission: static or dynamic. */
    if (BrG_6C661C == 0 && BrG_6C6624 == 0) {
        put(0xBC000002u, 0x80000040u);
        put(0x03860010u, (uint32_t)(uintptr_t)BrG_0AA868);
        put(0x03880010u, (uint32_t)(uintptr_t)BrG_0AA860);
    } else {
        unsigned char *dst = g_BrDrawLights + 24 * iCar;
        const float   *pPlayer = (const float *)BrG_6C2CF8;
        memcpy(dst, (const void *)BrG_0AA860, 24);
        dst[0x10] = (uint8_t)(int32_t)(pPlayer[0] * -120.0f);
        dst[0x11] = (uint8_t)(int32_t)(pPlayer[1] * -120.0f);
        dst[0x12] = (uint8_t)(int32_t)(pPlayer[2] * -120.0f);
        put(0xBC000002u, 0x80000040u);
        put(0x03860010u, (uint32_t)(uintptr_t)&dst[8]);
        put(0x03880010u, (uint32_t)(uintptr_t)dst);
    }

    /* 0xA9CE -- post-lights header: sync, two-cycle, geom mode. */
    put(0xE7000000u, 0);
    put(0xBA001001u, 0x00010000u);
    put(0xB7000000u, 0x00020205u);

    /* 0xAA25 -- BrG_6C6618 branch: set geom + render mode. */
    if (BrG_6C6618 != 0) {
        put(0xB7000000u, 0x00010000u);
        if (bKind != 2)
            g_BrDrawRenderMode = 0xC8000000u;
        else
            g_BrDrawRenderMode = 0x0C080000u;
    } else {
        g_BrDrawRenderMode = 0x0C080000u;
    }

    /* 0xAA83 -- culling: set (B7) and clear (B6), difficulty-swapped. */
    {
        uint32_t cullSet, cullClr;
        if (g_brRaceBeginDifficulty != BrG_6C1174) {
            cullSet = 0x00001000u;
            cullClr = 0x00002000u;
        } else {
            cullSet = 0x00002000u;
            cullClr = 0x00001000u;
        }
        put(0xB7000000u, cullSet);
        put(0xB6000000u, cullClr);
    }

    /* 0xAADE -- render mode base selection. */
    if (bKind == 2) {
        g_BrDrawModeBase = 0x011049D8u;
        put(0xFA000000u, ((uint32_t)g_BrDrawFogAlpha & 0xFF));
        if (g_brCfgGameMode == 2) {
            void *p = *(void *const *)(car + BR_CAR_OFF_P0F00);
            if (p != 0 &&
                *(const int32_t *)((const unsigned char *)p + 0x64) != 0 &&
                *(const uint32_t *)(car + BR_CAR_OFF_ALPHA) == 0x3EC00000u) {
                g_BrDrawRenderMode = 0;
                g_BrDrawModeBase   = 2;
            }
        }
    } else {
        if (distNear) {
            if ((void *)car == BrG_6C2CF8)
                g_BrDrawModeBase = 0x00112078u;
            else
                g_BrDrawModeBase = 0x00112038u;
        } else {
            g_BrDrawModeBase = 0x00112230u;
        }
    }

    /* 0xAB7E -- body pass header. */
    put(0xBA001402u, 0x00100000u);
    put(0xB900031Du, g_BrDrawModeBase | g_BrDrawRenderMode);

    BrRdpSetCombineLERP(put_slot(),
        TK_TEXEL0,   TK_ZERO, TK_PRIMITIVE, TK_ZERO,
        TK_ZERO,     TK_ZERO, TK_ZERO,      TK_TEXEL0,
        TK_ZERO,     TK_ZERO, TK_ZERO,      TK_COMBINED,
        TK_ZERO,     TK_ZERO, TK_ZERO,      TK_COMBINED);

    put(0xBA000C02u, BrG_6C0258);
    put(0xBA001001u, 0);
    put(0xB6000000u, 0x000C0000u);
    put(0xBC00000Au, colourA);
    put(0xBC00040Au, colourA);
    put(0xBC00200Au, colourB);
    put(0xBC00240Au, colourB);

    /* 0xACCA -- early wheel call (class 2 only). */
    if (bKind == 2)
        wheel_call(car);

    /* 0xACE3 -- four light MOVEMEMs (unconditional, +0x10/+0x20/+0x30). */
    put(0x039E0010u, g_BrCarLightSlot[iCar]);
    put(0x03980010u, g_BrCarLightSlot[iCar] + 0x10);
    put(0x039A0010u, g_BrCarLightSlot[iCar] + 0x20);
    put(0x039C0010u, g_BrCarLightSlot[iCar] + 0x30);

    /* 0xAD77 -- TLUT palette load. */
    put(0xFD100000u, (uint32_t)(uintptr_t)g_BrDrawTexBlob);
    put(0xE8000000u, 0);
    put(0xF50001E0u, 0x07000000u);
    put(0xE6000000u, 0);
    put(0xF0000000u, 0x0703C000u);
    put(0xE7000000u, 0);

    /* 0xAE34 -- specular MOVEMEM (payload from the TODO specular block). */
    put(0x03840010u, specMem);
    put(0x03820010u, specMem + 0x10);

    /* 0xAE72 -- underside pass (gated on suppress + i29B4). */
    lodOff = (uint32_t)lod * 40;

    if (g_BrDrawSuppress == 0 &&
        *(const int32_t *)(car + BR_CAR_OFF_I29B4) == 0) {
        put(0xBB000001u, 0xFFFFFFFFu);
        put(0xB6000000u, 0x000C0000u);
        put(0xE8000000u, 0);
        put(0xF5100000u, 0x07000000u);
        put(0xF50001F0u, 0x06000000u);
        put(0xF5000100u, 0x05000000u);
        put(0xB900031Du, g_BrDrawModeBase | g_BrDrawRenderMode);

        BrRdpSetCombineLERP(put_slot(),
            TK_TEXEL0,   TK_ZERO, TK_PRIMITIVE, TK_ZERO,
            TK_ZERO,     TK_ZERO, TK_ZERO,      TK_TEXEL0,
            TK_ZERO,     TK_ZERO, TK_ZERO,      TK_COMBINED,
            TK_ZERO,     TK_ZERO, TK_ZERO,      TK_COMBINED);

        put(0xBA000E02u, 0);

        BrRdpSetCombineLERP(put_slot(),
            TK_TEXEL0,   TK_ZERO, TK_PRIMITIVE, TK_ZERO,
            TK_TEXEL0,   TK_ZERO, TK_PRIMITIVE, TK_ZERO,
            TK_ZERO,     TK_ZERO, TK_ZERO,      TK_COMBINED,
            TK_ZERO,     TK_ZERO, TK_ZERO,      TK_COMBINED);

        put(0xB6000000u, 0x00040000u);
        put(0xBC00000Au, colourA);
        put(0xBC00040Au, colourA);
        put(0xBC00200Au, colourB);
        put(0xBC00240Au, colourB);
        put(0xBA000C02u, BrG_6C0258);
        {
            uint32_t underDL = *(const uint32_t *)(model + 0x8038 + lodOff);
            if (underDL != 0)
                put(0x06000000u, underDL);
        }

        /* 0xB0C6 -- shared tile setup (still inside suppress guard). */
        put(0xBB000001u, 0xFFFFFFFFu);
        put(0xB6000000u, 0x000C0000u);
        put(0xE8000000u, 0);
        put(0xF5100000u, 0x07000000u);
        put(0xF50001F0u, 0x06000000u);
        put(0xF5000100u, 0x05000000u);
    }

    /* 0xB176 -- model DL hook: 4-way selection on aux flags x f0E68 sign. */
    {
        uint8_t iTex = *(const uint8_t *)(model + 0x811B);
        const unsigned char *pTexRecs =
            *(const unsigned char *const *)(model + 0x8014);
        if (*(const uint32_t *)(pTexRecs + (uint32_t)iTex * 36 + 4) != 0 &&
            BrBootGlobal_ABAA0() == 0) {
            float fe68 = *(const float *)(car + BR_CAR_OFF_F0E68);
            uint32_t auxFlags =
                *(const uint32_t *)(*(void *const *)(car + BR_CAR_OFF_U29C0));
            uint32_t dlBase = *(const uint32_t *)(model + 0x80);
            uint32_t dlSel;
            if (auxFlags & 0xC0000u) {
                dlSel = !(fe68 >= 0.0f)
                    ? *(const uint32_t *)(model + 0x90)
                    : *(const uint32_t *)(model + 0x88);
            } else {
                dlSel = !(fe68 >= 0.0f)
                    ? *(const uint32_t *)(model + 0x8C)
                    : *(const uint32_t *)(model + 0x84);
            }
            if (g_BrDrawModelDlHook)
                g_BrDrawModelDlHook(dlBase, dlSel);
        }
    }

    /* 0xB1F8 -- glass prep: setup tiles + dot test. */
    {
        BrVec3 tmp;
        float  dot;
        put(0xBA001001u, 0x00010000u);
        put(0xBB000001u, 0xFFFFFFFFu);
        put(0xF5100000u, 0x07000000u);
        put(0xF50001F0u, 0x06000000u);
        put(0xF5000100u, 0x05000000u);

        BrVec3Sub(&tmp,
            (const BrVec3 *)(car + BR_CAR_OFF_POS),
            (const BrVec3 *)((const unsigned char *)BrG_6C6490 + 0x30));
        dot = BrVec3Dot(
            (const BrVec3 *)(car + BR_CAR_OFF_ROW2), &tmp);

        if (dot > 0.0) {
            /* 0xB2CB -- glass pass. */
            BrGfxEmitTexCmd(6,
                *(const void *const *)(model + 0x8014));
            put(0xE7000000u, 0);
            put(0xBA001402u, 0x00100000u);
            put(0xB900031Du, g_BrDrawModeBase | g_BrDrawRenderMode);

            BrRdpSetCombineLERP(put_slot(),
                TK_TEXEL0, TK_ZERO, TK_PRIMITIVE, TK_ZERO,
                TK_TEXEL0, TK_ZERO, TK_PRIMITIVE, TK_ZERO,
                TK_TEXEL0, TK_ZERO, TK_PRIMITIVE, TK_ZERO,
                TK_TEXEL0, TK_ZERO, TK_PRIMITIVE, TK_ZERO);

            put(0xBC00000Au, colourA);
            put(0xBC00040Au, colourA);
            put(0xBC00200Au, colourB);
            put(0xBC00240Au, colourB);
            put(0xBB000001u, 0xFFFFFFFFu);
            put(0xF5100000u, 0x07000000u);
            put(0xF50001F0u, 0x06000000u);
            put(0xF5000100u, 0x05000000u);
            {
                uint32_t glassDL = *(const uint32_t *)(model + 0x8030 + lodOff);
                if (glassDL != 0)
                    put(0x06000000u, glassDL);
            }
        }
    }

    /* 0xB4AA -- detail pass. */
    BrGfxEmitTexCmd(3,
        *(const void *const *)(model + 0x8014));
    put(0xE7000000u, 0);
    put(0xBA001402u, 0x00100000u);
    put(0xB900031Du, g_BrDrawModeBase | g_BrDrawRenderMode);

    BrRdpSetCombineLERP(put_slot(),
        TK_TEXEL0,   TK_ZERO, TK_PRIMITIVE, TK_ZERO,
        TK_ZERO,     TK_ZERO, TK_ZERO,      TK_PRIMITIVE,
        TK_ZERO,     TK_ZERO, TK_ZERO,      TK_COMBINED,
        TK_ZERO,     TK_ZERO, TK_ZERO,      TK_COMBINED);

    put(0xBC00000Au, colourA);
    put(0xBC00040Au, colourA);
    put(0xBC00200Au, colourB);
    put(0xBC00240Au, colourB);
    put(0xBB000001u, 0xFFFFFFFFu);
    put(0xF5100000u, 0x07000000u);
    put(0xF50001F0u, 0x06000000u);
    put(0xF5000100u, 0x05000000u);
    {
        uint32_t detailDL = *(const uint32_t *)(model + 0x8024 + lodOff);
        if (detailDL != 0)
            put(0x06000000u, detailDL);
    }

    /* TODO 0xB685-0xB925: reflection pass.
     * Gated on g_BrDrawReflectEnable (BSS 0), g_BrDrawWheelAlt == 0,
     * BrG_6C6624 == 0, g_BrDrawSuppress == 0, car->i29B4 == 0, plus a
     * two-arm test on BrG_6C661C vs the player.  Dead path in the running
     * port (g_BrDrawReflectEnable is BSS 0). */

    /* 0xB925 -- post-detail setup block. */
    put(0xE7000000u, 0);
    put(0xBA001402u, 0x00100000u);

    {
        uint32_t geomVal = 0x00040000u;
        if (g_BrDrawReflectFlag != 0)
            geomVal |= 0x00080000u;
        put(0xB7000000u, geomVal);
    }

    put(0xBB000001u, 0x08001000u);
    put(0xBA000C02u, BrG_6C0258);

    BrRdpSetCombineLERP(put_slot(),
        TK_TEXEL0,   TK_ZERO, TK_SHADE,     TK_ZERO,
        0x3F4,       TK_ZERO, TK_SHADE,     TK_ZERO,
        TK_TEXEL0,   TK_ZERO, TK_SHADE,     TK_ZERO,
        TK_TEXEL0,   TK_ZERO, TK_SHADE,     TK_ZERO);

    /* 0xBA18 -- 3-arm FB colour (G_SETENVCOLOR). */
    {
        uint32_t fbVal;
        uint32_t rgb = ((uint32_t)BrG_6C0260 << 24) |
                       ((uint32_t)BrG_6C1614 << 16) |
                       ((uint32_t)BrG_6C0200 << 8);

        if (BrG_6C6618 != 0) {
            if (specMem != 0) {
                uint32_t a = ((uint32_t)g_BrDrawByte78 >> 3) - 0x21;
                fbVal = rgb | (a & 0xFFu);
            } else {
                uint32_t a = ((uint32_t)g_BrDrawByte78 >> 1) + 0x7F;
                fbVal = rgb | (a & 0xFFu);
            }
        } else {
            fbVal = rgb | 0xFFu;
        }
        put(0xFB000000u, fbVal);
    }

    put(0xB900031Du, 0);
    put(0xB900031Du, g_BrDrawModeBase | g_BrDrawRenderMode);
    put(0xE8000000u, 0);
    put(0xBA000E02u, 0);

    /* 0xBB70 -- DC texture: indexed lookup via car+0x2714 and g_BrDrawRefIndex. */
    {
        int32_t idx2714 = *(const int32_t *)(car + BR_CAR_OFF_I2714);
        int8_t tblIdx = g_BrDrawRefTbl[idx2714 * 2 + g_BrDrawRefIndex];
        uint32_t texVal = g_BrDrawRefColors[tblIdx];
        put((texVal & 0x00FFFFFFu) | 0xDC000000u, 1);
    }

    put(0xBA000E02u, 0x00008000u);

    /* 0xBBCF -- F2 settile from player+0x2718 * -20.3718f. */
    {
        const float *pPlayer = (const float *)BrG_6C2CF8;
        float tileF = pPlayer[0x2718 / 4] * -20.3718f;
        int32_t tile = (int32_t)tileF;
        int32_t lo = tile + 2;
        int32_t hi = tile + 0x7E;
        uint32_t w0 = ((lo << 12) & 0x00FFF000u) | 0xF2000002u;
        uint32_t w1 = ((hi << 12) & 0x00FFF000u) | 0x000001FEu;
        put(w0, w1);
    }

    put(0xE7000000u, 0);
    put(0x03840010u, specMem);
    put(0x03820010u, specMem + 0x10u);

    /* 0xBC7B -- 2nd body DL at model + lodOff + 0x8028. */
    {
        uint32_t bodyDL2 = *(const uint32_t *)(model + 0x8028 + lodOff);
        if (bodyDL2 != 0)
            put(0x06000000u, bodyDL2);
    }

    /* 0xBCBF -- reflection DL at model + lodOff + 0x803C (conditional). */
    {
        uint32_t refDL = *(const uint32_t *)(model + 0x803C + lodOff);
        if (refDL != 0 &&
            (g_BrDrawSuppress != 0 ||
             *(const int32_t *)(car + BR_CAR_OFF_I29B4) != 0))
            put(0x06000000u, refDL);
    }

    /* 0xBD1A -- pop the model matrix, clear geom, restore light colours. */
    put(0xBD000000u, 0);
    put(0xB6000000u, 0x00040000u);
    put(0xBC00000Au, colourA);
    put(0xBC00040Au, colourA);
    put(0xBC00200Au, colourB);
    put(0xBC00240Au, colourB);
    put(0xBA000C02u, BrG_6C0258);
    put(0xBA000E02u, 0);

    /* 0xBDE8 -- late wheel call (non-class 2). */
    if (bKind != 2)
        wheel_call(car);

    /* 0xBE14 -- final: sync, combiner, render mode. */
    put(0xE7000000u, 0);
    put(0xBA001402u, 0);

    BrRdpSetCombineLERP(put_slot(),
        TK_TEXEL0, TK_ZERO, TK_PRIMITIVE, TK_ZERO,
        TK_TEXEL0, TK_ZERO, TK_PRIMITIVE, TK_ZERO,
        TK_TEXEL0, TK_ZERO, TK_PRIMITIVE, TK_ZERO,
        TK_TEXEL0, TK_ZERO, TK_PRIMITIVE, TK_ZERO);

    put(0xB900031Du, 3);

    /* 0xBE98 -- model cost accumulation. */
    BrS17GetState()->f6C161C += *(const int32_t *)(model + 0x8000);
}
