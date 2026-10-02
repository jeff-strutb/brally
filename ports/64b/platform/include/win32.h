/* win32.h: the part of the Win32 API the game core names.
 *
 * The core was written against Win32 (plus DirectSound, MCI, mmio and the
 * registry). The portable build keeps those calls exactly as the game makes
 * them; each OS's platform layer implements them. This header is that
 * contract: only what the core uses, every integer fixed-width at the width
 * the original had, so LP64 (macOS, Linux) and LLP64 (Windows x64) agree.
 * Handle and message-parameter types are pointer-width, as in Win64.
 *
 * Struct layouts and COM vtable orders follow the Windows SDK, so a Windows
 * build can hand these same declarations to the real system.
 */
#ifndef BR_WIN32_H
#define BR_WIN32_H

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- calling conventions: meaningful on 32-bit x86 only ------------------ */
#if !defined(_M_IX86) && !defined(__i386__)
#ifndef __stdcall
#define __stdcall
#endif
#ifndef __cdecl
#define __cdecl
#endif
#ifndef __fastcall
#define __fastcall
#endif
#ifndef __thiscall
#define __thiscall
#endif
#endif
#define WINAPI      __stdcall
#define CALLBACK    __stdcall
#define APIENTRY    __stdcall
#define WINAPIV     __cdecl
#define STDMETHODCALLTYPE __stdcall
#ifndef _CRTIMP
#define _CRTIMP
#endif
#define FAR
#define NEAR
#define CONST const
#define VOID void

/* ---- base types ---------------------------------------------------------- */
typedef uint8_t   BYTE;
typedef uint16_t  WORD;
typedef uint32_t  DWORD;
typedef int32_t   BOOL;
typedef int32_t   LONG;
typedef uint32_t  ULONG;
typedef uint32_t  UINT;
typedef int32_t   INT;
typedef int16_t   SHORT;
typedef uint16_t  USHORT;
typedef char      CHAR;
typedef uint8_t   UCHAR;
typedef int64_t   LONGLONG;
typedef uint64_t  ULONGLONG;
typedef int64_t   __int64_t_win;
typedef float     FLOAT;
typedef intptr_t  INT_PTR;
typedef uintptr_t UINT_PTR;
typedef intptr_t  LONG_PTR;
typedef uintptr_t ULONG_PTR;
typedef uintptr_t DWORD_PTR;
typedef size_t    SIZE_T;
typedef UINT_PTR  WPARAM;
typedef LONG_PTR  LPARAM;
typedef LONG_PTR  LRESULT;
typedef int32_t   HRESULT;
typedef WORD      ATOM;
typedef DWORD     COLORREF;

typedef void       *LPVOID, *PVOID;
typedef const void *LPCVOID;
typedef BYTE       *LPBYTE, *PBYTE;
typedef WORD       *LPWORD;
typedef DWORD      *LPDWORD, *PDWORD;
typedef LONG       *LPLONG;
typedef BOOL       *LPBOOL;
typedef int        *LPINT;
typedef CHAR       *LPSTR, *PSTR, *NPSTR;
typedef const CHAR *LPCSTR, *PCSTR;
typedef CHAR       *HPSTR;
typedef LPSTR       LPTSTR;
typedef LPCSTR      LPCTSTR;

#ifndef TRUE
#define TRUE  1
#endif
#ifndef FALSE
#define FALSE 0
#endif
#ifndef NULL
#define NULL  ((void *)0)
#endif
#ifndef MAX_PATH
#define MAX_PATH 260
#endif
#define INFINITE 0xFFFFFFFFu

#define LOWORD(l) ((WORD)((DWORD_PTR)(l) & 0xFFFF))
#define HIWORD(l) ((WORD)(((DWORD_PTR)(l) >> 16) & 0xFFFF))
#define LOBYTE(w) ((BYTE)((DWORD_PTR)(w) & 0xFF))
#define HIBYTE(w) ((BYTE)(((DWORD_PTR)(w) >> 8) & 0xFF))
#define MAKEWORD(a, b) ((WORD)(((BYTE)(a)) | ((WORD)((BYTE)(b))) << 8))
#define MAKELONG(a, b) ((LONG)(((WORD)(a)) | ((DWORD)((WORD)(b))) << 16))
#define RGB(r, g, b) ((COLORREF)(((BYTE)(r) | ((WORD)((BYTE)(g)) << 8)) | (((DWORD)(BYTE)(b)) << 16)))

/* ---- handles: opaque, pointer-width -------------------------------------- */
#define BR_DECLARE_HANDLE(n) struct n##__ { int unused; }; typedef struct n##__ *n
typedef void *HANDLE;
typedef HANDLE *LPHANDLE, *PHANDLE;
BR_DECLARE_HANDLE(HWND);
BR_DECLARE_HANDLE(HINSTANCE);
BR_DECLARE_HANDLE(HKEY);
BR_DECLARE_HANDLE(HICON);
BR_DECLARE_HANDLE(HBRUSH);
BR_DECLARE_HANDLE(HMENU);
BR_DECLARE_HANDLE(HDC);
BR_DECLARE_HANDLE(HBITMAP);
BR_DECLARE_HANDLE(HGDIOBJ_);
BR_DECLARE_HANDLE(HMMIO);
BR_DECLARE_HANDLE(HTASK);
BR_DECLARE_HANDLE(HACMOBJ);
typedef HINSTANCE HMODULE;
typedef HICON     HCURSOR;
typedef void     *HGDIOBJ;
typedef HANDLE    HGLOBAL;
typedef HANDLE    HLOCAL;
typedef HKEY     *PHKEY;
typedef UINT      MMRESULT;
typedef DWORD     MCIERROR;
typedef UINT      MCIDEVICEID;
typedef DWORD     FOURCC;

typedef INT_PTR (WINAPI *FARPROC)(void);
typedef LRESULT (CALLBACK *WNDPROC)(HWND, UINT, WPARAM, LPARAM);
typedef void    (CALLBACK *TIMERPROC)(HWND, UINT, UINT_PTR, DWORD);
typedef DWORD   (WINAPI *LPTHREAD_START_ROUTINE)(LPVOID);
typedef DWORD   (WINAPI *PTHREAD_START_ROUTINE)(LPVOID);

/* ---- structs ------------------------------------------------------------- */
typedef struct tagPOINT { LONG x, y; } POINT, *LPPOINT, *PPOINT;
typedef struct tagRECT  { LONG left, top, right, bottom; } RECT, *LPRECT, *PRECT;
typedef const RECT *LPCRECT;

typedef union _LARGE_INTEGER {
    struct { DWORD LowPart; LONG HighPart; } u;
    struct { DWORD LowPart; LONG HighPart; };
    LONGLONG QuadPart;
} LARGE_INTEGER, *PLARGE_INTEGER;

typedef struct _GUID {
    uint32_t Data1;
    uint16_t Data2;
    uint16_t Data3;
    uint8_t  Data4[8];
} GUID, *LPGUID;
typedef GUID IID, CLSID;
typedef IID *LPIID;
typedef CLSID *LPCLSID;
#ifdef __cplusplus
#define REFGUID  const GUID &
#define REFIID   const IID &
#define REFCLSID const CLSID &
#else
#define REFGUID  const GUID *
#define REFIID   const IID *
#define REFCLSID const CLSID *
#endif
#define DEFINE_GUID(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
    const GUID name = { l, w1, w2, { b1, b2, b3, b4, b5, b6, b7, b8 } }

typedef struct _SECURITY_ATTRIBUTES {
    DWORD  nLength;
    LPVOID lpSecurityDescriptor;
    BOOL   bInheritHandle;
} SECURITY_ATTRIBUTES, *LPSECURITY_ATTRIBUTES, *PSECURITY_ATTRIBUTES;

/* opaque to the core; the platform layer owns what is inside */
typedef struct _RTL_CRITICAL_SECTION { void *opaque[8]; } CRITICAL_SECTION,
    *LPCRITICAL_SECTION, *PCRITICAL_SECTION;

typedef struct _MEMORYSTATUS {
    DWORD  dwLength;
    DWORD  dwMemoryLoad;
    SIZE_T dwTotalPhys;
    SIZE_T dwAvailPhys;
    SIZE_T dwTotalPageFile;
    SIZE_T dwAvailPageFile;
    SIZE_T dwTotalVirtual;
    SIZE_T dwAvailVirtual;
} MEMORYSTATUS, *LPMEMORYSTATUS;

typedef struct _OSVERSIONINFOA {
    DWORD dwOSVersionInfoSize;
    DWORD dwMajorVersion;
    DWORD dwMinorVersion;
    DWORD dwBuildNumber;
    DWORD dwPlatformId;
    CHAR  szCSDVersion[128];
} OSVERSIONINFOA, OSVERSIONINFO, *LPOSVERSIONINFOA, *LPOSVERSIONINFO;
#define VER_PLATFORM_WIN32s        0
#define VER_PLATFORM_WIN32_WINDOWS 1
#define VER_PLATFORM_WIN32_NT      2

typedef struct tagMSG {
    HWND   hwnd;
    UINT   message;
    WPARAM wParam;
    LPARAM lParam;
    DWORD  time;
    POINT  pt;
} MSG, *LPMSG, *PMSG;

typedef struct tagWNDCLASSA {
    UINT      style;
    WNDPROC   lpfnWndProc;
    int       cbClsExtra;
    int       cbWndExtra;
    HINSTANCE hInstance;
    HICON     hIcon;
    HCURSOR   hCursor;
    HBRUSH    hbrBackground;
    LPCSTR    lpszMenuName;
    LPCSTR    lpszClassName;
} WNDCLASSA, WNDCLASS, *LPWNDCLASSA, *LPWNDCLASS;

/* ---- constants ----------------------------------------------------------- */
#define ERROR_SUCCESS          0L
#define ERROR_ALREADY_EXISTS   183L
#define WAIT_OBJECT_0          0x00000000u
#define WAIT_ABANDONED         0x00000080u
#define WAIT_TIMEOUT           0x00000102u
#define WAIT_FAILED            0xFFFFFFFFu
#define INVALID_HANDLE_VALUE   ((HANDLE)(LONG_PTR)-1)

#define GMEM_FIXED     0x0000
#define GMEM_MOVEABLE  0x0002
#define GMEM_ZEROINIT  0x0040
#define GMEM_SHARE     0x2000
#define GMEM_DDESHARE  0x2000
#define GHND           (GMEM_MOVEABLE | GMEM_ZEROINIT)
#define GPTR           (GMEM_FIXED | GMEM_ZEROINIT)

#define HKEY_CLASSES_ROOT    ((HKEY)(ULONG_PTR)0x80000000u)
#define HKEY_CURRENT_USER    ((HKEY)(ULONG_PTR)0x80000001u)
#define HKEY_LOCAL_MACHINE   ((HKEY)(ULONG_PTR)0x80000002u)
#define KEY_QUERY_VALUE      0x0001
#define KEY_READ             0x20019
#define KEY_ALL_ACCESS       0xF003F
#define REG_SZ               1
#define REG_BINARY           3
#define REG_DWORD            4

#define DRIVE_UNKNOWN     0
#define DRIVE_NO_ROOT_DIR 1
#define DRIVE_REMOVABLE   2
#define DRIVE_FIXED       3
#define DRIVE_REMOTE      4
#define DRIVE_CDROM       5
#define DRIVE_RAMDISK     6

#define DLL_PROCESS_DETACH 0
#define DLL_PROCESS_ATTACH 1
#define DLL_THREAD_ATTACH  2
#define DLL_THREAD_DETACH  3

#define MAKEINTRESOURCEA(i) ((LPSTR)((ULONG_PTR)((WORD)(i))))
#define MAKEINTRESOURCE MAKEINTRESOURCEA
#define IDC_ARROW       MAKEINTRESOURCE(32512)
#define IDI_APPLICATION MAKEINTRESOURCE(32512)
#define IMAGE_BITMAP    0
#define IMAGE_ICON      1
#define IMAGE_CURSOR    2
#define LR_DEFAULTCOLOR     0x0000
#define LR_LOADFROMFILE     0x0010
#define LR_CREATEDIBSECTION 0x2000
#define BLACK_BRUSH     4
#define NULL_BRUSH      5

#define CS_VREDRAW 0x0001
#define CS_HREDRAW 0x0002
#define CS_OWNDC   0x0020
#define WS_OVERLAPPED       0x00000000L
#define WS_POPUP            0x80000000L
#define WS_VISIBLE          0x10000000L
#define WS_CAPTION          0x00C00000L
#define WS_SYSMENU          0x00080000L
#define WS_MINIMIZEBOX      0x00020000L
#define WS_MAXIMIZEBOX      0x00010000L
#define WS_THICKFRAME       0x00040000L
#define WS_OVERLAPPEDWINDOW 0x00CF0000L
#define WS_EX_TOPMOST       0x00000008L
#define WS_EX_APPWINDOW     0x00040000L
#define CW_USEDEFAULT       ((int)0x80000000)
#define GWL_WNDPROC   (-4)
#define GWL_STYLE     (-16)
#define GWL_EXSTYLE   (-20)
#define GWL_USERDATA  (-21)
#define SW_HIDE            0
#define SW_SHOWNORMAL      1
#define SW_NORMAL          1
#define SW_SHOWMINIMIZED   2
#define SW_SHOWMAXIMIZED   3
#define SW_SHOW            5
#define SW_MINIMIZE        6
#define SW_RESTORE         9
#define SW_SHOWDEFAULT     10
#define PM_NOREMOVE 0x0000
#define PM_REMOVE   0x0001
#define MB_OK              0x00000000L
#define MB_OKCANCEL        0x00000001L
#define MB_YESNO           0x00000004L
#define MB_ICONHAND        0x00000010L
#define MB_ICONERROR       0x00000010L
#define MB_ICONSTOP        0x00000010L
#define MB_ICONQUESTION    0x00000020L
#define MB_ICONEXCLAMATION 0x00000030L
#define MB_ICONINFORMATION 0x00000040L
#define MB_SETFOREGROUND   0x00010000L
#define MB_TOPMOST         0x00040000L
#define MB_SYSTEMMODAL     0x00001000L
#define IDOK     1
#define IDCANCEL 2
#define IDYES    6
#define IDNO     7

#define WM_NULL          0x0000
#define WM_CREATE        0x0001
#define WM_DESTROY       0x0002
#define WM_MOVE          0x0003
#define WM_SIZE          0x0005
#define WM_ACTIVATE      0x0006
#define WM_SETFOCUS      0x0007
#define WM_KILLFOCUS     0x0008
#define WM_PAINT         0x000F
#define WM_CLOSE         0x0010
#define WM_QUIT          0x0012
#define WM_ERASEBKGND    0x0014
#define WM_SHOWWINDOW    0x0018
#define WM_ACTIVATEAPP   0x001C
#define WM_SETCURSOR     0x0020
#define WM_KEYDOWN       0x0100
#define WM_KEYUP         0x0101
#define WM_CHAR          0x0102
#define WM_SYSKEYDOWN    0x0104
#define WM_SYSKEYUP      0x0105
#define WM_SYSCHAR       0x0106
#define WM_COMMAND       0x0111
#define WM_SYSCOMMAND    0x0112
#define WM_TIMER         0x0113
#define WM_MOUSEMOVE     0x0200
#define WM_LBUTTONDOWN   0x0201
#define WM_LBUTTONUP     0x0202
#define WM_RBUTTONDOWN   0x0204
#define WM_RBUTTONUP     0x0205
#define WM_USER          0x0400
#define SC_SCREENSAVE    0xF140
#define SC_MONITORPOWER  0xF170
#define SC_KEYMENU       0xF100
#define WA_INACTIVE      0

#define VK_BACK    0x08
#define VK_TAB     0x09
#define VK_RETURN  0x0D
#define VK_SHIFT   0x10
#define VK_CONTROL 0x11
#define VK_MENU    0x12
#define VK_PAUSE   0x13
#define VK_ESCAPE  0x1B
#define VK_SPACE   0x20
#define VK_PRIOR   0x21
#define VK_NEXT    0x22
#define VK_END     0x23
#define VK_HOME    0x24
#define VK_LEFT    0x25
#define VK_UP      0x26
#define VK_RIGHT   0x27
#define VK_DOWN    0x28
#define VK_INSERT  0x2D
#define VK_DELETE  0x2E
#define VK_F1      0x70
#define VK_F12     0x7B

/* ---- COM ----------------------------------------------------------------- */
#define S_OK          ((HRESULT)0)
#define S_FALSE       ((HRESULT)1)
#define E_FAIL        ((HRESULT)0x80004005)
#define E_NOINTERFACE ((HRESULT)0x80004002)
#define E_OUTOFMEMORY ((HRESULT)0x8007000E)
#define SUCCEEDED(hr) (((HRESULT)(hr)) >= 0)
#define FAILED(hr)    (((HRESULT)(hr)) < 0)
#define CLSCTX_INPROC_SERVER 0x1
#define CLSCTX_ALL           0x17

typedef struct IUnknown IUnknown, *LPUNKNOWN;
struct IUnknownVtbl {
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(IUnknown *, REFIID, void **);
    ULONG   (STDMETHODCALLTYPE *AddRef)(IUnknown *);
    ULONG   (STDMETHODCALLTYPE *Release)(IUnknown *);
};
struct IUnknown { const struct IUnknownVtbl *lpVtbl; };

/* ---- kernel32 / user32 / gdi32 / advapi32 --------------------------------- */
HANDLE  WINAPI CreateEventA(LPSECURITY_ATTRIBUTES, BOOL, BOOL, LPCSTR);
HANDLE  WINAPI CreateMutexA(LPSECURITY_ATTRIBUTES, BOOL, LPCSTR);
HANDLE  WINAPI CreateThread(LPSECURITY_ATTRIBUTES, SIZE_T, LPTHREAD_START_ROUTINE,
                            LPVOID, DWORD, LPDWORD);
BOOL    WINAPI CloseHandle(HANDLE);
BOOL    WINAPI SetEvent(HANDLE);
BOOL    WINAPI ReleaseMutex(HANDLE);
DWORD   WINAPI WaitForSingleObject(HANDLE, DWORD);
DWORD   WINAPI WaitForMultipleObjects(DWORD, const HANDLE *, BOOL, DWORD);
void    WINAPI ExitThread(DWORD);
void    WINAPI Sleep(DWORD);
void    WINAPI InitializeCriticalSection(LPCRITICAL_SECTION);
void    WINAPI DeleteCriticalSection(LPCRITICAL_SECTION);
void    WINAPI EnterCriticalSection(LPCRITICAL_SECTION);
void    WINAPI LeaveCriticalSection(LPCRITICAL_SECTION);
BOOL    WINAPI QueryPerformanceCounter(LARGE_INTEGER *);
BOOL    WINAPI QueryPerformanceFrequency(LARGE_INTEGER *);
HGLOBAL WINAPI GlobalAlloc(UINT, SIZE_T);
HGLOBAL WINAPI GlobalFree(HGLOBAL);
HGLOBAL WINAPI GlobalHandle(LPCVOID);
LPVOID  WINAPI GlobalLock(HGLOBAL);
BOOL    WINAPI GlobalUnlock(HGLOBAL);
void    WINAPI GlobalMemoryStatus(LPMEMORYSTATUS);
HMODULE WINAPI GetModuleHandleA(LPCSTR);
HMODULE WINAPI LoadLibraryA(LPCSTR);
BOOL    WINAPI FreeLibrary(HMODULE);
FARPROC WINAPI GetProcAddress(HMODULE, LPCSTR);
BOOL    WINAPI DisableThreadLibraryCalls(HMODULE);
BOOL    WINAPI GetVersionExA(LPOSVERSIONINFOA);
UINT    WINAPI GetDriveTypeA(LPCSTR);
BOOL    WINAPI GetVolumeInformationA(LPCSTR, LPSTR, DWORD, LPDWORD, LPDWORD,
                                     LPDWORD, LPSTR, DWORD);
BOOL    WINAPI GetUserNameA(LPSTR, LPDWORD);
void    WINAPI OutputDebugStringA(LPCSTR);
LPSTR   WINAPI lstrcpyA(LPSTR, LPCSTR);
int     WINAPI lstrlenA(LPCSTR);
int     WINAPIV wsprintfA(LPSTR, LPCSTR, ...);
int     WINAPI LoadStringA(HINSTANCE, UINT, LPSTR, int);
#define lstrcpy   lstrcpyA
#define lstrlen   lstrlenA
#define wsprintf  wsprintfA
#define LoadString LoadStringA
#define GetVersionEx GetVersionExA
#define GetModuleHandle GetModuleHandleA
#define LoadLibrary LoadLibraryA
#define CreateEvent CreateEventA
#define CreateMutex CreateMutexA
#define OutputDebugString OutputDebugStringA
#define GetDriveType GetDriveTypeA
#define GetVolumeInformation GetVolumeInformationA
#define GetUserName GetUserNameA

ATOM    WINAPI RegisterClassA(const WNDCLASSA *);
HWND    WINAPI CreateWindowExA(DWORD, LPCSTR, LPCSTR, DWORD, int, int, int, int,
                               HWND, HMENU, HINSTANCE, LPVOID);
LRESULT WINAPI DefWindowProcA(HWND, UINT, WPARAM, LPARAM);
BOOL    WINAPI ShowWindow(HWND, int);
BOOL    WINAPI UpdateWindow(HWND);
BOOL    WINAPI IsWindow(HWND);
BOOL    WINAPI IsIconic(HWND);
BOOL    WINAPI BringWindowToTop(HWND);
BOOL    WINAPI SetForegroundWindow(HWND);
HWND    WINAPI GetForegroundWindow(void);
HWND    WINAPI GetActiveWindow(void);
HWND    WINAPI GetDesktopWindow(void);
HWND    WINAPI GetLastActivePopup(HWND);
HWND    WINAPI SetFocus(HWND);
HWND    WINAPI FindWindowA(LPCSTR, LPCSTR);
LONG    WINAPI GetWindowLongA(HWND, int);
DWORD   WINAPI GetWindowThreadProcessId(HWND, LPDWORD);
BOOL    WINAPI InvalidateRect(HWND, const RECT *, BOOL);
BOOL    WINAPI GetMessageA(LPMSG, HWND, UINT, UINT);
BOOL    WINAPI PeekMessageA(LPMSG, HWND, UINT, UINT, UINT);
BOOL    WINAPI TranslateMessage(const MSG *);
LRESULT WINAPI DispatchMessageA(const MSG *);
BOOL    WINAPI PostMessageA(HWND, UINT, WPARAM, LPARAM);
void    WINAPI PostQuitMessage(int);
BOOL    WINAPI WaitMessage(void);
UINT    WINAPI RegisterWindowMessageA(LPCSTR);
UINT_PTR WINAPI SetTimer(HWND, UINT_PTR, UINT, TIMERPROC);
BOOL    WINAPI KillTimer(HWND, UINT_PTR);
SHORT   WINAPI GetAsyncKeyState(int);
HCURSOR WINAPI SetCursor(HCURSOR);
HCURSOR WINAPI LoadCursorA(HINSTANCE, LPCSTR);
HICON   WINAPI LoadIconA(HINSTANCE, LPCSTR);
HANDLE  WINAPI LoadImageA(HINSTANCE, LPCSTR, UINT, int, int, UINT);
int     WINAPI MessageBoxA(HWND, LPCSTR, LPCSTR, UINT);
HGDIOBJ WINAPI GetStockObject(int);
BOOL    WINAPI DeleteObject(HGDIOBJ);
int     WINAPI GetObjectA(HANDLE, int, LPVOID);
#define RegisterClass RegisterClassA
#define CreateWindowEx CreateWindowExA
#define DefWindowProc DefWindowProcA
#define FindWindow FindWindowA
#define GetWindowLong GetWindowLongA
#define GetMessage GetMessageA
#define PeekMessage PeekMessageA
#define DispatchMessage DispatchMessageA
#define PostMessage PostMessageA
#define RegisterWindowMessage RegisterWindowMessageA
#define LoadCursor LoadCursorA
#define LoadIcon LoadIconA
#define LoadImage LoadImageA
#define MessageBox MessageBoxA
#define GetObject GetObjectA

LONG    WINAPI RegOpenKeyExA(HKEY, LPCSTR, DWORD, DWORD, PHKEY);
LONG    WINAPI RegQueryValueExA(HKEY, LPCSTR, LPDWORD, LPDWORD, LPBYTE, LPDWORD);
LONG    WINAPI RegCloseKey(HKEY);
#define RegOpenKeyEx RegOpenKeyExA
#define RegQueryValueEx RegQueryValueExA

HRESULT WINAPI CoInitialize(LPVOID);
void    WINAPI CoUninitialize(void);
HRESULT WINAPI CoCreateInstance(REFCLSID, LPUNKNOWN, DWORD, REFIID, LPVOID *);

/* ---- DirectX entry points the game imports by name ------------------------ */
/* Each returns a COM object whose vtable order is the SDK's; the platform
 * layer implements the objects. */
HRESULT WINAPI DirectInputCreateA(HINSTANCE, DWORD, LPVOID *, LPUNKNOWN);
HRESULT WINAPI DirectPlayLobbyCreateA(LPGUID, LPVOID *, LPUNKNOWN, LPVOID, DWORD);   /* DPLAYX #4 */
#define DirectInputCreate DirectInputCreateA

/* ---- winmm: time, mmio, MCI (the SDK packs mmsystem.h to 1) --------------- */
DWORD    WINAPI timeGetTime(void);
MMRESULT WINAPI timeBeginPeriod(UINT);
MMRESULT WINAPI timeEndPeriod(UINT);
#define TIMERR_NOERROR 0
#define MMSYSERR_NOERROR 0

#define mmioFOURCC(a, b, c, d) \
    ((DWORD)(BYTE)(a) | ((DWORD)(BYTE)(b) << 8) | ((DWORD)(BYTE)(c) << 16) | ((DWORD)(BYTE)(d) << 24))
#define FOURCC_RIFF mmioFOURCC('R', 'I', 'F', 'F')
#define FOURCC_LIST mmioFOURCC('L', 'I', 'S', 'T')
#define MMIO_READ       0x00000000
#define MMIO_WRITE      0x00000001
#define MMIO_READWRITE  0x00000002
#define MMIO_ALLOCBUF   0x00010000
#define MMIO_DENYWRITE  0x00000020
#define MMIO_DIRTY      0x10000000
#define MMIO_FINDCHUNK  0x0010
#define MMIO_FINDRIFF   0x0020
#define MMIO_FINDLIST   0x0040
#define MMIO_CREATERIFF 0x0020
#define MMIOERR_BASE           256
#define MMIOERR_CHUNKNOTFOUND  (MMIOERR_BASE + 9)
#ifndef SEEK_SET
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#endif

#pragma pack(push, 1)
typedef LRESULT (CALLBACK *LPMMIOPROC)(LPSTR, UINT, LPARAM, LPARAM);
typedef struct _MMCKINFO {
    FOURCC ckid;
    DWORD  cksize;
    FOURCC fccType;
    DWORD  dwDataOffset;
    DWORD  dwFlags;
} MMCKINFO, *PMMCKINFO, *LPMMCKINFO;
typedef const MMCKINFO *LPCMMCKINFO;
typedef struct _MMIOINFO {
    DWORD      dwFlags;
    FOURCC     fccIOProc;
    LPMMIOPROC pIOProc;
    UINT       wErrorRet;
    HTASK      htask;
    LONG       cchBuffer;
    HPSTR      pchBuffer;
    HPSTR      pchNext;
    HPSTR      pchEndRead;
    HPSTR      pchEndWrite;
    LONG       lBufOffset;
    LONG       lDiskOffset;
    DWORD      adwInfo[3];
    DWORD      dwReserved1;
    DWORD      dwReserved2;
    HMMIO      hmmio;
} MMIOINFO, *PMMIOINFO, *LPMMIOINFO;
typedef const MMIOINFO *LPCMMIOINFO;

typedef struct waveformat_tag {
    WORD  wFormatTag;
    WORD  nChannels;
    DWORD nSamplesPerSec;
    DWORD nAvgBytesPerSec;
    WORD  nBlockAlign;
} WAVEFORMAT, *PWAVEFORMAT, *LPWAVEFORMAT;
typedef struct pcmwaveformat_tag {
    WAVEFORMAT wf;
    WORD       wBitsPerSample;
} PCMWAVEFORMAT, *PPCMWAVEFORMAT, *LPPCMWAVEFORMAT;
typedef struct tWAVEFORMATEX {
    WORD  wFormatTag;
    WORD  nChannels;
    DWORD nSamplesPerSec;
    DWORD nAvgBytesPerSec;
    WORD  nBlockAlign;
    WORD  wBitsPerSample;
    WORD  cbSize;
} WAVEFORMATEX, *PWAVEFORMATEX, *LPWAVEFORMATEX;
typedef const WAVEFORMATEX *LPCWAVEFORMATEX;
#define WAVE_FORMAT_PCM 1

typedef struct tagMCI_GENERIC_PARMS { DWORD_PTR dwCallback; } MCI_GENERIC_PARMS,
    *LPMCI_GENERIC_PARMS;
typedef struct tagMCI_OPEN_PARMSA {
    DWORD_PTR   dwCallback;
    MCIDEVICEID wDeviceID;
    LPCSTR      lpstrDeviceType;
    LPCSTR      lpstrElementName;
    LPCSTR      lpstrAlias;
} MCI_OPEN_PARMSA, MCI_OPEN_PARMS, *LPMCI_OPEN_PARMSA, *LPMCI_OPEN_PARMS;
typedef struct tagMCI_PLAY_PARMS {
    DWORD_PTR dwCallback;
    DWORD     dwFrom;
    DWORD     dwTo;
} MCI_PLAY_PARMS, *LPMCI_PLAY_PARMS;
typedef struct tagMCI_SEEK_PARMS {
    DWORD_PTR dwCallback;
    DWORD     dwTo;
} MCI_SEEK_PARMS, *LPMCI_SEEK_PARMS;
typedef struct tagMCI_STATUS_PARMS {
    DWORD_PTR dwCallback;
    DWORD_PTR dwReturn;
    DWORD     dwItem;
    DWORD     dwTrack;
} MCI_STATUS_PARMS, *LPMCI_STATUS_PARMS;
typedef struct tagMCI_SET_PARMS {
    DWORD_PTR dwCallback;
    DWORD     dwTimeFormat;
    DWORD     dwAudio;
} MCI_SET_PARMS, *LPMCI_SET_PARMS;
#pragma pack(pop)

HMMIO    WINAPI mmioOpenA(LPSTR, LPMMIOINFO, DWORD);
MMRESULT WINAPI mmioClose(HMMIO, UINT);
LONG     WINAPI mmioRead(HMMIO, HPSTR, LONG);
LONG     WINAPI mmioSeek(HMMIO, LONG, int);
MMRESULT WINAPI mmioGetInfo(HMMIO, LPMMIOINFO, UINT);
MMRESULT WINAPI mmioSetInfo(HMMIO, LPCMMIOINFO, UINT);
MMRESULT WINAPI mmioAdvance(HMMIO, LPMMIOINFO, UINT);
MMRESULT WINAPI mmioDescend(HMMIO, LPMMCKINFO, const MMCKINFO *, UINT);
MMRESULT WINAPI mmioAscend(HMMIO, LPMMCKINFO, UINT);
#define mmioOpen mmioOpenA

MCIERROR WINAPI mciSendCommandA(MCIDEVICEID, UINT, DWORD_PTR, DWORD_PTR);
#define mciSendCommand mciSendCommandA
#define MCI_OPEN              0x0803
#define MCI_CLOSE             0x0804
#define MCI_PLAY              0x0806
#define MCI_SEEK              0x0807
#define MCI_STOP              0x0808
#define MCI_PAUSE             0x0809
#define MCI_SET               0x080D
#define MCI_STATUS            0x0814
#define MCI_RESUME            0x0855
#define MCI_NOTIFY            0x00000001L
#define MCI_WAIT              0x00000002L
#define MCI_FROM              0x00000004L
#define MCI_TO                0x00000008L
#define MCI_TRACK             0x00000010L
#define MCI_OPEN_SHAREABLE    0x00000100L
#define MCI_OPEN_ELEMENT      0x00000200L
#define MCI_OPEN_ALIAS        0x00000400L
#define MCI_OPEN_TYPE_ID      0x00001000L
#define MCI_OPEN_TYPE         0x00002000L
#define MCI_SEEK_TO_START     0x00000100L
#define MCI_SEEK_TO_END       0x00000200L
#define MCI_STATUS_ITEM       0x00000100L
#define MCI_STATUS_START      0x00000200L
#define MCI_STATUS_LENGTH     0x00000001L
#define MCI_STATUS_POSITION   0x00000002L
#define MCI_STATUS_NUMBER_OF_TRACKS 0x00000003L
#define MCI_STATUS_MODE       0x00000004L
#define MCI_STATUS_MEDIA_PRESENT 0x00000005L
#define MCI_STATUS_TIME_FORMAT 0x00000006L
#define MCI_STATUS_READY      0x00000007L
#define MCI_STATUS_CURRENT_TRACK 0x00000008L
#define MCI_SET_DOOR_OPEN     0x00000100L
#define MCI_SET_DOOR_CLOSED   0x00000200L
#define MCI_SET_TIME_FORMAT   0x00000400L
#define MCI_SET_AUDIO         0x00000800L
#define MCI_SET_ON            0x00002000L
#define MCI_SET_OFF           0x00004000L
#define MCI_FORMAT_MILLISECONDS 0
#define MCI_FORMAT_MSF        2
#define MCI_FORMAT_TMSF       10
#define MCI_STRING_OFFSET     512
#define MCI_MODE_NOT_READY    (MCI_STRING_OFFSET + 12)
#define MCI_MODE_STOP         (MCI_STRING_OFFSET + 13)
#define MCI_MODE_PLAY         (MCI_STRING_OFFSET + 14)
#define MCI_MODE_RECORD       (MCI_STRING_OFFSET + 15)
#define MCI_MODE_SEEK         (MCI_STRING_OFFSET + 16)
#define MCI_MODE_PAUSE        (MCI_STRING_OFFSET + 17)
#define MCI_MODE_OPEN         (MCI_STRING_OFFSET + 18)
#define MCI_DEVTYPE_CD_AUDIO  516
#define MCI_MAKE_TMSF(t, m, s, f) \
    ((DWORD)(((BYTE)(t) | ((WORD)(m) << 8)) | (((DWORD)(BYTE)(s) | ((WORD)(f) << 8)) << 16)))
#define MCI_TMSF_TRACK(t)  ((BYTE)(t))
#define MCI_TMSF_MINUTE(t) ((BYTE)(((WORD)(t)) >> 8))
#define MCI_TMSF_SECOND(t) ((BYTE)((t) >> 16))
#define MCI_TMSF_FRAME(t)  ((BYTE)((t) >> 24))
#define MCI_MSF_MINUTE(t)  ((BYTE)(t))
#define MCI_MSF_SECOND(t)  ((BYTE)(((WORD)(t)) >> 8))
#define MCI_MSF_FRAME(t)   ((BYTE)((t) >> 16))

/* ---- msacm --------------------------------------------------------------- */
MMRESULT WINAPI acmMetrics(HACMOBJ, UINT, LPVOID);
#define ACM_METRIC_MAX_SIZE_FORMAT 50

/* ---- DirectSound (SDK vtable order) --------------------------------------- */
#define DS_OK                 S_OK
#define DSSCL_NORMAL          0x00000001
#define DSSCL_PRIORITY        0x00000002
#define DSSCL_EXCLUSIVE       0x00000003
#define DSSCL_WRITEPRIMARY    0x00000004
#define DSBCAPS_PRIMARYBUFFER 0x00000001
#define DSBCAPS_STATIC        0x00000002
#define DSBCAPS_LOCHARDWARE   0x00000004
#define DSBCAPS_LOCSOFTWARE   0x00000008
#define DSBCAPS_CTRLFREQUENCY 0x00000020
#define DSBCAPS_CTRLPAN       0x00000040
#define DSBCAPS_CTRLVOLUME    0x00000080
#define DSBCAPS_CTRLDEFAULT   0x000000E0
#define DSBCAPS_STICKYFOCUS   0x00004000
#define DSBCAPS_GLOBALFOCUS   0x00008000
#define DSBCAPS_GETCURRENTPOSITION2 0x00010000
#define DSBPLAY_LOOPING       0x00000001
#define DSBSTATUS_PLAYING     0x00000001
#define DSBSTATUS_BUFFERLOST  0x00000002
#define DSBSTATUS_LOOPING     0x00000004
#define DSBLOCK_FROMWRITECURSOR 0x00000001
#define DSBLOCK_ENTIREBUFFER  0x00000002
#define DSBVOLUME_MIN         (-10000)
#define DSBVOLUME_MAX         0
#define DSBPAN_LEFT           (-10000)
#define DSBPAN_CENTER         0
#define DSBPAN_RIGHT          10000
#define DSBFREQUENCY_ORIGINAL 0
#define DSERR_BUFFERLOST      ((HRESULT)0x88780096)

typedef struct _DSBUFFERDESC {
    DWORD          dwSize;
    DWORD          dwFlags;
    DWORD          dwBufferBytes;
    DWORD          dwReserved;
    LPWAVEFORMATEX lpwfxFormat;
} DSBUFFERDESC, *LPDSBUFFERDESC;
typedef const DSBUFFERDESC *LPCDSBUFFERDESC;
typedef struct _DSCAPS  { DWORD dwSize; DWORD dwFlags; DWORD dwRest[22]; } DSCAPS, *LPDSCAPS;
typedef struct _DSBCAPS { DWORD dwSize; DWORD dwFlags; DWORD dwBufferBytes;
                          DWORD dwUnlockTransferRate; DWORD dwPlayCpuOverhead; } DSBCAPS, *LPDSBCAPS;

typedef struct IDirectSound IDirectSound, *LPDIRECTSOUND;
typedef struct IDirectSoundBuffer IDirectSoundBuffer, *LPDIRECTSOUNDBUFFER;
struct IDirectSoundVtbl {
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(IDirectSound *, REFIID, void **);
    ULONG   (STDMETHODCALLTYPE *AddRef)(IDirectSound *);
    ULONG   (STDMETHODCALLTYPE *Release)(IDirectSound *);
    HRESULT (STDMETHODCALLTYPE *CreateSoundBuffer)(IDirectSound *, LPCDSBUFFERDESC,
                                                   LPDIRECTSOUNDBUFFER *, LPUNKNOWN);
    HRESULT (STDMETHODCALLTYPE *GetCaps)(IDirectSound *, LPDSCAPS);
    HRESULT (STDMETHODCALLTYPE *DuplicateSoundBuffer)(IDirectSound *, LPDIRECTSOUNDBUFFER,
                                                      LPDIRECTSOUNDBUFFER *);
    HRESULT (STDMETHODCALLTYPE *SetCooperativeLevel)(IDirectSound *, HWND, DWORD);
    HRESULT (STDMETHODCALLTYPE *Compact)(IDirectSound *);
    HRESULT (STDMETHODCALLTYPE *GetSpeakerConfig)(IDirectSound *, LPDWORD);
    HRESULT (STDMETHODCALLTYPE *SetSpeakerConfig)(IDirectSound *, DWORD);
    HRESULT (STDMETHODCALLTYPE *Initialize)(IDirectSound *, const GUID *);
};
struct IDirectSound { const struct IDirectSoundVtbl *lpVtbl; };
struct IDirectSoundBufferVtbl {
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(IDirectSoundBuffer *, REFIID, void **);
    ULONG   (STDMETHODCALLTYPE *AddRef)(IDirectSoundBuffer *);
    ULONG   (STDMETHODCALLTYPE *Release)(IDirectSoundBuffer *);
    HRESULT (STDMETHODCALLTYPE *GetCaps)(IDirectSoundBuffer *, LPDSBCAPS);
    HRESULT (STDMETHODCALLTYPE *GetCurrentPosition)(IDirectSoundBuffer *, LPDWORD, LPDWORD);
    HRESULT (STDMETHODCALLTYPE *GetFormat)(IDirectSoundBuffer *, LPWAVEFORMATEX, DWORD, LPDWORD);
    HRESULT (STDMETHODCALLTYPE *GetVolume)(IDirectSoundBuffer *, LPLONG);
    HRESULT (STDMETHODCALLTYPE *GetPan)(IDirectSoundBuffer *, LPLONG);
    HRESULT (STDMETHODCALLTYPE *GetFrequency)(IDirectSoundBuffer *, LPDWORD);
    HRESULT (STDMETHODCALLTYPE *GetStatus)(IDirectSoundBuffer *, LPDWORD);
    HRESULT (STDMETHODCALLTYPE *Initialize)(IDirectSoundBuffer *, LPDIRECTSOUND, LPCDSBUFFERDESC);
    HRESULT (STDMETHODCALLTYPE *Lock)(IDirectSoundBuffer *, DWORD, DWORD, LPVOID *, LPDWORD,
                                      LPVOID *, LPDWORD, DWORD);
    HRESULT (STDMETHODCALLTYPE *Play)(IDirectSoundBuffer *, DWORD, DWORD, DWORD);
    HRESULT (STDMETHODCALLTYPE *SetCurrentPosition)(IDirectSoundBuffer *, DWORD);
    HRESULT (STDMETHODCALLTYPE *SetFormat)(IDirectSoundBuffer *, LPCWAVEFORMATEX);
    HRESULT (STDMETHODCALLTYPE *SetVolume)(IDirectSoundBuffer *, LONG);
    HRESULT (STDMETHODCALLTYPE *SetPan)(IDirectSoundBuffer *, LONG);
    HRESULT (STDMETHODCALLTYPE *SetFrequency)(IDirectSoundBuffer *, DWORD);
    HRESULT (STDMETHODCALLTYPE *Stop)(IDirectSoundBuffer *);
    HRESULT (STDMETHODCALLTYPE *Unlock)(IDirectSoundBuffer *, LPVOID, DWORD, LPVOID, DWORD);
    HRESULT (STDMETHODCALLTYPE *Restore)(IDirectSoundBuffer *);
};
struct IDirectSoundBuffer { const struct IDirectSoundBufferVtbl *lpVtbl; };

#define IDirectSound_QueryInterface(p, a, b)     (p)->lpVtbl->QueryInterface(p, a, b)
#define IDirectSound_AddRef(p)                   (p)->lpVtbl->AddRef(p)
#define IDirectSound_Release(p)                  (p)->lpVtbl->Release(p)
#define IDirectSound_CreateSoundBuffer(p, a, b, c) (p)->lpVtbl->CreateSoundBuffer(p, a, b, c)
#define IDirectSound_GetCaps(p, a)               (p)->lpVtbl->GetCaps(p, a)
#define IDirectSound_DuplicateSoundBuffer(p, a, b) (p)->lpVtbl->DuplicateSoundBuffer(p, a, b)
#define IDirectSound_SetCooperativeLevel(p, a, b) (p)->lpVtbl->SetCooperativeLevel(p, a, b)
#define IDirectSound_Compact(p)                  (p)->lpVtbl->Compact(p)
#define IDirectSound_Initialize(p, a)            (p)->lpVtbl->Initialize(p, a)
#define IDirectSoundBuffer_Release(p)            (p)->lpVtbl->Release(p)
#define IDirectSoundBuffer_GetCaps(p, a)         (p)->lpVtbl->GetCaps(p, a)
#define IDirectSoundBuffer_GetCurrentPosition(p, a, b) (p)->lpVtbl->GetCurrentPosition(p, a, b)
#define IDirectSoundBuffer_GetStatus(p, a)       (p)->lpVtbl->GetStatus(p, a)
#define IDirectSoundBuffer_Lock(p, a, b, c, d, e, f, g) (p)->lpVtbl->Lock(p, a, b, c, d, e, f, g)
#define IDirectSoundBuffer_Play(p, a, b, c)      (p)->lpVtbl->Play(p, a, b, c)
#define IDirectSoundBuffer_SetCurrentPosition(p, a) (p)->lpVtbl->SetCurrentPosition(p, a)
#define IDirectSoundBuffer_SetFormat(p, a)       (p)->lpVtbl->SetFormat(p, a)
#define IDirectSoundBuffer_SetVolume(p, a)       (p)->lpVtbl->SetVolume(p, a)
#define IDirectSoundBuffer_SetPan(p, a)          (p)->lpVtbl->SetPan(p, a)
#define IDirectSoundBuffer_SetFrequency(p, a)    (p)->lpVtbl->SetFrequency(p, a)
#define IDirectSoundBuffer_Stop(p)               (p)->lpVtbl->Stop(p)
#define IDirectSoundBuffer_Unlock(p, a, b, c, d) (p)->lpVtbl->Unlock(p, a, b, c, d)
#define IDirectSoundBuffer_Restore(p)            (p)->lpVtbl->Restore(p)

extern const GUID CLSID_DirectSound;
extern const GUID IID_IDirectSound;

/* ---- MSVC C runtime extras (the platform layer supplies them) ------------- */
typedef uint32_t _fsize_t;
struct _finddata_t {
    unsigned  attrib;
    int64_t   time_create;
    int64_t   time_access;
    int64_t   time_write;
    _fsize_t  size;
    char      name[260];
};
#define _A_NORMAL 0x00
#define _A_RDONLY 0x01
#define _A_HIDDEN 0x02
#define _A_SYSTEM 0x04
#define _A_SUBDIR 0x10
#define _A_ARCH   0x20
intptr_t __cdecl _findfirst(const char *, struct _finddata_t *);
int      __cdecl _findnext(intptr_t, struct _finddata_t *);
int      __cdecl _findclose(intptr_t);
char *   __cdecl _itoa(int, char *, int);
char *   __cdecl _ltoa(long, char *, int);
char *   __cdecl _strupr(char *);
char *   __cdecl _strlwr(char *);
int      __cdecl _stricmp(const char *, const char *);
int      __cdecl _strnicmp(const char *, const char *, size_t);
int      __cdecl _chdir(const char *);
char *   __cdecl _getcwd(char *, int);
int      __cdecl _getdrive(void);
int      __cdecl _chdrive(int);
int      __cdecl _finite(double);
#define itoa    _itoa
#define stricmp _stricmp
#define strupr  _strupr

#ifdef __cplusplus
}
#endif
#endif /* BR_WIN32_H */
