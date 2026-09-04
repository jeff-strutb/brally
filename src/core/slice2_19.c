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

/* Both display-list emitters below inline this in the original: take the
 * write cursor, advance it by 8 bytes, and fill the two words. */
static uint32_t *BrGfxTake2(void)
{
    uint32_t *p = g_BrGfxPtr;
    g_BrGfxPtr += 2;
    return p;
}

/* 0x10033E83 */
/* WHAT IT DOES: points the camera at what it is looking at and sets the lens,
 * then combines the two into the single transform everything in the world is
 * drawn through, and parks a copy of it where the renderer will find it.
 * Anything nearer than a fixed close distance, or further than the caller's
 * limit, is cut off. */
/* @implements 0x10033E83 d3d BrCamMatrixSetup */
/* @n64 0x8021B2F8 located */
#ifdef BR_MATCHING_BUILD
/* /Od: no locals at all -- the fovy chain is inline in the call (a named
 * local would cost a frame slot); pool alloc and matrix store direct. */
extern BrMat4 *BrSub_10069490(void);            /* glide 0x10062500 */
extern void BrGuMtxStore(const int pSrc[4][4], int pDst[4][4]);

void BrCamMatrixSetup(const BrCamBasis *pCam, float a2, float a3,
                      float a4, float a5)
{
    BrMat4LookAt(&g_BrViewMat,
                 pCam->eye.x, pCam->eye.y, pCam->eye.z,
                 pCam->eye.x + pCam->fwd.x,
                 pCam->eye.y + pCam->fwd.y,
                 pCam->eye.z + pCam->fwd.z,
                 pCam->up.x, pCam->up.y, pCam->up.z);

    g_BrCamFar  = a3;
    g_BrCamNear = 0.8f;   /* the literal 0x3F4CCCCD */

    /* ((a2 * K518) * (a5 / a4)) * K51C -- note a5/a4 here but a4/a5 as the
     * aspect. Both are in the original. */
    BrMat4Perspective7(&g_BrProjMat, &g_BrPerspNorm,
                       a2 * g_BrK08F518 * (a5 / a4) * g_BrK08F51C,
                       a4 / a5, g_BrCamNear, g_BrCamFar, 1.0f);

    BrMat4Mul(&g_BrViewMat, &g_BrProjMat, &g_BrCurMat);

    g_BrMtxSlot = BrSub_10069490();
    BrGuMtxStore((const int (*)[4])&g_BrCurMat, (int (*)[4])g_BrMtxSlot);
}
#else
void BrCamMatrixSetup(const BrCamBasis *pCam, float a2, float a3,
                      float a4, float a5)
{
    float fovy;

    BrMat4LookAt(&g_BrViewMat,
                 pCam->eye.x, pCam->eye.y, pCam->eye.z,
                 pCam->eye.x + pCam->fwd.x,
                 pCam->eye.y + pCam->fwd.y,
                 pCam->eye.z + pCam->fwd.z,
                 pCam->up.x, pCam->up.y, pCam->up.z);

    g_BrCamFar  = a3;
    g_BrCamNear = 0.8f;   /* the literal 0x3F4CCCCD, stored to 0x106C3360 */

    /* ((a2 * K518) * (a5 / a4)) * K51C -- note a5/a4 here but a4/a5 as the
     * aspect two lines down. Both are in the original. */
    fovy = a2 * g_BrK08F518;
    fovy = fovy * (a5 / a4);
    fovy = fovy * g_BrK08F51C;

    BrMat4Perspective7(&g_BrProjMat, &g_BrPerspNorm,
                       fovy, a4 / a5, g_BrCamNear, g_BrCamFar, 1.0f);

    BrMat4Mul(&g_BrViewMat, &g_BrProjMat, &g_BrCurMat);

    g_BrMtxSlot = BrPoolAlloc(g_BrPool);
    BrMat4Copy(&g_BrCurMat, (BrMat4 *)g_BrMtxSlot);   /* source first */
}
#endif

/* 0x10033F7E  Both parameters are dead; see the header. */
/* WHAT IT DOES: sets up a fixed camera looking straight at a flat scene at a
 * fixed distance -- what the menus and other flat screens are drawn through --
 * and issues the drawing commands that put that transform in force. The two
 * values it is passed are never looked at. */
/* @implements 0x10033F7E d3d BrCamMatrixSetupFixed */
/* @n64 0x8021B458 located */
#ifdef BR_MATCHING_BUILD
/* /Od TU: literal param self-assigns, the take-2 emit inlined per block
 * (own [ebp-N] slot each, globals re-read), the 0-arg pool alloc and the
 * matrix store called directly. Externs shared with BrCamMatrixSetup. */
void BrCamMatrixSetupFixed(float a1, float a2)
{
    a1 = a1;
    a2 = a2;

    BrMat4LookAt(&g_BrViewMat,
                 512.0f, 384.0f, 1000.0f,
                 512.0f, 384.0f,    0.0f,
                   0.0f,   1.0f,    0.0f);

    BrMat4Perspective7(&g_BrProjMatFixed, &g_BrPerspNorm,
                       45.0f, 1.3333334f, 10.0f, 2000.0f, 1.0f);

    BrMat4Mul(&g_BrViewMat, &g_BrProjMatFixed, &g_BrCurMat);

    {
        uint32_t *p_ = g_BrGfxPtr;
        g_BrGfxPtr += 2;
        p_[0] = 0xBC00000Eu;
        p_[1] = g_BrPerspNorm;
    }

    g_BrMtxSlot = BrSub_10069490();
    BrGuMtxStore((const int (*)[4])&g_BrCurMat, (int (*)[4])g_BrMtxSlot);

    {
        uint32_t *p_ = g_BrGfxPtr;
        g_BrGfxPtr += 2;
        p_[0] = 0x01030040u;
        p_[1] = (uint32_t)(uintptr_t)g_BrMtxSlot;
    }
}
#else
void BrCamMatrixSetupFixed(float a1, float a2)
{
    uint32_t *pCmd;

    (void)a1;
    (void)a2;

    BrMat4LookAt(&g_BrViewMat,
                 512.0f, 384.0f, 1000.0f,
                 512.0f, 384.0f,    0.0f,
                   0.0f,   1.0f,    0.0f);

    BrMat4Perspective7(&g_BrProjMatFixed, &g_BrPerspNorm,
                       45.0f, 1.3333334f, 10.0f, 2000.0f, 1.0f);

    BrMat4Mul(&g_BrViewMat, &g_BrProjMatFixed, &g_BrCurMat);

    pCmd = BrGfxTake2();
    pCmd[0] = 0xBC00000Eu;
    pCmd[1] = g_BrPerspNorm;      /* zero-extended from the u16 */

    g_BrMtxSlot = BrPoolAlloc(g_BrPool);
    BrMat4Copy(&g_BrCurMat, (BrMat4 *)g_BrMtxSlot);

    pCmd = BrGfxTake2();
    pCmd[0] = 0x01030040u;
    pCmd[1] = (uint32_t)(uintptr_t)g_BrMtxSlot;
}
#endif

/* 0x1003407D */
/* WHAT IT DOES: sets up flat drawing with no perspective at all, mapping a
 * rectangle of the given width and height onto the screen with the origin at
 * one corner, and issues the commands that put it in force. Depth is thrown
 * away entirely, so nothing drawn this way can be in front of or behind
 * anything else. */
/* @implements 0x1003407D d3d BrCamMatrixSetupOrtho */
#ifdef BR_MATCHING_BUILD
/* Same /Od TU and same four idioms as BrCamMatrixSetupFixed above -- literal
 * param self-assigns, the take-2 emit inlined per block with its own [ebp-N]
 * slot and the cursor re-read, the 0-arg pool alloc, and the matrix store
 * called directly. Every other function between 0x1002C0F3 and 0x1002E13B is
 * already byte-exact under /Od; this one was written in the /O2 shape, which
 * is the whole 19-instruction gap. */
void BrCamMatrixSetupOrtho(float w, float h)
{
    w = w;
    h = h;

    g_BrCurMat.m[0][0] = g_BrK08F514 / w;
    g_BrCurMat.m[0][1] = 0.0f;
    g_BrCurMat.m[0][2] = 0.0f;
    g_BrCurMat.m[0][3] = 0.0f;
    g_BrCurMat.m[1][0] = 0.0f;
    g_BrCurMat.m[1][1] = g_BrK08F514 / h;
    g_BrCurMat.m[1][2] = 0.0f;
    g_BrCurMat.m[1][3] = 0.0f;
    g_BrCurMat.m[2][0] = 0.0f;
    g_BrCurMat.m[2][1] = 0.0f;
    g_BrCurMat.m[2][2] = 0.0f;   /* explicit; z is discarded, not passed on */
    g_BrCurMat.m[2][3] = 0.0f;
    g_BrCurMat.m[3][0] = -1.0f;
    g_BrCurMat.m[3][1] = -1.0f;
    g_BrCurMat.m[3][2] = 0.0f;
    g_BrCurMat.m[3][3] = 1.0f;

    {
        uint32_t *p_ = g_BrGfxPtr;
        g_BrGfxPtr += 2;
        p_[0] = 0xBC00000Eu;
        p_[1] = g_BrPerspNorm;
    }

    g_BrMtxSlot = BrSub_10069490();
    BrGuMtxStore((const int (*)[4])&g_BrCurMat, (int (*)[4])g_BrMtxSlot);

    {
        uint32_t *p_ = g_BrGfxPtr;
        g_BrGfxPtr += 2;
        p_[0] = 0x01030040u;
        p_[1] = (uint32_t)(uintptr_t)g_BrMtxSlot;
    }
}
#else
void BrCamMatrixSetupOrtho(float w, float h)
{
    uint32_t *pCmd;

    g_BrCurMat.m[0][0] = g_BrK08F514 / w;
    g_BrCurMat.m[0][1] = 0.0f;
    g_BrCurMat.m[0][2] = 0.0f;
    g_BrCurMat.m[0][3] = 0.0f;
    g_BrCurMat.m[1][0] = 0.0f;
    g_BrCurMat.m[1][1] = g_BrK08F514 / h;
    g_BrCurMat.m[1][2] = 0.0f;
    g_BrCurMat.m[1][3] = 0.0f;
    g_BrCurMat.m[2][0] = 0.0f;
    g_BrCurMat.m[2][1] = 0.0f;
    g_BrCurMat.m[2][2] = 0.0f;   /* explicit; z is discarded, not passed on */
    g_BrCurMat.m[2][3] = 0.0f;
    g_BrCurMat.m[3][0] = -1.0f;
    g_BrCurMat.m[3][1] = -1.0f;
    g_BrCurMat.m[3][2] = 0.0f;
    g_BrCurMat.m[3][3] = 1.0f;

    pCmd = BrGfxTake2();
    pCmd[0] = 0xBC00000Eu;
    pCmd[1] = g_BrPerspNorm;

    g_BrMtxSlot = BrPoolAlloc(g_BrPool);
    BrMat4Copy(&g_BrCurMat, (BrMat4 *)g_BrMtxSlot);

    pCmd = BrGfxTake2();
    pCmd[0] = 0x01030040u;
    pCmd[1] = (uint32_t)(uintptr_t)g_BrMtxSlot;
}
#endif

/* ================================================================== */
/* 2. Display-list segment fixup                                      */
/* ================================================================== */

/* 0x10035060 */
/* WHAT IT DOES: corrects one address inside loaded data that still refers to
 * where the data used to live, shifting it to where it now sits -- and leaves
 * it alone if it points outside the block being moved. */
/* @implements 0x10035060 d3d BrDlRebaseWord */
/* @n64 0x8021D070 exact */
void BrDlRebaseWord(uint32_t *pWord, uint32_t lo, uint32_t hi, uint32_t base)
{
    if (*pWord >= lo && *pWord < hi)
        *pWord = *pWord - lo + base;
}

/* 0x10035089 */
/* WHAT IT DOES: walks a list of drawing commands just loaded from disk and
 * corrects the addresses inside it -- the ones naming where a model's corner
 * points and its textures live -- so that they point at where the data
 * actually is in memory. It stops at the command that ends the list. */
/* @implements 0x10035089 d3d BrDlRebase */
/* @n64 0x8021D098 located */
/* THREE things are load-bearing here, all visible only at /Od.
 *
 * (1) The null test is a WRAPPING if, not an early return: `if (p) { ... }`
 *     emits the single inverted `je end` the original has, while
 *     `if (!p) return;` emits `jne over / jmp end`.
 * (2) The step belongs in the for's THIRD clause.  `for (;; pDl += 2)` puts
 *     the increment at the TOP of the loop with a `jmp` over it on the first
 *     pass, which is 1002E744..1002E74C; writing `pDl += 2;` as the last
 *     statement of the body puts it at the bottom instead.
 * (3) It is a SWITCH on the EXPRESSION.  The original's compare chain runs
 *     4, 0xB8, 0xFD -- ASCENDING, with 4 and 0xFD sharing a target -- which
 *     an if/else chain cannot produce (it would test 4, 0xFD, 0xB8 in source
 *     order).  Switching on a NAMED local costs a second frame slot, because
 *     VC5 copies the value into its own switch temp; switching on the
 *     expression makes that temp the function's only local and restores the
 *     `push ecx` prologue. */
void BrDlRebase(uint32_t *pDl, uint32_t lo, uint32_t hi, uint32_t base)
{
    if (pDl != NULL) {
        for (;; pDl += 2) {
            switch ((pDl[0] >> 24) & 0xFFu) {
            case 0x04u:                          /* G_VTX     */
            case 0xFDu:                          /* G_SETTIMG */
                BrDlRebaseWord(&pDl[1], lo, hi, base);
                break;
            case 0xB8u:                          /* G_ENDDL   */
                return;
            }
        }
    }
}

/* 0x1003445A */
/* WHAT IT DOES: prepares one loaded model for drawing: it sets a global flag
 * from the current game mode unless the model asks to be left alone, then
 * scans the model's drawing commands and marks the model if that scan reports
 * a hit. What the flag and the mark ultimately control was not established
 * here, so the purpose beyond "per-model preparation" is unclear. */
/* @implements 0x1003445A d3d BrDlOwnerFixup */
void BrDlOwnerFixup(BrDlOwner *pOwner)
{
    /* A TERNARY, not an if/else.  At /Od the ternary's value lands in a
     * compiler temp at [ebp-8] and is then copied into `want` at [ebp-4],
     * which is where the original's `sub esp, 8` -- two dwords for one named
     * local -- comes from.  An if/else writes `want` directly and needs only
     * four bytes of frame. */
    int32_t want;

    g_Br6C666C = 0;

    want = (g_Br0B380C == 2 || g_Br0B380C == 8) ? 0 : 1;

    if ((pOwner->flags & 4u) == 0)
        g_Br6C666C = want;

    /* Compound `|=`, not a read-modify-write through a widening cast: the
     * original reads the halfword straight into cx and ors the low byte
     * (`mov cx,[eax+0x4c]; or cl,8`).  Spelling it as
     * `flags = (uint16_t)(flags | 8u)` adds the `xor edx,edx` zero-extension
     * the original does not have. */
    if (BrSub100341B3(pOwner->pDl, g_BrDlTableA))
        pOwner->flags |= 8u;
}

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
/* WHAT IT DOES: stores a colour as three separate red, green and blue
 * amounts, keeping only the bottom byte of each. */
/* @implements 0x10035CA0 d3d BrRgbSinkSet */
#ifdef BR_MATCHING_BUILD
/* Second argument is a struct so it is not register-eligible: __fastcall
 * then puts `this` in ecx and the three ints on the stack, i.e. thiscall. */
typedef struct { int r, g, b; } BrRgbSinkSetArgs;
void BR_THISCALL1 BrRgbSinkSet(BrRgbSink *pSink, BrRgbSinkSetArgs a)
{
    pSink->r = (unsigned char)a.r;
    pSink->g = (unsigned char)a.g;
    pSink->b = (unsigned char)a.b;
}
#else
void BrRgbSinkSet(BrRgbSink *pSink, int r, int g, int b)
{
    pSink->r = (unsigned char)r;
    pSink->g = (unsigned char)g;
    pSink->b = (unsigned char)b;
}
#endif

/* 0x100350EE */
/* WHAT IT DOES: repaints a car by writing the chosen colour into the twelve
 * body panels of its model and re-submitting them for drawing, then fixes up
 * the last panel's drawing settings differently depending on which extra
 * pieces the car has -- shadows and reflections, judging by there being four
 * separate variants. The colour is written into two slots of each panel while
 * the see-through bit is taken from a third, which looks like an indexing
 * slip in the original and is preserved. */
/* @implements 0x100350EE d3d BrCarGfxSetColour */
void BrCarGfxSetColour(BrCarGfx *pCar, int r, int g, int b)
{
    /* FIVE locals, `sub esp,0x14`. The tail's word pointer is its OWN
     * variable in the original -- ebp-4, and the most-used slot in the
     * function -- not the loop's `pw` reused. Sharing one costs a slot and
     * shifts every displacement in the function. */
    uint16_t  *pwTail;
    int32_t   i;
    BrGfxSlot *pSlot;
    uint16_t  *pw;

    if (g_BrCarCount == 0)
        return;

    for (i = 0; i < 12; i++) {
        uint16_t v;

        pSlot = &pCar->pSlots[pCar->aSlotIdx[i]];
        pw    = pSlot->pWords;
        /* Nested, not two `continue`s: the original's tests are two near
         * `je`/`jne` straight to the loop increment (0x1002E7FB and
         * 0x1002E810), and the early-exit spelling emits a short branch over
         * a jump instead. */
        if (pw != NULL && ((pSlot->f20 >> 24) & 0xFu) == 1u) {
            /* GOTCHA: the alpha bit is taken from pw[i], the results land in
             * pw[0] and pw[1]. Faithful.
             *
             * The byte swap is INLINE here. BrSwapHalf is a real call at /Od
             * and the original has none; the `and 0xffff` before each shift
             * is the uint16_t read, and the `sar` is the promotion to int. */
            v = (uint16_t)((pw[i] & 1u)
                           | ((uint32_t)r << 11)
                           | ((uint32_t)g << 6)
                           | ((uint32_t)b << 1));
            pw[0] = (uint16_t)(((v << 8) & 0xFF00) | ((v >> 8) & 0xFF));

            v = (uint16_t)((pw[i] & 1u)
                           | (((uint32_t)r & 0x1Eu) << 10)
                           | (((uint32_t)g & 0x1Eu) << 5)
                           | ((uint32_t)b & 0x1Eu));
            pw[1] = (uint16_t)(((v << 8) & 0xFF00) | ((v >> 8) & 0xFF));
        }
    }

    for (i = 0; i < pCar->cDl; i++)
        g_BrGfxSubmit(pCar->aDl[i]);

    pwTail = pCar->pSlots[pCar->aSlotIdx[11]].pWords;
    /* Wrapped, not two early returns: the original's tests are near `je` and
     * `jne` straight to the function's own `mov esp,ebp` (0x1002EAFF), and
     * the return spelling emits a short branch over a jump instead. */
    if (pwTail != NULL && g_Br0AC300 == 0) {
        if (pCar->aDlExtra[0] != 0) {
            if (g_Br6C661C != 0 || g_Br6C6624 != 0) {
                pwTail[15] = 0x0070u;   /* +0x1E */
                pwTail[10] = 0x8290u;   /* +0x14 */
            } else {
                pwTail[15] = 0x0190u;
                pwTail[10] = 0x01A0u;
            }
            pwTail[14] = 0x0190u;       /* +0x1C */
            pwTail[13] = pwTail[15];        /* +0x1A <- +0x1E */
            pwTail[9]  = 0x01A0u;       /* +0x12 */
            pwTail[8]  = pwTail[10];        /* +0x10 <- +0x14 */
            pwTail[12] = 0x8179u;       /* +0x18 */
            pwTail[7]  = 0x4192u;       /* +0x0E */
            pwTail[11] = 0x6BADu;       /* +0x16 */
            pwTail[6]  = 0x31C6u;       /* +0x0C */
            g_BrGfxSubmit(pCar->aDlExtra[0]);
        }

        if (pCar->aDlExtra[1] != 0) {
            pwTail[14] = 0x00C0u;
            pwTail[13] = pwTail[14];        /* +0x1A <- +0x1C, unlike block 1 */
            pwTail[9]  = 0x04F9u;
            pwTail[8]  = pwTail[9];         /* +0x10 <- +0x12, unlike block 1 */
            pwTail[11] = 0x6BADu;
            pwTail[6]  = 0x31C6u;
            g_BrGfxSubmit(pCar->aDlExtra[1]);
        }

        if (pCar->aDlExtra[2] != 0) {
            pwTail[14] = 0x0190u;
            pwTail[13] = pwTail[15];        /* +0x1A <- +0x1E */
            pwTail[9]  = 0x01A0u;
            pwTail[8]  = pwTail[10];        /* +0x10 <- +0x14 */
            pwTail[11] = 0x38E7u;
            pwTail[6]  = 0xFEFFu;
            g_BrGfxSubmit(pCar->aDlExtra[2]);
        }

        if (pCar->aDlExtra[3] != 0) {
            pwTail[14] = 0x00C0u;
            pwTail[13] = pwTail[14];
            pwTail[9]  = 0x04F9u;
            pwTail[8]  = pwTail[9];
            pwTail[11] = 0x38E7u;
            pwTail[6]  = 0xFEFFu;
        g_BrGfxSubmit(pCar->aDlExtra[3]);
        }
    }
}

/* 0x10035452 */
/* WHAT IT DOES: reads a car's current paint colour back out of its model and
 * expands it to full red, green and blue values, which is how the menus show
 * the player what colour the car is. The colour was stored more coarsely than
 * it was chosen, so what comes back is close to but not exactly what went
 * in. */
/* @implements 0x10035452 d3d BrCarGfxReadColour */
/* @n64 0x8021D2A0 located */
#ifdef BR_MATCHING_BUILD
/* True __thiscall with THREE stack args and no edx setup. That IS reachable:
 * declare every stack argument as a ONE-MEMBER STRUCT, which is never
 * register-eligible, so ecx takes `this`, edx is left alone and no dummy has
 * to be materialised. (The `int unused_edx` spelling used here before cost an
 * `xor edx,edx` at the call and pushed the guard's `jne` from short to near.)
 * See docs/VC5-IDIOMS.md, "CALLING one is ALSO reachable".
 *
 * Everything else is /Od-literal: pw[0] is RE-READ for every term (no `c`
 * local), and the locals are declared in the original's home order
 * (pSlot, b, r, g, pw -> -4,-8,-0xc,-0x10,-0x14). */
typedef struct BrRgbArg { int v; } BrRgbArg;
extern void __fastcall BrRgbSinkSet3(BrRgbSink *pSink,
                                     BrRgbArg r, BrRgbArg g, BrRgbArg b);

void BrCarGfxReadColour(BrRgbSink *pSink, const BrCarGfx *pCar)
{
    /* /Od homes locals by an internal NAME hash, not declaration order --
     * these single-letter names (a=slot, y=r, z=g, b=b, pw) are the set
     * that reproduces the original's frame layout
     * (slot=-4, b=-8, r=-0xc, g=-0x10, pw=-0x14); probed empirically. */
    const BrGfxSlot *a;
    BrRgbArg b, y, z;          /* the three struct args ARE the three locals */
    const uint16_t  *pw;

    a  = &pCar->pSlots[pCar->aSlotIdx[2]];
    pw = a->pWords;

    /* Nested ifs, no early returns: /Od emits ONE je-to-epilogue per
     * guard; `if (...) return;` costs a jne/jmp pair. */
    if (pw != NULL) {
        if (((a->f20 >> 24) & 0xFu) == 1u) {
            y.v = ((pw[0] >> 8) & 0xF8) | ((pw[0] >> 13) & 7);
            z.v = ((pw[0] >> 3) & 0xF8) | ((pw[0] >>  8) & 7);
            b.v = ((pw[0] << 2) & 0xF8) | ((pw[0] >>  3) & 7);
            BrRgbSinkSet3(pSink, y, z, b);
        }
    }
}
#else
void BrCarGfxReadColour(BrRgbSink *pSink, const BrCarGfx *pCar)
{
    const BrGfxSlot *pSlot = &pCar->pSlots[pCar->aSlotIdx[2]];
    const uint16_t  *pw    = pSlot->pWords;
    int c, r, g, b;

    if (pw == NULL)
        return;
    if (((pSlot->f20 >> 24) & 0xFu) != 1u)
        return;

    /* Read natively -- see the GOTCHA in the header. */
    c = (int)pw[0];
    r = ((c >> 8) & 0xF8) | ((c >> 13) & 7);
    g = ((c >> 3) & 0xF8) | ((c >>  8) & 7);
    b = ((c << 2) & 0xF8) | ((c >>  3) & 7);

    BrRgbSinkSet(pSink, r, g, b);
}
#endif

/* ================================================================== */
/* 4. Keyframe vertex animation                                       */
/* ================================================================== */

/* 0x10035585 */
/* WHAT IT DOES: sets how every animation in a set behaves -- play once, loop,
 * or run back and forth -- by turning the relevant switches on and off across
 * all of them at once. The three wrappers just below are the three settings a
 * caller actually asks for. */
/* @implements 0x10035585 d3d BrAnimFlagsApply */
void BrAnimFlagsApply(BrAnimSet *pSet, uint16_t orBits, uint32_t clearBits)
{
    int32_t i, n;
    BrAnimTrack *pT;

    clearBits = ~clearBits;          /* the original's `not eax`, 32-bit,
                                      * in the arg slot */

    /* Nested if (single je-to-epilogue), compound |=/&= (word ops end to
     * end: `or ax, word [ebp+0xc]` / `and ax, word [ebp+0x10]` -- the
     * value-cast spellings widen through eax with masks). */
    if (pSet->pList != NULL) {
        n = pSet->pList->n;
        for (i = 0; i < n; i++) {
            pT = pSet->pList->a[i];
            pT->flags |= orBits;
            pT->flags &= (uint16_t)clearBits;
        }
    }
}

/* The three playback modes a caller actually asks for. Each is one call to
 * BrAnimFlagsApply above with a fixed (set, clear) pair over bits 0 and 1:
 * bit 0 = repeat, bit 1 = reverse on the way back. */

/* WHAT IT DOES: play every animation in the set through ONCE and stop at the
 * end. Clears both the repeat and the bounce-back bits. */
/* @implements 0x1002ECAC glide BrAnimSetOnce */
/* @n64 0x8021D7E0 exact */
void BrAnimSetOnce(BrAnimSet *pSet)     { BrAnimFlagsApply(pSet, 0, 3); }

/* WHAT IT DOES: play every animation in the set on repeat, restarting from the
 * beginning each time round. Sets repeat, clears bounce-back. */
/* @implements 0x1002ECC1 glide BrAnimSetLoop */
/* @n64 0x8021D804 exact */
void BrAnimSetLoop(BrAnimSet *pSet)     { BrAnimFlagsApply(pSet, 1, 2); }

/* WHAT IT DOES: play every animation in the set back and forth for ever --
 * forwards to the end, then backwards to the start. Sets both bits. */
/* @implements 0x1002ECD6 glide BrAnimSetPingPong */
/* @n64 0x8021D828 exact */
void BrAnimSetPingPong(BrAnimSet *pSet) { BrAnimFlagsApply(pSet, 3, 0); }

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
/* WHAT IT DOES: loads a model from disk and makes it ready to draw -- reads
 * the file in, tells the address fixer where it landed, and runs the
 * byte-order and address correction over it. */
/* @implements 0x10036BD0 d3d BrModelLoad */
/* TWO arguments, not three, and the first callee is a thiscall.  The original
 * reads [esp+4] and [esp+8] only; the `pMgr` parameter is really the constant
 * 0x10AC0810 loaded into ecx (`mov ecx, 0x10ac0810`), so the loader is a
 * thiscall member on a fixed object.  Its two stack arguments are spelled as
 * one-pointer STRUCTS so neither can claim edx -- the convention slice1_09.c
 * already uses -- which is what makes a multi-argument thiscall reachable
 * from a CALL site at all.
 *
 * BrSegSetBases likewise takes two arguments here, not three: the original
 * pushes 0 and the loaded block and nothing else.  br_seg.c's matching body
 * already records that its third parameter is the port's own pMap slot, so
 * this call site simply declares the two-argument shape. */
#ifdef BR_MATCHING_BUILD
void *BrModelLoad(void *a1, void *a2)
{
    BrModelLoadArg x, y;
    void *p;

    x.p = a2;
    y.p = a1;
    p = BrSub100088B0(&g_brModelMgr, x, y);

    BrSegSetBases(0, (uint32_t)(uintptr_t)p);
    BrModelSwap(p);
    return p;
}
#else
void *BrModelLoad(void *pMgr, void *a1, void *a2)
{
    void *p;

    /* GOTCHA: a2 is pushed last, so it is the callee's FIRST argument. */
    p = BrSub100088B0(pMgr, a2, a1);

    BrSegSetBases(g_BrSegMap, 0, (uint32_t)(uintptr_t)p);
    BrModelSwap(p);
    return p;
}
#endif

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
/* WHAT IT DOES: always answers "yes"; the accepting counterpart of the
 * above. What it is installed as was not established. */
/* @implements 0x1003557B d3d BrRet1_1003557B */
int BrRet1_1003557B(void) { return 1; }
/* 0x10035B87 */
/* WHAT IT DOES: a second, separate routine that also always answers "yes".
 * Two identical bodies at different addresses, so callers of one are not
 * callers of the other; what either is installed as was not established. */
/* @d3donly 0x10035B87 BrRet1_10035B87 -- glide twin 0x1002EC2C COMDAT-folded onto BrRet1_1003557B */
int BrRet1_10035B87(void) { return 1; }


/* 0x10035BA7  The parameter is never read. */
/* WHAT IT DOES: writes out whatever message was last handed to the routine
 * below. It ignores the argument it is given and reads the stored one
 * instead. */
/* @implements 0x10035BA7 d3d BrLogEmit */
void BrLogEmit(void *ignored)
{
    (void)ignored;
    BrLogPrint(g_BrLogArg);
}

/* 0x10035BBA */
/* WHAT IT DOES: records a message and writes it out at once. Worth knowing
 * because elsewhere in the tree this same address is reached under the name
 * "BrFatal" -- it is not fatal, it only logs. */
/* @implements 0x10035BBA d3d BrLogSet */
/* @n64 0x8021E1F4 located */
void BrLogSet(void *p)
{
    g_BrLogArg = p;
    BrLogEmit(NULL);
}

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

/* WHAT IT DOES: return 0. */
/* @implements 0x1002E70A glide BrRet0_1002E70A */

int BrRet0_1002E70A(void)

{
  return 0;
}

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


void FUN_100746b4(void *d, void *s, unsigned n);

/* WHAT IT DOES: copy a rectangular block of texture rows from source to
 * destination, walking row by row with the caller's stride. The inner loop
 * of the texture uploader. */
/* @implements 0x1002E5B9 glide FUN_1002e5b9 */
/* auto-filed from ghidra --refine; transforms: as-is */

int FUN_1002e5b9(int param_1,int param_2,int param_3,int param_4)

{
  /* /Od: one 0x20 struct, fields in address order (ebp-0x20 .. ebp-4). */
  struct {
    int end;
    int n;
    int row;
    int len;
    int src;
    int dest;
    int k3;
    int sbyte;
  } s;
  
  s.end = 0;
  s.k3 = 3;
  s.src = 0;
  s.dest = 0;
  param_2 = param_2;
  for (s.row = 0; s.row < param_4; s.row = s.row + 1) {
    s.dest = 0;
    FUN_100746b4(&s.len,(void *)(param_3 + s.src),4);
    s.src = s.src + 4;
    s.end = s.src + s.len;
    while (s.src < s.end) {
      s.sbyte = (int)*(char *)(param_3 + s.src);
      s.src = s.src + 1;
      if (s.sbyte < 0) {
        for (s.n = -s.sbyte; s.n != 0; s.n = s.n + -1) {
          *(char *)(param_1 + s.dest) = *(char *)(param_3 + s.src);
          s.src = s.src + 1;
          s.dest = s.dest + param_4;
        }
      }
      else {
        s.n = s.sbyte + s.k3;
        s.sbyte = (int)*(char *)(param_3 + s.src);
        s.src = s.src + 1;
        for (; s.n != 0; s.n = s.n + -1) {
          *(char *)(param_1 + s.dest) = (char)s.sbyte;
          s.dest = s.dest + param_4;
        }
      }
    }
    param_1 = param_1 + 1;
  }
  return s.dest;
}

#endif /* BR_MATCHING_BUILD */
