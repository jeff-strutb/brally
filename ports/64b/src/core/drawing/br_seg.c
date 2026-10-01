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

/* 0x10018A10 (D3D twin 0x1002B9A0) -- calls 0x10018A30 BrRcaResetCounts
 * first, then stores the two args to the N64/host base globals. The port
 * writes through pMap instead. */
/* WHAT IT DOES: records where a chunk of N64 data used to live and where it
 * lives now, so that addresses inside it can be translated as the file is
 * walked. Every pointer in a loaded .rca or track file goes through this
 * mapping. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x104B16E4 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */  /* 0x104B16E0 */
/* BrRcaResetCounts: prototype in br_funcs.h */

/* WHAT IT DOES: point the address translator at a freshly loaded block of
 * game data. The disc still holds N64-format data full of N64 addresses, so
 * every pointer read out of it has to be rebased onto where the block
 * actually landed in memory; these two bases are what that rebasing uses.
 * Also resets the fixup counters for the new block.
 *
 * The block gets its own 32-bit address window (br_addr32.h): the fixed-up
 * slots in it keep the original's four bytes. */
/* @implements 0x10018A10 glide BrSegSetBases */
void BrSegSetBases(uint32_t n64Base, void *pHost)
{
    BrRcaResetCounts();
    g_brSegN64Base  = (int32_t)n64Base;
    g_brSegHostBase = (uint8_t *)pHost;
    br_addr32_window(pHost);
}
