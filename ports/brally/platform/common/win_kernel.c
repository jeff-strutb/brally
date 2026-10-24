/* win_kernel.c: what is left of kernel32 for the game -- global memory,
 * modules and the version query. */
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "plat.h"


/* ---- global memory: fixed blocks, the handle is the pointer ---------------------------- */
HGLOBAL WINAPI GlobalAlloc(UINT flags, SIZE_T n)
{
    void *p = (flags & GMEM_ZEROINIT) ? calloc(1, n ? n : 1) : malloc(n ? n : 1);
    return p;
}
HGLOBAL WINAPI GlobalFree(HGLOBAL h) { free(h); return NULL; }
HGLOBAL WINAPI GlobalHandle(LPCVOID p) { return (HGLOBAL)p; }
LPVOID  WINAPI GlobalLock(HGLOBAL h)   { return h; }
BOOL    WINAPI GlobalUnlock(HGLOBAL h) { (void)h; return TRUE; }

void WINAPI GlobalMemoryStatus(LPMEMORYSTATUS ms)
{
    /* what a 1998 machine reported: 128 MB, mostly free */
    ms->dwLength = sizeof *ms;
    ms->dwMemoryLoad = 20;
    ms->dwTotalPhys = 128u << 20;
    ms->dwAvailPhys = 96u << 20;
    ms->dwTotalPageFile = 256u << 20;
    ms->dwAvailPageFile = 200u << 20;
    ms->dwTotalVirtual = 2047u << 20;
    ms->dwAvailVirtual = 1900u << 20;
}

/* ---- modules ---------------------------------------------------------------------------- */
typedef struct pmod {
    char name[64];
    const plat_export *ex;
    int n;
    void *pe;                /* a resource DLL from disc */
    int loaded;              /* LoadLibraryA has been called for it */
} pmod;
static pmod s_mod[16];
static int s_nmod;
static int s_instance;      /* the address GetModuleHandle(NULL) names */

void plat_register_module(const char *dll, const plat_export *ex, int n)
{
    if (s_nmod < 16) {
        snprintf(s_mod[s_nmod].name, sizeof s_mod[0].name, "%s", dll);
        s_mod[s_nmod].ex = ex;
        s_mod[s_nmod].n = n;
        s_nmod++;
    }
}

static const char *base_name(const char *p)
{
    const char *s = p;
    for (; *p; p++)
        if (*p == '\\' || *p == '/' || *p == ':')
            s = p + 1;
    return s;
}

HMODULE WINAPI GetModuleHandleA(LPCSTR name)
{
    int i;
    /* the process itself, or a module already loaded */
    if (!name || !strcasecmp(base_name(name), "BRally.exe") || !strcasecmp(base_name(name), "BRGlide.dll"))
        return (HMODULE)&s_instance;
    for (i = 0; i < s_nmod; i++)
        if (s_mod[i].loaded && !strcasecmp(s_mod[i].name, base_name(name)))
            return (HMODULE)&s_mod[i];
    return NULL;
}

HMODULE WINAPI LoadLibraryA(LPCSTR name)
{
    int i;
    char host[1024];
    if (!name)
        return NULL;
    for (i = 0; i < s_nmod; i++)
        if (!strcasecmp(s_mod[i].name, base_name(name))) {
            s_mod[i].loaded = 1;
            PLOG("LoadLibraryA(%s): the platform's\n", name);
            return (HMODULE)&s_mod[i];
        }
    /* a resource-only DLL the game reads strings from */
    if (plat_path(name, host, sizeof host)) {
        void *pe = plat_pe_open(host);
        if (pe && s_nmod < 16) {
            snprintf(s_mod[s_nmod].name, sizeof s_mod[0].name, "%s", base_name(name));
            s_mod[s_nmod].pe = pe;
            s_mod[s_nmod].loaded = 1;
            return (HMODULE)&s_mod[s_nmod++];
        }
    }
    PLOG("LoadLibraryA(%s): not available\n", name);
    return NULL;
}

BOOL WINAPI FreeLibrary(HMODULE h) { (void)h; return TRUE; }

FARPROC WINAPI GetProcAddress(HMODULE h, LPCSTR name)
{
    pmod *m = (pmod *)h;
    int i;
    if (!m || m < s_mod || m >= s_mod + 16 || (uintptr_t)name < 0x10000)
        return NULL;
    for (i = 0; i < m->n; i++)
        if (!strcmp(m->ex[i].name, name))
            return (FARPROC)m->ex[i].fn;
    PLOG("GetProcAddress(%s, %s): not exported\n", m->name, name);
    return NULL;
}

BOOL WINAPI DisableThreadLibraryCalls(HMODULE h) { (void)h; return TRUE; }

int WINAPI LoadStringA(HINSTANCE h, UINT id, LPSTR out, int n)
{
    pmod *m = (pmod *)h;
    if (!m || m < s_mod || m >= s_mod + 16 || !m->pe || n <= 0)
        return 0;
    return plat_pe_string(m->pe, id, out, n);
}

/* ---- system queries ----------------------------------------------------------------------- */
BOOL WINAPI GetVersionExA(LPOSVERSIONINFOA v)
{
    /* Windows 98 */
    v->dwMajorVersion = 4;
    v->dwMinorVersion = 10;
    v->dwBuildNumber = 1998;
    v->dwPlatformId = VER_PLATFORM_WIN32_WINDOWS;
    v->szCSDVersion[0] = 0;
    return TRUE;
}


int WINAPIV wsprintfA(LPSTR out, LPCSTR fmt, ...)
{
    va_list ap;
    int n;
    va_start(ap, fmt);
    n = vsprintf(out, fmt, ap);
    va_end(ap);
    return n;
}
