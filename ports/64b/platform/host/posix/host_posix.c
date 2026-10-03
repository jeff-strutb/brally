/* host_posix.c: time, threads and files for POSIX systems (macOS, Linux). */
#define _GNU_SOURCE
#include <arpa/inet.h>
#include <dirent.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <errno.h>
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "host.h"

extern char **environ;

/* ---- time --------------------------------------------------------------- */
uint64_t host_ticks_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000u + (uint64_t)ts.tv_nsec;
}

void host_sleep_ms(uint32_t ms)
{
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    while (nanosleep(&ts, &ts) != 0 && errno == EINTR)
        ;
}

/* ---- threads -------------------------------------------------------------- */
struct host_mutex { pthread_mutex_t m; };
struct host_cond  { pthread_cond_t c; };
struct host_thread { pthread_t t; };

host_mutex *host_mutex_new(void)
{
    host_mutex *m = (host_mutex *)calloc(1, sizeof *m);
    pthread_mutexattr_t a;
    pthread_mutexattr_init(&a);
    pthread_mutexattr_settype(&a, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&m->m, &a);
    pthread_mutexattr_destroy(&a);
    return m;
}
void host_mutex_lock(host_mutex *m)   { pthread_mutex_lock(&m->m); }
void host_mutex_unlock(host_mutex *m) { pthread_mutex_unlock(&m->m); }
void host_mutex_free(host_mutex *m)
{
    if (m) {
        pthread_mutex_destroy(&m->m);
        free(m);
    }
}

host_cond *host_cond_new(void)
{
    host_cond *c = (host_cond *)calloc(1, sizeof *c);
    pthread_cond_init(&c->c, NULL);
    return c;
}

int host_cond_wait(host_cond *c, host_mutex *m, uint32_t timeout_ms)
{
    struct timespec ts;
    if (timeout_ms == 0xFFFFFFFFu) {
        pthread_cond_wait(&c->c, &m->m);
        return 0;
    }
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += timeout_ms / 1000;
    ts.tv_nsec += (long)(timeout_ms % 1000) * 1000000L;
    if (ts.tv_nsec >= 1000000000L) {
        ts.tv_sec++;
        ts.tv_nsec -= 1000000000L;
    }
    return pthread_cond_timedwait(&c->c, &m->m, &ts) == ETIMEDOUT;
}

void host_cond_broadcast(host_cond *c) { pthread_cond_broadcast(&c->c); }
void host_cond_free(host_cond *c)
{
    if (c) {
        pthread_cond_destroy(&c->c);
        free(c);
    }
}

host_thread *host_thread_start(void *(*fn)(void *), void *arg)
{
    host_thread *t = (host_thread *)calloc(1, sizeof *t);
    if (pthread_create(&t->t, NULL, fn, arg) != 0) {
        free(t);
        return NULL;
    }
    pthread_detach(t->t);
    return t;
}

void host_thread_exit(void) { pthread_exit(NULL); }
uintptr_t host_thread_self(void) { return (uintptr_t)pthread_self(); }

/* ---- files ------------------------------------------------------------------- */
struct host_dir { DIR *d; char path[1024]; };

host_dir *host_dir_open(const char *path)
{
    DIR *d = opendir(path);
    host_dir *h;
    if (!d)
        return NULL;
    h = (host_dir *)calloc(1, sizeof *h);
    h->d = d;
    strncpy(h->path, path, sizeof h->path - 1);
    return h;
}

const char *host_dir_next(host_dir *h, int *is_dir, uint64_t *size)
{
    struct dirent *e;
    struct stat st;
    char full[2048];
    while ((e = readdir(h->d)) != NULL) {
        snprintf(full, sizeof full, "%s/%s", h->path, e->d_name);
        if (stat(full, &st) != 0)
            continue;
        if (is_dir)
            *is_dir = S_ISDIR(st.st_mode);
        if (size)
            *size = (uint64_t)st.st_size;
        return e->d_name;
    }
    return NULL;
}

void host_dir_close(host_dir *h)
{
    if (h) {
        closedir(h->d);
        free(h);
    }
}

int host_mkdir(const char *path) { return mkdir(path, 0755) == 0 || errno == EEXIST; }

/* ---- network ---------------------------------------------------------------- */
struct host_sock { int fd; };

host_sock *host_udp_open(uint16_t port, uint32_t group)
{
    struct sockaddr_in a;
    host_sock *s;
    int fd = socket(AF_INET, SOCK_DGRAM, 0), one = 1;
    if (fd < 0)
        return NULL;
    if (group) {
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
#ifdef SO_REUSEPORT
        setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &one, sizeof one);
#endif
    }
    setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &one, sizeof one);
    memset(&a, 0, sizeof a);
    a.sin_family = AF_INET;
    a.sin_port = htons(port);
    a.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(fd, (struct sockaddr *)&a, sizeof a) != 0) {
        close(fd);
        return NULL;
    }
    if (group) {
        struct ip_mreq m;
        unsigned char loop = 1, ttl = 1;
        m.imr_multiaddr.s_addr = htonl(group);
        m.imr_interface.s_addr = htonl(INADDR_ANY);
        setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &m, sizeof m);
        setsockopt(fd, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof loop);
        setsockopt(fd, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof ttl);
    }
    s = (host_sock *)calloc(1, sizeof *s);
    s->fd = fd;
    return s;
}

uint16_t host_udp_port(host_sock *s)
{
    struct sockaddr_in a;
    socklen_t n = sizeof a;
    if (getsockname(s->fd, (struct sockaddr *)&a, &n) != 0)
        return 0;
    return ntohs(a.sin_port);
}

int host_udp_send(host_sock *s, const host_addr *to, const void *p, int n)
{
    struct sockaddr_in a;
    memset(&a, 0, sizeof a);
    a.sin_family = AF_INET;
    a.sin_port = htons(to->port);
    a.sin_addr.s_addr = htonl(to->ip);
    return (int)sendto(s->fd, p, (size_t)n, 0, (struct sockaddr *)&a, sizeof a);
}

int host_udp_recv(host_sock *s, host_addr *from, void *p, int cap, uint32_t wait_ms)
{
    struct pollfd pf;
    struct sockaddr_in a;
    socklen_t al = sizeof a;
    ssize_t n;
    pf.fd = s->fd;
    pf.events = POLLIN;
    if (poll(&pf, 1, (int)wait_ms) <= 0)
        return -1;
    n = recvfrom(s->fd, p, (size_t)cap, 0, (struct sockaddr *)&a, &al);
    if (n < 0)
        return -1;
    if (from) {
        from->ip = ntohl(a.sin_addr.s_addr);
        from->port = ntohs(a.sin_port);
    }
    return (int)n;
}

void host_udp_close(host_sock *s)
{
    if (s) {
        close(s->fd);
        free(s);
    }
}

/* ---- processes ---------------------------------------------------------------- */
static int self_path(char *out, size_t n);

intptr_t host_spawn_self(const char *const *env, const char *log)
{
    char exe[4096];
    char *argv[2];
    char **envp;
    size_t ne = 0, nn = 0, i, j;
    posix_spawn_file_actions_t fa;
    pid_t pid;
    int rc;
    if (!self_path(exe, sizeof exe))
        return 0;
    while (environ[ne])
        ne++;
    while (env && env[nn])
        nn++;
    envp = (char **)calloc(ne + nn + 1, sizeof *envp);
    /* the inherited environment less every name env sets, then env */
    for (i = j = 0; i < ne; i++) {
        size_t k, l = strcspn(environ[i], "=");
        int over = 0;
        for (k = 0; k < nn; k++)
            if (!strncmp(env[k], environ[i], l) && env[k][l] == '=')
                over = 1;
        if (!over)
            envp[j++] = environ[i];
    }
    for (i = 0; i < nn; i++)
        envp[j++] = (char *)env[i];
    envp[j] = NULL;
    argv[0] = exe;
    argv[1] = NULL;
    posix_spawn_file_actions_init(&fa);
    if (log) {
        posix_spawn_file_actions_addopen(&fa, 1, log, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        posix_spawn_file_actions_adddup2(&fa, 1, 2);
    }
    rc = posix_spawn(&pid, exe, &fa, NULL, argv, envp);
    posix_spawn_file_actions_destroy(&fa);
    free(envp);
    return rc == 0 ? (intptr_t)pid : 0;
}

void host_kill(intptr_t id, uint32_t grace_ms)
{
    uint32_t waited = 0;
    if (id <= 0)
        return;
    while (waited < grace_ms && waitpid((pid_t)id, NULL, WNOHANG) == 0) {
        host_sleep_ms(50);
        waited += 50;
    }
    if (waited >= grace_ms) {
        kill((pid_t)id, SIGTERM);
        waitpid((pid_t)id, NULL, 0);
    }
}

#ifdef __APPLE__
#include <mach-o/dyld.h>
static int self_path(char *out, size_t n)
{
    uint32_t sz = (uint32_t)n;
    return _NSGetExecutablePath(out, &sz) == 0;
}
#else
static int self_path(char *out, size_t n)
{
    ssize_t k = readlink("/proc/self/exe", out, n - 1);
    if (k <= 0)
        return 0;
    out[k] = 0;
    return 1;
}
#endif
