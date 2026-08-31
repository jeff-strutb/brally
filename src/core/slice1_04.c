/* slice1_04.c -- decompiled from BRD3D.dll, 0x1001DDB0-0x1002A8A0. */

#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include "slice1_04.h"

#include <stddef.h>
#include <string.h>

/* ==========================================================================
 * Texture size codecs
 * ========================================================================== */

/* 0x100251A0 */
/* WHAT IT DOES: picks the size code for a texture from its width and height:
 * it takes whichever is larger and works out which power of two it fits in,
 * counted downward from 256. It also reports whether the size was an exact
 * power of two or had to be rounded up. */
/* @implements 0x100251A0 d3d BrTexShiftFromSize */
int BrTexShiftFromSize(int *pShift, int a, int b)
{
    /* orig is two textually identical signed ladders (`cmp; jg`), one on `a`
     * when a > b and one on `b` otherwise -- not a shared helper. A factored
     * `BrTexShiftLadder` is two `call`s and 42 B against orig 430. */
    if (a > b) {
        if (a <=   1) { *pShift = 8; return 1; }
        if (a <=   2) { *pShift = 7; return 1; }
        if (a <=   4) { *pShift = 6; return 1; }
        if (a <=   8) { *pShift = 5; return 1; }
        if (a <=  16) { *pShift = 4; return 1; }
        if (a <=  32) { *pShift = 3; return 1; }
        if (a <=  64) { *pShift = 2; return 1; }
        if (a <= 128) { *pShift = 1; return 1; }
        if (a <= 256) { *pShift = 0; return 1; }
        *pShift = 0;
        return 0;
    }
    if (b <=   1) { *pShift = 8; return 1; }
    if (b <=   2) { *pShift = 7; return 1; }
    if (b <=   4) { *pShift = 6; return 1; }
    if (b <=   8) { *pShift = 5; return 1; }
    if (b <=  16) { *pShift = 4; return 1; }
    if (b <=  32) { *pShift = 3; return 1; }
    if (b <=  64) { *pShift = 2; return 1; }
    if (b <= 128) { *pShift = 1; return 1; }
    if (b <= 256) { *pShift = 0; return 1; }
    *pShift = 0;
    return 0;
}

/* 0x10028200 */
int BrTexAspectFromSize(int *pCode, int a, int b)
{
    int r;

    if (a > b) {
        r = (a * 8) / b;
        if (r == 0x40) { *pCode = 0; return 1; }
        if (r == 0x20) { *pCode = 1; return 1; }
        if (r == 0x10) { *pCode = 2; return 1; }
        /* No exact rung for r == 8 here: that would mean a == b, which this
         * branch has already excluded. */
        if (r > 0x40)  { *pCode = 0; return 0; }
        if (r > 0x20)  { *pCode = 1; return 0; }
        if (r > 0x10)  { *pCode = 2; return 0; }
        *pCode = 3;
        return 0;
    }

    r = (b * 8) / a;
    if (r == 0x40) { *pCode = 6; return 1; }
    if (r == 0x20) { *pCode = 5; return 1; }
    if (r == 0x10) { *pCode = 4; return 1; }
    if (r == 0x08) { *pCode = 3; return 1; }
    if (r > 0x40)  { *pCode = 6; return 0; }
    if (r > 0x20)  { *pCode = 5; return 0; }
    if (r > 0x10)  { *pCode = 4; return 0; }
    *pCode = 3;
    return 0;
}

/* 0x10028720 */
/* WHAT IT DOES: the reverse of the size coding: given a size code and an
 * aspect code, works back out the texture's width and height. An aspect code
 * it does not recognise leaves the height as the caller had it. */
/* @implements 0x10028720 d3d BrTexSizeFromShiftAspect */
void BrTexSizeFromShiftAspect(int *pA, int *pB, int shift, int aspect)
{
    /* First jump table: 9 entries at 0x10028820, indices 0..8, plus a
     * default that lands on the same code as index 8. */
    switch (shift) {
    case 0:  *pA = 0x100; break;
    case 1:  *pA = 0x80;  break;
    case 2:  *pA = 0x40;  break;
    case 3:  *pA = 0x20;  break;
    case 4:  *pA = 0x10;  break;
    case 5:  *pA = 8;     break;
    case 6:  *pA = 4;     break;
    case 7:  *pA = 2;     break;
    default: *pA = 1;     break;   /* case 8 and everything out of range */
    }

    /* Second jump table: 7 entries at 0x10028844. Out of range simply
     * returns, leaving *pB untouched. Divisions are `cdq`-corrected shifts,
     * i.e. truncation toward zero, matching C's / operator. */
    switch (aspect) {
    case 0:  *pB = *pA / 8; break;
    case 1:  *pB = *pA / 4; break;
    case 2:  *pB = *pA / 2; break;
    case 3:  *pB = *pA;     break;
    case 4:  *pB = *pA; *pA = *pA / 2; break;
    case 5:  *pB = *pA; *pA = *pA / 4; break;
    case 6:  *pB = *pA; *pA = *pA / 8; break;
    default: break;
    }
}

/* 0x10027B90 */
/* WHAT IT DOES: decides which of the backend's pixel formats a texture
 * should be created in, from the N64's own format and size codes plus one
 * extra mode flag. Most combinations fall through to the same general
 * format; only a couple of specific pairings get a format of their own. */
/* @implements 0x10027B90 d3d BrTexFormatCode */
int BrTexFormatCode(int a, int b, int c)
{
    if (a == 0) {
        if (b == 4) {
            /* `dec/neg/sbb eax,eax` yields 0 for c == 1 and -1 otherwise;
             * `and al,0xF7` then turns -1 into -9, and +11 gives 11 or 2. */
            return (c == 1) ? 11 : 2;
        }
        return 11;
    }
    if (a == 1) {
        if (b == 3) {
            /* same idiom, masked with 0xF8 and biased by 12 */
            return (c == 1) ? 12 : 4;
        }
        if (b == 4) {
            return 2;
        }
        return 11;
    }
    /* a == 2 reloads b in the original and then discards it -- a dead load,
     * not a missing case. Everything here returns 11. */
    return 11;
}

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
