/* br_dplayjoin.c -- net.
 *
 * Getting into a session: hosting one, joining somebody else's under the
 * player's Windows user name, and the COM instantiation both start from.
 *
 * Filed out of the address batches: these functions were
 * matched first and grouped by what they are afterwards.
 * Every function carries its original address.
 */

#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include <stdio.h>
#include <string.h>

#include "slice4_50.h"

/* ==========================================================================
 * 7. DirectPlay host / join / send
 * ========================================================================== */

/* 0x1003C150 */
/* WHAT IT DOES: sets this machine up as the host of a network game, so other
 * players can find and join it. If hosting fails it composes an explanatory
 * message and then throws it away without showing it to anyone -- the player
 * sees nothing. */
/* port-only body; Glide match is src/core/generated/0x100357E0.c */
void BrSub1003C150(void)
{
    unsigned char aDesc[BR50_DPDESC_SIZE];
    char          szMsg[BR50_DPMSG_SIZE];
    int32_t       hr;

    if (g_brP277B40 == NULL) {
        return;
    }

    memset(aDesc, 0, sizeof aDesc);     /* rep stosd, ecx = 0x33 */
    BrSub1003D130(aDesc);

    hr = BrSub1003C5C0(g_brP277B40, aDesc, g_brPA9D008);
    if (hr < 0) {
        /* Formatted into a stack buffer and dropped on the floor -- see the
         * header. Kept because the call to 0x1007C830 is observable. */
        BrSprintf(szMsg, "Could not host session because of error 0x%08X",
                  (unsigned int)hr);
        return;
    }

    g_br22AF18 = 2;
    BrSub10071550();
    BrSub10005B10(1);
}

/* 0x1003C260 */
/* WHAT IT DOES: joins a network game somebody else is hosting, under the
 * player's Windows user name. If that name is already taken it asks whether
 * to try again and does so with the same name -- which will fail the same way
 * unless something else changed it. As with hosting, the failure message it
 * builds is discarded rather than shown. */
/* @implements 0x1003C260 d3d BrSub1003C260 */
#ifdef BR_MATCHING_BUILD
/* Direct globals/callees; the 29D4 deref is unguarded as in the original;
 * the retry hook is a direct call; one shared return-1 tail. */
extern int   DAT_10273328;
extern int   DAT_10ac5d30;
extern char *DAT_10ac5d2c;
extern int   DAT_10ac4090;
extern int   DAT_10ac4098;
extern int   DAT_10226a48;
extern char  DAT_100aa5b0[];        /* "Could not join session ..." */
extern int   BrSub1003D030(void *pJoin);
extern int   BrSub1003C740(int hDp, void *pJoin, char *pszName, int a4);
extern int   BrSub100385E0(char *pszName);      /* glide 0x100385E0 */
extern void  BrSub100355F0(void);
extern void  BrSub100356B0(void);
extern void  BrSub10005B10(int v);
extern void  BrSub1003CE80(void);
__declspec(dllimport) int __stdcall GetUserNameA(char *, unsigned long *);

int BrSub1003C260(void)
{
    unsigned long cbName;
    unsigned char aJoin[0x10];
    char          szName[0x320];
    char          szMsg[0x400];
    int           hr;

    if (DAT_10273328 == 0)
        return 0;

    if (DAT_10ac5d30 == 0)
        return 1;
    if (*(unsigned short *)(DAT_10ac5d2c + 0x1E164) <= 0u)
        return 1;

    if (DAT_10ac4090 == 0) {
        hr = BrSub1003D030(aJoin);
        if (hr >= 0) {
            memset(szName, 0, sizeof(szName));
            cbName = 0xC8;
            GetUserNameA(szName, &cbName);

            hr = BrSub1003C740(DAT_10273328, aJoin, szName, DAT_10ac4098);
            if (hr == (int)0x88770820) {
                if (BrSub100385E0(szName) == 0)
                    return 0;
                hr = BrSub1003C740(DAT_10273328, aJoin, szName,
                                   DAT_10ac4098);
            }
        }
        if (hr < 0) {
            BrSub100355F0();
            BrSub100356B0();
            sprintf(szMsg, DAT_100aa5b0, hr);
            return 0;
        }
    }

    DAT_10226a48 = 1;
    BrSub10005B10(1);
    BrSub1003CE80();
    return 1;
}
#else
/* WHAT IT DOES: joins a network game somebody else is hosting, under the
 * player's Windows user name. If that name is already taken it asks whether
 * to try again and does so with the same name -- which will fail the same way
 * unless something else changed it. As with hosting, the failure message it
 * builds is discarded rather than shown. */
/* @implements 0x1003C260 d3d BrSub1003C260 */
int BrSub1003C260(void)
{
    unsigned char aJoin[BR50_DPJOIN_SIZE];
    char          szName[BR50_DPNAME_SIZE];
    char          szMsg[BR50_DPMSG_SIZE];
    uint32_t      cbName;
    int32_t       hr;

    if (g_brP277B40 == NULL) {
        return 0;
    }

    /* GOTCHA: 29D8 is the one that is null-tested; 29D4 is then dereferenced
     * unguarded. DEVIATION: guarded, folded into the same early-out. */
    if (g_brPAA29D8 == NULL || g_brPAA29D4 == NULL) {
        return 1;
    }
    /* `cmp word ptr [eax+0x1E164], 0 / jbe` -- unsigned, so this is == 0. */
    if (g_brPAA29D4->f1E164 == 0) {
        return 1;
    }

    if (g_brA9D000 == 0) {
        hr = BrSub1003D030(aJoin);
        if (hr >= 0) {
            memset(szName, 0, sizeof szName);   /* rep stosd, ecx = 0xC8 */
            cbName = BR50_DPNAME_CB;
            (void)BrPlatGetUserName(szName, &cbName);

            hr = BrSub1003C740(g_brP277B40, aJoin, szName, g_brPA9D008);

            /* DPERR_USERCANCEL: run 0x10042AF0 on the name and, if it says
             * yes, retry the join with the same arguments. If it says no the
             * original returns immediately -- WITHOUT the 0x1003BF60 /
             * 0x1003C020 teardown the ordinary failure path runs. */
            if (hr == (int32_t)0x88770820u) {
                if (g_brPfn42AF0_1 == NULL || g_brPfn42AF0_1(szName) == 0) {
                    return 0;
                }
                hr = BrSub1003C740(g_brP277B40, aJoin, szName, g_brPA9D008);
            }
        }

        if (hr < 0) {
            BrSub1003BF60();
            BrSub1003C020();
            BrSprintf(szMsg,
                      "Could not join session because of error 0x%08X",
                      (unsigned int)hr);
            return 0;
        }
    }

    g_br22AF18 = 1;
    BrSub10005B10(1);
    BrSub1003CE80();
    return 1;
}
#endif

/* ── Ghidra-matched functions ─────────────────────────── */
#ifdef BR_MATCHING_BUILD
#include <windows.h>
extern int DAT_10078828;
extern int DAT_10078858;

/* WHAT IT DOES: create a COM object via CoCreateInstance and return its interface pointer. */
/* @implements 0x10035BB0 glide BrComCreateInstance */

int BrComCreateInstance(int *param_1)

{
  LPVOID local_4;
  
  local_4 = (LPVOID)0x0;
  CoCreateInstance((IID *)&DAT_10078858,(LPUNKNOWN)0x0,1,(IID *)&DAT_10078828,&local_4);
  *param_1 = local_4;
  return;
}

/* 0x10035BE0 -- the teardown twin of BrComCreateInstance above.  0x10AC4098
 * points at the session record whose first dword is the IDirectPlay object
 * (0x10273328, g_brP277B40) and whose +8 is our DPID.  Every access re-reads
 * the pointer global: the original reloads it after each COM call. */
extern int *g_brSlot4098;   /* 0x10AC4098 -> the record at 0x10273328;
                             * the earlier arm names the same slot
                             * DAT_10ac4098 as a plain int */
extern int  DAT_10ac4094;   /* 0x10AC4094 live-session counter */
void BrSub1003D070(void);   /* 0x10036700 */
typedef int (__stdcall *BrDpCall2)(void *pThis, int a);
typedef int (__stdcall *BrDpCall1)(void *pThis);

/* WHAT IT DOES: leaves the network game -- clears the lobby list, then, if a
 * DirectPlay object exists, destroys our player (when we have one), closes
 * the session, releases the object and forgets it.  Finally it clears the
 * session pointer and counts one fewer live session.  Always returns 0. */
/* @implements 0x10035BE0 glide BrDpShutdown */
int BrDpShutdown(void)
{
  void *pObj;

  BrSub1003D070();
  pObj = (void *)g_brSlot4098[0];
  if (pObj != 0) {
    if (g_brSlot4098[2] != 0) {
      (*(BrDpCall2 *)(*(char **)pObj + 0x24))(pObj, g_brSlot4098[2]);  /* DestroyPlayer */
      g_brSlot4098[2] = 0;
    }
    pObj = (void *)g_brSlot4098[0];
    (*(BrDpCall1 *)(*(char **)pObj + 0x10))(pObj);                     /* Close */
    pObj = (void *)g_brSlot4098[0];
    (*(BrDpCall1 *)(*(char **)pObj + 0x08))(pObj);                     /* Release */
    g_brSlot4098[0] = 0;
  }
  g_brP277B40 = 0;
  DAT_10ac4094 = DAT_10ac4094 - 1;
  return 0;
}

/* 0x10035DD0 (D3D 0x1003C740, shared body) -- join a session.  The record
 * layouts are DirectPlay 3's: DPSESSIONDESC2 is 0x50 bytes, DPNAME 0x10,
 * DPCREDENTIALS 0x14; the vtable slots are IDirectPlay3's (+0x18
 * CreatePlayer, +0x9C SecureOpen).  The vtable pointer is read ONCE into a
 * local and both calls go through it, which is what the original's `mov
 * ebp,[ebx]` before SecureOpen and `call [ebp+0x18]` after say.
 *
 * PARKED T2 2026-09-09 at 613/615 B, register-blind 0+1: ONE missing
 * `xor r,r`.  The original materialises the credentials pointer's NULL as
 * its own `xor edi,edi` (0x10035E27) while the prologue zero (`xor edx,edx`,
 * a SCRATCH register) serves the description pointer, the object guard and
 * cred.dwFlags and dies before the first lstrlenA.  Ours folds the two zeros
 * into one callee-saved register, which rotates esi/edi/ebp for the rest of
 * the function.  Two levers that DID land here and are worth keeping: the
 * full-session test is `>= 8` with the error as the `if` arm, and the
 * CreatePlayer failure is the `if` arm with the success block as the `else`
 * (it ends in `return 0`, so VC5 places it last).
 * Dead probes (fn.py, all inert or worse): pDesc as a declaration
 * initialiser (inert); pCred as a declaration initialiser (+16 B); pCred =
 * NULL before the guard (+16 B); pCred = NULL after cred.dwFlags (inert);
 * pCred = NULL before the memset (inert); `pCred = 0` (inert); pCred
 * declared first (inert); pDesc = NULL after the guard with pCred before
 * it (REGNORM 9+4).  Next idea, untested: the NULL might be a value VC5
 * cannot fold, e.g. a pointer returned by an earlier expression. */
#include <windows.h>

typedef struct BrDpSessDesc {           /* DPSESSIONDESC2 */
    DWORD dwSize;                       /* +0x00 */
    DWORD dwFlags;                      /* +0x04 */
    DWORD guidInstance[4];              /* +0x08 */
    DWORD guidApplication[4];           /* +0x18 */
    DWORD dwMaxPlayers;                 /* +0x28 */
    DWORD dwCurrentPlayers;             /* +0x2C */
    char *lpszSessionName;              /* +0x30 */
    char *lpszPassword;                 /* +0x34 */
    DWORD dwReserved1;                  /* +0x38 */
    DWORD dwReserved2;                  /* +0x3C */
    DWORD dwUser1;                      /* +0x40 */
    DWORD dwUser2;                      /* +0x44 */
    DWORD dwUser3;                      /* +0x48 */
    DWORD dwUser4;                      /* +0x4C */
} BrDpSessDesc;

typedef struct BrDpName {               /* DPNAME */
    DWORD dwSize;
    DWORD dwFlags;
    char *lpszShortName;
    char *lpszLongName;
} BrDpName;

typedef struct BrDpCredentials {        /* DPCREDENTIALS */
    DWORD dwSize;
    DWORD dwFlags;
    char *lpszUsername;
    char *lpszPassword;
    char *lpszDomain;
} BrDpCredentials;

/* The session record at 0x10273328 (see BrDpShutdown above): the object,
 * the receive event, our player id, and two words the join swaps out and
 * restores on failure. */
typedef struct BrDpSess {
    void    *pDp;                       /* +0x00 */
    void    *hEvent;                    /* +0x04 */
    DWORD    idPlayer;                  /* +0x08 */
    int      f0C;                       /* +0x0C */
    int      f10;                       /* +0x10 */
} BrDpSess;

/* The login record: three 200-byte strings after a 200-byte header. */
typedef struct BrDpLogin {
    char aHead[200];
    char szUser[200];                   /* +200 */
    char szPass[200];                   /* +400 */
    char szDomain[200];                 /* +600 */
} BrDpLogin;

typedef int (__stdcall *BrDpSecureOpen)(void *pThis, BrDpSessDesc *pDesc,
                                        DWORD dwFlags, void *pSecurity,
                                        BrDpCredentials *pCred);
typedef int (__stdcall *BrDpCreatePlayer)(void *pThis, DWORD *pId,
                                          BrDpName *pName, void *hEvent,
                                          void *pData, DWORD cbData,
                                          DWORD dwFlags);

extern int  FUN_10036740(void *pDp, void **ppDesc);   /* GetSessionDesc, sized */
extern char DAT_10b71648[];             /* the player's name              */
extern int  DAT_10226a4c;               /* spectator-style join flag      */
extern DWORD DAT_100abde8, DAT_10226e80, DAT_10ac5d70, DAT_100abdf8;
extern DWORD DAT_100b3014, DAT_100bcbe8, DAT_10ac5d58;
extern DWORD DAT_10ac40a8[20];          /* the session name, 80 bytes     */

/* WHAT IT DOES: join the network game whose instance id is given.  Builds a
 * session description carrying that id, attaches the player's Windows user
 * name, password and domain as credentials when any are set, opens the
 * session, fetches its live description and refuses a full one (eight
 * players), then creates our player under the configured name -- swapping
 * the session record's object and flags in first and restoring them if that
 * fails.  On success the host's four user words and the session name are
 * copied into the game's globals.  The fetched description is always freed;
 * returns 0 or the DirectPlay error. */
/* @t4-pass 0x10035DD0 1 2026-09-09 probes 25 bytes 613 insns 186 regions 4 rows 1 census yes  (tools/crank.py) */
/* @t4-pass 0x10035DD0 1 2026-09-09 probes 10 bytes 613 insns 186 regions 4 rows 1 census no  (hand, fn.py variants: literal/order/amp/cast spellings, all inert) */
/* @t4-pass 0x10035DD0 2 2026-09-09 probes 10 bytes 613 insns 186 regions 4 rows 1 census yes  (hand, fn.py variants: decl orders, memset/vtable forms, all inert; corpus MISS at +0x50 len 10) */
/* @t3 0x10035DD0 2026-09-09 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 613/615 insns 186/187 rows 1+0 regions 4 oracle UNCLASSIFIED
 * @t3-effort passes 2 zero-movement 1 2
 * residue is one rematerialised zero (the xor singleton; the original
 * reuses a live zero register, -1 insn) plus colouring; identical
 * multiset otherwise, 4 masked regions.  Dead probes in the two ledger
 * lines.  Do not reopen before the end-grind (CLAUDE.md rule 12). */
/* @implements 0x10035DD0 glide BrDpSessionJoin */
int BrDpSessionJoin(void *pDp, DWORD *pGuidInstance, BrDpLogin *pLogin,
                    BrDpSess *pSess)
{
    BrDpSessDesc    *pDesc = NULL;
    BrDpCredentials  cred;
    BrDpName         name;
    DWORD            id;
    BrDpSess         save;
    BrDpSessDesc     desc;
    BrDpCredentials *pCred;
    char            *pVt;
    int              hr;
    DWORD            dwFlags;
    HGLOBAL          h;

    if (pDp == NULL)
        return (int)0x88770082;         /* DPERR_INVALIDOBJECT */

    memset(&desc, 0, sizeof(desc));
    desc.dwSize = sizeof(desc);
    desc.guidInstance[0] = pGuidInstance[0];
    pCred = NULL;
    desc.guidInstance[1] = pGuidInstance[1];
    desc.guidInstance[2] = pGuidInstance[2];
    desc.guidInstance[3] = pGuidInstance[3];

    memset(&cred, 0, sizeof(cred));
    cred.dwSize  = sizeof(cred);
    cred.dwFlags = 0;
    if (lstrlenA(pLogin->szUser) != 0) {
        pCred = &cred;
        cred.lpszUsername = pLogin->szUser;
    }
    if (lstrlenA(pLogin->szPass) != 0) {
        pCred = &cred;
        cred.lpszPassword = pLogin->szPass;
    }
    if (lstrlenA(pLogin->szDomain) != 0) {
        pCred = &cred;
        cred.lpszDomain = pLogin->szDomain;
    }

    pVt = *(char **)pDp;
    hr = (*(BrDpSecureOpen *)(pVt + 0x9C))(pDp, &desc, 0x81, NULL, pCred);
    if (hr >= 0) {
        memset(&name, 0, sizeof(name));
        name.dwSize        = sizeof(name);
        name.lpszShortName = DAT_10b71648;
        name.lpszLongName  = NULL;
        hr = FUN_10036740(pDp, (void **)&pDesc);
        if (hr >= 0) {
            if (pDesc->dwCurrentPlayers >= 8) {
                hr = (int)0x88770028;   /* DPERR_SESSIONFULL-class refusal */
            } else {
                dwFlags = 0;
                if (DAT_10226a4c != 0)
                    dwFlags = 0x200;
                save.pDp = pSess->pDp;
                save.f0C = pSess->f0C;
                pSess->pDp = pDp;
                pSess->f0C = 0;
                save.f10 = pSess->f10;
                pSess->f10 = (pDesc->dwFlags >> 8) & 1;
                hr = (*(BrDpCreatePlayer *)(pVt + 0x18))(pDp, &id, &name,
                                                         pSess->hEvent,
                                                         NULL, 0, dwFlags);
                if (hr < 0) {
                    pSess->pDp = save.pDp;
                    pSess->f0C = save.f0C;
                    pSess->f10 = save.f10;
                } else {
                    pSess->idPlayer = id;
                    DAT_100abde8 = pDesc->dwUser1;
                    DAT_10226e80 = pDesc->dwUser2;
                    DAT_10ac5d70 = pDesc->dwUser3;
                    DAT_100abdf8 = pDesc->dwUser4;
                    DAT_100b3014 = DAT_100abde8;
                    DAT_100bcbe8 = DAT_100abdf8;
                    DAT_10ac5d58 = DAT_10226e80;
                    memcpy(DAT_10ac40a8, pDesc->lpszSessionName,
                           sizeof(DAT_10ac40a8));
                    h = GlobalHandle(pDesc);
                    GlobalUnlock(h);
                    h = GlobalHandle(pDesc);
                    GlobalFree(h);
                    return 0;
                }
            }
        }
    }
    if (pDesc != NULL) {
        h = GlobalHandle(pDesc);
        GlobalUnlock(h);
        h = GlobalHandle(pDesc);
        GlobalFree(h);
    }
    return hr;
}

/* ── the lobby launch path ─────────────────────────────────────────────── */

/* CLSID_DirectPlayLobby (0x10078918), IID_IDirectPlayLobby3A (0x10078908),
 * and the IID ConnectEx asks for (0x10078848). */
extern int DAT_10078918;
extern int DAT_10078908;
extern int DAT_10078848;

/* The IDirectPlayLobby3A slots this function calls, all stdcall COM. */
typedef int (__stdcall *BrLobGetConn)(void *pThis, DWORD dwAppId,
                                      void *pData, DWORD *pcb);      /* +0x20 */
typedef int (__stdcall *BrLobSetConn)(void *pThis, DWORD dwFlags,
                                      DWORD dwAppId, void *pConn);   /* +0x30 */
typedef int (__stdcall *BrLobConnectEx)(void *pThis, DWORD dwFlags,
                                        void *riid, void **ppv,
                                        void *pUnk);                 /* +0x3C */
typedef int (__stdcall *BrDpOpenX)(void *pThis, DWORD *pOut, void *pDesc,
                                   int a, DWORD b, DWORD c,
                                   DWORD dwFlags);                   /* +0x18 */
typedef int (__stdcall *BrComRel)(void *pThis);                      /* +0x08 */

/* WHAT IT DOES: connects a lobby-launched game.  Creates the DirectPlay
 * lobby object, sizes and fetches the connection settings the lobby staged
 * (a too-small probe first, then a GlobalAlloc'd fetch), stamps the session
 * description with the game's flag word (0x44) and eight players, writes
 * the settings back and connects.  The obtained DirectPlay interface is
 * opened on the session description with 0x100 or'd in when the lobby said
 * host; on success the record gets the interface, the open result and the
 * host bit, and the lobby's player short name and session name are copied
 * into the game's globals.  Whatever was created but not handed over is
 * released and freed on every path; returns the failing HRESULT.
 *
 * RESIDUE (RAW 0+1, 455 vs 461 B, one instruction): the original computes
 * the host bit as `mov esi,[pMem+4]; AND ESI,0xFF; shr esi,1; and esi,1`
 * and no tested spelling seats the 6-byte and-0xff -- VC5 folds the low-byte
 * mask into the bit extract every time.  Everything else in the function is
 * byte-exact, including the neg/sbb/and-0x100 ternary at the Open call and
 * the whole cleanup ladder.
 * DEAD: `(uVar4 & 0xff) >> 1 & 1` (folds); `& 0xff` as its own named
 * assignment, as compound `&=` statements, and as `% 256` (all fold);
 * `(unsigned char)` cast of the dword local (folds to shr/and-1 via eax);
 * a direct `*(unsigned char *)` read and an int-from-byte widening
 * assignment (both emit a BYTE load, not dword+and -- the byte-slot
 * dword+and widening needs a stack slot and a register death, absent here);
 * an 8-bit bitfield member (extraction folds too); the same expression
 * through the C++ front end (also folds, and C++ costs elsewhere).  The
 * ternary must be spelled `flag != 0 ? 0x100 : 0` (the literal
 * `-(uint)(flag != 0) & 0x100` compiles to setne).
 * @t4-pass 2026-09-09 probes=9 result=-6B/raw0+1 census no */
/* @implements 0x10032320 glide BrDpLobbyConnect */
int BrDpLobbyConnect(int *param_1)
{
    int          *local_10;
    int          *local_c;
    unsigned int  local_8;
    DWORD         uStack_4;
    LPVOID        pMem;
    HGLOBAL       pvVar3;
    int           iVar2;
    unsigned int  uVar4;
    int           flag;
    char         *pcVar6;

    local_10 = (int *)0x0;
    local_c = (int *)0x0;
    pMem = (LPVOID)0x0;
    iVar2 = CoCreateInstance((IID *)&DAT_10078918, (LPUNKNOWN)0x0, 1,
                             (IID *)&DAT_10078908, (LPVOID *)&local_c);
    if (iVar2 >= 0) {
        iVar2 = (*(BrLobGetConn *)(*local_c + 0x20))(local_c, 0, 0, &local_8);
        if (iVar2 == (int)0x8877001e) {
            pvVar3 = GlobalAlloc(0x42, local_8);
            pMem = GlobalLock(pvVar3);
            if (pMem == (LPVOID)0x0) {
                iVar2 = (int)0x8007000e;
            } else {
                iVar2 = (*(BrLobGetConn *)(*local_c + 0x20))(local_c, 0, pMem,
                                                             &local_8);
                if (iVar2 >= 0) {
                    uVar4 = *(unsigned int *)((int)pMem + 4);
                    flag = (uVar4 & 0xff) >> 1 & 1;
                    *(int *)(*(int *)((int)pMem + 8) + 4) = 0x44;
                    *(int *)(*(int *)((int)pMem + 8) + 0x28) = 8;
                    iVar2 = (*(BrLobSetConn *)(*local_c + 0x30))(local_c, 0, 0,
                                                                 pMem);
                    if (iVar2 >= 0) {
                        iVar2 = (*(BrLobConnectEx *)(*local_c + 0x3c))(
                            local_c, 0, &DAT_10078848, (void **)&local_10, 0);
                        if (iVar2 >= 0) {
                            iVar2 = (*(BrDpOpenX *)(*local_10 + 0x18))(
                                local_10, &uStack_4, *(void **)((int)pMem + 0xc),
                                param_1[1], 0, 0,
                                flag != 0 ? 0x100 : 0);
                            if (iVar2 >= 0) {
                                *param_1 = (int)local_10;
                                param_1[2] = uStack_4;
                                if ((*(unsigned char *)((int)pMem + 4) & 2) != 0) {
                                    param_1[3] = 1;
                                } else {
                                    param_1[3] = 0;
                                }
                                strcpy(DAT_10b71648,
                                       *(char **)(*(int *)((int)pMem + 0xc) + 8));
                                pcVar6 = *(char **)(*(int *)((int)pMem + 8) + 0x30);
                                if (pcVar6 != (char *)0x0) {
                                    strcpy((char *)DAT_10ac40a8, pcVar6);
                                }
                                local_10 = (int *)0x0;
                            }
                        }
                    }
                }
            }
        }
    }
    if (local_10 != (int *)0x0) {
        (*(BrComRel *)(*local_10 + 8))(local_10);
    }
    if (local_c != (int *)0x0) {
        (*(BrComRel *)(*local_c + 8))(local_c);
    }
    if (pMem != (LPVOID)0x0) {
        pvVar3 = GlobalHandle(pMem);
        GlobalUnlock(pvVar3);
        pvVar3 = GlobalHandle(pMem);
        GlobalFree(pvVar3);
    }
    return iVar2;
}

#endif /* BR_MATCHING_BUILD */
