/* brr_null.c: a renderer backend that draws nothing (headless runs). */
#include <stdio.h>
#include <stdlib.h>

#include "brr.h"

static uint32_t s_next = 1;
static unsigned long s_frames, s_tris;

int brr_open(int width, int height)
{
    fprintf(stderr, "brr: null renderer %dx%d\n", width, height);
    return 1;
}

void brr_close(void)
{
    fprintf(stderr, "brr: %lu frames, %lu triangles\n", s_frames, s_tris);
}

uint32_t brr_texture(uint32_t id, const uint8_t *rgba, int w, int h)
{
    (void)rgba;
    (void)w;
    (void)h;
    return id ? id : s_next++;
}

void brr_texture_free(uint32_t id) { (void)id; }
void brr_clear(uint32_t argb, float depth, int colour, int depthbuf, const brr_state *clip)
{
    (void)argb; (void)depth; (void)colour; (void)depthbuf; (void)clip;
}
void brr_draw(const brr_state *st, const brr_vertex *v, int n) { (void)st; (void)v; s_tris += (unsigned long)n / 3; }
void brr_lfb_write(int x, int y, int w, int h, const uint16_t *p, int stride)
{
    (void)x; (void)y; (void)w; (void)h; (void)p; (void)stride;
}
void brr_present(void) { s_frames++; }
int brr_shot(const char *path) { (void)path; return 0; }
