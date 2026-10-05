/* rdr_null.c: no renderer (headless comparisons): rdr_init declines, so the
 * RCP layer does not run the display lists' drawing at all. */
#include "../rdr.h"

int rdr_init(void) { return 0; }
void rdr_window(void) {}
int rdr_presents(void) { return 0; }
void rdr_frame_begin(int fb_w, int fb_h) { (void)fb_w; (void)fb_h; }
int rdr_texture(const uint8_t *rgba, int w, int h) { (void)rgba; (void)w; (void)h; return 0; }
void rdr_texture_free(int tex) { (void)tex; }
void rdr_triangles(const RdrState *st, const RdrVtx *v, int n) { (void)st; (void)v; (void)n; }
void rdr_rect(const RdrState *st, float x0, float y0, float x1, float y1, float s, float t, float dsdx,
              float dtdy, int fill, const float rgba[4])
{
    (void)st; (void)x0; (void)y0; (void)x1; (void)y1; (void)s; (void)t; (void)dsdx; (void)dtdy;
    (void)fill; (void)rgba;
}
void rdr_clear_depth(void) {}
int rdr_covers(void) { return 0; }
void rdr_vi(uint32_t ctrl) { (void)ctrl; }
void rdr_frame_end(void) {}
const uint32_t *rdr_frame_pixels(int *w, int *h) { *w = *h = 0; return 0; }
