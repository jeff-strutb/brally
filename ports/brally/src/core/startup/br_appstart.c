/* br_appstart.c -- APPLICATION START-UP: admission, machine probe, settings.
 *
 * ARCHITECTURAL CONCERN: everything RallyMain does before it has a window.
 * See br_appstart.h for the key table, the two aliases, and the ESP trace that
 * decides what 0x10007F10 reads.
 *
 * Reference build: reference/brally/orig/BRGlide.dll.  Every address in this file is a Glide
 * address.  config/brally/shared.csv classes all three entry points `shared`:
 * 0x10007E80 <- D3D 0x10007B10, 0x10007F10 <- D3D 0x10007BA0,
 * 0x10007F40 <- D3D 0x10007BD0.
 */

/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
/* The port's BrChkFReadLine takes the FILE* itself; the original takes the
 * CHK handle whose first dword is the FILE*.  In the matching build the
 * header's prototype, the port body and its one caller in this file are
 * renamed out of the way, and the byte-exact form at the end of the file
 * takes the real name (the #undef is there). */
#define BrChkFReadLine BrChkFReadLine_port
#include "br_race.h"   /* br_globals: its objects */
#include "slice3_42.h"   /* br_globals: its objects */
#include "br_appstart.h"

#include <stdlib.h>
#include <string.h>
/* initialised in the original source (restored: the 64-bit core keeps them
 * private to this file; no Glide relocation places them elsewhere) */
char g_aBrCfgTrackDir [BR_APPCFG_DIR_MAX] = "tracks/";
char g_aBrCfgCarDir   [BR_APPCFG_DIR_MAX] = "cars/";

/* XSLICE 0x10003680 (D3D 0x10003320) -- CHK_FileExists, already ported in
 * port/src/slice1_01.c under its D3D address.  config/brally/shared.csv row 54 pairs
 * the two.  Reused rather than re-coined; see CONVENTIONS.md. */
/* BrChkFileExists: prototype in br_funcs.h */

/* The two globals this module writes that another module OWNS.  Declared, not
 * defined: the original has one object per address and so must this port. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */     /* 0x10226A48 -- port/src/br_racestep.c */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* 0x100B55F0 -- port/src/slice1_08.c   */

/* ==========================================================================
 * Storage.  Initialisers are BRGlide.dll's own .data bytes, read out of the
 * image; anything not listed here is .bss and starts at zero.
 * ========================================================================== */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                     /* 0x10226E78 */

/* 64-bit core: declared once, in br_globals.h or its struct's header */      /* 0x10B73540 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */         /* 0x10226A78 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */  /* 0x100B74C0 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* 0x100B7900 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */     /* 0x100B7D40 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */        /* 0x10B71648 */

/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x100B3014 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x10226E7C */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x10226E80 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x100A9360 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x10B71530 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x1007B320 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x1007B328 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x1007B32C */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x1007B324 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x100A5EAC */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x100B2E6C */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x10396EB0 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x118EEEDC */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x1007B074 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x1021CDF8 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x1021CE50 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x10226A40 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x10226A3C */

/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x10B71534, as an index */

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

/* (port-only BrAppStartSetHost removed) */


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

/* ==========================================================================
 * 0x10007F10 -- the machine probe.  See br_appstart.h for the ESP trace that
 * establishes index 2 (dwTotalPhys) rather than index 1 (dwMemoryLoad).
 * ========================================================================== */
#define BR_MEMSTATUS_LENGTH     0
#define BR_MEMSTATUS_TOTALPHYS  2

/* (port-only BrAppQueryTotalPhys removed) */


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
/* (port-only BrChkFReadLine removed) */


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
/* (port-only BrAppCfgBuildIniPath removed) */


/* 0x100080EA..0x1000812E (and its identical twin at 0x100085AD..0x100085E3).
 * `dec eax / je` three times: 1, 2, 3, then a default.  See br_appstart.h for
 * why index == value on 1..3 and 0 otherwise. */
/* (port-only cfg_select_input_device removed) */


/* (port-only BrCfgInputDeviceAddr removed) */


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
/* (port-only cfg_set_dir removed) */


/* --------------------------------------------------------------------------
 * The .ini half, 0x10007F40..0x100083F5.
 *
 * An ELSE-IF ladder over strncmp against the start of the line, so one line
 * sets at most one key and the first match wins.  The read loop is a
 * do-while: 0x10007FCB reads the first line and 0x100083DD reads the rest,
 * with 0x100083ED (CHK_FClose) as the single exit for both.
 * -------------------------------------------------------------------------- */
/* (port-only BrAppCfgParseIni removed) */


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
/* (port-only BrAppCfgParseCmdLine removed) */


/* @n64 0x8021DDFC located */
/* (port-only BrAppCfgParse removed) */


/* Not in the original: the original gets fresh .data from the loader. */
/* (port-only BrAppCfgResetForTest removed) */


/* -- Ghidra-matched functions --------------------------- */
#include <windows.h>
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* WHAT IT DOES: query total physical RAM via GlobalMemoryStatus and store it. */
/* @implements 0x10007F10 glide BrMemoryQuery */

int BrMemoryQuery(void)

{
  MEMORYSTATUS local_20;
  
  local_20.dwLength = 0x20;
  GlobalMemoryStatus(&local_20);
  (*(int *)&g_brTexSysMem) = local_20.dwTotalPhys;
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

/* ==========================================================================
 * 0x10003530 -- CHK_FReadLine, the byte-exact form.  The contract, the
 * stack trace and the two preserved defects are in the port body above and
 * in br_appstart.h; this differs from it only in taking the CHK handle and
 * reading the FILE* out of it at every use, as the original does.
 * ========================================================================== */
#undef BrChkFReadLine
/* The original imports `getc` and calls it (`mov ebp,[__imp__getc]`, hoisted
 * out of the loop); <stdio.h>'s getc MACRO would inline the buffer poke and
 * call _filbuf instead.  Only the macro is dropped -- the function prototype
 * above it in the header stays. */
#undef getc

struct BrChkHandle { FILE *pFile; };

/* WHAT IT DOES: reads one line of a CHK text file into the caller's buffer,
 * with the game's own conventions: a line always ends with a newline and a
 * NUL in the buffer, a carriage return counts as a newline and swallows the
 * line feed that follows it, and at end-of-file with nothing read it
 * returns NULL, which is how the caller's loop ends.  It returns a pointer
 * just past what it wrote. */
/* @implements 0x10003530 glide BrChkFReadLine */
char *BrChkFReadLine(char *pszDst, int cbMax, struct BrChkHandle *pChk)
{
    int n = 0;
    int c;

    /* Three things the bytes pin down:
     *  - the PARAMETER is the cursor.  A `p = pszDst` local hoists one load
     *    above the pushes; the original registerises the argument in the
     *    loop preheader and reloads it on the no-room path (0x100035CD).
     *  - a `while (n < cbMax)` loop: top `jle`, bottom `jl`, the count exit
     *    falling into its own `return`.  A `for(;;)` with an inner
     *    `if (n >= cbMax) return` folds that exit into the final return.
     *  - the CR arm BREAKS to the final `return`, which the no-room path
     *    shares (one epilogue, 0x100035D1); every other exit returns from
     *    where it stands, six four-pop epilogues in all.
     *  - EOF is the ELSE arm of `if (c != EOF)`, laid out last (0x10003587).
     *    As a leading `if (c == EOF)` its "store NUL, return +1" tail gets
     *    cross-jumped into the LF arm's identical tail: 11 bytes short. */
    while (n < cbMax) {
        c = getc(pChk->pFile);

        if (c != EOF) {
            if (c == '\r') {
                /* normalise to LF, then swallow a following LF or put back
                 * whatever else came */
                *pszDst++ = '\n';
                *pszDst++ = '\0';
                c = getc(pChk->pFile);
                if (c != EOF && c != '\n') {
                    ungetc(c, pChk->pFile);
                    return pszDst;
                }
                break;
            }

            if (c == '\n') {
                *pszDst++ = '\n';
                *pszDst   = '\0';
                return pszDst + 1;
            }

            *pszDst++ = (char)c;
            n++;
        } else {
            if (n == 0) {
                return NULL;
            }
            *pszDst = '\0';
            return pszDst + 1;
        }
    }
    return pszDst;
}

