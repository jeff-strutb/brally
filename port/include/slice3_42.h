/* slice3_42.h -- decompiled from BRD3D.dll, packet 0x100695D0-0x1006CCD0.
 *
 * Four unrelated things live in this address range:
 *
 *   1. 0x100695D0  quaternion+position -> 4x4 matrix.  Its one caller
 *      (0x10060A10, agent 40's packet) hands it a BrCarState, which PROVES
 *      agent 02's "f00..f0C is almost certainly a quaternion" guess and pins
 *      the component order: f00 is the SCALAR.
 *
 *   2. 0x10069A60-0x10069DE0  the control-binding object at 0x10B4DF30
 *      (the same object slice3_31.h calls `pB4DF30`).  Constructor, copy,
 *      default-table load, one binding setter and two getters.
 *
 *   3. 0x1006AA50-0x1006AD10  the replay recorder: a 12 MiB ring of packed
 *      car states.  The buffer's extent is not a guess -- 0x10B50308 plus
 *      8 * 0x10000 * 24 bytes lands exactly on 0x11750308, the next named
 *      global.
 *
 *   4. 0x1006AEB0-0x1006B510  rigid-body force/torque accumulation and the
 *      "velocity at a point" queries, operating on agent 44's BrRbBody.
 *
 * NOTHING here re-defines a type that already exists: BrVec3/BrMat4 come from
 * br_vec.h/br_mat.h, BrMat3 + BrMat3MulVec3 + BrRbBody from slice3_44.h,
 * BrCarState from slice1_02.h, BrCarPacked + the pack/unpack pair from
 * slice2_12.h.
 */
#ifndef SLICE3_42_H
#define SLICE3_42_H

#include <stdint.h>

#include "br_vec.h"       /* BrVec3                                          */
#include "br_mat.h"       /* BrMat4, BrMat4MulVec3, BrMat4MulVec3Transposed  */
#include "slice1_02.h"    /* BrCarState                                      */
#include "slice2_12.h"    /* BrCarPacked, BrCarStatePack, BrCarStateUnpack   */
#include "slice3_44.h"    /* BrMat3, BrMat3MulVec3, BrRbBody                 */

/* =====================================================================
 * 0. Cross-slice dependencies
 * ===================================================================== */

/* XSLICE 0x100607B0 */
/* Car record -> BrCarState.  Destination first (the original pushes the
 * state pointer last, i.e. it ends up at [esp] as arg1).  Falls inside agent
 * 39's declared range 0x1005AE70-0x100607B0 but is not exported there yet. */
extern void BrCarRecordToState(BrCarState *pDst, void *pCar);

/* XSLICE 0x10060A10 */
/* BrCarState -> car record; the inverse of the above, and the function that
 * calls BrMat4FromCarState below to fill the car's matrix at +0x220.
 * Lives in agent 40's packet. */
extern void BrCarRecordFromState(void *pCar, const BrCarState *pSrc);

/* =====================================================================
 * 1. 0x100695D0 -- BrCarState orientation+position -> BrMat4
 * ===================================================================== */

/* Reads only the first SEVEN floats of pSrc:
 *
 *     f00 = w, f04 = x, f08 = y, f0C = z    (quaternion, SCALAR FIRST)
 *     f10, f14, f18                         (translation)
 *
 * and writes a full 4x4 in the project's row-vector convention (v' = v * M),
 * i.e. the transpose of the usual column-vector rotation matrix, with the
 * translation in ROW 3.  This matches BrRbBuildMatrix (0x10074450) and
 * BrMat4Frustum, so the two matrix builders in this game agree.
 *
 *     s   = 2 / (w^2+x^2+y^2+z^2),  or 0 when that sum is exactly 0
 *
 *     m00 = 1 - s(y^2+z^2)   m01 = s(xy + zw)     m02 = s(xz - yw)   m03 = 0
 *     m10 = s(xy - zw)       m11 = 1 - s(x^2+z^2) m12 = s(yz + xw)   m13 = 0
 *     m20 = s(xz + yw)       m21 = s(yz - xw)     m22 = 1 - s(x^2+y^2) m23=0
 *     m30 = f10              m31 = f14            m32 = f18          m33 = 1
 *
 * GOTCHA: the guard is `if (norm == 0) s = 0`, taken on the x87 EQUAL flag
 * only.  A NaN component therefore does NOT take the guard -- it takes the
 * divide, exactly as in the original.  There is no normalisation: a non-unit
 * quaternion is handled by the 2/norm scale, so the matrix stays a pure
 * rotation.  Contrast BrRbBuildMatrix, which does NOT divide by the norm and
 * so scales the matrix instead.
 *
 * The translation dwords are copied as raw dwords in the original (not as
 * floats), so a NaN or trap representation passes through bit-exact. */
void BrMat4FromCarState(BrMat4 *pOut, const BrCarState *pSrc);

/* =====================================================================
 * 2. The control-binding object at 0x10B4DF30
 * ===================================================================== */

#define BR_CTRL_ACTIONS   28    /* the 0x1C in 0x10069B10 / 0x10069BC0 */
#define BR_CTRL_PROFILES   4    /* four 0xA8-byte blocks at 0, A8, 150, 1F8 */

/* One binding: three alternative sources for one action.  Each entry is
 *
 *     (deviceClass << 8) | code
 *
 * which is exactly what 0x10069B10 builds and what the two getters take
 * apart.  Observed classes in the shipped defaults:
 *
 *     0x00xx   keyboard, xx is a DirectInput scancode (0xCB left, 0x39 space)
 *     0x01xx   joystick button xx
 *     0x80xx.. joystick axis; the axis id is the HIGH byte
 *
 * Slot 0 is the profile's primary source and is the only one the collision
 * scan in 0x10069B10 looks at; slots 1 and 2 are alternates. */
typedef struct BrCtrlProfile {
    uint16_t e[BR_CTRL_ACTIONS][3];
} BrCtrlProfile;                     /* 0xA8 */

/* The shipped defaults, read out of .rdata at 0x100B4098 / 0x100B4140 /
 * 0x100B41E8 / 0x100B4290.  Index == profile index. */
extern const BrCtrlProfile g_BrCtrlDefaults[BR_CTRL_PROFILES];

/* The 0x874-byte object.  Everything past `f2B4` is initialised by
 * 0x10069C90 and copied verbatim by 0x10069DE0 but is never read inside this
 * packet, so the fields keep positional names.
 *
 * PORTABILITY: `pActive` is a pointer, so on a 64-bit host this struct is
 * larger than the original's 0x874 -- the same trade slice2_12.h documents
 * for BrNetSlot.  The offsets are kept in the field names. */
typedef struct BrCtrlCfg {
    BrCtrlProfile  profile[BR_CTRL_PROFILES];  /* 0x000                     */
    int32_t        active;                     /* 0x2A0  profile index      */
    BrCtrlProfile *pActive;                    /* 0x2A4  &profile[active]   */
    int32_t        f2A8, f2AC, f2B0;           /* 0x2A8  ctor writes 1,1,1  */
    uint32_t       f2B4[0x41];                 /* 0x2B4  ctor zeroes        */
    uint32_t       f3B8[0x100];                /* 0x3B8  ctor zeroes        */
    int32_t        f7B8;                       /* 0x7B8  ctor: 0x280 (640)  */
    int32_t        f7BC;                       /* 0x7BC  ctor: 0x1E0 (480)  */
    int32_t        f7C0;                       /* 0x7C0  ctor: 0x10         */
    int32_t        f7C4;                       /* 0x7C4  ctor: 0           */
    uint32_t       f7C8[4];                    /* 0x7C8  ctor zeroes        */
    int32_t        f7D8, f7DC;                 /* 0x7D8  ctor: 9, 9         */
    int32_t        f7E0;                       /* 0x7E0  ctor: 2            */
    int32_t        f7E4, f7E8;                 /* 0x7E4  ctor: 0, 0         */
    int32_t        f7EC, f7F0, f7F4;           /* 0x7EC  ctor: 1, 1, 1      */
    int32_t        f7F8;                       /* 0x7F8  ctor: 0            */
    int32_t        f7FC;                       /* 0x7FC  ctor: 3            */
    int32_t        f800, f804;                 /* 0x800  ctor: 0, 0         */
    int32_t        f808;                       /* 0x808  ctor: 4            */
    int32_t        f80C;                       /* 0x80C  ctor: 0            */
    uint32_t       f810[8];                    /* 0x810  ctor zeroes        */
    uint32_t       f830[16];                   /* 0x830  ctor zeroes        */
    int32_t        f870;                       /* 0x870  ctor: 1            */
} BrCtrlCfg;                                   /* 0x874 */

/* The single global instance at 0x10B4DF30 -- slice3_31.h's `pB4DF30`. */
extern BrCtrlCfg g_BrCtrlCfg;

/* 0x10069C90  in-place construct: all four profiles from the defaults,
 * active = 0, pActive = &profile[0], then the fixed field values listed
 * above.  Note 0x7B8/0x7BC = 640/480. */
void BrCtrlCfgInit(BrCtrlCfg *pThis);

/* 0x10069A90  __thiscall constructor: BrCtrlCfgInit then `return this`. */
BrCtrlCfg *BrCtrlCfgCtor(BrCtrlCfg *pThis);

/* 0x10069A60  a 10-byte thunk: load ecx with 0x10B4DF30 and tail-jump into
 * 0x10069A90, i.e. construct the global.
 *
 * PACKET NOTE: the work order calls this function 30 bytes long.  It is 10.
 * 0x10069A6A-0x10069A6F is `nop` padding and 0x10069A70 is a SEPARATE
 * function (`push 0x10069A80; call 0x1007E8B0` -- an atexit registration for
 * the destructor at 0x10069A80, both outside this packet). */
BrCtrlCfg *BrCtrlCfgInitGlobal(void);

/* 0x10069DE0  __thiscall operator=: copies all 0x874 bytes and then REBUILDS
 * pActive from the copied `active` so it points into *this*, not into pSrc.
 * Returns this, as the original leaves it in eax. */
BrCtrlCfg *BrCtrlCfgCopy(BrCtrlCfg *pThis, const BrCtrlCfg *pSrc);

/* 0x10069AA0  __thiscall: reload ONE profile from its default table.
 *
 * GOTCHA: the selector is dispatched as 1/2/3 with everything else falling
 * through to profile 0 -- so 0, 4, -1 and 99 all reset profile 0. */
void BrCtrlCfgLoadDefaults(BrCtrlCfg *pThis, int32_t profile);

/* 0x10069B10  __thiscall: bind `action` in `profile` to (hi, lo).
 *
 *   profile[action].e[0] = (uint16)((hi & 0xFF00) | (lo & 0xFF))
 *
 * (the original spells that as ((lo ^ hi) & 0xFF) ^ hi, which is the same
 * thing), and then RESTORES slots 1 and 2 from the default table, zeroing
 * either one whose default value collides with any slot-0 value now present
 * in the profile.
 *
 * GOTCHA: only `lo`'s low byte and `hi`'s second byte survive; the rest of
 * both arguments is discarded.  Same profile dispatch as above. */
void BrCtrlCfgAssign(BrCtrlCfg *pThis, int32_t profile, int32_t action,
                     int32_t hi, int32_t lo);

/* 0x10069BC0  __thiscall.  Name and signature taken verbatim from the XSLICE
 * declaration already in slice2_23.h -- do not rename.
 *
 * Returns slot 0 of profile `kind` (dispatched 1/2/3, else 0), action `key`,
 * masked with 0xFF00: the device class, NOT shifted down. */
int32_t BrFn10069BC0(void *pThis, int32_t kind, uint32_t key);

/* 0x10069C30  __thiscall.  Name and signature from slice2_23.h.
 *
 * Returns one byte of the same slot:
 *   kind == 1, 2 or 3 : the HIGH byte when the entry is >= 0x8000 (a joystick
 *                       axis), otherwise the LOW byte;
 *   any other kind    : the LOW byte, with NO 0x8000 test at all.
 *
 * GOTCHA: that asymmetry is real.  Entry 0x8000 answers 0x80 while entry
 * 0x0100 answers 0x00, so a caller cannot reconstruct the entry from the two
 * getters without knowing the class first. */
uint8_t BrFn10069C30(void *pThis, int32_t kind, uint32_t key);

/* =====================================================================
 * 3. Replay recorder
 * ===================================================================== */

#define BR_REPLAY_PLAYERS  8         /* the 8 in 0x1006AA50 / 0x1006ABB0   */
#define BR_REPLAY_FRAMES   0x10000   /* the 0x10000 guard in 0x1006AAB0    */

/* The stride is 24, not 22: the original's index arithmetic is
 * `lea eax,[eax+eax*2]; lea esi,[eax*8+0x10B50308]`.  Two bytes of each slot
 * are never touched. */
typedef struct BrReplaySlot {
    BrCarPacked rec;
    uint8_t     pad[2];
} BrReplaySlot;                      /* 24 */

/* 0x10B50308.  The flat index is (player << 16) + frame, so the players are
 * 0x10000 frames apart whatever BR_REPLAY_FRAMES-worth of them is used. */
extern BrReplaySlot g_BrReplayBuf[BR_REPLAY_PLAYERS * BR_REPLAY_FRAMES];

extern int32_t  g_BrReplayCount[BR_REPLAY_PLAYERS];  /* 0x10B502E8 */
extern int32_t  g_BrReplayOn;                        /* 0x11750308 */
extern int32_t  g_BrReplayCursor[BR_REPLAY_PLAYERS]; /* 0x11750310 */

extern int32_t  g_BrX0AA010;    /* 0x100AA010  session kind; 2 and 4 are 1-player */
extern int32_t  g_BrX06909B4;   /* 0x106909B4  non-zero suspends recording        */
extern int32_t  g_BrX06909E0;   /* 0x106909E0  playback transport state           */
extern uint32_t g_BrX18ABAD0;   /* 0x118ABAD0  button bitmask read by 0x1006AD10  */

/* Byte offsets inside one 0x2B68 car record.  Named, not typed, because
 * slice2_15.h's BrCar names only three of its fields and slice2_17.h already
 * established this convention for the rest. */
#define BR_S42_CAR_OFF_INDEX  0x0140   /* player index; selects the ring    */
#define BR_S42_CAR_OFF_POS    0x01DC   /* three floats                      */
#define BR_S42_CAR_OFF_VEL    0x01E8   /* three floats                      */
#define BR_S42_CAR_OFF_FLAG0  0x0362   /* nine bytes cleared on playback    */
#define BR_S42_CAR_OFF_F0FF4  0x0FF4   /* copied into BrCarState.f78        */

/* 0x1006AA50  zero the per-player frame counts and clear g_BrReplayOn.
 * GOTCHA: it zeroes 8 counts normally but only ONE when g_BrX0AA010 is 2 or
 * 4; counts 1..7 keep their old values in that case. */
void BrReplayReset(void);

/* 0x1006AAB0  append the car's current state to its ring.
 * No-op unless g_BrReplayOn && !g_BrX06909B4 && count < 0x10000.
 * Does NOT advance the count -- BrReplayAdvance does. */
void BrReplayRecord(void *pCar);

/* 0x1006AB20  advance every active frame count (saturating at 0x10000) and,
 * in the 2/4 session kinds only, advance g_BrReplayCursor[1] toward
 * g_BrReplayCount[1] - 1.
 *
 * GOTCHA: in those session kinds the loop above it ran with a length of ONE,
 * so g_BrReplayCount[1] is stale -- yet it is used here as the limit. */
void BrReplayAdvance(void);

/* 0x1006ABB0  zero all eight playback cursors. */
void BrReplayRewind(void);

/* 0x1006ABD0  drive one car from its ring at the current cursor.
 *
 * Unpacks slot [cursor] over the car, then -- if cursor < count - 2 -- also
 * unpacks slot [cursor+1] and writes the finite-difference velocity
 *
 *     car[0x1E8..0x1F0] = (next.f10,f14,f18 - car[0x1DC..0x1E4]) * 30
 *
 * (30 = the simulation rate).  When g_BrX06909E0 == 2 the nine bytes at
 * car+0x362, 0x363, 0x366..0x36C are also cleared.
 *
 * The car's own +0x0FF4 is copied into the unpacked state's f78 before the
 * state is applied, because BrCarStateUnpack is documented to leave f78
 * alone. */
void BrReplayApply(void *pCar, int32_t iPlayer);

/* 0x1006ACF0  BrReplayApply(pCar, car[0x140]). */
void BrReplayApplyCar(void *pCar);

/* 0x1006AD10  read the transport buttons out of g_BrX18ABAD0 and step all
 * eight cursors by the resulting delta, clamped to [0, count-1].
 *
 * Bit -> (state, step):  0x200000 -> (3, +1)   0x400000 -> (3, -1)
 *                        0x800000 -> (3, +10)  0x1000000 -> (3, -10)
 *                        0x100000 -> (1, +1)
 * The 0x800000/0x1000000 pair is tested after and overrides the first pair.
 * If the state ends up 1 the step is forced to 1; if it is 3 with a zero step
 * the state is demoted to 2.
 *
 * GOTCHA: the clamp order is store-then-fix -- the unclamped value is written
 * to the cursor first and only then replaced.  Single-threaded, so harmless,
 * but it is what the original does. */
void BrReplaySeek(void);

/* =====================================================================
 * 4. 0x1006AE20 -- bulk clear of the effect tables
 * ===================================================================== */

/* 600 x 32 bytes at 0x11750338.  The clear loop writes only the first SEVEN
 * dwords of each 32-byte record, so f1C survives; that is not an artefact of
 * the port, the pointer advances by 0x20 while the stores cover 0x1C. */
typedef struct BrFxRecord {
    uint32_t f00, f04, f08, f0C, f10, f14, f18;
    uint32_t f1C;                    /* NOT cleared by BrFxClearAll */
} BrFxRecord;                        /* 0x20 */

#define BR_FX_RECORDS  600           /* 4 * 150; (0x11754E38-0x11750338)/0x20 */
#define BR_FX_PAIRS    200           /* (0x11755490-0x11754E50)/8             */

extern BrFxRecord g_BrFx1750338[BR_FX_RECORDS];    /* 0x11750338 */
extern uint32_t   g_BrFx1754E50[BR_FX_PAIRS][2];   /* 0x11754E50 */
extern int32_t    g_BrX1754E38;                    /* 0x11754E38 */
extern int32_t    g_BrX17554A0, g_BrX17554A4;      /* 0x117554A0 */
extern int32_t    g_BrX17554C8, g_BrX17554CC;      /* 0x117554C8 */
extern int32_t    g_BrX17554D0, g_BrX17554D4;      /* 0x117554D0 */
extern int32_t    g_BrX17554D8, g_BrX17554DC;      /* 0x117554D8 */
extern int32_t    g_BrX17554E0, g_BrX17554E4;      /* 0x117554E0 */

/* 0x1006AE20  clear all of the above. */
void BrFxClearAll(void);

/* =====================================================================
 * 5. Rigid-body force accumulation  (0x1006AEB0 .. 0x1006B510)
 * ===================================================================== */

/* One node of the force list hung off body+0x18. */
typedef struct BrRbForce {
    struct BrRbForce *pNext;   /* 0x00 */
    int32_t           kind;    /* 0x04  0 and 1 select the frame; see below */
    BrVec3            f;       /* 0x08  the force                           */
    BrVec3            r;       /* 0x14  its application point               */
} BrRbForce;                   /* 0x20 */

/* THE SAME OBJECT AS BrRbBody (slice3_44.h), with the fields agent 44 left
 * inside `pad78` and inside the six leading dwords named.  Agent 44 could not
 * see them because 0x10074870 only clears those dwords; here they are used,
 * and four of them are POINTERS to sibling bodies.  That is why this is a
 * separate declaration rather than an edit to slice3_44.h -- the coordinator
 * should fold the two together and keep this layout.
 *
 * Every field below sits at the offset in its comment IN THE ORIGINAL, and
 * the ones agent 44 also names agree exactly: mode 0x1C, mass 0x2C,
 * inertia 0x30, invInertia 0x54, accel 0xFC, angAccel 0x108, f19C, f1B4,
 * f1C0..f1D8, total 0x1DC.
 *
 * PORTABILITY: five pointer fields, so on a 64-bit host neither the offsets
 * nor sizeof survive -- same trade slice2_12.h documents for BrNetSlot.  The
 * padding arrays are sized for the 32-bit layout and are nominal. */
typedef struct BrRbBodyFull {
    float                 f00;          /* 0x00                             */
    struct BrRbBodyFull  *child[4];     /* 0x04, 0x08, 0x0C, 0x10           */
    float                 f14;          /* 0x14                             */
    BrRbForce            *pForces;      /* 0x18  head of the force list     */
    int32_t               mode;         /* 0x1C  2 == "no torque"           */
    float                 dim[3];       /* 0x20                             */
    float                 mass;         /* 0x2C                             */
    BrMat3                inertia;      /* 0x30                             */
    BrMat3                invInertia;   /* 0x54                             */
    BrVec3                f78;          /* 0x78  a point, in world space    */
    BrVec3                vel;          /* 0x84  linear velocity, body frame*/
    float                 f90, f94, f98, f9C;
    BrVec3                angVel;       /* 0xA0  angular velocity, body frm */
    float                 fAC, fB0, fB4, fB8;
    BrMat4                m;            /* 0xBC  ends exactly at 0xFC       */
    BrVec3                accel;        /* 0xFC                             */
    BrVec3                angAccel;     /* 0x108                            */
    unsigned char         pad114[0x19C - 0x114];
    float                 f19C;         /* 0x19C                            */
    unsigned char         pad1A0[0x1B4 - 0x1A0];
    float                 f1B4;         /* 0x1B4  0 disables the torque leg */
    unsigned char         pad1B8[0x1C0 - 0x1B8];
    float                 f1C0, f1C4, f1C8, f1CC, f1D0, f1D4, f1D8;
} BrRbBodyFull;                          /* 0x1DC */

/* Where 0xEC lands inside the pad -- 0xEC..0xF4 is the lever arm
 * BrRbAccumChildForces transforms.  It is inside `m` (which starts at 0xBC),
 * i.e. it is m[3][0..2], the translation row.  That is not a coincidence and
 * not an overlap bug: the original reads the body's own origin. */
#define BR_S42_LEVER_ROW  3

/* 0x1006AEB0  walk pB->pForces and accumulate into pB itself:
 *
 *     kind 0 : v = node->f                        (already in body frame)
 *     kind 1 : v = M^T * node->f                  (world -> body)
 *     other  : v is LEFT OVER from the previous node
 *     pB->accel    += v
 *     if (pB->mode != 2)
 *         pB->angAccel += (M^T * node->r) x v
 *
 * GOTCHA: the `other` case really is a stale read -- the original's stack
 * slot is neither reinitialised nor written on that path, so on the first
 * node it reads uninitialised stack.  See the DEVIATION in the .c. */
void BrRbAccumOwnForces(BrRbBodyFull *pB);

/* 0x1006AFF0  walk pChild->pForces, accumulate the FORCE into the child and
 * the TORQUE into the parent, using the PARENT's matrix throughout:
 *
 *     kind 0 : v = M * node->f                    (M = pParent->m)
 *     kind 1 : v = node->f
 *     other  : v is left over, as above
 *     pChild->accel += v
 *     if (pChild->f1B4 != 0)
 *         pParent->angAccel += (M^T * pChild->m[3]) x (M^T * (v.x, v.y, 0))
 *
 * GOTCHA 1: kinds 0 and 1 mean the OPPOSITE of what they mean in
 * BrRbAccumOwnForces, and the transform is M rather than M^T.
 * GOTCHA 2: the second vector's Z is forced to zero before the transform, so
 * only the in-plane part of the force produces child torque.
 * GOTCHA 3: that transform is computed unconditionally, before the f1B4 test,
 * and thrown away when f1B4 is 0. */
void BrRbAccumChildForces(BrRbBodyFull *pParent, BrRbBodyFull *pChild);

/* 0x1006B170  turn the accumulated force/torque into accelerations:
 *
 *     t          = M * pB->accel
 *     pB->accel  = t                              (stored, then re-read)
 *     t.x = (child[2].x + child[1].x + child[0].x + child[3].x + t.x) / mass
 *     t.y = (child[3].y + child[0].y + child[1].y + child[2].y + t.y) / mass
 *     t.z =  t.z / mass
 *     pB->accel    = M^T * t
 *     pB->angAccel = M^T * (invInertia * (M * pB->angAccel))
 *
 * GOTCHA (the big one): Z NEVER SUMS THE CHILDREN.  X and Y each fold in all
 * four children's accel; Z only divides by the mass.  The two summation
 * orders also differ, which is why they are written out longhand above.
 *
 * All four child pointers are dereferenced unconditionally. */
void BrRbSolveAccel(BrRbBodyFull *pB);

/* 0x1006B260  one full pass: zero pB->accel/angAccel and the four children's
 * accel, BrRbAccumOwnForces(pB), BrRbAccumChildForces(pB, child[k]) for
 * k = 0..3, then BrRbSolveAccel(pB).
 *
 * Note the children's angAccel is NOT zeroed. */
void BrRbAccumAll(BrRbBodyFull *pB);

/* --- "velocity at a point" ------------------------------------------- *
 *
 * All three compute  vel + angVel x (M^T * p)  in the body frame, where M is
 * pB->m and angVel is (fA0, fA4, fA8).  They differ only in where p comes
 * from and whether the answer is rotated back to world.
 */

/* 0x1006B510  p is given directly (world space). */
void BrRbVelAtPoint(BrVec3 *pOut, const BrRbBodyFull *pB, const BrVec3 *pPoint);

/* 0x1006B430  p = pAt->f78.
 * GOTCHA: the original also does a dead `fst` of the Z term into its own
 * frame before storing the real results; there is nothing to read it. */
void BrRbVelAtBodyPoint(BrVec3 *pOut, const BrRbBodyFull *pB,
                        const BrRbBodyFull *pAt);

/* 0x1006B340  p = (pAt->f78.x, pAt->f78.y, 0) -- Z discarded -- and the
 * result is rotated back into world space with M before it is stored.
 *
 * GOTCHA: pOut is used as scratch: the original writes pB->vel into it, reads
 * it back to form the sum, and only then overwrites it with the final
 * product.  So pOut must not alias pB or pAt. */
void BrRbVelAtBodyPointXY(BrVec3 *pOut, const BrRbBodyFull *pB,
                          const BrRbBodyFull *pAt);

#endif /* SLICE3_42_H */
