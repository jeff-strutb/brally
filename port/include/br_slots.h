/* br_slots.h -- fixed 8-entry slot table, decompiled from BRD3D.dll (0x100586A0).
 *
 * A small table of 12-byte records reset to an "empty" state. Derived from
 * the original's loop bounds: it walks a cursor from 0x10AA253C to
 * 0x10AA259C in steps of 12, writing [cursor-4], [cursor], [cursor+4] --
 * so records begin at 0x10AA2538 and (0x259C-0x253C)/12 = 8 exactly.
 *
 * The empty marker is -1, not 0: the original stores 0xFFFFFFFF into the
 * first field and zero into the other two. Callers therefore test for -1,
 * and zero is a VALID id.
 */
#ifndef BR_SLOTS_H
#define BR_SLOTS_H

#define BR_SLOT_COUNT 8
#define BR_SLOT_EMPTY (-1)

typedef struct BrSlot {
    int id;      /* -1 when free */
    int a;
    int b;
} BrSlot;

/* ERRATUM -- THIS STRUCT IS NOT THE REAL MEMORY LAYOUT.
 *
 * The slot array really lives at 0x10AA2538..0x10AA2598 (8 x 12 bytes) and the
 * word below at 0x10AA288C, which is **756 bytes (0x2F4) further on** -- they
 * are NOT adjacent and are NOT one object. Verified arithmetically.
 *
 * Worse, 0x10AA288C is not a count at all in most of its uses: other modules
 * set it to 1 and test it as a FLAG, and it doubles as the DirectPlay send
 * gate (see the WARNING at the foot of this file).
 *
 * This struct is retained only because earlier modules already compile against
 * it, and it is self-consistent for callers that use BrSlotsReset/FindFree on
 * their own instance. Do NOT overlay it on the real globals, and do NOT assume
 * `count` aliases 0x10AA288C. New code should take the two separately, as
 * slice2_25 does (`g_aBrAA2538[8]` and `g_brAA288C`).
 */
typedef struct BrSlotTable {
    BrSlot aSlots[BR_SLOT_COUNT];
    int    count;               /* NOT at 0x10AA288C -- see erratum above */
} BrSlotTable;

/* 0x100586A0, the LOOP HALF ONLY: mark all eight slots free.
 *
 * This exists because 0x100586A0 has two halves that live in different places
 * on a 64-bit host. The loop walks the eight records at 0x10AA2538; the single
 * `mov [0x10AA288C], 0` before it touches a dword 756 bytes past the end of
 * that array, which the erratum above explains is NOT part of the same object.
 *
 * BrSlotsReset (the instance form, used by callers that own a BrSlotTable) and
 * slice6_77.c's BrSub100586A0 (the argumentless original form, over
 * slice2_25's g_aBrAA2538 / g_brAA288C) both call THIS. One address, one body:
 * writing the loop out twice would be the "second body for one original
 * address" hazard CONVENTIONS.md warns about -- it links clean and drifts. */
void BrSlotsResetArray(BrSlot *aSlots);

/* 0x100586A0  mark every slot free and reset the counter. */
void BrSlotsReset(BrSlotTable *pTable);

/* Convenience: first free slot index, or -1. Not in the original. */
int  BrSlotsFindFree(const BrSlotTable *pTable);

/* WARNING -- 0x10AA288C IS DUAL-PURPOSE.
 *
 * The same global that holds this table's `count` is ALSO the DirectPlay send
 * gate: `0x1003D950`-family senders (tags 2,3,4,5) transmit only when it is
 * zero. So a NON-EMPTY SLOT TABLE SILENTLY SUPPRESSES those network messages.
 * Tags 6, 7 and 8 ignore the gate and send regardless.
 *
 * This coupling is invisible in either module alone -- it was found only by
 * noticing the two addresses coincide. Do not "clean up" by giving the slot
 * table its own counter without tracing every reader of 0x10AA288C first.
 */

#endif /* BR_SLOTS_H */
