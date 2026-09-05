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

#endif /* BR_MATCHING_BUILD */
