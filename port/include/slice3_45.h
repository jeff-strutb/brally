/* slice3_45.h -- decompiled from BRD3D.dll, packet 0x10076420-0x100791D0.
 *
 * The packet is two unrelated clusters plus a handful of one-line guarded
 * global stores:
 *
 *   1. Entity ("car instance") state setters
 *        0x10076420 0x100764C0 0x10076700 0x100767A0 0x10076820 0x100769A0
 *        0x10076A00 0x10076A40 0x10076B20
 *
 *   2. DirectInput: binding queries, property helpers, force-feedback
 *        0x100773D0 0x10078420 0x100786E0 0x10078BC0 0x10078C30 0x10078C80
 *        0x10078E10 0x10078E50 0x10078E90 0x10078ED0 0x10078F20 0x100790B0
 *        0x100790E0 0x10079390 0x100791D0
 *
 *   3. One packed-dword global store
 *        0x10077090
 *
 * SKIPPED (see the report and slice3_45.c for the full reasons):
 *   0x10077200  0x10077310  0x100773F0  0x10078DB0
 *
 * ADDRESSES OWNED ELSEWHERE (not duplicated here):
 *   0x10076AE0 0x10076C90            -> slice1_09
 *   0x10076CE0..0x10076FA0 (WINMM)   -> slice1_09 skipped them, with error
 *                                       codes documented in its header
 *   0x10079550 (force-feedback shutdown, the sibling of 0x100791D0 below)
 *                                    -> slice1_10, BrFfbShutdown
 *
 * GENERAL DEVIATION (float): the original is x87 and keeps intermediates in
 * 80-bit registers. This port evaluates in `float`. Nothing in this file does
 * more than one arithmetic operation per store, so the only visible effect is
 * in sinf/cosf themselves.
 *
 * GENERAL DEVIATION (pointer width): several structs below have pointer
 * members at offsets pinned by the 32-bit original. On a 64-bit host the
 * fields after them shift. No code in this file uses absolute offsets, so
 * this is harmless -- but do not `memcpy` these structs from a disk image.
 */
#ifndef SLICE3_45_H
#define SLICE3_45_H

#include <stddef.h>
#include <stdint.h>

#include "br_vec.h"
#include "br_mat.h"
#include "slice1_09.h"   /* BrVec4, BrVec4Normalise (0x100741B0)            */
#include "slice1_10.h"   /* BrDiObj, BrFfb -- the force-feedback globals    */
#include "slice2_19.h"   /* BrCarGfx, BrCarGfxSetColour (0x100350EE)        */
#include "slice3_44.h"   /* BrRbState, BrRbBuildMatrix, BrMat4SetLastColumn */

/* ====================================================================== */
/* 1. The entity / car instance                                            */
/* ====================================================================== */

/* A BrMat4 immediately followed by one float. Six of these live at +0x273C,
 * stride 0x44. Only 0x10076B20 touches them and it does not establish what
 * f40 means beyond "gets pi/6 at reset". */
typedef struct BrEntFrame {
    BrMat4 m;      /* +0x00 */
    float  f40;    /* +0x40 */
} BrEntFrame;      /* 0x44 */

/* The object every 0x10076xxx routine here is a __thiscall member of.
 *
 * WHAT IS ESTABLISHED (and how):
 *
 *   +0x000  a BrMat4 whose translation ROW (m[3], i.e. +0x30) is written by
 *           the position setter and whose upper 3x3 is written by the heading
 *           setter. Row-vector convention, same as BrRbBuildMatrix.
 *   +0x1DC  a BrRbState (slice3_44.h). Pinned three ways: the position setter
 *           writes +0x1DC/1E0/1E4, the velocity setter +0x1E8/1EC/1F0, the
 *           angular-velocity setter +0x204/208/20C, and the orientation
 *           setters +0x1F4..0x200 -- exactly BrRbState's pos/vel/angVel/quat
 *           offsets, and 0x1DC + sizeof(BrRbState) == 0x220.
 *   +0x220  the BrMat4 that BrRbBuildMatrix(+0x220, +0x1DC) fills.
 *   +0x278  a second BrRbState, +0x2BC a third. Adjacent (0x2BC - 0x278 ==
 *           0x44). Every setter mirrors its field into BOTH of them; nothing
 *           in this packet ever reads them back, so which is "previous" and
 *           which is "interpolated" is NOT established.
 *   +0x29AC three colour bytes -- byte-for-byte slice2_19.h's BrRgbSink.
 *   +0x29C4 the BrCarGfx record this instance draws with.
 *
 * Everything else keeps a positional name. */
typedef struct BrEnt {
    BrMat4        mat0;                      /* 0x0000 m[3] mirrors pos    */
    BrMat4        mat40;                     /* 0x0040                     */
    BrMat4        mat80;                     /* 0x0080                     */
    BrMat4        matC0;                     /* 0x00C0                     */
    BrMat4        mat100;                    /* 0x0100                     */
    unsigned char pad140[0x1DC - 0x140];
    BrRbState     st;                        /* 0x01DC                     */
    BrMat4        matrix;                    /* 0x0220 built from st       */
    unsigned char pad260[0x278 - 0x260];
    BrRbState     stA;                       /* 0x0278                     */
    BrRbState     stB;                       /* 0x02BC                     */
    unsigned char pad300[0x340 - 0x300];
    uint32_t      f340[4];                   /* 0x0340 <- rec +0xC8..+0xD4 */
    unsigned char pad350[0xE28 - 0x350];
    uint32_t      fE28[12];                  /* 0x0E28 <- rec +0x98..+0xC4 */
    int32_t       fE58;                      /* 0x0E58 <- (int8)rec +0xD8  */
    int32_t       fE5C;                      /* 0x0E5C <- (int8)rec +0x96  */
    uint32_t      fE60;                      /* 0x0E60 <- fE9C             */
    int32_t       fE64;                      /* 0x0E64 <- (int8)rec +0x97  */
    unsigned char padE68[0xE9C - 0xE68];
    uint32_t      fE9C;                      /* 0x0E9C                     */
    unsigned char padEA0[0xF8C - 0xEA0];
    uint32_t      fF8C;                      /* 0x0F8C cleared by reset    */
    uint32_t      fF90;                      /* 0x0F90 cleared by reset    */
    unsigned char padF94[0x1024 - 0xF94];
    float         f1024[3];                  /* 0x1024 fourth vel mirror   */
    unsigned char pad1030[0x26C8 - 0x1030];
    float         f26C8[3];                  /* 0x26C8 second pos mirror   */
    unsigned char pad26D4[0x2734 - 0x26D4];
    BrEntFrame   *p2734;                     /* 0x2734 <- &aFrames[0]      */
    uint32_t      f2738;                     /* 0x2738 cleared by reset    */
    BrEntFrame    aFrames[6];                /* 0x273C stride 0x44         */
    unsigned char pad28D4[0x29AC - 0x28D4];
    unsigned char r, g, b;                   /* 0x29AC/AD/AE (= BrRgbSink) */
    unsigned char pad29AF[0x29C4 - 0x29AF];
    BrCarGfx     *pRec;                      /* 0x29C4                     */
} BrEnt;

/* 0x10076420  __thiscall, `ret 0xC`. Set the position.
 *
 * Writes (x,y,z) to FIVE places, in this order: mat0.m[3], f26C8, st.pos,
 * stB.pos, stA.pos -- note stB BEFORE stA -- then rebuilds `matrix` from
 * `st` with BrRbBuildMatrix.
 *
 * GOTCHA: mat0's translation row is one of the five. mat0 and `matrix` are
 * different matrices and only `matrix` is regenerated; mat0's upper 3x3 is
 * whatever BrEntSetHeading last left there. */
void BrEntSetPos(BrEnt *pE, float x, float y, float z);

/* 0x100764C0  __thiscall, `ret 4`. Set the orientation from ONE angle
 * (radians) about Z, as both a matrix and a quaternion.
 *
 *   mat0.m[0] = ( cos a, sin a, 0 )
 *   mat0.m[1] = ( cos(a + pi/2), sin(a + pi/2), 0 )   == (-sin a, cos a, 0)
 *   mat0.m[2] = ( 0, 0, 1 )
 *   st.quat   = ( cos(a/2), 0, 0, sin(a/2) )          scalar first
 *   stA.quat = stB.quat = st.quat
 *   BrRbBuildMatrix(&matrix, &st)
 *
 * GOTCHA: row 1 is NOT computed as (-sin a, cos a, 0). The original adds the
 * float nearest -pi/2 (0x1008FCA4 = -1.5707963705062866f) to the angle and
 * calls cosf/sinf again, so row 1 carries the argument-reduction error of two
 * extra transcendental calls. Reproduced exactly -- substituting the identity
 * changes the low bits.
 *
 * GOTCHA: mat0.m[*][3] and mat0.m[3][*] are NOT touched, so the matrix is
 * left with whatever fourth column it had. BrEntReset's BrMat4SetLastColumn
 * is what normally fixes that up. */
void BrEntSetHeading(BrEnt *pE, float a);

/* 0x10076700  __thiscall, `ret 4`. Copy a 0x40-byte matrix into mat0, derive
 * st.quat from it (0x100765E0), mirror the quaternion into stB then stA, and
 * rebuild `matrix` from `st`.
 *
 * GOTCHA: st.pos is NOT updated even though the source matrix's translation
 * row lands in mat0.m[3]. The rebuilt `matrix` therefore keeps the OLD
 * translation with the NEW rotation. */
void BrEntSetMatrix(BrEnt *pE, const BrMat4 *pSrc);

/* 0x100767A0  __thiscall, `ret 0xC`. Set the velocity into FOUR places:
 * st.vel, stB.vel, stA.vel, f1024 -- again stB before stA. No matrix rebuild
 * and no other state touched. */
void BrEntSetVel(BrEnt *pE, float x, float y, float z);

/* 0x10076820  __thiscall, `ret 0xC`. Compose three axis rotations onto the
 * EXISTING st.quat, then normalise and mirror.
 *
 *   st.quat = st.quat (x) (cos(a1/2), 0,        0,        sin(a1/2))   Z
 *   st.quat = st.quat (x) (cos(a2/2), 0,        sin(a2/2), 0       )   Y
 *   st.quat = st.quat (x) (cos(a3/2), sin(a3/2), 0,        0       )   X
 *   BrVec4Normalise(&st.quat)
 *   stB.quat = stA.quat = st.quat
 *
 * (the "(x)" is 0x10074090, whose operand convention is not established
 * here -- see BrSub10074090 below.)
 *
 * GOTCHA: this ACCUMULATES. It does not reset st.quat first, so calling it
 * with (0,0,0) is a no-op only up to the renormalisation.
 *
 * GOTCHA: unlike every other setter here it does NOT call BrRbBuildMatrix.
 * `matrix` is left stale until something else rebuilds it.
 *
 * GOTCHA: the half-angle constant is the float 0.5 at 0x1008FCA8, so the
 * arguments are RADIANS, not degrees. */
void BrEntSetOrientation(BrEnt *pE, float a1, float a2, float a3);

/* 0x100769A0  __thiscall, `ret 0xC`. Set the angular velocity into three
 * places: st.angVel, stB.angVel, stA.angVel. */
void BrEntSetAngVel(BrEnt *pE, float x, float y, float z);

/* 0x10076A00  __thiscall. Push the three colour bytes at +0x29AC down to the
 * graphics record and then call 0x10062C50 on the entity.
 *
 * Each byte is shifted right by 3 -- 8-bit to 5-bit, TRUNCATING, which is the
 * inverse of the replicate-expansion slice2_19.h documents for the readback
 * at 0x10035452. A round trip through both is therefore lossy. */
void BrEntRefreshColour(BrEnt *pE);

/* 0x10076A40  __thiscall, `ret 4`. Point pRec at entry `idx` of the table at
 * 0x100C12A0 and refresh the colour.
 *
 * The stride is built by the original as ((((idx*11) << 6) - idx) << 4 + idx)
 * * 8 == idx * 89992, which is slice2_15.h's BR_HUDSPRITE_STRIDE for the
 * SAME table address. Reproduced as the shift/add chain, in uint32_t, so the
 * wrap-around behaviour on a large index matches.
 *
 * GOTCHA: `idx` is not range-checked at all. */
void BrEntSetRecord(BrEnt *pE, int32_t idx);

/* 0x10076B20  __thiscall. Reset the instance and pull constants out of pRec.
 *
 *   BrMat4SetLastColumn on mat0, aFrames[0..3], aFrames[5], mat40, mat80,
 *     matC0, mat100      -- in that order
 *   aFrames[0..3].f40 = aFrames[5].f40 = pi/6 (0x3F060A92)
 *   p2734 = &aFrames[0]
 *   BrEntSetVel(pE, 0, 0, 0)
 *   fF8C = fF90 = f2738 = 0
 *   fE28[0..11] <- pRec +0x98 .. +0xC4     (12 dwords, contiguous)
 *   fE58        <- (int8) pRec +0xD8
 *   fE60        <- fE9C                    (self-copy)
 *   fE5C        <- (int8) pRec +0x96
 *   f340[0..3]  <- pRec +0xC8 .. +0xD4
 *   fE64        <- (int8) pRec +0x97
 *   pRec        =  NULL
 *
 * GOTCHA (looks like an original bug, preserved): aFrames[4] is SKIPPED.
 * The original walks 0x273C, 0x2780, 0x27C4, 0x2808 and then jumps straight
 * to 0x2890 -- 0x284C never gets its fourth column or its pi/6.
 *
 * GOTCHA: pRec is dereferenced and then NULLED. Calling this twice in a row
 * dereferences NULL the second time. Not guarded -- see the DEVIATION policy.
 *
 * GOTCHA: the three int8 fields are SIGN-extended (`movsx`), so 0x80..0xFF in
 * the record become negative ints. */
void BrEntReset(BrEnt *pE);

/* The table BrEntSetRecord indexes. 0x100C12A0; slice2_15.h calls the same
 * address the "sprite descriptor table" and gives the same 89992 stride. */
/* XSLICE 0x100C12A0 */
extern unsigned char g_aBrC12A0[];

/* 0x100765E0 -- matrix in, BrVec4 out. Almost certainly matrix-to-quaternion
 * (its result is used exactly where BrEntSetHeading writes a quaternion), but
 * that is inference, so the name stays positional. */
/* XSLICE 0x100765E0 */
extern void BrSub100765E0(const BrMat4 *pSrc, BrVec4 *pDst);

/* 0x10074090 -- three BrVec4 arguments. BrEntSetOrientation calls it as
 * f(&q, &q, &local) with `local` a unit quaternion, so arg1 is the
 * destination and arg2/arg3 the operands; the multiplication ORDER is not
 * established from this packet alone. Positional name on purpose. */
/* XSLICE 0x10074090 */
extern void BrSub10074090(BrVec4 *pDst, const BrVec4 *pA, const BrVec4 *pB);

/* 0x10062C50 -- __thiscall on the entity, no other arguments. */
/* XSLICE 0x10062C50 */
extern void BrSub10062C50(BrEnt *pE);

/* ====================================================================== */
/* 2. 0x10077090                                                           */
/* ====================================================================== */

/* 0x10077090  __cdecl, one dword split into two 16-bit halves.
 *
 *   g_br680598 = v;  g_br68059C = v & 0xFFFF;  g_br6805A0 = v >> 16;
 *   if (!(lo != 0 && hi == 0)) tail-jump to the 0x10008B80 stub.
 *
 * GOTCHA: read that condition again. The stub runs when the LOW half is
 * zero, or when both halves are non-zero. The one case that returns without
 * calling it is "low set, high clear" -- which is the case you would expect
 * to be the valid one. Preserved verbatim.
 *
 * GOTCHA: the original TAIL-JUMPS with its argument still on the stack. The
 * stub is a bare `ret` in this build (see CONTRACT.md), so slice2_26.h's
 * zero-argument declaration is used and the argument is simply dropped. */
void BrSet680598(uint32_t v);

extern uint32_t g_br680598;   /* 0x10680598 */
extern uint32_t g_br68059C;   /* 0x1068059C */
extern uint32_t g_br6805A0;   /* 0x106805A0 */

/* XSLICE 0x10008B80 -- a bare `ret` in this build. Declaration copied
 * verbatim from slice2_26.h so the two headers can coexist. */
extern void BrExt_10008B80(void);

/* ====================================================================== */
/* 3. DirectInput: COM shapes                                              */
/* ====================================================================== */

/* slice1_10.h already declares BrDiObj (`{ const BrDiVtbl *pVtbl; }`) and its
 * BrDiVtbl, truncated at slot 8 because that is all BrFfbShutdown needs. The
 * three vtables below are the SAME three interfaces seen further down. They
 * are deliberately given different names rather than redefining BrDiVtbl --
 * the two headers must be includable together. Access them by casting
 * `pObj->pVtbl`, which is what this translation unit does.
 *
 * As in slice1_10.h, what is preserved is the vtable SLOT INDEX, not the byte
 * offset; each comment gives the original x86 offset. */

typedef struct BrDiEffect    BrDiEffect;
typedef struct BrDiPropDword BrDiPropDword;
typedef struct BrDiPropRange BrDiPropRange;

/* The EnumDevices callback shape. Returns 0 (DIENUM_STOP) in every path the
 * original takes. */
typedef int32_t (*BrDiEnumDevicesCb)(const void *pDevInst, void *pvRef);

/* IDirectInputA -- the object at 0x118ABD70. */
typedef struct BrDiRootVtbl {
    void *pfnSlot0;                                             /* +0x00 */
    void *pfnSlot1;                                             /* +0x04 */
    long (*pfnRelease)(BrDiObj *pThis);                         /* +0x08 */
    long (*pfnCreateDevice)(BrDiObj *pThis, const void *rguid,
                            BrDiObj **ppDev, void *pUnkOuter);  /* +0x0C */
    long (*pfnEnumDevices)(BrDiObj *pThis, uint32_t devType,
                           BrDiEnumDevicesCb cb, void *pvRef,
                           uint32_t flags);                     /* +0x10 */
} BrDiRootVtbl;

/* IDirectInputDevice2A. Slot 18 (+0x48, CreateEffect) is what pins the "2". */
typedef struct BrDiDevVtbl {
    long (*pfnQueryInterface)(BrDiObj *pThis, const void *iid,
                              void **ppOut);                    /* +0x00 */
    void *pfnSlot1;                                             /* +0x04 */
    long (*pfnRelease)(BrDiObj *pThis);                         /* +0x08 */
    void *pfnSlot3;                                             /* +0x0C */
    void *pfnSlot4;                                             /* +0x10 */
    void *pfnSlot5;                                             /* +0x14 */
    long (*pfnSetProperty)(BrDiObj *pThis, uint32_t prop,
                           const void *pdiph);                  /* +0x18 */
    long (*pfnAcquire)(BrDiObj *pThis);                         /* +0x1C */
    long (*pfnUnacquire)(BrDiObj *pThis);                       /* +0x20 */
    void *pfnSlot9;                                             /* +0x24 */
    void *pfnSlot10;                                            /* +0x28 */
    long (*pfnSetDataFormat)(BrDiObj *pThis, const void *pdf);  /* +0x2C */
    void *pfnSlot12;                                            /* +0x30 */
    long (*pfnSetCooperativeLevel)(BrDiObj *pThis, void *hwnd,
                                   uint32_t flags);             /* +0x34 */
    void *pfnSlot14;                                            /* +0x38 */
    void *pfnSlot15;                                            /* +0x3C */
    void *pfnSlot16;                                            /* +0x40 */
    void *pfnSlot17;                                            /* +0x44 */
    long (*pfnCreateEffect)(BrDiObj *pThis, const void *rguid,
                            const BrDiEffect *pEff, BrDiObj **ppEff,
                            void *pUnkOuter);                   /* +0x48 */
} BrDiDevVtbl;

/* IDirectInputEffect -- the objects at 0x118ABDEC and 0x118ABDFC. */
typedef struct BrDiEffVtbl {
    void *pfnSlot0;                                             /* +0x00 */
    void *pfnSlot1;                                             /* +0x04 */
    long (*pfnRelease)(BrDiObj *pThis);                         /* +0x08 */
    void *pfnSlot3;                                             /* +0x0C */
    void *pfnSlot4;                                             /* +0x10 */
    void *pfnSlot5;                                             /* +0x14 */
    long (*pfnSetParameters)(BrDiObj *pThis, const BrDiEffect *pEff,
                             uint32_t flags);                   /* +0x18 */
    long (*pfnStart)(BrDiObj *pThis, uint32_t iterations,
                     uint32_t flags);                           /* +0x1C */
    long (*pfnStop)(BrDiObj *pThis);                            /* +0x20 */
} BrDiEffVtbl;

/* DIPROPDWORD / DIPROPRANGE, flattened (the DIPROPHEADER is inlined -- the
 * original builds these as one run of dwords on the stack). */
struct BrDiPropDword {
    uint32_t dwSize;        /* always 0x14 */
    uint32_t dwHeaderSize;  /* always 0x10 */
    uint32_t dwObj;
    uint32_t dwHow;
    uint32_t dwData;
};
struct BrDiPropRange {
    uint32_t dwSize;        /* always 0x18 */
    uint32_t dwHeaderSize;  /* always 0x10 */
    uint32_t dwObj;
    uint32_t dwHow;
    int32_t  lMin;
    int32_t  lMax;
};

/* DIEFFECT, dwSize 0x34 on x86 (13 dwords). */
struct BrDiEffect {
    uint32_t  dwSize;
    uint32_t  dwFlags;
    uint32_t  dwDuration;
    uint32_t  dwSamplePeriod;
    uint32_t  dwGain;
    uint32_t  dwTriggerButton;
    uint32_t  dwTriggerRepeatInterval;
    uint32_t  cAxes;
    uint32_t *rgdwAxes;
    int32_t  *rglDirection;
    void     *lpEnvelope;
    uint32_t  cbTypeSpecificParams;
    void     *lpvTypeSpecificParams;
};

/* DICONDITION (0x18) and DIPERIODIC (0x10). */
typedef struct BrDiCondition {
    int32_t  lOffset;
    int32_t  lPositiveCoefficient;
    int32_t  lNegativeCoefficient;
    uint32_t dwPositiveSaturation;
    uint32_t dwNegativeSaturation;
    int32_t  lDeadBand;
} BrDiCondition;

typedef struct BrDiPeriodic {
    uint32_t dwMagnitude;
    int32_t  lOffset;
    uint32_t dwPhase;
    uint32_t dwPeriod;
} BrDiPeriodic;

/* ====================================================================== */
/* 4. DirectInput: the polled input snapshot                               */
/* ====================================================================== */

/* One binding. The original reads `word ptr [entry]` and masks 0xFF00, so on
 * a little-endian host the low byte is the CODE and the high byte the KIND.
 * Three (code, kind) pairs per binding: a primary of any kind plus two
 * keyboard-only alternates -- alternates 1 and 2 are consulted only when
 * their kind byte is exactly 0. */
typedef struct BrInputBinding {
    uint8_t code0, kind0;
    uint8_t code1, kind1;
    uint8_t code2, kind2;
} BrInputBinding;   /* 6 bytes; 28 of them fill one 0xA8 profile record */

/* The kind byte. Anything not listed reads as "not pressed". */
enum {
    BR_BIND_KEY      = 0x00,  /* code = DirectInput scan code, 0..255       */
    BR_BIND_JOYBTN   = 0x01,  /* code = DIJOYSTATE2 button index, 0..127    */
    BR_BIND_MOUSEBTN = 0x03,  /* code = mouse button index, 0..3            */
    BR_BIND_JOYXNEG  = 0x80, BR_BIND_JOYXPOS = 0x81,
    BR_BIND_JOYYNEG  = 0x82, BR_BIND_JOYYPOS = 0x83,
    BR_BIND_JOYZNEG  = 0x84, BR_BIND_JOYZPOS = 0x85,
    BR_BIND_MOUXNEG  = 0x86, BR_BIND_MOUXPOS = 0x87,
    BR_BIND_MOUYNEG  = 0x88, BR_BIND_MOUYPOS = 0x89,
    BR_BIND_MOUZNEG  = 0x8A, BR_BIND_MOUZPOS = 0x8B
};

/* The axis threshold, +-50. ASYMMETRIC IN THE WRONG DIRECTION: the negative
 * test is `v < -50` and the positive test is `v > 50`, so exactly +-50 is
 * "not pressed" on both sides -- a 101-unit dead band, not 100. */
#define BR_BIND_DEADZONE 50

/* DIJOYSTATE2, the 0x110-byte format c_dfDIJoystick2 (confirmed: the
 * DIDATAFORMAT at 0x1007C7A0 has dwDataSize 0x110 and 164 objects). Only the
 * first three axes and the buttons are read here. */
typedef struct BrDiJoyState {
    int32_t  lX, lY, lZ;                     /* +0x00 +0x04 +0x08 */
    int32_t  lRx, lRy, lRz;                  /* +0x0C +0x10 +0x14 */
    int32_t  rglSlider[2];                   /* +0x18             */
    uint32_t rgdwPOV[4];                     /* +0x20             */
    uint8_t  rgbButtons[128];                /* +0x30             */
    uint8_t  padB0[0x110 - 0xB0];
} BrDiJoyState;

/* The 0x1C-byte per-frame mouse record at 0x118ABD38. The scaled triple and
 * the accumulator triple are separate: the accumulators carry across frames
 * and the scaled values are (accum << 7) / sensitivity, clamped to +-0x80. */
typedef struct BrMouseState {
    int32_t x, y, z;        /* +0x00 +0x04 +0x08  scaled, +-0x80 */
    int32_t ax, ay, az;     /* +0x0C +0x10 +0x14  accumulated    */
    uint8_t buttons[4];     /* +0x18                             */
} BrMouseState;

/* Everything BrInputIsDown / BrInputJustPressed read. The originals are
 * fourteen separate globals; they are gathered here so the pair can be tested
 * without a DirectInput device.
 *
 * COORDINATOR: pBindings aliases slice2_25.h's `void *g_brB4E1D4`, which
 * 0x10043400 points at &g_aBrB4DF30[g_brB4E1D0]. That record is 0xA8 bytes ==
 * exactly 28 BrInputBinding. The two must end up as one object.
 *
 * The `prev` / `cur` split: the frame poll (0x100773F0, skipped) copies the
 * old index to the *Prev global and flips the *Cur one, so the two buffers
 * hold this frame and last frame. That is what makes the rising-edge test in
 * BrInputJustPressed work. */
typedef struct BrInputState {
    const BrInputBinding *pBindings;   /* 0x10B4E1D4 (= g_brB4E1D4)        */
    uint8_t       aKeys[2][256];       /* 0x118AB8B8, two 0x100 buffers    */
    int32_t       iKeyPrev;            /* 0x118AB8B4                       */
    int32_t       iKeyCur;             /* 0x118ABAD8                       */
    BrDiJoyState  aJoy[2];             /* 0x118ABAE0, stride 0x110         */
    int32_t       iJoyPrev;            /* 0x118ABD7C                       */
    int32_t       iJoyCur;             /* 0x118ABAB8                       */
    BrMouseState  aMouse[2];           /* 0x118ABD38, stride 0x1C          */
    int32_t       iMousePrev;          /* 0x118ABAD4                       */
    int32_t       iMouseCur;           /* 0x118ABD80                       */
} BrInputState;

extern BrInputState g_brInput;

/* 0x10078420  __cdecl. "Is action `action` held down THIS frame?"
 *
 * Returns 0 or 0x80 -- never 1. Callers only ever do `test al,al`.
 *
 * The result is the primary binding's state ORed with the two keyboard
 * alternates. An alternate is consulted when its KIND byte is zero, which
 * means code byte 0 with kind byte 0 is a live binding to scan code 0, not
 * an "unbound" marker. There is no unbound encoding.
 *
 * GOTCHA: `action` is not range-checked. */
uint8_t BrInputIsDown(int32_t action);

/* 0x100786E0  __cdecl. "Did action `action` go down on THIS frame?"
 *
 * Rising edge: not set in the previous snapshot, set in the current one.
 *
 * GOTCHA: the return value is not a single truth value. Button and key
 * bindings yield 1; AXIS bindings yield 0x80. Both are non-zero and every
 * caller does `test al,al`, but do not compare the result against 1.
 *
 * GOTCHA: it reads the JOYSTICK through iJoyPrev/iJoyCur and the MOUSE
 * through iMousePrev/iMouseCur, but the keyboard through iKeyPrev/iKeyCur --
 * three independent pairs of indices, and BrInputIsDown uses only the three
 * "cur" ones. Keeping them consistent is the caller's problem. */
uint8_t BrInputJustPressed(int32_t action);

/* ====================================================================== */
/* 5. DirectInput: devices and force feedback                              */
/* ====================================================================== */

/* The force-feedback globals slice1_10.h already names. Defined by this
 * translation unit; slice1_10.c only takes a pointer to one. */
extern BrFfb g_brFfb;

/* The IDirectInput root (0x118ABD70) and the keyboard device (0x118ABDD0)
 * with its own nested-init counter (0x118ABDD8). */
extern BrDiObj *g_pBr18ABD70;
extern BrDiObj *g_pBr18ABDD0;
extern int32_t  g_br18ABDD8;

/* 1 while the force-feedback device holds EXCLUSIVE cooperative level; every
 * FFB entry point below is gated on it. 0x118ABDBC. */
extern int32_t  g_br18ABDBC;

/* The two DIEFFECT descriptors and their type-specific payloads.
 *   g_brDiEffSpring   0x118AB880, GUID_Spring, two DICONDITIONs at 0x118ABD08
 *   g_brDiEffSquare   0x118ABD88, GUID_Square, one DIPERIODIC at 0x118ABAC0
 * g_brDiSpringDir is the spring's rglDirection; NOTHING in the module ever
 * writes it. */
extern BrDiEffect    g_brDiEffSpring;    /* 0x118AB880 */
extern BrDiCondition g_brDiSpringCond[2];/* 0x118ABD08 */
extern int32_t       g_brDiSpringDir[2]; /* 0x118ABDF0 -- never initialised */
extern BrDiEffect    g_brDiEffSquare;    /* 0x118ABD88 */
extern BrDiPeriodic  g_brDiSquarePeriod; /* 0x118ABAC0 */

/* 1 while the spring effect is stopped. 0x118ABDF8. */
extern int32_t  g_br18ABDF8;
/* The spring coefficient currently loaded into the device. 0x118ABD78. */
extern int32_t  g_br18ABD78;

/* Tuning constants in .data, with their shipped values.
 *   g_br0BD424 = 10000   scale numerator for the ramp
 *   g_br0BD428 =  2000   the ramp's lower bound numerator
 *   g_br0BD42C = 10000   the ramp's upper bound numerator (recomputed)
 *   g_br0BD430 = -1, 0   the SQUARE effect's rglDirection
 *   g_br0BD438 = 125000  the SQUARE effect's duration, microseconds
 */
extern int32_t g_br0BD424;   /* 0x100BD424 */
extern int32_t g_br0BD428;   /* 0x100BD428 */
extern int32_t g_br0BD42C;   /* 0x100BD42C */
extern int32_t g_br0BD430[2];/* 0x100BD430 */
extern int32_t g_br0BD438;   /* 0x100BD438 */

/* XSLICE 0x10B4E1D0 -- slice2_25.h. Non-zero enables force feedback; the two
 * values that select a wheel are 1 and 2. */
extern int32_t g_brB4E1D0;
/* XSLICE 0x10B4E1E0 -- slice2_25.h. 0 selects the NON-exclusive fallback. */
extern int32_t g_brB4E1E0;
/* XSLICE 0x106909E0 -- slice2_11.h. Non-zero disables force feedback. */
extern int g_brFlag6909E0;
/* XSLICE 0x10680584 -- slice2_25.h. The HWND handed to SetCooperativeLevel. */
extern void *g_brP680584;

/* DEVIATION: the original hands its failure strings to KERNEL32!
 * OutputDebugStringA. This port routes them through this hook, which is NULL
 * (a no-op) by default. Set it to see the original's diagnostics. */
extern void (*g_pBrDbgPrint)(const char *pMsg);

/* 0x100773D0  __cdecl, no arguments. Acquire the force-feedback device.
 * Returns 1 when the device exists and Acquire returned >= 0, else 0.
 * Returns 0 -- not an error code -- when there is no device at all. */
int32_t BrDiAcquire(void);

/* 0x10078BC0  __cdecl. Release the KEYBOARD device when the last user leaves.
 * The exact shape of slice1_10.h's BrFfbShutdown, over different globals:
 *
 *   g_br18ABDD8--;  if (< 0) { = 0; return; }   <- clamp, NO teardown
 *   if (!= 0) return;
 *   Unacquire, then re-read the global, then Release, then NULL it
 *
 * GOTCHA (same two as BrFfbShutdown, verified independently here): the
 * underflow clamp RETURNS rather than falling through, so a stray extra
 * shutdown is swallowed; and the device pointer is re-read between Unacquire
 * and Release WITHOUT a second NULL test. Both reproduced. */
void BrDiKeyboardShutdown(void);

/* 0x10078C30  __cdecl. Build a DIPROPRANGE on the stack (dwSize 0x18,
 * dwHeaderSize 0x10) and hand it to IDirectInputDevice::SetProperty.
 *
 * `prop` is a MAKEDIPROP integer, not a pointer: the callers pass 4
 * (DIPROP_RANGE), 5 (DIPROP_DEADZONE) and 9 (DIPROP_AUTOCENTER). */
long BrDiSetPropRange(BrDiObj *pDev, uint32_t prop, uint32_t dwObj,
                      uint32_t dwHow, int32_t lMin, int32_t lMax);

/* 0x10078C80  __cdecl. The DIPROPDWORD twin of the above (dwSize 0x14). */
long BrDiSetPropDword(BrDiObj *pDev, uint32_t prop, uint32_t dwObj,
                      uint32_t dwHow, uint32_t dwData);

/* 0x10078E10 / 0x10078E50 / 0x10078E90 / 0x10078ED0 / 0x10078F20 /
 * 0x100790B0 all open with the same four-part guard:
 *
 *   (g_brB4E1D0 == 1 || g_brB4E1D0 == 2) && g_brB4E1E0 && g_br18ABDBC
 *   && !g_brFlag6909E0
 *
 * GOTCHA: g_brB4E1D0 == 3 (or any other non-zero) fails the guard even though
 * BrFfbInit treats every non-zero value as "enabled". The two disagree. */

/* 0x10078E10  set the square effect's direction, g_br0BD430[0]. */
void BrFfbSetDirection(int32_t dir);

/* 0x10078E50  set the square effect's pending duration to 250000 us. */
void BrFfbSetDurationLong(void);

/* 0x10078E90  set it to 125000 us -- the value the .data initialiser has. */
void BrFfbSetDurationShort(void);

/* 0x10078ED0  copy the pending duration into the square DIEFFECT and
 * re-submit it with DIEP_DURATION|DIEP_DIRECTION|DIEP_START (0x20000041),
 * i.e. restart the effect. No-op when the effect does not exist. */
void BrFfbCommitDuration(void);

/* 0x10078F20  __cdecl. The per-frame force-feedback ramp.
 *
 *   scaled = ((decay + 8) * g_br0BD424 * 1000) / 10000    -> g_br0BD42C
 *
 *   if (!enable) {                       (arg2 == 0)
 *       if (already stopped) return;
 *       Stop the spring effect; mark stopped; return;
 *   }
 *   if (was stopped) {
 *       Start the spring effect (1 iteration);
 *       BrFfbSetDurationLong();          <- note: 250000, not 125000
 *       reload scaled/g_br0BD424 from the globals; clear the stopped flag;
 *   }
 *   step  = g_br0BD424 * 1000 / 10000          == g_br0BD424 / 10
 *   if (up)  { g_br18ABD78 -= step; clamp UP   to g_br0BD428 * g_br0BD424 / 10000 }
 *   else     { g_br18ABD78 += step; clamp DOWN to scaled  * g_br0BD424 / 10000 }
 *   if (g_br18ABD78 changed) BrFfbSetSpringCoeff(g_br18ABD78);
 *
 * GOTCHA: `up != 0` DECREMENTS. The sense of arg1 is inverted relative to the
 * direction the coefficient moves.
 *
 * GOTCHA: the two bounds are not a min/max pair around one value -- the
 * decrementing branch clamps to a bound derived from g_br0BD428 and the
 * incrementing branch to one derived from the freshly computed g_br0BD42C.
 * If g_br0BD428 > g_br0BD42C the two clamps cross.
 *
 * GOTCHA: every division here is the compiler's magic-multiply for /10000 on
 * a value that was already truncated to 32 bits. The port does the multiply
 * in uint32_t and the divide in int32_t so the wrap matches. */
void BrFfbUpdateSpring(int32_t up, int32_t enable, int32_t decay);

/* 0x100790B0  __cdecl. Write `coeff` into the FIRST DICONDITION's positive
 * and negative coefficients (0x118ABD0C and 0x118ABD10) and re-submit the
 * spring DIEFFECT with DIEP_TYPESPECIFICPARAMS (0x100).
 *
 * GOTCHA: the second DICONDITION (the second axis) is left at whatever
 * BrFfbSetup put there. The two axes drift apart as soon as this runs.
 *
 * GOTCHA: no guard at all -- unlike its five neighbours this one does not
 * test g_brB4E1D0 / g_brB4E1E0 / g_br18ABDBC / g_brFlag6909E0. It only tests
 * the effect pointer. */
void BrFfbSetSpringCoeff(int32_t coeff);

/* 0x100790E0  __stdcall (`ret 8`), the IDirectInput::EnumDevices callback.
 *
 *   copy 16 bytes from pDevInst + 4                    (the device GUID)
 *   g_pBr18ABD70->CreateDevice(guid, &tmp, NULL)
 *   tmp->QueryInterface(IID_IDirectInputDevice2A, &g_brFfb.pDevice)
 *   tmp->Release()
 *   g_brFfb.pDevice->SetCooperativeLevel(g_brP680584, (uint32_t)pvRef)
 *   g_brFfb.pDevice->SetDataFormat(c_dfDIJoystick2)
 *
 * Returns 0 (DIENUM_STOP) on EVERY path, success included -- it takes the
 * first force-feedback device it is offered and never looks at the rest.
 *
 * GOTCHA: `pvRef` is not a pointer. BrFfbInit passes the integers 5
 * (DISCL_EXCLUSIVE|DISCL_FOREGROUND) and 6 (DISCL_NONEXCLUSIVE|
 * DISCL_FOREGROUND) straight through as the cooperative level.
 *
 * GOTCHA: the CreateDevice out-pointer in the original is the callback's own
 * argument slot -- it overwrites `pDevInst` on the stack. Harmless (the slot
 * is dead and the callee pops), but it is why the disassembly reads
 * [esp+0x18] for something that was an inbound argument.
 *
 * GOTCHA: when QueryInterface fails the original prints and returns WITHOUT
 * NULLing g_brFfb.pDevice. Only the two later failures clean up. */
int32_t BrFfbEnumDevice(const void *pDevInst, void *pvRef);

/* 0x10079390  __cdecl. Fill in both DIEFFECT descriptors and create both
 * effects. Called only by BrFfbInit, with (1000, 8000).
 *
 *   spring conditions = { {0, a, a, 10000, 10000, 0},
 *                         {0, b, b, 10000, 10000, 0} }
 *   spring DIEFFECT   = infinite duration, gain 10000, no trigger, 2 axes
 *                       (offsets 0 and 4), DIEFF_OBJECTOFFSETS|DIEFF_CARTESIAN
 *   -> CreateEffect(GUID_Spring); on success g_br18ABD78 = a and the effect
 *      is marked STOPPED (g_br18ABDF8 = 1)
 *   square periodic   = { 10000, 0, 0, 250000 }
 *   square DIEFFECT   = duration g_br0BD438, gain 10000, 2 axes, direction
 *                       &g_br0BD430
 *   -> CreateEffect(GUID_Square)
 *
 * GOTCHA IN THE ORIGINAL, DEVIATED FROM HERE: both DIEFFECTs' rgdwAxes point
 * at a two-dword buffer that is a LOCAL of this function. It dangles the
 * moment 0x10079390 returns, and BrFfbSetSpringCoeff / BrFfbCommitDuration
 * hand the same descriptors back to SetParameters later. This port stores the
 * axes in a file-scope array instead -- see the DEVIATION in the .c.
 *
 * GOTCHA: the spring's rglDirection points at 0x118ABDF0, which nothing in
 * the module ever writes. It is whatever the loader left there (zero). */
void BrFfbSetup(int32_t springCoeff, int32_t springCoeff2);

/* 0x100791D0  __cdecl, no arguments. The sibling initialiser of slice1_10.h's
 * BrFfbShutdown (0x10079550).
 *
 *   if (g_brB4E1D0 == 0) return 0;
 *   if (++g_brFfb.initCount != 1) return g_brB4E1D0;
 *   ... enumerate, acquire, set ranges and dead zones ...
 *   return g_brB4E1D0;
 *
 * GOTCHA: the two early exits return DIFFERENT values -- 0 when force
 * feedback is disabled, g_brB4E1D0 when it is already initialised. Callers
 * that treat the result as a plain success flag see "disabled" and "already
 * up" as different answers.
 *
 * GOTCHA: this is the asymmetry slice1_10.h note 2 describes from the other
 * side. The counter is incremented ONLY past the g_brB4E1D0 test; the
 * shutdown always decrements. The clamp in BrFfbShutdown is what stops the
 * counter running negative on a machine with force feedback off.
 *
 * GOTCHA: when g_brB4E1E0 is set the exclusive path is tried first, and it
 * falls back to the non-exclusive enumeration if EITHER EnumDevices fails or
 * no device was produced. The fallback re-enumerates -- it does not reuse the
 * device the first pass may have half-built.
 *
 * GOTCHA: the four SetProperty calls are checked with two different failure
 * labels, and BOTH labels Unacquire, Release and NULL the device -- but the
 * counter stays raised. That is exactly the state slice1_10.h describes as
 * "a raised count and a NULL device". */
int32_t BrFfbInit(void);

#endif /* SLICE3_45_H */
