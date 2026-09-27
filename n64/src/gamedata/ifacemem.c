/* ifacemem.c -- the interface memory pool: a bump allocator for front-end graphics and buffers
 */
#include "tgr/common.h"

/* -- declarations -- */
extern unsigned int D_80369B70;
unsigned int BrIfaceMemAlloc(int size);
int sprintf(char *buf, const char *fmt, ...);
void BrFatal(char *msg);
void func_80242B10(int *param_1);
void osSyncPrintf();
extern int D_80272500;
extern int D_8028D0B0;
extern int D_8028D0E0;
extern int D_8028DB80;
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

/* WHAT IT DOES: Take size bytes from the interface memory pool, 8-byte
 * aligned; running past the pool's end (0x800D4000) is fatal, with the
 * sizes in the message. */
/* @implements 0x8024296C tgr BrIfaceMemAlloc */
unsigned int BrIfaceMemAlloc(int size)
{
  unsigned int p;
  char msg[120];

  p = (D_80369B70 + 7) & ~7;
  D_80369B70 = p + size;
  if (D_80369B70 > 0x800D4000) {
    sprintf(msg, "Insufficient interface memory (need %d, have %d)", size, 0x800D4000 - p);
    BrFatal(msg);
  }
  return p;
}

/* WHAT IT DOES: Set aside memory for the paint shop: its two car-texture
 * buffers and a 14,848-byte data buffer, all from the interface memory
 * pool. */
/* @implements 0x80214A3C tgr BrPaintShopMemInit */
void BrPaintShopMemInit(void)
{
  func_80242B10(&D_8028D0B0);
  func_80242B10(&D_8028D0E0);
  osSyncPrintf("Allocating %d bytes for data_buf...\n",0x3a00);
  D_80272500 = BrIfaceMemAlloc(0x3a00);
}

/* WHAT IT DOES: Set aside the 2 KB decal buffer from the interface memory
 * pool, then the paint shop's two car-texture buffers. */
/* @implements 0x80248F38 tgr BrDecalMemInit */
void BrDecalMemInit(void)
{
  osSyncPrintf("\nAllocating %d bytes for decal buffer...\n\n",0x800);
  D_8028DB80 = BrIfaceMemAlloc(0x800);
  func_80242B10(&D_8028D0B0);
  func_80242B10(&D_8028D0E0);
}
