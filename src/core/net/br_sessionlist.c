/* br_sessionlist.c -- net.
 *
 * The lobby's session list: walk every row of the widget at 0x10AA29D4 and
 * let the nested list object act on each one.
 *
 * Filed out of the address batches: these functions were
 * matched first and grouped by what they are afterwards.
 * Every function carries its original address.
 */

#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include <stdint.h>

#include "slice6_70.h"

#ifdef BR_MATCHING_BUILD
/* The same two shapes BrSub1003C9B0 uses in slice6_70.c; repeated here
 * because a typedef cannot cross a translation unit. */
typedef struct { int i; } BrC9B0Arg;
typedef struct BrC9B0Vtbl {
    void *pad[11];
    void (__fastcall *f2C)(void *pThis, BrC9B0Arg a);
} BrC9B0Vtbl;
#endif

/* WHAT IT DOES: the same walk as BrSub1003C9B0, over the list object at
 * 0x10AA29D4 instead of 0x10AA29E4. */
#ifdef BR_MATCHING_BUILD
/* @implements 0x1003D070 d3d BrSub1003D070 */
#endif
void BrSub1003D070(void)
{
    unsigned char *p;
    unsigned int   n;
    unsigned int   i;

    p = (unsigned char *)g_brPAA29D4;
    if (p != NULL) {
        n = 0;
        i = 0;
        n = *(unsigned short *)(p + 0x1E164);
        if (n > 0) {
            do {
#ifdef BR_MATCHING_BUILD
                BrC9B0Arg a;
#endif
                unsigned char *pSub;

                pSub = (unsigned char *)g_brPAA29D4 + 0x3838;
#ifdef BR_MATCHING_BUILD
                a.i = (int)i;
                (*(BrC9B0Vtbl **)pSub)->f2C(pSub, a);
#else
                {
                    void **pVtbl = *(void ***)pSub;
                    ((void (*)(void *, unsigned))pVtbl[11])(pSub, i);
                }
#endif
                i++;
            } while (i < n);
        }
    }
}
