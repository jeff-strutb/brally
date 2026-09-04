/* br_keycache.c -- gamedata: the key-record cache.
 *
 * A cache of fixed-size records keyed by a 64-byte key, searched by a plain
 * linear scan. Filed out of slice2_12.c section 7.
 *
 * See slice2_12.h for the recovered layouts.
 */
#ifdef BR_MATCHING_BUILD
/* Header prototype is cdecl; the original is thiscall.  Rename the
 * prototype so the thiscall definition is not a C2373 redefinition. */
#define BrKeyCacheReset BrKeyCacheReset_cdecl_hdr
#define BrKeyCacheFind  BrKeyCacheFind_cdecl_hdr
#endif
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include "slice2_12.h"
#ifdef BR_MATCHING_BUILD
#undef BrKeyCacheReset
#undef BrKeyCacheFind
/* 0x1007DE40 -- local `operator delete`, an E8, not CRT free (FF 15). */
extern void BrOperatorDelete(void *p);
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* =====================================================================
 * 7. Key-record cache
 * ===================================================================== */

/* 0x10008670 */
/* WHAT IT DOES: looks through a cache for the record whose 64-byte key
 * matches the one asked for, and reports its position, or -1 if there is no
 * match. Only the middle of each record takes part in the comparison; the
 * first few words are payload the search ignores. What the cache holds is
 * not established here. */
#ifdef BR_MATCHING_BUILD
/* Orig is thiscall, one stack arg (the vtbl[1] argument, not the key).
 * vtbl[1](this, arg, &key) is thiscall / ret 8; the search is 16 unrolled
 * dword compares at +0x0C of each 0x4C-byte record. */
typedef void (__fastcall *BrKeyBuildFn)(BrKeyCache *pThis, int _edx,
                                        void *pArg, int32_t *pKey);

/* WHAT IT DOES: look up a cached entry by asking the cache's own key-
 * building function to turn the argument into a key, then scanning the
 * entries for a match. The cache is a plain linear scan, so it is sized for
 * tens of entries and not thousands. */
/* @implements 0x10008850 glide BrKeyCacheFind */
int32_t __fastcall BrKeyCacheFind(BrKeyCache *pCache, int _edx, void *pArg)
{
    int32_t          key[16];
    int32_t          i;
    uint32_t         n;
    BrKeyCacheEntry *pEnt;
    BrKeyBuildFn     pfn;

    pfn = *(BrKeyBuildFn *)((char *)pCache->pVtbl + 4);
    pfn(pCache, (int)pArg, pArg, key);

    pEnt = pCache->aEntries;
    n    = (uint32_t)pCache->cEntries;
    for (i = 0; (uint32_t)i < n; ++i) {
        if (pEnt[i].aKey[0]  != key[0])  continue;
        if (pEnt[i].aKey[1]  != key[1])  continue;
        if (pEnt[i].aKey[2]  != key[2])  continue;
        if (pEnt[i].aKey[3]  != key[3])  continue;
        if (pEnt[i].aKey[4]  != key[4])  continue;
        if (pEnt[i].aKey[5]  != key[5])  continue;
        if (pEnt[i].aKey[6]  != key[6])  continue;
        if (pEnt[i].aKey[7]  != key[7])  continue;
        if (pEnt[i].aKey[8]  != key[8])  continue;
        if (pEnt[i].aKey[9]  != key[9])  continue;
        if (pEnt[i].aKey[10] != key[10]) continue;
        if (pEnt[i].aKey[11] != key[11]) continue;
        if (pEnt[i].aKey[12] != key[12]) continue;
        if (pEnt[i].aKey[13] != key[13]) continue;
        if (pEnt[i].aKey[14] != key[14]) continue;
        if (pEnt[i].aKey[15] != key[15]) continue;
        return i;
    }
    return -1;
}
#else
/* WHAT IT DOES: the port spelling of the same lookup -- scan the cache's
 * entries for the one whose 64-byte key equals the key handed in and report
 * its position, or -1. The caller builds the key here rather than the cache
 * calling back into its own key-building function. */
/* @implements 0x10008850 glide BrKeyCacheFind */
int32_t BrKeyCacheFind(const BrKeyCache *pCache, const int32_t aKey[16])
{
    int32_t i;

    for (i = 0; i < pCache->cEntries; ++i) {
        int32_t j;
        int     fMatch = 1;

        for (j = 0; j < 16; ++j) {
            if (pCache->aEntries[i].aKey[j] != aKey[j]) {
                fMatch = 0;
                break;
            }
        }
        if (fMatch)
            return i;
    }
    return -1;                          /* `or eax,0xffffffff` */
}
#endif

/* 0x10008970 */
/* WHAT IT DOES: empties that cache: closes the file it was reading from,
 * frees the records, and zeroes the bookkeeping, leaving only the object's
 * first two words alone. */
/* @implements 0x10008970 d3d BrKeyCacheReset */
/* Original is __thiscall (`mov esi, ecx` / `ret`).  BR_THISCALL1 is the
 * single-arg fastcall spelling; the header stays cdecl. */
void BR_THISCALL1 BrKeyCacheReset(BrKeyCache *pCache)
{
    if (pCache->pFile != NULL)
        fclose(pCache->pFile);          /* 0x1007CD50  FF 15 */
    if (pCache->aEntries != NULL)
#ifdef BR_MATCHING_BUILD
        BrOperatorDelete(pCache->aEntries);  /* 0x1007DE40  E8, not free */
#else
        free(pCache->aEntries);
#endif

    pCache->aEntries = NULL;
    pCache->pFile    = NULL;
    pCache->f420     = 0;
    /* Four dwords at +0x008, then 0x100 dwords at +0x020.  The pointer
     * pair at +0x018 and f420 are stored separately, as in the original. */
    memset(&pCache->f008, 0, 16);
    memset(pCache->a020, 0, sizeof pCache->a020);
}

