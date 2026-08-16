/* br_ai.h -- opponent AI: the racing line, the lookahead, and the aim error.
 *
 * Decompiled from BRGlide.dll. This is the first pass over the AI; it ports
 * the part that could be read exactly and names, without porting, the part
 * that could not. See "WHAT IS NOT HERE" at the bottom.
 *
 *
 * WHERE THE RACING LINE LIVES -- .TRK HEADER +0x70
 * -----------------------------------------------
 * br_track.h lists +0x50..+0x5C, +0x68..+0x78, +0x8C/+0x90 and the block at
 * +0x98 as "widths known, meaning not". Two of those are now settled, and the
 * evidence is the code, not a byte histogram:
 *
 *   +0x70  the root of the AI PATH RING. 0x1005A780 (the last thing
 *          0x100311C0 does after a track loads) passes exactly this field to
 *          0x1005A6E0 / 0x1005A740, the node-tree mark/clear pair that
 *          slice3_40.c already ports as BrNodeMarkPass / BrNodeClearMarkPass
 *          from the D3D addresses 0x10061660 / 0x10061700. slice3_40.h calls
 *          the root `BrG_6C7CB8` (D3D 0x106C7CB8); the Glide global is
 *          0x106EED48, and 0x106EED48 == 0x106EECD8 + 0x70, where 0x106EECD8
 *          is the header buffer 0x100311C0 hands to the swapper. So
 *          `BrG_6C7CB8` IS track header +0x70. Nothing new is allocated for
 *          it: the path is part of the track file.
 *
 *   +0x98  ten 0x14-byte checkpoint gates, count at +0x160.  The same
 *          arithmetic: slice3_40.h's `BrPathSegs` (D3D 0x106C7CE0) and
 *          `BrPathSegCount` (D3D 0x106C7DA8) are Glide 0x106EED70 and
 *          0x106EEE38, i.e. header +0x98 and +0x160. slice3_40.h flagged
 *          "BR_PATH_SEG_MAX is an INFERENCE ... nothing proves the two are
 *          adjacent"; the header proves it -- 0x98 + 10*0x14 == 0x160, and
 *          0x160 is the last dword the swapper reverses before the tail it
 *          leaves alone. See BR_AI_GATE_* below.
 *
 * The two u16 lists at +0x8C/+0x90 are read only by 0x1006EC30 (D3D
 * 0x100759D0), the collision-grid lookup; they are not AI data. +0x50..+0x5C
 * and +0x68..+0x78 are named in the report, not here.
 *
 *
 * THE NODE AND THE POINT
 * ----------------------
 * A node is reached through relocated N64 pointers and carries a run of
 * 0x28-byte path points. The point layout is read off 0x1005D060, the AI's
 * own lookahead scan:
 *
 *      +0x00  BrVec3   left  edge     (0x1005D0E5 `lea ebx,[esi+0x58]` and
 *      +0x0C  BrVec3   centre          0x1005D0E2 `lea eax,[esi+0x40]` are
 *      +0x18  BrVec3   right edge      the two ends of a BrVec3Lerp; the
 *      +0x24  float    arc remaining   0.5*(A+B) it then forms is the centre)
 *
 * The node's own fields, from the same function and from 0x1005D82E:
 *
 *      +0x00  N64 ptr  next node in the ring   (slice3_40.h's f00)
 *      +0x04  N64 ptr  sibling                 (slice3_40.h's f04)
 *      +0x14  u16      point count
 *      +0x16  u16      flags; bit 0 = SKIP     (BR_NODE_FLAG_SKIP)
 *      +0x40  the points, `count` + 1 of them
 *
 * NOTE FOR slice3_40.c: its BrPathPoint starts the array at +0x4C rather than
 * +0x40 and models only two fields. Both fields it uses still land correctly
 * (its `pos` is the centre at +0x0C, its `f18` is the arc length at +0x24)
 * because the array origin and the field offsets shift together, so
 * BrPathWalk / BrPathWalkFrom are NOT wrong. The record simply has two more
 * BrVec3s than that model shows, and the array begins 0x0C earlier.
 * Its "`count` + 1 records" guess is confirmed here: consecutive nodes in
 * race.trk are 0x40 + (count+1)*0x28 bytes apart, exactly.
 *
 *
 * WHAT THE OPPONENT STEERS TOWARD  (0x1005D770, D3D 0x10064700, `shared`)
 * ----------------------------------------------------------------------
 * Every car is the SAME 0x2B68-byte record; only the function pointer at
 * car+0xF08 differs. 0x10019A70 installs it per entrant (0x1001A5CF /
 * 0x1001A60C): slot < the human-player count gets the thunk 0x1005D050 ->
 * 0x1005C8B0, everything else gets 0x1005E690 -> 0x1005D770. So there is no
 * lighter opponent object; there is one object and two controllers.
 *
 * The AI controller's first act is exactly what this header ports:
 *
 *      t   = BrAiLookahead(BrVec3Length(car+0x1024))     0x1005D7D5..0x1005D82C
 *      (node, i) = car->fF8C / car->fF90                 the waypoint cursor
 *      BrAiAdvanceTarget(&node, &i, t)                   0x1005D82E..0x1005D86B
 *      aim = pts[i].centre                               0x1005D875
 *      ... smoothed into car+0xF0C, then
 *      dir = BrAiAimDir(car+0x30, car+0xF0C)             0x1005D9B7 (0x10034420)
 *      lat = dot(car row 1, dir)                         0x1005D9C0 (0x10034310)
 *      fwd = dot(car row 0, dir)                         0x1005D9D2 -> car+0x2728
 *
 * and the control axis it finally writes is the NEGATION of a lateral-derived
 * quantity (0x1005DF26: `fld [esp+0x18]; fchs; fstp [car->p29C0 + 0x20]`),
 * clamped to +-1.0f at 0x1005DFBA / 0x1005DFC9.
 */
#ifndef BR_AI_H
#define BR_AI_H

#include <stdint.h>

#include "br_mat.h"      /* BrMat4 -- the car frame, row-major             */
#include "br_track.h"    /* BrTrack, BrTrackHdrU32                         */
#include "br_vec.h"      /* BrVec3, BrVec3Lerp, BrVec3Dot, BrVec3Length    */

/* ---------------------------------------------------------------------
 * Track header fields this module reads. Offsets, not guesses -- see above.
 * ------------------------------------------------------------------- */
#define BR_TRK_H_AIPATH      0x70u  /* root of the path ring                */
#define BR_TRK_H_AIPATH2     0x74u  /* a second entry point into the ring   */
#define BR_TRK_H_GATES       0x98u  /* BR_AI_GATE_MAX x BR_AI_GATE_STRIDE   */
#define BR_TRK_H_CGATES      0x160u /* how many of them are real            */

#define BR_AI_GATE_STRIDE    0x14u
#define BR_AI_GATE_MAX       10u    /* 0x98 + 10*0x14 == 0x160, exactly     */

/* ---------------------------------------------------------------------
 * The node and the point
 * ------------------------------------------------------------------- */

#define BR_AI_NODE_POINTS    0x40u  /* first point record                   */
#define BR_AI_POINT_STRIDE   0x28u
#define BR_AI_NODE_SKIP      0x0001u /* +0x16 bit 0; slice3_40.h's
                                      * BR_NODE_FLAG_SKIP, same bit         */

/* A cursor onto one node, decoded. This is a VIEW, never an overlay: the
 * image is big-endian N64 data and the two links are N64 addresses, so a
 * struct laid over it would be wrong on every host this port targets. */
typedef struct BrAiNode {
    const BrTrack *pTrack;
    uint32_t       off;      /* file offset of the node                     */
    uint32_t       offNext;  /* +0x00 relocated, 0 for none                 */
    uint32_t       offSib;   /* +0x04 relocated, 0 for none                 */
    uint16_t       count;    /* +0x14                                       */
    uint16_t       flags;    /* +0x16                                       */
} BrAiNode;

typedef struct BrAiPoint {
    BrVec3 left;             /* +0x00 */
    BrVec3 centre;           /* +0x0C */
    BrVec3 right;            /* +0x18 */
    float  arc;              /* +0x24  distance still to run this lap       */
} BrAiPoint;

/* Decode the node at `off`. Returns 0 on success, non-zero if the node or its
 * point array does not fit inside the image. */
int BrAiNodeAt(const BrTrack *pTrack, uint32_t off, BrAiNode *pOut);

/* The path ring's root: header +0x70, relocated. Non-zero if the track has
 * no path, or the field does not address the image. */
int BrAiRoot(const BrTrack *pTrack, BrAiNode *pOut);

/* Point `i` of a node. `i` may be `count` -- the array has count+1 records
 * and the extra one is what makes the original's read of pts[i+1] on the last
 * iteration legal rather than an overrun. Returns 0 on success. */
int BrAiPoint_(const BrAiNode *pNode, uint32_t i, BrAiPoint *pOut);

/* pts[0].arc of the root node: the lap length. 0x10061F60 uses exactly this
 * (`fmul dword ptr [ecx+0x64]` with ecx = header +0x70) to turn a lap count
 * into a distance, and 0x100600A9 adds it to a driver's progress on the lap
 * rollover. Returns 0.0f when the track has no path. */
float BrAiLapLength(const BrTrack *pTrack);

/* ---------------------------------------------------------------------
 * The lookahead  (0x1005D7D5 .. 0x1005D82C)
 * -------------------------------------------------------------------
 *      t = 20.0f + 3.0f * speed        20.0f at 0x100778FC, -3.0f at
 *                                      0x100778F4, combined with fsubr
 *      if (t > 80.0f) t = 80.0f        80.0f at 0x10077900
 *
 * The clamp polarity is the original's and it matters: `fcom` + `test ah,0x41`
 * + `jne` keeps t when the compare says less-or-equal OR UNORDERED, so a NaN
 * speed produces a NaN lookahead rather than 80. Written as `t > 80.0f`
 * below, which is false for NaN, and not as `!(t <= 80.0f)`, which is not.
 *
 * `speed` is |car+0x1024|, taken with BrVec3Length at 0x1005D7DC. That field
 * is not otherwise identified in this port, so the parameter is named for
 * what the arithmetic makes of it, not for what the field is called.
 * ------------------------------------------------------------------- */
#define BR_AI_LOOKAHEAD_BASE   20.0f
#define BR_AI_LOOKAHEAD_GAIN    3.0f
#define BR_AI_LOOKAHEAD_MAX    80.0f

float BrAiLookahead(float speed);

/* ---------------------------------------------------------------------
 * Advancing the waypoint cursor  (0x1005D82E .. 0x1005D86B)
 * -------------------------------------------------------------------
 *      do {
 *          dist -= pts[i].arc - pts[i+1].arc;      one segment
 *          i++;
 *          if (i == node->count) {                 hop to the next node
 *              node = node->next;
 *              while (node->flags & SKIP) node = node->sibling;
 *              i = 0;
 *          }
 *      } while (dist >= 0.0f);
 *
 * Three properties of the original that are easy to lose and are preserved:
 *
 *  - The body runs BEFORE the test, so the cursor ALWAYS advances by at least
 *    one point even for dist == 0. The loop is entered by fallthrough from
 *    the clamp above it, not by a jump to the test.
 *  - The test is `fcom 0.0f` + `test ah,1` + `je <loop>`, i.e. continue while
 *    C0 is clear. C0 is set for less-than AND for unordered, so a NaN dist
 *    ENDS the walk. C's `dist >= 0.0f` is false in both cases and is
 *    therefore exact here.
 *  - It reads pts[i+1] with i == count-1, one record past the count. That is
 *    the sentinel; it is real data, and slice3_40.h records the same read for
 *    BrPathWalk.
 *
 * DEVIATION: the original dereferences node->next and node->sibling with no
 * null check at all. A portable library cannot fault on a truncated file, so
 * this returns non-zero instead and leaves *pNode / *pIndex where the walk
 * got to. On well-formed data the check never fires -- race.trk's ring closes
 * through seventeen nodes with every `next` set.
 * ------------------------------------------------------------------- */
int BrAiAdvanceTarget(BrAiNode *pNode, uint32_t *pIndex, float dist);

/* ---------------------------------------------------------------------
 * The drivable corridor  (0x1005D0D7 / 0x1005D111)
 * -------------------------------------------------------------------
 * 0x1005D060 inset both edges by the same fraction and drives between the
 * results. The constant is 0.2f (0x3E4CCCCD, pushed as an immediate at both
 * sites). Because BrVec3Lerp is (a - b) * t + b, the two calls are not the
 * same function of t and the argument order is load-bearing:
 *
 *      nearRight = BrVec3Lerp(out, left,  right, t)   -> t=0 gives right
 *      nearLeft  = BrVec3Lerp(out, right, left,  t)   -> t=0 gives left
 * ------------------------------------------------------------------- */
#define BR_AI_CORRIDOR_INSET 0.2f

void BrAiCorridor(BrVec3 *pNearLeft, BrVec3 *pNearRight,
                  const BrAiPoint *pPt, float t);

/* ---------------------------------------------------------------------
 * Aiming  (0x1005D9B7 .. 0x1005D9E0)
 * ------------------------------------------------------------------- */

/* 0x10034420 (D3D 0x1003ADA0, `shared`): out = normalise(aim - pos), and
 * exactly (0,0,1) when the two coincide -- 0x100344AC stores 0, 0, 1.0f.
 * The zero test is `fcom 0.0f` + `test ah,0x40`, i.e. C3 alone, so an
 * unordered length also takes the (0,0,1) path. */
void BrAiAimDir(BrVec3 *pOut, const BrVec3 *pPos, const BrVec3 *pAim);

/* The car frame is the 0x44-byte frame at car+0x00: row-major, rows
 * (forward, right, up, position) -- a project-wide fact recorded in the
 * README, and consistent with 0x1005D060 taking car+0x30 as the car's
 * position and 0x10061F60 copying car+0x30..0x38 into a driver record's
 * BrVec3.
 *
 * *pFwd = dot(row 0, dir)   as 0x1005D9D2 computes it (-> car+0x2728)
 * *pLat = dot(row 1, dir)   as 0x1005D9C0 computes it
 *
 * Either out-pointer may be NULL. */
void BrAiAimError(const BrMat4 *pFrame, const BrVec3 *pDir,
                  float *pFwd, float *pLat);

/* Which way the opponent turns for an aim direction.
 *
 * The original's control axis is the NEGATION of a lateral quantity
 * (0x1005DF30 `fchs`), so with row 1 pointing right, a target on the LEFT
 * (lateral component negative) yields a POSITIVE steering command. That sign
 * is what this returns: > 0 means steer left, < 0 means steer right, 0 means
 * dead ahead.
 *
 * ONLY the sign is ported. The magnitude the original writes is the tail of a
 * chain that runs through 0x1005DA37..0x1005DF26 -- a lateral-offset target
 * from the overtaking pass, a divide by a track-width term, and two integer
 * hysteresis counters at car+0xEA0..0xEAC. Transcribing that from a partial
 * x87 trace would link cleanly and be wrong, so it is not here. */
float BrAiSteerDirection(const BrMat4 *pFrame, const BrVec3 *pDir);

/* ---------------------------------------------------------------------
 * WHAT IS NOT HERE, NAMED PLAINLY
 * -------------------------------------------------------------------
 *  - 0x1005D770 (D3D 0x10064700, 3858 B) itself: the opponent controller. Its
 *    target selection is ported above; its throttle/brake law, its
 *    recovery-from-spin timers (car+0xEA0/0xEA4/0xEA8/0xEAC) and its steering
 *    MAGNITUDE are not.
 *  - 0x1005D060 (856 B): the eight-deep corridor lookahead scan. Its point
 *    layout and its 0.2f inset are ported; the scan itself writes eleven
 *    globals per depth level and reaches BrSeg2Intersect, and is left out.
 *  - 0x1005C8B0 (D3D 0x10063840): the human controller, for comparison.
 *  - The difficulty table at 0x100B3018 -- see the report; its consumer
 *    0x1005C490 (D3D 0x10063420) is already flagged in the README as reading
 *    one index clamped and one not.
 *  - The node/section byte-swapper 0x10018B60 that br_track.c also declines.
 *    This module sidesteps it by decoding big-endian on read, which is why
 *    nothing here needs br_track.c to change.
 * ------------------------------------------------------------------- */

#endif /* BR_AI_H */
