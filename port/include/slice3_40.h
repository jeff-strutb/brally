/* slice3_40.h -- Boss Rally (BRD3D.dll) slice 3, agent 40.
 *
 * Packet range 0x100609E0 - 0x10065D50 (25 functions).  Five unrelated
 * clusters live in the range; only the ones that could be resolved with
 * confidence are ported here (see the agent report for the skip list):
 *
 *   1. network car-state apply / predict   0x100609E0 0x10060A10 0x10060CC0
 *   2. two 10-step option sliders          0x10060D50 0x10060D70
 *                                          0x10060DC0 0x10060DE0
 *   3. controller-state translation        0x10060E00
 *   4. scene-node 0x8000 mark/clear pass   0x10061660 0x100616C0 0x10061700
 *   5. car per-frame table init            0x10061BE0 0x100633E0
 *                                          0x10065630 0x10065710
 *   6. path walking + segment crossing     0x10065B20 0x10065C80
 *      (plus the image tint scale setter   0x10061460)
 *
 * Recovered from work/slice3/agent40.asm.  Fields whose meaning could not be
 * established keep positional names (fNN = byte offset NN).
 */
#ifndef SLICE3_40_H
#define SLICE3_40_H

#include <stdint.h>
#include <stddef.h>

#include "br_vec.h"      /* BrVec3, BrVec3Lerp (0x1003AFA0)                 */
#include "br_mat.h"      /* BrMat4                                          */
#include "slice1_02.h"   /* BrCarState -- 0xA0 bytes, forty floats          */
#include "slice1_07.h"   /* BrImgTintState (0x10AA3440/3448/345C)           */
#include "slice2_15.h"   /* BrCar -- stride 0x2B68, already defined there   */
#include "slice2_21.h"   /* BrVec2, BrSeg2Intersect, BrFtolTrunc            */
#include "slice3_44.h"   /* BrRbState, BrRbQuatDerivative, BrRbBuildMatrix  */

/* =====================================================================
 * The car / entity record
 *
 * The type is slice2_15.h's `BrCar` (stride 0x2B68, a project-wide known
 * fact) -- NOT a new one.  Only a handful of the eleven kilobytes are
 * touched by this packet and the ones that are do not form a describable
 * struct yet, so every access in slice3_40.c is written as an explicit byte
 * offset into the record.  That keeps the offset map -- which is the real
 * product of these functions -- literally readable, and does not force
 * slice2_15's layout to grow fields it cannot justify.
 *
 * Two offsets do line up with names slice2_15 already has: its f0FF4 is the
 * float BrCarApplyState guards, and its f1030 is the mph field.
 *
 * What this packet does pin down:
 *   +0x140         int32, a per-car index (used as a float scale seed and
 *                  as the argument to 0x10076A40)
 *   +0x164         passed whole to 0x1006F4A0
 *   +0x168 ..0x174 four pointers to sub-objects, each carrying a BrRbState
 *                  at +0x78 and a BrMat4 at +0xBC
 *   +0x1DC         BrRbState  (0x44 bytes) -- filled by BrCarApplyState
 *   +0x278, +0x2BC two more BrRbState copies of +0x1DC
 *   +0x106C..0x10DB  five parallel 4-element arrays (the struct-of-arrays
 *                  that the project notes record at car+0x10AC)
 *   +0x1120        144 records of 0x20 bytes
 *   +0x2320        144 records of 6 bytes (immediately after the above)
 *   +0x2680        18 dwords
 *   +0x29C0        pointer to a dword of flags
 *   +0x29C8..0x29D9 a 18-byte block cleared by BrCarClear29C8
 *   +0xFF4         a float compared against BrCarState.f78
 * ===================================================================== */

/* Sub-object offsets confirmed by 0x10061BE0 and 0x10061720. */
#define BR_CARSUB_RB   0x78    /* BrRbState */
#define BR_CARSUB_MAT  0xBC    /* BrMat4    */

/* DEVIATION -- pointer width.  The four sub-object pointers are 4 bytes
 * each in the 32-bit original and sit at 0x168, 0x16C, 0x170 and 0x174.
 * Four 8-byte pointers cannot fit there on a 64-bit host, so this port
 * treats 0x168 as the base of a `void *[4]`; on a 32-bit host that is
 * byte-identical to the original and on a 64-bit host it spreads to
 * 0x168..0x187.  The same caveat applies to the single pointer at 0x29C0,
 * which on a 64-bit host covers 0x29C4 as well -- 0x29C4 is read by
 * 0x10065D50, which is NOT ported here. */
#define BR_CAR_SUBPTR(pCar, i) \
    (((void **)(void *)((uint8_t *)(void *)(pCar) + 0x168))[(i)])

/* =====================================================================
 * The node / path type
 *
 * One type serves both clusters 4 and 6: 0x10061660 walks it by f00/f04 and
 * touches +0x11 and the flag word at +0x16, while 0x10065B20 / 0x10065C80
 * walk the same links and read the 0x28-byte point records that start at
 * +0x4C.  Neither f00 nor f04 could be shown to be "child" vs "sibling"
 * (0x10061660 recurses on f00 and iterates f04; 0x10065B20 follows f04 only
 * while flag bit 0 is set and then continues through f00), so both keep
 * positional names.
 * ===================================================================== */

/* +0x4C onwards: an array of 0x28-byte records.  Only two fields are read
 * by this packet.  f18 DECREASES along the array -- 0x10065B20 forms
 * pts[i].f18 - pts[i+1].f18 and treats it as a non-negative segment
 * length. */
typedef struct BrPathPoint {
    BrVec3  pos;                 /* +0x00 */
    float   f0C, f10, f14;       /* +0x0C */
    float   f18;                 /* +0x18  distance-remaining style scalar */
    uint8_t f1C[0x28 - 0x1C];    /* +0x1C */
} BrPathPoint;                   /* 0x28 */

#define BR_NODE_FLAG_SKIP  0x0001u   /* bit 0 of the +0x16 word */
#define BR_NODE_FLAG_MARK  0x8000u   /* bit 15, set by 0x10061660 */

/* DEVIATION: the two leading pointers are 4 bytes in the 32-bit original,
 * so on a 64-bit host every offset after them shifts.  Nothing here overlays
 * this struct on foreign memory, and `pts` is a flexible array member so the
 * point stride stays sizeof(BrPathPoint) whatever the pointer width is.
 * The comments keep the ORIGINAL offsets. */
typedef struct BrNode BrNode;
struct BrNode {
    BrNode  *f00;                /* +0x00 */
    BrNode  *f04;                /* +0x04 */
    uint8_t  f08[0x11 - 0x08];   /* +0x08 */
    uint8_t  f11;                /* +0x11  cleared by 0x10061660 */
    uint8_t  f12[0x14 - 0x12];   /* +0x12 */
    uint16_t count;              /* +0x14  number of point records */
    uint16_t flags;              /* +0x16 */
    uint8_t  f18[0x4C - 0x18];   /* +0x18 */
    BrPathPoint pts[];           /* +0x4C  `count` + 1 records, see below */
};

/* =====================================================================
 * The 2D segment table walked alongside the path (0x106C7CE0)
 *
 * Stride 0x14.  0x10065B20 hands offsets +0x08 and +0x00 of a record to
 * BrSeg2Intersect as its first two arguments, in that order, so the record
 * holds two 2D points.  The third dword is never read here.
 *
 * BR_PATH_SEG_MAX is an INFERENCE: the count variable sits at 0x106C7DA8,
 * exactly 0xC8 = 10 * 0x14 bytes after the table, so at most ten records
 * fit.  Nothing in the disassembly proves the two are adjacent.
 * ===================================================================== */
typedef struct BrPathSeg {
    BrVec2  a;      /* +0x00 */
    BrVec2  b;      /* +0x08 */
    int32_t f10;    /* +0x10  never read by this packet */
} BrPathSeg;        /* 0x14 */

#define BR_PATH_SEG_MAX 10

extern BrPathSeg BrPathSegs[BR_PATH_SEG_MAX];  /* 0x106C7CE0 */
extern int32_t   BrPathSegCount;               /* 0x106C7DA8 -- the modulus */

/* Outputs of both path walks. */
extern int32_t BrPathCrossCount;   /* 0x10AF96C0 */
extern int32_t BrPathWrapCount;    /* 0x10AF9B44 -- bumped when index 0 hit */
extern BrVec3  BrPathWalkPoint;    /* 0x10AF9B38 */
extern BrNode *BrPathWalkNode;     /* 0x10AF988C */
extern int32_t BrPathWalkIndex;    /* 0x10ACD490 */

/* =====================================================================
 * Cross-slice dependencies
 * ===================================================================== */

/* XSLICE 0x100607B0 -- skipped by agent 39 (its packet ends at this
 * address).  Fills a BrCarState from a car record; the exact contents are
 * not established here, hence the address-shaped name. */
extern void BrSub100607B0(BrCarState *pDst, BrCar *pCar);

/* XSLICE 0x10005130 -- declared exactly as slice2_11.h declares it. */
extern int BrNetCarStateSend(BrCarState *pState);

/* XSLICE 0x10005D30 -- no arguments; its result is compared for equality
 * against the slot index handed to BrCarPredictRemote, so it is almost
 * certainly the local player's slot.  Not named for that. */
extern int32_t BrSub10005D30(void);

/* XSLICE 0x100054A0 -- CAUTION.  slice2_12.h ports this address as
 *     int BrNetSlotPredict(BrCarState*, int32_t, BrNetState*, void*,
 *                          int32_t, uint32_t);
 * having lifted four globals (0x10221328, 0x1022AF34, 0x10094294,
 * 0x10003460) into extra parameters.  The ORIGINAL takes only (pDst, slot),
 * which is the form the call site in 0x10060CC0 uses.  Declaring
 * BrNetSlotPredict twice with different arities would not compile, so this
 * one is suffixed.  INTEGRATION: bind BrNetSlotPredictOrig to
 * BrNetSlotPredict with the four globals supplied. */
extern int BrNetSlotPredictOrig(BrCarState *pDst, int32_t slot);

/* XSLICE 0x100695D0 -- called from the middle of BrCarApplyState with
 * (pCar + 0x220, pState).  Effect unknown. */
extern void BrSub100695D0(void *pDst220, const BrCarState *pState);

/* XSLICE 0x1006F4A0 -- called with pCar + 0x164. */
extern void BrSub1006F4A0(void *pCar164);

/* XSLICE 0x100773F0 -- reads the raw input device.  Returns a bitfield in
 * the low 16 bits of EAX and writes two signed axis values through its two
 * out-parameters; only the low byte of each is used. */
extern uint32_t BrSub100773F0(int32_t *pAxis0, int32_t *pAxis1);

/* XSLICE 0x100B380C -- declared exactly as slice2_18.h declares it. */
extern int32_t BrG_0B380C;

/* XSLICE 0x10B4E708 / 0x10B4E70C -- declared exactly as slice2_25.h
 * declares them.  slice2_25.h also records the max of 9 for both, which the
 * cyclers below re-derive from their own compare immediates. */
extern int32_t g_brB4E708;
extern int32_t g_brB4E70C;

/* XSLICE 0x100BBAE0 -- declared exactly as slice1_08.h declares it. */
extern uint8_t BrSndMasterVolume;

/* Owned by this slice. */
extern int32_t BrG_6909B4;   /* 0x106909B4 -- non-zero suppresses prediction */
extern BrNode *BrG_6C7CB8;   /* 0x106C7CB8 -- root handed to the mark pass  */
extern uint8_t BrG_0BBAD8;   /* 0x100BBAD8 -- output of the level-A slider  */

/* =====================================================================
 * 1. Network car-state apply / predict
 * ===================================================================== */

/* 0x100609E0  build a BrCarState from pCar on the stack and hand it to
 * BrNetCarStateSend.  The 0xA0-byte stack buffer is what identifies the
 * intermediate as a BrCarState. */
void BrCarNetSendState(BrCar *pCar);

/* 0x10060A10  copy a received BrCarState into a car record.
 *
 * The field map is the point of this function; it is spelled out at each
 * assignment in the .c.  Summary:
 *
 *   pState->f00..f0C  -> the BrRbState quaternion at pCar+0x1F4 (w,x,y,z)
 *   pState->f10..f18  -> its position   (pCar+0x1DC)
 *   pState->f1C..f24  -> its velocity   (pCar+0x1E8)
 *   pState->f28..f30  -> its angular velocity (pCar+0x204)
 *   pState->f34..f48  -> seven scattered dwords, copied verbatim
 *   pState->f4C..f9C  -> fourteen fields put through __ftol first, five of
 *                        them narrowed to a single byte
 *   pState->f70/f74   -> two booleans, both testing EQUALITY WITH 0.0f
 *   pState->f78       -> a guarded write to pCar+0xFF4
 *   pState->f7C       -> pCar+0xE24
 *
 * GOTCHA: pState->f38 is stored TWICE, to pCar+0x73C and to pCar+0xB54.
 *
 * GOTCHA: the two 0.0f comparisons are "== 0.0f", not "!= 0.0f":
 *   f70 == 0 CLEARS bit 0x40000 of *(uint32*)pCar[0x29C0], otherwise SETS;
 *   f74 == 0 writes +1.0f to pCar+0xE68, otherwise -1.0f.
 * Both test the x87 C3 bit alone, and an UNORDERED compare sets C3, so a
 * NaN takes the same branch as 0.0f in both -- the opposite of what C's
 * `== 0.0f` would do.  Written as !(v < 0 || v > 0) in the .c for that
 * reason.
 *
 * GOTCHA: the pCar+0xFF4 guard is
 *       !(pCar[0xFF4] > 0.0f) || (pCar[0xFF4] + 1000.0f > pState->f78)
 * with both comparisons testing C3|C0, so unordered counts as "less or
 * equal" in each: a NaN in pCar[0xFF4] takes the first term (overwrite), a
 * NaN in pState->f78 fails the second (no overwrite).  The 1000.0f arrives
 * as a stored -1000.0f that is SUBTRACTED. */
void BrCarApplyState(BrCar *pCar, const BrCarState *pState);

/* 0x10060CC0  dead-reckon slot `slot` and, if that produced a state, apply
 * it to pCar and rebuild its matrices.
 *
 * Returns 1 in three different situations -- slot is the local slot, the
 * 0x106909B4 gate is set, or the prediction succeeded -- and 0 only when
 * the prediction itself failed.  The return therefore does NOT mean "pCar
 * was updated".
 *
 * GOTCHA: the argument order is (pCar, slot) but the two are read from the
 * stack in the opposite order, and `slot` is compared against
 * BrSub10005D30() BEFORE pCar is ever touched. */
int32_t BrCarPredictRemote(BrCar *pCar, int32_t slot);

/* =====================================================================
 * 2. Two 10-step option sliders
 *
 * Two independent index-plus-lookup-table pairs with identical code:
 *
 *   level A: index g_brB4E70C, table 0x100ADF68, output BrG_0BBAD8
 *   level B: index g_brB4E708, table 0x100ADF90, output BrSndMasterVolume
 *
 * B's output is slice1_08's byte master volume, so B is the master volume
 * slider; A's consumer is not in this packet, so it keeps a positional
 * name.  The two tables differ: A is linear over 0..255 in ten steps, B is
 * a perceptual curve (0, 85, 120, 147, 170, 190, 208, 225, 240, 255).
 *
 * The tables are dword arrays in the original and the code reads only the
 * LOW BYTE of the selected dword; every value happens to fit in a byte.
 *
 * GOTCHA: the clamp is asymmetric in an easily-missed way.  StepUp only
 * writes the index back when it was below 9, but it ALWAYS re-reads the
 * table afterwards -- so calling StepUp at 9 is not a no-op, it re-applies
 * table[9].  Same for StepDown at 0.  Both are used as "reapply the
 * current setting" calls at the ends of the range.
 * ===================================================================== */
#define BR_OPT_LEVEL_STEPS 10

extern const int32_t BrOptLevelATable[BR_OPT_LEVEL_STEPS];  /* 0x100ADF68 */
extern const int32_t BrOptLevelBTable[BR_OPT_LEVEL_STEPS];  /* 0x100ADF90 */

void BrOptLevelAStepUp(void);    /* 0x10060D50 */
void BrOptLevelAStepDown(void);  /* 0x10060D70 */
void BrOptLevelBStepUp(void);    /* 0x10060DC0 */
void BrOptLevelBStepDown(void);  /* 0x10060DE0 */

/* =====================================================================
 * 3. Controller-state translation  (0x10060E00)
 *
 * Reads the device through 0x100773F0 and packs the result into four bytes.
 * Byte 2 and byte 3 are the low bytes of the two axis out-params; bytes 0
 * and 1 are a bitfield rebuilt one flag at a time.
 *
 * The destination is written BYTE-WISE on purpose.  The original stores a
 * little-endian `word ptr [ecx]` of 0x8400 and then ORs into `byte [ecx]`
 * and `byte [ecx+1]` separately, so the two bytes are the unit of meaning,
 * not a host-endian 16-bit value.  Reproducing it byte-wise keeps the
 * layout identical on a big-endian host.
 *
 * The mapping (source bit -> destination byte and mask):
 *     0x0001 -> b1 |= 0x02      0x0100 -> b0 |= 0x08
 *     0x0002 -> b1 |= 0x01      0x0200 -> b0 |= 0x02
 *     0x0004 -> b1 |= 0x88      0x0400 -> b0 |= 0x04
 *     0x0008 -> b1 |= 0x40      0x8000 -> b1 |= 0x10
 *     0x0010 -> b0  = 0x00, b1 = 0x84   (an ASSIGNMENT, see below)
 *     0x0020 -> b0 |= 0x10
 *     0x0040 -> b0 |= 0x20
 * Bits 0x0800..0x4000 are ignored.
 *
 * GOTCHA: 0x0010 is handled FIRST and by assignment, not by OR, so it seeds
 * the pair rather than adding to it.  Every other flag then ORs on top.
 * GOTCHA: 0x0004 sets TWO bits at once (0x88).
 *
 * NAME: slice2_18.h already externs this address as
 * `void BrGfx60E00(void *p0)`; that exact declaration is kept. */
void BrGfx60E00(void *p0);

/* =====================================================================
 * 4. The node 0x8000 mark / clear pass
 * ===================================================================== */

/* 0x10061660  walk the f04 chain from pNode; for every node that has
 * neither BR_NODE_FLAG_MARK nor BR_NODE_FLAG_SKIP set, set the mark, then
 * if that node's f11 is exactly 2 and BrG_0B380C is 3 or 9 clear f11, then
 * recurse into f00.  Nodes that already carry either flag are skipped
 * entirely -- including their f00 subtree.
 *
 * GOTCHA: the mark is set BEFORE the recursion, so a cycle through f00
 * terminates instead of running away.  That is what the flag is for; it is
 * not a visibility bit. */
void BrNodeMarkPass(BrNode *pNode);

/* 0x100616C0  the exact inverse walk: for every node in the f04 chain that
 * HAS the mark, clear it and recurse into f00.  Nodes without the mark are
 * skipped, so this only unwinds what BrNodeMarkPass marked. */
void BrNodeClearMarkPass(BrNode *pNode);

/* 0x10061700  BrNodeMarkPass(BrG_6C7CB8) then BrNodeClearMarkPass of the
 * same root.  The mark is a scratch bit; the observable effect of the pair
 * is the f11 clearing done by the first pass. */
void BrNodeRunMarkPass(void);

/* =====================================================================
 * 5. Car per-frame / per-race init
 * ===================================================================== */

/* 0x10061BE0  BrSub1006F4A0(pCar+0x164), then for each of the four
 * sub-objects at pCar+0x168..0x174 build its matrix from its rigid-body
 * state: BrRbBuildMatrix(sub+0xBC, sub+0x78). */
void BrCarBuildMatrices(BrCar *pCar);

/* 0x100633E0  zero every region in a NULL-terminated {pointer, size} list.
 *
 * DEVIATION: the original hard-codes the list at 0x100B3700 and takes no
 * argument; the list is passed in here so the function is testable, exactly
 * as slice1_07 does for its four rectangle tables. */
typedef struct BrZeroRegion { void *p; uint32_t size; } BrZeroRegion;
void BrZeroRegions(BrZeroRegion *pList);

/* 0x10065630  reset the per-car working tables.
 *
 * GOTCHA -- there is a dead store here and it is in the original.  The
 * function first writes four floats to pCar+0x10AC..0x10B8 (a chain
 * v, v+0.034, v+0.068, v+0.102 with v = (float)pCar[0x140] * 0.137f, each
 * term derived from the previous UNROUNDED one) and then, in the very next
 * loop, zeroes all four of those same dwords.  The floats never survive the
 * call.  Preserved rather than removed: something else may still read the
 * transient value if this is ever re-entered, and removing it would hide a
 * real property of the build.
 *
 * The surviving writes are:
 *   pCar[0x106C + 4i] = (float)i * 0.15f      i = 0..3
 *   pCar[0x10AC + 4i] = 0
 *   pCar[0x10BC + 4i] = 2
 *   pCar[0x10CC + 4i] = 0
 *   pCar[0x10DC + 4i] = 0
 *   pCar[0x2320 + 6j] .. +4  = 0 (three u16)  j = 0..143
 *   pCar[0x1120 + 32j] at +0,+4,+8,+0x14,+0x18,+0x1C = 0
 *   pCar[0x2680 + 4k] = 0x00020002            k = 0..17
 *
 * GOTCHA: +0x0C and +0x10 of each 0x20-byte record are deliberately NOT
 * cleared.  The original has a `test dl,1` two-way branch around the +0x18
 * store whose two arms write the same zero -- a compiler artifact, not a
 * condition. */
void BrCarInitTables(BrCar *pCar);

/* 0x10065710  zero pCar+0x29C8..0x29D4 (four dwords) and the u16 at
 * pCar+0x29D8.  Eighteen bytes, not twenty. */
void BrCarClear29C8(BrCar *pCar);

/* =====================================================================
 * 6. Path walking
 *
 * Both routines consume a distance `t` along the point array of a node
 * chain and leave their answer in the globals BrPathWalkPoint /
 * BrPathWalkNode / BrPathWalkIndex.  Segment i runs from pts[i] to
 * pts[i+1] and its length is pts[i].f18 - pts[i+1].f18.
 *
 * GOTCHA (both): the loop runs i over [0, count) but reads pts[i+1], so it
 * reads ONE RECORD PAST the count on the last iteration.  Preserved.
 *
 * GOTCHA (both): if `t` is never consumed the walk falls off the end of the
 * chain and the three output globals are left holding whatever the previous
 * call put there.  They are only written on the hit path.
 * ===================================================================== */

/* 0x10065B20  walk from pNode consuming t, and count how many of the 2D
 * segments in BrPathSegs the walk crossed on the way.
 *
 * BrPathCrossCount and BrPathWrapCount are reset to 0 on entry.  For each
 * whole segment stepped over, and once more for the final partial segment,
 * the walk tests the path segment against BrPathSegs[(BrPathCrossCount + 1)
 * % BrPathSegCount]; a hit bumps BrPathCrossCount, and a hit on index 0
 * additionally bumps BrPathWrapCount.
 *
 * GOTCHA: BrSeg2Intersect is called as (&seg->b, &seg->a, ...) -- the
 * table record's SECOND point is passed first.
 *
 * GOTCHA: the final lerp is BrVec3Lerp(out, &pts[i+1].pos, &pts[i].pos, u).
 * BrPathWalkFrom below passes the same two points the OTHER WAY ROUND.
 * Since BrVec3Lerp computes (a - b) * t + b the two are NOT the same
 * function of u, and both are as the original has them. */
void BrPathWalk(BrNode *pNode, float t);

/* 0x10065C80  the same walk, but resumed from (pNode, index) and with the
 * first segment's length scaled by `s`; s is forced to 1.0f from the second
 * segment onwards.  Does not touch BrPathCrossCount / BrPathWrapCount.
 *
 * GOTCHA: this one runs TWO lerps.  The first is
 *   BrVec3Lerp(&BrPathWalkPoint, &pts[i].pos, &pts[i+1].pos, s)
 * -- note the operand order is the mirror of BrPathWalk's -- and the second
 * re-lerps that result toward pts[i+1].pos by t / (segLen * s), reading and
 * writing BrPathWalkPoint in the same call (BrVec3Lerp reads each component
 * before writing it, so the aliasing is safe and is relied on). */
void BrPathWalkFrom(BrNode *pNode, int32_t index, float s, float t);

/* =====================================================================
 * 7. Image tint scale  (0x10061460)
 *
 * Sets the three multipliers slice1_07 models as BrImgTintState.scaleR /
 * scaleG / scaleB (0x10AA3440, 0x10AA3448, 0x10AA345C).  Note the stride is
 * NOT uniform -- 0x00, 0x08, 0x1C -- which is why slice1_07 keeps the gaps
 * as positional fields.  0x10065D50 calls this with three consecutive BYTES
 * of an enemy-car record, so the arguments are 0..255 colour components
 * even though the slots are dwords.
 * ===================================================================== */
void BrImgTintSetScale(int32_t r, int32_t g, int32_t b);

#endif /* SLICE3_40_H */
