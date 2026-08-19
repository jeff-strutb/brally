/* br_appstart.h -- APPLICATION START-UP: admission, machine probe, settings.
 *
 * ARCHITECTURAL CONCERN: the three things RallyMain does BEFORE it has a
 * window, in the order it does them:
 *
 *   1. decide whether this instance is allowed to run at all   (0x10007E80)
 *   2. measure the machine                                     (0x10007F10)
 *   3. load settings from BossRally.ini and the command line   (0x10007F40)
 *
 * They are one family because they are one phase: nothing here draws, loads an
 * asset or touches the renderer, and everything here has finished before
 * 0x10019670 registers the window class.  Step 3 alone writes 24 globals that
 * eleven other modules read, which is why this file is mostly a table of who
 * owns what.
 *
 * Call sites, all in RallyMain (Glide 0x1001CC00):
 *
 *   0x1001CC91  call 0x10007E80   ZERO ABORTS -- RallyMain returns immediately
 *   0x1001CCA0  call 0x10007F10   no arguments, return ignored
 *   0x1001CCB0  push esi / call 0x10007F40   esi is arg3, the command line
 *
 * ==========================================================================
 * WHAT 0x10007E80 ACTUALLY GATES ON
 * ==========================================================================
 *
 * It is the SINGLE-INSTANCE guard, and nothing else.  It is not a version
 * check, a licence check or a hardware check -- RallyMain does its own memory
 * check separately at 0x1001CC5C (`< 0x600` -> MessageBoxA -> return 0), and
 * that one is upstream of this call and belongs to br_boot.c.
 *
 *   FindWindowA("BossRally", "Boss Rally")   -- the exact class and title
 *                                              0x10019670 later registers
 *   no such window  -> return 1, the game starts
 *   found           -> raise the OTHER instance, OutputDebugStringA
 *                      "CheckPrevious App - Another instance of this app is
 *                      already running.\n"  (the literal at 0x1007B330),
 *                      return 0, and RallyMain quits
 *
 * The activation dance is skipped when the existing window is already the
 * foreground thread's AND is not minimised -- i.e. it only steals focus when
 * focus is not already there.  The two GetWindowThreadProcessId calls compare
 * THREAD ids (the second out-parameter is NULL in both, so no process id is
 * ever fetched); `esi` is then replaced by GetLastActivePopup's result, so
 * ShowWindow/BringWindowToTop/SetForegroundWindow act on the POPUP, not on
 * the window FindWindowA returned.
 *
 * ==========================================================================
 * WHAT 0x10007F10 IS, AND THE ESP TRACE THAT DECIDES IT
 * ==========================================================================
 *
 * 36 bytes: one GlobalMemoryStatus into a 0x20-byte stack MEMORYSTATUS, then
 * one dword out of it into 0x10226E78.  WHICH dword is the whole content of
 * the function, and it is decided by an ESP trace, not by the displacement:
 *
 *   entry                 esp = E          ([E] is the return address)
 *   sub esp,0x20          esp = E-0x20     buffer B = E-0x20 .. E-1
 *   mov [esp],0x20        B.dwLength = 0x20
 *   push eax              esp = E-0x24     (eax == B)
 *   call GlobalMemoryStatus                STDCALL: it pops its own argument,
 *                         esp = E-0x20     so esp is back at B, not E-0x24
 *   mov ecx,[esp+8]       = E-0x18 = B+8
 *
 * MEMORYSTATUS is {dwLength, dwMemoryLoad, dwTotalPhys, dwAvailPhys,
 * dwTotalPageFile, dwAvailPageFile, dwTotalVirtual, dwAvailVirtual}, so B+8 is
 * dwTotalPhys -- INSTALLED PHYSICAL MEMORY IN BYTES.  Assume esp is still
 * E-0x24 at the read (i.e. treat the callee as cdecl) and B+8 becomes
 * B+4 == dwMemoryLoad, a 0..100 percentage, which is the wrong answer in the
 * most plausible-looking way available.
 *
 * ==========================================================================
 * WHAT 0x10007F40 IS
 * ==========================================================================
 *
 * CONFIRMED, not assumed: it is the settings loader, and it has TWO halves
 * that do not do the same thing.
 *
 *   FIRST HALF (0x10007F40..0x100083F5) -- BossRally.ini.
 *     Builds the path as  <base directory> + "BossRally.ini"  (two MSVC
 *     inline strcpy/strcat idioms, `repne scasb` + `rep movsd`/`movsb`), then
 *     CHK_FileExists, CHK_FReadOpen, a read-a-line loop, CHK_FClose.
 *     Each line is matched with strncmp against the START of the line, in an
 *     ELSE-IF ladder: the first key that matches wins and the rest of the
 *     ladder is skipped, so one line sets at most one setting.
 *     19 keys, of which 9 are ini-only.
 *
 *   SECOND HALF (0x100083F6..0x10008756) -- the command line.
 *     `arg` is RallyMain's arg3 (also stored to 0x105BC738).  NULL or empty
 *     returns immediately.  Each key is looked for with strstr ANYWHERE in the
 *     string, and every key is tested INDEPENDENTLY -- there is no else here,
 *     so one command line can set all 15.
 *     15 keys, of which 5 are command-line-only.
 *
 * Ten keys appear in both halves.  The command line is parsed second, so it
 * WINS over the .ini for those ten.
 *
 * Everything is case-SENSITIVE: strncmp and strstr, never _stricmp.  BRGlide
 * does import MSVCRT!_stricmp (0x1008C320) and this function does not call it.
 *
 * ==========================================================================
 * THE KEY TABLE.  Offsets are the constant displacements the original bakes
 * into the .ini half; each equals strlen(key), and the ESP trace that pins
 * them is in br_appstart.c above BrAppCfgParse.
 * ==========================================================================
 *
 *   key                  len  ini  cmd  global      port symbol
 *   NetworkPlay=          12   Y    Y   0x10226A48  g_brRaceNet (br_racestep)
 *   szPlayerName=         13   .    Y   0x10B71648  g_aBrCfgPlayerName
 *   chosenTrack=          12   Y    Y   0x100B3014  g_brCfgChosenTrack   (2)
 *   chosenCar=            10   Y    Y   0x10226E7C  g_brCfgChosenCar
 *   chosenWeather=        14   Y    Y   0x10226E80  g_brCfgChosenWeather
 *   gameMode=              9   Y    Y   0x100A9360  g_brCfgGameMode      (1)
 *   ReadJoystick=         13   Y    Y   0x10B71530  g_brCfgReadJoystick
 *                                     + 0x10B71534  g_iBrCfgInputDevice
 *   HandlingType=         13   Y    Y   0x1007B320  g_brCfgHandlingType  (1)
 *   SuspensionType=       15   Y    Y   0x1007B328  g_brCfgSuspensionType(1)
 *   TireType=              9   Y    Y   0x1007B32C  g_brCfgTireType      (2)
 *   TransmissionType=     17   Y    Y   0x1007B324  g_brCfgTransmission  (1)
 *   TrackDir=              9   Y    .   0x100B74C0  g_aBrCfgTrackDir "tracks/"
 *   CarDir=                7   Y    .   0x100B7900  g_aBrCfgCarDir   "cars/"
 *   SFXDir=                7   Y    .   0x100B7D40  g_aBrCfgSfxDir   "sfx/"
 *   Interpolate=          12   Y    .   0x100A5EAC  g_brCfgInterpolate   (1)
 *   SpeedSensitive=       15   Y    .   0x100B2E6C  g_brCfgSpeedSensitive(1)
 *   D3DDrawCarShadow=     17   Y    .   0x10396EB0  g_brCfgD3DCarShadow  INVERTED
 *   RunBenchmark=         13   Y    .   0x118EEEDC  g_brCfgRunBenchmark
 *   PlayMusic=            10   Y    .   0x1007B074  g_brCfgPlayMusic     (2)
 *   PlaySFX=               8   Y    .   0x100B55F0  BrSndG0B5DE8 (slice1_08)
 *   cPlayers=              9   .    Y   0x1021CDF8  g_brCfgPlayers
 *   bcar=                  5   .    Y   0x1021CE50  g_brCfgBenchCar
 *   btire=                 6   .    Y   0x10226A40  g_brCfgBenchTire
 *   bsuspension=          12   .    Y   0x10226A3C  g_brCfgBenchSuspension
 *
 * The value in brackets is the LOAD-TIME value read out of BRGlide.dll's
 * .data image, not a guess; everything else is .bss and starts at zero.  The
 * three directory defaults are likewise the image's own bytes.
 *
 * D3DDrawCarShadow= IS INVERTED, and the original says so in three
 * instructions (0x1000834A `neg eax` / `sbb eax,eax` / `inc eax`): the global
 * gets 1 when atoi returns 0 and 0 otherwise.  `D3DDrawCarShadow=1` therefore
 * STORES 0.  Reading it as a plain flag inverts every car shadow.
 *
 * ==========================================================================
 * TWO ALIASES, BOTH ALREADY OWNED ELSEWHERE
 * ==========================================================================
 *
 * Per CONVENTIONS.md, an original global must have exactly ONE host object.
 * Two of the 24 already have storage in other modules, so this module writes
 * through THEIR symbols and defines nothing of its own for them:
 *
 *   0x10226A48  g_brRaceNet    defined by port/src/br_racestep.c
 *   0x100B55F0  BrSndG0B5DE8   defined by port/src/slice1_08.c
 *
 * br_racestep.h line 365 already says "written only by 0x10007F40"; this is
 * the module that does the writing.  slice1_08.h already says 0x100B55F0 is
 * `PlaySFX=`; it was right.
 *
 * 0x100A9360 is modelled by br_race.h, but only as the `mode` FIELD of a
 * by-value BrRaceRules -- there is no global there to alias, so this module
 * owns the storage and br_race.h's rules struct is filled from it.
 *
 * ==========================================================================
 * THE BASE DIRECTORY, AND WHY THE SHIPPED DEFAULT PATH IS "BossRally.ini"
 * ==========================================================================
 *
 * 0x10B73540 is a 0x104-byte buffer filled by Glide 0x10063860 -- RallyMain
 * calls it at 0x1001CCA5, one instruction before this function -- from
 * HKLM\SOFTWARE\SouthPeak Interactive\Boss Rally\Directory, with a "\"
 * appended if the value does not already end in one.  0x10063860 is NOT
 * ported; its registry read has no host meaning.  At load the buffer is zero,
 * so with no registry key the .ini path is the bare relative "BossRally.ini".
 * RallyMain uses the same base for "BossRally.cfg" at 0x1001CCE2.
 *
 * ==========================================================================
 * WHAT IS NOT TRANSCRIBED HERE, AND WHY
 * ==========================================================================
 *
 * The CHK_ file layer is a different concern (slice1_01 covers part of it).
 * Of the four CHK_ entry points 0x10007F40 calls:
 *
 *   Glide 0x10003680  CHK_FileExists  -- ALREADY PORTED as BrChkFileExists in
 *                     port/src/slice1_01.c, under its D3D address 0x10003320.
 *                     Reused, not re-transcribed.
 *   Glide 0x10003530  CHK_FReadLine   -- transcribed here (BrChkFReadLine).
 *                     Not because it belongs here, but because its exact
 *                     newline behaviour is load-bearing for THIS parser: it
 *                     KEEPS the '\n' in the buffer, which is the entire reason
 *                     TrackDir=/CarDir=/SFXDir= chop their last byte.
 *   Glide 0x10003320  CHK_FReadOpen   -- NOT transcribed.  Only its operative
 *                     core (fopen of the path in mode "rb", the literal at
 *                     0x1007B0E0) is reproduced, as a static helper.  Its
 *                     debug logging, its two CHK_AllocateMemory calls for the
 *                     handle and a copy of the name, and its fatal path
 *                     (fopen "c:\RallyError.txt", fprintf, exit(1)) are that
 *                     function's, not this one's, and none of them changes a
 *                     parsed value.
 *   Glide 0x100035E0  CHK_FClose      -- NOT transcribed, same reasoning; its
 *                     fatal path is exit(1) when fclose returns EOF.
 *
 * The six USER32 calls in 0x10007E80 and the one KERNEL32 call in 0x10007F10
 * have no portable equivalent, so they go through a caller-installed hook
 * table (BrAppStartHost).  The DECISIONS around them are transcribed exactly;
 * only the platform primitives are indirected.  The default table is not a
 * stand-in that makes something happen: this host has no Win32 window manager
 * and never registers the class "BossRally", so FindWindowA's answer here is
 * genuinely NULL, and GlobalMemoryStatus's is genuinely all-zero.
 *
 * ==========================================================================
 * TWO THINGS RECORDED BECAUSE A MUTATION SURVIVED THE SUITE
 * ==========================================================================
 *
 * A surviving mutant is a fact about the code.  Neither of these is a hole in
 * the tests, and neither should be "fixed" by inventing an assertion:
 *
 *   - The command-line half's `strlen(arg) == 0` early-out (0x1000840E) is
 *     REDUNDANT.  strstr on an empty string matches none of the fifteen keys,
 *     so deleting the test changes nothing observable.  The NULL test one
 *     instruction earlier (0x100083FD) is a different matter and IS
 *     load-bearing -- without it the parse dereferences NULL.
 *   - The CHK_FileExists gate in front of the .ini open is load-bearing in
 *     the ORIGINAL only.  The original's CHK_FReadOpen does not return on
 *     failure -- it writes c:\RallyError.txt and calls exit(1) -- so without
 *     the gate a missing .ini would kill the game.  This port returns a NULL
 *     FILE* instead, which makes the gate unobservable here.  Transcribing
 *     CHK_FReadOpen's fatal path is what would make it testable again.
 */
#ifndef BR_APPSTART_H
#define BR_APPSTART_H

#include <stdint.h>
#include <stdio.h>

/* ------------------------------------------------------------------ *
 * The platform primitives.  Not in the original -- there they are direct
 * imports.  A test installs its own table to drive the decisions.
 * ------------------------------------------------------------------ */
typedef struct BrAppStartHost {
    /* 0x10007E80's six USER32 imports, plus OutputDebugStringA. */
    void         *(*pfnFindWindow)(const char *pszClass, const char *pszTitle);
    void         *(*pfnGetForegroundWindow)(void);
    uint32_t      (*pfnGetWindowThreadProcessId)(void *hWnd, uint32_t *pPid);
    int32_t       (*pfnIsIconic)(void *hWnd);
    void         *(*pfnGetLastActivePopup)(void *hWnd);
    void          (*pfnShowWindow)(void *hWnd, int32_t nCmdShow);
    void          (*pfnBringWindowToTop)(void *hWnd);
    void          (*pfnSetForegroundWindow)(void *hWnd);
    void          (*pfnOutputDebugString)(const char *psz);
    /* 0x10007F10's one KERNEL32 import.  The callee fills eight dwords; the
     * caller has already put 0x20 in aStatus[0] as dwLength. */
    void          (*pfnGlobalMemoryStatus)(uint32_t aStatus[8]);
} BrAppStartHost;

/* Install the table.  NULL restores the default (no window manager, no
 * memory figures).  Not in the original. */
void BrAppStartSetHost(const BrAppStartHost *pHost);

/* The window class and title FindWindowA looks for, 0x1007B378 / 0x1007B384.
 * 0x10019670 registers exactly these, which is what makes the guard work. */
#define BR_APP_WNDCLASS  "BossRally"
#define BR_APP_WNDTITLE  "Boss Rally"

/* 0x1007B330 -- what the guard prints when it refuses. */
#define BR_APP_PREVAPP_MSG \
    "CheckPrevious App - Another instance of this app is already running.\n"

/* SW_RESTORE, the literal 9 pushed at 0x10007ED2. */
#define BR_APP_SW_RESTORE  9

/* 0x10007E80.  1 == no previous instance, start the game.
 *              0 == one was found and raised; RallyMain must quit. */
int32_t BrAppCheckPreviousApp(void);

/* 0x10007F10.  Reads MEMORYSTATUS.dwTotalPhys into g_brAppTotalPhysBytes. */
void BrAppQueryTotalPhys(void);

/* 0x10226E78 -- installed physical memory, in BYTES.  See the ESP trace in
 * the banner: it is dwTotalPhys, not dwMemoryLoad. */
extern uint32_t g_brAppTotalPhysBytes;

/* ------------------------------------------------------------------ *
 * 0x10003530 -- CHK_FReadLine (D3D 0x100031E0).
 *
 * Reads one line into pszDst.  Its contract is nothing like fgets and every
 * bit of the difference matters to the parser:
 *
 *   - a '\n' IS written into the buffer for both LF and CR/CRLF endings, and
 *     it is followed by a NUL.  The caller therefore sees "key=value\n".
 *   - CR is normalised to '\n'; a following LF is swallowed, anything else is
 *     pushed back with ungetc.
 *   - at EOF with nothing read it returns NULL; that is the loop's exit.
 *   - it returns a pointer PAST the NUL it wrote, which the caller only ever
 *     tests for NULL.
 *   - PRESERVED DEFECT (0x10003573): when cbMax ordinary characters have been
 *     stored the function returns WITHOUT writing a NUL, so a line longer than
 *     the buffer leaves it unterminated.
 *   - PRESERVED DEFECT (0x1000357A / 0x100035A8): the CR and LF arms write two
 *     bytes at the cursor without re-checking cbMax, so a line of exactly
 *     cbMax-1 characters puts the NUL one byte past the end.  Give the buffer
 *     cbMax + 2 bytes; the original gives it cbMax and overruns.
 *
 * The original takes the CHK file handle and uses its +0x00 (the FILE*); this
 * takes the FILE* directly.
 * ------------------------------------------------------------------ */
char *BrChkFReadLine(char *pszDst, int32_t cbMax, FILE *pFile);

/* The .ini line buffer, 0x100 -- the literal pushed at 0x10007FC5 and
 * 0x100083D7.  BR_APPCFG_LINE_ALLOC is what a caller must actually allocate;
 * see the second preserved defect above. */
#define BR_APPCFG_LINE        0x100
#define BR_APPCFG_LINE_ALLOC  (BR_APPCFG_LINE + 2)

/* ------------------------------------------------------------------ *
 * 0x10007F40 -- the settings loader.  pszCmdLine is RallyMain's arg3 and may
 * be NULL; an empty string is treated exactly like NULL.
 * ------------------------------------------------------------------ */
void BrAppCfgParse(const char *pszCmdLine);

/* The two halves, exposed so a test can drive either alone.  The original has
 * no such split -- these are the two arms of the one function, cut at
 * 0x100083F6 where the .ini half falls into the command-line half. */
void BrAppCfgParseIni(void);                        /* 0x10007F40..0x100083F5 */
void BrAppCfgParseCmdLine(const char *pszCmdLine);  /* 0x100083F6..0x10008756 */

/* 0x10007F40..0x10007FA0 -- <base directory> + "BossRally.ini" into
 * g_aBrCfgIniPath.  Exposed because it is the only thing in the .ini half
 * that runs even when the file is absent. */
void BrAppCfgBuildIniPath(void);

#define BR_APPCFG_INI_NAME  "BossRally.ini"

/* ------------------------------------------------------------------ *
 * Buffer sizes.  Each is the distance to the next global the code actually
 * references, so each is an UPPER BOUND on the original's, measured rather
 * than assumed.
 * ------------------------------------------------------------------ */
#define BR_APPCFG_DIR_MAX     0x400  /* 0x100B74C0 -> 0x100B78C0            */
#define BR_APPCFG_NAME_MAX    0x400  /* 0x10B71648 -> 0x10B71A48            */
#define BR_APPCFG_BASEDIR_MAX 0x104  /* 0x10063860 passes 0x104 to RegQuery */
/* DEVIATION.  0x10226A78's next referenced neighbour is 0x10226B48, so the
 * original's .ini path buffer is at most 0xD0 bytes -- shorter than the 0x104
 * base directory it copies in, plus 13 for the name.  The original can
 * therefore overrun it.  This port gives it room instead of reproducing the
 * overrun, because the bytes it would corrupt are other modules' state. */
#define BR_APPCFG_PATH_MAX    (BR_APPCFG_BASEDIR_MAX + 0x40)

/* 0x10B73540 -- written by Glide 0x10063860 from the registry.  Unported;
 * empty at load, which makes the .ini path relative. */
extern char g_aBrCfgBaseDir[BR_APPCFG_BASEDIR_MAX];

/* 0x10226A78 -- the assembled path. */
extern char g_aBrCfgIniPath[BR_APPCFG_PATH_MAX];

/* The three directory prefixes.  Each carries its own trailing separator
 * because nothing ever appends one -- br_sfx.h already records this for
 * SFXDir.  Defaults are the .data image's own bytes. */
extern char g_aBrCfgTrackDir[BR_APPCFG_DIR_MAX];  /* 0x100B74C0 "tracks/" */
extern char g_aBrCfgCarDir  [BR_APPCFG_DIR_MAX];  /* 0x100B7900 "cars/"   */
extern char g_aBrCfgSfxDir  [BR_APPCFG_DIR_MAX];  /* 0x100B7D40 "sfx/"    */

/* 0x10B71648 -- szPlayerName=, truncated at the first space or newline. */
extern char g_aBrCfgPlayerName[BR_APPCFG_NAME_MAX];

/* The integer settings this module owns outright. */
extern int32_t g_brCfgChosenTrack;      /* 0x100B3014, load-time 2 */
extern int32_t g_brCfgChosenCar;        /* 0x10226E7C              */
extern int32_t g_brCfgChosenWeather;    /* 0x10226E80              */
extern int32_t g_brCfgGameMode;         /* 0x100A9360, load-time 1 */
extern int32_t g_brCfgReadJoystick;     /* 0x10B71530              */
extern int32_t g_brCfgHandlingType;     /* 0x1007B320, load-time 1 */
extern int32_t g_brCfgSuspensionType;   /* 0x1007B328, load-time 1 */
extern int32_t g_brCfgTireType;         /* 0x1007B32C, load-time 2 */
extern int32_t g_brCfgTransmission;     /* 0x1007B324, load-time 1 */
extern int32_t g_brCfgInterpolate;      /* 0x100A5EAC, load-time 1 */
extern int32_t g_brCfgSpeedSensitive;   /* 0x100B2E6C, load-time 1 */
extern int32_t g_brCfgD3DCarShadow;     /* 0x10396EB0, INVERTED    */
extern int32_t g_brCfgRunBenchmark;     /* 0x118EEEDC              */
extern int32_t g_brCfgPlayMusic;        /* 0x1007B074, load-time 2 */
extern int32_t g_brCfgPlayers;          /* 0x1021CDF8              */
extern int32_t g_brCfgBenchCar;         /* 0x1021CE50              */
extern int32_t g_brCfgBenchTire;        /* 0x10226A40              */
extern int32_t g_brCfgBenchSuspension;  /* 0x10226A3C              */

/* ------------------------------------------------------------------ *
 * 0x10B71534 -- the live input-device object, read by fourteen functions
 * (0x1001CE20, 0x1002F380, 0x10037660, 0x100379B0, 0x1003C950, 0x10058900,
 * 0x1005AFF0, 0x10061310, 0x100704E0, 0x100706D0, 0x10071710, 0x100719D0,
 * 0x100728C0 and this one).  0x10007F40 is the only writer.
 *
 * The original stores an ADDRESS: one of four objects at 0x10B71290,
 * 0x10B71338, 0x10B713E0, 0x10B71488 -- consecutive, stride 0xA8.  A host
 * dword cannot hold a host pointer, and the four objects are not ported, so
 * this models the choice as the INDEX.  The mapping falls out exactly:
 *
 *     ReadJoystick= 1 -> 0x10B71338 = base + 1*0xA8   index 1
 *     ReadJoystick= 2 -> 0x10B713E0 = base + 2*0xA8   index 2
 *     ReadJoystick= 3 -> 0x10B71488 = base + 3*0xA8   index 3
 *     anything else   -> 0x10B71290 = base + 0        index 0
 *
 * so index == value for 1..3 and 0 for everything else, INCLUDING values
 * above 3.  RallyMain passes 0x10B71290 -- index 0 -- to 0x10063060 at
 * 0x1001CD0D regardless of what this chose.
 * ------------------------------------------------------------------ */
#define BR_CFG_INPUT_BASE    0x10B71290u
#define BR_CFG_INPUT_STRIDE  0xA8u
#define BR_CFG_INPUT_COUNT   4

extern int32_t g_iBrCfgInputDevice;     /* 0x10B71534, as an index 0..3 */

/* The original address of device `i`, for anyone checking against a listing. */
uint32_t BrCfgInputDeviceAddr(int32_t i);

/* Reset every global this module owns to its load-time value, so a test can
 * parse more than once.  Not in the original -- the original gets fresh .data
 * from the loader.  It also resets the two aliased globals owned elsewhere,
 * because this module is their only writer. */
void BrAppCfgResetForTest(void);

#endif /* BR_APPSTART_H */
