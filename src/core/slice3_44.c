/* slice3_44.c -- decompiled from BRD3D.dll, packet 0x10071B80-0x10075F10.
 *
 * See slice3_44.h for the API, the layout notes and the gotchas, and the
 * pass report for the list of addresses that were deliberately skipped.
 *
 * Float constants used here, read out of orig/BRD3D.dll .rdata with
 * tools/pe.py rather than assumed:
 *
 *   0x1008FC48 = 0x3F800000 = 1.0f
 *   0x1008FC4C = 0x3F000000 = 0.5f
 *   0x1008FC54 = 0x3DAAAAAB = 1/12 (the correctly rounded float)
 *   in-line immediates: 0x3F800000 = 1.0f, 0x3F000000 = 0.5f,
 *                       0x3E322D0E = 0.174f
 */
#include "slice3_44.h"

/* The packed 3x3 helpers and the rigid-body integrator (0x10074AC0 BrMat3Mul
 * .. 0x1006D6B0 BrRbBuildMatrix) are filed together in
 * src/core/driving/br_rbinteg.c. */

/* ── Ghidra-matched functions ─────────────────────────── */
#ifdef BR_MATCHING_BUILD
extern int DAT_118ed1a0;
int FUN_1006c750();
int FUN_1006c800();
int FUN_1006c880();
int FUN_1006c8b0();



#endif /* BR_MATCHING_BUILD */
