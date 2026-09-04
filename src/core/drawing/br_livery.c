/* br_livery.c -- drawing: the car livery tables and their buffers.
 *
 * RESPONSIBILITY: drawing/ -- turn geometry and images into pixels.
 *
 * Filed out of the address batches, which are not modules.  One lookup into
 * the livery table (which leaves the row it read in a global on the way
 * past), and the shutdown pair that releases the bitmaps and then the three
 * shared buffers.
 *
 * slice3_40.c's preamble is carried over verbatim.  An include set that
 * looks redundant has already been shown elsewhere in this module to move
 * VC5's register allocation (see br_rdpmode.c).
 */
#include <string.h>

#ifdef BR_MATCHING_BUILD
/* Header prototype is cdecl; the original is thiscall.  Rename the
 * prototype so the thiscall definition is not a C2373 redefinition. */
#define BrCarInitTables BrCarInitTables_cdecl_hdr
#define BrCarClear29C8  BrCarClear29C8_cdecl_hdr
#define BrZeroRegions   BrZeroRegions_cdecl_hdr
#endif
#include "slice3_40.h"
#ifdef BR_MATCHING_BUILD
#undef BrCarInitTables
#undef BrCarClear29C8
#undef BrZeroRegions
void BrZeroRegions(void);
#endif

#include "br_match.h"    /* BR_THISCALL1 */

#ifdef BR_MATCHING_BUILD

extern int DAT_100ad7d8;
extern char DAT_100ad7e8;
extern int DAT_100b22d8;

/* WHAT IT DOES: look up one field of one car livery entry, and leave that
 * entry's identifier in a global on the way past -- so the caller gets a
 * value AND the table remembers which row it came from. */
/* @implements 0x10059FE0 glide FUN_10059fe0 */
/* auto-filed from ghidra --refine; transforms: as-is */

int FUN_10059fe0(int param_1,int param_2,int param_3)

{
  DAT_100b22d8 = *(int *)((char *)&DAT_100ad7e8 + (param_2 + param_1 * 0x1e) * 0x28);
  return ((int *)((char *)&DAT_100ad7d8 + param_1 * 1200 + param_2 * 40))[param_3];
}

extern int DAT_10ac67b0;
__declspec(dllimport) void __cdecl free(void *);
void FUN_1005a420(void);
void FUN_1005a6b0(void);

/* WHAT IT DOES: release everything the car-livery system holds: the bitmaps
 * first, then the three shared buffers. The single call site for shutting
 * that subsystem down. */
/* @implements 0x1005A6A0 glide FUN_1005a6a0 */
/* Tail-call wrapper: `call A; jmp B` -- VC5 /O2 turns the trailing call
 * into a jmp. The map had this merged with the 45-byte loop at
 * 0x1005A6B0 as one 61-byte function; split 2026-09-01. */

void FUN_1005a6a0(void)
{
  FUN_1005a420();
  FUN_1005a6b0();
}

/* WHAT IT DOES: free the three shared livery buffers and null the slots, so
 * a second call is harmless. */
/* @implements 0x1005A6B0 glide FUN_1005a6b0 */
/* Frees and zeroes the three pointer slots at DAT_10ac67b0..bc; the
 * dllimport free is hoisted into edi across the loop. */

void FUN_1005a6b0(void)
{
  int *puVar1;

  puVar1 = &DAT_10ac67b0;
  do {
    if (*puVar1 != 0) {
      free((void *)*puVar1);
      *puVar1 = 0;
    }
    puVar1 = puVar1 + 1;
  } while ((int)puVar1 < 0x10ac67bc);
  return;
}

#endif /* BR_MATCHING_BUILD */
