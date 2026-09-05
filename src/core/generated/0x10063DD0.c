/* Auto-generated from Ghidra decompilation — 0x10063DD0 */
#ifdef BR_MATCHING_BUILD

/* The original binary is /MD: CRT calls resolve through the import table. */
#define _CRTIMP __declspec(dllimport)
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <mmsystem.h>

#ifndef true
#define true 1
#define false 0
#endif
#ifndef NAN
unsigned long _ghidra_nan_bits = 0x7FC00000;
#define NAN (*(float*)&_ghidra_nan_bits)
#endif

typedef int (*funcptr)();

/* Forward declarations for unknown functions/globals */
/* The collision grid: four cells of 150 BrCollPlane records (32 bytes each,
 * include/slice1_08.h) at 0x11773698, then the 200-node contact pool at
 * 0x117781B0 (include/br_collresp.h). Only the first seven dwords of a plane
 * are cleared; the triangle index / flags word at +0x1C is left alone. */
typedef struct BrCollPlaneZ {
  float nx, ny, nz;
  float d;
  void *pV0;
  void *pV1;
  void *pV2;
  unsigned int tri;
} BrCollPlaneZ;
typedef struct BrCollNodeZ {
  void *pPlane;
  void *pNext;
} BrCollNodeZ;
extern BrCollPlaneZ DAT_11773698[4][150];
extern BrCollNodeZ DAT_117781b0[200];
extern void *DAT_11778198;          /* g_pBrCollRespList                 */
extern void *DAT_11778844;          /* g_pBrCrCursor                     */
extern int DAT_11778840;
extern int DAT_11778800;
extern int DAT_11778804;
extern float DAT_11778828;
extern float DAT_1177882c;
extern float DAT_11778830;
extern float DAT_11778834;
extern int DAT_11778838;
extern int DAT_1177883c;

/* T2 RESIDUE (2026-09-05): 132/134 bytes, 33/34 instructions, and the ONLY
 * difference in the whole function is the head's constant materialisation.
 *   original:  xor ecx,ecx / xor edx,edx, then the four float stores use EDX
 *              and the four int stores ECX, with `mov eax,DAT_11773698+4`
 *              scheduled between store 6 and store 7.
 *   ours:      one `xor ecx,ecx`; all eight stores use ECX and the grid
 *              pointer init is hoisted to the top of the function.
 * Everything else -- store ORDER (verified against the .obj relocations:
 * 828,800,82c,804,830,838,834,83c then 198,844,840), the down-counted 150
 * inner loop, the `eax-4` biased plane pointer, both pointer-bound outer
 * loops, the 8-byte node loop -- is byte-for-byte identical.
 * tools/corpus.py: NO solved function anywhere in the tree contains a run of
 * three of these instructions, so there is no proven spelling to copy.
 * DEAD PROBES (all still 132 bytes, one zero register, under /O2 /Op):
 *   floats assigned 0.0f / 0.0 / (float)0.0; all-int declarations; the
 *   float/int roles swapped; void*+NULL; unsigned int; volatile on either
 *   group; a float local zeroed first and stored from; the four floats as
 *   float[4]; the ints as int[2] pairs; chained assignment in 4/2/1 groups
 *   (int = float = 0, which reproduces the emitted ORDER exactly and still
 *   collapses to one register); i/j declaration order swapped.
 *   Loop-form probes are all WORSE: explicit plane pointer 143 B, explicit
 *   node pointer 133 B.
 *   Flags: /G3 /G4 /G5 /GB and /Ox /Op are all identical to /O2 /Op; plain
 *   /O2 (no /Op) is 136 B and puts an IMMEDIATE in the first store, which
 *   is the tell that MSVC5 materialises a constant into a register only
 *   after its first use -- the original has BOTH registers live before the
 *   first store, i.e. it saw two distinct constants where we give it one.
 * WHAT IT DOES: wipe the collision-response state at race start -- zero the
 * four cells of the collision grid (150 plane records each), the 200-node
 * contact pool, the contact list head and its bump cursor, and the handful
 * of accumulators next to them. */
/* @implements 0x10063DD0 glide BrCollRespReset */
void BrCollRespReset(void)
{
  int i;
  int j;

  DAT_11778828 = 0;
  DAT_11778800 = 0;
  DAT_1177882c = 0;
  DAT_11778804 = 0;
  DAT_11778830 = 0;
  DAT_11778838 = 0;
  DAT_11778834 = 0;
  DAT_1177883c = 0;
  for (i = 0; i < 4; i++) {
    for (j = 0; j < 150; j++) {
      DAT_11773698[i][j].nx = 0;
      DAT_11773698[i][j].ny = 0;
      DAT_11773698[i][j].nz = 0;
      DAT_11773698[i][j].d = 0;
      DAT_11773698[i][j].pV0 = 0;
      DAT_11773698[i][j].pV1 = 0;
      DAT_11773698[i][j].pV2 = 0;
    }
  }
  for (i = 0; i < 200; i++) {
    DAT_117781b0[i].pPlane = 0;
    DAT_117781b0[i].pNext = 0;
  }
  DAT_11778198 = 0;
  DAT_11778844 = 0;
  DAT_11778840 = 0;
}


#endif /* BR_MATCHING_BUILD */
