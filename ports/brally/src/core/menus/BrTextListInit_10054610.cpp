/* WHAT IT DOES: sets up a list of text rows -- the widget behind the game's
 * scrolling menus and high-score tables. It builds a hundred empty rows,
 * clears the scroll bookkeeping, plants four "none" markers that get their
 * real values later, and clears the hundred slots that can hold an
 * arbitrary lump of data alongside each row. */
/* @implements 0x10054610 glide BrTextListInit
 * @cpp_kind method
 * @cpp_symbol ??0TextList54610@@QAE@XZ
 *
 * Thiscall constructor, no EH frame: the hundred-item array is the last
 * member with a destructor, so nothing after it can throw.  The three
 * dwords before the array and everything from +0x1A92C on are member
 * initialisers (declaration order); +0x04..+0x14 and the blob memset are
 * the body.  The C twin in slice3_39.c stops at 160/218 B because a C
 * caller cannot emit the vector-constructor-iterator call.
 */
#define _CRTIMP __declspec(dllimport)
#include <string.h>
#include "slice3_39.h"   /* BrTextList, the canonical record */

#include "br_vtables.h"

/* The list constructor: 0x10074800 runs the text-box constructor
 * (0x10053E70) over the hundred rows, then the scalars, then the vtable
 * (0x10077720), then the body. */
extern "C" void *BrTextListInit(void *self)
{
    BrTextList *l = (BrTextList *)self;
    int i;

    for (i = 0; i < BR_TEXTLIST_ITEMS; i++)
        BrTextBoxInit(&l->aItems[i]);
    l->f18 = 0;
    l->f1C.u = 0;
    l->f20.u = 0;
    l->count = 0;
    l->f1A92E = 0;
    l->f1A930 = 0;
    l->f1A932 = -1;
    l->f1A934 = -1;
    l->f1A936 = -1;
    l->f1A938 = -1;
    for (i = 0; i < 14; i++)
        l->f1A99C[i].u = 0;
    l->pVtbl = (const BrTextListVtbl *)g_brVtbl_10077720;

    l->f04 = 0;
    l->f08 = 0;
    l->f0C = 0;
    l->f10 = 0;
    l->f14 = 0;
    memset(l->aBlobs, 0, sizeof l->aBlobs);
    return self;
}
