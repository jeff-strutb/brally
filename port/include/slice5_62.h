/* slice5_62.h -- agent 62, slice 5 ("close the broken links").
 *
 * Not an address range: fifteen individually-requested functions that an
 * already-ported module calls but nobody implemented.  Nine are here; six are
 * skipped (see SKIPPED at the bottom of this file and the report).
 *
 * Every name and signature below is the one the calling module's header
 * already declares -- they were grepped out of port/include before a line was
 * written, not invented here:
 *
 *   BrExt_100419D0        slice2_26.h:249   void (void *)
 *   BrSub1005FCF0         slice2_25.h:456   void (void)
 *   BrGbiGeoModeChanged   slice2_16.h:88    void (void)
 *   BrSub_100020D0        slice2_15.h:476   void (char *, float)
 *   BrSub_1003289F        slice2_15.h:478   void (int, int, int, int)
 *   BrSub_10069490        slice2_15.h:506   BrMat4 *(void)
 *   BrSub_1001C820        slice2_15.h:510   void (uint32_t, uint32_t)
 *   BrNetKeepAliveTick    slice2_11.h:183   void (void)
 *   BrSub100765E0         slice3_45.h:250   void (const BrMat4 *, BrVec4 *)
 *
 * SIGNATURE CONFLICTS FOUND (reported, not resolved here):
 *   0x1003CDA0 is BrExt_1003CDA0 in slice2_26.h and BrSub1003CDA0 in
 *              slice2_25.h -- same `void (void)` shape, two names.
 *   0x10058750 is BrOptFn10058750(BrOptObj *) in slice2_25.h and
 *              BrExt_10058750(BrPhase *) in slice2_26.h.  Both are the same
 *              one-pointer-argument function; the packet asked for both names
 *              for the one address.  Neither is implemented (see SKIPPED).
 *
 * HOW GLOBAL STATE IS MODELLED
 * ============================
 * These functions reach through fixed addresses for everything.  Rather than
 * scatter loose globals whose names could collide with another slice's, each
 * group gets one struct and one getter, and every field carries the original
 * address it stands for so the coordinator can alias it onto whatever slice
 * ends up owning the storage.  Where another header already models the same
 * address, the field comment says so.
 *
 * DEVIATION (whole file): the original stores CODE ADDRESSES into several
 * dword globals (0x100A7A00, 0x100A7CEC, 0x100A7CB4, 0x100A79EC).  Their
 * targets are outside this packet and their signatures are not established,
 * so they are modelled as uint32_t holding the original address.  Compare
 * them numerically against the BR_*_FN_* constants below; do not call them.
 */
#ifndef SLICE5_62_H
#define SLICE5_62_H

#include <stddef.h>
#include <stdint.h>

#include "br_mat.h"     /* BrMat4                              */
#include "br_pool.h"    /* BrPool, BrPoolAlloc (0x10069490)    */
#include "slice1_09.h"  /* BrVec4, BrVec4Normalise (0x100741B0) */

/* ==================================================================== */
/* 1. 0x100419D0 -- a four-argument virtual dispatch                    */
/* ==================================================================== */

/* slice1_06.h declined this one with "a four-argument virtual dispatch
 * through an object of unknown type; nothing about it is recoverable beyond
 * the vtable slot".  That is still true of the TYPES.  What IS recoverable,
 * and all that is modelled here:
 *
 *   owner  = *(void **)0x10AA2904
 *   table  = owner->+0x14
 *   target = table->+0x18 + index*4          index = *(int *)0x10A9DBD0
 *   if (target) target->vtbl[13](target, p, 1, 1, (const void *)0x100AB558)
 *
 * The fourth argument is NOT a string: the dwords at 0x100AB558 are
 * { 0x50, 0x1D, 0x1AE, 0x30, 0, 0 }, so it is passed as an opaque pointer.
 *
 * DEVIATION: on a 64-bit host a pointer is eight bytes, so BrX419D0Owner and
 * BrX419D0Table are a RE-LAYOUT of the original objects, not a byte-exact
 * model.  The +0x14 / +0x18 displacements are recorded in the comments; only
 * the access PATH is preserved. */

/* __thiscall in the original, four explicit arguments after `this`.
 * The return value is discarded by 0x100419D0. */
typedef int32_t (*BrX419D0Method)(void *pThis, void *p,
                                  int32_t a2, int32_t a3, const void *pv);

/* The dispatched-to object: only its vtable pointer is known, and only
 * slot 13 (byte offset 0x34) of that vtable is used. */
#define BR_X419D0_VTBL_SLOT 13
typedef struct BrX419D0Target {
    BrX419D0Method *apfn;      /* +0x00 vtable */
} BrX419D0Target;

typedef struct BrX419D0Table {
    uint32_t         f00[6];   /* +0x00..+0x14, not touched here          */
    BrX419D0Target  *apObj[1]; /* +0x18, indexed by the 0x10A9DBD0 index  */
} BrX419D0Table;

typedef struct BrX419D0Owner {
    uint32_t        f00[5];    /* +0x00..+0x10, not touched here */
    BrX419D0Table  *pTable;    /* +0x14 */
} BrX419D0Owner;

typedef struct BrX419D0State {
    BrX419D0Owner *pOwner;     /* 0x10AA2904 -- slice2_26.h's pAA2904   */
    int32_t        index;      /* 0x10A9DBD0                            */
    const void    *pvAB558;    /* 0x100AB558 -- opaque 4th argument     */
} BrX419D0State;

BrX419D0State *BrX419D0GetState(void);

/* 0x100419D0.  Does nothing at all when the selected slot is NULL. */
void BrExt_100419D0(void *p);

/* ==================================================================== */
/* 2. 0x1005FCF0 -- latch the session settings                          */
/* ==================================================================== */

/* Copies eight settings globals into their "committed" shadows, then ORs the
 * two halves of one 32-bit word into two separate accumulators.
 *
 * GOTCHA: the two ORs read the LOW and HIGH 16-bit halves of the SAME dword
 * at 0x10AA27E0 -- the original's second load is a `mov cx, word [0x10AA27E2]`,
 * which is that dword's high half, not a neighbouring variable.
 *
 * GOTCHA: the accumulators are OR-ed into, never assigned.  Bits already set
 * in f2A10 / f2A14 are never cleared by this function.
 *
 * GOTCHA: the table lookup is skipped entirely while f0AA010 is non-zero. */
typedef struct BrSessLatch {
    /* sources */
    int32_t   f094354;      /* 0x10094354 */
    int32_t   f094358;      /* 0x10094358 */
    int32_t   f09435C;      /* 0x1009435C */
    int32_t   fAA28A0;      /* 0x10AA28A0 */
    int32_t   fAA28A4;      /* 0x10AA28A4 -- low byte indexes the table  */
    int32_t   fAA28B8;      /* 0x10AA28B8 -- low byte indexes the table  */
    int32_t   fB4E1D0;      /* 0x10B4E1D0 */
    int32_t   f0AA010;      /* 0x100AA010 -- slice2_26.h's n0AA010      */
    uint32_t  fAA27E0;      /* 0x10AA27E0 -- both halves are consumed   */
    /* destinations */
    int32_t   fAA27EC;      /* 0x10AA27EC <- f094354 */
    int32_t   fAA27F0;      /* 0x10AA27F0 <- f09435C */
    int32_t   fAA27F4;      /* 0x10AA27F4 <- f094358 */
    int32_t   fAA27F8;      /* 0x10AA27F8 <- fB4E1D0 */
    int32_t   fAA26F0;      /* 0x10AA26F0 <- fAA28A0 (full dword)       */
    uint8_t   fAA26F4;      /* 0x10AA26F4 <- (uint8)fAA28B8             */
    uint8_t   fAA26F5;      /* 0x10AA26F5 <- (uint8)fAA28A4             */
    uint32_t  f0B380C;      /* 0x100B380C <- table[i]   (zero-extended) */
    uint32_t  f22B350;      /* 0x1022B350 <- table[i+1] (zero-extended) */
    uint32_t  fAA2A10;      /* 0x10AA2A10 |= fAA27E0 & 0xFFFF           */
    uint32_t  fAA2A14;      /* 0x10AA2A14 |= fAA27E0 >> 16              */
} BrSessLatch;

BrSessLatch *BrSessLatchGetState(void);

/* The bytes at 0x100B3820, indexed as table[2 * (12*fAA28B8 + fAA28A4)] and
 * [+1].  Verbatim from the shipped .data image.
 *
 * GOTCHA: only the first EIGHT bytes belong to this table in the image; from
 * offset 8 on it runs straight into the neighbouring 0x18-byte command
 * descriptor array (tags 0xE0..0xE4).  That is what the original reads too --
 * there is no bounds check anywhere -- so the bytes are reproduced as-is
 * rather than "corrected". */
#define BR_SESS_TABLE_BYTES 256
extern const uint8_t g_brB3820[BR_SESS_TABLE_BYTES];

void BrSub1005FCF0(void);

/* ==================================================================== */
/* 3. 0x1001E7C0 and 0x1001C820 -- rasteriser selection                 */
/* ==================================================================== */

/* Both of these pick a triangle/vertex routine out of a fixed set and park
 * its address in a global.  They share two of those globals, which is why
 * they share a state block here.
 *
 * The two deferred-render-state entries 0x1001E7C0 touches are entries 4 and
 * 5 of slice4_51.h's BrGbiRectState arrays (0x10277388 = aPending[4],
 * 0x1027738C = aPending[5], 0x10277408 = aShadow[4], 0x1027740C =
 * aShadow[5]), and the bits it sets in `dirty` are 0x10 and 0x20 -- exactly
 * "bit i guards entry i", which slice4_51.h documents.
 *
 * GOTCHA: the dirty bit is CLEARED when the newly computed pending value
 * already equals the shadow.  It is not a sticky "something changed" flag. */
#define BR_GBI_RS_COUNT 11

typedef struct BrRasterSel {
    uint32_t  geoCur;        /* 0x104C5178 -- BrGbiState.geo.cur       */
    uint32_t  geoPrev;       /* 0x104C517C -- BrGbiState.geo.prev      */

    uint32_t  dirty;                        /* 0x10277370             */
    uint32_t  aPending[BR_GBI_RS_COUNT];    /* 0x10277378..0x102773A0 */
    uint32_t  aShadow [BR_GBI_RS_COUNT];    /* 0x102773F8..0x10277420 */

    uint32_t  f4C16A0;       /* 0x104C16A0                             */
    uint32_t  f4C0DC0;       /* 0x104C0DC0 -- BrGbiRectState.f4C0DC0   */
    uint32_t  f4C0BB4;       /* 0x104C0BB4                             */

    uint32_t  f6C6618;       /* 0x106C6618                             */
    uint32_t  f6C661C;       /* 0x106C661C                             */
    uint32_t  f6C6624;       /* 0x106C6624                             */

    /* code-address slots -- see the file-level DEVIATION */
    uint32_t  pfn0A7A00;     /* 0x100A7A00 */
    uint32_t  pfn0A7CEC;     /* 0x100A7CEC */
    uint32_t  pfn0A7CB4;     /* 0x100A7CB4 */
    uint32_t  pfn0A79EC;     /* 0x100A79EC */
} BrRasterSel;

BrRasterSel *BrRasterSelGetState(void);

/* The only values the four slots above ever take. */
#define BR_FN_A00_10021BD0  0x10021BD0u
#define BR_FN_A00_10021E80  0x10021E80u
#define BR_FN_A00_10022480  0x10022480u
#define BR_FN_A00_100228F0  0x100228F0u
#define BR_FN_A00_100231D0  0x100231D0u
#define BR_FN_A00_10023A10  0x10023A10u
#define BR_FN_A00_10023CC0  0x10023CC0u

#define BR_FN_CEC_1001CFF0  0x1001CFF0u
#define BR_FN_CEC_1001E980  0x1001E980u
#define BR_FN_CEC_1001F2B0  0x1001F2B0u
#define BR_FN_CEC_1001FBE0  0x1001FBE0u

#define BR_FN_CB4_1001E170  0x1001E170u
#define BR_FN_CB4_10020380  0x10020380u
#define BR_FN_CB4_100205F0  0x100205F0u
#define BR_FN_CB4_10020860  0x10020860u

#define BR_FN_9EC_1001BC90  0x1001BC90u
#define BR_FN_9EC_1001C690  0x1001C690u
#define BR_FN_9EC_1001CA10  0x1001CA10u
#define BR_FN_9EC_1001CA90  0x1001CA90u

/* The geometry-mode bits 0x1001E7C0 tests.  Named by bit position only --
 * their meaning is not established by this packet. */
#define BR_GEO_BIT0         0x00000001u
#define BR_GEO_BIT9         0x00000200u
#define BR_GEO_BIT12        0x00001000u
#define BR_GEO_BIT13        0x00002000u
#define BR_GEO_BIT17        0x00020000u
#define BR_GEO_BIT18        0x00040000u
#define BR_GEO_BIT19        0x00080000u

/* 0x1001E7C0.  Called after every geometry-mode change; reads geoCur/geoPrev
 * directly and takes no arguments. */
void BrGbiGeoModeChanged(void);

/* 0x1001C820.  Selects pfn0A79EC from a (w0, w1) combine-mode pair and then
 * re-syncs pfn0A7A00 with the flag it just latched into f4C0DC0.
 *
 * GOTCHA: f4C0DC0 is zeroed on ENTRY, unconditionally, before any comparison.
 * Exactly one of the ten recognised pairs sets it back to 1.
 *
 * GOTCHA: one path leaves pfn0A79EC COMPLETELY UNCHANGED -- the
 * (0xFC127E08, 0xF3FFF2F8) pair when both f6C661C and f6C6624 are zero.
 * Every other path assigns it.
 *
 * GOTCHA: the tail only swaps pfn0A7A00 between 0x10021E80 and 0x10022480,
 * and only in the one direction that matches f4C0DC0.  Any other value it
 * happens to hold is left alone. */
void BrSub_1001C820(uint32_t w0, uint32_t w1);

/* ==================================================================== */
/* 4. 0x100020D0 -- format a duration                                   */
/* ==================================================================== */

/* sprintf(pszOut, "%d:%02d.%02d", minutes, seconds, hundredths) where the
 * hundredths come from __ftol(v * 100.0f).  The literal is the one at
 * 0x10094094 in the image and 100.0f is the constant at 0x1008F098; both were
 * read out of the DLL, not assumed.
 *
 * GOTCHA: every division is a signed truncate-toward-zero (the original's
 * magic-multiply sequences carry the `shr 31; add` correction).  A negative
 * `v` therefore yields "-1:-2.-30" style output, not a clamp.
 *
 * GOTCHA: __ftol maps out-of-range input to 0x80000000, so a huge or NaN `v`
 * produces the minutes/seconds decomposition of INT32_MIN rather than
 * saturating. */
void BrSub_100020D0(char *pszOut, float v);

/* ==================================================================== */
/* 5. 0x1003289F -- clamped scissor command                             */
/* ==================================================================== */

/* Clamps (x, y, w, h) to the screen bounds, optionally doubles all four, and
 * emits TWO eight-byte display-list commands: 0xE7000000/0 followed by a
 * 0xE2-tagged pair of packed 12-bit corners.
 *
 * GOTCHA: the clamp is asymmetric.  Underflow on x/y moves the origin AND
 * shortens the extent; overflow on x+w / y+h only shortens the extent.  The
 * extent is then floored at 0 but the origin is never re-checked, so an
 * entirely off-screen rectangle still emits a zero-size command rather than
 * being dropped.
 *
 * GOTCHA: the doubling happens AFTER clamping, so the emitted rectangle can
 * exceed the bounds it was just clamped to.
 *
 * GOTCHA: the corners go through `fild` -> float -> `__ftol`, i.e. int ->
 * float32 -> int.  Reproduced literally, so coordinates past 2^24 lose their
 * low bits exactly as in the original.  The scale factor at 0x1008F4EC is
 * 1.0f in the shipped DLL (read from the image), so this round trip is the
 * only thing it does.
 *
 * GOTCHA: only the FIRST of the two packed words carries the 0xE2000000 tag;
 * the second is a bare corner pair. */
typedef struct BrScissorClamp {
    int32_t   minX;      /* 0x10575508 */
    int32_t   maxX;      /* 0x10575500 */
    int32_t   minY;      /* 0x1057550C */
    int32_t   maxY;      /* 0x105754FC */
    int32_t   doubled;   /* 0x106C65E4 -- non-zero => shift all four left 1 */
    uint32_t *pDl;       /* 0x106C0680 -- display-list write cursor         */
} BrScissorClamp;

BrScissorClamp *BrScissorClampGetState(void);

#define BR_SCISSOR_TAG_SYNC  0xE7000000u
#define BR_SCISSOR_TAG_RECT  0xE2000000u

void BrSub_1003289F(int a, int b, int c, int d);

/* ==================================================================== */
/* 6. 0x10069490 -- already implemented, adapted                        */
/* ==================================================================== */

/* 0x10069490 IS br_pool.h's BrPoolAlloc; br_pool.h even names the address in
 * its first line.  It is NOT re-decompiled here.  slice2_15.h needs the
 * symbol in the original's no-argument, BrMat4-returning form, so this is a
 * two-line adapter over the existing implementation and nothing else.
 *
 * The pool's storage is not part of the port (the original's is the fixed
 * image range at 0x10AF9BC0); pBase must be set before the first call, the
 * same way slice4_51 hands out its state block. */
extern BrPool g_brPool10069490;

BrMat4 *BrSub_10069490(void);

/* ==================================================================== */
/* 7. 0x10004FC0 -- network keep-alive tick                             */
/* ==================================================================== */

/* GOTCHA (this is the whole function): the counter is only advanced while it
 * is ALREADY non-zero, and the tick that wraps it back to zero is exactly the
 * tick that does NOT send.  The send therefore happens on counter values
 * 1..26 and is skipped on the 27th, and a counter sitting at 0 never starts
 * on its own -- something else has to seed it.
 *
 * GOTCHA: the guarded region is tiny.  The mutex covers the counter only; the
 * five-condition test and the send are outside it.
 *
 * GOTCHA (an MSVC artifact that is nonetheless load-bearing): the `0` pushed
 * as the second slot of the 0x10004A10 call is left on the stack -- the
 * original pops only 4 bytes -- and is reused as the TENTH argument of the
 * 0x10004760 call.  0x10004A10 really does take one argument; slice1_02.h
 * models it with an extra state pointer.
 *
 * DEVIATION: WaitForSingleObject/ReleaseMutex are routed through slice1_02.h's
 * BrNetMutexLock / BrNetMutexUnlock, as slice4_50.h already does for the same
 * pattern at 0x100053F0. */
typedef struct BrNetKeepAlive {
    struct BrNetState *pNet;   /* owns 0x1022AF04 (mutex), 0x1022AAF4
                                * (counter) and 0x1022AF20 */
    int32_t  f22AF18;          /* 0x1022AF18 */
    int32_t  f22AF14;          /* 0x1022AF14 */
    int32_t  f6909E0;          /* 0x106909E0 */
    int32_t  fACEE50;          /* 0x10ACEE50 -- must be < f0BD3E0 to send */
    int32_t  f0BD3E0;          /* 0x100BD3E0 */
    int32_t  f22B34C;          /* 0x1022B34C */
    void    *p277B40;          /* 0x10277B40 -- its ADDRESS is argument 1 */
} BrNetKeepAlive;

BrNetKeepAlive *BrNetKeepAliveGetState(void);

/* The counter wraps at this value, i.e. `if (++n >= 27) n = 0`. */
#define BR_NET_KEEPALIVE_PERIOD 27

void BrNetKeepAliveTick(void);

/* ==================================================================== */
/* 8. 0x100765E0 -- matrix to quaternion                                */
/* ==================================================================== */

/* slice3_45.h guessed "almost certainly matrix-to-quaternion".  It is: the
 * four branches are the four standard Shepperd cases and the result is
 * BrVec4Normalise'd (0x100741B0) in place, which is what turns the
 * un-normalised numerator into the quaternion.
 *
 * NOTE ON THE LISTING: the packet's block for this address stops mid-function
 * at 0x1007664E; its "(112 bytes)" size is wrong.  The real function runs
 * 0x100765E0..0x100766FD (286 bytes) and tools/dumpasm.py reports the tail as
 * a separate "sub_10076650".  The banner address is correct -- this is a
 * function-boundary bug in the map, not a mispaired listing.
 *
 * Reading `m` as the row-major BrMat4 of br_mat.h, the four cases are:
 *
 *   m00 >= 0 && m11 + m22 >= 0 :  ( 1 + m00 + m11 + m22,
 *                                   m12 - m21, m20 - m02, m01 - m10 )
 *   m00 >= 0 && m11 + m22 <  0 :  ( m12 - m21,
 *                                   1 + m00 - m11 - m22, m01 + m10, m20 + m02 )
 *   m00 <  0 && m11 >= m22     :  ( m20 - m02, m01 + m10,
 *                                   1 - m00 + m11 - m22, m21 + m12 )
 *   m00 <  0 && m11 <  m22     :  ( m01 - m10, m20 + m02, m21 + m12,
 *                                   1 - m00 - m11 + m22 )
 *
 * then BrVec4Normalise.  Element 0 is the scalar, matching slice3_44.h's
 * "quat is stored SCALAR FIRST".
 *
 * GOTCHA: the OFF-DIAGONAL SIGNS ARE THE TRANSPOSE of the textbook form --
 * case 1 uses (m12 - m21) where the usual derivation uses (m21 - m12).  This
 * is consistent across all four cases, so it is the convention, not a bug.
 * Do not "fix" it.
 *
 * GOTCHA: the branch discriminants are NOT "largest diagonal element".  The
 * first test is `m00 >= 0`, not `trace >= 0`, and the second is
 * `m11 + m22 >= 0`.  For a proper rotation matrix these coincide with the
 * usual choice often enough to work, but they are not the same test, and for
 * a scaled or non-orthonormal input they pick a different case.
 *
 * GOTCHA: BrVec4Normalise has no zero-length guard (slice1_09.h), so a matrix
 * that lands on the degenerate case of its branch yields NaNs.
 *
 * DEVIATION: the original computes in x87 80-bit registers and rounds to
 * float only at each store; this is written in float.  The stored values can
 * differ in the last bit or two, and the two comparisons are made on
 * float-rounded sums here rather than on extended-precision ones.  There is
 * no portable way to reproduce 80-bit intermediates.
 *
 * The constants used are the image's: 0x1008FC60 = 0.0f, 0x1008FC74 = -1.0f,
 * 0x1008FC78 = 1.0f. */
void BrSub100765E0(const BrMat4 *pSrc, BrVec4 *pDst);

/* ====================================================================
 * SKIPPED, and why
 * ====================================================================
 *   0x1003CDA0  BrExt_1003CDA0 -- pure Win32/COM.  GlobalHandle,
 *               GlobalUnlock and GlobalFree on a GMEM handle plus a call
 *               through vtable slot 0x7C of the 0x10277B40 object.  Nothing
 *               is left once those are removed.  Returns 0x88770082 when
 *               that object is NULL, which is the only portable fact in it.
 *
 *   0x100443E0  BrExt_100443E0 -- C++ exception frame (fs:[0] handler
 *               0x1008D50B), `operator new(0xC8)`, a __thiscall constructor
 *               at 0x10048710, and two code addresses stored into object
 *               fields.  The object type is not established anywhere in the
 *               tree.  Portable in principle, not portable honestly.
 *
 *   0x10056A10  BrOptFn10056A10 -- 1498 bytes behind a C++ exception frame
 *               (handler 0x1008E9B3), with allocation, virtual dispatch and
 *               unwind-state bookkeeping interleaved.
 *
 *   0x10058750  BrOptFn10058750 / BrExt_10058750 -- 4109 bytes, same shape,
 *               and the packet asks for it under TWO different names with two
 *               different pointer types.  Skipped once, reported twice.
 *
 *   0x10062C50  BrSub10062C50 -- 1909 bytes of entity initialisation that
 *               writes offsets 0x164..0xE94 of the entity.  slice3_45.h's
 *               BrEnt models 0x140..0x1DC and 0x350..0xE28 as opaque padding,
 *               so porting it means writing through byte offsets into another
 *               header's struct.  Left for whoever extends BrEnt.
 *
 *               WHAT THE LISTING GIVES UP, so it does not have to be
 *               re-derived (all of this is arithmetic on the offsets, not
 *               inference):
 *
 *               - There are FIVE identical sub-objects at 0x164 + i*0x20C,
 *                 i = 0..4.  Within one of them: +0x04/+0x08/+0x0C/+0x10 are
 *                 back-pointers to the other four, +0x18 is a linked-list
 *                 head, +0x1C is a small int (1 for i=0, 2 for i=1..4),
 *                 +0x78 is a BrRbState and +0xBC the BrMat4 that
 *                 BrRbBuildMatrix(+0xBC, +0x78) fills.  0x10074870 is called
 *                 on the base of each.
 *               - i=0 is the body; i=1..4 are the wheels, and their order in
 *                 memory is LF, LR, RF, RR.  That is not a guess: the four
 *                 0x10008B80 calls at the end pass the format strings at
 *                 0x100B36D4 / 0x100B36B4 / 0x100B3694 / 0x100B3674, which
 *                 read "LF = ", "RF = ", "LR = ", "RR = ", against the
 *                 states at 0x3E8, 0x800, 0x5F4 and 0xA0C respectively.
 *                 (0x10008B80 is a bare `ret` in this build, so the calls
 *                 are no-ops -- but the strings still identify the corners.)
 *               - The whole body is skipped when pE->pRec (+0x29C4) is NULL;
 *                 only the two bytes at +0xE78 and +0xE80 are cleared on
 *                 that path.
 *               - +0x31C = (20.0f - (float)(int)pE->+0xE94 * -4.0f) * 16000.0f
 *                 with the three constants read from the image
 *                 (0x1008F89C = 20.0f, 0x1008F8C4 = -4.0f,
 *                 0x1008F8C8 = 16000.0f).
 *               - The 13 calls to 0x100746E0 are slice3_44.h's BrX100746E0,
 *                 whose last argument lands in slot 1.
 */

#endif /* SLICE5_62_H */
