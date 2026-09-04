/* br_u16cursor.c -- gamedata: read cursors over the u16 index tables.
 *
 * Filed out of the address batches: these functions were matched first and
 * grouped by what they are afterwards. Every function carries its original
 * address.
 */
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include "slice1_01.h"

/* ---------------------------------------------------------------------------
 * 0x10002EF0 -- read one u16 through a cursor and advance it.
 *
 * Both cursor fields are rewritten from one dword: the decremented count is
 * built as `(remaining + 0xFFFF) << 16` and the incremented position is ORed
 * into the low half.
 *
 * BUG PRESERVED: pos + 1 is computed in 32 bits and ORed in before the split,
 * so a position of 0xFFFF sets bit 16 and corrupts the count into
 * (remaining - 1) | 1 instead of carrying. Unreachable for tables under 64K
 * entries, faithful either way.
 *
 * The original returns AX only; the upper half of EAX is left holding part of
 * the table pointer on the success path and untouched on the failure path
 * (`xor ax,ax` is a 16-bit clear). Hence the uint16_t return type.
 *
 * DEVIATION: the table base was the global at 0x106C7C68; it is a parameter.
 */
/* WHAT IT DOES: reads the next entry from a table and moves the reader on by
 * one, counting down how many are left. Running off the end answers zero and
 * leaves the reader where it was -- but zero is also a perfectly valid entry,
 * so a caller cannot tell the two apart. */
/* @implements 0x10002EF0 d3d BrU16CursorNext */
#ifdef BR_MATCHING_BUILD
/* The original takes only the cursor; the table is the global at 0x106C7C68.
 * The portable prototype keeps pTable as an explicit argument. */
const uint16_t *g_br6C7C68;   /* 0x106C7C68 */

uint16_t BrU16CursorNext(BrU16Cursor *pCur)
{
    uint16_t rem;
    uint16_t pos;
    uint32_t packed;

    rem = pCur->remaining;
    if (rem) {
        pos = pCur->pos;
        packed = ((uint32_t)rem + 0xFFFFu) << 16 | ((uint32_t)pos + 1u);
        pCur->pos       = (uint16_t)packed;
        pCur->remaining = (uint16_t)(packed >> 16);
        return g_br6C7C68[pos];
    }
    return 0;
}
#else
uint16_t BrU16CursorNext(const uint16_t *pTable, BrU16Cursor *pCur)
{
    uint32_t rem, pos, packed;

    rem = pCur->remaining;
    if (rem == 0u) {
        return 0u;
    }

    pos = pCur->pos;
    packed = (uint32_t)((rem + 0xFFFFu) << 16) | (pos + 1u);

    pCur->pos       = (uint16_t)(packed & 0xFFFFu);
    pCur->remaining = (uint16_t)((packed >> 16) & 0xFFFFu);

    return pTable[pos];
}
#endif
