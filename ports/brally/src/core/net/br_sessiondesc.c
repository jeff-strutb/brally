/* br_sessiondesc.c -- net.
 *
 * Publishing the current race settings into the DirectPlay session
 * description, so a machine browsing the lobby sees what it would be
 * joining.
 *
 * Filed out of the address batches: these functions were
 * matched first and grouped by what they are afterwards.
 * Every function carries its original address.
 */

#include "slice2_25.h"   /* br_globals: its objects */
#include <stddef.h>
#include <stdint.h>

#include "slice6_74.h"

/* slice6_72.h -- 0x1003CDA0.  The port arm's only callee; carried from
 * slice6_74.c's cross-slice declaration block. */
extern void BrExt_1003CDA0(void);

/* 0x1003CDA0.  slice5_62.h already recorded this collision in writing --
 * "BrExt_1003CDA0 in slice2_26.h and BrSub1003CDA0 in slice2_25.h, same
 * `void (void)` shape, two names" -- and slice6_72.c owns the body. 8 call
 * sites. */
/* WHAT IT DOES: a second name for a routine that lives in another module,
 * reached under two different spellings by different parts of the game. It
 * forwards and nothing more; what the routine itself does is described where
 * its body is. */
/* @implements 0x1003CDA0 d3d BrSub1003CDA0 */
/* NOT an adapter in the image. Glide 0x10036430 and D3D 0x1003CDA0 are both
 * 212 bytes of the real body -- the two spellings the tree found are two
 * COPIES the linker did not fold, not a forwarder and an owner. slice6_72.c
 * carries the same body as BrExt_1003CDA0, but through a Br72Env struct of
 * function pointers, which is the port's own indirection and cannot match.
 * This arm calls DirectPlay and KERNEL32 the way the original does.
 *
 * What it does: publish the current race settings into the DirectPlay session
 * description -- four user dwords, then SetSessionDesc -- and free the
 * GlobalAlloc'd descriptor on every path. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */      /* the IDirectPlay4, NULL before a session */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* the four settings the session carries */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* The session description GlobalAlloc'd by 0x10036740; only the four user
 * dwords (+0x40..+0x4C on i386) are touched here. 64-bit core: the SDK
 * record at the native layout, where they follow two pointers. */
#include "dplay.h"
typedef DPSESSIONDESC2 BrDpSessionDesc;

typedef struct BrDpObj BrDpObj;
typedef struct BrDpVtbl {
    void *apfn00[31];                                        /* +0x00..+0x7B */
    int32_t (__stdcall *SetSessionDesc)(BrDpObj *,
                                        BrDpSessionDesc *,
                                        uint32_t);           /* +0x7C */
} BrDpVtbl;
struct BrDpObj { const BrDpVtbl *pVtbl; };

/* BrDpGetSessionDesc: prototype in br_funcs.h */
/* BrDpRefreshSettings: prototype in br_funcs.h */

/* 64-bit core: GlobalHandle is declared by the platform headers */
/* 64-bit core: GlobalUnlock is declared by the platform headers */
/* 64-bit core: GlobalFree is declared by the platform headers */

int32_t BrSub1003CDA0(void)
{
    BrDpSessionDesc *pDesc = NULL;
    BrDpObj *pDP;
    int32_t hr;

    pDP = (BrDpObj *)(*(void * *)&g_brP277B40);
    if (pDP == NULL) {
        return (int32_t)0x88770082;      /* 0x10036444 */
    }

    hr = FUN_10036740(pDP, &pDesc);                /* 0x10036453 */
    if (hr >= 0) {
        pDesc->dwUser1 = g_Br0B380C;                   /* 0x1003646A */
        pDesc->dwUser2 = (*(int32_t *)&g_226e80);                   /* 0x10036477 */
        pDesc->dwUser3 = (*(int32_t *)&DAT_10ac5d70);                   /* 0x10036484 */
        pDesc->dwUser4 = (*(int32_t *)&DAT_100abdf8);                   /* 0x10036490 */

        BrSub10044540();                           /* 0x10036493 */

        /* The original RE-READS the object here rather than reusing it. */
        pDP = (BrDpObj *)(*(void * *)&g_brP277B40);                   /* 0x10036498 */
        hr = pDP->pVtbl->SetSessionDesc(pDP, pDesc, 0u); /* 0x100364A7 */
    }

    if (hr < 0) {
        /* 0x100364B0 -- the failure path null-checks the descriptor. */
        if (pDesc != NULL) {
            GlobalUnlock(GlobalHandle(pDesc));
            GlobalFree(GlobalHandle(pDesc));
        }
        return hr;
    }
    /* 0x100364DC -- the success path does NOT null-check it. hr >= 0 means
     * 0x10036740 produced one. */
    GlobalUnlock(GlobalHandle(pDesc));
    GlobalFree(GlobalHandle(pDesc));
    return 0;
}
