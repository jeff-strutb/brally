/* brr_null.c: a renderer backend that draws nothing (headless runs).
 *
 * BR_RRLOG=PATH: one line per presented frame, a hash of every call this
 * backend took in it (draw state and vertices, clears, LFB writes, the map
 * table), with each texture named by a hash of its pixels rather than its
 * id. Two builds that hand the renderer the same work write the same file,
 * whatever layer between game and backend produced it. */
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "brr.h"

static FILE *s_log;
static int s_log_init;
static uint64_t s_hash = 1469598103934665603ull;
static uint32_t s_tex_n;
static uint64_t *s_tex;                 /* id -> pixel hash */

static int logging(void)
{
    if (!s_log_init) {
        const char *e = getenv("BR_RRLOG");
        s_log_init = 1;
        if (e && *e)
            s_log = fopen(e, "w");
    }
    return s_log != NULL;
}

static uint64_t fnv(uint64_t h, const void *p, size_t n)
{
    const unsigned char *b = (const unsigned char *)p;
    while (n--)
        h = (h ^ *b++) * 1099511628211ull;
    return h;
}
static void mix(const void *p, size_t n) { s_hash = fnv(s_hash, p, n); }
static void mix_tag(char c) { mix(&c, 1); }

static uint64_t tex_hash(uint32_t id) { return id && id < s_tex_n ? s_tex[id] : 0; }

/* the state as a backend sees it, padding zeroed and the texture by content */
static void mix_state(const brr_state *st)
{
    brr_state c;
    uint64_t th;
    memcpy(&c, st, sizeof c);
    memset((char *)&c + offsetof(brr_state, atest_ref) + 1, 0,
           offsetof(brr_state, depth_mode) - offsetof(brr_state, atest_ref) - 1);
    th = tex_hash(c.texture);
    c.texture = 0;
    mix(&c, sizeof c);
    mix(&th, sizeof th);
}

static uint32_t s_next = 1;
static unsigned long s_frames, s_tris;
static int s_w, s_h;

int brr_open(int width, int height)
{
    s_w = width;
    s_h = height;
    fprintf(stderr, "brr: null renderer %dx%d\n", width, height);
    return 1;
}

void brr_close(void)
{
    fprintf(stderr, "brr: %lu frames, %lu triangles\n", s_frames, s_tris);
}

uint32_t brr_texture(uint32_t id, const uint8_t *rgba, int w, int h)
{
    uint32_t r = id ? id : s_next++;
    if (logging()) {
        if (r >= s_tex_n) {
            uint32_t n = s_tex_n ? s_tex_n : 256;
            while (n <= r)
                n *= 2;
            s_tex = (uint64_t *)realloc(s_tex, n * sizeof *s_tex);
            memset(s_tex + s_tex_n, 0, (n - s_tex_n) * sizeof *s_tex);
            s_tex_n = n;
        }
        s_tex[r] = fnv(fnv(fnv(1469598103934665603ull, &w, sizeof w), &h, sizeof h),
                       rgba, (size_t)w * (size_t)h * 4);
    }
    return r;
}

void brr_texture_free(uint32_t id) { (void)id; }
void brr_clear(uint32_t argb, float depth, int colour, int depthbuf, const brr_state *clip)
{
    if (logging()) {
        mix_tag('c');
        mix(&argb, sizeof argb); mix(&depth, sizeof depth);
        mix(&colour, sizeof colour); mix(&depthbuf, sizeof depthbuf);
        if (clip)
            mix_state(clip);
    }
}
void brr_draw(const brr_state *st, const brr_vertex *v, int n)
{
    s_tris += (unsigned long)n / 3;
    if (logging()) {
        mix_tag('d');
        mix_state(st);
        mix(&n, sizeof n);
        mix(v, (size_t)n * sizeof *v);
    }
}
void brr_lfb_write(int x, int y, int w, int h, const uint16_t *p, int stride, const int d[4])
{
    if (logging()) {
        int r;
        mix_tag('l');
        mix(&x, sizeof x); mix(&y, sizeof y); mix(&w, sizeof w); mix(&h, sizeof h);
        mix(d, 4 * sizeof *d);
        for (r = 0; r < h; r++)
            mix((const char *)p + (size_t)r * (size_t)stride, (size_t)w * sizeof *p);   /* stride in bytes */
    }
}
void brr_target(int *w, int *h) { *w = s_w; *h = s_h; }
void brr_xf(const float (*t)[4], int n)
{
    if (logging()) {
        mix_tag('x');
        mix(&n, sizeof n);
        mix(t, (size_t)n * sizeof *t);
    }
}
void brr_present(void)
{
    s_frames++;
    if (logging()) {
        fprintf(s_log, "%lu %016llx\n", s_frames, (unsigned long long)s_hash);
        fflush(s_log);
        s_hash = 1469598103934665603ull;
    }
}
int brr_shot(const char *path) { (void)path; return 0; }
