/* br_crt.h -- the statically linked MSVC CRT functions the ported modules call.
 *
 * These are NOT decompiled and must not be. Everything at or above 0x1007CC40
 * in BRD3D.dll is MSVC's own CRT (established from `_cexit`'s body). Porting it
 * would be re-implementing Microsoft's 1997 C library for no benefit.
 *
 * Instead this module supplies host-CRT equivalents under the names the pass
 * modules already declare, so the tree links. Each entry records the original
 * address and any behaviour that differs from the naive host equivalent.
 */
#ifndef BR_CRT_H
#define BR_CRT_H

#include <stdint.h>

/* 0x1007DFE0 -- `operator new`, i.e. _nh_malloc(cb, 1).
 * DOES NOT ZERO. Several modules allocate 0xC8-byte objects here and rely on
 * a constructor to fill them; anything the ctor misses is garbage. */
void *BrOperatorNew(uint32_t cb);

/* 0x1007DE40 -- `operator delete` -> free. 511 call sites in the original. */
void  BrOperatorDelete(void *p);

/* 0x1007C8A0 -- MSVC `__ftol`: truncates toward zero, and stores the LOW DWORD
 * of a 64-bit fistp. Out-of-range input yields the x87 "indefinite" value
 * 0x80000000 rather than saturating -- several modules depend on that, notably
 * the ones whose divide-by-zero paths feed it infinities. */
int32_t BrFtolTrunc(float f);

#endif /* BR_CRT_H */
