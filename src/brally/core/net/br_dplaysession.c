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

/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include <string.h>

#include "slice6_73.h"
/* g_br73 is the port's gathering of separate originals.  The matching build
 * names the ones used here as the globals they are (config/brally/globals_glide.csv),
 * so each relocation resolves to its own variable. */
extern int32_t g_brUinAA2880;   /* 0x10AC5BD8 */
#define BR73_NAA2880 g_brUinAA2880
extern void *const *g_brUiapJoinBlob;   /* 0x10AC5D2C */
#define BR73_APJOINBLOB g_brUiapJoinBlob

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

    if (BR73_APJOINBLOB == NULL) {
        return 0;
    }
    /* Orig `mov eax,[eax+ecx*8+0x1de48]`: the pointer at 0x10AA29D4 is a
     * base, not a pointer-to-pointer table.  Each slot is 8 bytes. */
    pSrc = *(void *const *)((const char *)BR73_APJOINBLOB
                            + 0x1DE48 + (size_t)BR73_NAA2880 * 8);
    if (pSrc == NULL) {
        return 0;
    }
    /* four dword copies in the original */
    memcpy(pBlob, pSrc, 16);
    return 0;
}

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

/* ==========================================================================
 * 0x10035AC0 -- store one enumerated session into its provider slot
 * ========================================================================== */

/* The four 16-byte provider GUIDs the game recognises. */
extern unsigned char DAT_10078898[];
extern unsigned char DAT_10078878[];
extern unsigned char DAT_10078868[];
extern unsigned char DAT_10078888[];
/* The 16-slot session table at 0x10AC3080, stride 0xE0: +0x00 a 200-byte
 * player block, +0xC8 the session GUID, +0xD8 the payload size, +0xDC the
 * GlobalLock'd payload pointer (the same table BrGlobalFreeAll walks). */
extern unsigned char DAT_10ac3080[];
extern unsigned char DAT_10ac3148[];
extern unsigned char DAT_10ac3158[];
/* DAT_10ac315c is already declared as `int` above (BrGlobalFreeAll). */

/* The session GUID copy is a 16-byte structure assignment, which is what makes
 * the compiler form one destination pointer (lea) and store through it, rather
 * than fold the table address into each of the four dword stores. */
typedef struct { int a, b, c, d; } BrSessionGuid;

/* WHAT IT DOES: files an enumerated network session into the slot for the
 * provider whose 16-byte GUID it matches (0..3, or bail if none match). It
 * saves the 200-byte player block, the session's own GUID, then allocates a
 * movable buffer and copies the variable-length session payload into it,
 * remembering the buffer and its size. Always reports success. */
/* @implements 0x10035AC0 glide BrNetSessionStore */
int __stdcall BrNetSessionStore(char *pGuid, void *pPayload, unsigned int cbPayload,
                                int pEnum, int arg5, int arg6)
{
  int idx;
  int base;
  void *pBuf;

  if (memcmp(pGuid, DAT_10078898, 0x10) == 0)
    idx = 2;
  else if (memcmp(pGuid, DAT_10078878, 0x10) == 0)
    idx = 1;
  else if (memcmp(pGuid, DAT_10078868, 0x10) == 0)
    idx = 0;
  else if (memcmp(pGuid, DAT_10078888, 0x10) == 0)
    idx = 3;
  else
    return 1;

  base = idx * 0xe0;
  memcpy(DAT_10ac3080 + base, *(void **)(pEnum + 8), 0xc8);
  *(BrSessionGuid *)(DAT_10ac3148 + base) = *(BrSessionGuid *)pGuid;
  pBuf = GlobalLock(GlobalAlloc(0x42, cbPayload));
  *(void **)((unsigned char *)&DAT_10ac315c + base) = pBuf;
  if (pBuf != (void *)0x0) {
    memcpy(pBuf, pPayload, cbPayload);
    *(unsigned int *)(DAT_10ac3158 + base) = cbPayload;
  }
  return 1;
}

/* ------------------------------------------------------------------ */
/* 0x10035C50 -- host a new session                                    */
/* ------------------------------------------------------------------ */

extern int   DAT_10ac4090;
extern int   DAT_10077500, DAT_10077504, DAT_10077508, DAT_1007750c;
extern int   DAT_100b3014;           /* -> desc.dwUser1                  */
extern int   DAT_10226e80;           /* -> desc.dwUser2                  */
extern int  *DAT_10ac5d70;           /* the host record; -> desc.dwUser3 */
extern int   DAT_100abdf8;           /* -> desc.dwUser4                  */
extern char  DAT_10b71648[];         /* the player name                  */

typedef struct BrDpDesc2 {
    int size, flags;
    int guidI[4];
    int guidA[4];
    int maxPlayers, curPlayers;
    int pszName, pszPassword;
    int reserved1, reserved2;
    int user1, user2, user3, user4;
} BrDpDesc2;

typedef int (__stdcall *BrDpOpenFn)(void *, void *, int, int, int);
typedef int (__stdcall *BrDpCreatePlayerFn)(void *, int *, void *, int,
                                            int, int, int);

/* 0x10035C50 BrNetSessionHost is byte-exact in the C++ lane,
 * src/brally/core/net/BrNetSessionHost_10035C50.cpp (C1XX against the C++
 * DirectPlay interface).  The declarations above stay for the port. */

