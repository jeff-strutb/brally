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
 * (0x1005DF30 `fchs`), so a target on the -row1 side yields a POSITIVE
 * steering command and a target on the +row1 side a negative one. That sign
 * is what this returns; 0 means dead ahead.
 *
 * CORRECTION TO THE PROSE THIS COMMENT USED TO CARRY. It said "with row 1
 * pointing right, a target on the LEFT ... yields a POSITIVE steering
 * command", taking "row 1 is right" from the README's frame note. The image
 * says what row 1 IS, and it is not a naming convention:
 *
 *     0x1006F249  mov [esi+0x20], 0 / 0 / 1.0f     ; row 2 := (0,0,1)
 *     0x1006F255  cross(out = row1, a = row2, b = row0)
 *
 * so **row 1 == row2 x row0 == up x forward**. That is exactly the same
 * construction the AI uses for the PATH's lateral axis (BrAiPathFrame below,
 * `lateral = up x tangent`), which is why the correction law closes: the
 * lateral error and the aim error are measured along the same axis without
 * anyone having to know which way it points on screen. Whether up x forward
 * is "left" or "right" to a viewer is a question about the world's visual
 * handedness that nothing in this port pins down, and nothing here needs it.
 *
 * The MAGNITUDE is now ported too -- see BrAiSteerCompute below. This
 * function remains as the bare sign, and the existing tests for it remain
 * valid: they assert a relation to row 1, not to a compass direction. */
float BrAiSteerDirection(const BrMat4 *pFrame, const BrVec3 *pDir);

/* =====================================================================
 * THE OPPONENT CONTROLLER'S SECOND HALF  (0x1005D770, 3858 B)
 * =====================================================================
 *
 * Everything below is transcribed from one function. The order it appears in
 * is the order the original runs it. Every float constant carries the address
 * it was read from in BRGlide.dll; the four `qword` ones are DOUBLES in the
 * image and are compared in double precision, which is preserved.
 *
 * The vector leaves it calls are all already ported in br_vec.c under their
 * D3D addresses -- 0x100342B0/0x1003AC30 cross, 0x10034310/0x1003AC90 dot,
 * 0x10034560/0x1003AEE0 sub, 0x10034620/0x1003AFA0 lerp, 0x100346A0/0x1003B020
 * mul-add, 0x100346D0/0x1003B050 midpoint, 0x100347F0/0x1003B170 length. They
 * are reused, not re-derived. The one exception is the guarded normalise
 * 0x100344D0 (D3D 0x1003AE50); see br_ai.c for why that one is copied.
 * ===================================================================== */

/* ---------------------------------------------------------------------
 * 1. THE PATH FRAME AT THE TARGET  (0x1005DA05 .. 0x1005DA79)
 * -------------------------------------------------------------------
 * An orthonormal frame built at the waypoint the cursor is sitting on. The
 * original keeps it in the car record and this struct is those three fields:
 *
 *      tangent  car+0xF24   normalise(pts[i+1].centre - pts[i].centre)
 *      lateral  car+0xF30   normalise(up x tangent)
 *      up       car+0xF3C   normalise(tangent x (pts[i].left - pts[i].right))
 *
 * built in that order -- `up` first (0x1005DA3A + 0x1005DA5A), then `lateral`
 * over the top of the raw edge vector (0x1005DA6B + 0x1005DA74).
 *
 * TWO MEASURED FACTS, not assumptions:
 *
 *  - `up` comes out +Z on EVERY path point of BOTH shipped tracks (119/119 in
 *    race.trk, 366/366 in desert.trk). So the point's +0x00 edge is the one on
 *    the (up x tangent) side and `lateral` points at it; 0x1005DB34's
 *    half-width dot(lateral, left - centre) is consequently POSITIVE, which is
 *    what the corridor law below needs it to be.
 *  - `lateral` and the car frame's row 1 are the same construction (up x
 *    forward, see BrAiSteerDirection), so they agree in orientation whenever
 *    the car is roughly aligned with the path. That is the hinge the whole
 *    correction law turns on.
 *
 * pNext is pts[i+1], which is legal for every i the cursor can hold: the walk
 * leaves i < count and the array has count+1 records. */
typedef struct BrAiPathFrame {
    BrVec3 tangent;          /* car+0xF24 */
    BrVec3 lateral;          /* car+0xF30 */
    BrVec3 up;               /* car+0xF3C */
} BrAiPathFrame;

void BrAiPathFrameAt(BrAiPathFrame *pOut,
                     const BrAiPoint *pPt, const BrAiPoint *pNext);

/* ---------------------------------------------------------------------
 * 2. THE SIGNED LINE OFFSET  (0x1005DA79 .. 0x1005DAC0)
 * -------------------------------------------------------------------
 *      d      = pos - pts[i].centre               0x1005DA98  (-> car+0xF4C)
 *      d     += vel * 0.4f                        0x1005DAAD  (0x3ECCCCCD)
 *      offset = dot(lateral, d)                   0x1005DAB7
 *
 * Positive means the car is displaced toward the point's +0x00 edge. The 0.4f
 * is a velocity LEAD -- the controller steers for where the car will be, not
 * where it is -- and it is the same immediate the aim smoothing uses.
 * ------------------------------------------------------------------- */
#define BR_AI_LEAD 0.4f

float BrAiLineOffset(const BrAiPathFrame *pFrm, const BrVec3 *pPos,
                     const BrVec3 *pVel, const BrVec3 *pCentre);

/* The heading term, 0x1005DACD: dot(car frame row 0, lateral). How far the
 * car's nose has swung off the path's direction, measured on the same axis as
 * the offset. */
float BrAiHeading(const BrMat4 *pFrame, const BrAiPathFrame *pFrm);

/* ---------------------------------------------------------------------
 * 3. THE CORRIDOR  (0x1005DB0A .. 0x1005DB68)
 * -------------------------------------------------------------------
 *      halfWidth = dot(lateral, pts[i].left - pts[i].centre)   0x1005DB3D
 *      limit     = halfWidth > 5.0f ? halfWidth - 3.0f         0x1005DB5A
 *                                   : halfWidth * 0.4f         0x1005DB62
 *
 * 5.0f is 0x10077908, 3.0f is 0x100778DC, 0.4f is 0x1007790C. The compare is
 * `fcom` + `test ah,0x41` + `jne <the *0.4 arm>`, so <= AND UNORDERED take the
 * multiply, which `halfWidth > 5.0f` reproduces exactly.
 *
 * A car with |offset| STRICTLY GREATER than `limit` is corrected (rule 4); one
 * at or inside it is merely held (rule 5). The equality lands on `hold`
 * because 0x1005DB6C tests C0|C3 and C3 is set for equal.
 * ------------------------------------------------------------------- */
float BrAiHalfWidth(const BrAiPathFrame *pFrm, const BrAiPoint *pPt);
float BrAiCorridorLimit(float halfWidth);

/* ---------------------------------------------------------------------
 * 4. THE CORRECTION LAW  (0x1005DB79.., 0x1005E39F, 0x1005E3F1, 0x1005E43B)
 * -------------------------------------------------------------------
 * Two outputs: a MAGNITUDE and a requested DIRECTION. The direction lives in
 * the original as two independent globals, 0x10B1CBE8 and 0x10B1CF04, which
 * are zeroed together at 0x1005DAE5 and thereafter set only in matched pairs
 * -- never both, sometimes neither. They are modelled as one enum, which is
 * faithful because every consumer reads 0x10B1CBE8 first and only falls
 * through to 0x10B1CF04 when it is zero (0x1005DDF1, 0x1005DE6A).
 *
 * With s = (|offset| - limit) / |offset|, and taking `offset` positive to mean
 * displaced toward `lateral`:
 *
 *   offset > 0    heading > -0.05   BIAS_NEG  s * (-0.2 * heading) + 0.03
 *                 -0.15..-0.05      BIAS_NONE 0            (a dead band)
 *                 heading < -0.15   BIAS_POS  s * (-0.3 * heading) + 0.1
 *   offset <= 0   heading <  0.05   BIAS_POS  s * ( 0.2 * heading) + 0.03
 *                  0.05.. 0.15      BIAS_NONE 0
 *                 heading >  0.15   BIAS_NEG  s * ( 0.3 * heading) + 0.1
 *
 * The bias always points AWAY from the side the car is displaced to, except
 * in the outer arms, where the car is already swinging back hard and the bias
 * flips to damp the swing. -0.05/-0.15/0.05/0.15 are the doubles at
 * 0x10077910/0x10077920/0x10077930/0x10077938; -0.2/-0.3/0.2/0.3 are
 * 0x10077918/0x10077928/0x100778E0/0x10077940; the two offsets are `fsub` of
 * -0.03 (0x1007791C) and -0.1 (0x100778AC).
 *
 * NaN polarity, and it is not uniform: the two inner tests are `test ah,0x41`
 * + `jne`, so a NaN heading FAILS them and falls into the outer arm; the two
 * outer tests are `test ah,1` + `je`, so a NaN heading PASSES them. A NaN
 * heading therefore takes the -0.3 / +0.3 arm on both sides, and the negated
 * spellings below are what produce that.
 * ------------------------------------------------------------------- */
typedef enum BrAiBias {
    BR_AI_BIAS_NONE = 0,
    BR_AI_BIAS_POS  = 1,     /* 0x10B1CBE8 -- bias the aim toward +lateral */
    BR_AI_BIAS_NEG  = 2      /* 0x10B1CF04 -- bias the aim toward -lateral */
} BrAiBias;

float BrAiSteerCorrection(float offset, float heading, float limit,
                          BrAiBias *pBias);

/* ---------------------------------------------------------------------
 * 5. THE IN-CORRIDOR ARBITRATION  (0x1005E48D .. 0x1005E67D)
 * -------------------------------------------------------------------
 * Reached when |offset| <= limit. Below 10.0f of speed (0x10077898) it does
 * nothing at all. Above it, three quantities are reduced to a tri-state each
 * by the SAME +-0.1 dead band (0x100778A4 / 0x100778AC), at 0x1005E4B6,
 * 0x1005E4F3 and 0x1005E52B:
 *
 *      sVel  sign3(dot(velocity,      lateral))          0x1005E4B1
 *      sAux  sign3(dot(car+0x40,      up x aimDir))      0x1005E4EE
 *      sFwd  sign3(dot(car frame row0, up x aimDir))     0x1005E526
 *
 * `up x aimDir` is the local at 0x1005D995: cross((0,0,1), aimDir). NOTE the
 * opposite sense of the two families -- `lateral` is up x tangent and points
 * ALONG the path's own axis, while up x aimDir points along the axis of the
 * direction the car is aiming, so sVel and sFwd having the SAME sign means the
 * car is drifting and aiming in OPPOSITE directions.
 *
 * car+0x40 is not identified. It is three floats immediately past the 4x4 of
 * the 0x44-byte frame at car+0x00, so it is not a row of that matrix, and
 * nothing else in this function touches it. It is a parameter here rather
 * than a guess.
 *
 * The decision tree is transcribed literally in br_ai.c. Its magnitudes are
 * 0.1f (0x3DCCCCCD), 0.4f (0x3ECCCCCD) and 0.5f (0x3F000000) as immediates,
 * and several of its arms fall through to 0x1005E671, which CLEARS the
 * 0x10000 control bit -- reported through *pfCutThrottle.
 * ------------------------------------------------------------------- */

/* +1 when v > 0.1f, -1 when v < -0.1f, 0 between. NaN yields -1: the first
 * test is `fcom` + `test ah,0x41` + `jne`, which an unordered compare takes,
 * and the second is `fcomp` + `test ah,1` + `je`, which it does not. */
int BrAiSign3(float v);

float BrAiSteerHold(float speed, int sVel, int sAux, int sFwd,
                    BrAiBias *pBias, int *pfCutThrottle);

/* ---------------------------------------------------------------------
 * 6. THE AIM ERROR, SCALED AND CLAMPED  (0x1005DD52 .. 0x1005DDB4)
 * -------------------------------------------------------------------
 *      if (speed > 10.0f)  p = clamp(scale * lat, -1, +1)
 *      else                p = lat
 *
 * `lat` is dot(row 1, aimDir) -- BrAiAimError's lateral output. `scale` comes
 * out of the throttle ladder (rule 8) and is 1.0, 1.3 or 2.0, so a car that
 * has been told to brake also steers harder for the same aim error.
 *
 * THE CLAMP IS THE TRAP THE BRIEF WARNS ABOUT AND IT IS REPRODUCED. The upper
 * arm is `fcom 1.0` + `test ah,0x41` + `jne`, so it clamps only on an ordered
 * greater-than; the lower arm is `fcom -1.0` + `test ah,1` + `je <keep>`, so
 * it clamps on less-than OR UNORDERED. A NaN aim error therefore comes out as
 * -1.0f, full lock one way, not as NaN and not as +1.0f. Either clamp arm
 * also clears the 0x10000 control bit (0x1005DDA8).
 * ------------------------------------------------------------------- */
float BrAiSteerInput(float scale, float lat, float speed, int *pfCutThrottle);

/* ---------------------------------------------------------------------
 * 7. THE RESPONSE CURVE  (0x1005DDB4 .. 0x1005DEC6)
 * -------------------------------------------------------------------
 *      if (mag < 0.01f) mag = 0.01f            0x1005DDC5 (0x3C23D70A)
 *      k = 0.2f / mag                          0x1005DDCD, 0x100778E0
 *
 *      p >= 0:  BIAS_POS: p' = p > k ? p * (mag + 1) : p + 0.2f
 *               BIAS_NEG: p' = p > k ? p - mag * p   : p - 0.2f
 *               NONE:     p' = p
 *               out = 1 - (1 - p')^4
 *      p <  0:  BIAS_POS: p' = p < -k ? p - mag * p   : p + 0.2f
 *               BIAS_NEG: p' = p < -k ? p * (mag + 1) : p - 0.2f
 *               NONE:     p' = p
 *               out = (p' + 1)^4 - 1
 *
 * Both halves are the same odd-symmetric quartic, spelled twice because the
 * original spells it twice (0x1005DE53 and 0x1005DEB8, three `fmul st(0),st(1)`
 * apiece over a `fld st(0)` -- a FOURTH power, not a cube; the leftover copy is
 * what the extra `fstp st(0)` at 0x1005DECB pops). It maps [-1,1] onto [-1,1]
 * and is strongly expansive near zero: 0.1 in gives 0.34 out.
 *
 * BIAS_POS always moves p' up and BIAS_NEG always moves it down, on both sides
 * of zero -- that uniformity is what makes the enum in rule 4 meaningful, and
 * it is worth checking against the four arms above because the operand order
 * inverts between them.
 *
 * `k` is a magnitude-inverse gate: a WEAK request (mag near 0.01) gives k = 20,
 * which |p| <= 1 can never exceed, so a weak request always takes the additive
 * +-0.2 nudge and never the multiplicative one.
 *
 * TWO ROUGH EDGES, both the original's and both pinned by the tests:
 *
 *  - THE QUARTIC FOLDS. Each half is monotone only on its own side of its
 *    fold: (p'+1)^4 - 1 turns around at p' = -1 and 1 - (1-p')^4 at p' = +1.
 *    The bias can push p' past those -- with mag 0.5 and p = -0.9, BIAS_NEG
 *    gives p' = -1.35 and an output of -0.985, which is HIGHER than the
 *    unbiased -0.9999. So "POS raises, NEG lowers" holds only while
 *    |p'| <= 1, i.e. |p| <= 1/(1 + mag). It is not a bug to be smoothed out;
 *    it is what the four `fmul st(0),st(1)` chains compute.
 *  - IT IS DISCONTINUOUS AT p == 0. The branch is `fcomp 0.0` + `test ah,1`
 *    + `je`, so p == 0 takes the p >= 0 arm, where a BIAS_NEG nudge of -0.2
 *    lands on the far side of that arm's fold. The result jumps from -0.5904
 *    just below zero to -1.0736 at zero. The SIGN is continuous -- both mean
 *    "steer toward -lateral" -- so the control loop still closes; only the
 *    magnitude steps, and it steps past the nominal +-1.
 *
 * The value the original stores in car->p29C0->f20 is the NEGATION of this
 * (0x1005DF30) -- but only on the forward-drive path; see rule 9.
 * ------------------------------------------------------------------- */
float BrAiSteerResponse(float p, float mag, BrAiBias bias);

/* The whole of rules 3..7 as the original sequences them. `value` is what
 * 0x1005DF32 writes; `curve` is what 0x1005DEC6 leaves before the negation,
 * which rule 9's reverse arm uses the SIGN of rather than the value. */
typedef struct BrAiSteerIn {
    float lat;        /* dot(row 1, aimDir)             0x1005D9C0 */
    float offset;     /* BrAiLineOffset                 0x1005DAB7 */
    float heading;    /* BrAiHeading                    0x1005DACD */
    float limit;      /* BrAiCorridorLimit              0x1005DB5A */
    float speed;      /* |car+0x1024|                   0x1005DD59 */
    float scale;      /* the throttle ladder's output   0x1005DBE2 */
    int   sVel, sAux, sFwd;             /* rule 5's three tri-states */
} BrAiSteerIn;

typedef struct BrAiSteerOut {
    float    value;        /* -> car->p29C0->f20                      */
    float    curve;        /* the un-negated quartic output           */
    float    mag;          /* after the 0.01f floor                   */
    BrAiBias bias;
    int      fCutThrottle; /* the 0x10000 bit is cleared this frame   */
} BrAiSteerOut;

void BrAiSteerCompute(BrAiSteerOut *pOut, const BrAiSteerIn *pIn);

/* ---------------------------------------------------------------------
 * 8. THE THROTTLE LADDER  (0x1005DBDD .. 0x1005DD52)
 * -------------------------------------------------------------------
 * This is the whole of the throttle/brake law, and it lives inside 0x1005D770;
 * NEITHER of the two shared helpers is any part of it (see the tail of this
 * header for what they turn out to be).
 *
 * The loop walks 0x10AC680C levels of the corridor scan's output. Level i
 * (ONE-BASED -- 0x1005DBF2 seeds the counter at 1 and the array index trails
 * it by one) is a triangle {A[i], B[i], B[i+1]} taken from two globals with a
 * 0x0C stride: A at 0x10B1C9B8 and B at 0x10B1CA28. The third corner is read
 * from 0x10B1CA34, which is 0x10B1CA28 + 0x0C -- the SAME array, one element
 * on -- so the levels form a strip, not three parallel lists.
 *
 *      n  = normalise((B[i+1] - B[i]) x (A[i] - B[i]))     0x1005DC3A
 *      q  = max(|0.03f * dot(vel, A[i] - B[i])|, dot(vel, n))
 *
 * 0.03f is 0x10077944. The max is spelled as a three-way branch in the
 * original and is transcribed as one; the closed form is stated only as a
 * comment because the branch is what has the NaN behaviour.
 *
 * The ladder, thresholds scaling with the level so a far wall has to be
 * approached faster to matter (6.0f 0x10077948, 4.5f 0x1007794C, 3.0f
 * 0x100778DC):
 *
 *      q >  6.0*i    scale 2.0f, clear 0x10000, SET 0x40000, stop
 *      q >  4.5*i    scale 2.0f, clear 0x10000,              stop
 *      q >  3.0*i    scale 1.3f (0x3FA66666),                stop
 *      otherwise     next level; scale stays 1.0f (0x1005DBE2)
 *
 * NOT PORTED, AND NAMED: the loop's INPUT. A[], B[] and the count are written
 * by 0x1005D060, the corridor scan br_ai.h already declines, so the port has
 * the ladder and not the data that feeds it. BrAiThrottleTerm and
 * BrAiThrottleLevel are the two halves of the loop body; the driver that would
 * call them needs 0x1005D060 first.
 * ------------------------------------------------------------------- */
#define BR_AI_THROTTLE_SCALE_NONE 1.0f
#define BR_AI_THROTTLE_SCALE_LIFT 1.3f
#define BR_AI_THROTTLE_SCALE_HARD 2.0f

typedef enum BrAiThrottle {
    BR_AI_THR_CONTINUE = 0,
    BR_AI_THR_LIFT     = 1,
    BR_AI_THR_HARD     = 2,
    BR_AI_THR_BRAKE    = 3
} BrAiThrottle;

float        BrAiThrottleTerm(const BrVec3 *pA, const BrVec3 *pB,
                              const BrVec3 *pBNext, const BrVec3 *pVel);
BrAiThrottle BrAiThrottleLevel(float q, int iLevel);
float        BrAiThrottleScale(BrAiThrottle act);

/* ---------------------------------------------------------------------
 * 9. THE RECOVERY TIMERS  (0x1005DED6 .. 0x1005E04D)
 * -------------------------------------------------------------------
 * Four int32 at car+0xEA0..0xEAC. 0x1005C6D0 -- the respawn -- initialises
 * them at 0x1005C83B..0x1005C84D to 0, 0, 0 and **-180**, which is the only
 * place any of them is set to anything but the values below.
 *
 *      cHoldFwd  car+0xEA0   > 0 forces forward drive, counting down
 *      cHoldRev  car+0xEA4   > 0 forces reverse,       counting down
 *      cRevRun   car+0xEA8   frames spent reversing
 *      cFwdRun   car+0xEAC   frames spent driving forward
 *
 * Mode selection, with aimFwd = car+0x2728 = dot(row 0, aimDir) and
 * velFwd = dot(velocity, row 0):
 *
 *      aimFwd >= 0   cHoldRev != 0  -> REVERSE, cHoldRev--
 *                    velFwd < -1.0  -> BRAKE
 *                    otherwise      -> FORWARD
 *      aimFwd <  0   cHoldFwd != 0  -> FORWARD, cHoldFwd--
 *                    velFwd >  1.0  -> BRAKE
 *                    otherwise      -> REVERSE
 *
 * FORWARD writes f20 = -curve and bumps cFwdRun.
 * REVERSE writes f20 = sign(curve) -- NOT negated, so it COUNTERSTEERS -- sets
 *         0x10000 and 0x20000, and bumps cRevRun. While cRevRun is in
 *         (150, 270] (0x96 / 0x10E) it also multiplies f20 by the double -1.0
 *         at 0x10077958, i.e. flips back every other stretch; past 270 it
 *         resets cRevRun to 30 instead.
 * BRAKE   sets 0x40000 and writes NO steering at all, so the previous frame's
 *         command stands. That is the original's behaviour, not an omission.
 *
 * THE LATCH. Whichever run counter is being bumped, once it passes 30 (0x1E,
 * 0x1005DF44 / 0x1005E021) AND the speed is below 1.0f (0x100778F8), the
 * OPPOSITE hold counter is loaded with 60 (0x3C) and BOTH run counters are
 * zeroed (0x1005E041). So: 31 frames of trying to drive forward while stopped
 * buys 60 frames of forced reverse, and 31 frames of reversing while stopped
 * buys 60 frames of forced forward. Starting from the respawn value of -180,
 * the first latch cannot fire for 211 frames.
 * ------------------------------------------------------------------- */
#define BR_AI_RUN_LATCH   30     /* 0x1E  */
#define BR_AI_HOLD_FRAMES 60     /* 0x3C  */
#define BR_AI_REV_FLIP    150    /* 0x96  */
#define BR_AI_REV_RESET   270    /* 0x10E */
#define BR_AI_FWD_RUN_INIT (-180) /* 0xFFFFFF4C at 0x1005C84D */

/* The three control bits this function touches on car->p29C0's first dword.
 * They are named for the arms that set them, which is all the evidence there
 * is inside 0x1005D770; nothing here claims to know what the physics does with
 * them. 0x10000 is set unconditionally at 0x1005D9E8 every frame before any of
 * this runs, so `cut` below means "and then taken away again". */
#define BR_AI_CTL_10000 0x00010000u   /* set 0x1005D9E8/0x1005DFA5; cleared
                                       * 0x1005DD1A, 0x1005DD42, 0x1005DDA8,
                                       * 0x1005E671                          */
#define BR_AI_CTL_20000 0x00020000u   /* set only on the reverse arm, 0x1005DFD8 */
#define BR_AI_CTL_40000 0x00040000u   /* set 0x1005DD2A and 0x1005DF88        */

typedef struct BrAiRecovery {
    int32_t cHoldFwd;        /* car+0xEA0 */
    int32_t cHoldRev;        /* car+0xEA4 */
    int32_t cRevRun;         /* car+0xEA8 */
    int32_t cFwdRun;         /* car+0xEAC */
} BrAiRecovery;

typedef enum BrAiDrive {
    BR_AI_DRIVE_FORWARD = 0,
    BR_AI_DRIVE_REVERSE = 1,
    BR_AI_DRIVE_BRAKE   = 2
} BrAiDrive;

/* 0x1005C6D0's initialisation of the four, and nothing else of that function. */
void BrAiRecoveryReset(BrAiRecovery *pSt);

/* Runs one frame of the state machine. *pSteer is written only for FORWARD
 * and REVERSE -- BRAKE leaves it alone, as the original does -- and *pfSet
 * gets the BR_AI_CTL_* bits this frame ORs in. */
BrAiDrive BrAiDriveStep(BrAiRecovery *pSt, float aimFwd, float velFwd,
                        float speed, float curve,
                        float *pSteer, unsigned *pfSet);

/* ---------------------------------------------------------------------
 * 10. THE OVERTAKING PASS  (0x1005E054 .. 0x1005E1CD)
 * -------------------------------------------------------------------
 * Walks the 0x80-stride driver array at 0x10AF0858, 0x100B2F00 entries of it,
 * skipping null slots and itself. For each rival:
 *
 *      d = rival.progress - self.progress          both +0xFF4
 *      while (d >  lap) d -= lap                   lap = path root pts[0].arc
 *      while (d < -lap) d += lap                   0x1005E0A6 / 0x1005E0CE
 *      keep only 0 < d < best, best starting at 90.0f (0x42B40000)
 *
 * THE FOLD IS BY A WHOLE LAP, NOT HALF A LAP, and that changes what it is
 * for. 0x1005E09F reads the root node's pts[0].arc -- the full lap length that
 * BrAiLapLength returns -- and neither loop halves it, so d lands in
 * [-lap, +lap]. That is not a start/finish-line wrap: +0xFF4 is a CUMULATIVE
 * distance with the lap count already added in (0x100600A9 adds a lap to it on
 * the rollover), so two cars either side of the line already have close values
 * and need no wrapping. What the fold does is make LAPPED TRAFFIC visible -- a
 * rival one full lap plus ten ahead is treated as ten ahead. A rival genuinely
 * 990 behind on a 1000 lap stays 990 behind and is ignored.
 *
 * and for the nearest one kept, with diff = rival.f58 - self.offset:
 *
 *      diff <  3.0f   add = -10.0f * (1 - d/90), clamped at -5.0f
 *      diff > -3.0f   add = +10.0f * (1 - d/90), clamped at +5.0f
 *
 * 1/90 is 0x10077964, +-10 are 0x10077968/0x10077898, +-5 are
 * 0x1007796C/0x10077908, +-3 are 0x100778DC/0x100778F4. So a rival right on
 * the bumper (d -> 0) saturates the offset immediately and one at 90 gets
 * nothing; the sign pushes the car to the far side of the rival.
 *
 * THE SECOND TEST IS DEAD AND IS KEPT. `diff > -3.0f` is only reached when
 * `diff < 3.0f` was false, so it can only fail on a NaN. The original wrote
 * two overlapping conditions and the second one is unreachable for any ordered
 * input; transcribing it as an `else` would be tidier and would change the
 * NaN behaviour, so it is transcribed as it stands.
 *
 * WHAT THIS DOES *NOT* DO. The result reaches car+0xF48 at 0x1005E1C7, which
 * is AFTER the steering command has already been written at 0x1005DF32. So it
 * does not steer the car this frame and it is not part of the law above. It
 * feeds the block at 0x1005E1CD..0x1005E32D (unported) and whatever reads
 * car+0xF48 elsewhere.
 * ------------------------------------------------------------------- */
#define BR_AI_OVERTAKE_RANGE 90.0f
#define BR_AI_OVERTAKE_CLAMP 5.0f

typedef struct BrAiRival {
    int   fPresent;          /* the array slot's pointer was non-null */
    float progress;          /* car+0xFF4 */
    float offset;            /* car+0xF58, written by 0x1005D3C0      */
} BrAiRival;

float BrAiOvertakeOffset(const BrAiRival *paRivals, int cRivals, int iSelf,
                         float progress, float offset, float lapLength);

/* ---------------------------------------------------------------------
 * 11. THE AIM SMOOTHING  (0x1005D93D)
 * -------------------------------------------------------------------
 *      aim = BrVec3Lerp(aim, aim, target, 0.4f)
 * which, since BrVec3Lerp is (a - b) * t + b, is a 60% step toward the target
 * every frame. 0x3ECCCCCD is pushed as an immediate.
 * ------------------------------------------------------------------- */
void BrAiAimSmooth(BrVec3 *pAim, const BrVec3 *pTarget);

/* ---------------------------------------------------------------------
 * WHAT IS NOT HERE, NAMED PLAINLY
 * -------------------------------------------------------------------
 *  - 0x1005D060 (856 B): the eight-deep corridor lookahead scan. Its point
 *    layout and its 0.2f inset are ported; the scan itself writes eleven
 *    globals per depth level and reaches BrSeg2Intersect, and is left out.
 *    THIS IS NOW THE BINDING GAP: rule 8's ladder is ported and its input
 *    arrays (0x10B1C9B8, 0x10B1CA28, count 0x10AC680C) are what this scan
 *    fills, so the throttle law cannot be driven end to end without it.
 *  - 0x1005D770's own head and tail. The head (0x1005D770..0x1005D93D) is the
 *    early-out on car+0xF00's +0x68 bit 0, a call to 0x10001C90 gated on
 *    car->p29C0 bit 0x2000000, the profiler bracket 0x10008D60, and the
 *    smoothing of the aim toward the target through the three globals at
 *    0x10B1CE88 (rule 11 ports the smoothing, not the 0x10B1CE88 path). The
 *    tail (0x1005E1CD..0x1005E32D) applies a force at car+0x1E8 from the
 *    offsets and is not ported; nor are the calls out to 0x1006F170 and
 *    0x1005C6D0 that close the function.
 *  - 0x1005D3C0 (937 B) and 0x1005C6D0 (468 B). **NEITHER IS THE THROTTLE
 *    LAW**, which is what the brief that produced this section expected them
 *    to be. Read:
 *      0x1005D3C0 builds a DISPLACED racing-line point at car+0xF18 and then
 *        rebuilds exactly the frame of rule 1 around it. It picks the
 *        neighbouring waypoint on the side the car is on (0x1005D49E dots the
 *        car's offset from the segment midpoint against a 2D perpendicular of
 *        the averaged half-width vector, and takes i-1 or i+1 accordingly,
 *        hopping SKIP nodes exactly as BrAiAdvanceTarget does), forms two unit
 *        chord directions, scales them by half the segment arc (0x100778CC ==
 *        0.5f), and blends the two segment midpoints through a ratio built
 *        from two BrVec3Dist calls (0x10034760, which is br_vec.h's
 *        BrVec3Dist, D3D 0x1003B0E0) and the constant -2.0f (0x1007789C).
 *        It ends at 0x1005D749/0x1005D75B writing car+0xF58 = dot(lateral,
 *        pos - F18) and car+0xF48 = |that|. The ONLY thing 0x1005D770 uses out
 *        of it is car+0xF58, and only off the OTHER cars, in rule 10. The
 *        exact blend is a longer x87 trace than this pass could finish
 *        confidently, so it is named and not ported -- deliberately, per the
 *        rule that a plausible-looking wrong function is the worst outcome.
 *      0x1005C6D0 is the RESPAWN: bail unless car+0x35C < 0 or a timer at
 *        0x106EED10 has run out, then reset the car onto the path, zero four
 *        wheel records, and clear the recovery timers. BrAiRecoveryReset is
 *        its four stores at 0x1005C83B..0x1005C84D and nothing else.
 *  - 0x1005C8B0 (D3D 0x10063840): the human controller, for comparison.
 *  - The difficulty table at 0x100B3018 -- see the report; its consumer
 *    0x1005C490 (D3D 0x10063420) is already flagged in the README as reading
 *    one index clamped and one not.
 *  - The node/section byte-swapper 0x10018B60 that br_track.c also declines.
 *    This module sidesteps it by decoding big-endian on read, which is why
 *    nothing here needs br_track.c to change.
 * ------------------------------------------------------------------- */

#endif /* BR_AI_H */
