/* Win32 type shim for the IDO/N64 cross-compile.
 *
 * The PC decomp's portable engine bodies include <windows.h> for its typedefs
 * even where they touch no Win32 API at all.  Compiling those files for MIPS
 * needs the type names and nothing else -- nothing here is ever linked, and
 * the N64 build of the same source obviously calls no Win32 function.  Widths
 * are chosen to match Win32 on x86 (all 32-bit), which is also what O32 MIPS
 * gives for int/long/pointer, so struct layouts carry over unchanged.
 */
#ifndef _WINDOWS_H_SHIM
#define _WINDOWS_H_SHIM

#include <stddef.h>

typedef int                 BOOL;
typedef unsigned char       BYTE;
typedef unsigned short      WORD;
typedef unsigned int        DWORD;
typedef unsigned int        UINT;
typedef int                 INT;
typedef long                LONG;
typedef unsigned long       ULONG;
typedef short               SHORT;
typedef unsigned short      USHORT;
typedef char                CHAR;
typedef unsigned char       UCHAR;
typedef float               FLOAT;
typedef void               *PVOID;
typedef void               *LPVOID;
typedef const void         *LPCVOID;
typedef char               *LPSTR;
typedef const char         *LPCSTR;
typedef char               *LPTSTR;
typedef const char         *LPCTSTR;
typedef BYTE               *LPBYTE;
typedef WORD               *LPWORD;
typedef DWORD              *LPDWORD;
typedef int                *LPINT;
typedef long               *LPLONG;
typedef unsigned int        SIZE_T;
typedef int                 INT_PTR;
typedef unsigned int        UINT_PTR;
typedef long                LONG_PTR;
typedef unsigned int        ULONG_PTR;
typedef unsigned int        DWORD_PTR;
typedef unsigned int        WPARAM;
typedef long                LPARAM;
typedef long                LRESULT;
typedef long                HRESULT;
typedef unsigned short      ATOM;
typedef unsigned char       TCHAR;

typedef void               *HANDLE;
typedef void               *HWND;
typedef void               *HDC;
typedef void               *HINSTANCE;
typedef void               *HMODULE;
typedef void               *HMENU;
typedef void               *HICON;
typedef void               *HCURSOR;
typedef void               *HBRUSH;
typedef void               *HBITMAP;
typedef void               *HPALETTE;
typedef void               *HFONT;
typedef void               *HRGN;
typedef void               *HKEY;
typedef void               *HGLOBAL;
typedef void               *HLOCAL;
typedef void               *HGDIOBJ;
typedef void               *HRSRC;
typedef HKEY               *PHKEY;

#define WINAPI
#define APIENTRY
#define CALLBACK
#define WINGDIAPI
#define PASCAL
#define FAR
#define NEAR
#define CONST const
#define VOID void
#define __declspec(x)
#define __stdcall
#define __cdecl
#define __fastcall
#define _stdcall
#define _cdecl

#ifndef TRUE
#define TRUE  1
#endif
#ifndef FALSE
#define FALSE 0
#endif
#ifndef NULL
#define NULL 0
#endif
#ifndef MAX_PATH
#define MAX_PATH 260
#endif

typedef struct tagPOINT   { LONG x, y; } POINT, *LPPOINT;
typedef struct tagSIZE    { LONG cx, cy; } SIZE, *LPSIZE;
typedef struct tagRECT    { LONG left, top, right, bottom; } RECT, *LPRECT;
typedef struct _FILETIME  { DWORD dwLowDateTime, dwHighDateTime; } FILETIME;
typedef struct _SYSTEMTIME {
    WORD wYear, wMonth, wDayOfWeek, wDay, wHour, wMinute, wSecond, wMilliseconds;
} SYSTEMTIME;
typedef struct _GUID {
    DWORD Data1; WORD Data2; WORD Data3; BYTE Data4[8];
} GUID, IID, CLSID, *LPGUID, *LPCLSID;
typedef const GUID *REFGUID, *REFIID, *REFCLSID;
typedef struct tagMSG {
    HWND hwnd; UINT message; WPARAM wParam; LPARAM lParam;
    DWORD time; POINT pt;
} MSG, *LPMSG;
typedef struct _RGNDATA { DWORD dummy; } RGNDATA;
typedef struct tagPALETTEENTRY {
    BYTE peRed, peGreen, peBlue, peFlags;
} PALETTEENTRY, *LPPALETTEENTRY;
typedef struct tagBITMAPINFOHEADER {
    DWORD biSize; LONG biWidth, biHeight; WORD biPlanes, biBitCount;
    DWORD biCompression, biSizeImage; LONG biXPelsPerMeter, biYPelsPerMeter;
    DWORD biClrUsed, biClrImportant;
} BITMAPINFOHEADER;
typedef struct tagRGBQUAD { BYTE rgbBlue, rgbGreen, rgbRed, rgbReserved; } RGBQUAD;
typedef struct tagBITMAPINFO {
    BITMAPINFOHEADER bmiHeader; RGBQUAD bmiColors[1];
} BITMAPINFO, *LPBITMAPINFO;

typedef LRESULT (*WNDPROC)(HWND, UINT, WPARAM, LPARAM);
typedef BOOL (*FARPROC)(void);

#endif /* _WINDOWS_H_SHIM */
