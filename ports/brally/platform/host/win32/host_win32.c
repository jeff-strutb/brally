/* host_win32.c: time, threads, files, network and processes on Windows --
 * what host_posix.c is on macOS and Linux.
 *
 * The game's own Win32 surface (CreateMutexA, SetEvent ... in
 * platform/common) is the emulation every OS shares; this file is the host
 * under it and calls the real system. windows.h declares the system's
 * functions dllimport, so these calls go through the import table and never
 * bind to the emulation's functions of the same names. */
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "host.h"

/* ---- time --------------------------------------------------------------- */
uint64_t host_ticks_ns(void)
{
    static LARGE_INTEGER f;
    LARGE_INTEGER c;
    if (!f.QuadPart)
        QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&c);
    return (uint64_t)(c.QuadPart / f.QuadPart) * 1000000000u +
           (uint64_t)(c.QuadPart % f.QuadPart) * 1000000000u / (uint64_t)f.QuadPart;
}

void host_sleep_ms(uint32_t ms) { Sleep(ms); }

/* ---- threads -------------------------------------------------------------- */
struct host_mutex { CRITICAL_SECTION cs; };          /* recursive by nature */
struct host_cond  { CONDITION_VARIABLE cv; };
struct host_thread { HANDLE h; };

host_mutex *host_mutex_new(void)
{
    host_mutex *m = (host_mutex *)calloc(1, sizeof *m);
    InitializeCriticalSection(&m->cs);
    return m;
}
void host_mutex_lock(host_mutex *m)   { EnterCriticalSection(&m->cs); }
void host_mutex_unlock(host_mutex *m) { LeaveCriticalSection(&m->cs); }
void host_mutex_free(host_mutex *m)
{
    if (m) {
        DeleteCriticalSection(&m->cs);
        free(m);
    }
}

host_cond *host_cond_new(void)
{
    host_cond *c = (host_cond *)calloc(1, sizeof *c);
    InitializeConditionVariable(&c->cv);
    return c;
}

int host_cond_wait(host_cond *c, host_mutex *m, uint32_t timeout_ms)
{
    DWORD t = timeout_ms == 0xFFFFFFFFu ? INFINITE : timeout_ms;
    if (SleepConditionVariableCS(&c->cv, &m->cs, t))
        return 0;
    return GetLastError() == ERROR_TIMEOUT;
}

void host_cond_broadcast(host_cond *c) { WakeAllConditionVariable(&c->cv); }
void host_cond_free(host_cond *c) { free(c); }

typedef struct thread_start { void *(*fn)(void *); void *arg; } thread_start;

static DWORD WINAPI thread_main(LPVOID p)
{
    thread_start s = *(thread_start *)p;
    free(p);
    s.fn(s.arg);
    return 0;
}

host_thread *host_thread_start(void *(*fn)(void *), void *arg)
{
    host_thread *t = (host_thread *)calloc(1, sizeof *t);
    thread_start *s = (thread_start *)malloc(sizeof *s);
    s->fn = fn;
    s->arg = arg;
    t->h = CreateThread(NULL, 0, thread_main, s, 0, NULL);
    if (!t->h) {
        free(s);
        free(t);
        return NULL;
    }
    CloseHandle(t->h);                /* detached, as on POSIX */
    return t;
}

void host_thread_exit(void) { ExitThread(0); }
uintptr_t host_thread_self(void) { return (uintptr_t)GetCurrentThreadId(); }
void host_thread_interactive(void) { SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL); }

/* ---- files ------------------------------------------------------------------- */
struct host_dir {
    HANDLE           h;
    WIN32_FIND_DATAA fd;
    int              first;
};

host_dir *host_dir_open(const char *path)
{
    char pat[1100];
    host_dir *d = (host_dir *)calloc(1, sizeof *d);
    snprintf(pat, sizeof pat, "%s\\*", path);
    d->h = FindFirstFileA(pat, &d->fd);
    if (d->h == INVALID_HANDLE_VALUE) {
        free(d);
        return NULL;
    }
    d->first = 1;
    return d;
}

const char *host_dir_next(host_dir *d, int *is_dir, uint64_t *size)
{
    if (!d->first && !FindNextFileA(d->h, &d->fd))
        return NULL;
    d->first = 0;
    if (is_dir)
        *is_dir = (d->fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    if (size)
        *size = (uint64_t)d->fd.nFileSizeHigh << 32 | d->fd.nFileSizeLow;
    return d->fd.cFileName;
}

void host_dir_close(host_dir *d)
{
    if (d) {
        FindClose(d->h);
        free(d);
    }
}

int host_mkdir(const char *path)
{
    return CreateDirectoryA(path, NULL) || GetLastError() == ERROR_ALREADY_EXISTS;
}

/* ---- network ---------------------------------------------------------------- */
struct host_sock { SOCKET s; };

static int net_init(void)
{
    static int done, ok;
    if (!done) {
        WSADATA w;
        done = 1;
        ok = WSAStartup(MAKEWORD(2, 2), &w) == 0;
    }
    return ok;
}

host_sock *host_udp_open(uint16_t port, uint32_t group)
{
    struct sockaddr_in a;
    host_sock *h;
    SOCKET s;
    BOOL one = TRUE;
    if (!net_init())
        return NULL;
    s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET)
        return NULL;
    if (group)        /* several copies on one machine share the group's port */
        setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char *)&one, sizeof one);
    setsockopt(s, SOL_SOCKET, SO_BROADCAST, (const char *)&one, sizeof one);
    memset(&a, 0, sizeof a);
    a.sin_family = AF_INET;
    a.sin_port = htons(port);
    a.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(s, (struct sockaddr *)&a, sizeof a) != 0) {
        closesocket(s);
        return NULL;
    }
    if (group) {
        struct ip_mreq m;
        DWORD loop = 1, ttl = 1;
        m.imr_multiaddr.s_addr = htonl(group);
        m.imr_interface.s_addr = htonl(INADDR_ANY);
        setsockopt(s, IPPROTO_IP, IP_ADD_MEMBERSHIP, (const char *)&m, sizeof m);
        setsockopt(s, IPPROTO_IP, IP_MULTICAST_LOOP, (const char *)&loop, sizeof loop);
        setsockopt(s, IPPROTO_IP, IP_MULTICAST_TTL, (const char *)&ttl, sizeof ttl);
    }
    h = (host_sock *)calloc(1, sizeof *h);
    h->s = s;
    return h;
}

uint16_t host_udp_port(host_sock *h)
{
    struct sockaddr_in a;
    int n = sizeof a;
    if (getsockname(h->s, (struct sockaddr *)&a, &n) != 0)
        return 0;
    return ntohs(a.sin_port);
}

int host_udp_send(host_sock *h, const host_addr *to, const void *p, int n)
{
    struct sockaddr_in a;
    memset(&a, 0, sizeof a);
    a.sin_family = AF_INET;
    a.sin_port = htons(to->port);
    a.sin_addr.s_addr = htonl(to->ip);
    return sendto(h->s, (const char *)p, n, 0, (struct sockaddr *)&a, sizeof a);
}

int host_udp_recv(host_sock *h, host_addr *from, void *p, int cap, uint32_t wait_ms)
{
    WSAPOLLFD pf;
    struct sockaddr_in a;
    int al = sizeof a, n;
    pf.fd = h->s;
    pf.events = POLLRDNORM;
    pf.revents = 0;
    if (WSAPoll(&pf, 1, (INT)wait_ms) <= 0)
        return -1;
    n = recvfrom(h->s, (char *)p, cap, 0, (struct sockaddr *)&a, &al);
    if (n < 0)
        return -1;
    if (from) {
        from->ip = ntohl(a.sin_addr.s_addr);
        from->port = ntohs(a.sin_port);
    }
    return n;
}

void host_udp_close(host_sock *h)
{
    if (h) {
        closesocket(h->s);
        free(h);
    }
}

/* ---- processes ---------------------------------------------------------------- */
intptr_t host_spawn_self(const char *const *env, const char *log)
{
    char exe[MAX_PATH];
    char *block, *o;
    LPCH cur;
    const char *e;
    size_t nb = 1, i;
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    SECURITY_ATTRIBUTES sa;
    HANDLE out = NULL;
    BOOL ok;
    if (!GetModuleFileNameA(NULL, exe, sizeof exe))
        return 0;
    /* the inherited environment less every name env sets, then env */
    cur = GetEnvironmentStringsA();
    for (e = cur; *e; e += strlen(e) + 1)
        nb += strlen(e) + 1;
    for (i = 0; env && env[i]; i++)
        nb += strlen(env[i]) + 1;
    block = o = (char *)malloc(nb);
    for (e = cur; *e; e += strlen(e) + 1) {
        size_t l = strcspn(e, "=");
        int over = 0;
        for (i = 0; env && env[i]; i++)
            if (l && !_strnicmp(env[i], e, l) && env[i][l] == '=')
                over = 1;
        if (!over) {
            strcpy(o, e);
            o += strlen(e) + 1;
        }
    }
    for (i = 0; env && env[i]; i++) {
        strcpy(o, env[i]);
        o += strlen(env[i]) + 1;
    }
    *o = 0;
    FreeEnvironmentStringsA(cur);
    memset(&si, 0, sizeof si);
    si.cb = sizeof si;
    if (log) {
        sa.nLength = sizeof sa;
        sa.lpSecurityDescriptor = NULL;
        sa.bInheritHandle = TRUE;
        out = CreateFileA(log, GENERIC_WRITE, FILE_SHARE_READ, &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (out != INVALID_HANDLE_VALUE) {
            si.dwFlags = STARTF_USESTDHANDLES;
            si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
            si.hStdOutput = out;
            si.hStdError = out;
        } else {
            out = NULL;
        }
    }
    ok = CreateProcessA(exe, NULL, NULL, NULL, out != NULL, 0, block, NULL, &si, &pi);
    free(block);
    if (out)
        CloseHandle(out);
    if (!ok)
        return 0;
    CloseHandle(pi.hThread);
    return (intptr_t)pi.hProcess;
}

void host_kill(intptr_t id, uint32_t grace_ms)
{
    HANDLE h = (HANDLE)id;
    if (!id)
        return;
    if (WaitForSingleObject(h, grace_ms) != WAIT_OBJECT_0)
        TerminateProcess(h, 1);
    CloseHandle(h);
}
