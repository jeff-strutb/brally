/* br_texdetail.c -- drawing: the eight texture slots' detail levels.
 *
 * RESPONSIBILITY: drawing/ -- turn geometry and images into pixels.
 *
 * Filed out of slice3_44.c, an address batch and not a module.  0x1006E130
 * bumps one of eight slots up a level and tells the texture system; the
 * eight-word block above it is cleared and snapshotted by its two
 * neighbours.  That the two are the SAME eight is inferred from the count
 * and the addresses, not proved, and the file says so rather than renaming
 * anything on the strength of it.
 */
#include "slice3_44.h"

/* 0x11829850 */
unsigned int g_BrX1829850[8];

/* 0x10074E00 */
/* WHAT IT DOES: clears an eight-word block of state back to zero. What that
 * block holds is not established. */
/* @implements 0x10074E00 d3d BrSub10074E00 */
void BrSub10074E00(void)
{
    int i;

    for (i = 0; i < 8; ++i)
        g_BrX1829850[i] = 0u;
}

/* 0x10074E20 */
/* WHAT IT DOES: copies that same eight-word block out to the caller -- a
 * snapshot of it, whatever it holds. */
/* @implements 0x10074E20 d3d BrSub10074E20 */
void BrSub10074E20(unsigned int *pDst)
{
    int i;

    for (i = 0; i < 8; ++i)
        pDst[i] = g_BrX1829850[i];
}

#ifdef BR_MATCHING_BUILD

typedef int (*funcptr)();
extern funcptr DAT_118ed1d8;
int BrTex3dRecSet278();

/* WHAT IT DOES: bump one of eight texture slots up a detail level, to a
 * maximum of three, and tell the texture system about the new level. Ignores
 * an out-of-range slot. */
/* @implements 0x1006E130 glide FUN_1006e130 */
/* auto-filed from ghidra --refine; transforms: ge0 scaletemp */

void FUN_1006e130(int param_1,int param_2,int param_3)

{
  int iVar2;
  
  if (((param_1 >= 0)) && (param_1 < 8)) {
    iVar2 = *(int *)(param_3 + param_1 * 4);
    if (iVar2 < 3) {
      iVar2 = iVar2 + 1;
      *(int *)(param_3 + (param_1 * 4)) = iVar2;
      BrTex3dRecSet278(*(int *)(param_2 + (param_1 * 4)),iVar2);
      (*DAT_118ed1d8)(*(int *)(param_2 + (param_1 * 4)));
    }
  }
  return;
}

#endif /* BR_MATCHING_BUILD */
