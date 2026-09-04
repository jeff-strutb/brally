/* br_texqueue.c -- drawing: the texture re-download queue.
 *
 * RESPONSIBILITY: drawing/ -- turn geometry and images into pixels.
 *
 * Filed out of slice6_78.c, an address batch and not a module.  A 256-entry
 * ring of (id, address) pairs, written and read under one mutex: a texture
 * that has to go back to the card is pushed here and re-downloaded through
 * the hook at 0x118ED1D0 when the pop runs.
 *
 * slice6_78.c's preamble is carried over verbatim.  An include set that
 * looks redundant has already been shown elsewhere in this module to move
 * VC5's register allocation (see br_rdpmode.c).
 */
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include <stdarg.h>
#include "br_path.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "slice6_78.h"


/* The ring's three globals are defined in slice6_78.c beside the mutex
 * creator that owns them, and slice6_78.h does not declare them, so they
 * are declared here rather than moved. */
extern int32_t g_br18A9878;
extern int32_t g_br18AA098;
extern void   *g_br18AA0A0;

#ifdef BR_MATCHING_BUILD
extern char DAT_118ec998;
extern char DAT_118ec99c;
#ifndef BR_FUNCPTR_DEFINED
#define BR_FUNCPTR_DEFINED
typedef int (*funcptr)();
#endif
extern funcptr DAT_118ed1d0;
__declspec(dllimport) int __stdcall WaitForSingleObject(void *hHandle, unsigned int dwMilliseconds);
__declspec(dllimport) int __stdcall ReleaseMutex(void *hMutex);
extern int DAT_1021c788;

/* WHAT IT DOES: under the queue mutex, append an (id, addr) pair to the 256-entry
 * texture re-download ring at 0x118EC998, wrapping the write index at 0x100. */
/* @implements 0x1006E1D0 glide BrTexQueuePush */

void BrTexQueuePush(int param_1,int param_2)

{
  WaitForSingleObject(g_br18AA0A0,0xffffffff);
  *(int *)(&DAT_118ec998 + g_br18A9878 * 8) = param_1;
  *(int *)(&DAT_118ec99c + g_br18A9878 * 8) = param_2;
  g_br18A9878 = g_br18A9878 + 1;
  if (g_br18A9878 >= 0x100) {
    g_br18A9878 = 0;
  }
  ReleaseMutex(g_br18AA0A0);
  return;
}

/* WHAT IT DOES: under the queue mutex, if the ring is non-empty pop one (id, addr) pair
 * and re-download that texture through hook slot [0x118ED1D0], wrapping the read index. */
/* @implements 0x1006E220 glide BrTexQueuePop */

void BrTexQueuePop(void)

{
  WaitForSingleObject(g_br18AA0A0,0xffffffff);
  if (g_br18AA098 != g_br18A9878) {
    (*DAT_118ed1d0)(*(int *)(&DAT_118ec998 + g_br18AA098 * 8),
                    *(int *)(&DAT_118ec99c + g_br18AA098 * 8));
    g_br18AA098 = g_br18AA098 + 1;
    if (g_br18AA098 >= 0x100) {
      g_br18AA098 = 0;
    }
  }
  ReleaseMutex(g_br18AA0A0);
  return;
}

#endif /* BR_MATCHING_BUILD */
