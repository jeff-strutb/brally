/* br_dxver.h -- PLATFORM: the DirectX runtime capability probe.
 *
 * ARCHITECTURAL CONCERN: platform. This module answers exactly one question,
 * once, before the window exists: WHICH DIRECTX IS INSTALLED. Nothing here
 * draws, sounds, or reads input; it only interrogates the machine.
 *
 * (The brief that commissioned this work asked for `br_cmdline`. That is not
 * what 0x1001D8A0 is -- it never touches the command line, and RallyMain's
 * command line arrives separately at 0x105BC738 and is handed to 0x10007F40.
 * The module is named for the concern it turned out to be.)
 *
 * THE ONE FUNCTION
 *
 *   0x1001D8A0  (Glide, 924 bytes)   == 0x10030210 (D3D), `shared` in
 *                                      config/shared.csv, so either build
 *                                      answers and no arbitration is needed.
 *
 * It is the DirectX SDK's `GetDXVersion` sample routine, adapted: two out
 * pointers, no return value, and OutputDebugStringA on every failure arm.
 * Every identification below is from the bytes, not from the resemblance --
 * the three GUIDs were decoded out of .rdata and the seven diagnostic strings
 * read out of .data, and they say what the routine is doing at each step.
 *
 * HOW IT IS CALLED, AND WHAT 0x600 MEANS
 *
 * RallyMain (0x1001CC00) is the only caller:
 *
 *      1001CC3A  lea eax,[esp+0xC]      ; -> R-4
 *      1001CC3E  lea ecx,[esp+8]        ; -> R-8
 *      1001CC42  push eax
 *      1001CC43  push ecx
 *      1001CC50  call 0x1001D8A0        ; cdecl
 *      1001CC55  mov eax,[esp+0x10]     ; esp == R-0x18 here -> R-8
 *      1001CC59  add esp,8
 *      1001CC5C  cmp eax,0x600
 *      1001CC61  jae 0x1001CC91         ; UNSIGNED
 *
 * `push ecx` is last, so ecx is argument ONE. The slot RallyMain then reads
 * back is R-8, i.e. ecx's, i.e. argument one -- so argument one is the
 * VERSION and argument two is the PLATFORM. Confirmed independently from the
 * callee: its GetVersionExA-failed arm at 0x1001D8DD writes zero through both,
 * and 0x1001D908's platform-id arm writes 1/2 through the SECOND only.
 *
 * 0x600 is DIRECTX 6.0, and the routine earns that number in exactly one
 * place: 0x1001DC08..0x1001DC18 sets 0x500, queries the primary surface for
 * {0B2B8630-AD35-11D0-8EA6-00609797EA5B} == IID_IDirectDrawSurface4, and only
 * on success raises it to 0x600. IDirectDrawSurface4 is the interface DirectX
 * 6 added. So RallyMain's test is "DirectX 6 or later, or refuse to run" --
 * it is a hard requirement check, and the MessageBox it puts up otherwise is
 * built from runtime string ids 0x126 (caption) and 0x128 (text) through the
 * table lookup at 0x1006D280 (`[id*4 + 0x1186C488]`, valid ids 1..0x12E).
 * Those strings are NOT in the PE's resources -- the table is filled at
 * runtime -- so the text cannot be recovered from the binary alone.
 *
 * The compare is `jae`, i.e. UNSIGNED, and that is load-bearing rather than
 * incidental: see BrDxVersionIsSufficient below and the NT 3.x note.
 *
 * THE VERSION LADDER, each rung with the address that sets it
 *
 *   0x000  0x1001D8EB etc.  nothing usable
 *   0x100  0x1001DA6F       DirectDrawCreate succeeded            (DX1)
 *   0x200  0x1001DAB1       QI IID_IDirectDraw2 succeeded         (DX2)
 *   0x300  0x1001DB36       DINPUT.DLL exports DirectInputCreateA (DX3)
 *   0x500  0x1001DC08       QI IID_IDirectDrawSurface3 succeeded  (DX5)
 *   0x600  0x1001DC18       QI IID_IDirectDrawSurface4 succeeded  (DX6)
 *
 * There is no 0x400: DirectX 4 was never released, and the routine's ladder
 * reflects that rather than skipping a test.
 *
 * WHAT THIS MODULE DOES NOT DO
 *
 * The probe is entirely Win32 and COM, and this tree forbids both in portable
 * code. So the DECISION LOGIC -- the order of the probes, the value written at
 * each rung, which arm returns early, which pointer each call is made on -- is
 * transcribed here and is the whole content of the original; the six Win32
 * calls and five COM methods it makes are reached through BrDxHost. That is a
 * seam, not a stand-in: BrDxDetect has no default host and will not invent one,
 * so a caller that has no platform binding cannot accidentally receive a
 * plausible-looking DirectX version. There is no host binding in this tree yet,
 * and RallyMain itself is not transcribed either (br_boot.c covers the state
 * machine and frame tick, not 0x1001CC00), so this module currently has no
 * production caller. That is the frontier, stated rather than papered over.
 *
 * The original has NO static or global state, so there is no storage here to
 * alias against another module's -- the recurring hazard in CONVENTIONS.md
 * does not arise.
 */
#ifndef BR_DXVER_H
#define BR_DXVER_H

#include <stdint.h>

/* ------------------------------------------------------------------ *
 * GUIDs. All three live in .rdata -- they are DATA, not code, and are
 * decoded FIELD-WISE rather than overlaid, because the on-disk form is
 * little-endian d1/d2/d3 followed by eight raw bytes.
 *
 *   0x10077CB8  B3A6F3E0-2B43-11CF-A2DE-00AA00B93356  IID_IDirectDraw2
 *   0x10077CF8  DA044E00-69B2-11D0-A1D5-00AA00B8DFBB  IID_IDirectDrawSurface3
 *   0x10077D08  0B2B8630-AD35-11D0-8EA6-00609797EA5B  IID_IDirectDrawSurface4
 * ------------------------------------------------------------------ */
typedef struct BrDxGuid {
    uint32_t d1;
    uint16_t d2;
    uint16_t d3;
    uint8_t  d4[8];
} BrDxGuid;

extern const BrDxGuid BrIidIDirectDraw2;         /* 0x10077CB8 (.rdata) */
extern const BrDxGuid BrIidIDirectDrawSurface3;  /* 0x10077CF8 (.rdata) */
extern const BrDxGuid BrIidIDirectDrawSurface4;  /* 0x10077D08 (.rdata) */

/* Byte-for-byte GUID comparison. 1 == equal. */
int BrDxGuidEqual(const BrDxGuid *pA, const BrDxGuid *pB);

/* ------------------------------------------------------------------ *
 * The values the probe writes.
 * ------------------------------------------------------------------ */
#define BR_DXVER_NONE      0x000u
#define BR_DXVER_1         0x100u
#define BR_DXVER_2         0x200u
#define BR_DXVER_3         0x300u
#define BR_DXVER_5         0x500u
#define BR_DXVER_6         0x600u

/* RallyMain 0x1001CC5C: `cmp eax,0x600`. */
#define BR_DXVER_REQUIRED  BR_DXVER_6

/* Win32 VER_PLATFORM_* as this routine uses them. 0 is also written as a
 * deliberate "could not tell", which is not a Win32 value at all. */
#define BR_DXPLAT_UNKNOWN        0u
#define BR_DXPLAT_WIN32_WINDOWS  1u   /* 0x1001D9A8 */
#define BR_DXPLAT_WIN32_NT       2u   /* 0x1001D918 */

/* Literals the original hands to the DirectDraw calls. Named so the transcript
 * reads as the original does; sizes are the 32-bit ones the original used and
 * are quoted, never used to size a host struct. */
#define BR_OSVERSIONINFOA_SIZE     0x94u  /* 148, stored at 0x1001D8C8 */
#define BR_DDSURFACEDESC_SIZE      0x6Cu  /* 108, stored at 0x1001DB3E */
#define BR_DDSD_CAPS               0x00000001u  /* 0x1001DB46 */
#define BR_DDSCAPS_PRIMARYSURFACE  0x00000200u  /* 0x1001DB4E */
#define BR_DDSCL_NORMAL            0x00000008u  /* 0x1001DB2B */

/* The DDSURFACEDESC the original builds. It zeroes all 108 bytes with
 * `rep stosd` of 0x1B dwords at 0x1001DB2D and then sets exactly three
 * fields; those three are all that is modelled. Original offsets:
 *
 *     +0x00  dwSize          = 0x6C
 *     +0x04  dwFlags         = DDSD_CAPS
 *     +0x68  ddsCaps.dwCaps  = DDSCAPS_PRIMARYSURFACE
 *
 * This is NOT a layout to overlay on anything: 0x6C is the 32-bit size and a
 * host binding must build the real DDSURFACEDESC itself from these values. */
typedef struct BrDxSurfaceDesc {
    uint32_t dwSize;
    uint32_t dwFlags;
    uint32_t dwCaps;
} BrDxSurfaceDesc;

/* ------------------------------------------------------------------ *
 * The platform seam.
 *
 * Handles are opaque `void *`, never a dword: on LP64 an HMODULE, a FARPROC
 * and an interface pointer are all 64 bits wide and none of them fits in the
 * uint32_t the original kept them in.
 *
 * HRESULTs are int32_t and the original tests them SIGNED (`test eax,eax`
 * followed by `jge`/`jl`), so failure is `hr < 0`.
 * ------------------------------------------------------------------ */
typedef struct BrDxHost {
    void *pCtx;

    /* GetVersionExA on a 148-byte OSVERSIONINFOA. Zero == failed, exactly as
     * `test eax,eax; jne` at 0x1001D8D9 reads it. On success both outputs are
     * filled: dwPlatformId is OSVERSIONINFO+0x10, dwMajorVersion is +0x04. */
    int32_t (*pfnGetVersionEx)(void *pCtx, uint32_t *pdwPlatformId,
                               uint32_t *pdwMajorVersion);

    void   *(*pfnLoadLibrary)(void *pCtx, const char *pszName);
    void   *(*pfnGetProcAddress)(void *pCtx, void *hModule, const char *pszProc);
    void    (*pfnFreeLibrary)(void *pCtx, void *hModule);
    void    (*pfnOutputDebugString)(void *pCtx, const char *psz);

    /* DirectDrawCreate(NULL, ppDD, NULL) through the pointer GetProcAddress
     * returned. pfnCreate is passed so a binding -- and a test -- can see that
     * the probe really calls what it looked up. */
    int32_t (*pfnDirectDrawCreate)(void *pCtx, void *pfnCreate, void **ppDD);

    /* IUnknown::QueryInterface, vtbl +0x00. */
    int32_t (*pfnQueryInterface)(void *pCtx, void *pThis, const BrDxGuid *pIID,
                                 void **ppOut);
    /* IUnknown::Release, vtbl +0x08. */
    void    (*pfnRelease)(void *pCtx, void *pThis);
    /* IDirectDraw::SetCooperativeLevel, vtbl +0x50. */
    int32_t (*pfnSetCooperativeLevel)(void *pCtx, void *pDD, void *hWnd,
                                      uint32_t dwFlags);
    /* IDirectDraw::CreateSurface, vtbl +0x18. */
    int32_t (*pfnCreateSurface)(void *pCtx, void *pDD,
                                const BrDxSurfaceDesc *pDesc, void **ppSurf);
} BrDxHost;

/* The strings the original passes, at their .data addresses. Exported so a
 * test can pin WHICH arm ran, and so a host binding uses the original's own
 * library and entry-point names rather than retyping them. */
extern const char BrDxNameDInputDll[];         /* 0x100A9A44 "DINPUT.DLL" */
extern const char BrDxNameDInputCreate[];      /* 0x100A9A10 "DirectInputCreateA" */
extern const char BrDxNameDDrawDll[];          /* 0x100A99DC "DDRAW.DLL" */
extern const char BrDxNameDDrawCreate[];       /* 0x100A99C8 "DirectDrawCreate" */

extern const char BrDxMsgLoadDInputFailed[];   /* 0x100A9A24 */
extern const char BrDxMsgProcDInputFailed[];   /* 0x100A99E8 */
extern const char BrDxMsgLoadDDrawFailed[];    /* 0x100A99A8 */
extern const char BrDxMsgCreateDDrawFailed[];  /* 0x100A9990 */
extern const char BrDxMsgQiDDraw2Failed[];     /* 0x100A9978 */
extern const char BrDxMsgCoopLevelFailed[];    /* 0x100A995C */
extern const char BrDxMsgCreateSurfaceFailed[];/* 0x100A9940 */

/* ------------------------------------------------------------------ *
 * 0x1001D8A0 -- the probe.
 *
 * pHost is REQUIRED and is not defaulted; see the banner. *pdwDXVersion is
 * NOT written on every path (see the NT 3.x note in br_dxver.c), which is why
 * this cannot be a return value.
 * ------------------------------------------------------------------ */
void BrDxDetect(const BrDxHost *pHost,
                uint32_t *pdwDXVersion, uint32_t *pdwDXPlatform);

/* RallyMain 0x1001CC5C/0x1001CC61 -- `cmp eax,0x600` + `jae`, UNSIGNED.
 * 1 == the game is allowed to start.
 *
 * The unsignedness is observable, and only in one place. On every path where
 * the probe writes a version the value is 0..0x600 and `jae` and `jge` agree.
 * But on NT 3.x the probe returns WITHOUT writing (0x1001D923), and RallyMain
 * never initialised that stack slot -- `sub esp,8` at 0x1001CC00 is the only
 * thing that touches it -- so the compared value is whatever the stack held.
 * With bit 31 set, `jae` starts the game and `jge` would not. That is where
 * the polarity decides something, so it is preserved literally. */
int BrDxVersionIsSufficient(uint32_t dwDXVersion);

#endif /* BR_DXVER_H */
