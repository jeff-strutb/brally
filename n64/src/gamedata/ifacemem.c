/* ifacemem.c -- the interface memory pool: a bump allocator for front-end graphics and buffers
 */
#include "tgr/common.h"

/* -- declarations -- */
extern int D_80369B70;
/* -- end declarations -- */

/* WHAT IT DOES: Empty the interface memory pool: the next allocation starts
 * again at the bottom of its region. */
/* @implements 0x80242940 tgr BrIfaceMemReset */
void BrIfaceMemReset(void)
{
  D_80369B70 = 0x80096400;
}

/* WHAT IT DOES: Return the interface memory pool's current top, so a caller
 * can release everything allocated after this point later. */
/* @implements 0x80242954 tgr BrIfaceMemMark */
int BrIfaceMemMark(void)
{
  return D_80369B70;
}

/* WHAT IT DOES: Put the interface memory pool's top back to a mark taken
 * earlier, releasing everything allocated since. */
/* @implements 0x80242960 tgr BrIfaceMemRelease */
void BrIfaceMemRelease(int param_1)
{
  D_80369B70 = param_1;
}
