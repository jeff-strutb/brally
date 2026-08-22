/* br_slots.c -- see br_slots.h. Transcribed from 0x100586A0. */
#include "br_slots.h"

/* The loop half of 0x100586A0. See the header for why it is separate: the
 * counter the original clears is not adjacent to the array on this host, so
 * the two callers supply it themselves. */
void BrSlotsResetArray(BrSlot *aSlots)
{
    int i;

    for (i = 0; i < BR_SLOT_COUNT; i++) {
        aSlots[i].id = BR_SLOT_EMPTY;
        aSlots[i].a  = 0;
        aSlots[i].b  = 0;
    }
}

void BrSlotsReset(BrSlotTable *pTable)
{
    pTable->count = 0;
    BrSlotsResetArray(pTable->aSlots);
}

/* Not part of the original; provided so callers do not open-code the -1 test
 * and accidentally treat id 0 as free. */
int BrSlotsFindFree(const BrSlotTable *pTable)
{
    int i;
    for (i = 0; i < BR_SLOT_COUNT; i++)
        if (pTable->aSlots[i].id == BR_SLOT_EMPTY)
            return i;
    return -1;
}

#ifdef BR_MATCHING_BUILD
extern BrSlot g_aBrAA2538[BR_SLOT_COUNT];

/* WHAT IT DOES: walks the eight fixed slots at 0x10AA2538 and, for each
 * record whose `b` is already 0, writes id = -1, a = 0, b = 0. */
/* @implements 0x1003CA70 d3d BrSlotsResetIfBZero */
void BrSlotsResetIfBZero(void)
{
    int *p;
    int nZero;
    int nEmpty;

    nZero = 0;
    p = (int *)((char *)g_aBrAA2538 + 8);
    nEmpty = -1;
    do {
        if (*p == nZero) {
            p[-2] = nEmpty;
            p[-1] = nZero;
            p[0]  = nZero;
        }
        p += 3;
    } while ((int)p < (int)((char *)g_aBrAA2538 + 0x68));
}

/* WHAT IT DOES: looks a player slot up by its identifier and hands back the
 * value stored next to that id, or zero if none of the eight slots carry it. */
/* @implements 0x100586D0 d3d BrSlotsFindById */
int BrSlotsFindById(int id)
{
    int i;

    for (i = 0; i < BR_SLOT_COUNT; i++) {
        if (g_aBrAA2538[i].id == id)
            return g_aBrAA2538[i].a;
    }
    return 0;
}
#endif
