/* br_bits.h -- bit latch, byte swapping and handle lookup, from BRD3D.dll. */
#ifndef BR_BITS_H
#define BR_BITS_H

#include <stdint.h>

/* 0x10035FA0  thiscall. Moves the bits selected by `mask` out of `pending`
 * and ORs them into `latched`, clearing them from pending. Bits not in the
 * mask stay pending. */
typedef struct BrBitLatch { uint32_t pending, latched; } BrBitLatch;
void BrBitLatchTake(BrBitLatch *pLatch, uint32_t mask);

/* 0x100383C0  byte-swap exactly THREE consecutive u32s in place -- a
 * big-endian Vec3, which is what the .rca payload stores. The original is
 * fully unrolled over offsets 0x00..0x0B, so the count is fixed at 3, not a
 * parameter. */
void BrSwapVec3(void *pv);

/* 0x10074030  bounds-checked handle table lookup.
 *
 * Valid handles are 1..0x12E inclusive: the original rejects anything below 1
 * (`cmp eax,1 / jb`) and anything >= 0x12F. HANDLE 0 IS RESERVED as null and
 * returns 0, so callers use 0 rather than -1 for "none".
 *
 * DEVIATION (was undocumented, corrected): the ORIGINAL TAKES ONE ARGUMENT.
 * The table address 0x11829370 is hardcoded into the instruction
 * (`mov eax,[eax*4+0x11829370]`), not passed in; the single argument is the
 * handle. Call sites confirm it -- 0x100602E0 does `push 0xAC / call /
 * add esp,4`. The table is a parameter here purely so the port has no
 * hardcoded absolute address. Do not read this signature as evidence about
 * the original's calling convention.
 *
 * Observed handle values are string-table ids (0xAA, 0xAC, 0xAD, 0xAE), so
 * this is very likely a localised string lookup rather than a generic object
 * table. */
#define BR_HANDLE_MIN 1
#define BR_HANDLE_MAX 0x12E
void *BrHandleLookup(void *const *apTable, uint32_t handle);

#endif /* BR_BITS_H */
