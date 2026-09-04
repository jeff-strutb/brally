/* br_devrec.c -- controls: the input-device record table.
 *
 * Responsibility: reading what the player is doing. The game keeps a fixed
 * set of device records, reached through an index table rather than in order,
 * and this module answers questions about that table.
 *
 * Moved out of src/core/slice1_06.c (an address batch) unchanged. The
 * preamble below is carried over verbatim from that file, including the
 * matching-build renames: they decide the set of names the translation unit
 * sees, and trimming them changes the compiler's view of the code.
 */

#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
/* The original BrOptSave takes no arguments (loose globals in, packed
 * array out); hide the header's port prototype behind a rename so the
 * matching twin can define the real symbol -- the slice5_63.c caller keeps
 * the port signature (cdecl, extra args harmless at run time). */
#define BrOptSave   BrOptSave_hdr
#define BrOptAvailB BrOptAvailB_hdr
#ifdef BR_MATCHING_BUILD
/* The original BrNameListInit is a thiscall ctor with no stack args (vtbl
 * and fill string are fixed); hide the port's 3-arg prototype. */
#define BrNameListInit BrNameListInit_port
#include "slice1_06.h"
#undef BrNameListInit
#else
#include "slice1_06.h"
#endif
#undef BrOptSave
#undef BrOptAvailB
#else
#include "slice1_06.h"
#endif

#include <stdlib.h>
#include <string.h>

/* Layout facts the original's arithmetic depends on. */
typedef char br06_assert_devrec[
    (sizeof(BrDevRec) == BR_DEVREC_STRIDE) ? 1 : -1];

/* ==========================================================================
 * 0x10037070
 * ========================================================================== */

/* WHAT IT DOES: answers whether any of a fixed set of records -- reached
 * through an index table rather than in order -- is both in use, of one
 * particular kind, and already holding the value being asked about. It reads
 * like a "is this already taken?" test for input-device assignments, but
 * nothing in this packet confirms that, so treat the purpose as unconfirmed. */
BrDevCtx *g_brP6EECCC;                      /* 0x106EECCC */

/* @implements 0x10037070 d3d BrDevRecMatch */
int BrDevRecMatch(uint32_t value)
{
    int32_t i;

    for (i = 0; i < BR_DEVREC_SLOTS; i++) {
        const BrDevRec *pRec = &g_brP6EECCC->pRecs[g_brP6EECCC->abIndex[i]];

        if (pRec->f04 == 0u) {
            continue;
        }
        if ((pRec->f20 & BR_DEVREC_TYPE_MASK) != BR_DEVREC_TYPE_MATCH) {
            continue;
        }
        if (pRec->f04 == value) {
            return 1;
        }
    }
    return 0;
}
