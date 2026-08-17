/* br_dxver.c -- see br_dxver.h. PLATFORM: the DirectX capability probe,
 * transcribed from BRGlide.dll 0x1001D8A0 (924 bytes; D3D 0x10030210, shared).
 *
 * ============================================================================
 * THE STACK FRAME, TRACED EXPLICITLY, BECAUSE TWO DISPLACEMENTS IN THIS
 * FUNCTION MEAN DIFFERENT THINGS AT DIFFERENT POINTS
 * ============================================================================
 *
 * Let E be esp at entry, so the return address is at [E] and the two cdecl
 * arguments are at [E+4] and [E+8].
 *
 *     1001D8A0  sub esp,0x114      esp = E-0x114
 *     1001D8A6  push ebx           esp = E-0x118
 *     1001D8A7  push ebp           esp = E-0x11C
 *     1001D8A8  push esi           esp = E-0x120
 *     1001D8A9  push edi           esp = E-0x124
 *     1001D8B3  push eax           esp = E-0x128   (arg to GetVersionExA)
 *     1001D8D3  call GetVersionExA esp = E-0x124   (stdcall, callee pops)
 *
 * so the arguments are [esp+0x128] and [esp+0x12C] whenever esp == E-0x124,
 * which is what 0x1001D8DD/0x1001D8E4 read. The locals:
 *
 *     E-0x114   pDD      IDirectDraw *
 *     E-0x110   pDDS     the primary IDirectDrawSurface *
 *     E-0x10C   pDD2     QI(IID_IDirectDraw2) result
 *     E-0x108   pDDS3    QI(IID_IDirectDrawSurface3) result
 *     E-0x104   pDDS4    QI(IID_IDirectDrawSurface4) result
 *     E-0x100   ddsd     DDSURFACEDESC, 0x6C bytes, ending exactly at E-0x94
 *     E-0x94    osvi     OSVERSIONINFOA, 0x94 bytes
 *
 * That labelling is not a guess. The five stores at 0x1001D8B4..0x1001D8C4
 * (esp == E-0x128, displacements 0x14/0x18/0x1C/0x20/0x24) zero exactly
 * E-0x114..E-0x104 -- the five pointer slots and nothing else -- and the
 * `rep stosd` of 0x1B dwords at 0x1001DB2D fills E-0x100..E-0x94, abutting
 * osvi precisely. Both extents pin arithmetically.
 *
 * TWO DISPLACEMENT COLLISIONS. Both are the exact hazard CONVENTIONS.md
 * describes, and both are inside this one function:
 *
 *   [esp+0x24] at 0x1001D8C4 is E-0x104 (pDDS4), because esp is E-0x128
 *              there; at 0x1001DB27 it is E-0x100 (&ddsd), because esp is
 *              E-0x124. A `push eax` separates them.
 *
 *   [esp+0x14] at 0x1001DB2F is E-0x114 (pDD), because the `push 8` at
 *              0x1001DB2B has just lowered esp to E-0x128; at 0x1001DBC8,
 *              0x1001DBF7 and 0x1001DC14 it is E-0x110 (pDDS), esp being
 *              E-0x124. So the cooperative level is set on the DEVICE and the
 *              two version-deciding QueryInterfaces are made on the SURFACE.
 *              Read the displacement alone and DX5/DX6 detection appears to
 *              interrogate the IDirectDraw object, which would be wrong about
 *              the interfaces AND about which pointer gets released at
 *              0x1001DC21.
 *
 * ONE REGISTER ALSO CHANGES MEANING. ebx holds pdwDXPlatform from 0x1001D901,
 * and is RELOADED with pdwDXVersion at 0x1001DA5B. Every `mov [ebx],n` before
 * that address is a platform write (2, 0, 1, 0, 0, 0); every one after is a
 * version write (0x100, 0x200, 0x300, 0, 0, 0x500, 0x600). Consequence worth
 * stating: once DirectDrawCreate has succeeded, the platform word is never
 * written again -- not even on the failure arms that zero the version.
 *
 * ============================================================================
 * BEHAVIOUR PRESERVED THAT LOOKS LIKE A MISTAKE
 * ============================================================================
 *
 * These are recorded with the addresses that establish them, so the next
 * reader can check the claim rather than inherit it. None of them is called a
 * bug in the original except the first, which is one and is unreachable on any
 * machine that could run the game.
 *
 *  1. NT with dwMajorVersion < 4 (0x1001D91E `jae`, 0x1001D923) returns
 *     having written the PLATFORM twice (2, then 0) and the VERSION never.
 *     RallyMain's slot is uninitialised, so it compares garbage against 0x600.
 *     Only NT 3.x reaches it.
 *
 *  2. The DDRAW.DLL-missing arm at 0x1001D9C1 calls FreeLibrary on the NULL
 *     handle it just failed to get (`push edi` with edi == 0), and is the only
 *     failure arm with no OutputDebugStringA.
 *
 *  3. SetCooperativeLevel (0x1001DB72) and CreateSurface (0x1001DBB1) failing
 *     write the version back to ZERO, discarding the 0x300 that DINPUT.DLL had
 *     already earned. Every other failure arm leaves the rung it reached.
 *
 *  4. The IID_IDirectDrawSurface3 QI failing (0x1001DBDF) releases pDD and
 *     frees DDRAW.DLL but leaks pDDS, and emits no diagnostic. The DX5-only
 *     outcome (0x1001DC12 taking the jump) leaks pDDS too. Only the DX6 path
 *     releases it, at 0x1001DC21 -- and that path in turn leaks pDDS3 and
 *     pDDS4, which are never released on any path.
 *
 *  5. Any platform id other than 2 becomes BR_DXPLAT_WIN32_WINDOWS at
 *     0x1001D9A8, including VER_PLATFORM_WIN32s (0). The routine does not
 *     distinguish them.
 *
 *  6. On NT 4 exactly (0x1001D930), the probe never loads DDRAW.DLL at all and
 *     can report at most 0x300. Since RallyMain requires 0x600, the game
 *     refuses to start on NT 4 by construction, not by accident.
 */
#include "br_dxver.h"

#include <stddef.h>

/* ------------------------------------------------------------------ *
 * .rdata, decoded field-wise. See the header for the raw bytes.
 * ------------------------------------------------------------------ */
const BrDxGuid BrIidIDirectDraw2 = {           /* 0x10077CB8 */
    0xB3A6F3E0u, 0x2B43u, 0x11CFu,
    { 0xA2, 0xDE, 0x00, 0xAA, 0x00, 0xB9, 0x33, 0x56 }
};
const BrDxGuid BrIidIDirectDrawSurface3 = {    /* 0x10077CF8 */
    0xDA044E00u, 0x69B2u, 0x11D0u,
    { 0xA1, 0xD5, 0x00, 0xAA, 0x00, 0xB8, 0xDF, 0xBB }
};
const BrDxGuid BrIidIDirectDrawSurface4 = {    /* 0x10077D08 */
    0x0B2B8630u, 0xAD35u, 0x11D0u,
    { 0x8E, 0xA6, 0x00, 0x60, 0x97, 0x97, 0xEA, 0x5B }
};

int BrDxGuidEqual(const BrDxGuid *pA, const BrDxGuid *pB)
{
    int i;
    if (pA == NULL || pB == NULL)
        return pA == pB;
    if (pA->d1 != pB->d1 || pA->d2 != pB->d2 || pA->d3 != pB->d3)
        return 0;
    for (i = 0; i < 8; i++)          /* bounded: a GUID's tail is always 8 */
        if (pA->d4[i] != pB->d4[i])
            return 0;
    return 1;
}

/* ------------------------------------------------------------------ *
 * .data string literals, at the addresses the original pushes.
 * ------------------------------------------------------------------ */
const char BrDxNameDInputDll[]    = "DINPUT.DLL";          /* 0x100A9A44 */
const char BrDxNameDInputCreate[] = "DirectInputCreateA";  /* 0x100A9A10 */
const char BrDxNameDDrawDll[]     = "DDRAW.DLL";           /* 0x100A99DC */
const char BrDxNameDDrawCreate[]  = "DirectDrawCreate";    /* 0x100A99C8 */

const char BrDxMsgLoadDInputFailed[]    = "Couldn't LoadLibrary DInput\r\n";
const char BrDxMsgProcDInputFailed[]    = "Couldn't GetProcAddress DInputCreate\r\n";
const char BrDxMsgLoadDDrawFailed[]     = "Couldn't LoadLibrary DDraw\r\n";
const char BrDxMsgCreateDDrawFailed[]   = "Couldn't create DDraw\r\n";
const char BrDxMsgQiDDraw2Failed[]      = "Couldn't QI DDraw2\r\n";
const char BrDxMsgCoopLevelFailed[]     = "Couldn't Set coop level\r\n";
const char BrDxMsgCreateSurfaceFailed[] = "Couldn't CreateSurface\r\n";

/* 0x1001D8A0 -- the probe itself. The frame trace, the two displacement
 * collisions and the six preserved oddities are all in the file banner above;
 * the addresses in the margin below are where each decision is made.
 *
 * NOTE ON THIS COMMENT'S SHAPE: it opens with the address as its first token
 * on purpose. tools/isported.py's "banner over a body" detector requires that,
 * and with a decorative rule line first it reported this very function as
 * unported -- which is precisely the false negative that tool exists to
 * prevent. Validated by running it after writing this. */
void BrDxDetect(const BrDxHost *pH,
                uint32_t *pdwDXVersion, uint32_t *pdwDXPlatform)
{
    uint32_t dwPlatformId   = 0;   /* osvi +0x10, read at 0x1001D8FA */
    uint32_t dwMajorVersion = 0;   /* osvi +0x04, read at 0x1001D911 */
    void *hDDraw, *hDInput, *pfnDDrawCreate, *pfnDInputCreate;
    void *pDD   = NULL;            /* E-0x114 } all five zeroed at         */
    void *pDDS  = NULL;            /* E-0x110 } 0x1001D8B4..0x1001D8C4 by  */
    void *pDD2  = NULL;            /* E-0x10C } stores of edi, which is    */
    void *pDDS3 = NULL;            /* E-0x108 } zeroed at 0x1001D8B1       */
    void *pDDS4 = NULL;            /* E-0x104 }                            */
    BrDxSurfaceDesc ddsd;
    int32_t hr;

    /* 0x1001D8C8: osvi.dwOSVersionInfoSize = 0x94; 0x1001D8D3: GetVersionExA.
     * 0x1001D8D9 `test eax,eax; jne` -- zero is the failure. */
    if (pH->pfnGetVersionEx(pH->pCtx, &dwPlatformId, &dwMajorVersion) == 0) {
        *pdwDXVersion  = BR_DXVER_NONE;      /* 0x1001D8EB */
        *pdwDXPlatform = BR_DXPLAT_UNKNOWN;  /* 0x1001D8ED */
        return;
    }

    if (dwPlatformId == BR_DXPLAT_WIN32_NT) {          /* 0x1001D908 cmp eax,2 */
        *pdwDXPlatform = BR_DXPLAT_WIN32_NT;           /* 0x1001D918 */

        /* 0x1001D91E `cmp eax,4` then TWO branches off the SAME flags:
         *   0x1001D921 jae -> >= 4 continues
         *   0x1001D930 jne -> != 4, i.e. > 4, joins the DDRAW probe below
         * so only major == 4 exactly takes the DINPUT-only arm. */
        if (dwMajorVersion < 4) {
            *pdwDXPlatform = BR_DXPLAT_UNKNOWN;        /* 0x1001D923 */
            return;   /* NOTE 1: *pdwDXVersion is deliberately not written */
        }

        if (dwMajorVersion == 4) {
            /* NT 4. DirectX 2 shipped with it; SP3 brought DirectX 3, and the
             * only thing distinguishing them here is whether DINPUT.DLL
             * exports DirectInputCreateA. DDRAW.DLL is never consulted. */
            *pdwDXVersion = BR_DXVER_2;                /* 0x1001D93E */

            hDInput = pH->pfnLoadLibrary(pH->pCtx, BrDxNameDInputDll);
            if (hDInput == NULL) {                     /* 0x1001D94D */
                pH->pfnOutputDebugString(pH->pCtx, BrDxMsgLoadDInputFailed);
                return;                                /* stays at 0x200 */
            }

            pfnDInputCreate = pH->pfnGetProcAddress(pH->pCtx, hDInput,
                                                    BrDxNameDInputCreate);
            pH->pfnFreeLibrary(pH->pCtx, hDInput);     /* 0x1001D976 */
            if (pfnDInputCreate == NULL) {             /* 0x1001D97C */
                pH->pfnOutputDebugString(pH->pCtx, BrDxMsgProcDInputFailed);
                return;                                /* stays at 0x200 */
            }

            *pdwDXVersion = BR_DXVER_3;                /* 0x1001D996 */
            return;
        }
        /* major > 4 -- NT 5 / Windows 2000 and later. Falls into the probe
         * below with the platform word left at 2. */
    } else {
        /* NOTE 5: unconditional, for every id that is not 2. */
        *pdwDXPlatform = BR_DXPLAT_WIN32_WINDOWS;      /* 0x1001D9A8 */
    }

    /* ---- 0x1001D9AE: the common DirectDraw probe ------------------ */

    hDDraw = pH->pfnLoadLibrary(pH->pCtx, BrDxNameDDrawDll);  /* 0x1001D9B9 */
    if (hDDraw == NULL) {                                     /* 0x1001D9BD */
        *pdwDXVersion  = BR_DXVER_NONE;                       /* 0x1001D9C9 */
        *pdwDXPlatform = BR_DXPLAT_UNKNOWN;                   /* 0x1001D9CB */
        /* NOTE 2: FreeLibrary on the handle it did not get, and no
         * diagnostic. `push edi` at 0x1001D9C8 with edi == 0. */
        pH->pfnFreeLibrary(pH->pCtx, NULL);                   /* 0x1001D9CD */
        return;
    }

    pfnDDrawCreate = pH->pfnGetProcAddress(pH->pCtx, hDDraw, BrDxNameDDrawCreate);
    if (pfnDDrawCreate == NULL) {                             /* 0x1001D9EC */
        *pdwDXVersion  = BR_DXVER_NONE;                       /* 0x1001D9F8 */
        *pdwDXPlatform = BR_DXPLAT_UNKNOWN;                   /* 0x1001D9FA */
        pH->pfnFreeLibrary(pH->pCtx, hDDraw);
        /* The string says "LoadLibrary DDraw" although this is the
         * GetProcAddress arm. The original's, unchanged. */
        pH->pfnOutputDebugString(pH->pCtx, BrDxMsgLoadDDrawFailed);
        return;
    }

    /* 0x1001DA1C..0x1001DA21: DirectDrawCreate(NULL, &pDD, NULL). */
    hr = pH->pfnDirectDrawCreate(pH->pCtx, pfnDDrawCreate, &pDD);
    if (hr < 0) {                                             /* 0x1001DA25 jge */
        *pdwDXVersion  = BR_DXVER_NONE;                       /* 0x1001DA2F */
        *pdwDXPlatform = BR_DXPLAT_UNKNOWN;                   /* 0x1001DA35 */
        pH->pfnFreeLibrary(pH->pCtx, hDDraw);
        pH->pfnOutputDebugString(pH->pCtx, BrDxMsgCreateDDrawFailed);
        return;
    }

    /* From 0x1001DA5B the platform word is finished with; every write below
     * is a version write. See the register note in the banner. */
    *pdwDXVersion = BR_DXVER_1;                               /* 0x1001DA6F */

    /* 0x1001DA75: pDD->QueryInterface(IID_IDirectDraw2, &pDD2), vtbl +0x00. */
    hr = pH->pfnQueryInterface(pH->pCtx, pDD, &BrIidIDirectDraw2, &pDD2);
    if (hr < 0) {                                             /* 0x1001DA79 */
        pH->pfnRelease(pH->pCtx, pDD);                        /* 0x1001DA82 */
        pH->pfnFreeLibrary(pH->pCtx, hDDraw);
        pH->pfnOutputDebugString(pH->pCtx, BrDxMsgQiDDraw2Failed);
        return;                                               /* stays at 0x100 */
    }
    pH->pfnRelease(pH->pCtx, pDD2);                           /* 0x1001DAA9 */
    *pdwDXVersion = BR_DXVER_2;                               /* 0x1001DAB1 */

    /* 0x1001DAB7: the same DINPUT.DLL question as the NT 4 arm, asked again
     * on the mainline. */
    hDInput = pH->pfnLoadLibrary(pH->pCtx, BrDxNameDInputDll);
    if (hDInput == NULL) {                                    /* 0x1001DABB */
        pH->pfnOutputDebugString(pH->pCtx, BrDxMsgLoadDInputFailed);
        pH->pfnRelease(pH->pCtx, pDD);                        /* 0x1001DAD1 */
        pH->pfnFreeLibrary(pH->pCtx, hDDraw);
        return;                                               /* stays at 0x200 */
    }
    pfnDInputCreate = pH->pfnGetProcAddress(pH->pCtx, hDInput,
                                            BrDxNameDInputCreate);
    pH->pfnFreeLibrary(pH->pCtx, hDInput);                    /* 0x1001DAF7 */
    if (pfnDInputCreate == NULL) {                            /* 0x1001DAF9 */
        /* Order differs from the arm above: DDRAW.DLL is freed BEFORE pDD is
         * released here (0x1001DAFD then 0x1001DB07), and after it there. */
        pH->pfnFreeLibrary(pH->pCtx, hDDraw);
        pH->pfnRelease(pH->pCtx, pDD);
        pH->pfnOutputDebugString(pH->pCtx, BrDxMsgProcDInputFailed);
        return;                                               /* stays at 0x200 */
    }

    /* 0x1001DB2D: zero all 0x6C bytes, then set three fields. */
    ddsd.dwSize  = BR_DDSURFACEDESC_SIZE;                     /* 0x1001DB3E */
    ddsd.dwFlags = BR_DDSD_CAPS;                              /* 0x1001DB46 */
    ddsd.dwCaps  = BR_DDSCAPS_PRIMARYSURFACE;                 /* 0x1001DB4E */

    *pdwDXVersion = BR_DXVER_3;                               /* 0x1001DB36 */

    /* 0x1001DB59: pDD->SetCooperativeLevel(NULL, DDSCL_NORMAL), vtbl +0x50.
     * The `this` is pDD -- [esp+0x14] read at 0x1001DB2F with esp == E-0x128.
     * See the displacement note in the banner. */
    hr = pH->pfnSetCooperativeLevel(pH->pCtx, pDD, NULL, BR_DDSCL_NORMAL);
    if (hr < 0) {                                             /* 0x1001DB62 */
        pH->pfnRelease(pH->pCtx, pDD);
        pH->pfnFreeLibrary(pH->pCtx, hDDraw);
        *pdwDXVersion = BR_DXVER_NONE;   /* NOTE 3: 0x1001DB72, not 0x300 */
        pH->pfnOutputDebugString(pH->pCtx, BrDxMsgCoopLevelFailed);
        return;
    }

    /* 0x1001DB98: pDD->CreateSurface(&ddsd, &pDDS, NULL), vtbl +0x18. */
    hr = pH->pfnCreateSurface(pH->pCtx, pDD, &ddsd, &pDDS);
    if (hr < 0) {                                             /* 0x1001DB9D */
        pH->pfnRelease(pH->pCtx, pDD);
        pH->pfnFreeLibrary(pH->pCtx, hDDraw);
        *pdwDXVersion = BR_DXVER_NONE;   /* NOTE 3: 0x1001DBB1 */
        pH->pfnOutputDebugString(pH->pCtx, BrDxMsgCreateSurfaceFailed);
        return;
    }

    /* 0x1001DBD9: pDDS->QueryInterface(IID_IDirectDrawSurface3, &pDDS3).
     * The `this` is the SURFACE -- [esp+0x14] at 0x1001DBC8 with
     * esp == E-0x124. */
    hr = pH->pfnQueryInterface(pH->pCtx, pDDS, &BrIidIDirectDrawSurface3, &pDDS3);
    if (hr < 0) {                                             /* 0x1001DBDD */
        /* NOTE 4: pDDS leaked, no diagnostic, version left at 0x300. */
        pH->pfnRelease(pH->pCtx, pDD);                        /* 0x1001DBE6 */
        pH->pfnFreeLibrary(pH->pCtx, hDDraw);                 /* 0x1001DBEA */
        return;
    }

    *pdwDXVersion = BR_DXVER_5;                               /* 0x1001DC08 */

    /* 0x1001DC0E: pDDS->QueryInterface(IID_IDirectDrawSurface4, &pDDS4).
     * This is the test that produces the 0x600 RallyMain demands. */
    hr = pH->pfnQueryInterface(pH->pCtx, pDDS, &BrIidIDirectDrawSurface4, &pDDS4);
    if (hr >= 0) {                                            /* 0x1001DC12 jl */
        *pdwDXVersion = BR_DXVER_6;                           /* 0x1001DC18 */
        pH->pfnRelease(pH->pCtx, pDDS);                       /* 0x1001DC21 */
    }
    /* NOTE 4 again: when the QI fails, pDDS is not released at all. */

    pH->pfnRelease(pH->pCtx, pDD);                            /* 0x1001DC2B */
    pH->pfnFreeLibrary(pH->pCtx, hDDraw);                     /* 0x1001DC2F */

    /* pDDS3 and pDDS4 are never released on any path. */
    (void)pDDS3;
    (void)pDDS4;
}

/* RallyMain 0x1001CC5C `cmp eax,0x600` + 0x1001CC61 `jae`. Unsigned. */
int BrDxVersionIsSufficient(uint32_t dwDXVersion)
{
    return dwDXVersion >= BR_DXVER_REQUIRED;
}
