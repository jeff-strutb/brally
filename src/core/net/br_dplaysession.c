/* br_dplaysession.c -- net.
 *
 * Picking a session to join: the provider GUID the player selected, the
 * 16-byte blob that names the highlighted game, and the teardown that frees
 * every enumerated session record.
 *
 * Filed out of the address batches: these functions were
 * matched first and grouped by what they are afterwards.
 * Every function carries its original address.
 */

#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include <string.h>

#include "slice6_73.h"

/* ==========================================================================
 * 0x1003D030 -- the 16-byte join blob
 * ========================================================================== */

/* WHAT IT DOES: fetches the small identifying blob for the network game the
 * player has highlighted, which is what the join attempt hands to DirectPlay
 * to say which session it wants. It reports success even when there was
 * nothing to fetch, so the caller cannot tell the difference. */
/* @implements 0x1003D030 d3d BrSub1003D030 */
int32_t BrSub1003D030(void *pBlob)
{
    const void *pSrc;

    if (g_br73.apJoinBlob == NULL) {
        return 0;
    }
#ifdef BR_MATCHING_BUILD
    /* Orig `mov eax,[eax+ecx*8+0x1de48]`: the pointer at 0x10AA29D4 is a
     * base, not a pointer-to-pointer table.  Each slot is 8 bytes. */
    pSrc = *(void *const *)((const char *)g_br73.apJoinBlob
                            + 0x1DE48 + (size_t)g_br73.nAA2880 * 8);
#else
    pSrc = g_br73.apJoinBlob[g_br73.nAA2880];
#endif
    if (pSrc == NULL) {
        return 0;
    }
    /* four dword copies in the original */
    memcpy(pBlob, pSrc, 16);
    return 0;
}

#ifdef BR_MATCHING_BUILD
extern int32_t g_brAA287C;
extern uint8_t g_aBrA9C0B8[];

/* WHAT IT DOES: returns a pointer to the selected DirectPlay provider GUID. */
/* @implements 0x1003CFC0 d3d BrSub1003CFC0 */
int32_t BrSub1003CFC0(uint8_t **ppGuid)
{
    int32_t n;

    n = g_brAA287C;
    *ppGuid = g_aBrA9C0B8 + n * 224;
    return 0;
}
#endif

#ifdef BR_MATCHING_BUILD
#include <windows.h>
extern int DAT_10ac315c;

/* WHAT IT DOES: walk a table of GlobalAlloc pointers and free each one. */
/* @implements 0x10036670 glide BrGlobalFreeAll */

int BrGlobalFreeAll(void)

{
  LPCVOID pMem;
  HGLOBAL pvVar1;
  int *puVar2;
  
  puVar2 = &DAT_10ac315c;
  do {
    pMem = (LPCVOID)*puVar2;
    if (pMem != (LPCVOID)0x0) {
      pvVar1 = GlobalHandle(pMem);
      GlobalUnlock(pvVar1);
      pvVar1 = GlobalHandle(pMem);
      GlobalFree(pvVar1);
      *puVar2 = 0;
    }
    puVar2 = puVar2 + 0x38;
  } while ((int)puVar2 < 0x10ac3f5c);
  return;
}

#endif /* BR_MATCHING_BUILD */
