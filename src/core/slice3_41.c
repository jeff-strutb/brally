/* slice3_41.c -- Boss Rally (BRD3D.dll) slice 3, a later pass.
 *
 * See slice3_41.h for the packet inventory, the offsets that were recovered,
 * and the list of functions that were deliberately left out.
 *
 * Every x87 sequence in here was traced through its fxch chain; where the
 * original reads a status word twice and looks at different bits each time,
 * the C is written to reproduce that exactly (including what happens to a
 * NaN), not to look tidy.
 */
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "slice3_41.h"


/* =====================================================================
 * 1.  Driver records and the race-position sort
 * ===================================================================== */

/* The 8-byte element the original sorts.  `key` MUST come first: the
 * comparator dereferences the element pointer as a float directly. */
typedef struct BrRankPair {
    float   key;
    int32_t idx;
} BrRankPair;


/* The g_22AF18 == 0 half of 0x10066510. */
void BrRankAssign(BrDriver *pSlots, int32_t n)
{
    BrRankPair a[BR_RANK_MAX];
    int32_t    i, j, m = 0;

    for (i = 0; i < n; i++) {
        if ((pSlots[i].f68 & BR_DRIVER_SKIP) != 0)
            continue;

        /* DEVIATION: the original's pair buffer is a bare 0xA0-byte stack
         * array with no bound check, so a 21st participating slot smashes
         * the saved registers behind it.  Extra slots are dropped here
         * instead.  Everything at or below 20 participants is bit-identical. */
        if (m >= BR_RANK_MAX)
            break;

        a[m].idx = i;
        a[m].key = (pSlots[i].pCar != NULL) ? pSlots[i].pCar->fFF4
                                            : pSlots[i].f50;
        m++;
    }

    if (m != 0)
        qsort(a, (size_t)m, sizeof a[0], BrRankCmpKey);

    for (j = 0; j < m; j++) {
        BrDriver *pS = &pSlots[a[j].idx];

        /* Note `n`, not `m`: see the GOTCHA in the header. */
        if (pS->pCar != NULL)
            pS->pCar->fFF8 = n - j - 1;
        else
            pS->f54 = n - j - 1;
    }
}

/* =====================================================================
 * 2.  Variable-block save / restore
 * ===================================================================== */

/* 0x10067880 */
/* WHAT IT DOES: gathers a list of scattered game variables into one
 * contiguous block of memory -- the snapshot the replay and save-state code
 * works from. If the block turns out not to have been big enough it stops the
 * game with an error, but only after the overrun has already happened. */
/* @implements 0x10067880 d3d BrVarSave */
/* @n64 0x8022ADCC located */
void BrVarSave(const BrVarBlock *pTable, void *pDst, int32_t cbAvail)
{
    uint8_t *pOut = (uint8_t *)pDst;
    int32_t  cbUsed;

    while (pTable->pData != NULL) {
        memcpy(pOut, pTable->pData, (size_t)pTable->cb);
        pOut += pTable->cb;
        pTable++;
    }

    cbUsed = (int32_t)(pOut - (uint8_t *)pDst);
    if (cbUsed > cbAvail) {
        /* sprintf into an 80-byte stack buffer, as the original does --
         * confirmed against the N64 build (TGR USA 0x8022adcc), which
         * compiles the same source with plain sprintf + fatal. */
        char szMsg[0x50];
        sprintf(szMsg,
                "VAR SAVE OVERFLOW (%d avail, %d used)",
                (int)cbAvail, (int)cbUsed);
        BrFatal(szMsg);
    }
}

/* 0x10067900 */
/* WHAT IT DOES: puts a previously gathered snapshot back where it came from,
 * restoring every variable in the list. It trusts the buffer completely --
 * there is no length given and no check made. */
/* @implements 0x10067900 d3d BrVarLoad */
/* @n64 0x8022AE70 located */
void BrVarLoad(const BrVarBlock *pTable, const void *pSrc)
{
    const uint8_t *pIn = (const uint8_t *)pSrc;

    while (pTable->pData != NULL) {
        memcpy(pTable->pData, pIn, (size_t)pTable->cb);
        pIn += pTable->cb;
        pTable++;
    }
}


