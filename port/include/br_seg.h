/* br_seg.h -- N64 pointer rebasing, decompiled from BRD3D.dll (0x1002B970).
 *
 * The .rca payload is N64 data (big-endian, byte-swapped at load), so any
 * pointer embedded in it refers to the original N64 address space. This
 * routine rebases such a pointer into the host heap:
 *
 *     if (*p == 0)          leave it (null stays null)
 *     else if (*p < base)   *p = 0        -- below the region: not resolvable
 *     else                  *p += (host - base)
 *
 * `base` is the N64-side origin (0x1057553C) and `host` the loaded address
 * (0x10575538). This is what F3D G_VTX segment addresses have to go through
 * before they can be dereferenced.
 */
#ifndef BR_SEG_H
#define BR_SEG_H

#include <stdint.h>

typedef struct BrSegMap {
    uint32_t n64Base;     /* 0x1057553C */
    uint32_t hostBase;    /* 0x10575538 */
} BrSegMap;

/* Rebase one embedded pointer in place. */
void BrSegFixup(const BrSegMap *pMap, uint32_t *pPtr);

/* Resolve without mutating; returns 0 for null and for unresolvable inputs. */
uint32_t BrSegResolve(const BrSegMap *pMap, uint32_t n64Addr);

/* 0x1002B9A0  install the two bases. Argument order is (n64Base, hostBase):
 * the original stores arg1 to 0x1057553C (the value later compared against,
 * i.e. the N64-side origin) and arg2 to 0x10575538 (the value added, i.e. the
 * host address). Getting these backwards silently inverts every fixup. */
void BrSegSetBases(BrSegMap *pMap, uint32_t n64Base, uint32_t hostBase);

#endif /* BR_SEG_H */
