/* slice3_45.c -- decompiled from BRD3D.dll, packet 0x10076420-0x100791D0.
 *
 * See slice3_45.h for the per-function derivations and gotchas. This file
 * carries the DEVIATION notes and the constants read out of the DLL.
 *
 * FUNCTIONS SKIPPED, and exactly why:
 *
 *   0x10077200  Joystick "press anything" scan for the binding UI. It sets
 *               g_brB4E1D0 = 2 and points g_brB4E1D4 at &g_aBrB4DF30[2]
 *               (0x10B4E080 == 0x10B4DF30 + 2*0xA8). slice2_25.h owns that
 *               array behind two macros this packet cannot see, and naming
 *               0x10B4E080 separately would create a second name for an
 *               address inside it. Left out rather than guessed.
 *
 *   0x10077310  Mouse "press anything" scan. Reaches the mouse device through
 *               *(void **)(0x10AA2E80) + 0x50. slice3_39.h already declares
 *               0x10AA2E80 as `BrPointI *g_pBrAA2E80` -- an incompatible
 *               type. Redeclaring it would clash; declaring a second name for
 *               it would duplicate the storage, which is worse. NOTE for the
 *               integration: the function fills 0x10AA33C0..CC from the four
 *               mouse buttons, calls BrMenuSub1005FFF0 (0x1005FFF0), and then
 *               scans a DIFFERENT four dwords, 0x10AA33D0..DC, returning the
 *               index of the first non-zero or -1. The set and the scan are
 *               not the same array.
 *
 *   0x10078DB0  IDirectInput root teardown. Same 0x10AA2E80 problem.
 *               NOTE: unlike BrDiKeyboardShutdown and slice1_10.h's
 *               BrFfbShutdown, its counter has NO underflow clamp -- it is
 *               `dec; jne return`, so an extra call runs the teardown again
 *               at count -1, -2, ... every time it wraps back through 0.
 *
 *   0x100773F0  The 4143-byte per-frame input poll. It touches ~40 globals
 *               with no established meaning, calls eleven functions outside
 *               this packet, and reaches USER32!GetAsyncKeyState and the CRT
 *               sprintf. Skipped per CONTRACT.md rather than guessed.
 *               Four facts from it are load-bearing elsewhere and are worth
 *               recording:
 *                 - the keyboard/joystick/mouse double buffers are flipped
 *                   with `idx = (idx - 1) & 1` AFTER copying the old index to
 *                   the matching *Prev global, which is what makes
 *                   BrInputJustPressed's rising edge work;
 *                 - the mouse scaling is (accum << 7) / table[clamp(g,0,7)]
 *                   where table is at 0x100BD400, then clamped to [-0x80,
 *                   0x80] with the ACCUMULATOR rewritten to +-table[g] on
 *                   clamp -- the clamp feeds back;
 *                 - GetAsyncKeyState(0x46) and (0x50) toggle two debug
 *                   globals, and only after frame 15;
 *                 - the joystick buffer is DIJOYSTATE2 (0x110), matching the
 *                   DIDATAFORMAT at 0x1007C7A0.
 */
#include <math.h>
#include <string.h>

#include "br_match.h"
#ifdef BR_MATCHING_BUILD
/* Header is cdecl (this, x, y, z). Original is thiscall with ret 0xC. */
#define BrEntSetPos BrEntSetPos_hdr
#endif
#ifdef BR_MATCHING_BUILD
/* The entity setters are thiscall with three stack floats; hide the
 * port's cdecl prototypes so the twins can carry the fastcall shape. */
#define BrEntSetMatrix      BrEntSetMatrix_port
#define BrEntSetVel         BrEntSetVel_port
#define BrEntSetAngVel      BrEntSetAngVel_port
#define BrEntSetOrientation BrEntSetOrientation_port
#define BrEntSetHeading     BrEntSetHeading_port
#include "slice3_45.h"
#undef BrEntSetMatrix
#undef BrEntSetVel
#undef BrEntSetAngVel
#undef BrEntSetOrientation
#undef BrEntSetHeading
#else
#include "slice3_45.h"
#endif
#ifdef BR_MATCHING_BUILD
#undef BrEntSetPos
#endif

/* ====================================================================== */
/* Constants read out of orig/BRD3D.dll .rdata (do not re-derive)          */
/* ====================================================================== */

/* 0x1008FCA4 = 0xBFC90FDB. The float nearest -pi/2. BrEntSetHeading
 * SUBTRACTS it, i.e. adds pi/2. */
static const float kBrNegHalfPi = -1.5707963705062866f;

/* 0x1008FCA8 = 0x3F000000, exactly 0.5. The quaternion half-angle factor. */
static const float kBrHalf = 0.5f;

/* 0x3F060A92, an immediate in 0x10076B20. Bit-exact float(pi/6). */
static const float kBrSixthPi = 0.5235987901687622f;

/* 0x100907D0 and 0x10090780, the two effect GUIDs handed to CreateEffect
 * (byte order as it sits in the file). slice1_10.h identified them as
 * GUID_Spring {13541C27-8E33-11D0-9AD0-00A0C9A06E35} and GUID_Square
 * {13541C22-...}; the bytes below confirm it. */
static const unsigned char kBrGuidSpring[16] = {
    0x27, 0x1c, 0x54, 0x13, 0x33, 0x8e, 0xd0, 0x11,
    0x9a, 0xd0, 0x00, 0xa0, 0xc9, 0xa0, 0x6e, 0x35
};
static const unsigned char kBrGuidSquare[16] = {
    0x22, 0x1c, 0x54, 0x13, 0x33, 0x8e, 0xd0, 0x11,
    0x9a, 0xd0, 0x00, 0xa0, 0xc9, 0xa0, 0x6e, 0x35
};

/* 0x10090650 = {5944E682-C92E-11CF-BFC7-444553540000} = IID_IDirectInputDevice2A. */
static const unsigned char kBrIidDevice2A[16] = {
    0x82, 0xe6, 0x44, 0x59, 0x2e, 0xc9, 0xcf, 0x11,
    0xbf, 0xc7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00
};

/* 0x1007C7A0: DIDATAFORMAT { 0x18, 0x10, DIDF_ABSAXIS, 0x110, 164, rgodf }
 * -- c_dfDIJoystick2. Only its ADDRESS is used, so an opaque stand-in is
 * enough; the real table lives in the DLL's read-only data. */
static const uint32_t kBrDataFormatJoystick2[6] = {
    0x18u, 0x10u, 0x1u, 0x110u, 0xa4u, 0u
};

/* The literal strings the failure paths hand to OutputDebugStringA. */
static const char kBrErrCreateDevice[] = "Error: Failed to create device.\n";
static const char kBrErrDataFormat[]   = "Error: Failed to set game device data format.\n";
static const char kBrErrCoopLevel[]    = "Error: Failed to set cooperative level.\n";
static const char kBrErrInterface[]    = "Error: Failed to obtain interface.\n";
static const char kBrErrPropWord[]     = "Error: IDirectInputDevice::SetProperty(DIPH_WORD) FAILED\n";
static const char kBrErrPropRange[]    = "Error: IDirectInputDevice::SetProperty(DIPH_RANGE) FAILED\n";
static const char kBrErrProperty[]     = "Error: Failed to change device property.\n";

/* DEVIATION: the original calls KERNEL32!OutputDebugStringA. That is not
 * portable, so the sink is a function pointer defaulting to a no-op. Every
 * call site, and the order of the calls relative to the COM calls around
 * them, is preserved exactly. */
void (*g_pBrDbgPrint)(const char *pMsg) = NULL;

static void BrDbgPrint(const char *pMsg)
{
    if (g_pBrDbgPrint != NULL) {
        g_pBrDbgPrint(pMsg);
    }
}

/* ====================================================================== */
/* Globals this translation unit owns                                      */
/* ====================================================================== */

/* 0x100C12A0. Owned here, but under slice2_20's name for it -- see the ALIAS
 * note in slice3_45.h. 16 records of 89992 bytes, which is exactly the gap to
 * the next referenced global (0x10220B20). .bss in the original, so the zero
 * initialisation is the original's own. */
unsigned char g_ab0C12A0[BR45_CARGFX_STRIDE * BR45_CARGFX_COUNT] = { 0 };

BrInputState g_brInput;

BrFfb    g_brFfb;
BrDiObj *g_pBr18ABD70;
BrDiObj *g_pBr18ABDD0;
int32_t  g_br18ABDD8;
int32_t  g_br18ABDBC;

BrDiEffect    g_brDiEffSpring;
BrDiCondition g_brDiSpringCond[2];
int32_t       g_brDiSpringDir[2];
BrDiEffect    g_brDiEffSquare;
BrDiPeriodic  g_brDiSquarePeriod;

int32_t g_br18ABDF8;
int32_t g_br18ABD78;

uint32_t g_br680598;
uint32_t g_br68059C;
uint32_t g_br6805A0;

/* Initialised data, values read straight out of the DLL image at
 * 0x100BD424: 10 27 00 00 | d0 07 00 00 | 10 27 00 00 |
 *             ff ff ff ff 00 00 00 00 | 48 e8 01 00 */
int32_t g_br0BD424 = 10000;
int32_t g_br0BD428 = 2000;
int32_t g_br0BD42C = 10000;
int32_t g_br0BD430[2] = { -1, 0 };
int32_t g_br0BD438 = 125000;

/* ====================================================================== */
/* Small helpers                                                           */
/* ====================================================================== */

/* Native-endian dword load through memcpy: the original does `mov r,[p+n]`
 * on its own in-memory record, so this is a native read, not one of the
 * big-endian payloads CONTRACT.md warns about. memcpy rather than a cast
 * because the record is only byte-aligned in this port. */
static uint32_t BrLoad32(const unsigned char *p)
{
    uint32_t v;
    memcpy(&v, p, sizeof v);
    return v;
}

/* The compiler's magic-multiply for signed /10000 (0x68DB8BAD, sar 12, plus
 * the sign bit). Reproduced as a plain division: for every int32_t input the
 * two agree, because the original truncates toward zero exactly as C99 does.
 * The MULTIPLY that feeds it is done in uint32_t so its wrap matches. */
static int32_t BrDiv10000(int32_t v)
{
    return v / 10000;
}

static int32_t BrMulWrap(int32_t a, int32_t b)
{
    return (int32_t)((uint32_t)a * (uint32_t)b);
}

static int32_t BrAddWrap(int32_t a, int32_t b)
{
    return (int32_t)((uint32_t)a + (uint32_t)b);
}

/* Cast helpers for the three COM interfaces. slice1_10.h's BrDiObj carries a
 * `const BrDiVtbl *`; these reinterpret it as the wider vtables declared in
 * slice3_45.h. See the note there -- BrDiVtbl is deliberately not redefined. */
static const BrDiRootVtbl *BrDiRoot(BrDiObj *p)
{
    return (const BrDiRootVtbl *)(const void *)p->pVtbl;
}
static const BrDiDevVtbl *BrDiDev(BrDiObj *p)
{
    return (const BrDiDevVtbl *)(const void *)p->pVtbl;
}
static const BrDiEffVtbl *BrDiEff(BrDiObj *p)
{
    return (const BrDiEffVtbl *)(const void *)p->pVtbl;
}

#ifdef BR_MATCHING_BUILD
typedef long (__stdcall *BrDiSetParamsFn)(BrDiObj *, const BrDiEffect *, uint32_t);
typedef long (__stdcall *BrDiSetPropFn)(BrDiObj *, uint32_t, const void *);
#define BR_DI_SETPARAMS(p, eff, flags) \
    ((BrDiSetParamsFn)(((const BrDiEffVtbl *)(const void *)(p)->pVtbl)->pfnSetParameters))((p), (eff), (flags))
#define BR_DI_SETPROP(p, prop, pdiph) \
    ((BrDiSetPropFn)(((const BrDiDevVtbl *)(const void *)(p)->pVtbl)->pfnSetProperty))((p), (prop), (pdiph))
#else
#define BR_DI_SETPARAMS(p, eff, flags) \
    (BrDiEff(p)->pfnSetParameters((p), (eff), (flags)))
#define BR_DI_SETPROP(p, prop, pdiph) \
    (BrDiDev(p)->pfnSetProperty((p), (prop), (pdiph)))
#endif

/* ====================================================================== */
/* 1. Entity state setters                                                 */
/* ====================================================================== */

/* @implements 0x10076420 d3d BrEntSetPos */
/* @n64 0x8021FE04 located */
#ifdef BR_MATCHING_BUILD
/* Struct second arg is not register-eligible, so __fastcall is thiscall. */
typedef struct { float x, y, z; } BrEntSetPosArgs;
void BR_THISCALL1 BrEntSetPos(BrEnt *pE, BrEntSetPosArgs a)
{
    /* Store order is the original's: mat0.m[3], f26C8, st, stB, stA. */
    pE->mat0.m[3][0] = a.x;
    pE->mat0.m[3][1] = a.y;
    pE->mat0.m[3][2] = a.z;

    pE->f26C8[0] = a.x;
    pE->f26C8[1] = a.y;
    pE->f26C8[2] = a.z;

    pE->st.pos.x = a.x;
    pE->st.pos.y = a.y;
    pE->st.pos.z = a.z;

    pE->stB.pos.x = a.x;
    pE->stB.pos.y = a.y;
    pE->stB.pos.z = a.z;

    pE->stA.pos.x = a.x;
    pE->stA.pos.y = a.y;
    pE->stA.pos.z = a.z;

    BrRbBuildMatrix(&pE->matrix, &pE->st);
}
#else
void BrEntSetPos(BrEnt *pE, float x, float y, float z)
{
    /* Store order is the original's: mat0.m[3], f26C8, st, stB, stA. */
    pE->mat0.m[3][0] = x;
    pE->mat0.m[3][1] = y;
    pE->mat0.m[3][2] = z;

    pE->f26C8[0] = x;
    pE->f26C8[1] = y;
    pE->f26C8[2] = z;

    pE->st.pos.x = x;
    pE->st.pos.y = y;
    pE->st.pos.z = z;

    pE->stB.pos.x = x;
    pE->stB.pos.y = y;
    pE->stB.pos.z = z;

    pE->stA.pos.x = x;
    pE->stA.pos.y = y;
    pE->stA.pos.z = z;

    BrRbBuildMatrix(&pE->matrix, &pE->st);
}
#endif

/* 0x100764C0 */
/* WHAT IT DOES: points a car (or other object in the world) in a given
 * compass direction, keeping it upright -- it can only turn about the
 * vertical, not tip or roll. It writes the new facing into every copy of the
 * object's state the physics keeps, so nothing is left pointing the old way. */
/* @implements 0x100764C0 d3d BrEntSetHeading */
#ifdef BR_MATCHING_BUILD
/* thiscall + one stack float (ret 4); sin/cos are the float-arg tree
 * wrappers, as in BrEntSetOrientation below.
 *
 * RESIDUE (glide 0x1006F720, 25 masked byte-diffs, multiset 0+0): one
 * scheduling fork only.  The original emits the c/s/0 stores BEFORE
 * popping the pending sin result (`fstp [esi+0x14]` after the three
 * movs); every probed spelling here pops it right at the call return.
 * Probed and dead: statement-order permutations of the m11/h statements,
 * a volatile-pinned m11 store, direct-call-in-statement (moves the call),
 * qw reads after the chain (drags the tail onto the FPU).  Everything
 * else is byte-exact: the dword-pun copies below reproduce the
 * original's integer-mov float copies (fld/fstp batching otherwise),
 * the z triple-store is a chained assignment (fst/fst/fstp), and qw/qx/
 * qy reload as dword puns after the second half-angle call. */
extern float BrSinF(float a);      /* glide 0x10002560 */
extern float BrCosF(float a);      /* glide 0x100023E0 */

void __fastcall BrEntSetHeading(BrEnt *pE, float a)
{
    float c  = BrCosF(a);
    float s  = BrSinF(a);
    float b  = a - kBrNegHalfPi;   /* a + pi/2, to the float's precision */
    float cb = BrCosF(b);
    float sb = BrSinF(b);
    float h;
    uint32_t qw, qx, qy;

    *(uint32_t *)&pE->mat0.m[0][0] = *(uint32_t *)&c;
    *(uint32_t *)&pE->mat0.m[0][1] = *(uint32_t *)&s;
    pE->mat0.m[0][2] = 0.0f;
    pE->mat0.m[1][1] = sb;
    h = a * kBrHalf;
    *(uint32_t *)&pE->mat0.m[1][0] = *(uint32_t *)&cb;
    pE->mat0.m[1][2] = 0.0f;
    pE->mat0.m[2][0] = 0.0f;
    pE->mat0.m[2][1] = 0.0f;
    pE->mat0.m[2][2] = 1.0f;

    pE->st.quat.f00 = BrCosF(h);
    pE->st.quat.f04 = 0.0f;
    pE->st.quat.f08 = 0.0f;

    pE->stA.quat.f0C = pE->stB.quat.f0C = pE->st.quat.f0C = BrSinF(h);

    qw = *(uint32_t *)&pE->st.quat.f00;
    qx = *(uint32_t *)&pE->st.quat.f04;
    qy = *(uint32_t *)&pE->st.quat.f08;

    *(uint32_t *)&pE->stB.quat.f00 = qw;
    *(uint32_t *)&pE->stA.quat.f00 = qw;
    *(uint32_t *)&pE->stB.quat.f04 = qx;
    *(uint32_t *)&pE->stB.quat.f08 = qy;
    *(uint32_t *)&pE->stA.quat.f04 = qx;
    *(uint32_t *)&pE->stA.quat.f08 = qy;

    BrRbBuildMatrix(&pE->matrix, &pE->st);
}
#else
void BrEntSetHeading(BrEnt *pE, float a)
{
    float c  = cosf(a);
    float s  = sinf(a);
    float b  = a - kBrNegHalfPi;   /* a + pi/2, to the float's precision */
    float cb = cosf(b);
    float sb = sinf(b);
    float h  = a * kBrHalf;
    float qw, qx, qy, qz;

    pE->mat0.m[0][0] = c;
    pE->mat0.m[0][1] = s;
    pE->mat0.m[0][2] = 0.0f;
    pE->mat0.m[1][1] = sb;
    pE->mat0.m[1][0] = cb;
    pE->mat0.m[1][2] = 0.0f;
    pE->mat0.m[2][0] = 0.0f;
    pE->mat0.m[2][1] = 0.0f;
    pE->mat0.m[2][2] = 1.0f;

    pE->st.quat.f00 = cosf(h);
    pE->st.quat.f04 = 0.0f;
    pE->st.quat.f08 = 0.0f;

    /* The original reloads w/x/y from memory here, BEFORE computing z, and
     * writes z to all three copies with fst/fst/fstp. Preserved. */
    qw = pE->st.quat.f00;
    qx = pE->st.quat.f04;
    qy = pE->st.quat.f08;
    qz = sinf(h);

    pE->st.quat.f0C  = qz;
    pE->stB.quat.f0C = qz;
    pE->stA.quat.f0C = qz;

    pE->stB.quat.f00 = qw;
    pE->stA.quat.f00 = qw;
    pE->stB.quat.f04 = qx;
    pE->stB.quat.f08 = qy;
    pE->stA.quat.f04 = qx;
    pE->stA.quat.f08 = qy;

    BrRbBuildMatrix(&pE->matrix, &pE->st);
}
#endif

/* Shared tail of 0x10076700 and 0x10076820: mirror st.quat into stB then
 * stA. Written out in the originals; identical instruction sequence in both. */
static void BrEntMirrorQuat(BrEnt *pE)
{
    pE->stB.quat.f00 = pE->st.quat.f00;
    pE->stB.quat.f04 = pE->st.quat.f04;
    pE->stB.quat.f08 = pE->st.quat.f08;
    pE->stB.quat.f0C = pE->st.quat.f0C;

    pE->stA.quat.f00 = pE->st.quat.f00;
    pE->stA.quat.f04 = pE->st.quat.f04;
    pE->stA.quat.f08 = pE->st.quat.f08;
    pE->stA.quat.f0C = pE->st.quat.f0C;
}

/* 0x10076700 */
/* WHAT IT DOES: sets an object's position and facing wholesale from a
 * ready-made transform, and works the facing back out into the form the
 * physics stores. Note that it does NOT update the physics' idea of where the
 * object is, only which way it is turned, so the rebuilt transform ends up
 * with the new rotation but the old position. */
/* @implements 0x10076700 d3d BrEntSetMatrix */
/* @n64 0x80220150 located */
/* Thiscall with ONE stack argument (`mov ebx,ecx` then `[esp+4]`, `ret 4`),
 * spelled the way the rest of this file's entity setters are.  The
 * quaternion mirror is written out here rather than calling
 * BrEntMirrorQuat: VC5 does not inline the static helper, and the original
 * has the eight dword copies in line. */
#ifdef BR_MATCHING_BUILD
void __fastcall BrEntSetMatrix(BrEnt *pE, int _edx_unused, const BrMat4 *pSrc)
{
    (void)_edx_unused;

    /* `rep movsd` of 16 dwords. */
    memcpy(&pE->mat0, pSrc, sizeof(BrMat4));

    BrSub100765E0(pSrc, &pE->st.quat);

    pE->stB.quat.f00 = pE->st.quat.f00;
    pE->stB.quat.f04 = pE->st.quat.f04;
    pE->stB.quat.f08 = pE->st.quat.f08;
    pE->stB.quat.f0C = pE->st.quat.f0C;

    pE->stA.quat.f00 = pE->st.quat.f00;
    pE->stA.quat.f04 = pE->st.quat.f04;
    pE->stA.quat.f08 = pE->st.quat.f08;
    pE->stA.quat.f0C = pE->st.quat.f0C;

    BrRbBuildMatrix(&pE->matrix, &pE->st);
}
#else
void BrEntSetMatrix(BrEnt *pE, const BrMat4 *pSrc)
{
    /* `rep movsd` of 16 dwords. */
    memcpy(&pE->mat0, pSrc, sizeof(BrMat4));

    BrSub100765E0(pSrc, &pE->st.quat);
    BrEntMirrorQuat(pE);

    BrRbBuildMatrix(&pE->matrix, &pE->st);
}
#endif

/* 0x100767A0 */
/* WHAT IT DOES: tells an object how fast and in which direction it is
 * travelling, writing it into all four places the game keeps that figure so
 * they agree. Nothing else about the object is disturbed. */
/* @implements 0x100767A0 d3d BrEntSetVel */
/* @n64 0x802201C8 located */
#ifdef BR_MATCHING_BUILD
void __fastcall BrEntSetVel(BrEnt *pE, int _edx_unused, float x, float y,
                            float z)
{
    (void)_edx_unused;

    pE->st.vel.x = x;
    pE->st.vel.y = y;
    pE->st.vel.z = z;

    pE->stB.vel.x = x;
    pE->stB.vel.y = y;
    pE->stB.vel.z = z;

    pE->stA.vel.x = x;
    pE->stA.vel.y = y;
    pE->stA.vel.z = z;

    pE->f1024[0] = x;
    pE->f1024[1] = y;
    pE->f1024[2] = z;
}
#else
void BrEntSetVel(BrEnt *pE, float x, float y, float z)
{
    pE->st.vel.x = x;
    pE->st.vel.y = y;
    pE->st.vel.z = z;

    pE->stB.vel.x = x;
    pE->stB.vel.y = y;
    pE->stB.vel.z = z;

    pE->stA.vel.x = x;
    pE->stA.vel.y = y;
    pE->stA.vel.z = z;

    pE->f1024[0] = x;
    pE->f1024[1] = y;
    pE->f1024[2] = z;
}
#endif

/* 0x10076820 */
/* WHAT IT DOES: turns an object by three angles about its three axes. It
 * ADDS the rotation to however the object was already facing rather than
 * replacing it, and unlike the other setters here it leaves the object's
 * drawing transform stale until something else rebuilds it. */
/* @implements 0x10076820 d3d BrEntSetOrientation */
#ifdef BR_MATCHING_BUILD
/* thiscall + three stack floats; sin/cos are the float-arg tree wrappers
 * (sin FIRST per axis), quat built fresh each axis with immediate zeros. */
extern float BrSinF(float a);      /* glide 0x10002560 */
extern float BrCosF(float a);      /* glide 0x100023E0 */

void __fastcall BrEntSetOrientation(BrEnt *pE, int _edx_unused,
                                    float a1, float a2, float a3)
{
    float h1 = a1 * kBrHalf;
    float h2 = a2 * kBrHalf;
    float h3 = a3 * kBrHalf;
    BrVec4 q;

    (void)_edx_unused;

    {
        float sn = BrSinF(h1);
        q.f00 = BrCosF(h1);
        q.f04 = 0.0f;
        q.f08 = 0.0f;
        q.f0C = sn;
    }
    BrSub10074090(&pE->st.quat, &pE->st.quat, &q);

    {
        float sn = BrSinF(h2);
        q.f00 = BrCosF(h2);
        q.f04 = 0.0f;
        q.f08 = sn;
        q.f0C = 0.0f;
    }
    BrSub10074090(&pE->st.quat, &pE->st.quat, &q);

    {
        float sn = BrSinF(h3);
        q.f00 = BrCosF(h3);
        q.f04 = sn;
        q.f08 = 0.0f;
        q.f0C = 0.0f;
    }
    BrSub10074090(&pE->st.quat, &pE->st.quat, &q);

    BrVec4Normalise(&pE->st.quat);

    pE->stB.quat.f00 = pE->st.quat.f00;
    pE->stB.quat.f04 = pE->st.quat.f04;
    pE->stB.quat.f08 = pE->st.quat.f08;
    pE->stB.quat.f0C = pE->st.quat.f0C;
    pE->stA.quat.f00 = pE->st.quat.f00;
    pE->stA.quat.f04 = pE->st.quat.f04;
    pE->stA.quat.f08 = pE->st.quat.f08;
    pE->stA.quat.f0C = pE->st.quat.f0C;
}
#else
/* @implements 0x10076820 d3d BrEntSetOrientation */
void BrEntSetOrientation(BrEnt *pE, float a1, float a2, float a3)
{
    /* All three half-angles are formed up front, before any call. */
    float h1 = a1 * kBrHalf;
    float h2 = a2 * kBrHalf;
    float h3 = a3 * kBrHalf;
    BrVec4 q;

    /* Z: (cos, 0, 0, sin) */
    q.f0C = sinf(h1);
    q.f00 = cosf(h1);
    q.f04 = 0.0f;
    q.f08 = 0.0f;
    BrSub10074090(&pE->st.quat, &pE->st.quat, &q);

    /* Y: (cos, 0, sin, 0) */
    q.f08 = sinf(h2);
    q.f00 = cosf(h2);
    q.f04 = 0.0f;
    q.f0C = 0.0f;
    BrSub10074090(&pE->st.quat, &pE->st.quat, &q);

    /* X: (cos, sin, 0, 0) */
    q.f04 = sinf(h3);
    q.f00 = cosf(h3);
    q.f08 = 0.0f;
    q.f0C = 0.0f;
    BrSub10074090(&pE->st.quat, &pE->st.quat, &q);

    BrVec4Normalise(&pE->st.quat);
    BrEntMirrorQuat(pE);
    /* No BrRbBuildMatrix here -- see the header. */
}
#endif

/* 0x100769A0 */
/* WHAT IT DOES: tells an object how fast it is spinning, writing it into all
 * three places the game keeps that figure so they agree. */
/* @implements 0x100769A0 d3d BrEntSetAngVel */
/* @n64 0x80220358 located */
#ifdef BR_MATCHING_BUILD
void __fastcall BrEntSetAngVel(BrEnt *pE, int _edx_unused, float x, float y,
                               float z)
{
    (void)_edx_unused;

    pE->st.angVel.x = x;
    pE->st.angVel.y = y;
    pE->st.angVel.z = z;

    pE->stB.angVel.x = x;
    pE->stB.angVel.y = y;
    pE->stB.angVel.z = z;

    pE->stA.angVel.x = x;
    pE->stA.angVel.y = y;
    pE->stA.angVel.z = z;
}
#else
void BrEntSetAngVel(BrEnt *pE, float x, float y, float z)
{
    pE->st.angVel.x = x;
    pE->st.angVel.y = y;
    pE->st.angVel.z = z;

    pE->stB.angVel.x = x;
    pE->stB.angVel.y = y;
    pE->stB.angVel.z = z;

    pE->stA.angVel.x = x;
    pE->stA.angVel.y = y;
    pE->stA.angVel.z = z;
}
#endif

/* 0x10076A00 */
/* WHAT IT DOES: pushes the paint colour a car has been given down into the
 * artwork the renderer actually uses, which is how the player's chosen colour
 * reaches the screen. The colour loses precision on the way -- it is stored
 * more coarsely than it was chosen -- so reading it back does not give the
 * same value. */
/* @implements 0x10076A00 d3d BrEntRefreshColour */
/* @n64 0x80220398 located */
void __fastcall BrEntRefreshColour(BrEnt *pE)
{
    BrCarGfxSetColour(pE->pRec, pE->r >> 3, pE->g >> 3, pE->b >> 3);
    BrSub10062C50(pE);
}

/* 0x10076A40 */
/* WHAT IT DOES: attaches one of the sixteen car artwork records to this
 * object -- which model and textures it is drawn with -- and immediately
 * repaints it in its own colour. The record number is not checked at all, so
 * an out-of-range one silently points at whatever memory follows the table. */
/* @implements 0x10076A40 d3d BrEntSetRecord */
/* @n64 0x802203F0 located */
#ifdef BR_MATCHING_BUILD
void __fastcall BrEntSetRecord(BrEnt *pE, void *_dummy, int32_t idx)
#else
void BrEntSetRecord(BrEnt *pE, int32_t idx)
#endif
{
    /* The original's exact shift/add chain, in uint32_t so it wraps the same
     * way: ((((idx*11) << 6) - idx) << 4) + idx, times 8 == idx * 89992. */
    uint32_t t = (uint32_t)idx;
    uint32_t d = t + t * 4u;      /* lea edx,[eax+eax*4] */
    d = t + d * 2u;               /* lea edx,[eax+edx*2] */
    d <<= 6;
    d -= t;
    d <<= 4;
    d += t;
    d *= 8u;                      /* lea eax,[edx*8 + 0x100C12A0] */

    pE->pRec = (BrCarGfx *)(void *)(g_aBrC12A0 + d);
    BrEntRefreshColour(pE);
}

/* 0x10076B20 */
/* WHAT IT DOES: returns a car to a clean state -- straightens out all its
 * internal transforms, stops it dead, and copies the handling constants for
 * this particular car model out of the artwork record into the car itself,
 * after which it lets go of the record. One of the six sub-transforms is
 * skipped where the other five are done, which looks like a bug in the
 * original and is preserved. */
/* port-only body; Glide match is src/core/cpp/0x1006FD90.cpp */
void BrEntReset(BrEnt *pE)
{
    const unsigned char *rec;
    int i;

    BrMat4SetLastColumn(&pE->mat0);

    BrMat4SetLastColumn(&pE->aFrames[0].m);
    pE->aFrames[0].f40 = kBrSixthPi;
    BrMat4SetLastColumn(&pE->aFrames[1].m);
    pE->aFrames[1].f40 = kBrSixthPi;
    BrMat4SetLastColumn(&pE->aFrames[2].m);
    pE->aFrames[2].f40 = kBrSixthPi;
    BrMat4SetLastColumn(&pE->aFrames[3].m);
    pE->aFrames[3].f40 = kBrSixthPi;
    /* aFrames[4] (0x284C) is skipped by the original -- see the header. */
    BrMat4SetLastColumn(&pE->aFrames[5].m);
    pE->aFrames[5].f40 = kBrSixthPi;

    pE->p2734 = &pE->aFrames[0];

    BrMat4SetLastColumn(&pE->mat40);
    BrMat4SetLastColumn(&pE->mat80);
    BrMat4SetLastColumn(&pE->matC0);
    BrMat4SetLastColumn(&pE->mat100);

#ifdef BR_MATCHING_BUILD
    BrEntSetVel(pE, 0, 0.0f, 0.0f, 0.0f);
#else
    BrEntSetVel(pE, 0.0f, 0.0f, 0.0f);
#endif

    pE->fF8C  = 0;
    pE->fF90  = 0;
    pE->f2738 = 0;

    rec = (const unsigned char *)(const void *)pE->pRec;

    /* rec +0x98 .. +0xC4 -> fE28[0..11]. The original does this as a 7-dword
     * `rep movsd` followed by five hand-written moves; the two runs are
     * contiguous in both the source and the destination. */
    for (i = 0; i < 12; ++i) {
        pE->fE28[i] = BrLoad32(rec + 0x98 + (size_t)i * 4u);
    }

    /* `movsx` -- these three are SIGN-extended bytes. */
    pE->fE58 = (int32_t)(int8_t)rec[0xD8];
    pE->fE60 = pE->fE9C;
    pE->fE5C = (int32_t)(int8_t)rec[0x96];

    for (i = 0; i < 4; ++i) {
        pE->f340[i] = BrLoad32(rec + 0xC8 + (size_t)i * 4u);
    }

    /* The original reads rec[0x97] into eax, THEN clears pRec, THEN stores. */
    {
        int32_t v = (int32_t)(int8_t)rec[0x97];
        pE->pRec = NULL;
        pE->fE64 = v;
    }
}

/* ====================================================================== */
/* 2. 0x10077090                                                           */
/* ====================================================================== */

void BrSet680598(uint32_t v)
{
    uint32_t lo = v & 0xFFFFu;
    uint32_t hi = v >> 16;

    g_br680598 = v;
    g_br68059C = lo;
    g_br6805A0 = hi;

    if (lo == 0u) {
        BrExt_10008B80();
        return;
    }
    if (hi == 0u) {
        return;
    }
    BrExt_10008B80();
}

/* ====================================================================== */
/* 3. Input binding queries                                                */
/* ====================================================================== */

/* DEVIATION: the six buffer indices are masked with & 1 before they index a
 * two-element array. The original does not mask on the read side, but the
 * only writer (0x100773F0) produces `(x - 1) & 1`, so every value it can hold
 * is already 0 or 1. The mask turns what would be undefined behaviour in this
 * port into the original's behaviour for every reachable state. */
#define BR_BUF(i) ((size_t)((i) & 1))


/* DIJOYSTATE padded to the original's 0x110 stride, and the 0x1C mouse rec */
typedef struct BrInJoy {
    int32_t lX, lY, lZ;               /* +0x00 +0x04 +0x08 */
    uint8_t pad0C[0x30 - 0x0C];
    uint8_t rgbButtons[0x110 - 0x30]; /* +0x30 */
} BrInJoy;
typedef struct BrInMouse {
    int32_t x, y, z;                  /* +0x00 +0x04 +0x08 */
    uint8_t pad0C[0x18 - 0x0C];
    uint8_t buttons[0x1C - 0x18];     /* +0x18 */
} BrInMouse;
extern const unsigned char *g_BrPadModeBytes;   /* 0x10B71534, slice2_19.c */

/* 0x10071710 */
/* Matching-model globals for the DirectInput query path (glide addresses).
 * The bindings pointer 0x10B71534 is the SAME object slice2_19.c reads as
 * g_BrPadModeBytes -- one storage, aliased on purpose there, indexed here. */
int32_t g_brInKeyCur;                     /* 0x118EEBF0 */
int32_t g_brInJoyCur;                     /* 0x118EEBD0 */
int32_t g_brInMouseCur;                   /* 0x118EEE98 */
uint8_t g_brInKeys[2][256];               /* 0x118EE9D0 */
#ifdef BR_MATCHING_BUILD
/* Incomplete-type extern so axis compares stay `[reg + disp32]` (orig
 * `cmp [ebx + g_brInJoy], imm`) instead of adding the base into the
 * scaled index. The port keeps the sized arrays. */
extern BrInJoy   g_brInJoy[];
extern BrInMouse g_brInMouse[];
#else
BrInJoy   g_brInJoy[2];                   /* 0x118EEBF8, stride 0x110 */
BrInMouse g_brInMouse[2];                 /* 0x118EEE50, stride 0x1C  */
#endif

/* WHAT IT DOES: answers "is the player holding down the control for this
 * action right now?" -- checking whichever key, button, stick direction or
 * mouse movement the action is bound to, plus up to two keyboard alternatives
 * that always apply. Stick and mouse directions only count once they are
 * pushed past a dead zone, so a resting stick reads as nothing.
 *
 * The original indexes the key/button tables with UNCHECKED bytes (no & 1 /
 * & 3 masks) and switches on the u16 binding word masked to its high byte.
 *
 * Nested `<=` / `!=` at each pivot reproduces orig's binary-tree node form
 * (`cmp; jg; cmp; je` at the root; last three cases a linear == chain). A
 * switch compactes the root; a flat else-if chain is a linear ladder. */
/* @implements 0x10071710 glide BrInputIsDown */
uint8_t BrInputIsDown(int32_t action)
{
    uint8_t r = 0;
    const uint8_t *b = g_BrPadModeBytes + 6 * action;
    int32_t cur = g_brInKeyCur;
    int32_t w = *(const uint16_t *)(const void *)b & 0xFF00;
    int32_t w2 = w;

    if (w <= 0x100) {
        if (w2 != 0x100) {
            if (w2 == 0)
                r = (uint8_t)(g_brInKeys[cur][b[0]] & 0x80u);
        } else {
            r = (uint8_t)(g_brInJoy[g_brInJoyCur].rgbButtons[b[0]] & 0x80u);
        }
    } else if (w <= 0x8000) {
        if (w != 0x8000) {
            if (w == 0x300)
                r = (uint8_t)(g_brInMouse[g_brInMouseCur].buttons[b[0]] & 0x80u);
        } else {
            if (g_brInJoy[g_brInJoyCur].lX < -50) r = 0x80;
        }
    } else if (w <= 0x8200) {
        if (w != 0x8200) {
            if (w == 0x8100) {
                if (g_brInJoy[g_brInJoyCur].lX > 50) r = 0x80;
            }
        } else {
            if (g_brInJoy[g_brInJoyCur].lY < -50) r = 0x80;
        }
    } else if (w <= 0x8400) {
        if (w != 0x8400) {
            if (w == 0x8300) {
                if (g_brInJoy[g_brInJoyCur].lY > 50) r = 0x80;
            }
        } else {
            if (g_brInJoy[g_brInJoyCur].lZ < -50) r = 0x80;
        }
    } else if (w <= 0x8600) {
        if (w != 0x8600) {
            if (w == 0x8500) {
                if (g_brInJoy[g_brInJoyCur].lZ > 50) r = 0x80;
            }
        } else {
            if (g_brInMouse[g_brInMouseCur].x < -50) r = 0x80;
        }
    } else if (w <= 0x8800) {
        if (w != 0x8800) {
            if (w == 0x8700) {
                if (g_brInMouse[g_brInMouseCur].x > 50) r = 0x80;
            }
        } else {
            if (g_brInMouse[g_brInMouseCur].y < -50) r = 0x80;
        }
    } else if (w == 0x8900) {
        if (g_brInMouse[g_brInMouseCur].y > 50) r = 0x80;
    } else if (w == 0x8A00) {
        if (g_brInMouse[g_brInMouseCur].z < -50) r = 0x80;
    } else if (w == 0x8B00) {
        if (g_brInMouse[g_brInMouseCur].z > 50) r = 0x80;
    }

    if (!b[3]) {
        unsigned idx = (unsigned char)b[2];
        r |= (uint8_t)(g_brInKeys[cur][idx] & 0x80u);
    }
    if (!b[5]) {
        unsigned idx = (unsigned char)b[4];
        r |= (uint8_t)(g_brInKeys[cur][idx] & 0x80u);
    }
    return r;
}

/* DEVIATION (same as above): the mouse button index is masked with & 3. The
 * original indexes a four-byte field with an unchecked byte. */

/* 0x100786E0 */
/* WHAT IT DOES: answers "did the player press this control on THIS frame?",
 * by comparing what the controls read now against what they read last frame.
 * It is what stops a held-down key repeating in the menus. Curiously it
 * answers a different value for a stick edge than for a key press, so the
 * caller cannot treat the two as interchangeable. */
/* @implements 0x100786E0 d3d BrInputJustPressed */
uint8_t BrInputJustPressed(int32_t action)
{
    const BrInputBinding *b = &g_brInput.pBindings[action];
    const uint8_t *kPrev = g_brInput.aKeys[BR_BUF(g_brInput.iKeyPrev)];
    const uint8_t *kCur  = g_brInput.aKeys[BR_BUF(g_brInput.iKeyCur)];
    const BrDiJoyState *jPrev = &g_brInput.aJoy[BR_BUF(g_brInput.iJoyPrev)];
    const BrDiJoyState *jCur  = &g_brInput.aJoy[BR_BUF(g_brInput.iJoyCur)];
    const BrMouseState *mPrev = &g_brInput.aMouse[BR_BUF(g_brInput.iMousePrev)];
    const BrMouseState *mCur  = &g_brInput.aMouse[BR_BUF(g_brInput.iMouseCur)];
    uint8_t r = 0;
    uint8_t c = b->code0;

    switch (b->kind0) {
    case BR_BIND_KEY:
        if ((kPrev[c] & 0x80u) == 0u && (kCur[c] & 0x80u) != 0u) r = 1;
        break;
    case BR_BIND_JOYBTN:
        if ((jPrev->rgbButtons[c] & 0x80u) == 0u &&
            (jCur->rgbButtons[c] & 0x80u) != 0u) r = 1;
        break;
    case BR_BIND_MOUSEBTN:
        if ((mPrev->buttons[c & 3u] & 0x80u) == 0u &&
            (mCur->buttons[c & 3u] & 0x80u) != 0u) r = 1;
        break;

    /* Axis edges yield 0x80, NOT 1. See the header. */
    case BR_BIND_JOYXNEG:
        if (jPrev->lX >= -BR_BIND_DEADZONE && jCur->lX < -BR_BIND_DEADZONE) r = 0x80;
        break;
    case BR_BIND_JOYXPOS:
        if (jPrev->lX <=  BR_BIND_DEADZONE && jCur->lX >  BR_BIND_DEADZONE) r = 0x80;
        break;
    case BR_BIND_JOYYNEG:
        if (jPrev->lY >= -BR_BIND_DEADZONE && jCur->lY < -BR_BIND_DEADZONE) r = 0x80;
        break;
    case BR_BIND_JOYYPOS:
        if (jPrev->lY <=  BR_BIND_DEADZONE && jCur->lY >  BR_BIND_DEADZONE) r = 0x80;
        break;
    case BR_BIND_JOYZNEG:
        if (jPrev->lZ >= -BR_BIND_DEADZONE && jCur->lZ < -BR_BIND_DEADZONE) r = 0x80;
        break;
    case BR_BIND_JOYZPOS:
        if (jPrev->lZ <=  BR_BIND_DEADZONE && jCur->lZ >  BR_BIND_DEADZONE) r = 0x80;
        break;
    case BR_BIND_MOUXNEG:
        if (mPrev->x >= -BR_BIND_DEADZONE && mCur->x < -BR_BIND_DEADZONE) r = 0x80;
        break;
    case BR_BIND_MOUXPOS:
        if (mPrev->x <=  BR_BIND_DEADZONE && mCur->x >  BR_BIND_DEADZONE) r = 0x80;
        break;
    case BR_BIND_MOUYNEG:
        if (mPrev->y >= -BR_BIND_DEADZONE && mCur->y < -BR_BIND_DEADZONE) r = 0x80;
        break;
    case BR_BIND_MOUYPOS:
        if (mPrev->y <=  BR_BIND_DEADZONE && mCur->y >  BR_BIND_DEADZONE) r = 0x80;
        break;
    case BR_BIND_MOUZNEG:
        if (mPrev->z >= -BR_BIND_DEADZONE && mCur->z < -BR_BIND_DEADZONE) r = 0x80;
        break;
    case BR_BIND_MOUZPOS:
        if (mPrev->z <=  BR_BIND_DEADZONE && mCur->z >  BR_BIND_DEADZONE) r = 0x80;
        break;
    default:
        break;
    }

    if (b->kind1 == 0) {
        if ((kPrev[b->code1] & 0x80u) == 0u && (kCur[b->code1] & 0x80u) != 0u) {
            r |= 1u;
        }
    }
    if (b->kind2 == 0) {
        if ((kPrev[b->code2] & 0x80u) == 0u && (kCur[b->code2] & 0x80u) != 0u) {
            r |= 1u;
        }
    }
    return r;
}

/* ====================================================================== */
/* 4. DirectInput devices                                                  */
/* ====================================================================== */

/* 0x100773D0 */
/* WHAT IT DOES: takes hold of the joystick or wheel so the game can read it,
 * which Windows requires again every time the game comes back to the
 * foreground. It reports whether it succeeded, and says "no" harmlessly if
 * there is no such device. */
/* port-only body; Glide match is src/core/generated/0x100706B0.c */
int32_t BrDiAcquire(void)
{
    BrDiObj *pDev = g_brFfb.pDevice;

    if (pDev == NULL) {
        return 0;
    }
    return (BrDiDev(pDev)->pfnAcquire(pDev) >= 0) ? 1 : 0;
}

/* 0x10078BC0 */
/* WHAT IT DOES: lets go of the keyboard, but only once as many parts of the
 * game have finished with it as asked for it in the first place -- it counts
 * users rather than shutting down on the first call. An extra call after the
 * count has already reached zero does nothing at all. */
/* port-only body; Glide match is src/core/generated/0x10071EB0.c */
void BrDiKeyboardShutdown(void)
{
    BrDiObj *pDev;

    g_br18ABDD8 -= 1;
    if (g_br18ABDD8 < 0) {
        g_br18ABDD8 = 0;
        return;                       /* clamp, and NO teardown */
    }
    if (g_br18ABDD8 != 0) {
        return;
    }
    pDev = g_pBr18ABDD0;
    if (pDev == NULL) {
        return;
    }
    BrDiDev(pDev)->pfnUnacquire(pDev);
    /* Re-read, deliberately without a second NULL test -- as the original. */
    pDev = g_pBr18ABDD0;
    BrDiDev(pDev)->pfnRelease(pDev);
    g_pBr18ABDD0 = NULL;
}

/* 0x10078C30 */
/* WHAT IT DOES: tells Windows what range of numbers one axis of a controller
 * should report -- how far left and right count as full deflection. A small
 * convenience wrapper around the awkward Windows call; the game uses it to
 * scale the wheel and stick into the range it wants. */
/* @implements 0x10078C30 d3d BrDiSetPropRange */
long BrDiSetPropRange(BrDiObj *pDev, uint32_t prop, uint32_t dwObj,
                      uint32_t dwHow, int32_t lMin, int32_t lMax)
{
    BrDiPropRange r;

    r.dwSize       = 0x18u;
    r.dwHeaderSize = 0x10u;
    r.dwObj        = dwObj;
    r.dwHow        = dwHow;
    r.lMin         = lMin;
    r.lMax         = lMax;

    return BR_DI_SETPROP(pDev, prop, &r);
}

/* 0x10078C80 */
/* WHAT IT DOES: the same, for controller settings that are a single number
 * rather than a range -- the dead zone and the wheel's self-centring are the
 * two the game sets this way. */
/* @implements 0x10078C80 d3d BrDiSetPropDword */
long BrDiSetPropDword(BrDiObj *pDev, uint32_t prop, uint32_t dwObj,
                      uint32_t dwHow, uint32_t dwData)
{
    BrDiPropDword d;

    /* Assignment order = the original's store order: data, obj, how, then
     * the two header constants (which the scheduler sinks past the pushes). */
    d.dwObj  = dwObj;
    d.dwHow  = dwHow;
    d.dwData = dwData;
    d.dwSize       = 0x14u;
    d.dwHeaderSize = 0x10u;

    return BR_DI_SETPROP(pDev, prop, &d);
}

/* 0x10078E10 */
/* WHAT IT DOES: chooses which way the next shake of a force-feedback wheel
 * will push. It is remembered rather than sent, taking effect the next time
 * the effect is committed, and it does nothing at all unless force feedback
 * is switched on and a suitable wheel is attached. */
/* @implements 0x10078E10 d3d BrFfbSetDirection */
void BrFfbSetDirection(int32_t dir)
{
    if (g_brB4E1D0 != 1 && g_brB4E1D0 != 2) {
        return;
    }
    if (g_brB4E1E0 == 0) {
        return;
    }
    if (g_br18ABDBC == 0) {
        return;
    }
    if (g_brFlag6909E0 != 0) {
        return;
    }
    g_br0BD430[0] = dir;
}

/* 0x10078E50 */
/* WHAT IT DOES: asks for the next shake of the wheel to be the long one --
 * a quarter of a second. Like the direction it is only remembered, and only
 * when force feedback is actually available. */
/* @implements 0x10078E50 d3d BrFfbSetDurationLong */
void BrFfbSetDurationLong(void)
{
    if (g_brB4E1D0 != 1 && g_brB4E1D0 != 2) {
        return;
    }
    if (g_brB4E1E0 == 0) {
        return;
    }
    if (g_br18ABDBC == 0) {
        return;
    }
    if (g_brFlag6909E0 != 0) {
        return;
    }
    g_br0BD438 = 0x3D090;   /* 250000 us */
}

/* 0x10078E90 */
/* WHAT IT DOES: the same, for the short shake -- an eighth of a second. */
/* @implements 0x10078E90 d3d BrFfbSetDurationShort */
void BrFfbSetDurationShort(void)
{
    if (g_brB4E1D0 != 1 && g_brB4E1D0 != 2) {
        return;
    }
    if (g_brB4E1E0 == 0) {
        return;
    }
    if (g_br18ABDBC == 0) {
        return;
    }
    if (g_brFlag6909E0 != 0) {
        return;
    }
    g_br0BD438 = 0x1E848;   /* 125000 us */
}

/* 0x10078ED0 */
/* WHAT IT DOES: actually delivers the shake -- it hands the wheel the length
 * and direction that were chosen above and tells it to start, which is what
 * the player feels on a bump or a collision. */
/* @implements 0x10078ED0 d3d BrFfbCommitDuration */
void BrFfbCommitDuration(void)
{
    BrDiObj *pEff;

    if (g_brB4E1D0 != 1 && g_brB4E1D0 != 2) {
        return;
    }
    if (g_brB4E1E0 == 0) {
        return;
    }
    if (g_br18ABDBC == 0) {
        return;
    }
    if (g_brFlag6909E0 != 0) {
        return;
    }
    g_brDiEffSquare.dwDuration = (uint32_t)g_br0BD438;

    pEff = g_brFfb.pEffectSquare;
    if (pEff == NULL) {
        return;
    }
    /* 0x20000041 = DIEP_DURATION | DIEP_DIRECTION | DIEP_START */
    BR_DI_SETPARAMS(pEff, &g_brDiEffSquare, 0x20000041u);
}

/* 0x100790B0 */
/* WHAT IT DOES: sets how hard a force-feedback wheel pulls back towards
 * centre -- the weight of the steering the player feels -- and sends the new
 * strength to the wheel at once. Unusually for this group it does not first
 * check that force feedback is enabled. */
/* @implements 0x100790B0 d3d BrFfbSetSpringCoeff */
void BrFfbSetSpringCoeff(int32_t coeff)
{
    BrDiObj *pEff;

    g_brDiSpringCond[0].lPositiveCoefficient = coeff;
    g_brDiSpringCond[0].lNegativeCoefficient = coeff;

    pEff = g_brFfb.pEffectSpring;
    if (pEff == NULL) {
        return;
    }
    /* 0x100 = DIEP_TYPESPECIFICPARAMS */
    BR_DI_SETPARAMS(pEff, &g_brDiEffSpring, 0x100u);
}

/* 0x10078F20 */
/* WHAT IT DOES: eases the weight of the steering up or down a step at a time
 * rather than jumping to it, so the wheel's resistance changes smoothly as
 * the car speeds up or slows down, stopping at a floor and a ceiling that
 * depend on how much resistance the caller has asked for. Asking for it to be
 * off stops the effect entirely, and asking for it again afterwards restarts
 * it. The wheel is only told when the value has actually moved. */
/* @implements 0x10078F20 d3d BrFfbUpdateSpring */
void BrFfbUpdateSpring(int32_t up, int32_t enable, int32_t decay)
{
    int32_t scaled;
    int32_t rate;
    int32_t cur;
    int32_t before;
    int32_t bound;
    BrDiObj *pEff;

    if (g_brB4E1D0 != 1 && g_brB4E1D0 != 2) {
        return;
    }
    if (g_brB4E1E0 == 0) {
        return;
    }
    if (g_br18ABDBC == 0) {
        return;
    }
    if (g_brFlag6909E0 != 0) {
        return;
    }

    /* ((decay + 8) * g_br0BD424) * 1000 / 10000, all in 32 bits. */
    scaled = BrMulWrap(BrMulWrap(BrAddWrap(decay, 8), g_br0BD424), 1000);
    scaled = BrDiv10000(scaled);
    g_br0BD42C = scaled;

    if (enable == 0) {
        if (g_br18ABDF8 != 0) {
            return;                    /* already stopped */
        }
        pEff = g_brFfb.pEffectSpring;
        if (pEff != NULL) {
            BrDiEff(pEff)->pfnStop(pEff);
        }
        g_br18ABDF8 = 1;
        return;
    }

    if (g_br18ABDF8 != 0) {
        pEff = g_brFfb.pEffectSpring;
        if (pEff != NULL) {
            BrDiEff(pEff)->pfnStart(pEff, 1u, 0u);
        }
        BrFfbSetDurationLong();
        /* Both are re-read from memory here -- BrFfbSetDurationLong could in
         * principle have changed them. */
        scaled = g_br0BD42C;
        g_br18ABDF8 = 0;
    }

    cur    = g_br18ABD78;
    before = cur;

    if (up != 0) {
        /* The original forms -(g_br0BD424) * 1000 and divides THAT, so the
         * negation happens before the truncation. Kept in that order. */
        rate = BrDiv10000(BrMulWrap(g_br0BD424, -1000));
        cur  = BrAddWrap(before, rate);
        g_br18ABD78 = cur;
        bound = BrDiv10000(BrMulWrap(g_br0BD428, g_br0BD424));
        if (cur < bound) {                 /* lower clamp */
            cur = bound;
            g_br18ABD78 = cur;
        }
    } else {
        rate = BrDiv10000(BrMulWrap(g_br0BD424, 1000));
        cur  = BrAddWrap(before, rate);
        g_br18ABD78 = cur;
        bound = BrDiv10000(BrMulWrap(scaled, g_br0BD424));
        if (cur > bound) {                 /* upper clamp */
            cur = bound;
            g_br18ABD78 = cur;
        }
    }

    if (cur != before) {
        BrFfbSetSpringCoeff(cur);
    }
}

/* DEVIATION: in the original both DIEFFECTs' rgdwAxes point at a two-dword
 * STACK local of 0x10079390, which dangles the moment it returns -- and
 * BrFfbSetSpringCoeff / BrFfbCommitDuration hand those descriptors back to
 * SetParameters afterwards. Reproducing that would be undefined behaviour in
 * this port, so the buffer is file-scope. Its contents (0, 4) and the fact
 * that BOTH descriptors share ONE buffer are preserved. */
static uint32_t g_brFfbAxes[2];

/* 0x10079390 */
/* WHAT IT DOES: builds the two force-feedback effects the game uses -- the
 * constant centring pull that gives the steering its weight, and the shake
 * used for bumps and impacts -- and hands both to the wheel ready to be
 * started. The centring effect is created but deliberately left stopped. */
/* @implements 0x10079390 d3d BrFfbSetup */
void BrFfbSetup(int32_t springCoeff, int32_t springCoeff2)
{
    BrDiObj *pDev = g_brFfb.pDevice;
    long hr;

    g_brDiSpringCond[1].lPositiveCoefficient = springCoeff2;
    g_brDiSpringCond[1].lNegativeCoefficient = springCoeff2;

    g_brDiSpringCond[0].lOffset              = 0;
    g_brDiSpringCond[0].lPositiveCoefficient = springCoeff;
    g_brDiSpringCond[0].lNegativeCoefficient = springCoeff;
    g_brDiSpringCond[0].dwPositiveSaturation = 10000u;
    g_brDiSpringCond[0].dwNegativeSaturation = 10000u;
    g_brDiSpringCond[0].lDeadBand            = 0;
    g_brDiSpringCond[1].lOffset              = 0;
    g_brDiSpringCond[1].dwPositiveSaturation = 10000u;
    g_brDiSpringCond[1].dwNegativeSaturation = 10000u;
    g_brDiSpringCond[1].lDeadBand            = 0;

    g_brDiEffSpring.dwSize                 = 0x34u;   /* sizeof on x86 */
    g_brDiEffSpring.dwFlags                = 0x12u;   /* OBJECTOFFSETS|CARTESIAN */
    g_brDiEffSpring.dwDuration             = 0xFFFFFFFFu;  /* INFINITE */
    g_brDiEffSpring.dwSamplePeriod         = 0u;
    g_brDiEffSpring.dwGain                 = 10000u;
    g_brDiEffSpring.dwTriggerButton        = 0xFFFFFFFFu;  /* DIEB_NOTRIGGER */
    g_brDiEffSpring.dwTriggerRepeatInterval = 0u;
    g_brDiEffSpring.cAxes                  = 2u;
    g_brDiEffSpring.rgdwAxes               = g_brFfbAxes;
    g_brDiEffSpring.rglDirection           = g_brDiSpringDir;
    g_brDiEffSpring.lpEnvelope             = NULL;
    g_brDiEffSpring.cbTypeSpecificParams   = 0x30u;   /* two DICONDITIONs */
    g_brDiEffSpring.lpvTypeSpecificParams  = g_brDiSpringCond;

    g_brFfbAxes[0] = 0u;   /* lX */
    g_brFfbAxes[1] = 4u;   /* lY */

    hr = BrDiDev(pDev)->pfnCreateEffect(pDev, kBrGuidSpring, &g_brDiEffSpring,
                                        &g_brFfb.pEffectSpring, NULL);
    if (hr == 0) {
        g_br18ABD78 = springCoeff;
        g_br18ABDF8 = 1;               /* created, but not started */
    }

    g_brDiSquarePeriod.dwMagnitude = 10000u;
    g_brDiSquarePeriod.lOffset     = 0;
    g_brDiSquarePeriod.dwPhase     = 0u;
    g_brDiSquarePeriod.dwPeriod    = 0x3D090u;   /* 250000 us */

    g_brDiEffSquare.dwSize                  = 0x34u;
    g_brDiEffSquare.dwFlags                 = 0x12u;
    g_brDiEffSquare.dwDuration              = (uint32_t)g_br0BD438;
    g_brDiEffSquare.dwSamplePeriod          = 0u;
    g_brDiEffSquare.dwGain                  = 10000u;
    g_brDiEffSquare.dwTriggerButton         = 0xFFFFFFFFu;
    g_brDiEffSquare.dwTriggerRepeatInterval = 0u;
    g_brDiEffSquare.cAxes                   = 2u;
    g_brDiEffSquare.rgdwAxes                = g_brFfbAxes;
    g_brDiEffSquare.rglDirection            = g_br0BD430;
    g_brDiEffSquare.lpEnvelope              = NULL;
    g_brDiEffSquare.cbTypeSpecificParams    = 0x10u;  /* one DIPERIODIC */
    g_brDiEffSquare.lpvTypeSpecificParams   = &g_brDiSquarePeriod;

    (void)BrDiDev(pDev)->pfnCreateEffect(pDev, kBrGuidSquare, &g_brDiEffSquare,
                                         &g_brFfb.pEffectSquare, NULL);
}

/* 0x100790E0 */
/* WHAT IT DOES: called by Windows once for each controller it finds; this is
 * the game deciding to use that one. It opens the device, claims it, and
 * describes what sort of data it wants back, stopping the search on the first
 * one that works. Every way it can fail writes a message to the debugger and
 * gives the device back. */
/* @implements 0x100790E0 d3d BrFfbEnumDevice */
int32_t BrFfbEnumDevice(const void *pDevInst, void *pvRef)
{
    unsigned char guid[16];
    BrDiObj *pTmp = NULL;
    BrDiObj *pDev;
    long hr;

    memcpy(guid, (const unsigned char *)pDevInst + 4, sizeof guid);

    if (BrDiRoot(g_pBr18ABD70)->pfnCreateDevice(g_pBr18ABD70, guid,
                                                &pTmp, NULL) < 0) {
        BrDbgPrint(kBrErrCreateDevice);
        return 0;
    }

    hr = BrDiDev(pTmp)->pfnQueryInterface(pTmp, kBrIidDevice2A,
                                          (void **)(void *)&g_brFfb.pDevice);
    BrDiDev(pTmp)->pfnRelease(pTmp);
    if (hr < 0) {
        /* Note: g_brFfb.pDevice is NOT cleared on this path. */
        BrDbgPrint(kBrErrInterface);
        return 0;
    }

    pDev = g_brFfb.pDevice;
    /* pvRef is an integer cooperative level, not a pointer. */
    if (BrDiDev(pDev)->pfnSetCooperativeLevel(pDev, g_brP680584,
            (uint32_t)(uintptr_t)pvRef) < 0) {
        BrDbgPrint(kBrErrCoopLevel);
    } else {
        pDev = g_brFfb.pDevice;
        if (BrDiDev(pDev)->pfnSetDataFormat(pDev, kBrDataFormatJoystick2) >= 0) {
            return 0;                       /* success -- DIENUM_STOP */
        }
        BrDbgPrint(kBrErrDataFormat);
    }

    pDev = g_brFfb.pDevice;
    BrDiDev(pDev)->pfnRelease(pDev);
    g_brFfb.pDevice = NULL;
    return 0;
}

/* 0x100791D0 */
/* WHAT IT DOES: finds the player's wheel or joystick and gets it ready. It
 * looks first for one that can do force feedback, and if it finds one it
 * turns off the wheel's own self-centring (the game supplies its own) and
 * builds the effects; failing that it settles for any controller at all. Then
 * it sets both axes to the range the game expects with no dead zone. It only
 * does the work once no matter how many times it is called. */
/* @implements 0x100791D0 d3d BrFfbInit */
int32_t BrFfbInit(void)
{
    BrDiObj *pDev;
    long hr;

    if (g_brB4E1D0 == 0) {
        return 0;
    }

    g_brFfb.initCount += 1;
    if (g_brFfb.initCount != 1) {
        return g_brB4E1D0;     /* NOT 0 -- see the header */
    }

    if (g_brB4E1E0 != 0 &&
        BrDiRoot(g_pBr18ABD70)->pfnEnumDevices(g_pBr18ABD70, 4u,
            BrFfbEnumDevice, (void *)(uintptr_t)5u, 0x101u) == 0 &&
        g_brFfb.pDevice != NULL) {

        /* pvRef 5 = DISCL_EXCLUSIVE|DISCL_FOREGROUND,
         * flags 0x101 = DIEDFL_ATTACHEDONLY|DIEDFL_FORCEFEEDBACK. */
        BrDiPropDword d;

        g_br18ABDBC = 1;

        d.dwSize       = 0x14u;
        d.dwHeaderSize = 0x10u;
        d.dwObj        = 0u;
        d.dwHow        = 0u;    /* DIPH_DEVICE */
        d.dwData       = 0u;    /* autocentre OFF */

        pDev = g_brFfb.pDevice;
        /* property 9 = DIPROP_AUTOCENTER */
        if (BrDiDev(pDev)->pfnSetProperty(pDev, 9u, &d) < 0) {
            BrDbgPrint(kBrErrProperty);
        }
        (void)BrDiAcquire();
        BrFfbSetup(0x3E8, 0x1F40);
    } else {
        /* pvRef 6 = DISCL_NONEXCLUSIVE|DISCL_FOREGROUND, flags 1. */
        (void)BrDiRoot(g_pBr18ABD70)->pfnEnumDevices(g_pBr18ABD70, 4u,
                 BrFfbEnumDevice, (void *)(uintptr_t)6u, 1u);
        (void)BrDiAcquire();
        g_br18ABDBC = 0;
    }

    pDev = g_brFfb.pDevice;
    if (pDev == NULL) {
        return 0;
    }

    /* Axis 0 (lX): range +-0x80, no dead zone. Then axis 4 (lY), the same.
     * dwHow 1 = DIPH_BYOFFSET; prop 4 = DIPROP_RANGE, 5 = DIPROP_DEADZONE. */
    hr = BrDiSetPropRange(pDev, 4u, 0u, 1u, -0x80, 0x80);
    if (hr < 0) {
        goto failRange;
    }
    pDev = g_brFfb.pDevice;
    hr = BrDiSetPropDword(pDev, 5u, 0u, 1u, 0u);
    if (hr < 0) {
        goto failDword;
    }
    pDev = g_brFfb.pDevice;
    hr = BrDiSetPropRange(pDev, 4u, 4u, 1u, -0x80, 0x80);
    if (hr < 0) {
        goto failRange;
    }
    pDev = g_brFfb.pDevice;
    hr = BrDiSetPropDword(pDev, 5u, 4u, 1u, 0u);
    if (hr < 0) {
        goto failDword;
    }
    return g_brB4E1D0;

failRange:
    BrDbgPrint(kBrErrPropRange);
    goto teardown;
failDword:
    BrDbgPrint(kBrErrPropWord);
teardown:
    pDev = g_brFfb.pDevice;
    BrDiDev(pDev)->pfnUnacquire(pDev);
    pDev = g_brFfb.pDevice;
    BrDiDev(pDev)->pfnRelease(pDev);
    g_brFfb.pDevice = NULL;
    /* NOTE: initCount stays raised. See slice1_10.h. */
    return 0;
}
