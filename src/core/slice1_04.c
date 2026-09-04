/* slice1_04.c -- decompiled from BRD3D.dll, 0x1001DDB0-0x1002A8A0. */

#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include "slice1_04.h"

#include <stddef.h>
#include <string.h>

/* ==========================================================================
 * Record table search -- 0x10028630
 * ========================================================================== */

int BrTblFind(const BrTblRec *aRecs, unsigned int count, const BrTblRec *pRec)
{
    unsigned int i;
    int32_t probeF4C;

    if (count == 0) {
        return -1;
    }

    /* Hoisted out of the loop in the original too: the back edge targets the
     * comparison, not this load. */
    probeF4C = pRec->f4C;

    for (i = 0; i < count; ++i) {
        const BrTblRec *pEnt = &aRecs[i];

        if (pEnt->f4C != probeF4C) {
            continue;
        }
        if (pEnt->f50 != pRec->f50) {
            continue;
        }
        /* Either side reporting f268 != 1 ACCEPTS the entry outright and
         * skips the key comparison entirely. */
        if (pEnt->f268 != 1) {
            return (int)i;
        }
        if (pRec->f268 != 1) {
            return (int)i;
        }
        if (memcmp(pEnt->f294, pRec->f294, sizeof pEnt->f294) == 0) {
            return (int)i;
        }
    }
    return -1;
}

/* ==========================================================================
 * EAR loader -- 0x1002A8A0
 * ========================================================================== */

const char *const g_pszBrEarDllPds = "earpds.dll";
const char *const g_pszBrEarDllIas = "earias.dll";
const char *const g_pszBrEarWndMsg = "EAR Interactive Around-Sound";

const char *const g_apszBrEarProc[BR_EAR_PROC_COUNT] = {
    "_EAR_DLL_AAA_Validate@4",
    "_EAR_DLL_AssignHwnd@4",
    "_EAR_DLL_ChangeChannelControl@8",
    "_EAR_DLL_ClearChannel@8",
    "_EAR_DLL_EarInactive@0",
    "_EAR_DLL_GetEventStatus@8",
    "_EAR_DLL_GetLastError@0",
    "_EAR_DLL_GetVersion@0",
    "_EAR_DLL_InitializeEar@4",
    "_EAR_DLL_MixEvent@4",
    "_EAR_DLL_MoveEvent@4",
    "_EAR_DLL_RegisterBank@8",
    "_EAR_DLL_RegisterChannel@16",
    "_EAR_DLL_RegisterEnvironment@4",
    "_EAR_DLL_RegisterMatrix@4",
    "_EAR_DLL_RegisterPreset@8",
    "_EAR_DLL_ResetEar@0",
    "_EAR_DLL_SetAttenuationLevel@8",
    "_EAR_DLL_SetUserDistanceUnit@8",
    "_EAR_DLL_ShowLastError@0",
    "_EAR_DLL_ShutDownBank@4",
    "_EAR_DLL_ShutDownChannel@4",
    "_EAR_DLL_ShutDownEar@0",
    "_EAR_DLL_ShutDownEnvironment@4",
    "_EAR_DLL_ShutDownEvent@8",
    "_EAR_DLL_ShutDownMatrix@4",
    "_EAR_DLL_ShutDownPreset@4",
    "_EAR_DLL_StartEvent@4",
    "_EAR_DLL_StartTimer@0",
    "_EAR_DLL_ShutDownTimer@0",
    "_EAR_DLL_UpdateEar@0"
};

int BrEarLoad(BrEarState *pState, const BrEarPlatform *pPlat, int usePds)
{
    /* The original copies the chosen name into a 12-byte stack slot with an
     * inline strcpy and passes that. Both names are 10 characters, so it
     * fits exactly; 16 here for headroom.
     * DEVIATION: bounded copy instead of the inline rep movsd. */
    char szName[16];
    const char *pszWant;
    int i;

    if (pState->hModule != NULL && pState->apfn[BR_EAR_UPDATE_EAR] != NULL) {
        return 1;
    }

    if (usePds != 0) {
        pState->preferPds = 1;
        pszWant = g_pszBrEarDllPds;
    } else {
        pszWant = g_pszBrEarDllIas;
    }
    strncpy(szName, pszWant, sizeof szName - 1);
    szName[sizeof szName - 1] = '\0';

    pState->hModule = pPlat->pfnGetModuleHandle(szName);
    if (pState->hModule == NULL) {
        pState->usedLoadLibrary = 1;
        pState->hModule = pPlat->pfnLoadLibrary(szName);

        /* When usePds != 0 the original does NOT bail here; it falls straight
         * through to the GetProcAddress storm with a NULL handle. Preserved. */
        if (pState->hModule == NULL && usePds == 0) {
            pState->usedLoadLibrary = 0;   /* `mov [g_5754D4], ebx`, ebx == 0 */
            pState->fallbackTried   = 1;
            pState->hModule = pPlat->pfnGetModuleHandle(g_pszBrEarDllPds);
            if (pState->hModule == NULL) {
                pState->usedLoadLibrary = 1;
                pState->hModule = pPlat->pfnLoadLibrary(g_pszBrEarDllPds);
                if (pState->hModule == NULL) {
                    return 0;
                }
            }
        }
    }

    /* 0x1002A97A -- a second "already bound" gate, before any resolution. */
    if (pState->apfn[BR_EAR_UPDATE_EAR] != NULL) {
        return 1;
    }

    for (i = 0; i < BR_EAR_PROC_COUNT; ++i) {
        pState->apfn[i] = pPlat->pfnGetProcAddress(pState->hModule,
                                                   g_apszBrEarProc[i]);
    }

    /* The original ORs together 31 `sete` results and tests the accumulator
     * once; every slot is checked, none is exempt. */
    for (i = 0; i < BR_EAR_PROC_COUNT; ++i) {
        if (pState->apfn[i] == NULL) {
            return 0;
        }
    }

    pState->windowMessage = pPlat->pfnRegisterWindowMessage(g_pszBrEarWndMsg);
    return 1;
}
