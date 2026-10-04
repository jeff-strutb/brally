/* br_textmode.h -- drawing: how the next string is aligned, and the
 * clipper's vertex free list.
 *
 * Responsibility: turn text and clip geometry into pixels.
 */
#ifndef BR_TEXTMODE_H
#define BR_TEXTMODE_H
#ifdef __cplusplus
extern "C" {  /* BR_CLINK_BEGIN: every original function has C linkage */
#endif

#include <stdint.h>
#include "br_match.h"

/* 0x10019250  next string is not a special/heading pass. */
/* BrClear_10019250: prototype in br_funcs.h */
/* 0x10019270  centre the next string (0 left, 1 right, 2 centre). */
/* BrSet_10019270: prototype in br_funcs.h */
/* 0x1000F620  wipe two 128-byte scratch tables. */
/* BrClearTables_1000F620: prototype in br_funcs.h */
/* 0x10073B90  rewind a two-word read cursor. */
/* BrPairReset_10073B90: prototype in br_funcs.h */
/* 0x1000F460  rebuild the 64-node clip-vertex free list. */
/* BrNodeChainReset_1000F460: prototype in br_funcs.h */

/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

#ifdef __cplusplus
}  /* BR_CLINK_END */
#endif
#endif
