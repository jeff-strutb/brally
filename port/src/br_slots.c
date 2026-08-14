/* br_slots.c -- see br_slots.h. Transcribed from 0x100586A0. */
#include "br_slots.h"

void BrSlotsReset(BrSlotTable *pTable)
{
    int i;

    pTable->count = 0;
    for (i = 0; i < BR_SLOT_COUNT; i++) {
        pTable->aSlots[i].id = BR_SLOT_EMPTY;
        pTable->aSlots[i].a  = 0;
        pTable->aSlots[i].b  = 0;
    }
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
