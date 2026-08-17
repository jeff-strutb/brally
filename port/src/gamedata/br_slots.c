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
