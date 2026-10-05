/* zutil.c -- zlib 1.0.4's utilities (zutil.c), as the ROM has it: the
 * version string and the allocator, which the game replaced with a bump
 * allocator over a fixed range.
 *
 * zlib 1.0.4, Copyright (C) 1995-1996 Jean-loup Gailly.  For conditions of
 * distribution and use, see the notice in tgr/zinfl.h.  Altered from the
 * original: K&R headers written as prototypes; zcalloc and zcfree are the
 * game's.
 */
#include "tgr/zinfl.h"

/* -- declarations -- */
void osSyncPrintf(const char *fmt, ...);
/* -- end declarations -- */

/* WHAT IT DOES: The zlib version string, "1.0.4". */
/* @implements 0x80242810 tgr zlibVersion */
const char *zlibVersion(void)
{
  return ZLIB_VERSION;
}

/* WHAT IT DOES: Allocate items * size bytes from the heap range opaque
 * points at: the block starts 8-aligned and the range's next pointer moves
 * on by the size rounded up to 8; running past the range's end prints a
 * message and hangs. */
/* @implements 0x8024281C tgr zcalloc */
voidpf zcalloc(voidpf opaque, unsigned items, unsigned size)
{
  BrZHeap *h = (BrZHeap *)opaque;
  voidpf p;

  p = (voidpf)(((uLong)h->next + 7) & ~7);
  size *= items;
  size = (size + 7) & ~7;
  if (h->end < (h->next += size))
  {
    osSyncPrintf("Insufficient memory for zcalloc\n");
    while (1)
      ;
  }
  return p;
}

/* WHAT IT DOES: Nothing: the heap is dropped whole, never freed block by
 * block. */
/* @implements 0x80242880 tgr zcfree */
void zcfree(voidpf opaque, voidpf ptr)
{
}
