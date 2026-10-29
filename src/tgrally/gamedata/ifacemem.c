/* ifacemem.c -- the interface memory pool: a bump allocator for front-end graphics and buffers
 */
#include "tgr/common.h"

/* -- declarations -- */
extern unsigned int D_80369B70;
unsigned int BrIfaceMemAlloc(int size);
int sprintf(char *buf, const char *fmt, ...);
void BrFatal(char *msg);
typedef struct BrIfaceImage {
  unsigned int data;            /* 0x00  RAM copy, from the interface pool */
  int rom;                      /* 0x04  compressed image in ROM */
  int x8;
  unsigned char depth;          /* 0x0C  bits per pixel */
  char pad0d[3];
  unsigned int w;               /* 0x10 */
  unsigned int h;               /* 0x14 */
} BrIfaceImage;
void BrAllocPaintShopGfxMem(BrIfaceImage *img);
int BrRomReadSize(int rom);
unsigned int BrRomUnpack(unsigned int dst, int rom, unsigned int *size);
void osSyncPrintf();
extern int D_80272500;
extern BrIfaceImage D_8028D0B0;
extern BrIfaceImage D_8028D0E0;
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
  BrAllocPaintShopGfxMem(&D_8028D0B0);
  BrAllocPaintShopGfxMem(&D_8028D0E0);
#if !TGR_REGION_JPN
  osSyncPrintf("Allocating %d bytes for data_buf...\n",0x3a00);
#endif
  D_80272500 = BrIfaceMemAlloc(0x3a00);
}
