/* slice6_77.c -- see slice6_77.h for the identification of both functions,
 * the evidence for the storage they use, and the gotchas. */
#include "slice6_77.h"

#include "br_slots.h"    /* BrSlot */
#include "slice2_25.h"   /* g_aBrAA2538, g_brAA288C, g_brB4E1D0/D4/E0,
                          * g_aBrB4DF30 and its stride/count                 */
#include "slice3_45.h"   /* BrFfbInit (0x100791D0), g_brFfb; pulls in
                          * slice1_10.h for BrFfbShutdown (0x10079550)       */

/* ── Ghidra-matched functions ─────────────────────────── */
