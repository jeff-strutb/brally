/* br_uiopt.c -- menus: four of the options/lobby screen actions -- match the
 * session to the highlighted one, the host's "go", the plain back-out, and
 * the network-screen setup.
 *
 * Filed out of slice2_25.c, whose preamble it keeps verbatim below (the
 * storage those bodies read is declared by slice2_25.h and still DEFINED in
 * slice2_25.c -- nothing here defines it a second time).  The original banner
 * follows.
 *
 * slice2_25.c -- another module's packet, 0x10042880-0x100446D0 (46 functions).
 *
 * See slice2_25.h for what the module is and how the three repeated shapes
 * work. Everything here is a transcription; the DEVIATION list is at the
 * bottom of the file and every deviation is also marked at its line.
 *
 * A WARNING FOR INTEGRATION, about br_slots.h
 * -----------------------------------------------
 * br_slots.h declares
 *
 *     typedef struct BrSlotTable { BrSlot aSlots[8]; int count; } BrSlotTable;
 *
 * with the comment that `count` is 0x10AA288C. That struct is NOT the memory
 * layout: the slot array ends at 0x10AA2598 and 0x10AA288C is 0x2F4 bytes
 * further on. This packet reads and writes both, independently, and they
 * cannot be one object. The array is exposed here as g_aBrAA2538 and
 * 0x10AA288C as the separate flag g_brAA288C. (In this packet 0x10AA288C is
 * used as a flag -- set to 1 at 0x10043B10, tested at 0x10043925 -- not as a
 * count, which is further evidence they are unrelated.)
 */
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include "slice2_25.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Carried verbatim from slice2_25.c, which keeps its own -- it is `static
 * __inline` there, so the definition could not travel, and without it the
 * calls below are implicit (C4013) and leave an undefined external: a link
 * failure match_sweep.py cannot see, because it only compiles the matching
 * configuration. Found by tools/portcheck.py. It holds no state of its own,
 * so two copies cannot drift, the same reason BrFtol is duplicated in
 * slice1_02.c and slice2_12.c.
 *
 * The original INLINES this in every announcing cycler: the 3-arg send,
 * then an intrinsic strcpy (repne scasb + rep movsd/movsb). */
static __inline void BrOptFlushMessage(void)
{
    BrSub1003D210(g_brP680584, g_brPA9D008, 1);
    strcpy(g_aBrA9DD28, g_aBr39B720);        /* DEVIATION: rep movsb */
}

#ifdef BR_MATCHING_BUILD
/* KERNEL32 IAT used verbatim by BrOpt3A00 (0x1003CF50) / BrOpt3810. */
__declspec(dllimport) void *__stdcall GlobalHandle(void *);
__declspec(dllimport) int   __stdcall GlobalUnlock(void *);
__declspec(dllimport) void *__stdcall GlobalFree(void *);
#endif

/* 0x100437B0 */
/* WHAT IT DOES: makes the network session the game is talking to match the
 * one the player has highlighted, and does nothing if it already does. */
/* @implements 0x100437B0 d3d BrOpt37B0 */
int BrOpt37B0(void)
{
    if (g_brPA9D008->f08 != g_br0AB3E0)
        BrSub1003DA40(g_brPA9D008, g_br0AB3E0);
    return 1;
}

/* 0x10043A00. GOTCHA: the DirectPlay pointer is NOT null-checked before
 * 0x1003D0B0 is called with it -- unlike every other use in this packet. */
/* WHAT IT DOES: the host pressing "go": it refuses if there is nobody else
 * connected, refuses if any joined player has not confirmed they are ready,
 * and otherwise closes the session to further joiners and starts the race.
 * Note it asks DirectPlay for the session details without first checking
 * there is a session -- unlike everywhere else in this module. */
/* @implements 0x10043A00 d3d BrOpt3A00 */
int BrOpt3A00(void)
{
#ifdef BR_MATCHING_BUILD
    /* orig: KERNEL32 IAT (FF 15 / call esi), strcpy as repne scasb+rep movs,
     * SetSessionDesc stdcall `push 0; push desc; push this; call [vtbl+0x7c]`,
     * too-few path inlines FlushMessage (not a helper call). */
    BrDPSessionDesc *pDesc;
    int              fAllReady;
    BrSlot          *pSlot;
    typedef long (__stdcall *FnSetDesc)(BrDPlay *, BrDPSessionDesc *, uint32_t);

    pDesc = NULL;
    BrSub1003D0B0(g_brP277B40, &pDesc);
    if (pDesc == NULL)
        return 1;

    if (pDesc->dwCurrentPlayers <= 1) {
        /* orig does Unlock/Free HERE and returns -- not a jump to the
         * shared tail (that tail is only the players>1 arms). */
        strcpy(g_aBrA9DD28, BrStrGet(BR_OPT_STR_TOOFEW));
        BrSub1003D210(g_brP680584, g_brPA9D008, 1);
        strcpy(g_aBrA9DD28, g_aBr39B720);
        GlobalUnlock(GlobalHandle(pDesc));
        GlobalFree(GlobalHandle(pDesc));
        return 1;
    } else if (g_brAA2884 != 0) {
        fAllReady = 1;
        /* orig `cmp eax, &g_aBrAA2538[8]; jl` -- signed pointer compare. */
        for (pSlot = g_aBrAA2538;
             (int)pSlot < (int)(g_aBrAA2538 + BR_SLOT_COUNT);
             pSlot++) {
            if (pSlot->a != 0)
                continue;
            if (pSlot->id != BR_SLOT_EMPTY) {
                fAllReady = 0;
                break;
            }
        }
        if (fAllReady) {
            BrSub1003D9F0(g_brPA9D008);
            g_brAA288C = 1;
            pDesc->dwFlags |= 0x20;
            ((FnSetDesc)g_brP277B40->pVtbl->pfnSetSessionDesc)(
                g_brP277B40, pDesc, 0);
        } else {
            strcpy(g_aBrA9DD28, BrStrGet(BR_OPT_STR_NOTREADY));
            BrSub1003D210(g_brP680584, g_brPA9D008, 1);
            strcpy(g_aBrA9DD28, g_aBr39B720);
        }
    } else {
        BrSub1003D950(g_brPA9D008, BrSub10058700());
    }

    GlobalUnlock(GlobalHandle(pDesc));
    GlobalFree(GlobalHandle(pDesc));
    return 1;
#else
    BrDPSessionDesc *pDesc = NULL;
    int              fAllReady;
    int              i;

    BrSub1003D0B0(g_brP277B40, &pDesc);
    if (pDesc == NULL)
        return 1;

    if (pDesc->dwCurrentPlayers <= 1) {
        strcpy(g_aBrA9DD28, BrStrGet(BR_OPT_STR_TOOFEW));
        BrOptFlushMessage();
    } else if (g_brAA2884 != 0) {
        fAllReady = 1;
        for (i = 0; i < BR_SLOT_COUNT; ++i) {
            if (g_aBrAA2538[i].a != 0)
                continue;
            if (g_aBrAA2538[i].id != BR_SLOT_EMPTY) {
                fAllReady = 0;
                break;
            }
        }
        if (fAllReady) {
            BrSub1003D9F0(g_brPA9D008);
            g_brAA288C = 1;
            pDesc->dwFlags |= 0x20;
            g_brP277B40->pVtbl->pfnSetSessionDesc(g_brP277B40, pDesc, 0);
        } else {
            strcpy(g_aBrA9DD28, BrStrGet(BR_OPT_STR_NOTREADY));
            BrSub1003D210(g_brP680584, g_brPA9D008, 1);
            strcpy(g_aBrA9DD28, g_aBr39B720);
        }
    } else {
        BrSub1003D950(g_brPA9D008, BrSub10058700());
    }

    BrGlobalUnlock(BrGlobalHandle(pDesc));
    BrGlobalFree(BrGlobalHandle(pDesc));
    return 1;
#endif
}

/* 0x10043FA0. Returns 0. */
/* WHAT IT DOES: leaves the current screen for the one behind it, in the
 * simplest form -- close and go back. */
/* @implements 0x10043FA0 d3d BrOpt3FA0 */
#ifdef BR_MATCHING_BUILD
/* Orig is thiscall slot+0x18 with one stack arg (`push 1; mov ecx,this;
 * mov edx,[ecx]; call [edx+0x18]`) then a1/a3 pointer copy.  Header
 * pfnSlot6 is cdecl and g_brPAA2904 is (*g_ppBrPhaseCur).  Pass pVtbl as
 * the unused edx slot so the call is `call [edx+0x18]`; a named temp
 * keeps the subsequent copy as `a1`/`a3` instead of ecx + hoisted xor. */
int BrOpt3FA0(BrGameObj *pGame)
{
    extern BrOptObj *DAT_10ac5c5c;
    extern BrOptObj *DAT_10ac5c60;
    typedef void (__fastcall *Slot6)(BrGameSub *pThis, void *edx_slot, int arg);
    BrOptObj *p;

    ((Slot6)pGame->pSub->pVtbl->pfnSlot6)(pGame->pSub, pGame->pSub->pVtbl, 1);
    p = DAT_10ac5c60;
    DAT_10ac5c5c = p;
    return 0;
}
#else
int BrOpt3FA0(BrGameObj *pGame)
{
    BrGameSub *pSub = pGame->pSub;

    pSub->pVtbl->pfnSlot6(pSub, 1);
    g_brPAA2904 = g_brPAA2908;
    return 0;
}
#endif

/* 0x100441A0. DEVIATION: declared void. The original falls off two of its
 * three exits without loading eax, so its "return value" is whatever the
 * last call left behind; no caller in the DLL examines it.
 *
 * GOTCHA: the session descriptor fetched here is never released -- there is
 * no GlobalHandle/GlobalFree pair on this path, unlike 0x10043810 and
 * 0x10043A00. That leak is in the original. */
/* WHAT IT DOES: sets up the network game screens: it re-opens the session
 * to joiners if it had been closed, builds the several screens the lobby
 * needs, and either starts hosting or announces the session, depending on
 * whether this machine is the host. The session details it fetches from
 * DirectPlay are never given back -- that leak is in the original. */
/* @implements 0x100441A0 d3d BrOpt41A0 */
void BrOpt41A0(void)
{
    BrDPSessionDesc *pDesc;
#ifdef _MSC_VER
    /* Header types the slot cdecl; IDirectPlay4::SetSessionDesc is stdcall
     * (`call [ecx+0x7C]` with no `add esp`). Local vtable view only. */
    typedef long (__stdcall *BrOptSetSessFn)(BrDPlay *, BrDPSessionDesc *, uint32_t);
    typedef struct { void *aSlots[31]; BrOptSetSessFn pfnSetSessionDesc; } BrOptDPlayVtblStd;
#endif

    g_brAA287C = 1;
    BrSub100586A0();

    if (g_brAA2884 != 0) {
        pDesc = NULL;
        if (g_brP277B40 != NULL)
            BrSub1003D0B0(g_brP277B40, &pDesc);
        if (pDesc != NULL) {
            pDesc->dwFlags &= ~0x20u;    /* clear DPSESSION_JOINDISABLED */
#ifdef _MSC_VER
            ((const BrOptDPlayVtblStd *)g_brP277B40->pVtbl)
                ->pfnSetSessionDesc(g_brP277B40, pDesc, 0);
#else
            g_brP277B40->pVtbl->pfnSetSessionDesc(g_brP277B40, pDesc, 0);
#endif
        }
    }

    BrSub10043BF0(NULL);
    BrOptOpen2940(NULL);
    BrOptOpen2948(NULL);

    if (g_brAA2884 != 0) {
        BrOptOpen294C(NULL);
        BrOptOpen2950B(NULL);
    } else {
        BrSub1003CE80();
        BrOptOpen2950A(NULL);
    }

    if (g_brAA2884 != 0) {
        if (g_brAA2888 == 0) {
            BrSub1003C150();
            g_brAA2888 = 1;
            return;
        }
    }
    /* Second test of 0x10AA2884: original keeps eax live and re-tests at 0x10044267. */
    if (g_brAA2884 != 0)
        BrSub1003CDA0();
}
