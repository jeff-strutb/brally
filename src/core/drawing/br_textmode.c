/* br_textmode.c -- drawing.  See br_textmode.h. */
#include "br_textmode.h"

#include <stddef.h>
#include <string.h>

#ifdef BR_MATCHING_BUILD
extern uint8_t  g_4B0360;
extern uint8_t  g_4B035C;
extern uint32_t g_2E5E98, g_2E54C0, g_2E5ECC;
extern uint32_t g_364308, g_363F68;
#else
uint8_t  g_4B0360;
uint8_t  g_4B035C;
uint32_t g_2E5E98, g_2E54C0, g_2E5ECC;
uint32_t g_364308[32];
uint32_t g_363F68[32];
#endif

/* WHAT IT DOES: turn off a one-byte text-pass flag sitting next to the
 * alignment byte.  The next string is drawn without that special pass. */
/* @implements 0x10019250 d3d BrClear_10019250 */
void BrClear_10019250(void)
{
    g_4B0360 = 0;
}

/* WHAT IT DOES: centre the next string the HUD/menu text emitter draws.
 * 0 is left, 1 is right, 2 is centre -- this is the centre setter. */
/* @implements 0x10019270 d3d BrSet_10019270 */
void BrSet_10019270(void)
{
    g_4B035C = 2;
}

/* WHAT IT DOES: wipe two 128-byte scratch tables used by a later pass. */
/* @implements 0x1000F620 d3d BrClearTables_1000F620 */
void BrClearTables_1000F620(void)
{
    memset(&g_364308, 0, 0x80);
    memset(&g_363F68, 0, 0x80);
}

/* WHAT IT DOES: rewind a two-word cursor so the next read starts at the
 * beginning. */
/* @implements 0x10073B90 d3d BrPairReset_10073B90 */
void BR_THISCALL1 BrPairReset_10073B90(uint32_t *pThis)
{
    pThis[0] = 0;
    pThis[1] = 0;
}

/* WHAT IT DOES: rebuilds the clipper's free list: 64 vertex nodes chained
 * together, last node first. */
void BrNodeChainReset_1000F460(void)
{
    uint32_t *p = &g_2E5E98;
    uint32_t prev = 0;

    do {
        *p = prev;
        prev = (uint32_t)(uintptr_t)p;
        p = (uint32_t *)((char *)p - 0x28);
    } while ((int)(uintptr_t)p >= (int)(uintptr_t)&g_2E54C0);
    g_2E5ECC = prev;
}
