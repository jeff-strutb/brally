/* windows.h — minimal shim so the byte-matched SetVideo.exe translation units
 * compile on macOS/clang unchanged.
 *
 * NOT byte-matched, NOT part of the decomp. This header exists only so the
 * matched sources under src/exe/setvideo/ can be compiled verbatim by a
 * non-Windows toolchain; nothing here is @implements-tagged and the match
 * tooling never sees it.
 *
 * Thirty-eight of SetVideo's 42 functions build against it, WinMain and all
 * five dialog procedures included. The user32 calls they make are declared
 * here and implemented on AppKit in win32_dialog.m; the registry lookup and
 * three CRT hooks are the only functions the port replaces outright.
 */
#ifndef BR_PORT_WINDOWS_H
#define BR_PORT_WINDOWS_H

/* Pull the real stdio in FIRST: the _flag compatibility macro below must not
 * be live while <stdio.h> itself is being parsed. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* --- MSVC CRT spellings ------------------------------------------------- */

/* GetIniValue() compares keys case-insensitively. */
#define _stricmp  strcasecmp
#define _strnicmp strncasecmp

/* ReadListLine() distinguishes "end of this include file" from "read error"
 * by testing MSVC's FILE::_flag against _IOEOF (0x10). BSD stdio spells the
 * same bit FILE::_flags & __SEOF (0x20), so rewrite the member access:
 *
 *     fp->_flag & 0x10   ->   fp->_flags & 0x20 ? 0x10 : 0 & 0x10
 *
 * `&` binds tighter than `?:`, so that is (flags & __SEOF) ? 0x10 : 0 — the
 * same truth value the original tests. */
#define _flag _flags & 0x20 ? 0x10 : 0

/* MSVC opens the device database in text mode ("rt") and its CRT collapses
 * each CRLF to a bare LF on the way through fgets. There is no text mode on
 * BSD, and the retail BossRally.vdb is a DOS file, so without this every
 * value keeps a trailing '\r' and — worse — every blank line arrives as the
 * one-character string "\r" instead of "\n". ReadList only drops lines of
 * one character, so those blanks become entries, and GetIniValue then finds
 * a line with no '=' and exits: "Unable to parse  in section [...]".
 *
 * CHK_FGets needs no equivalent: it reads with getc and already folds CR and
 * CRLF to LF itself. */
static inline char *br_port_fgets(char *s, int n, FILE *f)
{
    size_t len;

    if ((fgets)(s, n, f) == 0)
        return 0;
    len = strlen(s);
    if (len >= 2 && s[len - 2] == '\r' && s[len - 1] == '\n') {
        s[len - 2] = '\n';
        s[len - 1] = 0;
    }
    return s;
}
#define fgets br_port_fgets

/* --- ILP32 -> LP64 allocation sizes ------------------------------------- */

/* The matched code sizes its own allocations for a 32-bit target and gets
 * every one of them from CHK_AllocateMemory -> malloc:
 *
 *     CHK_AllocateMemory(8,       ...)  INI / Section / CHKFile  (ptr + int)
 *     CHK_AllocateMemory(0xc,     ...)  ObjList        (int + ptr + ptr)
 *     CHK_AllocateMemory(n * 4,   ...)  ObjList::rgsz  (n pointers)
 *     CHK_AllocateMemory(len + 1, ...)  a string
 *
 * On arm64 a pointer is 8 bytes, not 4, so the first three are half the size
 * they need to be and ReadList would run off the end of rgsz. Doubling every
 * request is exactly the right correction for the three pointer-bearing
 * cases (8->16, 0xc->24, n*4->n*8) and harmless slack for the strings.
 *
 * Done here rather than in the matched sources so those stay untouched. */
static inline void *br_port_malloc(unsigned long n) { return (malloc)(2 * n); }
#define malloc(n) br_port_malloc((unsigned long)(n))

/* --- Win32 types -------------------------------------------------------- */

/* Objective-C already has a BOOL and it is not an int; the shim's own API
 * below uses plain int so the C and ObjC halves agree on return width. */
#ifndef __OBJC__
typedef int                 BOOL;
#endif
typedef unsigned char       BYTE;
typedef unsigned short      WORD;
typedef unsigned int        DWORD;
typedef long                LONG;
typedef unsigned int        UINT;
typedef unsigned long       WPARAM;
typedef long                LPARAM;
typedef char               *LPSTR;
typedef const char         *LPCSTR;
typedef void               *HANDLE;
typedef void               *HWND;
typedef void               *HINSTANCE;
typedef void               *HKEY;

#define __stdcall
#define MAKEINTRESOURCE(i) ((LPSTR)(unsigned long)(WORD)(i))

/* --- Messages, ids and control messages the dialog procedures use ------- */

#define WM_INITDIALOG   0x0110
#define WM_COMMAND      0x0111

#define IDOK            1
#define IDCANCEL        2

#define CB_ADDSTRING    0x0143
#define CB_GETCURSEL    0x0147
#define CB_SETCURSEL    0x014E
#define CB_GETITEMDATA  0x0150
#define CB_SETITEMDATA  0x0151

#define DWL_USER        8       /* SetWindowLongA(hWnd, 8, lParam) */

typedef int (*DLGPROC)(HWND, UINT, WPARAM, LPARAM);

/* --- The Win32 surface, reimplemented on AppKit in win32_dialog.m ------- */

/* Every CHK_* helper traces through OutputDebugStringA when gChkVerbose is
 * set. Routed to stderr. */
void OutputDebugStringA(const char *s);

/* This is the whole of Win32 that SetVideo.exe needs. The eleven calls below
 * are implemented against AppKit so the original dialog procedures and
 * WinMain run unmodified; nothing else from user32 is referenced anywhere in
 * the binary's game code. */
LPARAM SetWindowLongA(HWND hWnd, int index, LPARAM value);
LPARAM GetWindowLongA(HWND hWnd, int index);
LPARAM SendDlgItemMessageA(HWND hWnd, int id, UINT msg,
                           WPARAM wParam, LPARAM lParam);
HWND   GetDlgItem(HWND hWnd, int id);
int    EndDialog(HWND hWnd, int result);
int    CheckDlgButton(HWND hWnd, int id, UINT check);
UINT   IsDlgButtonChecked(HWND hWnd, int id);
int    CheckRadioButton(HWND hWnd, int first, int last, int check);
int    DialogBoxParamA(HINSTANCE hInst, LPSTR templ, HWND parent,
                       DLGPROC proc, LPARAM lParam);
HWND   GetDesktopWindow(void);
int    MessageBoxA(HWND hWnd, LPCSTR text, LPCSTR caption, UINT type);

#endif /* BR_PORT_WINDOWS_H */
