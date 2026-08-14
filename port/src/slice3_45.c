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

#include "slice3_45.h"

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

/* ====================================================================== */
/* 1. Entity state setters                                                 */
/* ====================================================================== */

/* 0x10076420 */
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

/* 0x100764C0 */
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
void BrEntSetMatrix(BrEnt *pE, const BrMat4 *pSrc)
{
    /* `rep movsd` of 16 dwords. */
    memcpy(&pE->mat0, pSrc, sizeof(BrMat4));

    BrSub100765E0(pSrc, &pE->st.quat);
    BrEntMirrorQuat(pE);

    BrRbBuildMatrix(&pE->matrix, &pE->st);
}

/* 0x100767A0 */
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

/* 0x10076820 */
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

/* 0x100769A0 */
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

/* 0x10076A00 */
void BrEntRefreshColour(BrEnt *pE)
{
    BrCarGfxSetColour(pE->pRec, pE->r >> 3, pE->g >> 3, pE->b >> 3);
    BrSub10062C50(pE);
}

/* 0x10076A40 */
void BrEntSetRecord(BrEnt *pE, int32_t idx)
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

    BrEntSetVel(pE, 0.0f, 0.0f, 0.0f);

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

/* 0x10078420 */
uint8_t BrInputIsDown(int32_t action)
{
    const BrInputBinding *b = &g_brInput.pBindings[action];
    const uint8_t *keys = g_brInput.aKeys[BR_BUF(g_brInput.iKeyCur)];
    const BrDiJoyState *joy = &g_brInput.aJoy[BR_BUF(g_brInput.iJoyCur)];
    const BrMouseState *mou = &g_brInput.aMouse[BR_BUF(g_brInput.iMouseCur)];
    uint8_t r = 0;

    switch (b->kind0) {
    case BR_BIND_KEY:
        r = (uint8_t)(keys[b->code0] & 0x80u);
        break;
    case BR_BIND_JOYBTN:
        r = (uint8_t)(joy->rgbButtons[b->code0] & 0x80u);
        break;
    case BR_BIND_MOUSEBTN:
        r = (uint8_t)(mou->buttons[b->code0 & 3u] & 0x80u);
        break;
    case BR_BIND_JOYXNEG: if (joy->lX < -BR_BIND_DEADZONE) r = 0x80; break;
    case BR_BIND_JOYXPOS: if (joy->lX >  BR_BIND_DEADZONE) r = 0x80; break;
    case BR_BIND_JOYYNEG: if (joy->lY < -BR_BIND_DEADZONE) r = 0x80; break;
    case BR_BIND_JOYYPOS: if (joy->lY >  BR_BIND_DEADZONE) r = 0x80; break;
    case BR_BIND_JOYZNEG: if (joy->lZ < -BR_BIND_DEADZONE) r = 0x80; break;
    case BR_BIND_JOYZPOS: if (joy->lZ >  BR_BIND_DEADZONE) r = 0x80; break;
    case BR_BIND_MOUXNEG: if (mou->x  < -BR_BIND_DEADZONE) r = 0x80; break;
    case BR_BIND_MOUXPOS: if (mou->x  >  BR_BIND_DEADZONE) r = 0x80; break;
    case BR_BIND_MOUYNEG: if (mou->y  < -BR_BIND_DEADZONE) r = 0x80; break;
    case BR_BIND_MOUYPOS: if (mou->y  >  BR_BIND_DEADZONE) r = 0x80; break;
    case BR_BIND_MOUZNEG: if (mou->z  < -BR_BIND_DEADZONE) r = 0x80; break;
    case BR_BIND_MOUZPOS: if (mou->z  >  BR_BIND_DEADZONE) r = 0x80; break;
    default:
        break;
    }

    if (b->kind1 == 0) {
        r |= (uint8_t)(keys[b->code1] & 0x80u);
    }
    if (b->kind2 == 0) {
        r |= (uint8_t)(keys[b->code2] & 0x80u);
    }
    return r;
}

/* DEVIATION (same as above): the mouse button index is masked with & 3. The
 * original indexes a four-byte field with an unchecked byte. */

/* 0x100786E0 */
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
int32_t BrDiAcquire(void)
{
    BrDiObj *pDev = g_brFfb.pDevice;

    if (pDev == NULL) {
        return 0;
    }
    return (BrDiDev(pDev)->pfnAcquire(pDev) >= 0) ? 1 : 0;
}

/* 0x10078BC0 */
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

    return BrDiDev(pDev)->pfnSetProperty(pDev, prop, &r);
}

/* 0x10078C80 */
long BrDiSetPropDword(BrDiObj *pDev, uint32_t prop, uint32_t dwObj,
                      uint32_t dwHow, uint32_t dwData)
{
    BrDiPropDword d;

    d.dwSize       = 0x14u;
    d.dwHeaderSize = 0x10u;
    d.dwObj        = dwObj;
    d.dwHow        = dwHow;
    d.dwData       = dwData;

    return BrDiDev(pDev)->pfnSetProperty(pDev, prop, &d);
}

/* ====================================================================== */
/* 5. Force feedback                                                       */
/* ====================================================================== */

/* The guard shared by 0x10078E10, 0x10078E50, 0x10078E90, 0x10078ED0 and
 * 0x10078F20. Written out inline in all five; identical every time. */
static int BrFfbEnabled(void)
{
    if (g_brB4E1D0 != 1 && g_brB4E1D0 != 2) {
        return 0;
    }
    if (g_brB4E1E0 == 0) {
        return 0;
    }
    if (g_br18ABDBC == 0) {
        return 0;
    }
    if (g_brFlag6909E0 != 0) {
        return 0;
    }
    return 1;
}

/* 0x10078E10 */
void BrFfbSetDirection(int32_t dir)
{
    if (BrFfbEnabled()) {
        g_br0BD430[0] = dir;
    }
}

/* 0x10078E50 */
void BrFfbSetDurationLong(void)
{
    if (BrFfbEnabled()) {
        g_br0BD438 = 0x3D090;   /* 250000 us */
    }
}

/* 0x10078E90 */
void BrFfbSetDurationShort(void)
{
    if (BrFfbEnabled()) {
        g_br0BD438 = 0x1E848;   /* 125000 us */
    }
}

/* 0x10078ED0 */
void BrFfbCommitDuration(void)
{
    BrDiObj *pEff;

    if (!BrFfbEnabled()) {
        return;
    }
    g_brDiEffSquare.dwDuration = (uint32_t)g_br0BD438;

    pEff = g_brFfb.pEffectSquare;
    if (pEff == NULL) {
        return;
    }
    /* 0x20000041 = DIEP_DURATION | DIEP_DIRECTION | DIEP_START */
    BrDiEff(pEff)->pfnSetParameters(pEff, &g_brDiEffSquare, 0x20000041u);
}

/* 0x100790B0 */
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
    BrDiEff(pEff)->pfnSetParameters(pEff, &g_brDiEffSpring, 0x100u);
}

/* 0x10078F20 */
void BrFfbUpdateSpring(int32_t up, int32_t enable, int32_t decay)
{
    int32_t scaled;
    int32_t rate;
    int32_t cur;
    int32_t before;
    int32_t bound;
    BrDiObj *pEff;

    if (!BrFfbEnabled()) {
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
