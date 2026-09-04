/* br_name.c -- drawing.
 *
 * Filed out of the address batches: these functions were
 * matched first and grouped by what they are afterwards.
 * Every function carries its original address.
 */
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import
 * table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include <stdint.h>

#ifdef BR_MATCHING_BUILD

extern char DAT_10396f08[];
extern int  DAT_10077750;

BrNameList *__fastcall BrNameListInit(BrNameList *pThis, int _edx_unused)
{
    char *d = (char *)pThis->asz;
    int   n;

    (void)_edx_unused;
    pThis->pVtbl = (const void *)&DAT_10077750;
    memset(d, 0, sizeof(pThis->asz));

    n = BR_NAMELIST_COUNT;
    do {
        strcpy(d, DAT_10396f08);
        d += BR_NAMELIST_STRIDE;
    } while (--n != 0);

    return pThis;
}

#endif /* BR_MATCHING_BUILD */
