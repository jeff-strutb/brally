/* slice2_11.h -- Boss Rally (BRD3D.dll) decompilation, work packet 11.
 *
 * Address range 0x100011F0 - 0x10005130.  Three clusters live in here:
 *
 *   1. the chase-camera rig that hangs off the end of a car/entity record
 *      (0x100011F0, 0x100015D0, 0x10001760, 0x10001890, 0x10001970,
 *       0x10001FF0)
 *   2. a few small pure helpers (0x100020D0, 0x10002E90, 0x10002F40)
 *   3. CD-audio track stepping (0x10002930/0x10002970/0x100029B0) and the
 *      network car-state send throttle (0x10005130)
 *
 * The Win32-only parts of the packet (MCI cdaudio wrappers, mutex/thread
 * helpers, the SEH-framed packet senders) are deliberately NOT ported; see
 * the pass report for the list and the reasons.
 *
 * ---------------------------------------------------------------------
 * x87 MODELLING NOTE (applies to this whole file)
 * ---------------------------------------------------------------------
 * The original is x87 code compiled by MSVC, whose CRT leaves the FPU
 * precision control at 53 bits.  Intermediates that the original never
 * stores therefore carry more precision than a C `float`.  Everything here
 * uses `float` except where the original compares against an intermediate
 * it has NOT rounded to memory -- there `double` is used, because it is the
 * closer model.  Each such place is marked.
 *
 * The original's comparisons are `fcom`/`fcomp` + `test ah,<mask>`:
 *     test ah,0x01   -> C0    : a <  b  OR unordered
 *     test ah,0x40   -> C3    : a == b  OR unordered
 *     test ah,0x41   -> C0|C3 : a <= b  OR unordered
 * NaN therefore takes the "true" side of every one of these.  Where that is
 * reachable the C is written as a negated comparison (`!(a >= b)` etc.) so
 * the unordered case still lands on the original's branch.  Do not "tidy"
 * those into the positive form.
 */
#ifndef SLICE2_11_H
#define SLICE2_11_H

#include <stdint.h>
#include <stddef.h>

#include "br_vec.h"
#include "slice1_02.h"   /* BrCarState  -- 0xA0 bytes, forty floats */
#include "slice1_08.h"   /* BrCollPlane -- the 32-byte collision-plane record */

/* ==================================================================== */
/* Camera frame                                                         */
/* ==================================================================== */

/* A 0x44-byte record: a 4x4 row-major matrix whose first three columns are
 * the only ones these routines touch, followed by one loose float.
 *
 * The size is fixed by 0x100019D0, which copies the record with
 * `mov ecx,0x11 / rep movsd` (17 dwords = 0x44) three separate times; the
 * row stride is fixed by 0x10001890 and 0x10001CC9, which treat +0x00,
 * +0x10, +0x20 and +0x30 as four independent BrVec3.
 *
 * Roles established by 0x10001890 (which builds them):
 *   f00  forward -- normalise(target - f30)
 *   f10  cross(reference-up, f00)
 *   f20  cross(f00, f10)
 *   f30  world position
 * f40 is written by 0x100019D0 only and is not modelled beyond its slot. */
typedef struct BrCamFrame {
    BrVec3 f00; float f0C;
    BrVec3 f10; float f1C;
    BrVec3 f20; float f2C;
    BrVec3 f30; float f3C;
    float  f40;
} BrCamFrame;

/* ==================================================================== */
/* Car/entity record -- byte offsets, no struct                         */
/* ==================================================================== */

/* The record is the 0x2B68-byte entity of slice1_09.h.  Only the fields
 * these routines touch are named, and they are reached by byte offset for
 * exactly the reason given there: no struct is invented for a record whose
 * layout is 99% unknown. */
#define BR_CAR_OFF_FRAME      0x0000  /* BrCamFrame -- the car's own basis   */
#define BR_CAR_OFF_V204       0x0204  /* BrVec3, magnitude read as a speed   */
#define BR_CAR_OFF_MODE       0x0F78  /* int, set to 2 by 0x10001970         */
#define BR_CAR_OFF_CAMFLAG    0x0F7C  /* int, selects "simple" camera paths  */
#define BR_CAR_OFF_ACTIVECAM  0x2734  /* selected frame -- see below         */
#define BR_CAR_OFF_ACTIVECAM2 0x2738  /* selected frame -- see below         */
#define BR_CAR_OFF_CAM_A      0x273C  /* BrCamFrame                          */
#define BR_CAR_OFF_CAM_B      0x2780  /* BrCamFrame                          */
#define BR_CAR_OFF_CAM_C      0x27C4  /* BrCamFrame                          */
#define BR_CAR_OFF_CAM_D      0x2808  /* BrCamFrame                          */
#define BR_CAR_OFF_SLEW       0x28DC  /* float, slewed +-0.1f per tick       */
#define BR_CAR_OFF_ANCHOR     0x28E0  /* BrVec3, the look-at anchor          */
#define BR_CAR_OFF_PREVPOS    0x28EC  /* BrVec3                              */
#define BR_CAR_OFF_SHAKE      0x28F8  /* float                               */
#define BR_CAR_OFF_V2900      0x2900  /* BrVec3                              */

/* DEVIATION -- the two "active camera" slots.
 *
 * The original stores a 32-bit POINTER in each of +0x2734 and +0x2738, and
 * the only values it ever stores are `this + <one of BR_CAR_OFF_CAM_*>`
 * (0x10001970 stores this+0x2808, 0x10001FF0 stores this+0x2780 or
 * this+0x273C).  A host pointer does not fit in a 4-byte slot, and the two
 * slots are ADJACENT, so writing 8-byte pointers into them would make each
 * write destroy the other -- and the second slot would additionally run
 * over the camera frame at +0x273C.
 *
 * Both slots therefore hold the frame's BYTE OFFSET WITHIN THE RECORD here.
 * That is the same information, keeps the record byte-for-byte identical to
 * the original, and works on any host.  0x100019D0 compares +0x2734 against
 * a frame address; that comparison becomes an offset comparison. */
BrCamFrame *BrCarActiveCam(void *pCar);
BrCamFrame *BrCarActiveCam2(void *pCar);

/* ==================================================================== */
/* Cross-slice dependencies                                             */
/* ==================================================================== */

/* Vector length.  Sits with the br_vec.h cluster but is not declared there.
 * Returns sqrtf(x*x + y*y + z*z); the sum of squares is rounded to float32
 * before the square root is taken. */
/* XSLICE 0x1003B170 */
extern float BrVec3Length(const BrVec3 *pV);

/* Hash a world (x, y) onto one of four resident collision-grid cells,
 * loading the cell if it is not already resident.  Returns the cell slot,
 * sign-extended from 16 bits by every caller. */
/* XSLICE 0x1006F720 */
extern short BrCollGridCellAcquire(float x, float y);

/* sprintf.  Statically linked MSVC CRT. */
/* XSLICE 0x1007C830 */

/* Collision grid: four cells of BR_COLL_CELL_PLANES records each, with the
 * per-cell record count in a parallel u16 array.  The original hardcodes
 * the two bases (0x11750338 and 0x117554A0); they are extern pointers here
 * so the module links and can be tested.
 * DEVIATION: representation only -- the addressing arithmetic is unchanged. */
#define BR_COLL_CELL_PLANES 150
/* XSLICE 0x11750338 */
extern BrCollPlane *g_pBrCollGrid;
/* XSLICE 0x117554A0 */
extern const uint16_t *g_pBrCollGridCount;

/* Set to 0 on entry to BrCamCollideSweep and to 1 only if it moved the
 * camera.  0x100019D0 reads it immediately after the call. */
/* XSLICE 0x100C129C */
extern int g_brCamCollided;

/* Two mode flags read all over this packet.  Neither has an established
 * meaning; both are compared against literals only. */
/* XSLICE 0x100AA010 */
extern int g_brMode0AA010;    /* == 5 short-circuits three camera routines */
/* XSLICE 0x106909E0 */
extern int g_brFlag6909E0;
/* XSLICE 0x100AA8B4 */
extern int g_brMode0AA8B4;    /* == 1 selects -11.0f over -19.8f           */

/* 64x64 u16 grid sampled by BrGrid64Fetch. */
/* XSLICE 0x106C7CB4 */
extern const uint16_t *g_pBrGrid64;

/* u16 payload table indexed by BrU16QueuePop. */
/* XSLICE 0x106C7CB0 */
extern const uint16_t *g_pBrU16QueueTable;

/* CD-audio track bookkeeping. */
/* XSLICE 0x100940A4 */ extern int g_brCdEnabled;    /* 0 == no CD support   */
/* XSLICE 0x10220CD0 */ extern int g_brCdPlaying;
/* XSLICE 0x10220C38 */ extern int g_brCdTrackLast;
/* XSLICE 0x10220C44 */ extern int g_brCdTrackFirst;
/* XSLICE 0x10220CD4 */ extern int g_brCdTrackCur;
/* XSLICE 0x10002910 */ extern int  BrCdTrackGet(void);
/* XSLICE 0x100027C0 */ extern void BrCdTrackPlay(int track);

/* Network send throttle state. */
/* XSLICE 0x10220CF0 */ extern BrCarState g_brNetLastFull;

/* 0x10220D68 -- ALIAS RESOLVED, and it was a live bug.
 *
 * This packet used to declare a free-standing `float g_brNet220D68`. It is
 * not free-standing: 0x10220D68 - 0x10220CF0 == 0x78, and BrCarState::f78 is
 * float #30 -- BR_NET_STAMP, the very field the fast path compares. So the
 * original is comparing the incoming state's stamp against the LAST FULL
 * SNAPSHOT's stamp, which is what the "while the reference has not" wording
 * below always meant.
 *
 * Modelled as a separate object, `g_brNetLastFull = *pState` no longer
 * updated it, so the crossing test stayed true and BrNetCarStateSend sent a
 * full packet on every call for the rest of the race. Two objects, one
 * address, cleanly linked, wrong at runtime -- exactly the failure mode
 * CONVENTIONS.md warns about. Kept as a macro so call sites read the same. */
#define g_brNet220D68 (g_brNetLastFull.f78)
/* XSLICE 0x1022AF40 */ extern int        g_brNetTickCount;
/* XSLICE 0x10094298 */ extern int        g_brNetSendCount;
/* XSLICE 0x10220E60 */ extern float      g_abrNetPeak[7];   /* 0x10220E60..+0x18 */
/* XSLICE 0x10004C60 */ extern int  BrNetSendFull(BrCarState *pState);
/* XSLICE 0x10004E50 */ extern int  BrNetSendDelta(BrCarState *pState,
                                                   BrCarState *pRef);
/* XSLICE 0x100053F0 */ extern void BrNetSendFlush(void);
/* XSLICE 0x10004FC0 */ extern void BrNetKeepAliveTick(void);

/* ==================================================================== */
/* 1. Camera rig                                                        */
/* ==================================================================== */

/* 0x100011F0  __thiscall, ret 8.
 *
 * Sweep the segment that ends at pCam->f30 against the collision triangles
 * of one or two grid cells, TWICE, and pull the camera in front of whatever
 * it would otherwise be inside.
 *
 *   pass 1 starts at the car's look-at anchor (+0x28E0)
 *   pass 2 starts at *pPrevPos (the camera's position last tick)
 *
 * A plane counts only if dot(dir, n) < 0 (front-facing), the hit parameter
 * t is > 0 and closer than the best so far, and the hit point is inside the
 * triangle (BrTriContainsPoint).
 *
 * GOTCHA -- the cell indices are computed ONCE, from the pass-1 endpoints,
 * and reused for pass 2 even though pass 2 starts somewhere else.
 *
 * GOTCHA -- the two passes are textually duplicated in the original and are
 * identical except for two things: pass 1 seeds tBest with
 * (len + 0.1f) / len (i.e. it lets the ray overshoot by a fixed 0.1 world
 * units) while pass 2 seeds it with exactly 1.0f, and pass 2 resets the
 * "best plane" to NULL first, so a pass-1 hit alone does nothing.
 *
 * On a pass-2 hit the camera is placed at hit + n * 0.3f, and is then
 * additionally clamped so it is no further from the anchor than the anchor
 * was from the camera's ORIGINAL position.
 *
 * Sets g_brCamCollided to 0 on entry, to 1 only if it moved the camera. */
void BrCamCollideSweep(void *pCar, BrCamFrame *pCam, const BrVec3 *pPrevPos);

/* 0x100015D0  __thiscall, ret 8.  Place the chase camera.
 *
 * `bias` is a float passed BY VALUE as the second stack argument; it is
 * ignored entirely on the CAMFLAG != 0 path.
 *
 * CAMFLAG != 0 (the "simple" rig):
 *     pCam->f30 = car.f30 + car.f20 * 2.4f
 *     pCam->f30 += car.f00 * (g_brMode0AA8B4 == 1 ? -11.0f : -19.8f)
 *
 * CAMFLAG == 0 (the real rig): builds an ideal camera offset of length
 * 11.0f (or 19.8f) behind the car, lerps the current camera position toward
 * it by `bias`, and re-adds the car position.
 *
 * GOTCHA -- it temporarily subtracts 2.4f from pCam->f30.z on entry and
 * adds it back at the very end, so the height offset is excluded from the
 * distance maths.  The two constants are separate globals (0x1008F014 =
 * +2.4f, 0x1008F020 = -2.4f) and are applied as `-a` then `-b`; they happen
 * to cancel exactly. */
void BrCamPlaceChase(void *pCar, BrCamFrame *pCam, float bias);

/* 0x10001760  __thiscall, no stack arguments.  Update the look-at anchor.
 *
 * CAMFLAG != 0:  anchor = car.f30 + car.f20 * 1.1f, and nothing else.
 *
 * CAMFLAG == 0:  anchor = (car.f30.x, car.f30.y, car.f30.z + 0.66f), then
 * -- unless g_brMode0AA010 == 5, which returns here -- a scalar at +0x28DC
 * is slewed by at most 0.1f per call toward
 *
 *      speed <  3.5 :  2.0f
 *      speed <  7.0 :  4.0f - speed * (4.0f/7.0f)     (continuous at both ends)
 *      otherwise    :  0.0f
 *
 * where speed = |car.v204|, and finally anchor += car.f00 * that scalar.
 *
 * GOTCHA -- the slew never overshoots: whichever of (current +- 0.1f) and
 * the target is nearer the current value is stored.  The comparison is made
 * against the UNROUNDED x87 value, which is why the step is computed in
 * double here. */
void BrCamAnchorUpdate(void *pCar);

/* 0x10001890  __thiscall, ret 4.  Orient a camera frame at the car.
 *
 *   f00 = normalise(car.anchor - pCam->f30)
 *   f10 = cross(up, f00)        up = car.f20 if CAMFLAG != 0, else (0,0,1)
 *   f20 = cross(f00, f10)
 *
 * GOTCHA -- if the camera is exactly on the anchor the normalise is skipped
 * and f00 is left at its previous value; only if that previous value is
 * ALSO zero-length is it replaced (by a straight copy of car.f00).  So a
 * degenerate frame silently keeps stale orientation for one tick. */
void BrCamOrient(void *pCar, BrCamFrame *pCam);

/* 0x10001970  __thiscall, no stack arguments.  Point the active camera at
 * frame D (+0x2808), seat it at
 *      car.f30 + car.f00 * 6.0f + car.f10 * 2.0f + car.f20
 * and set the mode word at +0x0F78 to 2.
 * The frame's orientation is left untouched -- BrCamOrient supplies it. */
void BrCamFrameInitD(void *pCar);

/* 0x10001FF0  __thiscall, no stack arguments.  Reset the chase camera.
 *
 * Selects frame B (+0x2780) when g_brMode0AA010 == 5 and frame A (+0x273C)
 * otherwise, seats B's position at
 *      car.f30 + car.f20 * 4.0f  (+ car.f00 * 10.0f if g_brFlag6909E0)
 *      - car.f00
 * and copies that position into +0x2838 (frame D's position), +0x28EC and
 * +0x2900.  Clears the shake accumulator and seeds the slew scalar at 2.0f.
 *
 * GOTCHA -- both +0x2734 and +0x2738 receive the SAME frame pointer, and
 * the frame that gets its position computed (B) is not necessarily the one
 * that was selected. */
void BrCamFrameInitB(void *pCar);

/* ==================================================================== */
/* 2. Small pure helpers                                                */
/* ==================================================================== */

/* 0x100020D0  __cdecl.  Format a duration as "%d:%02d.%02d".
 *
 * seconds is truncated toward zero after multiplication by 100 (__ftol),
 * then split minutes / seconds / hundredths with signed truncating
 * division, so a negative duration prints with the sign on the minutes and
 * positive seconds/hundredths, e.g. -61.5 -> "-1:-1.-50".  Reproduced.
 *
 * DEVIATION: the original calls sprintf into an unbounded buffer.  A
 * capacity argument is added here and snprintf is used.  The original needs
 * at least 18 bytes for an arbitrary int minute count. */
void BrTimeFormat(char *pDst, size_t cap, float seconds);

/* 0x10002E90  __cdecl.  Sample the 64x64 u16 grid at g_pBrGrid64.
 *
 * Returns 0 unless both indices are in [0, 64).  Otherwise, with
 * a = grid[j*64 + i] and b = grid[j*64 + i + 1]:
 *
 *      result = (((b - a) & 0xFFFF) << 16) | a
 *
 * GOTCHA -- the original computes `b + (a << 16) - a` and only then shifts
 * left by 16, so the `a << 16` term is shifted straight out of the register
 * and contributes nothing.  It is kept in the C for fidelity; do not
 * "simplify" it away and then wonder why the operand order looks odd.
 *
 * GOTCHA -- `b` is read at index+1 with no second bounds check, so
 * i == 63 reads the first element of the next row (and j == 63, i == 63
 * reads one past the grid). */
uint32_t BrGrid64Fetch(int i, int j);

/* 0x10002F40  __cdecl.  Pop one entry from a u16 ring.
 *
 * The two u16 at pQ are a read cursor and a remaining count.  Returns 0 and
 * changes nothing when the count is 0; otherwise returns
 * g_pBrU16QueueTable[head], advances head and decrements count.
 *
 * GOTCHA -- head and count are updated through a single 32-bit register:
 * `((count - 1) << 16) | (head + 1)`.  When head is 0xFFFF the carry out of
 * the low half is OR-ed into the new count, corrupting it.  Reproduced
 * verbatim.
 *
 * GOTCHA -- head is not bounds-checked against the table at all.
 *
 * The original returns in AX only, leaving the high half of EAX holding the
 * table base pointer's high half; the return type is u16 here. */
uint16_t BrU16QueuePop(void *pQ);

/* ==================================================================== */
/* 3. CD-audio track stepping                                           */
/* ==================================================================== */

/* 0x10002930 / 0x10002970 / 0x100029B0.  __cdecl, all return 1 always.
 *
 * All three no-op unless g_brCdEnabled and g_brCdPlaying are both non-zero,
 * then step BrCdTrackGet() by -1 / +1 / +1, store it in g_brCdTrackCur and
 * hand it to BrCdTrackPlay.
 *
 * GOTCHA -- the two "+1" variants differ only in what happens at the top:
 * BrCdTrackNext CLAMPS to g_brCdTrackLast, BrCdTrackNextWrap WRAPS to
 * g_brCdTrackFirst.  Same shape, different sentinel; do not merge them.
 *
 * GOTCHA -- all three write the out-of-range value to g_brCdTrackCur FIRST
 * and only then overwrite it with the clamped one. */
int BrCdTrackPrev(void);
int BrCdTrackNext(void);
int BrCdTrackNextWrap(void);

/* ==================================================================== */
/* 4. Network car-state send throttle                                   */
/* ==================================================================== */

/* 0x10005130  __cdecl.
 *
 * Fast path: if pState->f78 >= 4188888.0f while g_brNet220D68 is below it,
 * snapshot pState into g_brNetLastFull and send a full packet immediately.
 * 4188888.0f (0x4A7FAB60) is a sentinel, not a measurement.
 *
 * Otherwise a 3-tick accumulator runs: for two ticks out of three the seven
 * floats at pState->f80..f98 are folded into a running per-field maximum
 * and the function returns 1 without sending.  On the third tick the maxima
 * are pushed back into pState, the accumulator is cleared, and a packet
 * goes out -- a full one every fourth send, a delta against
 * g_brNetLastFull otherwise.
 *
 * GOTCHA -- only the full-packet path refreshes g_brNetLastFull, so the
 * delta reference is up to four sends old.
 *
 * GOTCHA -- the max-folding comparisons are `fcomp` + `test ah,1`, so a NaN
 * on either side takes the assign branch.  Reproduced. */
int BrNetCarStateSend(BrCarState *pState);

#endif /* SLICE2_11_H */
