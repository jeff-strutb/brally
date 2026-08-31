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
#ifdef BR_MATCHING_BUILD
extern int32_t g_brSegN64Base;   /* 0x104B16E4 */
extern int32_t g_brSegHostBase;  /* 0x104B16E0 */
void BrRcaResetCounts(void);

/* @implements 0x10018A10 glide BrSegSetBases */
void BrSegSetBases(BrSegMap *pMap, uint32_t n64Base, uint32_t hostBase)
{
    /* Orig is two stack args; the unused third is the port's pMap slot. */
    BrRcaResetCounts();
    g_brSegN64Base  = (int32_t)pMap;     /* arg1 */
    g_brSegHostBase = (int32_t)n64Base;  /* arg2 */
    (void)hostBase;
}
#else
void BrSegSetBases(BrSegMap *pMap, uint32_t n64Base, uint32_t hostBase)
{
    pMap->n64Base  = n64Base;
    pMap->hostBase = hostBase;
}
#endif
