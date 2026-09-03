/* br_drawcar.c -- see br_drawcar.h.  The vehicle's display list, from
 * BRGlide.dll.
 *
 * Read off the GLIDE build, which is this project's reference.  Both
 * functions here are classed `shared` in config/shared.csv, so the D3D
 * twins (0x1000C6E0 and 0x1000CBE0) are the same code under other numbers.
 */
#ifdef BR_MATCHING_BUILD
/* Header is (const void *, void *).  Original is a 4x4 int copy
 * (`mov ebp,[ecx+eax]` / `mov [eax],ebp`, not fld/fstp). */
#define BrGuMtxStore BrGuMtxStore_port
#endif
#include "br_drawcar.h"
#ifdef BR_MATCHING_BUILD
#undef BrGuMtxStore
#endif
#include "slice1_05.h"   /* BrGfxWords, BrRdpSetCombineLERP, BrMat4Mul   */
#include "slice2_15.h"   /* g_4B16A0 / g_4B16AC scene accumulators       */
#include "slice2_17.h"   /* BrGfxEmitTexCmd, BrS17GetState               */
#include "slice2_18.h"   /* BrG_6C0680 cursor; BrFogFactorAtPoint; car globals */
#include "slice2_19.h"   /* g_BrMtxSlot current projection slot          */
#ifdef BR_MATCHING_BUILD
/* Header is the port's (volume, x, y).  The original reads the span grid
 * as a global and takes only (x, y).  Hide the port prototype so this TU
 * can call the two-float form. */
#define BrSpanTestPoint BrSpanTestPoint_port
#endif
#include "slice2_21.h"   /* BrSpanVolume, BrSpanTestPoint                */
#ifdef BR_MATCHING_BUILD
#undef BrSpanTestPoint
/* Original pushes the two world-space floats as dwords (mov/push), not
 * through the x87.  Spelling the prototype as int32_t is what makes VC5
 * emit that; a float prototype fld/fstps and the function grows. */
int BrSpanTestPoint(int32_t xBits, int32_t yBits);
/* 0x10009C10 is cdecl 1-arg.  The 2-arg prototype is the port view; a
 * 2-arg call here would push a dummy at both wheel sites. */
void BrCarDrawWheels_raw(void *pCar);
/* 0x106E86AC -- original adds model+0x8000 into this dword, no getter. */
extern int32_t g_6C161C;
#endif
#include "br_racebegin.h" /* g_brRaceBeginDifficulty, g_brRaceBeginNTexSet */
#include "br_appstart.h"  /* g_brCfgGameMode                             */
#include "br_bootfrontier.h" /* BrBootGlobal_ABAA0                       */
#include "br_objlife.h"      /* g_AC300 (0x100ABAA0 mode-change flag)    */
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
uint32_t g_BrDrawReflectTexA;               /* 0x1184C474  DC tex, 6C661C  */
uint32_t g_BrDrawReflectTexB;               /* 0x1184C480  DC tex, default */

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
#ifdef BR_MATCHING_BUILD
/* It is a MACRO, not a call.  The bytes evaluate the SECOND word only after
 * the first has been stored -- 0x10009C9C writes 0xB900031D into [eax] and
 * only then loads 0x10273644 for [eax+4], and the wheel-list tail at
 * 0x10009F32 re-reads the model global after its own [eax] store.  A
 * function, even __inline, evaluates its arguments first, so VC5 CSEs that
 * load with the guard test in front of it and cross-jumps the two arms into
 * one append.  Every argument at every site here is a pure load or a
 * constant, so macro and call are equivalent; only the order differs. */
#define put(w0_, w1_)                                                    \
    do { uint32_t *p_ = BrG_6C0680;                                      \
         BrG_6C0680 += 2;                                                \
         p_[0] = (w0_);                                                  \
         p_[1] = (w1_); } while (0)
#else
static void put(uint32_t w0, uint32_t w1)
{
    uint32_t *p = BrG_6C0680;
    BrG_6C0680 += 2;
    p[0] = w0;
    p[1] = w1;
}
#endif

/* The command the combiner builder writes into.  The original bumps the
 * cursor BEFORE the call and hands the routine the old slot, so a caller
 * that inspects the cursor mid-flight sees it already advanced. */
static __inline BrGfxWords *put_slot(void)
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
#ifdef BR_MATCHING_BUILD
void BrGuMtxStore(const int pSrc[4][4], int pDst[4][4])
{
    int i, j;
    for (i = 0; i < 4; ++i)
        for (j = 0; j < 4; ++j)
            pDst[i][j] = pSrc[i][j];
}
#else
void BrGuMtxStore(const void *pSrc, void *pDst)
{
    const uint32_t *s = (const uint32_t *)pSrc;
    uint32_t       *d = (uint32_t *)pDst;
    int i;
    for (i = 0; i < 16; ++i)
        d[i] = s[i];
}
#endif

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
/* The model record the original reaches through the global 0x106EA398 --
 * slice2_18's BrG_6C3308.  Re-read at every use, the way the bytes do
 * (0x10009C11, 0x10009F24, 0x10009F45, 0x10009F53, 0x10009F73). */
#define BR_WHEEL_MDL(off) \
    (*(const uint32_t *)((const unsigned char *)BrG_6C3308 + (off)))

/* @implements 0x10009C10 glide BrCarDrawWheels */
void BrCarDrawWheels(const BrCarView *pCar, const BrModelView *pModel)
{
#ifdef BR_MATCHING_BUILD
    /* The original takes ONE argument -- the raw 0x2B68 car record -- reads
     * the model from the global above rather than from a second argument,
     * and its matrix allocator (0x10062500 == d3d 0x10069490) cannot fail,
     * so there is no NULL test and no separate display-list address.  The
     * port form below is the same code through the repacked view pair and
     * the frontier-safe allocator; this arm is what the bytes say.
     * BrCarView is byte-accurate only to +0x140, so bKind is reached by raw
     * offset the way BrCarDrawBody already reaches it. */
    const unsigned char *car = (const unsigned char *)pCar;
    const BrMat4 *pWheel;
    BrMat4       *pSlot;
    int           pass;

    (void)pModel;

    /* 0x10009C19 -- the gate, read once and from the model, not per pass */
    if (BR_WHEEL_MDL(0x80BCu) == 0)
        return;

    /* 0x10009C2C -- edi walks the four 0x40-byte wheel matrices at car+0x40;
     * the pass count is a spilled down-counter at [esp+0x10]. */
    pWheel = (const BrMat4 *)(car + BR_CAR_OFF_AWHEEL);
    pass = 4;
    do {
        put(0xE7000000u, 0);                    /* pipe sync            */
        put(0xBA001402u, 0x00100000u);          /* two-cycle            */

        if (car[BR_CAR_OFF_KIND] == 2) {
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

        /* 0x10009D75 -- the shared tail. */
        BrMat4Scale(&g_BrDrawScale, 0.003921569f, 0.003921569f, 0.003921569f);
        BrMat4Mul(&g_BrDrawScale, pWheel, &g_BrDrawWorld);

        pSlot = BrSub_10069490();               /* 0x10062500, no arguments */
        BrGuMtxStore(&g_BrDrawWorld, pSlot);
        put(0x01060040u, (uint32_t)pSlot);      /* gsSPMatrix, PUSH|LOAD */

        BrMat4Mul(&g_BrDrawWorld, &g_BrDrawView, &g_BrDrawCombined);

        pSlot = BrSub_10069490();
        BrGuMtxStore(&g_BrDrawCombined, pSlot);
        put(0x039E0010u, (uint32_t)pSlot);
        put(0x03980010u, (uint32_t)pSlot + 0x10u);
        put(0x039A0010u, (uint32_t)pSlot + 0x20u);
        put(0x039C0010u, (uint32_t)pSlot + 0x30u);

        put(0xBB000001u, 0xFFFFFFFFu);          /* texture on            */
        put(0xB6000000u, 0x000C0000u);          /* clear both texgen bits*/
        put(0xE8000000u, 0);                    /* tile sync             */
        put(0xF5100000u, 0x07000000u);
        put(0xF50001F0u, 0x06000000u);
        put(0xF5000100u, 0x05000000u);

        if (g_BrDrawWheelAlt != 0) {
            if (BR_WHEEL_MDL(0x80C4u) != 0)
                put(0x06000000u, BR_WHEEL_MDL(0x80C4u));
        } else {
            if (BR_WHEEL_MDL(0x80BCu) != 0)
                put(0x06000000u, BR_WHEEL_MDL(0x80BCu));
        }

        put(0xBD000000u, 0);                    /* pop matrix            */
        pWheel = pWheel + 1;
    } while (--pass != 0);
#else
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
#endif
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

#ifdef BR_MATCHING_BUILD
#define BR_CAR_SPAN(x, y) BrSpanTestPoint(*(int32_t *)&(x), *(int32_t *)&(y))
#else
#define BR_CAR_SPAN(x, y) BrSpanTestPoint(&g_BrFrameHull, (x), (y))
#endif

/* @implements 0x10009FC0 glide BrCarVisibilityUpdate */
void BrCarVisibilityUpdate(void *pCar)
{
    unsigned char *car = (unsigned char *)pCar;
    const BrVec3  *pPos;
    BrVec3         probe;

    /* +0xF08 is one of the record's guard pointers; nothing happens without it. */
    if (*(void **)(car + BR_CAR_OFF_GUARD) == NULL)
        return;

    pPos = (const BrVec3 *)(car + BR_CAR_OFF_POS);

    /* A distance from the car to BrG_6C6490's +0x30 that the original computes
     * and then discards (fstp st(0)).  Kept for fidelity; it has no effect. */
    (void)BrVec3Dist(pPos,
                     (const BrVec3 *)((const unsigned char *)BrG_6C6490 + 0x30));

    /* Index is reloaded at every write -- a cached iCar claims ebx, which
     * the original never saves. */
    g_BrCarVisOpaque[*(int32_t *)(car + BR_CAR_OFF_ICAR)] = 0;
    g_BrCarVisAny[*(int32_t *)(car + BR_CAR_OFF_ICAR)]    = 0;

    *(float *)(car + BR_CAR_OFF_FOG) = BrFogFactorAtPoint(pPos);

    /* The player's own car is never span-culled; jump straight to the
     * self/active-camera test.  Otherwise a car is visible if its position --
     * or, when a mode flag forces it, a point 6 units to its side -- lands in
     * the hull. */
    if ((void *)car != BrG_6C2CF8) {
        if (BrG_6C661C != 0 || BrG_6C6624 != 0) {
            BrVec3MulAdd(&probe, pPos, (const BrVec3 *)car, 6.0f);
            if (BR_CAR_SPAN(probe.x, probe.y) == 0 &&
                BR_CAR_SPAN(pPos->x, *(float *)(car + BR_CAR_OFF_POS + 4)) == 0)
                return;                         /* culled */
        } else if (BR_CAR_SPAN(pPos->x, *(float *)(car + BR_CAR_OFF_POS + 4)) == 0) {
            return;                             /* culled */
        }

        /* Visible.  The player-car branch below only applies to the player,
         * so a non-player car falls straight through to the flag set. */
        if ((void *)car != BrG_6C2CF8)
            goto set_flags;
    }

    /* Player car: if its active camera points at one of its own two cam
     * frames and the override flag is clear, translucent pass only. */
    {
        void *pActiveCam = *(void **)(car + BR_CAR_OFF_ACTIVECAM);
        if (pActiveCam == (void *)(car + BR_CAR_OFF_CAMA) ||
            pActiveCam == (void *)(car + BR_CAR_OFF_CAMB)) {
            if (BrG_6C6614 == 0) {
                g_BrCarVisAny[*(int32_t *)(car + BR_CAR_OFF_ICAR)] = 1;
                return;
            }
        }
    }

set_flags:
    /* Opaque cars (class != 2) show in both passes; class-2 cars only in the
     * translucent pass.  `one` is a register (eax = 1) both stores share. */
    {
        unsigned char kind = *(unsigned char *)(car + BR_CAR_OFF_KIND);
        int32_t       one  = 1;
        if (kind != 2)
            g_BrCarVisOpaque[*(int32_t *)(car + BR_CAR_OFF_ICAR)] = one;
        g_BrCarVisAny[*(int32_t *)(car + BR_CAR_OFF_ICAR)] = one;
    }
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
 * STATE 2026-09-03: 30 slot-masked divergence regions, 37 raw; 1843 vs
 * 1843 instructions (EQUAL -- no missing or extra code anywhere), 7536 vs
 * 7577 bytes.  The whole residue is encoding/allocation.  Region 1 is the
 * frame itself (`sub esp,0x48` vs orig `0x4c`); close that first, every
 * later region's displacements move with it.  Read the recomp's frame map
 * from a `/FAcs` listing (recipe in docs/VC5-IDIOMS.md) rather than
 * inferring it -- displacement histograms cannot be compared across two
 * builds whose frame sizes differ, and that is how the pack[0]/pack[1] claim
 * corrected below went wrong.
 * FRAME CENSUS 2026-09-03 (region 1, the 0x48-vs-0x4c gap).  Counting the
 * stack slots each build actually WRITES, rather than comparing raw
 * displacements (which cannot line up across two different frame sizes):
 *   orig   11 local dwords (0x10..0x30, 0x38, 0x3c) + the two packed BYTE
 *          slots 0x31/0x32 inside the 0x30 dword + 2 arg slots (0x60,0x64),
 *          and a 4-BYTE HOLE at 0x34 that it never writes;
 *   ours   12 local dwords, 0x10..0x3c contiguous and dense, no byte slots,
 *          + 2 arg slots (0x5c,0x60).
 * So orig's frame is LARGER while writing FEWER dwords -- the gap is not a
 * variable we are missing, it is that orig's packer left a hole and packed
 * two bytes into an existing dword where ours packs densely and spends a
 * whole dword.
 * ‼ RETRACTED 2026-09-03 (session 7), and the retraction re-opens region 1.
 * The census closed with "chasing 'which value are we not homing' is the
 * wrong question; instruction counts are equal (1843 = 1843), so no value
 * is missing."  THE COUNTS WERE NEVER EQUAL.  `divergence.py` was counting
 * the COFF function extent's 16-byte alignment padding -- FIFTEEN trailing
 * nops -- as recompiled code.  The tool is fixed (commit a00add5) and the
 * honest figure is 1,828 vs 1,843: this build is FIFTEEN INSTRUCTIONS AND
 * 56 BYTES SHORT.  Something IS missing, and the 4-byte frame gap is
 * consistent with it rather than a pure packing curiosity.  Do not quote
 * the equality again.
 * MEASURED, do not re-run: declaring pack[0]/pack[1] block-scoped inside the
 * colour if/else (their whole live range) is byte-identical -- it does not
 * move them out of the arg slots into byte slots.
 *
 * WORKLIST 2026-09-03, from `tools/msetdiff.py` (register-blind instruction
 * multiset, relocs masked, small immediates KEPT).  The region count cannot
 * see any of this -- divergence.py wildcards imm32 -- and the two builds
 * have EQUAL instruction counts, so these are byte-vs-dword storage choices,
 * not missing code:
 *   orig has 4 more `and R,0xff`, 4 more 32-bit `or R,R`, 2 more `shl R,8`
 *   and 3 more `mov byte ptr [esp+S],B`;
 *   ours has 4 more `mov dword ptr [esp+S],R` and 2 more `lea R,[R*K]`.
 * Read together: three or four values that the original keeps as BYTE
 * locals (stored to byte slots, read back with the dword-load + `and 0xff`
 * widening that is VC5's own idiom for a `unsigned char` local) are DWORD
 * locals here, and the colour packs in the else-arms are consequently built
 * with byte-lane moves (`mov dh,al`) where orig builds them with explicit
 * `shl R,8` / `or`.  `topB` is the first candidate to check.  This is the
 * same currency as the 0x31/0x32 byte-slot gap in the frame census above --
 * likely one defect, not two.
 *
 * SESSION 8 (2026-09-03): TWO OF THE FIFTEEN ARE BANKED, region 30 -> 29,
 * 1,828 -> 1,831 instructions, 56 -> 48 bytes short.  `--deltas` says this
 * function's drift is honest (no SUSPECT resyncs) and concentrated in two
 * blocks: region 5 -37 (the colour-arm cross-jump below) and region 6 -30;
 * every other region moves 0..5 bytes except region 21's +15.
 * A whole-function PUSH CENSUS is what found the closed one, and it is a
 * cheap triage worth repeating on any emit-heavy function: group the pushes
 * by the call that consumes them and compare the counts per call.  Orig and
 * ours had the same 34 call groups and identical register-push totals, and
 * exactly ONE group differed -- the model-DL hook, orig 4 pushes to our 2.
 * The fix is the branch-selected-emit lever again: the CALL belongs inside
 * each of the four arms, not one call on a selected `dlSel`.  Written that
 * way the block is instruction-for-instruction and register-for-register the
 * original, first arm's private `push eax; push ecx; jmp` included.
 * SAME SESSION, the census then paid again -- and this is the reusable
 * lesson.  Comparing each call's argument SEQUENCE (not just its multiset)
 * found TWO combiner calls whose sixteen tokens sat in the wrong slots.
 * The original's argument list is recoverable from the bytes with no
 * guessing at all: cdecl pushes right-to-left, so the Nth push is argument
 * (nargs + 1 - N), and `push ebp` is TK_ZERO because ebp is this function's
 * zero register.  Decoded that way the two calls read
 *   0xBA22: ZERO,ZERO,ZERO,TEXEL1_A / ZERO,ZERO,ZERO,TEXEL0  (twice)
 *   0xBA9A: TEXEL0,SHADE,0x3F4,SHADE / ZERO,ZERO,ZERO,TEXEL0 (twice)
 * -- both regular, and the second is an actual lerp between shade and
 * texel, which is what the function's name says it does.  The old spellings
 * had the right tokens in the wrong positions and passed TK_ZERO where the
 * original passes TEXEL0 and 0x3F4, so the emitted display list was wrong,
 * not merely differently compiled.  Four more regions: 29 -> 25 masked,
 * 33 raw, and 48 -> 40 bytes short.  All 34 call groups now agree token for
 * token.  ‼ Neither tool could see this: divergence.py wildcards imm32, and
 * a multiset comparison passes a permutation.  On any emit-heavy function,
 * run the sequence census before believing a region map.
 *
 * SESSION 12 (2026-09-03) -- THE LIGHT-DIRECTION COPY IS BYTE-EXACT, and it
 * proves a rule this file should have applied a session earlier: ‼ A DEAD
 * VERDICT MEASURED AGAINST A WRONG FRAME IS STALE.  That copy carried five
 * measured-dead spellings and the note "treat this region as T3a UNTIL THE
 * FRAME IS SOLVED"; the frame was solved last session, and the SIXTH spelling
 * lands instruction-for-instruction (see the comment at the site for the two
 * rules that make it work).  Masked regions 26 -> 24, instructions 14 short
 * -> 13.  RE-TEST EVERY allocation-sensitive "do not re-run" note in a
 * function after its frame -- or any other global allocation input -- moves.
 * SESSION 12 RE-TESTS of this file's own dead list, done under that rule:
 *   - arm 1's colourB through a named top local: STILL DEAD, and by the same
 *     tell as before -- region 3's first divergence moves orig+0x327 ->
 *     orig+0x30c and the reloc-masked byte diff rises 4,532 -> 4,680.
 *   - arm 1's pack as its OWN two-byte array (`packA[2]`, the storage class
 *     that closed the frame): AMBIGUOUS, and worth re-reading rather than
 *     re-running.  Measured after the light-direction fix it takes the byte
 *     diff 4,720 -> 4,562 and the instruction gap 13 short -> 10, recovering
 *     one of the two missing `shl R,8` -- but it introduces a redundant
 *     `mov B,B` and a slot reload, so `msetdiff` rows go 27 -> 28, the
 *     register-blind gap 57 -> 58, and region 3's first divergence again
 *     moves 27 bytes earlier.  NOT taken: it trades one real instruction for
 *     two spurious ones and un-merges the cross-jump only partially.  Revisit
 *     it only as part of attacking the byte-lane family, never on its own.
 *     (Note it FLIPPED from clearly-bad to ambiguous when the frame and the
 *     float copy landed -- more evidence for the staleness rule above.)
 *   - arm 1's colourA top through a named `uint8_t` (the session-10 lever
 *     that worked in arm 3): BYTE-IDENTICAL here.  Regions 2/3 stay
 *     canonicalisation, not allocation.
 * SESSION 12 SCREENS, both negative, so nobody re-runs them: this function's
 * frame now MATCHES (`tools/framescreen.py` no longer lists it), and it has
 * no `(double)` modelling and no qword spills, so the Glide-is-float lever
 * does not apply here.
 *
 * SESSION 11 (2026-09-03) -- ‼ REGION 1, THE FRAME, IS CLOSED.  `sub esp,
 * 0x4c` matches and the prologue is byte-exact instruction for instruction.
 * The fix was one declaration: the two colour-pack byte locals are an ARRAY,
 * `uint8_t pack[2]`, not two scalars.  VC5 never enregisters an array, so it
 * spends a locals-area slot on it instead of tucking two scalars into the
 * dead argument slots -- and that missing locals dword WAS the 0x48-vs-0x4c
 * gap the frame census below spends its whole length hunting.  ‼ The census's
 * closing claim ("chasing which value we are not homing is the wrong
 * question") is now RETRACTED for the second time: it was the right question,
 * and the answer was a storage class, not a value.  Masked regions 25 -> 23;
 * `msetdiff.py` rows unchanged at 25/11 with the two `sub esp` / `add esp`
 * rows retired in exchange for one float operand swap.  fn.py's RAW/REGNORM
 * read 2 worse because every slot displacement moved; trust msetdiff and the
 * masked region count here, not those.
 * SAME SESSION, a second block closed: the sky texture-window command words.
 * The original RE-READS `pSkyAng->s0` and `pSkyAng->t0` from the struct for
 * the SECOND word (`mov edx,[eax]; mov eax,[eax+4]` at 0x1798) -- they are
 * not named locals.  Caching them in `s`/`t` and assembling `w0`/`w1` before
 * the put made VC5 spill the pair and the finished word to slots
 * (`mov [esp+0x38],ecx; mov ecx,[esp+0x60]; mov edx,[esp+0x38]`).  Reading
 * the fields inline in both words makes the block instruction-for-instruction
 * the original and removes its 15-byte drift.  ‼ GENERALISE: a named local
 * that CACHES A STRUCT FIELD is wrong wherever the original re-reads it --
 * the same rule the frame note at the bottom of this header already states
 * for car+0x140 and BrG_6C3308.  Check every cached field against the bytes.
 * ‼ AND READ THE SIZE NUMBER CAREFULLY AFTER A FIX LIKE THAT: this one took
 * the function from 38 bytes short to 53 short, because the spill it removed
 * was three instructions of accidental padding against a real deficit
 * elsewhere.  Bytes moved the wrong way while the multiset went 64+75 ->
 * 52+66.  Rank by the register-blind multiset, never by size alone.
 *
 * SESSION 10 (2026-09-03) -- ARM 3 MOVES.  Its colourA top component now
 * goes through its OWN uint8_t local (`topA = BrG_6C1580;` assigned with
 * pack[0]/pack[1], third of the three, then `(uint32_t)topA << 8` in the pack)
 * instead of being nested inline as `(uint32_t)(uint8_t)BrG_6C1580`.  Inline,
 * VC5 loads it straight into the high lane (`mov dh,byte ptr [mem]`); named,
 * it loads to a byte register first and then `mov dh,cl`, which is what the
 * original does (`mov al,[106e8610]` at 0x3d6, `mov dh,al` at 0x3e9) and it
 * also lets all three byte loads issue together the way the original
 * schedules them.  Reloc-masked byte diff 4,658 -> 4,539; instructions
 * 1,831 -> 1,832 (12 short -> 11); bytes 7,537 -> 7,539 (40 short -> 38);
 * region 6's change -30 -> -28; masked regions FLAT at 25, frame intact
 * (first divergence still +0x2).  ‼ GENERALISE THIS BEFORE ANYTHING ELSE
 * HERE: the Horner packs' TOP component wants a named byte local wherever
 * the original loads it to a byte register before the lane move -- check
 * each pack site against the bytes, one at a time.
 * SESSION 10 PROBES, DEAD, do not re-run:
 *   - the same named-top-local applied to ARM 1's colourB (`top1 =
 *     g_BrDrawByte80`) looks better on size (38 -> 34 bytes short, 11 -> 9
 *     instructions) and is a REGRESSION: reloc-masked byte diff 4,539 ->
 *     4,716 and region 4's first divergence moves 27 bytes earlier,
 *     orig+0x327 -> orig+0x30c.  That is the identical signature the
 *     session-7 packA0/packA1 probe left, so it is one wall: ANY change
 *     inside arm 1's colourB pack un-merges the cross-jumped tail early.
 *     Judge arm 1 by the first-divergence address, never by size.
 *   - giving arm 3's colourA its own byte locals (packA0/packA1, distinct
 *     from the pack[0]/pack[1] that colourB reuses) is BYTE-IDENTICAL: VC5
 *     coalesces them back onto the same slots, so the two-slot question is
 *     not decided by how many variables the source declares.
 * What is LEFT in arm 3, and it is one instruction pair: the original homes
 * BOTH byte locals before colourA and reads both back widened (`mov
 * [esp+0x31],dl; mov [esp+0x32],cl` ... `mov eax,[esp+0x31]; and eax,0xff;
 * or edx,eax`), while we still let pack[0] forward from its register and spell
 * that term `mov dl,al`.  Only pack[1] goes through a slot here.
 *
 * SESSION 9 (2026-09-03) -- RE-RANKING, no movement (25 masked / 33 raw,
 * 1,831 vs 1,843 insns, 7,537 vs 7,577 bytes; unchanged from session 8).
 * ‼ REGION 6 IS MIS-ATTRIBUTED ABOVE.  Its -30 does NOT come from the
 * three-float light-direction copy that opens it at orig+0x4cf (that block
 * is 34 bytes in both builds); it accrues in the stretch BEFORE it.  Split
 * the 0x3c0..0x4cf window by block and it reads:
 *     arm 2 tail   orig 0x3c0-0x3ca vs ours 0x39b-0x3a5     0
 *     ARM 3        orig 0x3ca-0x427 vs ours 0x3a5-0x3ea   -24
 *     shared tail  orig 0x427-0x4cf vs ours 0x3ea-0x48c    -6
 * So arm 3 carries four fifths of region 6, and with region 5's -37 (arm 1)
 * the two colour arms are 61 of this function's 40-byte deficit.  Grind
 * arm 3, not the float copy -- the T3a note on the copy stands, it is just
 * not where the bytes are.
 * WHAT ARM 3 ACTUALLY DIFFERS BY, read instruction for instruction: the
 * original materialises BOTH byte locals into their slots before colourA
 * and reads both back with the dword-load + `and 0xff` widening
 * (`mov [esp+0x31],dl; mov [esp+0x32],cl; mov ecx,[esp+0x32]; mov dh,al;
 * mov eax,[esp+0x31]; and ecx,0xff; and eax,0xff; or edx,eax`), then does
 * it a SECOND time for colourB.  We home only one of the two and let VC5
 * forward the other from its register, so the `top << 8 | pack[0]` merge
 * collapses into two byte-lane moves (`mov dh,al; mov dl,al`) instead of
 * `mov dh,al; or edx,<widened>`.  That single forwarded copy is the whole
 * -24: it is the same currency as the WORKLIST rows above (orig's 4 extra
 * `and R,0xff` / 4 extra `or R,R` / 2 extra `shl R,8` / 3 extra
 * `mov byte [esp+S],B`), so those rows are ONE defect at ONE site, not a
 * family spread over the function.
 * SESSION 9 PROBE, DEAD, do not re-run: spelling arm 3's colourA pack terms
 * with an explicit widening -- `(pack[0] & 0xFFu)` / `(pack[1] & 0xFFu)` -- is
 * BYTE-IDENTICAL.  VC5 folds a redundant `& 0xFF` on an already-uint8_t
 * operand before it chooses the byte lane, so the widening cannot be
 * requested from the source at this site; what decides it is whether the
 * value is still live in a register, i.e. the same forwarding question.
 *
 * WHERE THE 15 MISSING INSTRUCTIONS ARE (2026-09-03, session 7).  The
 * biggest single gap is region 4/5: orig runs 0x327..0x3c0 where we run
 * 0x327..0x39b, 37 bytes short in one block.  Read the two streams and the
 * cause is plain -- THE ORIGINAL GIVES ARM 1 ITS OWN COPY OF THE colourB
 * PACK and we cross-jump arm 1 into the arm-2/3 shared tail.  Orig arm 1
 * runs its own 0x327-0x358 (`mov dl,[6C65BC]; mov [esp+0x32],dl; xor ecx,
 * ecx; mov edx,[esp+0x31]; mov ch,al; mov eax,[esp+0x32]; and/or/shl...`)
 * and only then `jmp 0x446`; ours emits four instructions and jumps
 * straight into the tail that arms 2 and 3 share.  The reason the original
 * does NOT merge them is that its arm-1 copy interleaves the leftover x87
 * pop (`fstp st(0)` at 0x356, near the END of the pack) while ours
 * schedules the pop before the jump, which makes the two tails
 * byte-identical and lets VC5 cross-jump all three arms instead of two.
 * That is the same cross-jumping-of-identical-tails emitter residue the
 * C++ lane hit; it is NOT the byte-slot spelling, which is already right.
 * MEASURED DEAD, do not re-run: giving arm 1 its own block-scoped
 * `packA0`/`packA1` byte locals so the tails read different slots.  It
 * does un-merge part of the pack -- +3 real instructions, and the two
 * `shl R,8` and the `mov B,B` drop out of the multiset -- but it moves
 * region 4's FIRST divergence 27 bytes EARLIER (orig+0x327 -> orig+0x30c)
 * for no region-count change, so it is a net regression.  (Judge this
 * region by the first-divergence address, not the region count: the count
 * stayed 30/37 through both the good and the bad half of that probe.)
 *
 * Regions 2 and 3 (orig+0x2c8 / +0x2f0) are ONE defect: in colourA's
 * Horner pack the first ftol result goes to the HIGH byte in orig
 * (`xor edx,edx; mov dh,al` ... `mov dl,al`) and to the LOW byte in ours
 * (`mov dl,al` ... `mov dh,al`).  MEASURED DEAD, do not re-run: swapping
 * the `|` operands, adding the missing `(uint32_t)` cast to the low term,
 * splitting the top component into its own statement, hoisting it into a
 * block-scoped temp, and swapping the two globals' high/low roles -- all
 * five are byte-identical to the current form.  The byte lane follows
 * VC5's evaluation order (simpler subtree first), which no commutative or
 * statement-level spelling reaches; see the canonicalisation entry in
 * docs/VC5-IDIOMS.md.
 *
 * Frame: `sub esp, 0x4c; push ebx; mov ebx, pCar; push ebp; xor ebp,ebp`.
 * ebp is the zero register (154 uses: `push ebp` for TK_ZERO / put w1=0).
 * Arg slots are reused: lodBias at [esp+0x64] becomes lod, then a command
 * pointer in the reflection pass.  Re-read car+0x140 and BrG_6C3308; do
 * not cache them.
 * ==================================================================== */
/* @implements 0x1000A110 glide BrCarDrawVehicle */
void BrCarDrawVehicle(void *pCar, int32_t lodBias)
{
    unsigned char *car = (unsigned char *)pCar;
    int32_t  lod, distNear, flag290C;
    float    dist;
    uint32_t colourA, colourB;
    uint8_t  pack[2];  /* ‼ AN ARRAY, NOT TWO SCALARS -- THIS IS WHAT
                             * CLOSES THE FRAME.  VC5 never enregisters an
                             * array, so `pack` gets its own slot in the
                             * LOCALS area instead of being packed into the
                             * dead argument slots, and that is the
                             * original's layout: byte slots 0x31/0x32
                             * overlaid on the upper bytes of a dword.  It
                             * takes `sub esp,0x48` to `sub esp,0x4c` and the
                             * prologue is now BYTE EXACT against the
                             * original, instruction for instruction.
                             * Two scalars (`uint8_t pack0, pack1;`) was the
                             * form here for eight sessions, and the /FAcs
                             * equate table showed why it could never work:
                             * `_pack0$ = 8, _pack1$ = 12` -- VC5 had put
                             * them in the reused ARG slots and spent no
                             * locals-area dword at all, which IS the 4-byte
                             * frame gap the census below hunts for.  Do not
                             * go back to scalars to tidy the spelling. */
    uint32_t lodOff;
    uint32_t specMem = 0;
    BrSkyAngles *pSkyAng = 0;
    BrMat4  *pSlot;
    BrVec3   dirTmp;
    BrVec3   glassTmp;

    /* 0xA11B -- six guard tests.  Orig compares against ebp (zero-reg). */
    if (*(void **)(car + BR_CAR_OFF_GUARD) == 0) return;
    if (*(void **)(car + BR_CAR_OFF_P0168) == 0) return;
    if (*(void **)(car + BR_CAR_OFF_P0170) == 0) return;
    if (*(void **)(car + BR_CAR_OFF_P016C) == 0) return;
    if (*(void **)(car + BR_CAR_OFF_P0174) == 0) return;
    flag290C = 0;
    if (g_BrCarVisAny[*(int32_t *)(car + BR_CAR_OFF_ICAR)] == 0) return;

    /* 0xA174 -- distance + LOD. */
    dist = BrVec3Dist(
        (const BrVec3 *)(car + BR_CAR_OFF_POS),
        (const BrVec3 *)((const unsigned char *)BrG_6C6490 + 0x30));

    /* 0xA198 -- fog (class 2 only).  Orig never caches +0x29AF; read it
     * fresh at every site (5 reads in the original). */
    if (*(car + BR_CAR_OFF_KIND) == 2) {
        g_BrDrawFogAlpha =
            (int32_t)(*(const float *)(car + BR_CAR_OFF_ALPHA) * 255.0f);
        put(0xF8000000u,
            ((((uint32_t)(uint8_t)BrG_6C0260 << 8
             | (uint8_t)BrG_6C1614) << 8
             | (uint8_t)BrG_6C0200) << 8)
             | ((uint32_t)g_BrDrawFogAlpha & 0xFFu));
    }

    /* 0xA1F1 -- flag290C gate from track records. */
    *(int32_t *)(car + BR_CAR_OFF_I2714) = 0;
    if (*(void *const *)(car + BR_CAR_OFF_P294C) != 0) {
        uint32_t k = *(const uint16_t *)(car + BR_CAR_OFF_U290C);
        /* Read the flags global inline -- a hoisted pointer local occupies
         * edx and rotates the whole function's register assignment. */
        if (((const unsigned char *)g_BrDrawTrackFlags)[k * 84 + 0x4C] & 0x10) {
            flag290C = 1;
            *(int32_t *)(car + BR_CAR_OFF_I2714) = 1;
        }
    }

    /* 0xA232 -- model setup.  Re-read BrG_6C3308 at every use. */
    BrG_6C3308 = *(void *const *)(car + BR_CAR_OFF_MODEL);

    /* 0xA23D -- LOD computation. */
    if (g_brRaceBeginNTexSet == 2) {
        if (!(dist >= 40.0f)) {
            lod = 0;
        } else {
            lod = 1;
            if (dist >= 80.0f) lod = 2;
        }
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

#ifdef BR_MATCHING_BUILD
    /* 0x10062500 cannot fail; store the pointer itself (re-read iCar). */
    pSlot = BrSub_10069490();
    g_BrCarMtxSlot[*(int32_t *)(car + BR_CAR_OFF_ICAR)] = (uint32_t)pSlot;
    BrGuMtxStore(&g_BrDrawWorld,
        (int (*)[4])g_BrCarMtxSlot[*(int32_t *)(car + BR_CAR_OFF_ICAR)]);
#else
    pSlot = mtx_alloc(&g_BrCarMtxSlot[*(int32_t *)(car + BR_CAR_OFF_ICAR)]);
    if (pSlot) BrGuMtxStore(&g_BrDrawWorld, pSlot);
#endif

    BrMat4Mul(&g_BrDrawWorld, &g_BrDrawView, &g_BrDrawCombined);
    BrGuMtxHookNop(&g_BrDrawCombined);

#ifdef BR_MATCHING_BUILD
    pSlot = BrSub_10069490();
    g_BrCarLightSlot[*(int32_t *)(car + BR_CAR_OFF_ICAR)] = (uint32_t)pSlot;
    BrGuMtxStore(&g_BrDrawCombined,
        (int (*)[4])g_BrCarLightSlot[*(int32_t *)(car + BR_CAR_OFF_ICAR)]);
#else
    pSlot = mtx_alloc(&g_BrCarLightSlot[*(int32_t *)(car + BR_CAR_OFF_ICAR)]);
    if (pSlot) BrGuMtxStore(&g_BrDrawCombined, pSlot);
#endif

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
    g_BrDrawClass[*(int32_t *)(car + BR_CAR_OFF_ICAR)] = lod;

    /* 0xA393 -- three-arm light colour computation. */
    /* Colours are packed HORNER-style -- (((top<<8 | p0) << 8 | p1) << 8) --
     * never as independent <<24|<<16|<<8 terms.  The middle/low components
     * of colourB go through the byte locals pack[0]/pack[1] in ALL three
     * arms (arms 2 and 3 tail-merge from `mov eax,[esp+0x31]` on).  Arm 1's
     * colourA nests the ftol results directly: dh/dl take the first two
     * as (uint8_t) casts, the third is spelled `& 0xFF`. */
    if (BrG_6C661C != 0) {
        float div = dist * 0.1f;
        if (!(div >= 1.0f)) div = 1.0f;
        colourA = ((((uint32_t)(uint8_t)(int32_t)((float)(int32_t)BrG_6C1580 / div) << 8
                   | (uint8_t)(int32_t)((float)(int32_t)BrG_6C335C / div)) << 8
                   | ((uint32_t)(int32_t)((float)(int32_t)BrG_6C0968 / div) & 0xFF)) << 8);
        pack[0] = BrG_6C0960;
        pack[1] = BrG_6C65BC;
        colourB = ((((uint32_t)(uint8_t)g_BrDrawByte80 << 8 | pack[0]) << 8
                   | pack[1]) << 8);
    } else {
        /* arms 2 (dim *4/5) and 3 (plain) share colourB's Horner tail --
         * the original merges them at 0x427, spilling pack[0]/pack[1] to the
         * [esp+0x31]/[esp+0x32] byte slots and reading them back with & 0xFF
         * at the common pack.  Factor the final statement out to reproduce it. */
        uint8_t topB, topA;
        if (flag290C != 0) {
            topB  = (uint8_t)((g_BrDrawByte80 * 4) / 5);
            colourA = 0;
            pack[0] = (uint8_t)((BrG_6C0960 * 4) / 5);
            pack[1] = (uint8_t)((BrG_6C65BC * 4) / 5);
        } else {
            pack[0] = BrG_6C335C;
            pack[1] = BrG_6C0968;
            topA  = BrG_6C1580;
            colourA = ((((uint32_t)topA << 8 | pack[0]) << 8
                       | pack[1]) << 8);
            topB  = g_BrDrawByte80;
            pack[0] = BrG_6C0960;
            pack[1] = BrG_6C65BC;
        }
        colourB = ((((uint32_t)topB << 8 | pack[0]) << 8 | pack[1]) << 8);
    }

    /* 0xA556 -- two G_MTX pushes: model and projection. */
    put(0x01060040u, g_BrCarMtxSlot[*(int32_t *)(car + BR_CAR_OFF_ICAR)]);
    put(0x01030040u, (uint32_t)(uintptr_t)g_BrMtxSlot);

    /* 0xA5A1 -- light-direction computation: build g_BrDrawDir0 and
     * g_BrDrawDir1 from camera, player, and car positions. */
    if (BrG_6C661C != 0) {
        if (BrG_6C6490 == (void *)((unsigned char *)BrG_6C2CF8 + 0x2808))
            BrVec3Negate(&g_BrDrawDir0, (const BrVec3 *)BrG_6C6490);
        else
            BrVec3Negate(&g_BrDrawDir0, (const BrVec3 *)BrG_6C2CF8);
    } else {
        /* ‼ SOLVED 2026-09-03, and exactly as the old note predicted: "treat
         * this region as T3a UNTIL THE FRAME IS SOLVED".  The frame is solved
         * now (see the header's session-11 entry), and the sixth copy
         * spelling lands byte-exact where five had failed against the wrong
         * frame.  This block is instruction-for-instruction the original:
         *     fld z; fld y; mov x; fstp y; mov x; fstp z
         * Two rules make it work.  BOTH floats need a named temp, so their
         * live ranges overlap and VC5 keeps them on the x87 stack; and the
         * INTEGER member must be stored FIRST, because a temp whose load and
         * store are adjacent gets copy-propagated into an integer move (that
         * is what happened to y when its store came before x's, leaving one
         * fld/fstp instead of two).  x is spelled inline for the same reason
         * in reverse: it is the one that SHOULD be an integer move.
         * GENERAL LESSON, worth more than the region: a dead verdict measured
         * against a wrong frame is STALE.  Re-test every allocation-sensitive
         * "measured, do not re-run" note in a function after its frame
         * changes -- this one had five spellings on it. */
        {
        float fz = BrG_6C0670.z, fy = BrG_6C0670.y;
        g_BrDrawDir0.x = BrG_6C0670.x;
        g_BrDrawDir0.y = fy;
        g_BrDrawDir0.z = fz;
        }
    }
    BrVec3NormaliseGuard(&g_BrDrawDir0);

    /* Integer field copy, order x, z, y. */
    g_BrDrawDir1.x = g_BrDrawDir0.x;
    g_BrDrawDir1.z = g_BrDrawDir0.z;
    g_BrDrawDir1.y = g_BrDrawDir0.y;

    {
        float  len;
        BrVec3Sub(&dirTmp,
            (const BrVec3 *)((const unsigned char *)BrG_6C6490 + 0x30),
            (const BrVec3 *)(car + BR_CAR_OFF_POS));
        len = BrVec3Length(&dirTmp);
        if (len == 0.0f)
            BrVec3Negate(&dirTmp, (const BrVec3 *)BrG_6C6490);
        else
            BrVec3DivBy(&dirTmp, len);

        BrVec3Midpoint(&g_BrDrawDir1, &dirTmp, &g_BrDrawDir1);

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

    /* 0xA6F6 -- four pool allocations, then look-at / angles.
     * Orig: 0x10062500 (discarded), 0x10062550 → pSkyAng,
     * two 0x100625A0 → pLights then specMem. */
    {
        BrLightPair   *pLights;
        float          eyeX, eyeY, atOffset, eyeScale;
        const float   *pCam = (const float *)BrG_6C6490;
        const float   *pCarF = (const float *)car;

#ifdef BR_MATCHING_BUILD
        (void)BrSub_10069490();
#endif
        pSkyAng  = (BrSkyAngles *)BrPool16Alloc();
        pLights  = (BrLightPair *)BrPool32Alloc();
#ifdef BR_MATCHING_BUILD
        specMem  = (uint32_t)(uintptr_t)BrPool32Alloc();
#else
        specMem  = pLights ? (uint32_t)(uintptr_t)pLights : 0;
        (void)BrPool32Alloc();
#endif

        atOffset = 0.0f;
        eyeScale = 0.0f;

        /* x/y compared ONCE; z picks the arm (orig 0x61c-0x655). */
        if (pCam[12] == pCarF[12] && pCam[13] == pCarF[13]) {
            if (pCam[14] == pCarF[14])
                atOffset = 1.0f;
            else
                eyeScale = 0.1f;
        }

        eyeX = pCam[0];
        eyeY = pCam[1];
        if (eyeX == 0.0f && eyeY == 0.0f)
            eyeX = 0.0001f;

        BrLightDirsFromLookAt(&g_BrDrawCombined, pLights,
            eyeX, eyeY, 0.0f,
            0.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f);

        BrLightDirsAndAngles(&g_BrDrawCombined, pLights, pSkyAng,
            pCam[12], pCam[13], pCam[14],
            pCarF[12] + eyeScale, pCarF[13],
            pCarF[14] + atOffset,
            0.0f, 0.0f, 1.0f,
            g_BrDrawDir1.x, g_BrDrawDir1.y, g_BrDrawDir1.z,
            g_BrDrawDir1.x, g_BrDrawDir1.y, g_BrDrawDir1.z,
            64, 64);
    }

    /* 0xA820 -- dist-gated canned body-setup DL (0x100A9FC8 vs 0x100A9F00).
     * Orig stores the ADDRESS of the object as an immediate, not a load. */
    if (dist > 10.0f)
        put(0x06000000u, (uint32_t)(uintptr_t)&BrG_0AA838);
    else
        put(0x06000000u, (uint32_t)(uintptr_t)&BrG_0AA770);

    /* 0xA86A -- Lights1 emission: static or dynamic. */
    if (BrG_6C661C == 0 && BrG_6C6624 == 0) {
        put(0xBC000002u, 0x80000040u);
        put(0x03860010u, (uint32_t)(uintptr_t)&BrG_0AA868);
        put(0x03880010u, (uint32_t)(uintptr_t)&BrG_0AA860);
    } else {
        /* icar is RE-READ from car+0x140 for the copy and for EVERY byte
         * store (bases 0x102733b0/b1/b2 fold the +0x10/11/12); only the
         * player pointer is cached (esi). */
        const float *pPlayer;
#ifdef BR_MATCHING_BUILD
        memcpy(&g_BrDrawLights[*(int32_t *)(car + BR_CAR_OFF_ICAR) * 24],
               (const void *)&BrG_0AA860, 24);
#else
        memcpy(&g_BrDrawLights[*(int32_t *)(car + BR_CAR_OFF_ICAR) * 24],
               (const void *)BrG_0AA860, 24);
#endif
        pPlayer = (const float *)BrG_6C2CF8;
        g_BrDrawLights[*(int32_t *)(car + BR_CAR_OFF_ICAR) * 24 + 0x10] =
            (uint8_t)(int32_t)(pPlayer[0] * -120.0f);
        g_BrDrawLights[*(int32_t *)(car + BR_CAR_OFF_ICAR) * 24 + 0x11] =
            (uint8_t)(int32_t)(pPlayer[1] * -120.0f);
        g_BrDrawLights[*(int32_t *)(car + BR_CAR_OFF_ICAR) * 24 + 0x12] =
            (uint8_t)(int32_t)(pPlayer[2] * -120.0f);
        put(0xBC000002u, 0x80000040u);
        /* Both payloads recompute icar*24 from car+0x140 -- the original
         * does NOT reuse dst here (lea edx,[ecx+ecx*2]; lea [edx*8+base]). */
        put(0x03860010u, (uint32_t)(uintptr_t)
            &g_BrDrawLights[*(int32_t *)(car + BR_CAR_OFF_ICAR) * 24 + 8]);
        put(0x03880010u, (uint32_t)(uintptr_t)
            &g_BrDrawLights[*(int32_t *)(car + BR_CAR_OFF_ICAR) * 24]);
    }

    /* 0xA9CE -- post-lights header: sync, two-cycle, geom mode. */
    put(0xE7000000u, 0);
    put(0xBA001001u, 0x00010000u);
    put(0xB7000000u, 0x00020205u);

    /* 0xAA25 -- BrG_6C6618 branch: set geom + render mode. */
    if (BrG_6C6618 != 0) {
        put(0xB7000000u, 0x00010000u);
        if (*(car + BR_CAR_OFF_KIND) != 2)
            g_BrDrawRenderMode = 0xC8000000u;
        else
            g_BrDrawRenderMode = 0x0C080000u;
    } else {
        g_BrDrawRenderMode = 0x0C080000u;
    }

    /* 0xAA83 -- culling: set (B7) and clear (B6), difficulty-swapped.
     * Each value is an inline xor + neg/sbb ternary, recomputed per put
     * (both globals re-read for the second emit). */
    put(0xB7000000u,
        ((g_brRaceBeginDifficulty ^ BrG_6C1174) ? 0x00001000u : 0x00002000u));
    put(0xB6000000u,
        ((g_brRaceBeginDifficulty ^ BrG_6C1174) ? 0x00002000u : 0x00001000u));

    /* 0xAADE -- render mode base selection. */
    if (*(car + BR_CAR_OFF_KIND) == 2) {
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

    /* 0xACCA -- early wheel call (class 2 only).  Orig: push ebx; call; add esp,4. */
#ifdef BR_MATCHING_BUILD
    if (car[BR_CAR_OFF_KIND] == 2)
        BrCarDrawWheels_raw(car);
#else
    if (car[BR_CAR_OFF_KIND] == 2)
        wheel_call(car);
#endif

    /* 0xACE3 -- four light MOVEMEMs (unconditional, +0x10/+0x20/+0x30). */
    put(0x039E0010u, g_BrCarLightSlot[*(int32_t *)(car + BR_CAR_OFF_ICAR)]);
    put(0x03980010u, g_BrCarLightSlot[*(int32_t *)(car + BR_CAR_OFF_ICAR)] + 0x10);
    put(0x039A0010u, g_BrCarLightSlot[*(int32_t *)(car + BR_CAR_OFF_ICAR)] + 0x20);
    put(0x039C0010u, g_BrCarLightSlot[*(int32_t *)(car + BR_CAR_OFF_ICAR)] + 0x30);

    /* 0xAD77 -- TLUT palette load.  Orig stores DL word1 as an address-of-symbol
     * immediate (mov [eax+4], OFFSET g_BrDrawTexBlob), not the pointer's runtime
     * value.  &g_BrDrawTexBlob reproduces that store form for the matching build;
     * the port keeps the value-read semantics. */
#ifdef BR_MATCHING_BUILD
    put(0xFD100000u, (uint32_t)(uintptr_t)&g_BrDrawTexBlob);
#else
    put(0xFD100000u, (uint32_t)(uintptr_t)g_BrDrawTexBlob);
#endif
    put(0xE8000000u, 0);
    put(0xF50001E0u, 0x07000000u);
    put(0xE6000000u, 0);
    put(0xF0000000u, 0x0703C000u);
    put(0xE7000000u, 0);

    /* 0xAE34 -- specular MOVEMEM (payload from the TODO specular block). */
    put(0x03840010u, specMem);
    put(0x03820010u, specMem + 0x10);

    /* 0xAE72 -- underside pass (gated on suppress + i29B4). */
    /* lea eax,[eax+eax*4]; shl eax,3  — not imul 40. */
    /* lodOff is NOT computed here -- the 0x8038 and 0x8030 sites inline
     * (lod+lod*4)<<3, and only the 0x8024 site assigns lodOff
     * (orig 0x153d stores it to the dead pCar arg slot). */

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
            if (*(const uint32_t *)((const unsigned char *)BrG_6C3308 + 0x8038 +
                    (uint32_t)((lod + lod * 4) << 3)) != 0)
                put(0x06000000u, *(const uint32_t *)((const unsigned char *)BrG_6C3308 + 0x8038 +
                    (uint32_t)((lod + lod * 4) << 3)));
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
        uint8_t iTex = *(const uint8_t *)((const unsigned char *)BrG_6C3308 + 0x811B);
        const unsigned char *pTexRecs =
            *(const unsigned char *const *)((const unsigned char *)BrG_6C3308 + 0x8014);
        /* Orig reads the 0x100ABAA0 mode-change flag DIRECTLY (cmp dword
         * [0x100abaa0],ebp); the port routed it through the BrBootGlobal_ABAA0
         * stub, which cannot inline across TUs at /O2.  g_AC300 is that flag. */
        if (*(const uint32_t *)(pTexRecs + (uint32_t)iTex * 36 + 4) != 0 &&
            g_AC300 == 0) {
            float fe68 = *(const float *)(car + BR_CAR_OFF_F0E68);
            uint32_t auxFlags =
                *(const uint32_t *)(*(void *const *)(car + BR_CAR_OFF_U29C0));
            /* The CALL lives inside each of the four arms -- there is no
             * `dlSel` variable.  That is what puts the whole two-argument
             * setup in the first arm (`mov eax,[ecx+0x90]; mov ecx,[ecx+0x80];
             * push eax; push ecx; jmp`) and lets VC5 cross-jump only the other
             * three, which is exactly the original's block layout; selecting
             * into one variable and calling once gives a single shared push
             * pair and loses two instructions.  Same lever as the branch-
             * selected DL emits (docs/VC5-IDIOMS.md).
             * Orig calls the hook UNCONDITIONALLY and reads dlBase (model+0x80)
             * at the call site, not hoisted -- the null-check was a port-safety
             * addition the original never had. */
#ifdef BR_MATCHING_BUILD
#define BR_DLHOOK(sel) g_BrDrawModelDlHook( \
                *(const uint32_t *)((const unsigned char *)BrG_6C3308 + 0x80), (sel))
#else
#define BR_DLHOOK(sel) do { if (g_BrDrawModelDlHook) g_BrDrawModelDlHook( \
                *(const uint32_t *)((const unsigned char *)BrG_6C3308 + 0x80), (sel)); \
            } while (0)
#endif
            if (auxFlags & 0xC0000u) {
                if (!(fe68 >= 0.0f))
                    BR_DLHOOK(*(const uint32_t *)((const unsigned char *)BrG_6C3308 + 0x90));
                else
                    BR_DLHOOK(*(const uint32_t *)((const unsigned char *)BrG_6C3308 + 0x88));
            } else {
                if (!(fe68 >= 0.0f))
                    BR_DLHOOK(*(const uint32_t *)((const unsigned char *)BrG_6C3308 + 0x8C));
                else
                    BR_DLHOOK(*(const uint32_t *)((const unsigned char *)BrG_6C3308 + 0x84));
            }
#undef BR_DLHOOK
        }
    }

    /* 0xB1F8 -- glass prep: setup tiles + dot test. */
    {
        float  dot;
        put(0xBA001001u, 0x00010000u);
        put(0xBB000001u, 0xFFFFFFFFu);
        put(0xF5100000u, 0x07000000u);
        put(0xF50001F0u, 0x06000000u);
        put(0xF5000100u, 0x05000000u);

        BrVec3Sub(&glassTmp,
            (const BrVec3 *)(car + BR_CAR_OFF_POS),
            (const BrVec3 *)((const unsigned char *)BrG_6C6490 + 0x30));
        dot = BrVec3Dot(
            (const BrVec3 *)(car + BR_CAR_OFF_ROW2), &glassTmp);

        if (dot > 0.0) {
            /* 0xB2CB -- glass pass. */
            BrGfxEmitTexCmd(6,
                *(const void *const *)((const unsigned char *)BrG_6C3308 + 0x8014));
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
                if (*(const uint32_t *)((const unsigned char *)BrG_6C3308 + 0x8030 +
                        (uint32_t)((lod + lod * 4) << 3)) != 0)
                    put(0x06000000u, *(const uint32_t *)((const unsigned char *)BrG_6C3308 + 0x8030 +
                        (uint32_t)((lod + lod * 4) << 3)));
            }
        }
    }

    /* 0xB4AA -- detail pass. */
    BrGfxEmitTexCmd(3,
        *(const void *const *)((const unsigned char *)BrG_6C3308 + 0x8014));
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
        lodOff = (uint32_t)((lod + lod * 4) << 3);
        if (*(const uint32_t *)((const unsigned char *)BrG_6C3308 + 0x8024 + lodOff) != 0)
            put(0x06000000u, *(const uint32_t *)((const unsigned char *)BrG_6C3308 + 0x8024 + lodOff));
    }

    /* 0xB685-0xB925 -- reflection pass. */
    if (g_BrDrawReflectEnable != 0 &&
        g_BrDrawWheelAlt == 0 &&
        BrG_6C6624 == 0 &&
        !(flag290C != 0 && BrG_6C661C == 0) &&
        !(BrG_6C661C != 0 && (void *)car == BrG_6C2CF8) &&
        g_BrDrawSuppress == 0 &&
        *(const int32_t *)(car + BR_CAR_OFF_I29B4) == 0) {
        put(0xE7000000u, 0);
        put(0xBA001402u, 0);
        put(0xB7000000u, 0x00040000u);
        put(0xBB000001u, 0x0F800F80u);
        put(0xBA000C02u, BrG_6C0258);
        put(0xFA000000u, 0xFFFFCCFFu);
        BrRdpSetCombineLERP(put_slot(),
            TK_ZERO,     TK_ZERO, TK_ZERO,     TK_TEXEL1_A,
            TK_ZERO,     TK_ZERO, TK_ZERO,     TK_TEXEL0,
            TK_ZERO,     TK_ZERO, TK_ZERO,     TK_TEXEL1_A,
            TK_ZERO,     TK_ZERO, TK_ZERO,     TK_TEXEL0);
        put(0xB900031Du, 4);
        put(0xE8000000u, 0);
        put(0xBA000E02u, 0);
        /* Inline the ternary into the put arg so the tex select evaluates
         * AFTER the put macro's slot-pointer bump (orig alloc-first order). */
        put(((BrG_6C661C != 0 ? g_BrDrawReflectTexA : g_BrDrawReflectTexB)
             & 0x00FFFFFFu) | 0xDC000000u, 1);
        put(0xBA000602u, 0xC0u);
        /* Orig RE-READS both fields for the second word (`mov edx,[eax];
         * mov eax,[eax+4]` at 0x1798), so they are NOT named locals here --
         * naming them spills the pair and the assembled word to slots. */
        put(((((uint32_t)pSkyAng->s0 & 0xFFFu) | 0xFFFF2000u) << 12) |
            ((uint32_t)pSkyAng->t0 & 0xFFFu),
            (((((uint32_t)pSkyAng->s0 + 0xFCu) << 12) & 0x00FFF000u) |
             (((uint32_t)pSkyAng->t0 + 0xFCu) & 0xFFFu)));
        {
            if (*(const uint32_t *)(
                    (const unsigned char *)BrG_6C3308 + lodOff + 0x803C) != 0)
                put(0x06000000u, *(const uint32_t *)(
                    (const unsigned char *)BrG_6C3308 + lodOff + 0x803C));
        }
        put(0xBA000602u, BrG_6C0688);
    }

    /* 0xB925 -- post-detail setup block. */
    put(0xE7000000u, 0);
    put(0xBA001402u, 0x00100000u);

    /* Orig is the branchless neg/sbb ternary, not an if/or. */
    put(0xB7000000u,
        (g_BrDrawReflectFlag != 0 ? 0x00080000u : 0u) | 0x00040000u);

    put(0xBB000001u, 0x08001000u);
    put(0xBA000C02u, BrG_6C0258);

    /* Argument ORDER read back out of the original's push stream, not
     * guessed: each row is (A - B) * C + D, so this one really is a LERP
     * between shade and texel by 0x3F4, twice.  The old spelling had the
     * same tokens in the wrong slots and passed TK_ZERO where the original
     * passes TK_TEXEL0 and 0x3F4 -- invisible to divergence.py, which
     * wildcards imm32. */
    BrRdpSetCombineLERP(put_slot(),
        TK_TEXEL0,   TK_SHADE, 0x3F4,   TK_SHADE,
        TK_ZERO,     TK_ZERO,  TK_ZERO, TK_TEXEL0,
        TK_TEXEL0,   TK_SHADE, 0x3F4,   TK_SHADE,
        TK_ZERO,     TK_ZERO,  TK_ZERO, TK_TEXEL0);

    /* 0xBA18 -- 3-arm FB colour (G_SETENVCOLOR).  The put() is INSIDE each
     * arm (three full copies of the Horner pack in the bytes); the inner
     * gate is flag290C ([esp+0x18] in the original), NOT specMem. */
    if (BrG_6C6618 != 0) {
        if (flag290C != 0)
            put(0xFB000000u,
                ((((uint32_t)(uint8_t)BrG_6C0260 << 8
                 | (uint8_t)BrG_6C1614) << 8
                 | (uint8_t)BrG_6C0200) << 8)
                 | ((((uint32_t)g_BrDrawByte78 >> 3) - 0x21) & 0xFFu));
        else
            put(0xFB000000u,
                ((((uint32_t)(uint8_t)BrG_6C0260 << 8
                 | (uint8_t)BrG_6C1614) << 8
                 | (uint8_t)BrG_6C0200) << 8)
                 | ((((uint32_t)g_BrDrawByte78 >> 1) + 0x7Fu) & 0xFFu));
    } else {
        put(0xFB000000u,
            ((((uint32_t)(uint8_t)BrG_6C0260 << 8
             | (uint8_t)BrG_6C1614) << 8
             | (uint8_t)BrG_6C0200) << 8) | 0xFFu);
    }

    put(0xB900031Du, 0);
    put(0xB900031Du, g_BrDrawModeBase | g_BrDrawRenderMode);
    put(0xE8000000u, 0);
    put(0xBA000E02u, 0);

    /* 0xBB70 -- DC texture: indexed lookup via car+0x2714 and g_BrDrawRefIndex. */
    {
        int32_t idx2714 = *(const int32_t *)(car + BR_CAR_OFF_I2714);
        /* Orig: movsx ecx, byte[idx2714 + refIndex*2 + 0x100A5C78] then
         * [tblIdx*4 + 0x100A5C58].  Both symbols are the array DATA at a
         * fixed link address (folded as a displacement), not pointer vars,
         * so &g_-cast to the pinned base.  The *2 scales refIndex. */
#ifdef BR_MATCHING_BUILD
        int8_t tblIdx = ((const int8_t *)&g_BrDrawRefTbl)[idx2714 + g_BrDrawRefIndex * 2];
        uint32_t texVal = ((const uint32_t *)&g_BrDrawRefColors)[tblIdx];
#else
        int8_t tblIdx = g_BrDrawRefTbl[idx2714 + g_BrDrawRefIndex * 2];
        uint32_t texVal = g_BrDrawRefColors[tblIdx];
#endif
        put((texVal & 0x00FFFFFFu) | 0xDC000000u, 1);
    }

    put(0xBA000E02u, 0x00008000u);

    /* 0xBBCF -- F2 settile.  Orig: ftol(player+0x2718 * -20.3718), then
     * ecx = 0xFFFFFFDF - tile; lo = ecx+2; hi = ecx+0x7E. */
    {
        int32_t tile = (int32_t)(
            *(const float *)((const unsigned char *)BrG_6C2CF8 + 0x2718) *
            -20.3718318939209f);
        int32_t adj = -33 - tile;
        int32_t lo = adj + 2;
        int32_t hi = adj + 0x7E;
        uint32_t w0 = ((lo << 12) & 0x00FFF000u) | 0xF2000002u;
        uint32_t w1 = ((hi << 12) & 0x00FFF000u) | 0x000001FEu;
        put(w0, w1);
    }

    put(0xE7000000u, 0);
    put(0x03840010u, specMem);
    put(0x03820010u, specMem + 0x10u);

    /* 0xBC7B -- 2nd body DL at model + lodOff + 0x8028. */
    {
        if (*(const uint32_t *)((const unsigned char *)BrG_6C3308 + 0x8028 + lodOff) != 0)
            put(0x06000000u, *(const uint32_t *)((const unsigned char *)BrG_6C3308 + 0x8028 + lodOff));
    }

    /* 0xBCBF -- reflection DL at model + lodOff + 0x803C (conditional). */
    {
        if (*(const uint32_t *)((const unsigned char *)BrG_6C3308 + 0x803C + lodOff) != 0 &&
            (g_BrDrawSuppress != 0 ||
             *(const int32_t *)(car + BR_CAR_OFF_I29B4) != 0))
            put(0x06000000u, *(const uint32_t *)((const unsigned char *)BrG_6C3308 + 0x803C + lodOff));
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
#ifdef BR_MATCHING_BUILD
    if (car[BR_CAR_OFF_KIND] != 2)
        BrCarDrawWheels_raw(car);
#else
    if (car[BR_CAR_OFF_KIND] != 2)
        wheel_call(car);
#endif

    /* 0xBE14 -- final: sync, combiner, render mode. */
    put(0xE7000000u, 0);
    put(0xBA001402u, 0);

    BrRdpSetCombineLERP(put_slot(),
        TK_TEXEL0, TK_ZERO, TK_PRIMITIVE, TK_ZERO,
        TK_TEXEL0, TK_ZERO, TK_PRIMITIVE, TK_ZERO,
        TK_TEXEL0, TK_ZERO, TK_PRIMITIVE, TK_ZERO,
        TK_TEXEL0, TK_ZERO, TK_PRIMITIVE, TK_ZERO);

    put(0xB900031Du, 3);

    /* 0xBE98 -- model cost accumulation.  Orig adds into 0x106E86AC
     * directly from a reload of BrG_6C3308. */
#ifdef BR_MATCHING_BUILD
    g_6C161C += *(const int32_t *)((const unsigned char *)BrG_6C3308 + 0x8000);
#else
    BrS17GetState()->f6C161C += *(const int32_t *)((const unsigned char *)BrG_6C3308 + 0x8000);
#endif
}

/* BrDesktopSetup (0x10009C00) stays in ghidra_batch.c — context-sensitive codegen. */
