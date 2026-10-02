/* win_kernel.c: kernel32 for the game -- events, mutexes, threads, critical
 * sections, timers, global memory, modules and the system queries.
 *
 * Every waitable object shares one lock and one condition: a state change
 * wakes every waiter, and each re-checks what it waits for. The game waits
 * on a handful of objects at a time, so this is simple and enough. */
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "plat.h"

int g_plat_log;

/* ---- waitable objects ---------------------------------------------------------- */
enum { K_EVENT = 0x4B45, K_MUTEX, K_THREAD };

typedef struct kobj {
    int        type;
    int        signaled;       /* event: set; thread: finished */
    int        manual;         /* event: manual reset */
    uintptr_t  owner;          /* mutex: owning thread, 0 = free */
    int        depth;          /* mutex: recursion */
    LPTHREAD_START_ROUTINE fn; /* thread */
    LPVOID     arg;
    DWORD      code;
} kobj;

static host_mutex *s_k;
static host_cond  *s_kc;

static void kinit(void)
{
    if (!s_k) {
        s_k = host_mutex_new();
        s_kc = host_cond_new();
    }
}

/* Handles are small numbers, as on Windows (whose handles always fit in 32
 * bits): the game keeps them in 32-bit fields of its records. A handle is
 * (slot + 1) * 4 in a table of objects. */
#define KMAX 4096
static kobj *s_ktab[KMAX];
static int   s_kn;

static kobj *knew(int type)
{
    kobj *o = (kobj *)calloc(1, sizeof *o);
    kinit();
    o->type = type;
    return o;
}

static HANDLE khandle(kobj *o)
{
    int i;
    host_mutex_lock(s_k);
    i = s_kn < KMAX ? s_kn++ : -1;
    if (i >= 0)
        s_ktab[i] = o;
    host_mutex_unlock(s_k);
    return i < 0 ? NULL : (HANDLE)(uintptr_t)((i + 1) * 4);
}

static kobj *kget(HANDLE h)
{
    uint32_t v = (uint32_t)(uintptr_t)h;
    if (v == 0 || (v & 3) != 0 || v / 4 > (uint32_t)s_kn)
        return NULL;
    return s_ktab[v / 4 - 1];
}

HANDLE WINAPI CreateEventA(LPSECURITY_ATTRIBUTES sa, BOOL manual, BOOL initial, LPCSTR name)
{
    kobj *o = knew(K_EVENT);
    (void)sa;
    (void)name;
    o->manual = manual;
    o->signaled = initial;
    return khandle(o);
}

HANDLE WINAPI CreateMutexA(LPSECURITY_ATTRIBUTES sa, BOOL owned, LPCSTR name)
{
    kobj *o = knew(K_MUTEX);
    (void)sa;
    (void)name;
    if (owned) {
        o->owner = host_thread_self();
        o->depth = 1;
    }
    return khandle(o);
}

BOOL WINAPI SetEvent(HANDLE h)
{
    kobj *o = kget(h);
    if (!o || o->type != K_EVENT)
        return FALSE;
    host_mutex_lock(s_k);
    o->signaled = 1;
    host_cond_broadcast(s_kc);
    host_mutex_unlock(s_k);
    return TRUE;
}

BOOL WINAPI ReleaseMutex(HANDLE h)
{
    kobj *o = kget(h);
    BOOL ok = FALSE;
    if (!o || o->type != K_MUTEX)
        return FALSE;
    host_mutex_lock(s_k);
    if (o->owner == host_thread_self() && o->depth > 0) {
        if (--o->depth == 0)
            o->owner = 0;
        host_cond_broadcast(s_kc);
        ok = TRUE;
    }
    host_mutex_unlock(s_k);
    return ok;
}

/* can this thread take o now (and if so, take it) -- under s_k */
static int ktake(kobj *o, int take)
{
    uintptr_t self = host_thread_self();
    switch (o->type) {
    case K_EVENT:
        if (!o->signaled)
            return 0;
        if (take && !o->manual)
            o->signaled = 0;
        return 1;
    case K_MUTEX:
        if (o->owner != 0 && o->owner != self)
            return 0;
        if (take) {
            o->owner = self;
            o->depth++;
        }
        return 1;
    case K_THREAD:
        return o->signaled;
    }
    return 0;
}

DWORD WINAPI WaitForMultipleObjects(DWORD n, const HANDLE *h, BOOL all, DWORD ms)
{
    uint64_t end = ms == INFINITE ? 0 : host_ticks_ns() + (uint64_t)ms * 1000000u;
    DWORD i, r = WAIT_TIMEOUT;
    kinit();
    /* an invalid handle fails the wait at once, as on Windows */
    for (i = 0; i < n; i++) {
        kobj *o = kget(h[i]);
        if (!o || (o->type != K_EVENT && o->type != K_MUTEX && o->type != K_THREAD))
            return WAIT_FAILED;
    }
    host_mutex_lock(s_k);
    for (;;) {
        if (all) {
            for (i = 0; i < n && ktake(kget(h[i]), 0); i++)
                ;
            if (i == n) {
                for (i = 0; i < n; i++)
                    ktake(kget(h[i]), 1);
                r = WAIT_OBJECT_0;
                break;
            }
        } else {
            for (i = 0; i < n; i++)
                if (ktake(kget(h[i]), 1))
                    break;
            if (i < n) {
                r = WAIT_OBJECT_0 + i;
                break;
            }
        }
        if (ms == 0)
            break;
        if (ms == INFINITE) {
            host_cond_wait(s_kc, s_k, INFINITE);
        } else {
            uint64_t now = host_ticks_ns();
            if (now >= end)
                break;
            host_cond_wait(s_kc, s_k, (uint32_t)((end - now) / 1000000u) + 1);
        }
    }
    host_mutex_unlock(s_k);
    return r;
}

DWORD WINAPI WaitForSingleObject(HANDLE h, DWORD ms)
{
    if (!h)
        return WAIT_FAILED;
    return WaitForMultipleObjects(1, &h, FALSE, ms);
}

static void *kthread_main(void *p)
{
    kobj *o = (kobj *)p;
    DWORD code = o->fn(o->arg);
    host_mutex_lock(s_k);
    o->code = code;
    o->signaled = 1;
    host_cond_broadcast(s_kc);
    host_mutex_unlock(s_k);
    return NULL;
}

HANDLE WINAPI CreateThread(LPSECURITY_ATTRIBUTES sa, SIZE_T stack, LPTHREAD_START_ROUTINE fn,
                           LPVOID arg, DWORD flags, LPDWORD id)
{
    kobj *o = knew(K_THREAD);
    (void)sa;
    (void)stack;
    (void)flags;
    o->fn = fn;
    o->arg = arg;
    if (!host_thread_start(kthread_main, o)) {
        free(o);
        return NULL;
    }
    {
        HANDLE hT = khandle(o);
        if (id)
            *id = (DWORD)(uintptr_t)hT;
        return hT;
    }
}

void WINAPI ExitThread(DWORD code)
{
    (void)code;
    host_thread_exit();
}

BOOL WINAPI CloseHandle(HANDLE h)
{
    /* Objects stay allocated: a thread may still signal one that the game
     * has closed, and the game creates few of them. */
    return kget(h) != NULL;
}

void WINAPI Sleep(DWORD ms)
{
    plat_pump(0);
    host_sleep_ms(ms);
}

/* ---- critical sections ------------------------------------------------------------ */
void WINAPI InitializeCriticalSection(LPCRITICAL_SECTION cs) { cs->opaque[0] = host_mutex_new(); }
void WINAPI DeleteCriticalSection(LPCRITICAL_SECTION cs)
{
    host_mutex_free((host_mutex *)cs->opaque[0]);
    cs->opaque[0] = NULL;
}
void WINAPI EnterCriticalSection(LPCRITICAL_SECTION cs) { host_mutex_lock((host_mutex *)cs->opaque[0]); }
void WINAPI LeaveCriticalSection(LPCRITICAL_SECTION cs) { host_mutex_unlock((host_mutex *)cs->opaque[0]); }

/* ---- time ------------------------------------------------------------------------------ */
static uint64_t s_t0;
static uint64_t since_start(void)
{
    if (!s_t0)
        s_t0 = host_ticks_ns();
    return host_ticks_ns() - s_t0;
}

/* the performance counter at a PC-typical 1.193182 MHz, so values the game
 * keeps in 32 bits wrap no sooner than they did on the original */
#define PERF_HZ 1193182u
BOOL WINAPI QueryPerformanceFrequency(LARGE_INTEGER *f)
{
    f->QuadPart = PERF_HZ;
    return TRUE;
}

BOOL WINAPI QueryPerformanceCounter(LARGE_INTEGER *c)
{
    uint64_t ns = since_start();
    c->QuadPart = (LONGLONG)(ns / 1000000000u * PERF_HZ + ns % 1000000000u * PERF_HZ / 1000000000u);
    return TRUE;
}

DWORD WINAPI timeGetTime(void) { return (DWORD)(since_start() / 1000000u); }
MMRESULT WINAPI timeBeginPeriod(UINT p) { (void)p; return TIMERR_NOERROR; }
MMRESULT WINAPI timeEndPeriod(UINT p)   { (void)p; return TIMERR_NOERROR; }

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

UINT WINAPI GetDriveTypeA(LPCSTR root)
{
    int d = root && root[0] && root[1] == ':' ? toupper((unsigned char)root[0]) - 'A' + 1 : plat_drive();
    if (d == PLAT_CD_DRIVE)
        return DRIVE_CDROM;
    if (d == 3)
        return DRIVE_FIXED;
    return DRIVE_NO_ROOT_DIR;
}

BOOL WINAPI GetVolumeInformationA(LPCSTR root, LPSTR label, DWORD nlabel, LPDWORD serial,
                                  LPDWORD maxlen, LPDWORD flags, LPSTR fs, DWORD nfs)
{
    int d = root && root[0] && root[1] == ':' ? toupper((unsigned char)root[0]) - 'A' + 1 : plat_drive();
    if (label && nlabel)
        snprintf(label, nlabel, "%s", d == PLAT_CD_DRIVE ? PLAT_CD_LABEL : "");
    if (serial)
        *serial = 0x19980000u;
    if (maxlen)
        *maxlen = 255;
    if (flags)
        *flags = 0;
    if (fs && nfs)
        snprintf(fs, nfs, "%s", d == PLAT_CD_DRIVE ? "CDFS" : "FAT32");
    return d == PLAT_CD_DRIVE || d == 3;
}

BOOL WINAPI GetUserNameA(LPSTR out, LPDWORD n)
{
    const char *u = getenv("USER");
    if (!u)
        u = "Player";
    if (!out || !n || *n <= strlen(u))
        return FALSE;
    strcpy(out, u);
    *n = (DWORD)strlen(u) + 1;
    return TRUE;
}

void WINAPI OutputDebugStringA(LPCSTR s) { PLOG("%s", s ? s : ""); }
LPSTR WINAPI lstrcpyA(LPSTR d, LPCSTR s) { return strcpy(d, s); }
int   WINAPI lstrlenA(LPCSTR s) { return s ? (int)strlen(s) : 0; }

int WINAPIV wsprintfA(LPSTR out, LPCSTR fmt, ...)
{
    va_list ap;
    int n;
    va_start(ap, fmt);
    n = vsprintf(out, fmt, ap);
    va_end(ap);
    return n;
}
