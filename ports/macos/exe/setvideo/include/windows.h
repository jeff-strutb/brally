/* windows.h — minimal shim so the byte-matched SetVideo.exe translation units
 * compile on macOS/clang unchanged.
 *
 * NOT byte-matched, NOT part of the decomp. This header exists only so the
 * matched sources under src/exe/setvideo/ can be compiled verbatim by a
 * non-Windows toolchain; nothing here is @implements-tagged and the match
 * tooling never sees it.
 *
 * Only the portable 29 of SetVideo's 42 functions are built against it. The
 * ten dialog/registry functions are not ported (there is no Win32 message
 * loop here and the .rsrc dialog templates were never extracted from the
 * original binary), so USER32/ADVAPI32 are declared, never defined.
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

typedef int                 BOOL;
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

/* --- The one Win32 call the portable functions actually make ------------ */

/* Every CHK_* helper traces through OutputDebugStringA when gChkVerbose is
 * set. Routed to stderr. */
void OutputDebugStringA(const char *s);

#endif /* BR_PORT_WINDOWS_H */
