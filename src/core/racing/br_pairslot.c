/* br_pairslot.c -- racing.
 *
 * Resetting one two-field slot record. Filed out of slice2_19.c.
 */
#ifdef BR_MATCHING_BUILD
/* The original binary is /MD: CRT calls resolve through the import table. */
#define _CRTIMP __declspec(dllimport)
#endif
#include "slice2_19.h"

/* 0x10035041 */
/* WHAT IT DOES: clears one field of a two-field record and sets the other to
 * the value given. What the record is for was not established, so nothing
 * beyond that can honestly be said. */
/* @implements 0x10035041 d3d BrPairSlotReset */
void BrPairSlotReset(BrPairSlot *p, uint32_t v)
{
    p->f04 = 0;
    p->f08 = v;
}
