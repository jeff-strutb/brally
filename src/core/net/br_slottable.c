/* br_slottable.c -- net.
 *
 * The eight-entry player-slot table: emptying it at the start or end of a
 * session, and toggling one player's flag.
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
#include <stddef.h>
#include <stdint.h>

#include "br_slots.h"    /* BrSlot */
#include "slice2_25.h"   /* g_aBrAA2538, g_brAA288C, g_brPA9D008 */

/* ==========================================================================
 * 0x100586A0
 * ========================================================================== */

/* WHAT IT DOES: clears the player slot table and its counter at the start or
 * end of a session. Beware that the counter it clears doubles as the gate
 * that permits network sends, so clearing it here re-opens that gate --
 * behaviour of the original, not of this port. */
/* @implements 0x100586A0 d3d BrSub100586A0 */
void BrSub100586A0(void)
{
    /* Cursor on field `a` (0x10AA253C): stores are [eax-4]/[eax]/[eax+4],
     * bound 0x10AA259C. Zero is reused from edx; -1 from ecx. The original
     * compare is signed (`jl`). */
    int32_t *p;
    int32_t nZero;
    int32_t nEmpty;

    nZero = 0;
    p = (int32_t *)((char *)g_aBrAA2538 + 4);
    g_brAA288C = nZero;
    nEmpty = -1;
    do {
        p[-1] = nEmpty;
        p[0]  = nZero;
        p[1]  = nZero;
        p += 3;
#ifdef BR_MATCHING_BUILD
    } while ((int32_t)p < (int32_t)((char *)g_aBrAA2538 + 0x64));
#else
    } while (p < (int32_t *)((char *)g_aBrAA2538 + 0x64));
#endif
}

/* 0x10058700 / Glide 0x100515B0, 72 bytes.
 *
 * The original's loop bound is the address 0x10AA2598, reached from
 * 0x10AA2538 in steps of 12 -- eight records, which is br_slots.h's
 * BR_SLOT_COUNT.  The match key is the record's FIRST dword (the id) against
 * the UI object's +0x08.
 */
/* WHAT IT DOES: flips one player slot's flag on or off and reports the new
 * value, finding the slot by matching an identifier against the one the
 * current object carries. Nothing happens, and it answers no, if there is no
 * such slot. */
/* @implements 0x10058700 d3d BrSub10058700 */
int BrSub10058700(void)
{
    int i;

    if (g_brPA9D008 == NULL) {
        return 0;
    }

    for (i = 0; i < BR_SLOT_COUNT; ++i) {
        if (g_aBrAA2538[i].id == g_brPA9D008->f08) {
            /* `sete cl` on the OLD value, store, then re-read for the
             * return -- the original really does load it back. */
            g_aBrAA2538[i].a = (g_aBrAA2538[i].a == 0) ? 1 : 0;
            return g_aBrAA2538[i].a;
        }
    }
    return 0;
}

/* 0x10036080 (Glide), 123 bytes.
 *
 * The walk is a pointer stepped by 12 with a parallel int index, the bound
 * being the address past the eighth record, compared SIGNED (`jl`) -- the
 * same shape as 0x100586A0 above.  The free-slot index is kept in a CHAR
 * (`or bl,0xff` / `test bl,bl` / `movsx`), and both arms of the final
 * assignment write both flags.
 */
/* WHAT IT DOES: registers an id in the eight-entry player-slot table.  If the
 * id is already there its `b` flag is set; otherwise the first free slot
 * (id == -1) takes it, with `a` set when the id is 1 (the host), and `b`
 * set.  A full table drops the id silently. */
/* @implements 0x10036080 glide BrSlotMark */
void BrSlotMark(int id)
{
    char    iFree = -1;
    int     i = 0;
    BrSlot *p;

#ifdef BR_MATCHING_BUILD
    for (p = g_aBrAA2538; (int)p < (int)&g_aBrAA2538[BR_SLOT_COUNT]; p++) {
#else
    for (p = g_aBrAA2538; p < &g_aBrAA2538[BR_SLOT_COUNT]; p++) {
#endif
        if (p->id == id) {
            g_aBrAA2538[i].b = 1;
            return;
        }
        if (p->id == -1 && iFree < 0)
            iFree = (char)i;
        i++;
    }
    if (iFree >= 0) {
        g_aBrAA2538[iFree].id = id;
        if (id == 1) {
            g_aBrAA2538[iFree].a = 1;
            g_aBrAA2538[iFree].b = 1;
        } else {
            g_aBrAA2538[iFree].a = 0;
            g_aBrAA2538[iFree].b = 1;
        }
    }
}
