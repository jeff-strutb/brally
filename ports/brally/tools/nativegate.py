#!/usr/bin/env python3
"""nativegate.py -- the Boss Rally port talks to its own platform layer only.

Fails (exit 1, one line a hit) if the port's code -- comments and strings
aside -- names any of the foreign APIs the original was written against:
Win32 and its types, COM and DirectX, Glide 2, winmm/MCI, or the MSVC
runtime's extras. The OS hosts (platform/host/windows, platform/host/win32)
are the port's own Windows backends and call Windows itself; they are not
scanned. Run from anywhere: tools/nativegate.py"""
import os
import re
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
SCAN = ['src', 'include', 'platform/common', 'platform/include', 'platform/render']
SKIP = ('platform/host/', 'platform/render/gl/gl_ctx.c')   # the port's own OS backends

WIN32 = r'''CreateWindowExA RegisterClassA DefWindowProcA PostMessageA PostQuitMessage GetMessageA PeekMessageA
TranslateMessage DispatchMessageA WaitMessage SetTimer KillTimer GetAsyncKeyState ShowWindow UpdateWindow
SetFocus IsIconic IsWindow GetActiveWindow GetForegroundWindow SetForegroundWindow BringWindowToTop
GetDesktopWindow GetWindowLongA GetWindowThreadProcessId FindWindowA GetLastActivePopup InvalidateRect
SetCursor LoadCursorA LoadIconA LoadImageA GetObjectA DeleteObject GetStockObject MessageBoxA
RegisterWindowMessageA RegOpenKeyExA RegQueryValueExA RegCloseKey CreateMutexA CreateEventA CreateThread
SetEvent ReleaseMutex WaitForSingleObject WaitForMultipleObjects CloseHandle ExitThread Sleep
InitializeCriticalSection EnterCriticalSection LeaveCriticalSection DeleteCriticalSection
QueryPerformanceCounter QueryPerformanceFrequency timeGetTime timeBeginPeriod timeEndPeriod
GlobalAlloc GlobalFree GlobalLock GlobalUnlock GlobalHandle GlobalMemoryStatus GetModuleHandleA
LoadLibraryA FreeLibrary GetProcAddress DisableThreadLibraryCalls LoadStringA GetVersionExA
GetDriveTypeA GetVolumeInformationA GetUserNameA OutputDebugStringA lstrcpyA lstrlenA wsprintfA
mciSendCommandA mmioOpenA mmioClose mmioRead mmioSeek mmioDescend mmioAscend mmioGetInfo mmioSetInfo
mmioAdvance acmMetrics'''.split()
TYPES = r'''DWORD HANDLE HWND HINSTANCE HMODULE HGLOBAL HKEY HMMIO LPVOID LPCVOID LPSTR LPCSTR WPARAM LPARAM
LRESULT HRESULT BOOL UINT LONG ULONG WORD BYTE LARGE_INTEGER MMCKINFO WAVEFORMATEX MSG WNDCLASSA FARPROC
GUID REFIID CLSID INFINITE CRITICAL_SECTION LPDWORD'''.split()
CRT = r'''_itoa _ltoa _strupr _strlwr _stricmp _strnicmp _chdir _getcwd _getdrive _chdrive _findfirst
_findnext _findclose _finddata_t _errno _onexit dllonexit _except_handler3 _finite'''.split()
PATTERNS = [
    r'\b(?:%s)\b' % '|'.join(WIN32 + TYPES + CRT),
    r'\bI?Direct(?:Draw|Input|Sound|Play)\w*', r'\bLPDIRECT\w+', r'\bCo(?:CreateInstance|Initialize|Uninitialize)\b',
    r'\blpVtbl\b', r'\bQueryInterface\b', r'->tbl->', r'->vt->',
    r'\bgr[A-Z][A-Za-z]+\s*\(', r'\bgu(?:Fog|Tex)\w*', r'\bGr(?:Vertex|TexInfo|HwConfiguration|[A-Z]\w*_t)\b',
    r'\bGR_[A-Z_]+\b', r'\bFx(?:U8|U16|U32|I32|Bool|Float)\b',
    r'\b(?:DP|DPL|DI|DS|DD)(?:4|3)?_\w+\b', r'\bDPSESSIONDESC2\b', r'\bDPID\b', r'\bDPNAME\b',
]
RX = re.compile('|'.join('(?:%s)' % p for p in PATTERNS))
CODE = re.compile(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\\n])*"|\'(?:\\.|[^\'\\\n])*\'', re.S)


def main():
    hits = 0
    for top in SCAN:
        for d, _, fs in os.walk(os.path.join(ROOT, top)):
            for f in fs:
                if not f.endswith(('.c', '.cpp', '.h', '.m', '.mm')):
                    continue
                p = os.path.join(d, f)
                rel = os.path.relpath(p, ROOT)
                if rel.startswith(SKIP):
                    continue
                s = open(p, errors='ignore').read()
                code = CODE.sub(lambda m: re.sub(r'[^\n]', ' ', m.group(0)), s)
                for m in RX.finditer(code):
                    line = code.count('\n', 0, m.start()) + 1
                    print('%s:%d: %s' % (rel, line, m.group(0)))
                    hits += 1
    print('nativegate: %d foreign names' % hits)
    return 1 if hits else 0


sys.exit(main())
