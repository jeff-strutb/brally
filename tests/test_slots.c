#include "br_slots.h"
#include <stdio.h>
#include <string.h>

static int g_fail;
static void check(int c, const char *w)
{ printf("  [%s] %s\n", c ? "PASS" : "FAIL", w); if (!c) g_fail = 1; }

int main(void)
{
    BrSlotTable t;
    int i, allEmpty = 1;

    memset(&t, 0x5A, sizeof(t));
    BrSlotsReset(&t);

    for (i = 0; i < BR_SLOT_COUNT; i++)
        if (t.aSlots[i].id != BR_SLOT_EMPTY || t.aSlots[i].a || t.aSlots[i].b)
            allEmpty = 0;
    check(allEmpty, "all 8 slots reset to {-1,0,0}");
    check(t.count == 0, "counter reset");
    check(sizeof(BrSlot) == 12, "record is 12 bytes as in the original");

    check(BrSlotsFindFree(&t) == 0, "first free slot after reset is 0");

    /* id 0 must count as OCCUPIED -- the empty marker is -1, not 0 */
    t.aSlots[0].id = 0;
    check(BrSlotsFindFree(&t) == 1, "id 0 is occupied, not free");

    for (i = 0; i < BR_SLOT_COUNT; i++) t.aSlots[i].id = i;
    check(BrSlotsFindFree(&t) == -1, "full table reports no free slot");

    printf(g_fail ? "\nFAILED\n" : "\nALL PASSED\n");
    return g_fail;
}
