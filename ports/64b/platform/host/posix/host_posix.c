/* host_posix.c: time, threads and files for POSIX systems (macOS, Linux). */
#define _GNU_SOURCE
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "host.h"

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
