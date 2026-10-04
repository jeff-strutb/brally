/* WHAT IT DOES: creates the object that holds one whole menu -- its pages,
 * which page is showing, and two lists of a hundred default names each,
 * pre-filled as "Driver 1", "Driver 2" and so on from a pattern in the
 * game's text table.  Either list failing to allocate raises the
 * out-of-memory error.  It leaves the "what to do when this menu opens"
 * slot untouched, so it holds rubbish until whoever created the menu fills
 * it in. */
/* @implements 0x10041B60 glide BrOptObjCtor
 * @cpp_kind method
 * @cpp_symbol ??0OptObj41B60@@QAE@XZ
 *
 * Thiscall constructor with EH: the two `new NameList` expressions are
 * unwind states 0 and 1 (the raw allocation is freed if the list's own
 * constructor throws), `this` spilled to the frame for the funclet.  The
 * seven stores before the vptr are member initialisers in declaration
 * order; the vptr lands before the body.  sprintf is the /MD import,
 * cached in ebp across the fill loop; the loop's test is on the
 * strength-reduced byte offset (`cmp esi,0x6590`).
 */
#define _CRTIMP __declspec(dllimport)
#include <stdio.h>
#include "br_phase.h"   /* BrPhase_, the canonical record */
#include <string.h>

#include "slice1_06.h"   /* BrNameList */
#include "br_vtables.h"

/* The phase constructor: scalars, the vtable (0x100776C8), then two
 * hundred-entry name lists (operator new + 0x100559B0), each filled with
 * the numbered default names. */
extern "C" void *BrOptObjCtor(void *self)
{
    BrPhase_ *ph = (BrPhase_ *)self;
    BrNameList *a, *b;
    int i;

    ph->pfnHook = 0;
    ph->f0C = 0;
    ph->nPages = 0;
    ph->iPage = 0;
    ph->pCur = 0;
    ph->f68 = 1;
    ph->fBC = 0;
    ph->pVtbl = (const BrPhaseVtbl_ *)g_brVtbl_100776C8;

    a = (BrNameList *)BrOperatorNew(sizeof(BrNameList));
    if (a != 0)
        a = BrNameListInit(a);
    ph->fC0 = a;
    if (a == 0)
        FUN_100378c0(6);
    b = (BrNameList *)BrOperatorNew(sizeof(BrNameList));
    if (b != 0)
        b = BrNameListInit(b);
    ph->fC4 = b;
    if (b == 0)
        FUN_100378c0(6);

    for (i = 0; i < 100; i++) {
        sprintf(a->asz[i], BrStrGet(0xBE), i);
        sprintf(b->asz[i], BrStrGet(0xBE), i);
    }

    memset(ph->aFlags, 0, sizeof ph->aFlags);
    return self;
}
