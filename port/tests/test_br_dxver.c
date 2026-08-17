/* test_br_dxver.c -- the DirectX capability probe, 0x1001D8A0.
 *
 * WHAT THIS ASSERTS, AND WHY EACH ASSERTION CAN FAIL
 *
 * The probe is a ladder of eleven decisions and the only way to get it wrong
 * is to put a rung in the wrong place, make a call on the wrong object, or
 * lose one of the arms where the original throws away a rung it had already
 * earned. So the suite drives a SCRIPTED HOST and pins three things at once
 * for every case: the version, the platform, and the exact sequence of
 * platform calls.
 *
 * Two traps this project has documented are specifically guarded here.
 *
 *  - "a fixture that leaves the deciding field zero passes under both
 *    readings". The NT 3.x arm at 0x1001D923 returns WITHOUT writing the
 *    version. Initialising the output to 0 would make "not written" and
 *    "written 0" indistinguishable, so every case seeds both outputs with
 *    SENTINEL and checks the sentinel survives where the original is silent.
 *
 *  - "a stack displacement is meaningless without its ESP". [esp+0x14] is pDD
 *    at 0x1001DB2F and pDDS at 0x1001DBC8/0x1001DBF7/0x1001DC14. The host
 *    hands out DISTINCT tokens for the device and the surface and the call
 *    log records which one each call was made on, so transcribing the two
 *    version-deciding QueryInterfaces against the device fails loudly instead
 *    of quietly answering the wrong question.
 *
 * The log is capped and the overflow flag is asserted, so a transcription
 * that looped would fail rather than hang.
 */
#include "br_dxver.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

static int g_fails;

#define CHECK(c) do { if (!(c)) { \
    printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); g_fails++; } } while (0)

/* ------------------------------------------------------------------ *
 * The scripted host.
 * ------------------------------------------------------------------ */

/* Distinct tokens, so "which object was this call made on" is answerable. */
static int OBJ_DD, OBJ_DDS, OBJ_DD2, OBJ_DDS3, OBJ_DDS4;
static int MOD_DDRAW, MOD_DINPUT, PFN_DDC, PFN_DIC;
#define TOK(x) ((void *)&(x))

#define MAXEV 64
#define EVLEN 96
static char g_ev[MAXEV][EVLEN];
static int  g_nev;
static int  g_evOverflow;

static void ev(const char *fmt, ...)
{
    va_list ap;
    if (g_nev >= MAXEV) { g_evOverflow = 1; return; }
    va_start(ap, fmt);
    vsnprintf(g_ev[g_nev], EVLEN, fmt, ap);
    va_end(ap);
    g_nev++;
}

static const char *nm(void *p)
{
    if (p == NULL)          return "NULL";
    if (p == TOK(OBJ_DD))   return "pDD";
    if (p == TOK(OBJ_DDS))  return "pDDS";
    if (p == TOK(OBJ_DD2))  return "pDD2";
    if (p == TOK(OBJ_DDS3)) return "pDDS3";
    if (p == TOK(OBJ_DDS4)) return "pDDS4";
    if (p == TOK(MOD_DDRAW))  return "hDDRAW";
    if (p == TOK(MOD_DINPUT)) return "hDINPUT";
    if (p == TOK(PFN_DDC))  return "pfnDDrawCreate";
    if (p == TOK(PFN_DIC))  return "pfnDInputCreate";
    return "UNKNOWN";
}

static const char *iidnm(const BrDxGuid *p)
{
    if (BrDxGuidEqual(p, &BrIidIDirectDraw2))        return "IID_IDirectDraw2";
    if (BrDxGuidEqual(p, &BrIidIDirectDrawSurface3)) return "IID_IDDrawSurface3";
    if (BrDxGuidEqual(p, &BrIidIDirectDrawSurface4)) return "IID_IDDrawSurface4";
    return "IID_UNKNOWN";
}

struct Script {
    int32_t  rcGetVersionEx;   /* 0 == the API failed */
    uint32_t dwPlatformId;
    uint32_t dwMajorVersion;
    int      fNoDDrawDll;
    int      fNoDInputDll;
    int      fNoDDrawCreateProc;
    int      fNoDInputCreateProc;
    int32_t  hrDDrawCreate;
    int32_t  hrQiDDraw2;
    int32_t  hrCoopLevel;
    int32_t  hrCreateSurface;
    int32_t  hrQiSurface3;
    int32_t  hrQiSurface4;
};
static struct Script g_s;

static int32_t h_getver(void *ctx, uint32_t *ppid, uint32_t *pmaj)
{
    (void)ctx;
    ev("GetVersionEx");
    if (g_s.rcGetVersionEx == 0)
        return 0;
    *ppid = g_s.dwPlatformId;
    *pmaj = g_s.dwMajorVersion;
    return g_s.rcGetVersionEx;
}

static void *h_loadlib(void *ctx, const char *psz)
{
    (void)ctx;
    ev("LoadLibrary(%s)", psz);
    if (strcmp(psz, "DDRAW.DLL") == 0)
        return g_s.fNoDDrawDll ? NULL : TOK(MOD_DDRAW);
    if (strcmp(psz, "DINPUT.DLL") == 0)
        return g_s.fNoDInputDll ? NULL : TOK(MOD_DINPUT);
    ev("!! unexpected library");
    return NULL;
}

static void *h_getproc(void *ctx, void *hMod, const char *psz)
{
    (void)ctx;
    ev("GetProcAddress(%s,%s)", nm(hMod), psz);
    if (strcmp(psz, "DirectDrawCreate") == 0)
        return g_s.fNoDDrawCreateProc ? NULL : TOK(PFN_DDC);
    if (strcmp(psz, "DirectInputCreateA") == 0)
        return g_s.fNoDInputCreateProc ? NULL : TOK(PFN_DIC);
    ev("!! unexpected proc");
    return NULL;
}

static void h_freelib(void *ctx, void *hMod)  { (void)ctx; ev("FreeLibrary(%s)", nm(hMod)); }
static void h_dbg(void *ctx, const char *psz) { (void)ctx; ev("Debug(%s)", psz); }
static void h_release(void *ctx, void *p)     { (void)ctx; ev("Release(%s)", nm(p)); }

static int32_t h_ddcreate(void *ctx, void *pfn, void **ppDD)
{
    (void)ctx;
    ev("DirectDrawCreate(%s)", nm(pfn));
    if (g_s.hrDDrawCreate >= 0)
        *ppDD = TOK(OBJ_DD);
    return g_s.hrDDrawCreate;
}

static int32_t h_qi(void *ctx, void *pThis, const BrDxGuid *pIID, void **ppOut)
{
    int32_t hr;
    void *pOut;
    (void)ctx;
    ev("QI(%s,%s)", nm(pThis), iidnm(pIID));
    if (BrDxGuidEqual(pIID, &BrIidIDirectDraw2)) {
        hr = g_s.hrQiDDraw2;  pOut = TOK(OBJ_DD2);
    } else if (BrDxGuidEqual(pIID, &BrIidIDirectDrawSurface3)) {
        hr = g_s.hrQiSurface3; pOut = TOK(OBJ_DDS3);
    } else if (BrDxGuidEqual(pIID, &BrIidIDirectDrawSurface4)) {
        hr = g_s.hrQiSurface4; pOut = TOK(OBJ_DDS4);
    } else {
        return -1;
    }
    if (hr >= 0)
        *ppOut = pOut;
    return hr;
}

static int32_t h_coop(void *ctx, void *pDD, void *hWnd, uint32_t dwFlags)
{
    (void)ctx;
    ev("SetCoopLevel(%s,hwnd=%s,flags=0x%x)", nm(pDD), nm(hWnd),
       (unsigned)dwFlags);
    return g_s.hrCoopLevel;
}

static int32_t h_createsurf(void *ctx, void *pDD, const BrDxSurfaceDesc *pD,
                            void **ppSurf)
{
    (void)ctx;
    ev("CreateSurface(%s,size=0x%x,flags=0x%x,caps=0x%x)", nm(pDD),
       (unsigned)pD->dwSize, (unsigned)pD->dwFlags, (unsigned)pD->dwCaps);
    if (g_s.hrCreateSurface >= 0)
        *ppSurf = TOK(OBJ_DDS);
    return g_s.hrCreateSurface;
}

static const BrDxHost g_host = {
    NULL,
    h_getver, h_loadlib, h_getproc, h_freelib, h_dbg,
    h_ddcreate, h_qi, h_release, h_coop, h_createsurf
};

/* ------------------------------------------------------------------ *
 * Driving one case.
 *
 * SENTINEL, not zero: see the banner. A case that leaves an output alone is
 * distinguishable from one that writes 0 only because these differ.
 * ------------------------------------------------------------------ */
#define SENTINEL 0xDEADBEEFu

static uint32_t g_ver, g_plat;

/* A machine on which everything works: Windows 98, full DirectX 6. */
static void script_ok_win9x(void)
{
    memset(&g_s, 0, sizeof g_s);
    g_s.rcGetVersionEx = 1;
    g_s.dwPlatformId   = BR_DXPLAT_WIN32_WINDOWS;
    g_s.dwMajorVersion = 4;
    /* every hr already 0 == S_OK, every fNo* already 0 */
}

static void run(void)
{
    g_nev = 0;
    g_evOverflow = 0;
    g_ver = SENTINEL;
    g_plat = SENTINEL;
    BrDxDetect(&g_host, &g_ver, &g_plat);
    CHECK(g_evOverflow == 0);      /* a looping transcript fails, not hangs */
}

static int hasev(const char *pszNeedle)
{
    int i;
    for (i = 0; i < g_nev && i < MAXEV; i++)
        if (strstr(g_ev[i], pszNeedle) != NULL)
            return 1;
    return 0;
}

static int countev(const char *pszNeedle)
{
    int i, n = 0;
    for (i = 0; i < g_nev && i < MAXEV; i++)
        if (strstr(g_ev[i], pszNeedle) != NULL)
            n++;
    return n;
}

static void dumpev(const char *pszWhy)
{
    int i;
    printf("  -- call log (%s):\n", pszWhy);
    for (i = 0; i < g_nev && i < MAXEV; i++)
        printf("     %2d %s\n", i, g_ev[i]);
}

/* Exact whole-sequence comparison. Stronger than "contains", and it is what
 * catches a rung moved rather than removed. */
static void evseq(const char *const *exp, int n, const char *pszWhat)
{
    int i, bad = 0;
    if (g_nev != n) bad = 1;
    for (i = 0; i < n && i < g_nev; i++)
        if (strcmp(g_ev[i], exp[i]) != 0) bad = 1;
    if (bad) {
        printf("FAIL %s: call sequence (expected %d events, got %d)\n",
               pszWhat, n, g_nev);
        for (i = 0; i < n; i++) printf("     want %2d %s\n", i, exp[i]);
        dumpev(pszWhat);
        g_fails++;
    }
}

/* ------------------------------------------------------------------ *
 * 1. GetVersionExA failing -- 0x1001D8DD
 * ------------------------------------------------------------------ */
static void test_getversion_failed(void)
{
    static const char *const want[] = { "GetVersionEx" };
    script_ok_win9x();
    g_s.rcGetVersionEx = 0;
    run();
    CHECK(g_ver  == BR_DXVER_NONE);
    CHECK(g_plat == BR_DXPLAT_UNKNOWN);
    evseq(want, 1, "getversion_failed");
}

/* ------------------------------------------------------------------ *
 * 2. NT 3.x -- 0x1001D91E `jae` not taken, 0x1001D923.
 *
 * Platform is written TWICE (2 then 0) and the version is never written at
 * all. The sentinel is the whole point of this case.
 * ------------------------------------------------------------------ */
static void test_nt3_leaves_version_unwritten(void)
{
    static const char *const want[] = { "GetVersionEx" };
    script_ok_win9x();
    g_s.dwPlatformId   = BR_DXPLAT_WIN32_NT;
    g_s.dwMajorVersion = 3;
    run();
    CHECK(g_ver  == SENTINEL);                 /* NOT written -- 0 would be wrong */
    CHECK(g_plat == BR_DXPLAT_UNKNOWN);
    evseq(want, 1, "nt3");                     /* and no library is loaded */
}

/* ------------------------------------------------------------------ *
 * 3-5. NT 4 exactly -- 0x1001D930. DDRAW.DLL is never consulted, so the best
 * this machine can report is 0x300 and RallyMain's 0x600 gate always refuses.
 * ------------------------------------------------------------------ */
static void test_nt4(void)
{
    static const char *const want_dx3[] = {
        "GetVersionEx",
        "LoadLibrary(DINPUT.DLL)",
        "GetProcAddress(hDINPUT,DirectInputCreateA)",
        "FreeLibrary(hDINPUT)"
    };
    static const char *const want_nodll[] = {
        "GetVersionEx",
        "LoadLibrary(DINPUT.DLL)",
        "Debug(Couldn't LoadLibrary DInput\r\n)"
    };
    static const char *const want_noproc[] = {
        "GetVersionEx",
        "LoadLibrary(DINPUT.DLL)",
        "GetProcAddress(hDINPUT,DirectInputCreateA)",
        "FreeLibrary(hDINPUT)",
        "Debug(Couldn't GetProcAddress DInputCreate\r\n)"
    };

    /* DirectX 3 present */
    script_ok_win9x();
    g_s.dwPlatformId   = BR_DXPLAT_WIN32_NT;
    g_s.dwMajorVersion = 4;
    run();
    CHECK(g_ver  == BR_DXVER_3);
    CHECK(g_plat == BR_DXPLAT_WIN32_NT);
    CHECK(BrDxVersionIsSufficient(g_ver) == 0);   /* NT4 can never start the game */
    CHECK(!hasev("DDRAW"));                       /* the arm never touches DDraw */
    evseq(want_dx3, 4, "nt4_dx3");

    /* DINPUT.DLL absent -- stays at 0x200 */
    script_ok_win9x();
    g_s.dwPlatformId   = BR_DXPLAT_WIN32_NT;
    g_s.dwMajorVersion = 4;
    g_s.fNoDInputDll   = 1;
    run();
    CHECK(g_ver  == BR_DXVER_2);
    CHECK(g_plat == BR_DXPLAT_WIN32_NT);
    evseq(want_nodll, 3, "nt4_nodll");

    /* DirectInputCreateA absent -- stays at 0x200, and the library is freed
     * BEFORE the pointer is tested (0x1001D976 precedes 0x1001D97C) */
    script_ok_win9x();
    g_s.dwPlatformId        = BR_DXPLAT_WIN32_NT;
    g_s.dwMajorVersion      = 4;
    g_s.fNoDInputCreateProc = 1;
    run();
    CHECK(g_ver  == BR_DXVER_2);
    CHECK(g_plat == BR_DXPLAT_WIN32_NT);
    evseq(want_noproc, 5, "nt4_noproc");
}

/* ------------------------------------------------------------------ *
 * 6. The whole successful ladder, in order.
 *
 * This is the case that pins WHICH OBJECT each call is made on, and the two
 * IIDs that decide 0x500 and 0x600.
 * ------------------------------------------------------------------ */
static void test_full_success_win9x(void)
{
    static const char *const want[] = {
        "GetVersionEx",
        "LoadLibrary(DDRAW.DLL)",
        "GetProcAddress(hDDRAW,DirectDrawCreate)",
        "DirectDrawCreate(pfnDDrawCreate)",
        "QI(pDD,IID_IDirectDraw2)",
        "Release(pDD2)",
        "LoadLibrary(DINPUT.DLL)",
        "GetProcAddress(hDINPUT,DirectInputCreateA)",
        "FreeLibrary(hDINPUT)",
        "SetCoopLevel(pDD,hwnd=NULL,flags=0x8)",
        "CreateSurface(pDD,size=0x6c,flags=0x1,caps=0x200)",
        "QI(pDDS,IID_IDDrawSurface3)",
        "QI(pDDS,IID_IDDrawSurface4)",
        "Release(pDDS)",
        "Release(pDD)",
        "FreeLibrary(hDDRAW)"
    };
    script_ok_win9x();
    run();
    CHECK(g_ver  == BR_DXVER_6);
    CHECK(g_plat == BR_DXPLAT_WIN32_WINDOWS);
    CHECK(BrDxVersionIsSufficient(g_ver) == 1);
    evseq(want, 16, "full_success");
    /* pDDS3 and pDDS4 are leaked on every path -- 0x1001DC21 releases pDDS */
    CHECK(countev("Release(pDDS3)") == 0);
    CHECK(countev("Release(pDDS4)") == 0);
}

/* ------------------------------------------------------------------ *
 * 7-8. The platform word.
 * ------------------------------------------------------------------ */
static void test_platform_word(void)
{
    /* 0x1001D9A8 is unconditional for every id that is not 2, including
     * VER_PLATFORM_WIN32s == 0 */
    script_ok_win9x();
    g_s.dwPlatformId = 0;
    run();
    CHECK(g_plat == BR_DXPLAT_WIN32_WINDOWS);
    CHECK(g_ver  == BR_DXVER_6);

    /* NT 5 goes through the DDraw probe with the platform left at 2 */
    script_ok_win9x();
    g_s.dwPlatformId   = BR_DXPLAT_WIN32_NT;
    g_s.dwMajorVersion = 5;
    run();
    CHECK(g_plat == BR_DXPLAT_WIN32_NT);
    CHECK(g_ver  == BR_DXVER_6);
    CHECK(hasev("LoadLibrary(DDRAW.DLL)"));

    /* ...and once DirectDrawCreate has succeeded the platform is final: the
     * later failure arms zero the VERSION only (ebx is reloaded at
     * 0x1001DA5B). Coop-level failure on NT5 must leave platform == 2. */
    script_ok_win9x();
    g_s.dwPlatformId   = BR_DXPLAT_WIN32_NT;
    g_s.dwMajorVersion = 5;
    g_s.hrCoopLevel    = -1;
    run();
    CHECK(g_plat == BR_DXPLAT_WIN32_NT);
    CHECK(g_ver  == BR_DXVER_NONE);
}

/* ------------------------------------------------------------------ *
 * 9-11. The three arms that zero BOTH words, before ebx is reloaded.
 * ------------------------------------------------------------------ */
static void test_early_failures_zero_both(void)
{
    static const char *const want_nodll[] = {
        "GetVersionEx",
        "LoadLibrary(DDRAW.DLL)",
        "FreeLibrary(NULL)"           /* 0x1001D9CD, on the handle it lacks */
    };
    static const char *const want_noproc[] = {
        "GetVersionEx",
        "LoadLibrary(DDRAW.DLL)",
        "GetProcAddress(hDDRAW,DirectDrawCreate)",
        "FreeLibrary(hDDRAW)",
        "Debug(Couldn't LoadLibrary DDraw\r\n)"
    };
    static const char *const want_nocreate[] = {
        "GetVersionEx",
        "LoadLibrary(DDRAW.DLL)",
        "GetProcAddress(hDDRAW,DirectDrawCreate)",
        "DirectDrawCreate(pfnDDrawCreate)",
        "FreeLibrary(hDDRAW)",
        "Debug(Couldn't create DDraw\r\n)"
    };

    script_ok_win9x();
    g_s.fNoDDrawDll = 1;
    run();
    CHECK(g_ver  == BR_DXVER_NONE);
    CHECK(g_plat == BR_DXPLAT_UNKNOWN);
    CHECK(countev("Debug(") == 0);      /* the only silent failure arm */
    evseq(want_nodll, 3, "no_ddraw_dll");

    script_ok_win9x();
    g_s.fNoDDrawCreateProc = 1;
    run();
    CHECK(g_ver  == BR_DXVER_NONE);
    CHECK(g_plat == BR_DXPLAT_UNKNOWN);
    evseq(want_noproc, 5, "no_ddraw_proc");

    script_ok_win9x();
    g_s.hrDDrawCreate = -1;
    run();
    CHECK(g_ver  == BR_DXVER_NONE);
    CHECK(g_plat == BR_DXPLAT_UNKNOWN);
    evseq(want_nocreate, 6, "ddraw_create_failed");
}

/* ------------------------------------------------------------------ *
 * 12-14. The rungs that are KEPT and the two that are thrown away.
 * ------------------------------------------------------------------ */
static void test_rungs(void)
{
    /* IID_IDirectDraw2 fails -> keeps 0x100 */
    script_ok_win9x();
    g_s.hrQiDDraw2 = -1;
    run();
    CHECK(g_ver  == BR_DXVER_1);
    CHECK(g_plat == BR_DXPLAT_WIN32_WINDOWS);
    CHECK(hasev("Debug(Couldn't QI DDraw2\r\n)"));
    CHECK(countev("Release(pDD)") == 1);
    CHECK(countev("Release(pDD2)") == 0);   /* nothing was handed back */

    /* DINPUT.DLL absent on the mainline -> keeps 0x200 */
    script_ok_win9x();
    g_s.fNoDInputDll = 1;
    run();
    CHECK(g_ver == BR_DXVER_2);
    CHECK(hasev("Debug(Couldn't LoadLibrary DInput\r\n)"));
    CHECK(!hasev("SetCoopLevel"));

    /* DirectInputCreateA absent -> keeps 0x200, and this arm frees DDRAW.DLL
     * BEFORE releasing pDD (0x1001DAFD then 0x1001DB07) -- the opposite order
     * from the arm just above it. */
    script_ok_win9x();
    g_s.fNoDInputCreateProc = 1;
    run();
    CHECK(g_ver == BR_DXVER_2);
    {
        static const char *const want[] = {
            "GetVersionEx",
            "LoadLibrary(DDRAW.DLL)",
            "GetProcAddress(hDDRAW,DirectDrawCreate)",
            "DirectDrawCreate(pfnDDrawCreate)",
            "QI(pDD,IID_IDirectDraw2)",
            "Release(pDD2)",
            "LoadLibrary(DINPUT.DLL)",
            "GetProcAddress(hDINPUT,DirectInputCreateA)",
            "FreeLibrary(hDINPUT)",
            "FreeLibrary(hDDRAW)",
            "Release(pDD)",
            "Debug(Couldn't GetProcAddress DInputCreate\r\n)"
        };
        evseq(want, 12, "no_dinput_proc_mainline");
    }

    /* SetCooperativeLevel fails -> the 0x300 already earned is DISCARDED */
    script_ok_win9x();
    g_s.hrCoopLevel = -1;
    run();
    CHECK(g_ver == BR_DXVER_NONE);      /* not BR_DXVER_3 */
    CHECK(hasev("Debug(Couldn't Set coop level\r\n)"));

    /* CreateSurface fails -> likewise */
    script_ok_win9x();
    g_s.hrCreateSurface = -1;
    run();
    CHECK(g_ver == BR_DXVER_NONE);      /* not BR_DXVER_3 */
    CHECK(hasev("Debug(Couldn't CreateSurface\r\n)"));

    /* IID_IDirectDrawSurface3 fails -> keeps 0x300, says NOTHING, and leaks
     * the surface (0x1001DBDF releases pDD only) */
    script_ok_win9x();
    g_s.hrQiSurface3 = -1;
    run();
    CHECK(g_ver == BR_DXVER_3);
    CHECK(countev("Debug(") == 0);
    CHECK(countev("Release(pDD)")  == 1);
    CHECK(countev("Release(pDDS)") == 0);

    /* IID_IDirectDrawSurface4 fails -> 0x500, and the surface is STILL not
     * released: 0x1001DC21 is inside the taken-branch only */
    script_ok_win9x();
    g_s.hrQiSurface4 = -1;
    run();
    CHECK(g_ver == BR_DXVER_5);
    CHECK(BrDxVersionIsSufficient(g_ver) == 0);
    CHECK(countev("Release(pDDS)") == 0);
    CHECK(countev("Release(pDD)")  == 1);
    CHECK(countev("Debug(") == 0);
}

/* ------------------------------------------------------------------ *
 * 15. RallyMain's gate: 0x1001CC5C `cmp eax,0x600`, 0x1001CC61 `jae`.
 * ------------------------------------------------------------------ */
static void test_requirement_gate(void)
{
    CHECK(BR_DXVER_REQUIRED == 0x600u);
    CHECK(BrDxVersionIsSufficient(BR_DXVER_NONE) == 0);
    CHECK(BrDxVersionIsSufficient(BR_DXVER_1)    == 0);
    CHECK(BrDxVersionIsSufficient(BR_DXVER_2)    == 0);
    CHECK(BrDxVersionIsSufficient(BR_DXVER_3)    == 0);
    CHECK(BrDxVersionIsSufficient(BR_DXVER_5)    == 0);
    CHECK(BrDxVersionIsSufficient(0x5FFu)        == 0);
    CHECK(BrDxVersionIsSufficient(BR_DXVER_6)    == 1);   /* the boundary */
    CHECK(BrDxVersionIsSufficient(0x601u)        == 1);
    CHECK(BrDxVersionIsSufficient(0x700u)        == 1);

    /* The compare is `jae`, not `jge`. Only a value with bit 31 set can tell
     * them apart, and only the NT 3.x path -- which returns without writing
     * the version at all -- can produce one. Transcribed as signed, this
     * would be 0. */
    CHECK(BrDxVersionIsSufficient(0x80000000u) == 1);
    CHECK(BrDxVersionIsSufficient(0xFFFFFFFFu) == 1);
}

/* ------------------------------------------------------------------ *
 * 16. The GUIDs, against the raw .rdata bytes.
 *
 * Checked byte-for-byte in the file's own little-endian order, so a field
 * transcribed with the wrong endianness fails here rather than at the QI.
 * ------------------------------------------------------------------ */
static void guidbytes(const BrDxGuid *g, unsigned char *out)
{
    int i;
    out[0] = (unsigned char)(g->d1 & 0xFF);
    out[1] = (unsigned char)((g->d1 >> 8) & 0xFF);
    out[2] = (unsigned char)((g->d1 >> 16) & 0xFF);
    out[3] = (unsigned char)((g->d1 >> 24) & 0xFF);
    out[4] = (unsigned char)(g->d2 & 0xFF);
    out[5] = (unsigned char)((g->d2 >> 8) & 0xFF);
    out[6] = (unsigned char)(g->d3 & 0xFF);
    out[7] = (unsigned char)((g->d3 >> 8) & 0xFF);
    for (i = 0; i < 8; i++)
        out[8 + i] = g->d4[i];
}

static void test_guids(void)
{
    /* orig/BRGlide.dll .rdata, read with tools/pe.py */
    static const unsigned char rd_dd2[16] = {           /* 0x10077CB8 */
        0xE0,0xF3,0xA6,0xB3, 0x43,0x2B, 0xCF,0x11,
        0xA2,0xDE,0x00,0xAA,0x00,0xB9,0x33,0x56 };
    static const unsigned char rd_s3[16] = {            /* 0x10077CF8 */
        0x00,0x4E,0x04,0xDA, 0xB2,0x69, 0xD0,0x11,
        0xA1,0xD5,0x00,0xAA,0x00,0xB8,0xDF,0xBB };
    static const unsigned char rd_s4[16] = {            /* 0x10077D08 */
        0x30,0x86,0x2B,0x0B, 0x35,0xAD, 0xD0,0x11,
        0x8E,0xA6,0x00,0x60,0x97,0x97,0xEA,0x5B };
    unsigned char b[16];

    guidbytes(&BrIidIDirectDraw2, b);
    CHECK(memcmp(b, rd_dd2, 16) == 0);
    guidbytes(&BrIidIDirectDrawSurface3, b);
    CHECK(memcmp(b, rd_s3, 16) == 0);
    guidbytes(&BrIidIDirectDrawSurface4, b);
    CHECK(memcmp(b, rd_s4, 16) == 0);

    /* The three must be mutually distinct, or "which IID was asked for" in
     * the log above would prove nothing. */
    CHECK(BrDxGuidEqual(&BrIidIDirectDraw2, &BrIidIDirectDrawSurface3) == 0);
    CHECK(BrDxGuidEqual(&BrIidIDirectDrawSurface3,
                        &BrIidIDirectDrawSurface4) == 0);
    CHECK(BrDxGuidEqual(&BrIidIDirectDraw2, &BrIidIDirectDraw2) == 1);
}

/* ------------------------------------------------------------------ *
 * 17. The literals handed to DirectDraw, as stored by the original.
 * ------------------------------------------------------------------ */
static void test_literals(void)
{
    CHECK(BR_DDSURFACEDESC_SIZE     == 0x6Cu);   /* 0x1001DB3E */
    CHECK(BR_DDSD_CAPS              == 0x1u);    /* 0x1001DB46 */
    CHECK(BR_DDSCAPS_PRIMARYSURFACE == 0x200u);  /* 0x1001DB4E */
    CHECK(BR_DDSCL_NORMAL           == 0x8u);    /* 0x1001DB2B */
    CHECK(BR_OSVERSIONINFOA_SIZE    == 0x94u);   /* 0x1001D8C8 */

    /* and they reach the calls, not just the header */
    script_ok_win9x();
    run();
    CHECK(hasev("SetCoopLevel(pDD,hwnd=NULL,flags=0x8)"));
    CHECK(hasev("CreateSurface(pDD,size=0x6c,flags=0x1,caps=0x200)"));
}

int main(void)
{
    test_getversion_failed();
    test_nt3_leaves_version_unwritten();
    test_nt4();
    test_full_success_win9x();
    test_platform_word();
    test_early_failures_zero_both();
    test_rungs();
    test_requirement_gate();
    test_guids();
    test_literals();

    if (g_fails != 0) {
        printf("%d FAILURE(S)\n", g_fails);
        return 1;
    }
    printf("br_dxver: all checks passed, 0 failures\n");
    return 0;
}
