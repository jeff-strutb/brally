/* test_br_appstart.c -- application start-up: admission, machine probe,
 * settings.
 *
 * WHAT THIS ASSERTS AND HOW EACH CHECK CAN FAIL
 *
 * Three functions, and each has exactly one way to be wrong that a compiler
 * will never catch:
 *
 *  - 0x10007E80 is a GATE.  Return the wrong polarity and RallyMain either
 *    never starts or starts twice.  The checks below pin the polarity AND the
 *    four-way branch inside it, including the fact that the activation
 *    sequence acts on GetLastActivePopup's result rather than on the window
 *    FindWindowA returned.
 *  - 0x10007F10 reads ONE dword out of a MEMORYSTATUS.  The fixture fills all
 *    eight with distinct values, so reading the neighbouring field -- which is
 *    exactly what an ESP mis-trace produces -- fails instead of passing with a
 *    plausible number.
 *  - 0x10007F40 has nineteen value offsets, and every one of them is an
 *    `[esp+N]` whose meaning depends on where esp is.  Each key below is fed a
 *    DISTINCT value, so an offset that is off by even one character parses a
 *    different number and the check fails.  Feeding every key the same 1 would
 *    make this suite unable to fail, which is the defect this project has
 *    found in its own tests twice.
 *
 * The .ini/command-line asymmetry gets its own checks, because it is real
 * behaviour and easy to "tidy" away: the .ini half matches only at the START
 * of a line, the command-line half matches ANYWHERE; the .ini half is an
 * else-if ladder, the command-line half is not; and nine keys exist only in
 * one half or the other.
 *
 * BOUNDED.  main() arms alarm(20).  BrAppCfgParseIni drives a loop whose
 * termination depends on BrChkFReadLine consuming input, and a mutation that
 * stops it consuming would otherwise hang the suite instead of failing it --
 * which is worse than red.
 */
#include "br_appstart.h"
#include "br_tmpfile.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int g_fails;

#define CHECK(c) do { if (!(c)) { \
    printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); g_fails++; } } while (0)

/* ------------------------------------------------------------------ *
 * Stand-ins for the two globals this module writes but does not own.
 * port/src/br_racestep.c and port/src/slice1_08.c define the real ones, and
 * the host build links those; a test that pulled them in would drag nine
 * further modules for two words.  build.sh documents this pattern.
 * ------------------------------------------------------------------ */
int32_t g_brRaceNet;        /* 0x10226A48 */
int32_t BrSndG0B5DE8 = 1;   /* 0x100B55F0 */

/* ==================================================================== *
 * 0x10007E80 -- the gate
 * ==================================================================== */

/* A fake window manager.  Handles are the addresses of these ints. */
static int g_hFound, g_hPopup, g_hForeground;

static struct {
    int      cFindWindow;
    int      cShowWindow;
    int      cBringToTop;
    int      cSetForeground;
    int      cDebugString;
    void    *hShowWindow;
    void    *hBringToTop;
    void    *hSetForeground;
    int32_t  nCmdShow;
    char     szDebug[256];
    /* inputs */
    void    *hFindResult;
    uint32_t idOfFound;
    uint32_t idOfForeground;
    int32_t  fIconicFound;
    int32_t  fIconicPopup;
    const char *pszClassSeen;
    const char *pszTitleSeen;
} g_wm;

static void *wm_find(const char *pszClass, const char *pszTitle)
{
    g_wm.cFindWindow++;
    g_wm.pszClassSeen = pszClass;
    g_wm.pszTitleSeen = pszTitle;
    return g_wm.hFindResult;
}
static void *wm_foreground(void)          { return &g_hForeground; }
static uint32_t wm_threadid(void *hWnd, uint32_t *pPid)
{
    /* The original passes NULL here at both call sites; if a transcription
     * started passing a real pointer this would fail. */
    CHECK(pPid == NULL);
    if (hWnd == &g_hForeground) return g_wm.idOfForeground;
    return g_wm.idOfFound;
}
static int32_t wm_iconic(void *hWnd)
{
    return (hWnd == &g_hPopup) ? g_wm.fIconicPopup : g_wm.fIconicFound;
}
static void *wm_lastpopup(void *hWnd)     { (void)hWnd; return &g_hPopup; }
static void  wm_show(void *hWnd, int32_t n)
{ g_wm.cShowWindow++; g_wm.hShowWindow = hWnd; g_wm.nCmdShow = n; }
static void  wm_bringtop(void *hWnd)      { g_wm.cBringToTop++;    g_wm.hBringToTop = hWnd; }
static void  wm_setfg(void *hWnd)         { g_wm.cSetForeground++; g_wm.hSetForeground = hWnd; }
static void  wm_debug(const char *psz)
{
    g_wm.cDebugString++;
    snprintf(g_wm.szDebug, sizeof g_wm.szDebug, "%s", psz);
}

static uint32_t g_aMemStatus[8];
static void wm_memstatus(uint32_t aStatus[8])
{
    int i;
    /* dwLength must already be 0x20 -- the caller sets it before the call. */
    CHECK(aStatus[0] == 0x20);
    for (i = 0; i < 8; i++) {
        aStatus[i] = g_aMemStatus[i];
    }
}

static const BrAppStartHost g_hostFake = {
    wm_find, wm_foreground, wm_threadid, wm_iconic, wm_lastpopup,
    wm_show, wm_bringtop, wm_setfg, wm_debug, wm_memstatus
};

static void wm_reset(void)
{
    memset(&g_wm, 0, sizeof g_wm);
    g_wm.idOfFound = 111u;
    g_wm.idOfForeground = 111u;
    BrAppStartSetHost(&g_hostFake);
}

static void test_gate(void)
{
    /* --- no previous instance: the game may start ------------------ */
    wm_reset();
    g_wm.hFindResult = NULL;
    CHECK(BrAppCheckPreviousApp() == 1);
    CHECK(g_wm.cFindWindow == 1);
    CHECK(strcmp(g_wm.pszClassSeen, "BossRally") == 0);
    CHECK(strcmp(g_wm.pszTitleSeen, "Boss Rally") == 0);
    /* nothing else runs on this arm */
    CHECK(g_wm.cDebugString == 0);
    CHECK(g_wm.cSetForeground == 0);

    /* --- found, and it belongs to another thread: raise it --------- */
    wm_reset();
    g_wm.hFindResult    = &g_hFound;
    g_wm.idOfFound      = 222u;
    g_wm.idOfForeground = 111u;
    g_wm.fIconicFound   = 0;
    g_wm.fIconicPopup   = 0;
    CHECK(BrAppCheckPreviousApp() == 0);
    CHECK(g_wm.cBringToTop == 1);
    CHECK(g_wm.cSetForeground == 1);
    /* the popup is NOT iconic, so no ShowWindow */
    CHECK(g_wm.cShowWindow == 0);
    /* and everything acts on the POPUP, not on what FindWindowA returned */
    CHECK(g_wm.hBringToTop    == (void *)&g_hPopup);
    CHECK(g_wm.hSetForeground == (void *)&g_hPopup);
    CHECK(g_wm.cDebugString == 1);
    CHECK(strcmp(g_wm.szDebug, BR_APP_PREVAPP_MSG) == 0);

    /* --- found, same thread, NOT minimised: do NOT steal focus ----- */
    wm_reset();
    g_wm.hFindResult    = &g_hFound;
    g_wm.idOfFound      = 111u;
    g_wm.idOfForeground = 111u;
    g_wm.fIconicFound   = 0;
    CHECK(BrAppCheckPreviousApp() == 0);
    CHECK(g_wm.cBringToTop == 0);
    CHECK(g_wm.cSetForeground == 0);
    CHECK(g_wm.cShowWindow == 0);
    /* the message is printed on BOTH arms -- 0x10007EE9 is the join */
    CHECK(g_wm.cDebugString == 1);

    /* --- found, same thread, minimised: restore it ----------------- */
    wm_reset();
    g_wm.hFindResult    = &g_hFound;
    g_wm.idOfFound      = 111u;
    g_wm.idOfForeground = 111u;
    g_wm.fIconicFound   = 1;
    g_wm.fIconicPopup   = 1;
    CHECK(BrAppCheckPreviousApp() == 0);
    CHECK(g_wm.cShowWindow == 1);
    CHECK(g_wm.hShowWindow == (void *)&g_hPopup);
    CHECK(g_wm.nCmdShow == BR_APP_SW_RESTORE);   /* SW_RESTORE, the literal 9 */
    CHECK(g_wm.cBringToTop == 1);
    CHECK(g_wm.cSetForeground == 1);

    /* --- the two IsIconic calls ask about DIFFERENT windows --------
     * The found window is minimised (so the restore arm is taken) but the
     * popup is not (so ShowWindow must be skipped).  Read both from the same
     * handle and this pair cannot both hold. */
    wm_reset();
    g_wm.hFindResult    = &g_hFound;
    g_wm.idOfFound      = 111u;
    g_wm.idOfForeground = 111u;
    g_wm.fIconicFound   = 1;
    g_wm.fIconicPopup   = 0;
    CHECK(BrAppCheckPreviousApp() == 0);
    CHECK(g_wm.cShowWindow == 0);
    CHECK(g_wm.cBringToTop == 1);

    /* --- the default host lets the game start ---------------------- */
    BrAppStartSetHost(NULL);
    CHECK(BrAppCheckPreviousApp() == 1);
    BrAppStartSetHost(&g_hostFake);
}

/* ==================================================================== *
 * 0x10007F10 -- the machine probe
 * ==================================================================== */
static void test_total_phys(void)
{
    int i;

    wm_reset();
    /* Eight DISTINCT values.  dwMemoryLoad (index 1) is the field an ESP
     * mis-trace lands on, and it is deliberately a value that would look
     * perfectly reasonable if it were wrong. */
    for (i = 0; i < 8; i++) {
        g_aMemStatus[i] = 0xA0000000u + (uint32_t)i;
    }
    g_brAppTotalPhysBytes = 0;
    BrAppQueryTotalPhys();
    CHECK(g_brAppTotalPhysBytes == 0xA0000002u);   /* dwTotalPhys */
    CHECK(g_brAppTotalPhysBytes != 0xA0000001u);   /* NOT dwMemoryLoad */
    CHECK(g_brAppTotalPhysBytes != 0xA0000003u);   /* NOT dwAvailPhys  */

    /* The default host has no memory figures and must not invent one. */
    BrAppStartSetHost(NULL);
    g_brAppTotalPhysBytes = 0x1234u;
    BrAppQueryTotalPhys();
    CHECK(g_brAppTotalPhysBytes == 0u);
    BrAppStartSetHost(&g_hostFake);
}

/* ==================================================================== *
 * 0x10003530 -- CHK_FReadLine
 * ==================================================================== */
static FILE *write_tmp(int slot, const char *pszStem, const void *pBytes,
                       size_t cb, const char **ppszPath)
{
    const char *pszPath = BrTmpPath(slot, pszStem);
    FILE *pf = fopen(pszPath, "wb");
    if (pf == NULL) {
        return NULL;
    }
    fwrite(pBytes, 1, cb, pf);
    fclose(pf);
    if (ppszPath != NULL) {
        *ppszPath = pszPath;
    }
    return fopen(pszPath, "rb");
}

static void test_readline(void)
{
    char  aBuf[BR_APPCFG_LINE_ALLOC];
    FILE *pf;
    char *p;
    int   guard;

    /* LF endings: the '\n' is KEPT in the buffer.  Everything the three
     * directory keys do depends on this. */
    pf = write_tmp(0, "/tmp/br_appstart_lf", "ab\ncd\n", 6, NULL);
    CHECK(pf != NULL);
    if (pf != NULL) {
        memset(aBuf, '#', sizeof aBuf);
        p = BrChkFReadLine(aBuf, BR_APPCFG_LINE, pf);
        CHECK(p != NULL);
        CHECK(strcmp(aBuf, "ab\n") == 0);
        memset(aBuf, '#', sizeof aBuf);
        p = BrChkFReadLine(aBuf, BR_APPCFG_LINE, pf);
        CHECK(p != NULL);
        CHECK(strcmp(aBuf, "cd\n") == 0);
        /* EOF with nothing read is the loop's exit, and it is the ONLY
         * NULL return. */
        CHECK(BrChkFReadLine(aBuf, BR_APPCFG_LINE, pf) == NULL);
        fclose(pf);
    }

    /* CRLF: the CR becomes '\n' and the LF is swallowed, so two lines, not
     * four. */
    pf = write_tmp(1, "/tmp/br_appstart_crlf", "ab\r\ncd\r\n", 8, NULL);
    CHECK(pf != NULL);
    if (pf != NULL) {
        guard = 0;
        memset(aBuf, '#', sizeof aBuf);
        CHECK(BrChkFReadLine(aBuf, BR_APPCFG_LINE, pf) != NULL);
        CHECK(strcmp(aBuf, "ab\n") == 0);
        memset(aBuf, '#', sizeof aBuf);
        CHECK(BrChkFReadLine(aBuf, BR_APPCFG_LINE, pf) != NULL);
        CHECK(strcmp(aBuf, "cd\n") == 0);
        while (BrChkFReadLine(aBuf, BR_APPCFG_LINE, pf) != NULL && guard < 8) {
            guard++;
        }
        CHECK(guard == 0);   /* no spurious empty lines from the swallowed LF */
        fclose(pf);
    }

    /* Bare CR followed by an ordinary character: the character is pushed back
     * with ungetc and starts the next line. */
    pf = write_tmp(2, "/tmp/br_appstart_cr", "ab\rcd\n", 6, NULL);
    CHECK(pf != NULL);
    if (pf != NULL) {
        memset(aBuf, '#', sizeof aBuf);
        CHECK(BrChkFReadLine(aBuf, BR_APPCFG_LINE, pf) != NULL);
        CHECK(strcmp(aBuf, "ab\n") == 0);
        memset(aBuf, '#', sizeof aBuf);
        CHECK(BrChkFReadLine(aBuf, BR_APPCFG_LINE, pf) != NULL);
        CHECK(strcmp(aBuf, "cd\n") == 0);   /* the 'c' was NOT eaten */
        fclose(pf);
    }

    /* A final line with no newline: terminated, non-NULL, and the NEXT call
     * is the NULL. */
    pf = write_tmp(3, "/tmp/br_appstart_noeol", "xy", 2, NULL);
    CHECK(pf != NULL);
    if (pf != NULL) {
        memset(aBuf, '#', sizeof aBuf);
        CHECK(BrChkFReadLine(aBuf, BR_APPCFG_LINE, pf) != NULL);
        CHECK(strcmp(aBuf, "xy") == 0);
        CHECK(BrChkFReadLine(aBuf, BR_APPCFG_LINE, pf) == NULL);
        fclose(pf);
    }

    /* cbMax <= 0 returns the destination untouched and reads nothing. */
    pf = write_tmp(0, "/tmp/br_appstart_lf", "ab\n", 3, NULL);
    CHECK(pf != NULL);
    if (pf != NULL) {
        memset(aBuf, '#', sizeof aBuf);
        CHECK(BrChkFReadLine(aBuf, 0, pf) == aBuf);
        CHECK(aBuf[0] == '#');
        fclose(pf);
    }
}

/* ==================================================================== *
 * 0x10007F40 -- the settings loader
 * ==================================================================== */

/* Point the loader at a scratch .ini.  The base directory is a PREFIX, not a
 * directory with a separator -- the original just concatenates -- so this also
 * exercises BrAppCfgBuildIniPath for real. */
static const char *cfg_write_ini(const char *pszText)
{
    static char aPath[512];
    const char *pszPrefix = BrTmpPath(0, "/tmp/br_appstart_cfg");
    FILE *pf;

    BrAppCfgResetForTest();
    snprintf(g_aBrCfgBaseDir, sizeof g_aBrCfgBaseDir, "%s.", pszPrefix);
    snprintf(aPath, sizeof aPath, "%s.%s", pszPrefix, BR_APPCFG_INI_NAME);

    pf = fopen(aPath, "wb");
    if (pf != NULL) {
        fputs(pszText, pf);
        fclose(pf);
    }
    return aPath;
}

static void test_ini_path(void)
{
    BrAppCfgResetForTest();
    /* Empty base directory: the shipped default, since 0x10063860's registry
     * read has no host meaning.  The path is the bare relative name. */
    BrAppCfgBuildIniPath();
    CHECK(strcmp(g_aBrCfgIniPath, "BossRally.ini") == 0);

    BrAppCfgResetForTest();
    snprintf(g_aBrCfgBaseDir, sizeof g_aBrCfgBaseDir, "C:\\Games\\Rally\\");
    BrAppCfgBuildIniPath();
    CHECK(strcmp(g_aBrCfgIniPath, "C:\\Games\\Rally\\BossRally.ini") == 0);
}

/* Every key gets a DIFFERENT value, so an offset that is off by one character
 * yields a different number rather than the same 1. */
static void test_ini_every_key(void)
{
    const char *pszPath = cfg_write_ini(
        "NetworkPlay=11\n"
        "chosenTrack=12\n"
        "chosenCar=13\n"
        "chosenWeather=14\n"
        "gameMode=15\n"
        "ReadJoystick=2\n"
        "HandlingType=17\n"
        "SuspensionType=18\n"
        "TireType=19\n"
        "TransmissionType=20\n"
        "TrackDir=mytracks/\n"
        "CarDir=mycars/\n"
        "SFXDir=mysfx/\n"
        "Interpolate=24\n"
        "SpeedSensitive=25\n"
        "D3DDrawCarShadow=1\n"
        "RunBenchmark=27\n"
        "PlayMusic=28\n"
        "PlaySFX=29\n");

    BrAppCfgParseIni();

    CHECK(g_brRaceNet            == 11);
    CHECK(g_brCfgChosenTrack     == 12);
    CHECK(g_brCfgChosenCar       == 13);
    CHECK(g_brCfgChosenWeather   == 14);
    CHECK(g_brCfgGameMode        == 15);
    CHECK(g_brCfgReadJoystick    == 2);
    CHECK(g_brCfgHandlingType    == 17);
    CHECK(g_brCfgSuspensionType  == 18);
    CHECK(g_brCfgTireType        == 19);
    CHECK(g_brCfgTransmission    == 20);
    CHECK(g_brCfgInterpolate     == 24);
    CHECK(g_brCfgSpeedSensitive  == 25);
    CHECK(g_brCfgRunBenchmark    == 27);
    CHECK(g_brCfgPlayMusic       == 28);
    CHECK(BrSndG0B5DE8           == 29);

    /* The directory keys strip the byte BrChkFReadLine left -- the '\n'. */
    CHECK(strcmp(g_aBrCfgTrackDir, "mytracks/") == 0);
    CHECK(strcmp(g_aBrCfgCarDir,   "mycars/")   == 0);
    CHECK(strcmp(g_aBrCfgSfxDir,   "mysfx/")    == 0);

    /* D3DDrawCarShadow= is INVERTED: the file says 1 and the global is 0. */
    CHECK(g_brCfgD3DCarShadow == 0);

    /* ReadJoystick=2 selects device 2. */
    CHECK(g_iBrCfgInputDevice == 2);

    remove(pszPath);
}

static void test_ini_shadow_inverted(void)
{
    const char *pszPath;

    pszPath = cfg_write_ini("D3DDrawCarShadow=0\n");
    BrAppCfgParseIni();
    CHECK(g_brCfgD3DCarShadow == 1);        /* 0 in the file -> 1 in the word */
    remove(pszPath);

    pszPath = cfg_write_ini("D3DDrawCarShadow=7\n");
    BrAppCfgParseIni();
    CHECK(g_brCfgD3DCarShadow == 0);        /* anything non-zero -> 0 */
    remove(pszPath);
}

static void test_ini_defaults_survive_absent_file(void)
{
    BrAppCfgResetForTest();
    snprintf(g_aBrCfgBaseDir, sizeof g_aBrCfgBaseDir,
             "%s.absent.", BrTmpPath(1, "/tmp/br_appstart_none"));
    BrAppCfgParseIni();

    /* The path is still built -- that happens before CHK_FileExists. */
    CHECK(strstr(g_aBrCfgIniPath, BR_APPCFG_INI_NAME) != NULL);
    /* and the .data load-time values are untouched */
    CHECK(g_brCfgChosenTrack    == 2);
    CHECK(g_brCfgGameMode       == 1);
    CHECK(g_brCfgTireType       == 2);
    CHECK(g_brCfgPlayMusic      == 2);
    CHECK(g_brCfgHandlingType   == 1);
    CHECK(strcmp(g_aBrCfgTrackDir, "tracks/") == 0);
    CHECK(strcmp(g_aBrCfgCarDir,   "cars/")   == 0);
    CHECK(strcmp(g_aBrCfgSfxDir,   "sfx/")    == 0);
}

/* The .ini half matches with strncmp against the START of the line. */
static void test_ini_matches_only_at_line_start(void)
{
    const char *pszPath = cfg_write_ini(
        " gameMode=15\n"          /* leading space -- no match */
        "xgameMode=16\n"          /* prefixed      -- no match */
        "# chosenCar=17\n");      /* commented     -- no match */
    BrAppCfgParseIni();
    CHECK(g_brCfgGameMode  == 1);   /* the load-time value */
    CHECK(g_brCfgChosenCar == 0);
    remove(pszPath);
}

/* Five keys exist ONLY on the command line.  Putting them in the .ini must do
 * nothing at all. */
static void test_ini_ignores_cmdline_only_keys(void)
{
    const char *pszPath = cfg_write_ini(
        "cPlayers=4\n"
        "bcar=5\n"
        "btire=6\n"
        "bsuspension=7\n"
        "szPlayerName=Nigel\n");
    BrAppCfgParseIni();
    CHECK(g_brCfgPlayers         == 0);
    CHECK(g_brCfgBenchCar        == 0);
    CHECK(g_brCfgBenchTire       == 0);
    CHECK(g_brCfgBenchSuspension == 0);
    CHECK(g_aBrCfgPlayerName[0]  == '\0');
    remove(pszPath);
}

static void test_cmdline_every_key(void)
{
    BrAppCfgResetForTest();
    /* One string, fifteen keys, all distinct values: the command-line half has
     * no else, so every one must land. */
    BrAppCfgParseCmdLine(
        "-x NetworkPlay=31 chosenTrack=32 chosenCar=33 chosenWeather=34 "
        "gameMode=35 ReadJoystick=3 HandlingType=37 SuspensionType=38 "
        "TireType=39 TransmissionType=40 cPlayers=41 bcar=42 btire=43 "
        "bsuspension=44 szPlayerName=Nigel more");

    CHECK(g_brRaceNet            == 31);
    CHECK(g_brCfgChosenTrack     == 32);
    CHECK(g_brCfgChosenCar       == 33);
    CHECK(g_brCfgChosenWeather   == 34);
    CHECK(g_brCfgGameMode        == 35);
    CHECK(g_brCfgReadJoystick    == 3);
    CHECK(g_iBrCfgInputDevice    == 3);
    CHECK(g_brCfgHandlingType    == 37);
    CHECK(g_brCfgSuspensionType  == 38);
    CHECK(g_brCfgTireType        == 39);
    CHECK(g_brCfgTransmission    == 40);
    CHECK(g_brCfgPlayers         == 41);
    CHECK(g_brCfgBenchCar        == 42);
    CHECK(g_brCfgBenchTire       == 43);
    CHECK(g_brCfgBenchSuspension == 44);
    /* truncated at the first space */
    CHECK(strcmp(g_aBrCfgPlayerName, "Nigel") == 0);
}

/* Nine keys exist ONLY in the .ini.  On the command line they must do
 * nothing -- this is not a tidy-up, it is what the disassembly says. */
static void test_cmdline_ignores_ini_only_keys(void)
{
    BrAppCfgResetForTest();
    BrAppCfgParseCmdLine(
        "TrackDir=zz/ CarDir=yy/ SFXDir=xx/ Interpolate=9 SpeedSensitive=9 "
        "D3DDrawCarShadow=1 RunBenchmark=9 PlayMusic=9 PlaySFX=9");
    CHECK(strcmp(g_aBrCfgTrackDir, "tracks/") == 0);
    CHECK(strcmp(g_aBrCfgCarDir,   "cars/")   == 0);
    CHECK(strcmp(g_aBrCfgSfxDir,   "sfx/")    == 0);
    CHECK(g_brCfgInterpolate    == 1);
    CHECK(g_brCfgSpeedSensitive == 1);
    CHECK(g_brCfgD3DCarShadow   == 0);
    CHECK(g_brCfgRunBenchmark   == 0);
    CHECK(g_brCfgPlayMusic      == 2);
    CHECK(BrSndG0B5DE8          == 1);
}

static void test_cmdline_player_name(void)
{
    BrAppCfgResetForTest();
    BrAppCfgParseCmdLine("szPlayerName=Jo\nrest");
    CHECK(strcmp(g_aBrCfgPlayerName, "Jo") == 0);   /* cut at the newline */

    BrAppCfgResetForTest();
    BrAppCfgParseCmdLine("szPlayerName=Solo");
    CHECK(strcmp(g_aBrCfgPlayerName, "Solo") == 0); /* no delimiter at all */
}

static void test_cmdline_empty_and_null(void)
{
    /* The NULL guard IS load-bearing: remove it and strstr(NULL, ...) takes
     * the process down.  Mutation M35 confirms it (rc=139). */
    BrAppCfgResetForTest();
    BrAppCfgParseCmdLine(NULL);
    CHECK(g_brCfgPlayers == 0);
    CHECK(g_brCfgGameMode == 1);

    /* THE EMPTY-STRING EARLY-OUT AT 0x1000840E IS REDUNDANT, and this block
     * cannot fail because of it -- it is here to DOCUMENT the behaviour, not
     * to guard it, and saying so is the point.  Mutation M21 deleted the
     * guard and the suite stayed green, because strstr("", key) is NULL for
     * all fifteen keys, so the loop below it does nothing either way.  The
     * original computes strlen with `repne scasb`/`not`/`dec` and branches on
     * zero purely to skip work.  Anyone strengthening this suite should not
     * go looking for an assertion that pins it; there is not one to write. */
    BrAppCfgResetForTest();
    BrAppCfgParseCmdLine("");
    CHECK(g_brCfgPlayers == 0);
    CHECK(g_brCfgGameMode == 1);
}

/* The command line is parsed SECOND, so for the ten shared keys it wins. */
static void test_cmdline_beats_ini(void)
{
    const char *pszPath = cfg_write_ini(
        "gameMode=15\n"
        "chosenTrack=12\n"
        "PlayMusic=28\n");

    BrAppCfgParse("gameMode=99");

    CHECK(g_brCfgGameMode    == 99);   /* command line wins */
    CHECK(g_brCfgChosenTrack == 12);   /* .ini still applies where unopposed */
    CHECK(g_brCfgPlayMusic   == 28);   /* an .ini-only key is untouched */
    remove(pszPath);
}

static void test_input_device_mapping(void)
{
    static const struct { int32_t nJoy; int32_t iDev; } aCase[] = {
        { 0, 0 }, { 1, 1 }, { 2, 2 }, { 3, 3 },
        { 4, 0 }, { 99, 0 }, { -1, 0 }
    };
    char aCmd[64];
    size_t i;

    for (i = 0; i < sizeof aCase / sizeof aCase[0]; i++) {
        BrAppCfgResetForTest();
        snprintf(aCmd, sizeof aCmd, "ReadJoystick=%d", (int)aCase[i].nJoy);
        BrAppCfgParseCmdLine(aCmd);
        CHECK(g_brCfgReadJoystick == aCase[i].nJoy);
        CHECK(g_iBrCfgInputDevice == aCase[i].iDev);
    }

    /* The four device objects are consecutive at stride 0xA8; these are the
     * addresses the original stores into 0x10B71534. */
    CHECK(BrCfgInputDeviceAddr(0) == 0x10B71290u);
    CHECK(BrCfgInputDeviceAddr(1) == 0x10B71338u);
    CHECK(BrCfgInputDeviceAddr(2) == 0x10B713E0u);
    CHECK(BrCfgInputDeviceAddr(3) == 0x10B71488u);
}

int main(void)
{
    /* BOUNDED: see the banner.  A mutation that stops BrChkFReadLine
     * consuming input would hang BrAppCfgParseIni; this turns that into a
     * non-zero exit rather than a stuck suite. */
    alarm(20);

    test_gate();
    test_total_phys();
    test_readline();
    test_ini_path();
    test_ini_every_key();
    test_ini_shadow_inverted();
    test_ini_defaults_survive_absent_file();
    test_ini_matches_only_at_line_start();
    test_ini_ignores_cmdline_only_keys();
    test_cmdline_every_key();
    test_cmdline_ignores_ini_only_keys();
    test_cmdline_player_name();
    test_cmdline_empty_and_null();
    test_cmdline_beats_ini();
    test_input_device_mapping();

    if (g_fails != 0) {
        printf("%d FAILURE(S)\n", g_fails);
        return 1;
    }
    printf("br_appstart: all checks passed\n");
    return 0;
}
