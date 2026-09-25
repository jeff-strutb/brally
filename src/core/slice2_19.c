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

/* 0x1003563A BrAnimUpdate now lives in src/core/scene/br_animupdate.c. */

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

/* 0x10035FC0 BrBitEdgeSplit now lives in src/core/controls/br_bitedge.c. */

/* 0x10036C00 BrModelSwap (and its byte-reversal helpers BrRev4/BrRev2/
 * BrRdBe32/BrLd32/BrLd16) now lives in src/core/gamedata/br_modelswap.c. */

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
