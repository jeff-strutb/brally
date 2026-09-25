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

/* kBrNegHalfPi and kBrHalf moved with BrEntSetHeading to
 * src/core/driving/br_entheading.c. */

/* 0x3F060A92, an immediate in 0x10076B20. Bit-exact float(pi/6). */
static const float kBrSixthPi = 0.5235987901687622f;

/* The effect GUIDs, IID_IDirectInputDevice2A, the joystick data format and
 * the debug strings moved with their only users to
 * src/core/controls/br_ffbdev.c. The debug sink below stays: g_pBrDbgPrint is
 * this TU's global, and removing the block moves BrInputIsDown's codegen
 * (TU state) -- br_ffbdev.c carries its own copy of the static sink. */

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

/* BrFfbInit's original caches &KERNEL32!OutputDebugStringA in a register and
 * calls through it (`mov edi,[__imp__]; call edi`, stdcall). The matching arm
 * spells that pointer; the port routes the same local through the safe sink
 * above. */
#ifdef BR_MATCHING_BUILD
__declspec(dllimport) void __stdcall OutputDebugStringA(const char *pMsg);
typedef void (__stdcall *BrDbgSink)(const char *pMsg);
#define BR_DBG_SINK OutputDebugStringA
#else
typedef void (*BrDbgSink)(const char *pMsg);
#define BR_DBG_SINK BrDbgPrint
#endif

/* BrFfbEnumDevice's original calls the import DIRECTLY per site
 * (`call dword ptr [__imp__OutputDebugStringA]`), unlike BrFfbInit's cached
 * register. The port routes the same sites through the safe sink. */
#ifdef BR_MATCHING_BUILD
#define BR_DBG_OUT(msg) OutputDebugStringA(msg)
#else
#define BR_DBG_OUT(msg) BrDbgPrint(msg)
#endif

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
static __inline int32_t BrDiv10000(int32_t v)
{
    return v / 10000;
}

static __inline int32_t BrMulWrap(int32_t a, int32_t b)
{
    return (int32_t)((uint32_t)a * (uint32_t)b);
}

static __inline int32_t BrAddWrap(int32_t a, int32_t b)
{
    return (int32_t)((uint32_t)a + (uint32_t)b);
}

/* Cast helpers for the three COM interfaces. slice1_10.h's BrDiObj carries a
 * `const BrDiVtbl *`; these reinterpret it as the wider vtables declared in
 * slice3_45.h. See the note there -- BrDiVtbl is deliberately not redefined. */
static __inline const BrDiRootVtbl *BrDiRoot(BrDiObj *p)
{
    return (const BrDiRootVtbl *)(const void *)p->pVtbl;
}
static __inline const BrDiDevVtbl *BrDiDev(BrDiObj *p)
{
    return (const BrDiDevVtbl *)(const void *)p->pVtbl;
}
static __inline const BrDiEffVtbl *BrDiEff(BrDiObj *p)
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

/* 0x100764C0 BrEntSetHeading is filed in src/core/driving/br_entheading.c. */

/* 0x10076700 BrEntSetMatrix, 0x100767A0 BrEntSetVel, 0x10076820
 * BrEntSetOrientation and 0x100769A0 BrEntSetAngVel -- with the quaternion
 * mirror they share -- now live in src/core/driving/br_entstate.c. */


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
/* @t4-pass 0x10071710 1 2026-09-07 probes 73 bytes 685 insns 183 regions 8 rows 27 census yes  (tools/crank.py) */
/* @t4-pass 0x10071710 2 2026-09-07 probes 73 bytes 685 insns 183 regions 8 rows 27 census yes  (tools/crank.py) */
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
/* port-only body; Glide match is src/core/controls/br_inputpoll.c */
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

/* 4. DirectInput devices -- BrDiAcquire, BrDiKeyboardShutdown,
 * BrFfbUpdateSpring, BrFfbSetup, BrFfbEnumDevice and BrFfbInit are filed in
 * src/core/controls/br_ffbdev.c (the inline helpers above are copied there). */
