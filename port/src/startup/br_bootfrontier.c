/* br_bootfrontier.c -- see br_bootfrontier.h.
 *
 * Every entry here does NOTHING and RECORDS that it was asked to. None of
 * them fabricates a result.
 */
#include "br_bootfrontier.h"

#include <stdio.h>

enum {
    F_10007F10, F_10063860, F_1006D1A0, F_10007F40, F_10063060, F_10009C00,
    F_10032530, F_1006C290, F_10058AF0,
    F_10063970, F_1006C990, F_100628B0,
    F_1006C460, F_10056260, F_1006E280, F_SETMODETAIL,
    F_COUNT
};

static const char *const s_apszName[F_COUNT] = {
    "0x10007F10 RallyMain init",
    "0x10063860 RallyMain init",
    "0x1006D1A0 RallyMain init",
    "0x10007F40 RallyMain cmdline",
    "0x10063060 config load",
    "0x10009C00 DirectPlay init (net)",
    "0x10032530 state0 init",
    "0x1006C290 sfx bank select",
    "0x10058AF0 state0 init",
    "0x10063970 state3 (5 args)",
    "0x1006C990 load image",
    "0x100628B0 state3 init",
    "0x1006C460 SOUND reset (not renderer)",
    "0x10056260 state4",
    "0x1006E280 state4 (PORTED: BrSub10075020)",
    "0x1001CE9D state4 config tail"
};

static int32_t s_aHits[F_COUNT];

static void (*s_pfn10007F10)(void);
static void (*s_pfn10007F40)(const char *);
static void (*s_pfn10063060)(void);
static void (*s_pfn10063860)(void);

void BrBootFrontierInstall(void (*pfn10007F10)(void),
                           void (*pfn10007F40)(const char *),
                           void (*pfn10063060)(void))
{
    s_pfn10007F10 = pfn10007F10;
    s_pfn10007F40 = pfn10007F40;
    s_pfn10063060 = pfn10063060;
}

/* 0x10063860 -- the install directory, now transcribed in br_basedir.c. Same
 * hook shape and the same reason: the frontier takes no link dependencies. */
void BrBootFrontierInstallBaseDir(void (*pfn10063860)(void))
{
    s_pfn10063860 = pfn10063860;
}
static void (*s_pfnFrameHook)(void);

void BrBootFrontierSetFrameHook(void (*pfn)(void)) { s_pfnFrameHook = pfn; }

int         BrBootFrontierCount(void)      { return F_COUNT; }
const char *BrBootFrontierName(int i)
{
    return (i >= 0 && i < F_COUNT) ? s_apszName[i] : "";
}
int32_t     BrBootFrontierHits(int i)
{
    return (i >= 0 && i < F_COUNT) ? s_aHits[i] : 0;
}

void BrBootFrontierReset(void)
{
    int i;
    s_pfnFrameHook = NULL;
    s_pfn10007F10 = NULL; s_pfn10007F40 = NULL; s_pfn10063060 = NULL;
    s_pfn10063860 = NULL;
    for (i = 0; i < F_COUNT; i++)
        s_aHits[i] = 0;
}

void BrBootFrontierReport(void)
{
    int i, n = 0;
    for (i = 0; i < F_COUNT; i++)
        if (s_aHits[i] != 0) n++;
    if (n == 0) {
        printf("boot frontier: nothing reached\n");
        return;
    }
    printf("boot frontier -- reached but NOT transcribed:\n");
    for (i = 0; i < F_COUNT; i++)
        if (s_aHits[i] != 0)
            printf("    %-34s %6d\n", s_apszName[i], (int)s_aHits[i]);
}

/* ---- the entries -------------------------------------------------- */

/* THE FRONTIER TAKES NO DEPENDENCIES, and that is a design constraint rather
 * than an accident.
 *
 * 0x10007F10 and 0x10007F40 are now transcribed in br_appstart.c, so the
 * obvious move is to call them from here. That was tried and reverted: it
 * couples the boot chain to br_appstart's whole closure -- slice1_01,
 * slice1_08, br_racestep, br_vec, and onward -- so every boot test would have
 * to link the config and race graph to assert a state transition. One of those
 * modules also defines a stand-in that collides with a real definition, which
 * is the documented reason this tree does not use an archive.
 *
 * Instead the frontier carries OPTIONAL HOOKS, NULL by default. A run with no
 * hook still counts the reach and does nothing, which is the honest edge; a
 * host that has the full graph linked installs the real functions and the same
 * counters then record real work. Nothing here fabricates a result either way.
 */
void BrBootFrontier_10007F10(void)
{
    ++s_aHits[F_10007F10];
    if (s_pfn10007F10 != NULL) s_pfn10007F10();
}
void BrBootFrontier_10063860(void)
{
    ++s_aHits[F_10063860];
    if (s_pfn10063860 != NULL) s_pfn10063860();
}
void BrBootFrontier_1006D1A0(void) { ++s_aHits[F_1006D1A0]; }
void BrBootFrontier_10063060(void)
{
    ++s_aHits[F_10063060];
    if (s_pfn10063060 != NULL) s_pfn10063060();
}
void BrBootFrontier_10009C00(void) { ++s_aHits[F_10009C00]; }

void BrBootFrontier_10007F40(const char *pszCmdLine)
{
    ++s_aHits[F_10007F40];
    if (s_pfn10007F40 != NULL) s_pfn10007F40(pszCmdLine);
}

/* 0x1001CCB5..0x1001CD0D. The two inlined string ops, recognised as strcpy
 * then strcat rather than transcribed as `repne scasb` / `rep movsd`. The
 * SOURCE at 0x10B73540 is a directory this port does not own yet, so the
 * result is the suffix alone until it does -- and that is visible in
 * BrBootConfigPath() rather than hidden. */
static char s_szConfigPath[512];

void BrBootBuildConfigPath(void)
{
    size_t n = 0;
    s_szConfigPath[0] = 0;
    while (n + 1 < sizeof s_szConfigPath && "BossRally.cfg"[n] != 0) {
        s_szConfigPath[n] = "BossRally.cfg"[n];
        n++;
    }
    s_szConfigPath[n] = 0;
}

const char *BrBootConfigPath(void) { return s_szConfigPath; }
void BrBootFrontier_10032530(void) { ++s_aHits[F_10032530]; }
void BrBootFrontier_10058AF0(void) { ++s_aHits[F_10058AF0]; }
void BrBootFrontier_100628B0(void) { ++s_aHits[F_100628B0]; }
void BrBootFrontier_1006C460(void) { ++s_aHits[F_1006C460]; }
void BrBootFrontier_10056260(void) { ++s_aHits[F_10056260]; }
void BrBootFrontier_SetModeTail(void) { ++s_aHits[F_SETMODETAIL]; }

void BrBootFrontier_1006C290(int32_t set)
{
    (void)set;
    ++s_aHits[F_1006C290];
}

void BrBootFrontier_10063970(int32_t a1, int32_t a2, int32_t a3,
                             int32_t a4, int32_t a5)
{
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    ++s_aHits[F_10063970];
}

void BrBootFrontier_1006C990(const char *pszImg, int32_t flags)
{
    (void)pszImg; (void)flags;
    ++s_aHits[F_1006C990];
}

/* Returns 0. NOT a fabricated handle: 0x1001CE98 stores this into 0x10AC6748,
 * and every consumer of that global treats zero as "absent". Returning
 * anything else would be inventing a device that does not exist. */
int32_t BrBootFrontier_1006E280(void)
{
    ++s_aHits[F_1006E280];
    return 0;
}

/* ---- globals owned elsewhere --------------------------------------- *
 * Each reports its LOAD-TIME value. The .data image is zero for all six,
 * verified rather than assumed -- these addresses sit in the zero-filled
 * region and no initialiser writes them.
 *
 * They are accessors rather than storage on purpose. Defining them here would
 * create a second definition competing with the real owner when that module
 * lands, which is precisely the failure slice7_81.c's thirteen globals exist
 * to warn about. When the owner arrives, re-point these six functions and
 * nothing else changes.
 * ------------------------------------------------------------------- */
int32_t BrBootGlobal_AC5C5C(void) { return 0; }
int32_t BrBootGlobal_ABAA0(void)  { return 0; }
int32_t BrBootGlobal_B71A48(void) { return 0; }
int32_t BrBootGlobal_B71A4C(void) { return 0; }
int32_t BrBootGlobal_B71A50(void) { return 0; }
int32_t BrBootGlobal_B71A54(void) { return 0; }

/* State 4 spreads the mode over six globals. The three width slots and the
 * three height slots are listed in br_boot.c; two of them (0x100A7514 /
 * 0x100A7518) are what CreateWindowExA reads, so this is the mode in the
 * operative sense and not merely a record of it. Storage lands here until the
 * owning module exists. */
static int32_t s_cxMode, s_cyMode, s_AC6748;

void BrBootSetModeGlobals(int32_t cx, int32_t cy) { s_cxMode = cx; s_cyMode = cy; }
void BrBootSetAC6748(int32_t v)                   { s_AC6748 = v; }

int32_t BrBootModeW(void)   { return s_cxMode; }
int32_t BrBootModeH(void)   { return s_cyMode; }
int32_t BrBootAC6748(void)  { return s_AC6748; }
