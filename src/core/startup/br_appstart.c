/* br_appstart.c -- APPLICATION START-UP: admission, machine probe, settings.
 *
 * ARCHITECTURAL CONCERN: everything RallyMain does before it has a window.
 * See br_appstart.h for the key table, the two aliases, and the ESP trace that
 * decides what 0x10007F10 reads.
 *
 * Reference build: orig/BRGlide.dll.  Every address in this file is a Glide
 * address.  config/shared.csv classes all three entry points `shared`:
 * 0x10007E80 <- D3D 0x10007B10, 0x10007F10 <- D3D 0x10007BA0,
 * 0x10007F40 <- D3D 0x10007BD0.
 */

#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include "br_appstart.h"

#include <stdlib.h>
#include <string.h>

/* XSLICE 0x10003680 (D3D 0x10003320) -- CHK_FileExists, already ported in
 * port/src/slice1_01.c under its D3D address.  config/shared.csv row 54 pairs
 * the two.  Reused rather than re-coined; see CONVENTIONS.md. */
extern int BrChkFileExists(const char *pPath);

/* The two globals this module writes that another module OWNS.  Declared, not
 * defined: the original has one object per address and so must this port. */
extern int32_t g_brRaceNet;     /* 0x10226A48 -- port/src/br_racestep.c */
extern int32_t BrSndG0B5DE8;    /* 0x100B55F0 -- port/src/slice1_08.c   */

/* ==========================================================================
 * Storage.  Initialisers are BRGlide.dll's own .data bytes, read out of the
 * image; anything not listed here is .bss and starts at zero.
 * ========================================================================== */
uint32_t g_brAppTotalPhysBytes;                     /* 0x10226E78 */

char g_aBrCfgBaseDir  [BR_APPCFG_BASEDIR_MAX];      /* 0x10B73540 */
char g_aBrCfgIniPath  [BR_APPCFG_PATH_MAX];         /* 0x10226A78 */
char g_aBrCfgTrackDir [BR_APPCFG_DIR_MAX] = "tracks/";  /* 0x100B74C0 */
char g_aBrCfgCarDir   [BR_APPCFG_DIR_MAX] = "cars/";    /* 0x100B7900 */
char g_aBrCfgSfxDir   [BR_APPCFG_DIR_MAX] = "sfx/";     /* 0x100B7D40 */
char g_aBrCfgPlayerName[BR_APPCFG_NAME_MAX];        /* 0x10B71648 */

int32_t g_brCfgChosenTrack     = 2;   /* 0x100B3014 */
int32_t g_brCfgChosenCar       = 0;   /* 0x10226E7C */
int32_t g_brCfgChosenWeather   = 0;   /* 0x10226E80 */
int32_t g_brCfgGameMode        = 1;   /* 0x100A9360 */
int32_t g_brCfgReadJoystick    = 0;   /* 0x10B71530 */
int32_t g_brCfgHandlingType    = 1;   /* 0x1007B320 */
int32_t g_brCfgSuspensionType  = 1;   /* 0x1007B328 */
int32_t g_brCfgTireType        = 2;   /* 0x1007B32C */
int32_t g_brCfgTransmission    = 1;   /* 0x1007B324 */
int32_t g_brCfgInterpolate     = 1;   /* 0x100A5EAC */
int32_t g_brCfgSpeedSensitive  = 1;   /* 0x100B2E6C */
int32_t g_brCfgD3DCarShadow    = 0;   /* 0x10396EB0 */
int32_t g_brCfgRunBenchmark    = 0;   /* 0x118EEEDC */
int32_t g_brCfgPlayMusic       = 2;   /* 0x1007B074 */
int32_t g_brCfgPlayers         = 0;   /* 0x1021CDF8 */
int32_t g_brCfgBenchCar        = 0;   /* 0x1021CE50 */
int32_t g_brCfgBenchTire       = 0;   /* 0x10226A40 */
int32_t g_brCfgBenchSuspension = 0;   /* 0x10226A3C */

int32_t g_iBrCfgInputDevice    = 0;   /* 0x10B71534, as an index */

/* ==========================================================================
 * The platform hook table.
 * ========================================================================== */

/* The default primitives.  NOT stand-ins that make something happen: this
 * host has no Win32 window manager and never registers the class
 * "BossRally", so FindWindowA's answer really is NULL here, which is the arm
 * that lets the game start.  GlobalMemoryStatus likewise has no host
 * equivalent, so it leaves the caller's zeroed image alone and
 * g_brAppTotalPhysBytes stays 0 -- which is what a run should report, not a
 * plausible memory size. */
static void *host_find_window_none(const char *pszClass, const char *pszTitle)
{
    (void)pszClass;
    (void)pszTitle;
    return NULL;
}

static void host_memstatus_none(uint32_t aStatus[8])
{
    (void)aStatus;
}

static const BrAppStartHost g_brAppStartHostDefault = {
    host_find_window_none,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    host_memstatus_none
};

static const BrAppStartHost *g_pBrAppStartHost = &g_brAppStartHostDefault;

void BrAppStartSetHost(const BrAppStartHost *pHost)
{
    g_pBrAppStartHost = (pHost != NULL) ? pHost : &g_brAppStartHostDefault;
}

/* ==========================================================================
 * 0x10007E80 -- CheckPreviousApp.  THE GATE: RallyMain aborts on 0.
 *
 * Register trace, because `esi` changes meaning halfway through and that is
 * the one thing a reader can get wrong here:
 *
 *   0x10007E91  esi = FindWindowA(...)          the OTHER instance's window
 *   0x10007EAD  ebx = thread of the FOREGROUND window
 *   0x10007EAF  eax = thread of esi
 *   0x10007EC9  esi = GetLastActivePopup(esi)   REBOUND -- everything after
 *                                               this acts on the popup
 *
 * Both GetWindowThreadProcessId calls pass NULL as the process-id out
 * parameter (`push 0` at 0x10007EA5 and 0x10007EAA), so no process id is ever
 * fetched; the comparison at 0x10007EB7 is between THREAD ids.
 *
 * The branch shape, exactly as written:
 *
 *   0x10007EB9  jne  -> different thread: restore, unconditionally
 *   0x10007EC0  je   -> same thread and NOT iconic: skip the restore
 *               otherwise (same thread, iconic): fall into the restore
 *
 * which is `if (idPopupOwner != idForeground || IsIconic(hWnd))`.  IsIconic is
 * only reached when the threads match, so || short-circuiting is not an
 * approximation of the original, it is the original.
 * ========================================================================== */
/* WHAT IT DOES: checks whether Boss Rally is already running, and if it is,
 * brings that copy's window to the front instead of starting a second one.
 * Whether it restores the window depends on whether the other copy owns the
 * foreground and whether it is minimised. */
#ifndef BR_MATCHING_BUILD
int32_t BrAppCheckPreviousApp(void)
{
    const BrAppStartHost *pH = g_pBrAppStartHost;
    void    *hWnd;
    uint32_t idForeground;
    uint32_t idOther;

    hWnd = (pH->pfnFindWindow != NULL)
         ? pH->pfnFindWindow(BR_APP_WNDCLASS, BR_APP_WNDTITLE)
         : NULL;

    if (hWnd == NULL) {
        return 1;                       /* 0x10007EFA: mov eax,1 / ret */
    }

    idForeground = (pH->pfnGetWindowThreadProcessId != NULL &&
                    pH->pfnGetForegroundWindow != NULL)
                 ? pH->pfnGetWindowThreadProcessId(pH->pfnGetForegroundWindow(),
                                                   NULL)
                 : 0u;
    idOther      = (pH->pfnGetWindowThreadProcessId != NULL)
                 ? pH->pfnGetWindowThreadProcessId(hWnd, NULL)
                 : 0u;

    if (idOther != idForeground ||
        (pH->pfnIsIconic != NULL && pH->pfnIsIconic(hWnd) != 0)) {

        if (pH->pfnGetLastActivePopup != NULL) {
            hWnd = pH->pfnGetLastActivePopup(hWnd);   /* 0x10007EC9: esi rebound */
        }
        if (pH->pfnIsIconic != NULL && pH->pfnIsIconic(hWnd) != 0) {
            if (pH->pfnShowWindow != NULL) {
                pH->pfnShowWindow(hWnd, BR_APP_SW_RESTORE);
            }
        }
        if (pH->pfnBringWindowToTop != NULL) {
            pH->pfnBringWindowToTop(hWnd);
        }
        if (pH->pfnSetForegroundWindow != NULL) {
            pH->pfnSetForegroundWindow(hWnd);
        }
    }

    /* 0x10007EE9 -- reached from BOTH arms. */
    if (pH->pfnOutputDebugString != NULL) {
        pH->pfnOutputDebugString(BR_APP_PREVAPP_MSG);
    }
    return 0;                           /* 0x10007EF5: xor eax,eax */
}
#endif /* !BR_MATCHING_BUILD -- matching twin in the windows.h block below */

/* ==========================================================================
 * 0x10007F10 -- the machine probe.  See br_appstart.h for the ESP trace that
 * establishes index 2 (dwTotalPhys) rather than index 1 (dwMemoryLoad).
 * ========================================================================== */
#define BR_MEMSTATUS_LENGTH     0
#define BR_MEMSTATUS_TOTALPHYS  2

void BrAppQueryTotalPhys(void)
{
    uint32_t aStatus[8];

    memset(aStatus, 0, sizeof aStatus);
    aStatus[BR_MEMSTATUS_LENGTH] = 0x20;    /* 0x10007F17: mov [esp],0x20 */

    if (g_pBrAppStartHost->pfnGlobalMemoryStatus != NULL) {
        g_pBrAppStartHost->pfnGlobalMemoryStatus(aStatus);
    }

    /* 0x10007F26/0x10007F2A: mov ecx,[esp+8] / mov [0x10226E78],ecx */
    g_brAppTotalPhysBytes = aStatus[BR_MEMSTATUS_TOTALPHYS];
}

/* ==========================================================================
 * 0x10003530 -- CHK_FReadLine.  Contract and the two preserved defects are in
 * br_appstart.h; this is the transcription.
 *
 * Stack trace for the three arguments, since the prologue pushes four
 * registers between entry and the first read:
 *
 *   entry            esp = E    [E+4]=dst  [E+8]=cbMax  [E+0xC]=pFile
 *   mov eax,[esp+8]             cbMax, read BEFORE any push
 *   push ebx/ebp/esi/edi        esp = E-0x10
 *   [esp+0x14] = E+4  -> dst    (0x1000354C, 0x100035CD)
 *   [esp+0x18] = E+8  -> cbMax  (0x10003569)
 *   [esp+0x1c] = E+0xC-> pFile  (0x10003542)
 * ========================================================================== */
char *BrChkFReadLine(char *pszDst, int32_t cbMax, FILE *pFile)
{
    char   *p = pszDst;
    int32_t n = 0;
    int     c;

    if (cbMax <= 0) {
        return pszDst;                  /* 0x100035CD..0x100035D7 */
    }

    for (;;) {
        c = getc(pFile);

        if (c == EOF) {                 /* 0x10003558 */
            if (n == 0) {
                return NULL;            /* 0x100035BB: the loop's only exit */
            }
            *p = '\0';                  /* 0x100035C2 */
            return p + 1;
        }

        if (c == '\r') {                /* 0x1000355D */
            *p++ = '\n';                /* 0x1000357A: CR is normalised to LF */
            *p++ = '\0';                /* 0x1000357E, then inc esi again     */
            c = getc(pFile);
            if (c != EOF && c != '\n') {
                ungetc(c, pFile);       /* 0x10003598: not a CRLF, put it back */
            }
            return p;
        }

        if (c == '\n') {                /* 0x10003562 */
            *p++ = '\n';                /* 0x100035A8: the LF is KEPT         */
            *p   = '\0';
            return p + 1;               /* 0x100035AF: lea eax,[esi+1]        */
        }

        *p++ = (char)c;                 /* 0x10003567 */
        n++;
        if (n >= cbMax) {
            /* 0x1000356F/0x10003573: `jl` back to the top, else return esi.
             * PRESERVED DEFECT: no NUL is written on this path. */
            return p;
        }
    }
}

/* ==========================================================================
 * 0x10007F40 -- the settings loader.
 *
 * THE ESP TRACE, because every value offset in the .ini half is an `[esp+N]`
 * and the pushes move between them.  This is the trap that has shipped twice
 * on this project, so it is written out rather than asserted.
 *
 *   entry                          esp = E   ([E] return, [E+4] pszCmdLine)
 *   sub esp,0x100                  esp = E-0x100    line buffer L = E-0x100
 *   push ebx/ebp/esi/edi           esp = E-0x110    <- the body's resting esp
 *
 *   so at rest:  L      == [esp+0x10]
 *                arg    == [esp+0x114]   (confirmed at 0x100083F6)
 *
 * The ladder's two reads are at DIFFERENT esp, one push apart:
 *
 *   push 0xC                       esp = E-0x114
 *   lea ecx,[esp+0x14]  = E-0x100 = L + 0      <- the strncmp subject
 *   push key / push ecx / call strncmp
 *   add esp,0xC                    esp = E-0x110
 *   lea edx,[esp+0x1c]  = E-0xF4  = L + 0x0C   <- the atoi subject
 *
 * Read both at the resting esp and the first becomes L+4, i.e. strncmp would
 * be matching four characters into the line.  Read both at E-0x114 and the
 * second becomes L+8, i.e. atoi would start eight characters in for a
 * twelve-character key.  Only the trace above makes all nineteen offsets come
 * out equal to their key's length, which is the check that it is right.
 *
 * Verified for every key: 0x1C-0x10=12 NetworkPlay=, 0x1C-0x10=12
 * chosenTrack=, 0x1A-0x10=10 chosenCar=, 0x1E-0x10=14 chosenWeather=,
 * 0x19-0x10=9 gameMode=, 0x1D-0x10=13 ReadJoystick=, 0x1D-0x10=13
 * HandlingType=, 0x1F-0x10=15 SuspensionType=, 0x19-0x10=9 TireType=,
 * 0x21-0x10=17 TransmissionType=, 0x19-0x10=9 TrackDir=, 0x17-0x10=7 CarDir=,
 * 0x17-0x10=7 SFXDir=, 0x1C-0x10=12 Interpolate=, 0x1F-0x10=15
 * SpeedSensitive=, 0x21-0x10=17 D3DDrawCarShadow=, 0x1D-0x10=13
 * RunBenchmark=, 0x1A-0x10=10 PlayMusic=, 0x18-0x10=8 PlaySFX=.
 * ========================================================================== */

/* 0x10007F40..0x10007FA0: two MSVC inline string idioms, `repne scasb` to
 * measure followed by `rep movsd`/`rep movsb` to copy -- a strcpy then a
 * strcat, not two loops to transcribe literally. */
void BrAppCfgBuildIniPath(void)
{
    size_t cch;

    /* strcpy(0x10226A78, 0x10B73540) */
    strncpy(g_aBrCfgIniPath, g_aBrCfgBaseDir, sizeof g_aBrCfgIniPath - 1);
    g_aBrCfgIniPath[sizeof g_aBrCfgIniPath - 1] = '\0';

    /* strcat(0x10226A78, "BossRally.ini") */
    cch = strlen(g_aBrCfgIniPath);
    if (cch + sizeof BR_APPCFG_INI_NAME <= sizeof g_aBrCfgIniPath) {
        memcpy(g_aBrCfgIniPath + cch, BR_APPCFG_INI_NAME,
               sizeof BR_APPCFG_INI_NAME);
    }
}

/* 0x100080EA..0x1000812E (and its identical twin at 0x100085AD..0x100085E3).
 * `dec eax / je` three times: 1, 2, 3, then a default.  See br_appstart.h for
 * why index == value on 1..3 and 0 otherwise. */
static void cfg_select_input_device(int32_t nJoystick)
{
    if (nJoystick >= 1 && nJoystick <= 3) {
        g_iBrCfgInputDevice = nJoystick;
    } else {
        g_iBrCfgInputDevice = 0;
    }
}

uint32_t BrCfgInputDeviceAddr(int32_t i)
{
    if (i < 0 || i >= BR_CFG_INPUT_COUNT) {
        return 0u;
    }
    return BR_CFG_INPUT_BASE + (uint32_t)i * BR_CFG_INPUT_STRIDE;
}

/* 0x100081F8..0x1000822B, 0x10008246..0x10008279, 0x10008294..0x100082C7 --
 * the three directory keys, byte-for-byte the same shape three times.
 *
 *   strcpy(dst, line + cchKey);
 *   len = strlen(dst);
 *   dst[len - 1] = 0;          <- drops the '\n' BrChkFReadLine kept
 *
 * The final store is `mov byte [ecx + <dst-2>], al` with ecx = len+1 and
 * al = 0, i.e. dst[len-1].  PRESERVED DEFECT, guarded here: on the file's
 * LAST line, if it has no newline, len is 0 and the original writes one byte
 * BEFORE the buffer.  The instructions that establish it are 0x1000821A
 * (edi = dst), 0x1000821F..0x10008224 (ecx = len+1) and 0x10008226 (the
 * store).  The port keeps the truncation -- which is real, and eats a
 * character from an unterminated last line -- and drops only the underflow,
 * because the byte it would corrupt belongs to another module. */
static void cfg_set_dir(char *pszDst, size_t cbDst, const char *pszValue)
{
    size_t len;

    strncpy(pszDst, pszValue, cbDst - 1);
    pszDst[cbDst - 1] = '\0';

    len = strlen(pszDst);
    if (len > 0) {                       /* DEVIATION: the original does not test */
        pszDst[len - 1] = '\0';
    }
}

/* --------------------------------------------------------------------------
 * The .ini half, 0x10007F40..0x100083F5.
 *
 * An ELSE-IF ladder over strncmp against the start of the line, so one line
 * sets at most one key and the first match wins.  The read loop is a
 * do-while: 0x10007FCB reads the first line and 0x100083DD reads the rest,
 * with 0x100083ED (CHK_FClose) as the single exit for both.
 * -------------------------------------------------------------------------- */
void BrAppCfgParseIni(void)
{
    char  aLine[BR_APPCFG_LINE_ALLOC];
    FILE *pFile;

    BrAppCfgBuildIniPath();

    /* 0x10007FA1: if CHK_FileExists says no, the whole half is skipped and
     * no file is opened -- so CHK_FClose is not reached either.
     *
     * THIS TEST IS LOAD-BEARING IN THE ORIGINAL AND UNOBSERVABLE IN THE PORT,
     * and the difference is worth stating rather than leaving as a puzzle.
     * The original's next call is CHK_FReadOpen (0x10003320), which does not
     * return on failure: it writes "CHK_FReadOpen(): error opening file %s"
     * into c:\RallyError.txt and calls exit(1) at 0x1000840E.  So without this
     * gate a missing .ini would KILL THE GAME.  This port does not transcribe
     * that fatal path (see br_appstart.h), so a missing file merely produces a
     * NULL FILE* three lines below and the gate looks redundant.
     * Deleting it is mutation M38, and M38 SURVIVES the suite for exactly this
     * reason -- not because the suite is weak.  Transcribing CHK_FReadOpen is
     * what would make it killable; it is on the frontier. */
    if (BrChkFileExists(g_aBrCfgIniPath) == 0) {
        return;
    }

    /* The operative core of CHK_FReadOpen (0x10003320); see br_appstart.h for
     * what of it is deliberately not here.  "rb" is the literal at
     * 0x1007B0E0 -- the original opens the .ini in BINARY mode, which is why
     * CHK_FReadLine has to handle CR itself. */
    pFile = fopen(g_aBrCfgIniPath, "rb");
    if (pFile == NULL) {
        /* DEVIATION: the original cannot reach here -- CHK_FReadOpen exits. */
        return;
    }

    while (BrChkFReadLine(aLine, BR_APPCFG_LINE, pFile) != NULL) {

        if (strncmp(aLine, "NetworkPlay=", 12) == 0) {
            g_brRaceNet = atoi(aLine + 12);                 /* 0x10226A48 */

        } else if (strncmp(aLine, "chosenTrack=", 12) == 0) {
            g_brCfgChosenTrack = atoi(aLine + 12);

        } else if (strncmp(aLine, "chosenCar=", 10) == 0) {
            g_brCfgChosenCar = atoi(aLine + 10);

        } else if (strncmp(aLine, "chosenWeather=", 14) == 0) {
            g_brCfgChosenWeather = atoi(aLine + 14);

        } else if (strncmp(aLine, "gameMode=", 9) == 0) {
            g_brCfgGameMode = atoi(aLine + 9);

        } else if (strncmp(aLine, "ReadJoystick=", 13) == 0) {
            g_brCfgReadJoystick = atoi(aLine + 13);
            cfg_select_input_device(g_brCfgReadJoystick);

        } else if (strncmp(aLine, "HandlingType=", 13) == 0) {
            g_brCfgHandlingType = atoi(aLine + 13);

        } else if (strncmp(aLine, "SuspensionType=", 15) == 0) {
            g_brCfgSuspensionType = atoi(aLine + 15);

        } else if (strncmp(aLine, "TireType=", 9) == 0) {
            g_brCfgTireType = atoi(aLine + 9);

        } else if (strncmp(aLine, "TransmissionType=", 17) == 0) {
            g_brCfgTransmission = atoi(aLine + 17);

        } else if (strncmp(aLine, "TrackDir=", 9) == 0) {
            cfg_set_dir(g_aBrCfgTrackDir, sizeof g_aBrCfgTrackDir, aLine + 9);

        } else if (strncmp(aLine, "CarDir=", 7) == 0) {
            cfg_set_dir(g_aBrCfgCarDir, sizeof g_aBrCfgCarDir, aLine + 7);

        } else if (strncmp(aLine, "SFXDir=", 7) == 0) {
            cfg_set_dir(g_aBrCfgSfxDir, sizeof g_aBrCfgSfxDir, aLine + 7);

        } else if (strncmp(aLine, "Interpolate=", 12) == 0) {
            g_brCfgInterpolate = atoi(aLine + 12);

        } else if (strncmp(aLine, "SpeedSensitive=", 15) == 0) {
            g_brCfgSpeedSensitive = atoi(aLine + 15);

        } else if (strncmp(aLine, "D3DDrawCarShadow=", 17) == 0) {
            /* 0x1000834A neg / sbb / inc -- the flag is INVERTED. */
            g_brCfgD3DCarShadow = (atoi(aLine + 17) == 0) ? 1 : 0;

        } else if (strncmp(aLine, "RunBenchmark=", 13) == 0) {
            g_brCfgRunBenchmark = atoi(aLine + 13);

        } else if (strncmp(aLine, "PlayMusic=", 10) == 0) {
            g_brCfgPlayMusic = atoi(aLine + 10);

        } else if (strncmp(aLine, "PlaySFX=", 8) == 0) {
            BrSndG0B5DE8 = atoi(aLine + 8);                 /* 0x100B55F0 */
        }
    }

    /* 0x100083ED: CHK_FClose.  Its debug logging and its exit(1)-on-EOF path
     * are that function's; see br_appstart.h. */
    fclose(pFile);
}

/* --------------------------------------------------------------------------
 * The command-line half, 0x100083F6..0x10008756.
 *
 * strstr, ANYWHERE in the string, and NO else -- every key is tested, so one
 * command line can set all fifteen.  The value offset is the key's own length,
 * computed inline with `repne scasb` / `not ecx` / `dec ecx` and added to
 * strstr's result at each site.
 *
 * The guard is two tests: NULL (0x100083FD), then strlen == 0 (0x1000840E's
 * `not ecx` / `dec ecx` / `je`).  An empty command line does nothing at all --
 * not even the five command-line-only keys.
 * -------------------------------------------------------------------------- */
void BrAppCfgParseCmdLine(const char *pszCmdLine)
{
    const char *p;
    char       *q;

    if (pszCmdLine == NULL) {
        return;                                  /* 0x100083FF */
    }
    if (strlen(pszCmdLine) == 0) {
        return;                                  /* 0x10008411 */
    }

    p = strstr(pszCmdLine, "NetworkPlay=");
    if (p != NULL) {
        g_brRaceNet = atoi(p + 12);
    }

    /* 0x10008458..0x100084CB -- the only non-integer key.
     * PUSH ORDER TRAP: `push 0x20` (0x10008473) and `push 0x10B71648`
     * (0x1000847A) are the two arguments of the strchr CALLED at 0x100084A7,
     * pushed BEFORE the inline strcpy that fills the buffer.  They are not
     * arguments to anything in between. */
    p = strstr(pszCmdLine, "szPlayerName=");
    if (p != NULL) {
        strncpy(g_aBrCfgPlayerName, p + 13, sizeof g_aBrCfgPlayerName - 1);
        g_aBrCfgPlayerName[sizeof g_aBrCfgPlayerName - 1] = '\0';

        q = strchr(g_aBrCfgPlayerName, ' ');     /* 0x20 */
        if (q != NULL) {
            *q = '\0';
        }
        q = strchr(g_aBrCfgPlayerName, '\n');    /* 0x0A */
        if (q != NULL) {
            *q = '\0';
        }
    }

    p = strstr(pszCmdLine, "chosenTrack=");
    if (p != NULL) {
        g_brCfgChosenTrack = atoi(p + 12);
    }

    p = strstr(pszCmdLine, "chosenCar=");
    if (p != NULL) {
        g_brCfgChosenCar = atoi(p + 10);
    }

    p = strstr(pszCmdLine, "chosenWeather=");
    if (p != NULL) {
        g_brCfgChosenWeather = atoi(p + 14);
    }

    p = strstr(pszCmdLine, "gameMode=");
    if (p != NULL) {
        g_brCfgGameMode = atoi(p + 9);
    }

    p = strstr(pszCmdLine, "ReadJoystick=");
    if (p != NULL) {
        g_brCfgReadJoystick = atoi(p + 13);
        cfg_select_input_device(g_brCfgReadJoystick);
    }

    p = strstr(pszCmdLine, "HandlingType=");
    if (p != NULL) {
        g_brCfgHandlingType = atoi(p + 13);
    }

    p = strstr(pszCmdLine, "SuspensionType=");
    if (p != NULL) {
        g_brCfgSuspensionType = atoi(p + 15);
    }

    p = strstr(pszCmdLine, "TireType=");
    if (p != NULL) {
        g_brCfgTireType = atoi(p + 9);
    }

    p = strstr(pszCmdLine, "TransmissionType=");
    if (p != NULL) {
        g_brCfgTransmission = atoi(p + 17);
    }

    p = strstr(pszCmdLine, "cPlayers=");
    if (p != NULL) {
        g_brCfgPlayers = atoi(p + 9);
    }

    p = strstr(pszCmdLine, "bcar=");
    if (p != NULL) {
        g_brCfgBenchCar = atoi(p + 5);
    }

    p = strstr(pszCmdLine, "btire=");
    if (p != NULL) {
        g_brCfgBenchTire = atoi(p + 6);
    }

    p = strstr(pszCmdLine, "bsuspension=");
    if (p != NULL) {
        g_brCfgBenchSuspension = atoi(p + 12);
    }
}

/* @n64 0x8021DDFC located */
void BrAppCfgParse(const char *pszCmdLine)
{
    BrAppCfgParseIni();
    BrAppCfgParseCmdLine(pszCmdLine);
}

/* Not in the original: the original gets fresh .data from the loader. */
void BrAppCfgResetForTest(void)
{
    memset(g_aBrCfgBaseDir,    0, sizeof g_aBrCfgBaseDir);
    memset(g_aBrCfgIniPath,    0, sizeof g_aBrCfgIniPath);
    memset(g_aBrCfgPlayerName, 0, sizeof g_aBrCfgPlayerName);

    memset(g_aBrCfgTrackDir, 0, sizeof g_aBrCfgTrackDir);
    memset(g_aBrCfgCarDir,   0, sizeof g_aBrCfgCarDir);
    memset(g_aBrCfgSfxDir,   0, sizeof g_aBrCfgSfxDir);
    memcpy(g_aBrCfgTrackDir, "tracks/", sizeof "tracks/");
    memcpy(g_aBrCfgCarDir,   "cars/",   sizeof "cars/");
    memcpy(g_aBrCfgSfxDir,   "sfx/",    sizeof "sfx/");

    g_brCfgChosenTrack     = 2;
    g_brCfgChosenCar       = 0;
    g_brCfgChosenWeather   = 0;
    g_brCfgGameMode        = 1;
    g_brCfgReadJoystick    = 0;
    g_brCfgHandlingType    = 1;
    g_brCfgSuspensionType  = 1;
    g_brCfgTireType        = 2;
    g_brCfgTransmission    = 1;
    g_brCfgInterpolate     = 1;
    g_brCfgSpeedSensitive  = 1;
    g_brCfgD3DCarShadow    = 0;
    g_brCfgRunBenchmark    = 0;
    g_brCfgPlayMusic       = 2;
    g_brCfgPlayers         = 0;
    g_brCfgBenchCar        = 0;
    g_brCfgBenchTire       = 0;
    g_brCfgBenchSuspension = 0;

    g_iBrCfgInputDevice    = 0;
    g_brAppTotalPhysBytes  = 0;

    /* The two this module writes but does not own. */
    g_brRaceNet            = 0;
    BrSndG0B5DE8           = 1;
}

/* ── Ghidra-matched functions ─────────────────────────── */
#ifdef BR_MATCHING_BUILD
#include <windows.h>
extern int DAT_10226e78;

/* WHAT IT DOES: query total physical RAM via GlobalMemoryStatus and store it. */
/* @implements 0x10007F10 glide BrMemoryQuery */

int BrMemoryQuery(void)

{
  MEMORYSTATUS local_20;
  
  local_20.dwLength = 0x20;
  GlobalMemoryStatus(&local_20);
  DAT_10226e78 = local_20.dwTotalPhys;
  return;
}

/* WHAT IT DOES: single-instance guard -- see the port version above.  The
 * original calls USER32 straight through the import table; the /MD import
 * pointers for GetWindowThreadProcessId and IsIconic are CSEd into edi
 * across their repeated calls, which /O2 does on its own. */
/* @implements 0x10007E80 glide BrAppCheckPreviousApp */
int32_t BrAppCheckPreviousApp(void)
{
    HWND hWnd = FindWindowA(BR_APP_WNDCLASS, BR_APP_WNDTITLE);

    if (hWnd != NULL) {
        HWND hFg = GetForegroundWindow();

        if (GetWindowThreadProcessId(hWnd, NULL)
                != GetWindowThreadProcessId(hFg, NULL)
            || IsIconic(hWnd)) {

            hWnd = GetLastActivePopup(hWnd);
            if (IsIconic(hWnd)) {
                ShowWindow(hWnd, BR_APP_SW_RESTORE);
            }
            BringWindowToTop(hWnd);
            SetForegroundWindow(hWnd);
        }

        OutputDebugStringA(BR_APP_PREVAPP_MSG);
        return 0;
    }
    return 1;
}

#endif /* BR_MATCHING_BUILD */
