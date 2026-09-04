/* br_livery.c -- drawing: the car livery tables and their buffers.
 *
 * RESPONSIBILITY: drawing/ -- turn geometry and images into pixels.
 *
 * Filed out of the address batches, which are not modules.  One lookup into
 * the livery table (which leaves the row it read in a global on the way
 * past), the shutdown pair that releases the bitmaps and then the three
 * shared buffers, and 0x1005A420 -- the bitmap release itself, which came
 * from a different batch.
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


/* ‼ MAP DEFECT, fixed 2026-09-04.  config/functions_glide.csv listed
 * 0x1005A480 as one 91-byte function.  It is two: a 5-byte `jmp 1005A490`
 * plus eleven alignment nops (16 bytes, MSVC emits the padding inside the
 * first function), then the 75-byte loader at the 16-aligned address that
 * nothing CALLS -- only the jump reaches it, so the map builder never saw an
 * entry.  Same class as 0x10002EB0/F10 in slice1_01.c.  Split to
 * 0x1005A480/16 + 0x1005A490/75 and both bins re-extracted.  The wrapper is
 * byte-exact.  The loader is PARKED at 7 diffs, register-blind 0+0: the
 * original keeps the counter in esi and the slot pointer in edi, VC5 gives
 * us the reverse.  Dead (2026-09-01 on the merged row, re-run 2026-09-04
 * on the split one): declaration order, init order, `buf` first, `++i` in
 * the argument, `*p++`, `register` on either, a symbolic end pointer
 * (1+1), `while` and `for` over p (+9 B), `for` over i with `[i-1]` (5+5)
 * or `[i]`+`i+1` (1+1, 72 B).  Do not re-probe. */

__declspec(dllimport) int __cdecl sprintf(char *pDst, const char *pFmt, ...);
extern char s_Paint_damage_d_bmp_100b2e58[];
int BrBmpLoadRgba(char *);

/* WHAT IT DOES: load the three damage-decal bitmaps (Paint\damage1.bmp ..
 * damage3.bmp) into the three shared livery slots that FUN_1005a6b0 frees.
 * The dllimport sprintf is hoisted into ebx across the loop. */
/* @implements 0x1005A490 glide BrLiveryLoadDamageBmps */
void BrLiveryLoadDamageBmps(void)
{
  int i;
  int *p;
  char buf[0x400];

  i = 0;
  p = &DAT_10ac67b0;
  do {
    i = i + 1;
    sprintf(buf, s_Paint_damage_d_bmp_100b2e58, i);
    *p = BrBmpLoadRgba(buf);
    p = p + 1;
  } while ((int)p < 0x10ac67bc);
}

/* WHAT IT DOES: the public entry that loads the damage decals -- it does
 * nothing but hand off to the loader above.  A `jmp`, since VC5 /O2 turns a
 * trailing call into a tail jump; the 16-byte row is that jump plus padding. */
/* @implements 0x1005A480 glide BrLiveryLoadDamage */
void BrLiveryLoadDamage(void)
{
  BrLiveryLoadDamageBmps();
}


extern int DAT_100ad7d8;

/* WHAT IT DOES: free every loaded car-livery bitmap -- walks the whole
 * table, releasing each allocation and clearing the slot so it is not freed
 * twice. */
/* @implements 0x1005A420 glide FUN_1005a420 */
/* auto-filed from ghidra --refine; transforms: as-is */

void FUN_1005a420(void)

{
  int *p;
  int *row;
  int *q;
  int i;
  int j;
  void *mem;

  p = &DAT_100ad7d8;
  do {
    row = p;
    i = 0x1e;
    do {
      q = row;
      j = 4;
      do {
        mem = (void *)*q;
        if (mem != (void *)0x0) {
          free(mem);
          *q = 0;
        }
        q = q + 1;
        j = j + -1;
      } while (j != 0);
      row = row + 10;
      i = i + -1;
    } while (i != 0);
    p = row;
  } while ((int)p < 0x100b22d8);
  return;
}

#endif /* BR_MATCHING_BUILD */
