/* host_win.c -- kernel32, user32, gdi32, advapi32, ole32, winmm, msacm32
 * as the game uses them (port code). Behaviour follows the live oracle
 * (tools/brbox_imports.py), which the T3 build was certified under.
 */
#include "host.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <mach/mach_time.h>

/* ============================================================== time == */
static u64 now_us(void)
{
    static mach_timebase_info_data_t tb;
    static u64 t0;
    u64 t = mach_absolute_time();
    if (!tb.denom) { mach_timebase_info(&tb); t0 = t; }
    return (t - t0) * tb.numer / tb.denom / 1000;
}
u32 h_timeGetTime(void) { return (u32)(now_us() / 1000) + 1000; }
u32 h_timeBeginPeriod(u32 p) { (void)p; return 0; }
void h_timeEndPeriod(u32 p) { (void)p; }
u32 h_QueryPerformanceFrequency(u32 p) { W_ST(u64, p, 0, 1000000ULL); return 1; }
u32 h_QueryPerformanceCounter(u32 p) { W_ST(u64, p, 0, now_us() + 1000000ULL); return 1; }
void h_Sleep(u32 ms)
{
    happ_pump(0);
    if (ms) usleep(ms * 1000);
}

/* ============================================================ handles == */
enum { HK_FREE, HK_EVENT, HK_MUTEX, HK_THREAD, HK_REG, HK_LIB, HK_OBJ };
typedef struct {
    int kind;
    int signalled, manual;
    pthread_t thr;
    u32 owner_count;
    pthread_t owner;
    const char *name;
} hobj;
static hobj g_h[1024];
static pthread_mutex_t g_hl = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_hc = PTHREAD_COND_INITIALIZER;
#define HBASE 0x00100000u

static u32 hnew(int kind)
{
    int i;
    pthread_mutex_lock(&g_hl);
    for (i = 1; i < 1024 && g_h[i].kind; i++) ;
    memset(&g_h[i], 0, sizeof g_h[i]);
    g_h[i].kind = kind;
    pthread_mutex_unlock(&g_hl);
    return HBASE + (u32)i * 4;
}
static hobj *hget(u32 h)
{
    u32 i = (h - HBASE) / 4;
    return (h >= HBASE && i < 1024 && g_h[i].kind) ? &g_h[i] : NULL;
}

u32 h_CloseHandle(u32 h)
{
    hobj *o = hget(h);
    if (!o) return 0;
    pthread_mutex_lock(&g_hl);
    if (o->kind != HK_THREAD) o->kind = HK_FREE;
    pthread_mutex_unlock(&g_hl);
    return 1;
}
u32 h_CreateEventA(u32 sa, u32 manual, u32 init, u32 name)
{
    u32 h = hnew(HK_EVENT);
    hobj *o = hget(h);
    (void)sa; (void)name;
    o->manual = (int)manual;
    o->signalled = init != 0;
    return h;
}
u32 h_SetEvent(u32 h)
{
    hobj *o = hget(h);
    if (!o) return 0;
    pthread_mutex_lock(&g_hl);
    o->signalled = 1;
    pthread_cond_broadcast(&g_hc);
    pthread_mutex_unlock(&g_hl);
    return 1;
}
u32 h_CreateMutexA(u32 sa, u32 own, u32 name)
{
    u32 h = hnew(HK_MUTEX);
    hobj *o = hget(h);
    (void)sa; (void)name;
    if (own) { o->owner = pthread_self(); o->owner_count = 1; }
    return h;
}
u32 h_ReleaseMutex(u32 h)
{
    hobj *o = hget(h);
    if (!o || !o->owner_count) return 0;
    pthread_mutex_lock(&g_hl);
    if (--o->owner_count == 0) pthread_cond_broadcast(&g_hc);
    pthread_mutex_unlock(&g_hl);
    return 1;
}

static int ready(hobj *o)
{
    switch (o->kind) {
    case HK_EVENT: return o->signalled;
    case HK_MUTEX: return !o->owner_count || pthread_equal(o->owner, pthread_self());
    case HK_THREAD: return o->signalled;
    default: return 1;
    }
}
static void consume(hobj *o)
{
    if (o->kind == HK_EVENT && !o->manual) o->signalled = 0;
    if (o->kind == HK_MUTEX) { o->owner = pthread_self(); o->owner_count++; }
}

static u32 wait_n(u32 n, const u32 *hs, int all, u32 ms)
{
    struct timespec ts;
    u64 dl = now_us() + (u64)ms * 1000;
    u32 i;
    pthread_mutex_lock(&g_hl);
    for (;;) {
        int nready = 0, first = -1;
        for (i = 0; i < n; i++) {
            hobj *o = hget(hs[i]);
            if (!o || ready(o)) { nready++; if (first < 0) first = (int)i; }
        }
        if ((all && nready == (int)n) || (!all && first >= 0)) {
            for (i = 0; i < n; i++) {
                hobj *o = hget(hs[i]);
                if (o && (all || (int)i == first)) consume(o);
            }
            pthread_mutex_unlock(&g_hl);
            return all ? 0 : (u32)first;
        }
        if (ms == 0 || (ms != 0xFFFFFFFFu && now_us() >= dl)) {
            pthread_mutex_unlock(&g_hl);
            return 0x102;                       /* WAIT_TIMEOUT */
        }
        {
            u64 t = now_us() + 5000;
            if (ms != 0xFFFFFFFFu && t > dl) t = dl;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_nsec += 5000000;
            if (ts.tv_nsec >= 1000000000) { ts.tv_sec++; ts.tv_nsec -= 1000000000; }
            pthread_cond_timedwait(&g_hc, &g_hl, &ts);
            (void)t;
        }
    }
}
u32 h_WaitForSingleObject(u32 h, u32 ms) { return wait_n(1, &h, 0, ms); }
u32 h_WaitForMultipleObjects(u32 n, u32 p, u32 all, u32 ms)
{
    u32 hs[64], i;
    if (n > 64) n = 64;
    for (i = 0; i < n; i++) hs[i] = H32(p + i * 4);
    return wait_n(n, hs, (int)all, ms);
}

/* critical sections: the game's CRITICAL_SECTION (24 bytes) holds our index */
static pthread_mutex_t g_cs[256];
static int g_ncs;
void h_InitializeCriticalSection(u32 cs)
{
    pthread_mutexattr_t a;
    int i;
    pthread_mutex_lock(&g_hl);
    i = ++g_ncs;
    pthread_mutex_unlock(&g_hl);
    pthread_mutexattr_init(&a);
    pthread_mutexattr_settype(&a, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&g_cs[i], &a);
    memset(W_P(cs), 0, 24);
    HW32(cs, 0xC5000000u | (u32)i);
}
static pthread_mutex_t *csof(u32 cs)
{
    u32 v = H32(cs);
    if ((v & 0xFF000000u) != 0xC5000000u) { h_InitializeCriticalSection(cs); v = H32(cs); }
    return &g_cs[v & 0xFF];
}
void h_EnterCriticalSection(u32 cs) { pthread_mutex_lock(csof(cs)); }
void h_LeaveCriticalSection(u32 cs) { pthread_mutex_unlock(csof(cs)); }
void h_DeleteCriticalSection(u32 cs) { (void)cs; }

/* threads: each on its own 1 MB shadow stack below the main one */
static int g_nthreads;
typedef struct { u32 start, param, sp; u32 h; } thr_arg;
static void *thr_main(void *p)
{
    thr_arg a = *(thr_arg *)p;
    u32 r;
    free(p);
    w_sp = a.sp;
    r = w_icall_i_i(a.start, a.param);
    pthread_mutex_lock(&g_hl);
    hget(a.h)->signalled = 1;
    pthread_cond_broadcast(&g_hc);
    pthread_mutex_unlock(&g_hl);
    (void)r;
    return NULL;
}
u32 h_CreateThread(u32 sa, u32 ss, u32 start, u32 param, u32 flags, u32 ptid)
{
    thr_arg *a = malloc(sizeof *a);
    u32 h = hnew(HK_THREAD);
    int k;
    (void)sa; (void)ss; (void)flags;
    pthread_mutex_lock(&g_hl);
    k = g_nthreads++;
    pthread_mutex_unlock(&g_hl);
    if (k >= 15) { fprintf(stderr, "CreateThread: out of shadow stacks\n"); return 0; }
    a->start = start;
    a->param = param;
    a->sp = 0x02000000u + (u32)(k + 1) * 0x100000u - 16;
    a->h = h;
    if (ptid) HW32(ptid, 0x1000 + k);
    HLOG("CreateThread(start=%08X param=%08X)\n", start, param);
    pthread_create(&hget(h)->thr, NULL, thr_main, a);
    return h;
}
void h_ExitThread(u32 code) { (void)code; pthread_exit(NULL); }
u32 h_DisableThreadLibraryCalls(u32 h) { (void)h; return 1; }

/* ============================================================ system == */
u32 h_GetVersionExA(u32 p)
{
    /* OSVERSIONINFOA: Windows 98 (4.10.1998), VER_PLATFORM_WIN32_WINDOWS */
    HW32(p + 4, 4);
    HW32(p + 8, 10);
    HW32(p + 12, 1998);
    HW32(p + 16, 1);
    memset(W_P(p + 20), 0, 128);
    return 1;
}
void h_GlobalMemoryStatus(u32 p)
{
    HW32(p + 0, 32);
    HW32(p + 4, 30);
    HW32(p + 8, 128u << 20);
    HW32(p + 12, 96u << 20);
    HW32(p + 16, 256u << 20);
    HW32(p + 20, 200u << 20);
    HW32(p + 24, 2047u << 20);
    HW32(p + 28, 1800u << 20);
}
u32 h_GlobalAlloc(u32 fl, u32 n) { return hmem_alloc(n, (fl & 0x40) != 0); }
u32 h_GlobalLock(u32 h) { return h; }
u32 h_GlobalUnlock(u32 h) { (void)h; return 0; }
u32 h_GlobalHandle(u32 p) { return p; }
u32 h_GlobalFree(u32 h) { hmem_free(h); return 0; }

u32 h_GetDriveTypeA(u32 p)
{
    char d = p ? HS(p)[0] : vfs_cwd()[0];
    d = (char)(d & ~0x20);
    return d == 'C' ? 3 : d == 'D' ? 5 : 1;
}
u32 h_GetVolumeInformationA(u32 root, u32 name, u32 nn, u32 serial, u32 maxc,
                            u32 flags, u32 fs, u32 nfs)
{
    char d = root ? (char)(HS(root)[0] & ~0x20) : vfs_cwd()[0];
    if (d != 'C' && d != 'D') return 0;
    if (name) snprintf(HS(name), nn, "%s", d == 'D' ? "Boss Rally" : "SYSTEM");
    if (serial) HW32(serial, d == 'D' ? 0x12345678 : 0x1C0FFEE);
    if (maxc) HW32(maxc, 255);
    if (flags) HW32(flags, 0);
    if (fs) snprintf(HS(fs), nfs, "%s", d == 'D' ? "CDFS" : "FAT32");
    return 1;
}
u32 h_GetUserNameA(u32 buf, u32 pn)
{
    if (H32(pn) < 7) { HW32(pn, 7); return 0; }
    strcpy(HS(buf), "Player");
    HW32(pn, 7);
    return 1;
}

/* registry: the install path, as the installer wrote it */
u32 h_RegOpenKeyExA(u32 k, u32 sub, u32 opt, u32 sam, u32 out)
{
    (void)opt; (void)sam;
    if (k == 0x80000002u && sub && !strcasecmp(HS(sub), "software\\southpeak interactive\\boss rally")) {
        HW32(out, hnew(HK_REG));
        return 0;
    }
    HLOG("RegOpenKeyExA(%08X, %s) -> not found\n", k, sub ? HS(sub) : "");
    return 2;
}
u32 h_RegQueryValueExA(u32 k, u32 name, u32 res, u32 type, u32 data, u32 pcb)
{
    const char *v = "C:\\BOSSRALLY\\";
    u32 n = (u32)strlen(v) + 1;
    (void)k; (void)res;
    if (!name || strcasecmp(HS(name), "Directory")) return 2;
    if (type) HW32(type, 1);
    if (data) {
        if (!pcb || H32(pcb) < n) { if (pcb) HW32(pcb, n); return 234; }
        memcpy(W_P(data), v, n);
    }
    if (pcb) HW32(pcb, n);
    return 0;
}
u32 h_RegCloseKey(u32 k) { h_CloseHandle(k); return 0; }

/* ========================================================== ole32 === */
u32 h_CoInitialize(u32 r) { (void)r; return 0; }
void h_CoUninitialize(void) {}

/* ============================================================ modules == */
/* BRString.dll is the only resource module; its RT_STRING table is read
 * straight out of the disc's DLL. */
static u8 *g_strdll;
static size_t g_strdll_n;
static void load_strdll(void)
{
    char host[1024];
    FILE *f;
    if (g_strdll) return;
    if (!vfs_resolve("D:\\BRString.dll", host, sizeof host, 0)) return;
    f = fopen(host, "rb");
    if (!f) return;
    fseek(f, 0, SEEK_END);
    g_strdll_n = (size_t)ftell(f);
    fseek(f, 0, SEEK_SET);
    g_strdll = malloc(g_strdll_n);
    if (fread(g_strdll, 1, g_strdll_n, f) != g_strdll_n) { free(g_strdll); g_strdll = NULL; }
    fclose(f);
}
static u32 le32(const u8 *p) { return p[0] | p[1] << 8 | p[2] << 16 | (u32)p[3] << 24; }
static u16 le16(const u8 *p) { return (u16)(p[0] | p[1] << 8); }

/* find resource (type, id) -> (pointer, size) */
static const u8 *res_find(u32 type, u32 id, u32 *psz)
{
    u32 pe, nsec, optsz, rsrc_rva = 0, rsrc_off = 0, i;
    const u8 *d = g_strdll, *root, *e;
    if (!d) return NULL;
    pe = le32(d + 0x3C);
    nsec = le16(d + pe + 6);
    optsz = le16(d + pe + 20);
    for (i = 0; i < nsec; i++) {
        const u8 *s = d + pe + 24 + optsz + i * 40;
        if (!memcmp(s, ".rsrc", 5)) { rsrc_rva = le32(s + 12); rsrc_off = le32(s + 20); }
    }
    if (!rsrc_off) return NULL;
    root = d + rsrc_off;
    {
        const u8 *dir = root;
        u32 want[3] = { type, id, 0xFFFFFFFFu };
        int lvl;
        for (lvl = 0; lvl < 3; lvl++) {
            u32 n = le16(dir + 12) + le16(dir + 14), j, off = 0;
            int ok = 0;
            for (j = 0; j < n; j++) {
                e = dir + 16 + j * 8;
                if (want[lvl] == 0xFFFFFFFFu || le32(e) == want[lvl]) {
                    off = le32(e + 4);
                    ok = 1;
                    break;
                }
            }
            if (!ok) return NULL;
            if (off & 0x80000000u) dir = root + (off & 0x7FFFFFFF);
            else {
                const u8 *de = root + off;
                *psz = le32(de + 4);
                return d + rsrc_off + (le32(de) - rsrc_rva);
            }
        }
    }
    return NULL;
}

u32 h_LoadStringA(u32 hinst, u32 id, u32 buf, u32 n)
{
    u32 sz, k, len = 0;
    const u8 *p;
    (void)hinst;
    load_strdll();
    p = res_find(6, (id >> 4) + 1, &sz);
    if (!p || !n) { if (n) HS(buf)[0] = 0; return 0; }
    for (k = 0; k < (id & 15); k++) p += 2 + 2 * le16(p);
    len = le16(p);
    p += 2;
    for (k = 0; k < len && k + 1 < n; k++) {
        u16 c = le16(p + 2 * k);
        HS(buf)[k] = (char)(c < 256 ? c : '?');
    }
    HS(buf)[k] = 0;
    return k;
}

u32 h_GetModuleHandleA(u32 name)
{
    if (!name) return 0x00400000u;               /* BRally.exe */
    if (!strcasecmp(HS(name), "brglide.dll")) return 0x10000000u;
    return 0;
}

/* LoadLibrary / GetProcAddress: the DLLs the game opens by name */
static const char *g_libs[] = { "brstring.dll", "earias.dll", "earpds.dll", "ddraw.dll", "dinput.dll", 0 };
u32 h_LoadLibraryA(u32 name)
{
    const char *n = HS(name), *b = strrchr(n, '\\');
    int i;
    b = b ? b + 1 : n;
    for (i = 0; g_libs[i]; i++)
        if (!strcasecmp(b, g_libs[i])) { HLOG("LoadLibraryA(%s)\n", n); return 0x60000000u + (u32)i * 0x10000u; }
    HLOG("LoadLibraryA(%s) -> NULL\n", n);
    return 0;
}
u32 h_FreeLibrary(u32 h) { (void)h; return 1; }

u32 hear_proc(const char *name);            /* host_ear.c */
u32 hdx_proc(const char *lib, const char *name);   /* host_dx.c */
u32 h_GetProcAddress(u32 h, u32 name)
{
    int i = (int)((h - 0x60000000u) >> 16);
    const char *n = name >= 0x10000 ? HS(name) : "#ord";
    u32 a = 0;
    if (h < 0x60000000u || i > 4) return 0;
    if (i == 1 || i == 2) a = hear_proc(n);
    else if (i == 3 || i == 4) a = hdx_proc(g_libs[i], n);
    HLOG("GetProcAddress(%s, %s) -> %08X\n", g_libs[i], n, a);
    return a;
}

/* ============================================================ user32 == */
typedef struct { u32 hwnd, msg, wp, lp, t; } hmsg;
static hmsg g_q[1024];
static int g_qh, g_qt;
static pthread_mutex_t g_ql = PTHREAD_MUTEX_INITIALIZER;
static u32 g_classes_proc[16];
static char g_classes_name[16][64];
static int g_ncls;
typedef struct { u32 hwnd, proc, w, h, visible; u32 longs[8]; } hwin;
static hwin g_win[8];
static int g_nwin;
static u32 g_focus;
static struct { u32 hwnd, id, ms, proc; u64 due; } g_timer[32];

void hwin_post(u32 hwnd, u32 msg, u32 wp, u32 lp)
{
    pthread_mutex_lock(&g_ql);
    if (((g_qt + 1) & 1023) != g_qh) {
        g_q[g_qt] = (hmsg){ hwnd, msg, wp, lp, h_timeGetTime() };
        g_qt = (g_qt + 1) & 1023;
    }
    pthread_mutex_unlock(&g_ql);
}
u32 hwin_main_hwnd(void) { return g_nwin ? g_win[0].hwnd : 0; }

static hwin *winof(u32 h)
{
    int i;
    for (i = 0; i < g_nwin; i++) if (g_win[i].hwnd == h) return &g_win[i];
    return NULL;
}
static u32 send(u32 hwnd, u32 msg, u32 wp, u32 lp)
{
    hwin *w = winof(hwnd);
    u32 r;
    if (!w) return 0;
    r = w_icall_iiii_i(w->proc, hwnd, msg, wp, lp);
    HLOG("  WndProc(%08X, msg %04X, %08X, %08X) -> %08X\n", hwnd, msg, wp, lp, r);
    return r;
}

u32 h_RegisterClassA(u32 wc)
{
    if (g_ncls >= 16) return 0;
    g_classes_proc[g_ncls] = H32(wc + 4);
    snprintf(g_classes_name[g_ncls], 64, "%s", H32(wc + 36) >= 0x10000 ? HS(H32(wc + 36)) : "#");
    HLOG("RegisterClassA(%s, proc=%08X)\n", g_classes_name[g_ncls], g_classes_proc[g_ncls]);
    return 0xC000u + (u32)++g_ncls;
}

u32 h_CreateWindowExA(u32 ex, u32 cls, u32 title, u32 style, u32 x, u32 y,
                      u32 w, u32 h, u32 parent, u32 menu, u32 inst, u32 param)
{
    int i;
    u32 proc = 0, hwnd, cs, r;
    const char *cn = cls >= 0x10000 ? HS(cls) : "#";
    for (i = 0; i < g_ncls; i++) if (!strcasecmp(g_classes_name[i], cn)) proc = g_classes_proc[i];
    if (!proc) { fprintf(stderr, "CreateWindowExA: class %s not registered\n", cn); return 0; }
    hwnd = 0x00020000u + 0x10u * (u32)(g_nwin + 1);
    g_win[g_nwin++] = (hwin){ hwnd, proc, w & 0xFFFF, h & 0xFFFF, 0, {0} };
    HLOG("CreateWindowExA(%s, %s, %ux%u) -> %08X\n", cn, title ? HS(title) : "", w, h, hwnd);
    happ_init();
    cs = hmem_alloc(48, 1);
    {
        u32 v[12] = { param, inst, menu, parent, h, w, y, x, style, title, cls, ex };
        memcpy(W_P(cs), v, 48);
    }
    r = send(hwnd, 0x81, 0, cs);                 /* WM_NCCREATE */
    if (!r) { g_nwin--; HLOG("  WM_NCCREATE refused: window destroyed\n"); return 0; }
    r = send(hwnd, 0x01, 0, cs);                 /* WM_CREATE */
    if (r == 0xFFFFFFFFu) { g_nwin--; return 0; }
    return hwnd;
}

u32 h_ShowWindow(u32 hwnd, u32 cmd)
{
    hwin *w = winof(hwnd);
    u32 was;
    if (!w) return 0;
    was = w->visible;
    if (cmd && !was) {
        w->visible = 1;
        send(hwnd, 0x18, 1, 0);                  /* WM_SHOWWINDOW */
        send(hwnd, 0x1C, 1, 0);                  /* WM_ACTIVATEAPP */
        send(hwnd, 0x86, 1, 0);                  /* WM_NCACTIVATE */
        send(hwnd, 0x06, 1, 0);                  /* WM_ACTIVATE */
        g_focus = hwnd;
        send(hwnd, 0x07, 0, 0);                  /* WM_SETFOCUS */
        send(hwnd, 0x05, 0, (w->h << 16) | w->w);/* WM_SIZE */
    }
    return was ? 1 : 0;
}
u32 h_UpdateWindow(u32 h) { (void)h; return 1; }
u32 h_SetFocus(u32 h) { u32 o = g_focus; g_focus = h; return o; }
u32 h_GetForegroundWindow(void) { return g_focus; }
u32 h_GetActiveWindow(void) { return g_focus; }
u32 h_GetDesktopWindow(void) { return 0x00010010u; }
u32 h_IsWindow(u32 h) { return winof(h) || h == 0x00010010u; }
u32 h_FindWindowA(u32 c, u32 t) { (void)c; (void)t; return 0; }
u32 h_IsIconic(u32 h) { (void)h; return 0; }
u32 h_GetWindowThreadProcessId(u32 h, u32 pid) { (void)h; if (pid) HW32(pid, 0xFFF00001u); return 0xFFF00002u; }
u32 h_BringWindowToTop(u32 h) { (void)h; return 1; }
u32 h_SetForegroundWindow(u32 h) { (void)h; return 1; }
u32 h_GetLastActivePopup(u32 h) { return h; }
u32 h_GetWindowLongA(u32 h, u32 i) { hwin *w = winof(h); return w && (s32)i >= 0 && i < 32 ? w->longs[i / 4] : 0; }
u32 h_InvalidateRect(u32 h, u32 r, u32 e) { (void)h; (void)r; (void)e; return 1; }
u32 h_SetCursor(u32 c) { (void)c; return 0; }
u32 h_LoadCursorA(u32 i, u32 n) { (void)i; (void)n; return 0x00030001u; }
u32 h_LoadIconA(u32 i, u32 n) { (void)i; (void)n; return 0x00030002u; }
u32 h_DefWindowProcA(u32 h, u32 m, u32 wp, u32 lp)
{
    (void)h; (void)wp; (void)lp;
    if (m == 0x81) return 1;                     /* WM_NCCREATE: continue */
    if (m == 0x10) { hwin_post(h, 0x02, 0, 0); return 0; }  /* WM_CLOSE -> WM_DESTROY */
    return 0;
}
static char g_winmsg[16][64];
static int g_nwinmsg;
u32 h_RegisterWindowMessageA(u32 name)
{
    int i;
    for (i = 0; i < g_nwinmsg; i++) if (!strcmp(g_winmsg[i], HS(name))) return 0xC100u + (u32)i;
    if (g_nwinmsg < 16) snprintf(g_winmsg[g_nwinmsg++], 64, "%s", HS(name));
    return 0xC100u + (u32)(g_nwinmsg - 1);
}
u32 h_PostMessageA(u32 h, u32 m, u32 wp, u32 lp) { hwin_post(h, m, wp, lp); return 1; }
void h_PostQuitMessage(u32 code) { hwin_post(0, 0x12, code, 0); }
u32 h_SetTimer(u32 hwnd, u32 id, u32 ms, u32 proc)
{
    int i;
    for (i = 0; i < 32; i++)
        if (!g_timer[i].ms || (g_timer[i].hwnd == hwnd && g_timer[i].id == id)) {
            g_timer[i].hwnd = hwnd;
            g_timer[i].id = hwnd ? id : (u32)(i + 1);
            g_timer[i].ms = ms ? ms : 1;
            g_timer[i].proc = proc;
            g_timer[i].due = now_us() + (u64)ms * 1000;
            return g_timer[i].id;
        }
    return 0;
}
u32 h_KillTimer(u32 hwnd, u32 id)
{
    int i;
    for (i = 0; i < 32; i++)
        if (g_timer[i].ms && g_timer[i].hwnd == hwnd && g_timer[i].id == id) { g_timer[i].ms = 0; return 1; }
    return 0;
}
static void timers(void)
{
    int i;
    u64 t = now_us();
    for (i = 0; i < 32; i++)
        if (g_timer[i].ms && t >= g_timer[i].due) {
            int queued = 0, k;
            g_timer[i].due = t + (u64)g_timer[i].ms * 1000;
            pthread_mutex_lock(&g_ql);
            for (k = g_qh; k != g_qt; k = (k + 1) & 1023)
                if (g_q[k].msg == 0x113 && g_q[k].wp == g_timer[i].id) queued = 1;
            pthread_mutex_unlock(&g_ql);
            if (!queued) hwin_post(g_timer[i].hwnd, 0x113, g_timer[i].id, g_timer[i].proc);
        }
}
static void write_msg(u32 p, const hmsg *m)
{
    u32 v[7] = { m->hwnd, m->msg, m->wp, m->lp, m->t, 320, 240 };
    memcpy(W_P(p), v, 28);
}
static int take(u32 p, u32 hwnd, u32 lo, u32 hi, int remove)
{
    int k, got = 0;
    pthread_mutex_lock(&g_ql);
    for (k = g_qh; k != g_qt; k = (k + 1) & 1023) {
        hmsg *m = &g_q[k];
        if (hwnd && m->hwnd != hwnd) continue;
        if ((lo || hi) && !(lo <= m->msg && m->msg <= hi)) continue;
        write_msg(p, m);
        if (remove) {
            int j;
            for (j = k; ((j + 1) & 1023) != g_qt; j = (j + 1) & 1023)
                g_q[j] = g_q[(j + 1) & 1023];
            g_qt = (g_qt - 1) & 1023;
        }
        got = 1;
        break;
    }
    pthread_mutex_unlock(&g_ql);
    return got;
}
u32 h_PeekMessageA(u32 p, u32 hwnd, u32 lo, u32 hi, u32 rm)
{
    happ_pump(0);
    timers();
    if (g_happ_quit) { g_happ_quit = 0; hwin_post(hwin_main_hwnd(), 0x10, 0, 0); }
    return (u32)take(p, hwnd, lo, hi, rm & 1);
}
u32 h_GetMessageA(u32 p, u32 hwnd, u32 lo, u32 hi)
{
    for (;;) {
        happ_pump(0);
        timers();
        if (g_happ_quit) { g_happ_quit = 0; hwin_post(hwin_main_hwnd(), 0x10, 0, 0); }
        if (take(p, hwnd, lo, hi, 1))
            return H32(p + 4) == 0x12 ? 0 : 1;
        happ_pump(5);
    }
}
u32 h_WaitMessage(void) { happ_pump(5); return 1; }
u32 h_TranslateMessage(u32 p) { (void)p; return 0; }
u32 h_DispatchMessageA(u32 p)
{
    u32 hwnd = H32(p), msg = H32(p + 4), wp = H32(p + 8), lp = H32(p + 12);
    if (msg == 0x113 && lp)                      /* WM_TIMER with a TIMERPROC */
        return w_icall_iiii_i(lp, hwnd, msg, wp, h_timeGetTime()), 0;
    return send(hwnd, msg, wp, lp);
}
u32 h_MessageBoxA(u32 hwnd, u32 text, u32 cap, u32 type)
{
    (void)hwnd; (void)type;
    fprintf(stderr, "MessageBox [%s]: %s\n", cap ? HS(cap) : "", text ? HS(text) : "");
    return 1;                                    /* IDOK */
}
u32 h_GetAsyncKeyState(u32 vk) { return happ_key_down((int)vk) ? 0x8000u : 0; }

/* gdi32 / images */
u32 h_GetStockObject(u32 i) { return 0x00040000u + i; }
u32 h_DeleteObject(u32 o) { (void)o; return 1; }
u32 h_GetObjectA(u32 o, u32 n, u32 p) { (void)o; if (p) memset(W_P(p), 0, n); return 0; }
u32 h_LoadImageA(u32 inst, u32 name, u32 type, u32 cx, u32 cy, u32 fl)
{
    (void)inst; (void)type; (void)cx; (void)cy; (void)fl;
    HLOG("LoadImageA(%s)\n", name >= 0x10000 ? HS(name) : "#");
    return 0x00050001u;
}

/* ============================================================= winmm == */
/* CD audio (the soundtrack is Redbook audio on the disc): how the port plays
 * music is an open decision (ports/README.md), so the device opens and every
 * command succeeds silently. */
u32 h_mciSendCommandA(u32 id, u32 msg, u32 p1, u32 p2)
{
    HLOG("mciSendCommand(%u, %04X, %08X)\n", id, msg, p1);
    if (msg == 0x803 && p2) HW32(p2 + 4, 1);                  /* MCI_OPEN: wDeviceID */
    if (msg == 0x814 && p2) HW32(p2 + 4, 12);                 /* MCI_STATUS: 12 tracks etc. */
    return 0;
}
u32 h_acmMetrics(u32 o, u32 m, u32 p) { (void)o; (void)m; if (p) HW32(p, 0); return 0; }

/* mmio: RIFF reading over the VFS */
typedef struct { FILE *f; } hmmio;
static hmmio g_mmio[32];
u32 h_mmioOpenA(u32 name, u32 info, u32 fl)
{
    char host[1024];
    int i;
    (void)info;
    if (!name || !vfs_resolve(HS(name), host, sizeof host, 0)) return 0;
    for (i = 1; i < 32 && g_mmio[i].f; i++) ;
    if (i == 32) return 0;
    g_mmio[i].f = fopen(host, (fl & 3) ? "r+b" : "rb");
    return g_mmio[i].f ? (u32)i : 0;
}
static FILE *mmf(u32 h) { return h < 32 ? g_mmio[h].f : NULL; }
u32 h_mmioClose(u32 h, u32 fl) { (void)fl; if (mmf(h)) { fclose(g_mmio[h].f); g_mmio[h].f = NULL; } return 0; }
u32 h_mmioRead(u32 h, u32 buf, u32 n) { FILE *f = mmf(h); return f ? (u32)fread(W_P(buf), 1, n, f) : (u32)-1; }
u32 h_mmioSeek(u32 h, u32 off, u32 org)
{
    FILE *f = mmf(h);
    if (!f || fseek(f, (long)(s32)off, (int)org)) return (u32)-1;
    return (u32)ftell(f);
}
/* MMCKINFO: ckid, cksize, fccType, dwDataOffset, dwFlags */
u32 h_mmioDescend(u32 h, u32 ck, u32 parent, u32 fl)
{
    FILE *f = mmf(h);
    u32 end = parent ? H32(parent + 12) + H32(parent + 4) : 0xFFFFFFFFu;
    u32 want_id = H32(ck), want_type = H32(ck + 8);
    if (!f) return 0x101;
    for (;;) {
        u8 hd[12];
        u32 pos = (u32)ftell(f), id, sz;
        if (pos + 8 > end) return 0x101;                        /* MMIOERR_CHUNKNOTFOUND */
        if (fread(hd, 1, 8, f) != 8) return 0x101;
        id = hd[0] | hd[1] << 8 | hd[2] << 16 | (u32)hd[3] << 24;
        sz = hd[4] | hd[5] << 8 | hd[6] << 16 | (u32)hd[7] << 24;
        if (id == 0x46464952u || id == 0x5453494Cu) {           /* RIFF / LIST */
            u32 type;
            if (fread(hd + 8, 1, 4, f) != 4) return 0x101;
            type = hd[8] | hd[9] << 8 | hd[10] << 16 | (u32)hd[11] << 24;
            if ((fl & 0x20 && id == 0x46464952u && (!want_type || type == want_type)) ||
                (fl & 0x40 && id == 0x5453494Cu && (!want_type || type == want_type)) ||
                (!(fl & 0x60) && (!want_id || want_id == id))) {
                HW32(ck, id); HW32(ck + 4, sz); HW32(ck + 8, type);
                HW32(ck + 12, pos + 8); HW32(ck + 16, 0);
                return 0;
            }
            fseek(f, (long)(pos + 8 + ((sz + 1) & ~1u)), SEEK_SET);
            continue;
        }
        if (!(fl & 0x60) && (!want_id || want_id == id)) {
            HW32(ck, id); HW32(ck + 4, sz); HW32(ck + 8, 0);
            HW32(ck + 12, pos + 8); HW32(ck + 16, 0);
            return 0;
        }
        fseek(f, (long)(pos + 8 + ((sz + 1) & ~1u)), SEEK_SET);
    }
}
u32 h_mmioAscend(u32 h, u32 ck, u32 fl)
{
    FILE *f = mmf(h);
    (void)fl;
    if (!f) return 0x101;
    fseek(f, (long)(H32(ck + 12) + ((H32(ck + 4) + 1) & ~1u)), SEEK_SET);
    return 0;
}
u32 h_mmioGetInfo(u32 h, u32 info, u32 fl) { (void)h; (void)fl; memset(W_P(info), 0, 72); return 0; }
u32 h_mmioSetInfo(u32 h, u32 info, u32 fl) { (void)h; (void)info; (void)fl; return 0; }
u32 h_mmioAdvance(u32 h, u32 info, u32 fl) { (void)h; (void)info; (void)fl; return 0; }
