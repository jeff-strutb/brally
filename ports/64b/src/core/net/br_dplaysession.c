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
#include "br_coretypes.h"   /* br_globals: its objects */
#include "slice2_25.h"   /* br_globals: its objects */
#include <string.h>

#include "slice6_73.h"
/* g_br73 is the port's gathering of separate originals.  The matching build
 * names the ones used here as the globals they are (config/globals_glide.csv),
 * so each relocation resolves to its own variable. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x10AC5BD8 */
#define BR73_NAA2880 (*(int32_t *)&g_5BD8)
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x10AC5D2C */
#define BR73_APJOINBLOB (*(void *const * *)&g_brPAA29D4)

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

/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* WHAT IT DOES: returns a pointer to the selected DirectPlay provider GUID. */
/* @implements 0x1003CFC0 d3d BrSub1003CFC0 */
int32_t BrSub1003CFC0(uint8_t **ppGuid)
{
    int32_t n;

    n = (*(int32_t *)&DAT_10ac5bd4);
    *ppGuid = (uint8_t *)g_aBrNetSession[n].guid;
    return 0;
}

#include <windows.h>

/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* WHAT IT DOES: walk a table of GlobalAlloc pointers and free each one. */
/* @implements 0x10036670 glide BrGlobalFreeAll */

int BrGlobalFreeAll(void)

{
  LPCVOID pMem;
  HGLOBAL pvVar1;
  int i;

  for (i = 0; i < 16; i++) {
    pMem = g_aBrNetSession[i].pPayload;
    if (pMem != (LPCVOID)0x0) {
      pvVar1 = GlobalHandle(pMem);
      GlobalUnlock(pvVar1);
      pvVar1 = GlobalHandle(pMem);
      GlobalFree(pvVar1);
      g_aBrNetSession[i].pPayload = 0;
    }
  }
  return;
}

/* ==========================================================================
 * 0x10035AC0 -- store one enumerated session into its provider slot
 * ========================================================================== */

/* The four 16-byte provider GUIDs the game recognises. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* The 16-slot session table at 0x10AC3080, stride 0xE0: +0x00 a 200-byte
 * player block, +0xC8 the session GUID, +0xD8 the payload size, +0xDC the
 * GlobalLock'd payload pointer (the same table BrGlobalFreeAll walks). */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
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
                                void *pEnum, int arg5, int arg6)
{
  int idx;
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

  /* pEnum: a DirectPlay name record, two DWORDs then the pointer to
   * its 200-byte block (offset 8 on both 32- and 64-bit layouts) */
  memcpy(g_aBrNetSession[idx].player, *(void **)((char *)pEnum + 8), 0xc8);
  *(BrSessionGuid *)g_aBrNetSession[idx].guid = *(BrSessionGuid *)pGuid;
  pBuf = GlobalLock(GlobalAlloc(0x42, cbPayload));
  g_aBrNetSession[idx].pPayload = pBuf;
  if (pBuf != (void *)0x0) {
    memcpy(pBuf, pPayload, cbPayload);
    g_aBrNetSession[idx].cbPayload = cbPayload;
  }
  return 1;
}

/* ------------------------------------------------------------------ */
/* 0x10035C50 -- host a new session                                    */
/* ------------------------------------------------------------------ */

/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */           /* -> desc.dwUser1                  */
/* 64-bit core: declared once, in br_globals.h or its struct's header */           /* -> desc.dwUser2                  */
/* 64-bit core: declared once, in br_globals.h or its struct's header */           /* the host record; -> desc.dwUser3 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */           /* -> desc.dwUser4                  */
/* 64-bit core: declared once, in br_globals.h or its struct's header */         /* the player name                  */

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

/* WHAT IT DOES: hosts a brand-new game on the network. Builds the session
 * description -- our application id, room for eight players, the caller's
 * record as the name and the shared settings in the four user slots --
 * and asks DirectPlay to open it as the host. Then fills in the host
 * record and creates our own player in the session; if the player cannot
 * be made the record is put back the way it was and the error returned.
 * Networking switched off, or no DirectPlay object, refuses politely. */
/* @t4-pass 0x10035C50 1 2026-09-12 probes 10 bytes 383 insns 111 regions 4 rows 0 census no  (hand: memset sizeof/16, truthy fold, null spelling, hex/dec immediates x5, cast 1 -- all inert) */
/* @t4-pass 0x10035C50 2 2026-09-12 probes 10 bytes 383 insns 111 regions 4 rows 0 census yes  (hand: *rec read/write, ptr+0 name, register vt, uncast err, hex 0, dec vtable disp, ptr casts x3 -- all inert; census push 16 jcc 4 call 2 rep 1 ret 3 IDENTICAL both streams) */
/* @t3 0x10035C50 2026-09-12 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 383/383 insns 111/111 rows 0+0 regions 4 oracle EQUIVALENT
 * @t3-effort passes 2 zero-movement 1 2
 * Residue is register-role rotation only: the vptr/pHost pair lands in
 * ebp/edi where ours crosses them, and the desc-fill's load/store pairing
 * rotates with it -- multiset 0+0, size and instruction count exact, the
 * equivalence oracle says EQUIVALENT. What the source hunt proved: the
 * record pointer is a THIRD ARGUMENT (re-read from its arg slot after
 * Open -- any desc read-back aliases the frame and costs 6 insns), id and
 * the name memset share one zero web ORDERED rec, memset, id, and the
 * guid pairs fill 0,1 then 2,3 with the vt hoist between. Earlier draft
 * orders, the desc-slot read-back, memcpy pun, explicit name zeros and
 * full natural field order are all recorded dead above this line's
 * history. Do not reopen before the end-grind. */
/* @implements 0x10035C50 glide BrNetSessionHost */
int BrNetSessionHost(void *pIface, char *pHost, int *pRec)
{
    BrDpDesc2 desc;
    int  saved[3];
    int  name[6];
    int  id;
    int *rec;
    int  vt;
    int  hr;

    if (g_guardB == 0) {
        if (pIface == 0) {
            return (int)0x88770082;
        }
        memset(&desc, 0, 0x50);
        desc.guidA[0] = DAT_10077500;
        desc.guidA[1] = DAT_10077504;
        vt       = *(int *)pIface;        /* ONE vptr hoist, both calls */
        desc.guidA[2] = DAT_10077508;
        desc.guidA[3] = DAT_1007750c;
        desc.flags = (*(int *)(pHost + 0xc8) != 0 ? 0x100 : 0) + 0x40;
        desc.user1 = (*(int *)&g_Br0B380C);
        desc.user2 = g_226e80;
        desc.size = 0x50;
        desc.maxPlayers = 8;
        desc.pszName = (int)pHost;
        desc.user3 = (int)DAT_10ac5d70;
        desc.user4 = DAT_100abdf8;
        hr = (*(BrDpOpenFn *)(vt + 0x9c))(pIface, &desc, 0x82, 0, 0);
        if (hr < 0) {
            return hr;
        }
        rec = pRec;
        memset(name, 0, 0x10);
        id  = 0;
        saved[0] = rec[0];
        saved[1] = rec[3];
        saved[2] = rec[4];
        rec[0] = (int)pIface;
        rec[3] = 1;
        rec[4] = *(int *)(pHost + 0xc8);
        name[0] = 0x10;
        name[2] = (int)g_aBrCfgPlayerName;
        hr = (*(BrDpCreatePlayerFn *)(vt + 0x18))
                 (pIface, &id, name, rec[1], 0, 0, 0x100);
        if (hr < 0) {
            rec[0] = saved[0];
            rec[3] = saved[1];
            rec[4] = saved[2];
            return hr;
        }
        rec[2] = id;
    }
    return 0;
}

