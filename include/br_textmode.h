/* br_textmode.h -- drawing: how the next string is aligned, and the
 * clipper's vertex free list.
 *
 * Responsibility: turn text and clip geometry into pixels.
 */
#ifndef BR_TEXTMODE_H
#define BR_TEXTMODE_H

#include <stdint.h>
#include "br_match.h"

/* 0x10019250  next string is not a special/heading pass. */
void BrClear_10019250(void);
/* 0x10019270  centre the next string (0 left, 1 right, 2 centre). */
void BrSet_10019270(void);
/* 0x1000F620  wipe two 128-byte scratch tables. */
void BrClearTables_1000F620(void);
/* 0x10073B90  rewind a two-word read cursor. */
void BR_THISCALL1 BrPairReset_10073B90(uint32_t *pThis);
/* 0x1000F460  rebuild the 64-node clip-vertex free list. */
void BrNodeChainReset_1000F460(void);

extern uint8_t  g_4B0360;
extern uint8_t  g_4B035C;
extern uint32_t g_2E5E98, g_2E54C0, g_2E5ECC;
#ifdef BR_MATCHING_BUILD
extern uint32_t g_364308;
extern uint32_t g_363F68;
#else
extern uint32_t g_364308[32];
extern uint32_t g_363F68[32];
#endif

#endif
