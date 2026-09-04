/* br_uilistwalk.c -- menus: walk every row of the on-screen list the global
 * 0x10AA29E4 points at, asking the nested list object at +0x3838 to do slot
 * +0x2C for each row (0x1003C9B0 d3d / 0x10036040 glide).
 *
 * Filed out of slice6_70.c, whose preamble it keeps verbatim below so the
 * compiler's view of the body is unchanged.  The original banner follows.
 *
 * slice6_70.c -- Boss Rally (BRD3D.dll), slice 6, packet 70.
 *
 * See slice6_70.h for the packet's four name conflicts, the globals this file
 * owns, and the per-function GOTCHAs. The tail of this file records the
 * DEVIATIONs and the argument for each of the six addresses left out.
 *
 * Every function here was read out of work/slice6/packet70.asm at the address
 * its WANTED name encodes, and carries that address in its comment. All twelve
 * banner/body pairings in the packet were re-checked against the body's own
 * `sub_XXXXXXXX @ XXXXXXXX` line: all twelve agree.
 */

#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include <stdint.h>
#include <string.h>
#ifdef BR_MATCHING_BUILD
#include <stdio.h>
#endif
#include "slice1_03.h"      /* BrComCallLocked68 (0x1000C4D0) */

#include "slice6_70.h"

/* Owned by, and still defined in, slice6_70.c. */
extern uint8_t *g_brPAA29E4;

/* The vtable view is shared with BrSub1003D070, which stayed behind: these
 * are typedefs only, so each file keeps its own copy. */
#ifdef BR_MATCHING_BUILD
typedef struct { int i; } BrC9B0Arg;
typedef struct BrC9B0Vtbl {
    void *pad[11];
    void (__fastcall *f2C)(void *pThis, BrC9B0Arg a);
} BrC9B0Vtbl;
#endif


/* WHAT IT DOES: walks every row of one particular on-screen list -- the one
 * the global 0x10AA29E4 currently points at -- and asks the nested list
 * object at +0x3838 to do whatever its vtable slot +0x2C does for that row's
 * index. A missing pointer or a zero row-count is a no-op. The pointer is
 * re-read before every row because the call is allowed to replace it. */
#ifdef BR_MATCHING_BUILD
/* @implements 0x1003C9B0 d3d BrSub1003C9B0 */
/* @implements 0x10036040 glide BrSub1003C9B0 */
#endif
void BrSub1003C9B0(void)
{
    uint8_t  *pObj;
    unsigned  i;
    unsigned  n;

    pObj = g_brPAA29E4;
    if (pObj == NULL) {
        return;
    }
    n = *(uint16_t *)(pObj + 0x1E164);
    for (i = 0; i < n; i++) {
        uint8_t *pSub;
#ifdef BR_MATCHING_BUILD
        BrC9B0Arg a;
#endif
        pObj = g_brPAA29E4;
        pSub = pObj + 0x3838;
#ifdef BR_MATCHING_BUILD
        a.i = (int)i;
        (*(BrC9B0Vtbl **)pSub)->f2C(pSub, a);
#else
        {
            void **pVtbl = *(void ***)pSub;
            ((void (*)(void *, unsigned))pVtbl[11])(pSub, i);
        }
#endif
    }
}
