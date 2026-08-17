/* br_seg.c -- N64 pointer rebasing. See br_seg.h.
 *
 * Transcribed from 0x1002B970. Note the asymmetry: a pointer *below* the base
 * is zeroed rather than clamped or passed through, so unresolvable references
 * become null and the caller's null checks catch them. Preserved exactly --
 * "fixing" it to pass through would turn a caught null into a wild pointer.
 */
#include "br_seg.h"

void BrSegFixup(const BrSegMap *pMap, uint32_t *pPtr)
{
    uint32_t v = *pPtr;

    if (v == 0)
        return;                        /* null stays null */
    if (v < pMap->n64Base) {
        *pPtr = 0;                     /* below the region: unresolvable */
        return;
    }
    *pPtr = v - pMap->n64Base + pMap->hostBase;
}

uint32_t BrSegResolve(const BrSegMap *pMap, uint32_t n64Addr)
{
    if (n64Addr == 0 || n64Addr < pMap->n64Base)
        return 0;
    return n64Addr - pMap->n64Base + pMap->hostBase;
}

/* 0x1002B9A0 -- the original also calls 0x1002B9C0 first (not yet
 * decompiled); that call does not touch either base, so the assignment
 * semantics below are complete. */
/* WHAT IT DOES: records where a chunk of N64 data used to live and where it
 * lives now, so that addresses inside it can be translated as the file is
 * walked. Every pointer in a loaded .rca or track file goes through this
 * mapping. */
/* @implements 0x1002B9A0 d3d BrSegSetBases */
void BrSegSetBases(BrSegMap *pMap, uint32_t n64Base, uint32_t hostBase)
{
    pMap->n64Base  = n64Base;
    pMap->hostBase = hostBase;
}
