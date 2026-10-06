/* br_drawcar.c -- see br_drawcar.h.  The vehicle's display list, from
 * BRGlide.dll.
 *
 * !!!! 2026-09-05 -- THE BYTE-LANE WALL IS BROKEN AT THE SHARED colourB JOIN
 * (see the cbTop comment at the site).  State: msetdiff 13+5 -> 9+5,
 * instructions 8 short -> 4, bytes 31 short -> 16, REGNORM 5+13 -> 5+9,
 * frame intact.  Masked regions 24 -> 29, the documented artefact here.
 *   HOW IT WAS FOUND, and it is the reusable part: a WRITE/READ CENSUS of the
 *   original's byte slots.  Slots 0x31/0x32 are each written FOUR times and
 *   read THREE -- the writes at 0x3a6/0x3c0 (arm 2) have no read of their own
 *   because arm 2 `jmp 0x427` INTO the read, which arm 3 falls through to.
 *   That asymmetry is what identifies a shared join and says which values
 *   have to be live across the edge.  Census the slots before theorising.
 *   !! AND THE LEVER IS JOIN-ONLY -- MEASURED, do not re-run.  The identical
 *   dword-partial applied to the two STRAIGHT-LINE pack sites (arm 1's
 *   colourB, arm 3's colourA), separately AND together, is BYTE-IDENTICAL:
 *   with no edge to cross there is nothing for it to change and VC5 folds
 *   `(part | pack[0])` straight back into the lane form.
 *   !! ALSO RE-TESTED UNDER THE NEW ALLOCATION (staleness rule) and the old
 *   verdict HOLDS: arm 3's `topA` moved LAST, matching the original's load
 *   order (pack, pack, top at 0x3ca-0x3d6), leaves the multiset unchanged at
 *   9+5 and costs one raw row each side (41+45 -> 42+46).  topA stays first.
 *   WHAT IS LEFT: 6 MISSING + 1 EXTRA of byte-lane at those two straight-line
 *   sites (~300 recorded-dead compiles plus the four above), the float
 *   operand swap (4 rows, no N64 oracle -- below), and the pCam reload.
 *   !! 2026-09-09 re-measure under the table-aware gates and the new canon
 *   classes: gate 0 PASSES (marker words were prose, reworded), A1/A2/A4/A5
 *   PASS, the x87 commute quad is now CLASSIFIED, and A3 is down to 8
 *   unpaired rows -- 2 byte homes + 2 widens + 2 or-merges + the pCam
 *   reload vs 1 lane move.  ALL of them are this wall.  No new lever: the
 *   scalar re-test still stands (frame and rows unchanged since 2026-09-05),
 *   and a {store,and,or}-for-nothing classifier absorption would not be
 *   sound -- an unpaired or-merge is exactly what A3 exists to refuse.
 *   T3 is blocked on breaking this wall, not on effort.
 *
 * !! 2026-09-05 (parallel probe sweep) -- 0x1000A110's two residue defects
 * each took one more measured-dead lever.  Do NOT re-run:
 *   - THE FLOAT OPERAND SWAP (2nd light call arg pCarF[12] + eyeScale, orig
 *     flds the stack local, ours the struct field): the N64 commutative-order
 *     ORACLE is UNAVAILABLE here.  build/tgrally/n64/report.csv has BrCarDrawVehicle
 *     as status=MISS with an EMPTY n64_va -- no located Top Gear Rally twin
 *     (pairs.csv / probe.csv have zero drawcar hits), so IDO/MIPS cannot state
 *     which operand loads first.  Both C spellings that could flip it (summand
 *     swap; ptr[0] off its own pointer) are already byte-identical under VC5.
 *     No source truth reachable; leave the swap parked.
 *   - THE BYTE-LANE PACK as TWO SCALAR uint8_t locals (pack0/pack1 instead of
 *     pack[2]), assigned on both arms and read across the colourA/colourB join
 *     to reproduce 0x1001E380's live-range shape WITHOUT a second array: INERT.
 *     Frame HELD (FIRSTDIV +0x17, no collapse) but the scalars coalesced to a
 *     byte-equivalent allocation -- msetdiff 13+5 unchanged, the widening rows
 *     (and R,0xff / or R,R / mov byte [esp+S],B) all still MISSING.  So a
 *     multi-edge scalar spelling is not the lever either; the home needs the
 *     use-COUNT shape (0x1001E380 reads its locals 16x), which no faithful
 *     spelling of this function can add.  The wall is real.
 *
 * Read off the GLIDE build, which is this project's reference.  Both
 * functions here are classed `shared` in config/brally/shared.csv, so the D3D
 * twins (0x1000C6E0 and 0x1000CBE0) are the same code under other numbers.
 */
/* Header is (const void *, void *).  Original is a 4x4 int copy
 * (`mov ebp,[ecx+eax]` / `mov [eax],ebp`, not fld/fstp). */
#define BrGuMtxStore BrGuMtxStore_port
#include "br_drawcar.h"
#undef BrGuMtxStore
#include "slice1_05.h"   /* BrGfxWords, BrRdpSetCombineLERP, BrMat4Mul   */
#include "slice2_15.h"   /* g_4B16A0 / g_4B16AC scene accumulators       */
#include "slice2_17.h"   /* BrGfxEmitTexCmd, BrS17GetState               */
#include "slice2_18.h"   /* BrG_6C0680 cursor; BrFogFactorAtPoint; car globals */
#include "slice2_19.h"   /* g_BrMtxSlot current projection slot          */
/* Header is the port's (volume, x, y).  The original reads the span grid
 * as a global and takes only (x, y).  Hide the port prototype so this TU
 * can call the two-float form. */
#define BrSpanTestPoint BrSpanTestPoint_port
#include "slice2_21.h"   /* BrSpanVolume, BrSpanTestPoint                */
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
void BrGuMtxStore(const int pSrc[4][4], int pDst[4][4])
{
    int i, j;
    for (i = 0; i < 4; ++i)
        for (j = 0; j < 4; ++j)
            pDst[i][j] = pSrc[i][j];
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
/* The model record the original reaches through the global 0x106EA398 --
 * slice2_18's BrG_6C3308.  Re-read at every use, the way the bytes do
 * (0x10009C11, 0x10009F24, 0x10009F45, 0x10009F53, 0x10009F73). */
#define BR_WHEEL_MDL(off) \
    (*(const uint32_t *)((const unsigned char *)BrG_6C3308 + (off)))

/* WHAT IT DOES: draw a car's four wheels, each with its own matrix so it can
 * steer and spin independently of the body. Called once per car per frame,
 * right after the body. */
/* @implements 0x10009C10 glide BrCarDrawWheels */
void BrCarDrawWheels(const BrCarView *pCar, const BrModelView *pModel)
{
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

#define BR_CAR_SPAN(x, y) BrSpanTestPoint(*(int32_t *)&(x), *(int32_t *)&(y))

/* WHAT IT DOES: decide, once per car per frame, whether that car will be
 * drawn at all and in which pass -- solid, see-through, or not at all -- and
 * work out how much fog sits over it. A car is visible if its position (or,
 * in some modes, a point six units to its side) falls inside the visible
 * hull. The player's own car is never culled by that test; instead it drops
 * to the see-through pass when the active camera is one of its own two
 * mounts, which is what stops the bodywork filling the screen in the
 * bumper views. */
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
 * transcribed against tools/brally/x87emu.py, not hand-derived (see the golden
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
/* @t4-pass 0x1000BEB0 1 2026-09-07 probes 108 bytes 1488 insns 399 regions 8 rows 59 census yes  (tools/brally/crank.py) */
/* @t4-pass 0x1000BEB0 2 2026-09-07 probes 108 bytes 1488 insns 399 regions 8 rows 59 census yes  (tools/brally/crank.py) */
/* @implements 0x1000BEB0 glide BrCarDrawBody */
void BrCarDrawBody(void *pCar)
{
    unsigned char       *car = (unsigned char *)pCar;

    /* 0x1000BEB0 -- nothing draws unless one of the two mode flags is set. */
    if (BrG_6C661C == 0 && BrG_6C6624 == 0) {
        return;
    }
    /* 0x1000BED0 -- only cars the visibility pass marked for the opaque pass. */
    if (g_BrCarVisOpaque[*(const int32_t *)(car + BR_CAR_OFF_ICAR)] == 0) {
        return;
    }
    /* 0x1000BEE3 -- the player's own car, when its active camera object is the
     * record's own +0x27C4 slot, is left to the other passes. */
    if ((void *)car == BrG_6C2CF8 &&
        BrG_6C6490 == (void *)((unsigned char *)BrG_6C2CF8 + BR_CAR_OFF_CAMSLOT)) {
        return;
    }
    /* 0x1000BEFF -- class 2 is the translucent pass, not this one. */
    if (*(const unsigned char *)(car + BR_CAR_OFF_KIND) == 2) {
        return;
    }

    /* 0x1000BF0C -- publish the model to the scratch global the tail (and
     * 0x1000A110) read, then work from it. */
    BrG_6C3308 = *(void *const *)(car + BR_CAR_OFF_MODEL);

    /* 0x1000BF18 -- the two matrices: the car's pooled model matrix pushed as
     * the modelview, the shared projection slot loaded after it. */
    put(0x01060040u, g_BrCarMtxSlot[*(const int32_t *)(car + BR_CAR_OFF_ICAR)]);
    put(0x01030040u, (uint32_t)(uintptr_t)g_BrMtxSlot);

    /* 0x1000BF5F -- the four 16-byte blocks of the car's lighting matrix
     * (0x9E/0x98/0x9A/0x9C at +0/+0x10/+0x20/+0x30 of one pooled slot). */
    put(0x039E0010u, g_BrCarLightSlot[*(const int32_t *)(car + BR_CAR_OFF_ICAR)]);
    put(0x03980010u, g_BrCarLightSlot[*(const int32_t *)(car + BR_CAR_OFF_ICAR)] + 0x10u);
    put(0x039A0010u, g_BrCarLightSlot[*(const int32_t *)(car + BR_CAR_OFF_ICAR)] + 0x20u);
    put(0x039C0010u, g_BrCarLightSlot[*(const int32_t *)(car + BR_CAR_OFF_ICAR)] + 0x30u);

    /* 0x1000C004 -- the canned setup list, then the model's texture command. */
    put(0x06000000u, (uint32_t)(uintptr_t)&BrG_0AA838);
    BrGfxEmitTexCmd(5, *(const void *const *)((const unsigned char *)BrG_6C3308 +
                                               BR_MODEL_OFF_TEXRECS));

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
    if (*(const uint32_t *)((const unsigned char *)BrG_6C3308 +
                            BR_MODEL_OFF_BODYDL) != 0) {
        put(0x06000000u, *(const uint32_t *)((const unsigned char *)BrG_6C3308 +
                                             BR_MODEL_OFF_BODYDL));
    }
    put(0xE7000000u, 0);
    put(0xBA000602u, BrG_6C0688);

    /* 0x1000C1B5 -- headlight glare, non-player cars only. */
    if ((void *)car != BrG_6C2CF8) {
        const BrVec3 *pRow0     = (const BrVec3 *)(car + BR_CAR_OFF_MTX);
        const BrVec3 *pPos      = (const BrVec3 *)(car + BR_CAR_OFF_POS);
        BrVec3 dir, basis;
        float  len, dot1, val;

        /* dir = camPos - (pos + row0); its length is the car-to-camera
         * distance measured from a point one basis unit ahead. */
        BrVec3Add(&dir, pPos, pRow0);
        BrVec3Sub(&dir, (const BrVec3 *)((const unsigned char *)BrG_6C6490 + 0x30), &dir);
        len = BrVec3Length(&dir);

        if (len != 0.0f) {                       /* g_0771A8 == 0.0 */
            BrVec3DivBy(&dir, len);              /* dir -> unit direction */

            /* 0x1000C243 -- FRONT glare: the view opposes the camera basis. */
            if (BrVec3Dot(&dir, (const BrVec3 *)BrG_6C6490) < 0.0f) {
                BrVec3Scale(&basis, pRow0, 1.0f);
                BrVec3MulAddTo(&basis,
                    (const BrVec3 *)((const unsigned char *)pRow0 + 0x20), 0.0f);
                dot1 = BrVec3Dot(&dir, &basis);
                val  = -(BrVec3Dot(&dir, (const BrVec3 *)BrG_6C6490) * dot1);
                if (val > 0.95f) {               /* g_0771CC == 0.95 */
                    len = len * len;
                    g_4B16AC += ((val - 0.95f) * 750.0f) / len;
                }
            }

            /* 0x1000C2FA -- BACK glare: the view runs with the camera basis. */
            if (BrVec3Dot(&dir, (const BrVec3 *)BrG_6C6490) > 0.95f) {
                BrVec3Scale(&basis, pRow0, 1.0f);
                BrVec3MulAddTo(&basis,
                    (const BrVec3 *)((const unsigned char *)pRow0 + 0x20), 0.0f);
                dot1 = BrVec3Dot(&dir, &basis);
                val  = BrVec3Dot(&dir, (const BrVec3 *)BrG_6C6490) * dot1;
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
    put(0x03860010u, (uint32_t)(uintptr_t)&BrG_0AA868);
    put(0x03880010u, (uint32_t)(uintptr_t)&BrG_0AA860);
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

typedef struct {
    uint32_t _00;
    uint32_t dl04;          /* +0x8024 */
    uint32_t dl08;          /* +0x8028 */
    uint32_t _0c;
    uint32_t dl10;          /* +0x8030 */
    uint32_t _14;
    uint32_t dl18;          /* +0x8038 */
    uint32_t dl1c;          /* +0x803C */
    uint32_t _20[2];
} BrCarLodRec;
typedef struct {
    unsigned char _0000[0x8020];
    BrCarLodRec   lod[3];
} BrCarModel;
extern BrCarModel *DAT_106ea398;
extern uint8_t DAT_106e8610, DAT_106ea3ec, DAT_106e79f8, DAT_106e79f0, DAT_106ed64c, DAT_106b7c80;

/* ==================================================================== *
 * 0x1000A110 -- draw one vehicle: body, underside, glass, detail,
 * reflection and wheels.  7,577 bytes, byte-exact.
 *
 * Transcribed instruction by instruction from the original.  The points
 * where the obvious spelling compiles to something else:
 *   - The model record and the colour bytes are named by their Glide
 *     addresses.  The colour arms read six byte globals; spelled with the
 *     D3D-addressed names, one name stood for different addresses at
 *     different sites, and VC5's value numbering then ordered the loads and
 *     byte lanes differently from the original.
 *   - The two pack bytes are elements 1 and 2 of a four-byte array, so VC5
 *     homes both and reads them back widened; the array's dword is the
 *     original's frame slot that pLights later reuses.
 *   - Each colour arm writes its own colourB; VC5 cross-jumps the identical
 *     suffix of arms 2 and 3, and the top byte goes into place inside each
 *     arm because it sits in a different register in each.  Arm 1 loads the
 *     top byte first.
 *   - The sky-block eye is a BrVec3 (z never written): that is what puts
 *     eye.x/eye.y between the pack dword and dirTmp, and atOffset in the dead
 *     pCar argument slot.  The camera is read through BrG_6C6490 at each use.
 *     eyeScale is volatile: the original stores it and adds it from memory
 *     with the operands in that order.
 *   - The per-LOD display lists are fields of a 40-byte record array inside
 *     the model, reached through the typed model global, which puts the
 *     model in the base register of each address.
 * ==================================================================== */
/* WHAT IT DOES: draws one car.  Picks a level of detail from its distance,
 * builds the light colours (dimmed in the rain and for flagged cars), aims
 * the lights from the camera, then emits the body, underside, glass, detail
 * and reflection display lists and the wheels.  Called once per visible car
 * per frame. */
/* @implements 0x1000A110 glide BrCarDrawVehicle */
void BrCarDrawVehicle(void *pCar, int32_t lodBias)
{
    unsigned char *car = (unsigned char *)pCar;
    int32_t  lod, distNear, flag290C;
    float    dist;
    uint32_t colourA, colourB;
    uint8_t  pack[4];   /* the pack bytes are [1] and [2]; see the header */
    uint32_t specMem = 0;
    /* !! FUNCTION-SCOPE ON PURPOSE.  The SECOND specular MOVEMEM pair (0xBC3F)
     * points at the SECOND pool allocation, not the third -- read off the
     * original's slots: [esp+0x2c] takes 0x10062550's result (pSkyAng, read
     * once at 0x1772), [esp+0x30] the FIRST 0x100625A0 (read at 0x690 for the
     * light calls AND at 0x1b1c for this pair) and [esp+0x28] the SECOND
     * 0x100625A0 (read only at 0xc78, the FIRST pair).  Spelling both pairs
     * `specMem` emitted the wrong address in the display list AND made VC5
     * CSE the shared `specMem + 0x10` into a slot (`lea R,[R+0x10]`,
     * `mov [esp+S],R` ... `mov R,[esp+S]`) where the original recomputes it
     * destructively at each site (`add edx,0x10` at 0xd38 and 0x1b59). */
    BrLightPair *pLights;
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

    /* 0xA232 -- model setup.  Re-read the model global at every use. */
    DAT_106ea398 = *(BrCarModel *const *)(car + BR_CAR_OFF_MODEL);

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

    /* 0x10062500 cannot fail; store the pointer itself (re-read iCar). */
    pSlot = BrSub_10069490();
    g_BrCarMtxSlot[*(int32_t *)(car + BR_CAR_OFF_ICAR)] = (uint32_t)pSlot;
    BrGuMtxStore(&g_BrDrawWorld,
        (int (*)[4])g_BrCarMtxSlot[*(int32_t *)(car + BR_CAR_OFF_ICAR)]);

    BrMat4Mul(&g_BrDrawWorld, &g_BrDrawView, &g_BrDrawCombined);
    BrGuMtxHookNop(&g_BrDrawCombined);

    pSlot = BrSub_10069490();
    g_BrCarLightSlot[*(int32_t *)(car + BR_CAR_OFF_ICAR)] = (uint32_t)pSlot;
    BrGuMtxStore(&g_BrDrawCombined,
        (int (*)[4])g_BrCarLightSlot[*(int32_t *)(car + BR_CAR_OFF_ICAR)]);

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

    /* 0xA393 -- light colours, Horner-packed: rain (divided by distance),
     * flagged (dimmed to 4/5) or plain. */
    if (BrG_6C661C != 0) {
        float div = dist * 0.1f;
        uint8_t top;
        if (!(div >= 1.0f)) div = 1.0f;
        colourA = ((((uint32_t)(uint8_t)(int32_t)((float)(int32_t)DAT_106e8610 / div) << 8
                   | (uint8_t)(int32_t)((float)(int32_t)DAT_106ea3ec / div)) << 8
                   | ((uint32_t)(int32_t)((float)(int32_t)DAT_106e79f8 / div) & 0xFF)) << 8);
        top = DAT_106b7c80;
        pack[1] = DAT_106e79f0;
        pack[2] = DAT_106ed64c;
        colourB = ((((uint32_t)top << 8 | pack[1]) << 8) | pack[2]) << 8;
    } else if (flag290C != 0) {
        uint8_t top = (uint8_t)((DAT_106b7c80 * 4) / 5);
        colourA = 0;
        pack[1] = (uint8_t)((DAT_106e79f0 * 4) / 5);
        pack[2] = (uint8_t)((DAT_106ed64c * 4) / 5);
        colourB = ((((uint32_t)top << 8 | pack[1]) << 8) | pack[2]) << 8;
    } else {
        uint8_t top, topA;
        topA = DAT_106e8610;
        pack[1] = DAT_106ea3ec;
        pack[2] = DAT_106e79f8;
        colourA = ((((uint32_t)topA << 8 | pack[1]) << 8) | pack[2]) << 8;
        top = DAT_106b7c80;
        pack[1] = DAT_106e79f0;
        pack[2] = DAT_106ed64c;
        colourB = ((((uint32_t)top << 8 | pack[1]) << 8) | pack[2]) << 8;
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
        /* !! SOLVED 2026-09-03, and exactly as the old note predicted: "treat
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

    /* Integer field copy. */
    g_BrDrawDir1.x = g_BrDrawDir0.x;
    g_BrDrawDir1.y = g_BrDrawDir0.y;
    g_BrDrawDir1.z = g_BrDrawDir0.z;

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
     * Orig: 0x10062500 (discarded), 0x10062550 -> pSkyAng,
     * two 0x100625A0 -> pLights then specMem. */
    {
        float          atOffset;
        volatile float eyeScale;
        BrVec3         eye;
        const float   *pCarF = (const float *)car;

        (void)BrSub_10069490();
        pSkyAng  = (BrSkyAngles *)BrPool16Alloc();
        pLights  = (BrLightPair *)BrPool32Alloc();
        specMem  = (uint32_t)(uintptr_t)BrPool32Alloc();

        atOffset = 0.0f;
        eyeScale = 0.0f;

        /* x/y compared ONCE; z picks the arm (orig 0x61c-0x655). */
        if (((const float *)BrG_6C6490)[12] == pCarF[12] &&
            ((const float *)BrG_6C6490)[13] == pCarF[13]) {
            if (((const float *)BrG_6C6490)[14] == pCarF[14])
                atOffset = 1.0f;
            else
                eyeScale = 0.1f;
        }

        eye.x = ((const float *)BrG_6C6490)[0];
        eye.y = ((const float *)BrG_6C6490)[1];
        if (eye.x == 0.0f && eye.y == 0.0f)
            eye.x = 0.0001f;

        BrLightDirsFromLookAt(&g_BrDrawCombined, pLights,
            eye.x, eye.y, 0.0f,
            0.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f);

        /* !! The angles call writes its direction pair into the SECOND pool
         * block, not pLights: the original pushes [esp+0x28] (0x1000A7FF,
         * `mov edx,[esp+0x68]` after 16 pushes) where the look-at call above
         * pushed [esp+0x30].  Passing pLights here made the angles call
         * overwrite the look-at pair and left the second block unwritten --
         * found by the live oracle (tools/brally/t3live.py) on a real race frame. */
        BrLightDirsAndAngles(&g_BrDrawCombined, (BrLightPair *)specMem, pSkyAng,
            ((const float *)BrG_6C6490)[12], ((const float *)BrG_6C6490)[13], ((const float *)BrG_6C6490)[14],
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
        memcpy(&g_BrDrawLights[*(int32_t *)(car + BR_CAR_OFF_ICAR) * 24],
               (const void *)&BrG_0AA860, 24);
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
    if (car[BR_CAR_OFF_KIND] == 2)
        BrCarDrawWheels_raw(car);

    /* 0xACE3 -- four light MOVEMEMs (unconditional, +0x10/+0x20/+0x30). */
    put(0x039E0010u, g_BrCarLightSlot[*(int32_t *)(car + BR_CAR_OFF_ICAR)]);
    put(0x03980010u, g_BrCarLightSlot[*(int32_t *)(car + BR_CAR_OFF_ICAR)] + 0x10);
    put(0x039A0010u, g_BrCarLightSlot[*(int32_t *)(car + BR_CAR_OFF_ICAR)] + 0x20);
    put(0x039C0010u, g_BrCarLightSlot[*(int32_t *)(car + BR_CAR_OFF_ICAR)] + 0x30);

    /* 0xAD77 -- TLUT palette load.  Orig stores DL word1 as an address-of-symbol
     * immediate (mov [eax+4], OFFSET g_BrDrawTexBlob), not the pointer's runtime
     * value.  &g_BrDrawTexBlob reproduces that store form for the matching build;
     * the port keeps the value-read semantics. */
    put(0xFD100000u, (uint32_t)(uintptr_t)&g_BrDrawTexBlob);
    put(0xE8000000u, 0);
    put(0xF50001E0u, 0x07000000u);
    put(0xE6000000u, 0);
    put(0xF0000000u, 0x0703C000u);
    put(0xE7000000u, 0);

    /* 0xAE34 -- specular MOVEMEM (payload from the specMem block above). */
    put(0x03840010u, specMem);
    put(0x03820010u, specMem + 0x10);

    /* 0xAE72 -- underside pass (gated on suppress + i29B4). */

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
            if (DAT_106ea398->lod[lod].dl18 != 0)
                put(0x06000000u, DAT_106ea398->lod[lod].dl18);
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
        uint8_t iTex = *(const uint8_t *)((const unsigned char *)DAT_106ea398 + 0x811B);
        const unsigned char *pTexRecs =
            *(const unsigned char *const *)((const unsigned char *)DAT_106ea398 + 0x8014);
        /* Orig reads the 0x100ABAA0 mode-change flag DIRECTLY (cmp dword
         * [0x100abaa0],ebp); the port routed it through the BrBootGlobal_ABAA0
         * wrapper, which cannot inline across TUs at /O2.  g_AC300 is that flag. */
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
             * selected DL emits (docs/brally/VC5-IDIOMS.md).
             * Orig calls the hook UNCONDITIONALLY and reads dlBase (model+0x80)
             * at the call site, not hoisted -- the null-check was a port-safety
             * addition the original never had. */
#define BR_DLHOOK(sel) g_BrDrawModelDlHook( \
                *(const uint32_t *)((const unsigned char *)DAT_106ea398 + 0x80), (sel))
            if (auxFlags & 0xC0000u) {
                if (!(fe68 >= 0.0f))
                    BR_DLHOOK(*(const uint32_t *)((const unsigned char *)DAT_106ea398 + 0x90));
                else
                    BR_DLHOOK(*(const uint32_t *)((const unsigned char *)DAT_106ea398 + 0x88));
            } else {
                if (!(fe68 >= 0.0f))
                    BR_DLHOOK(*(const uint32_t *)((const unsigned char *)DAT_106ea398 + 0x8C));
                else
                    BR_DLHOOK(*(const uint32_t *)((const unsigned char *)DAT_106ea398 + 0x84));
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
                *(const void *const *)((const unsigned char *)DAT_106ea398 + 0x8014));
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
                if (DAT_106ea398->lod[lod].dl10 != 0)
                    put(0x06000000u, DAT_106ea398->lod[lod].dl10);
            }
        }
    }

    /* 0xB4AA -- detail pass. */
    BrGfxEmitTexCmd(3,
        *(const void *const *)((const unsigned char *)DAT_106ea398 + 0x8014));
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
        if (DAT_106ea398->lod[lod].dl04 != 0)
            put(0x06000000u, DAT_106ea398->lod[lod].dl04);
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
            if (DAT_106ea398->lod[lod].dl1c != 0)
                put(0x06000000u, DAT_106ea398->lod[lod].dl1c);
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

    /* 0xBB70 -- reflection colour: a two-column byte table picks the entry
     * for this car's flag and the reflection index, which selects the
     * colour word.  Read after the slot is taken, as the bytes do. */
    put((((const uint32_t *)&g_BrDrawRefColors)[((const int8_t *)&g_BrDrawRefTbl)[
            *(const int32_t *)(car + BR_CAR_OFF_I2714) + g_BrDrawRefIndex * 2]]
         & 0x00FFFFFFu) | 0xDC000000u, 1);

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
    /* 0xBC3F -- the SECOND specular MOVEMEM pair, and it is pLights, NOT
     * specMem; see the declaration comment. */
    put(0x03840010u, (uint32_t)(uintptr_t)pLights);
    put(0x03820010u, (uint32_t)(uintptr_t)pLights + 0x10u);

    /* 0xBC7B -- second body display list of this LOD. */
    {
        if (DAT_106ea398->lod[lod].dl08 != 0)
            put(0x06000000u, DAT_106ea398->lod[lod].dl08);
    }

    /* 0xBCBF -- reflection display list of this LOD (conditional). */
    {
        if ((g_BrDrawSuppress != 0 ||
             *(const int32_t *)(car + BR_CAR_OFF_I29B4) != 0) &&
            DAT_106ea398->lod[lod].dl1c != 0)
            put(0x06000000u, DAT_106ea398->lod[lod].dl1c);
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
    if (car[BR_CAR_OFF_KIND] != 2)
        BrCarDrawWheels_raw(car);

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
    g_6C161C += *(const int32_t *)((const unsigned char *)DAT_106ea398 + 0x8000);
}

/* 0x10009C00 BrDPlayBootInit is in net/br_dplay.c. */
