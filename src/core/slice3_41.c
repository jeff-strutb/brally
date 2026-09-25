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


