/* Hand-matched from disassembly — 0x100597F0
 * Inlined memset: fills `count` bytes at `dst` with byte `c`
 * (broadcast to a dword, rep stosd for count/4, rep stosb for count&3). */
#ifdef BR_MATCHING_BUILD

#include <string.h>
#pragma intrinsic(memset)

/* WHAT IT DOES: fill a block of memory with a repeated byte. A thin wrapper
 * whose argument ORDER differs from the C library's -- destination, count,
 * then value -- which is exactly the trap to watch for at a call site. */
/* @implements 0x100597F0 glide FUN_100597f0 */
void FUN_100597f0(void *dst, unsigned count, int c)
{
  memset(dst, c, count);
}

#endif /* BR_MATCHING_BUILD */
