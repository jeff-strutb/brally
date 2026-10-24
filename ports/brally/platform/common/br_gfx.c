/* br_gfx.c: the game's drawing (br_gfx.h), over the renderer interface (brr.h).
 *
 * The game's texture memory is a range of bytes it allocates in itself: an
 * upload copies the game's texels to an offset, and a bind reads the bytes
 * at an offset with the description IT is given -- the game uploads with
 * one description and binds with another (the large font page). A decoded
 * RGBA8 copy is cached per (offset, format, size) and dropped when an
 * upload overwrites its bytes. */
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "plat.h"
#include "br_gfx.h"
#include "brr.h"

#define TEX_RAM   (4u << 20)        /* the game sizes its texture memory to 4 MB */

static brr_state s_st;
static int s_w = 640, s_h = 480;
static const int s_lower_left = 1;      /* the game's y runs up from the bottom */
static int s_rw = 640, s_rh = 480;      /* the target (the screen map below) */
static int s_depth_mode;

/* ---- the call log ------------------------------------------------------------- */
/* BR_GLLOG=PATH: during the frames BR_TRACE_FRAMES=A:B selects (script.c),
 * the drawing calls one per line with the vertices expanded. */
int g_plat_tracing;
static FILE *s_gllog;

static void gllog(const char *fmt, ...)
{
    static int init;
    va_list ap;
    if (!g_plat_tracing)
        return;
    if (!init) {
        const char *p = getenv("BR_GLLOG");
        init = 1;
        if (p && (s_gllog = fopen(p, "w")) != NULL)
            setvbuf(s_gllog, NULL, _IONBF, 0);
    }
    if (!s_gllog)
        return;
    va_start(ap, fmt);
    vfprintf(s_gllog, fmt, ap);
    va_end(ap);
    fputc('\n', s_gllog);
}

static void gllog_vtx(const BrGfxVertex *v)
{
    if (g_plat_tracing && s_gllog)
        fprintf(s_gllog, "  v %.4g %.4g z%.4g w%.4g st %.4g %.4g rgba %.4g %.4g %.4g %.4g\n",
                v->x, v->y, v->ooz, v->oow, v->tc[0].sow, v->tc[0].tow, v->r, v->g, v->b, v->a);
}

/* ---- colours: the game's RGBA (red in the top byte) to ARGB ------------------- */
static uint32_t argb(uint32_t c)
{
    return (c & 0xFF) << 24 | c >> 8;
}

/* ---- start-up ------------------------------------------------------------------- */
void br_gfx_shutdown(void) { brr_close(); }

int br_gfx_open(int w, int h)
{
    s_w = w;
    s_h = h;
    s_st.clip_x0 = 0;
    s_st.clip_y0 = 0;
    s_st.clip_x1 = s_w;
    s_st.clip_y1 = s_h;
    s_st.blend_rgb_src = 4;     /* one */
    s_st.blend_rgb_dst = 0;     /* zero */
    s_st.blend_a_src = 4;
    s_st.blend_a_dst = 0;
    s_st.atest_fn = 7;          /* always */
    s_st.depth_fn = 1;          /* less */
    s_st.depth_mask = 1;
    if (!brr_open(s_w, s_h))
        return 0;
    brr_target(&s_rw, &s_rh);
    return 1;
}

/* ---- frame ------------------------------------------------------------------------- */
void br_gfx_clip(uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1)
{
    gllog("clip %u %u %u %u", x0, y0, x1, y1);
    s_st.clip_x0 = (int32_t)x0;
    s_st.clip_x1 = (int32_t)x1;
    if (s_lower_left) {
        s_st.clip_y0 = s_h - (int32_t)y1;
        s_st.clip_y1 = s_h - (int32_t)y0;
    } else {
        s_st.clip_y0 = (int32_t)y0;
        s_st.clip_y1 = (int32_t)y1;
    }
}

/* ---- the screen map ---------------------------------------------------------------- */
/* Where each draw goes on a target of any shape, as the wasm lane placed it
 * (ports/brally-wasm/wasm/host/host_glide.m): the game's picture at its own
 * shape, as large as fits, centred.  The renderer takes it as a map table of
 * one entry (brr_xf), set at the swap. */
typedef struct { double ax, bx, ay, by; } xmap;     /* target pixels = a * game pixels + b */
static float s_xt[1][4];

static xmap map_box(void)
{
    double u = fmin((double)s_rw / s_w, (double)s_rh / s_h);
    return (xmap){ u, (s_rw - s_w * u) * 0.5, u, (s_rh - s_h * u) * 0.5 };
}

void plat_pointer_to_game(int *x, int *y)
{
    xmap m = map_box();
    int uw, uh;
    double tx, ty;
    plat_pointer_units(&uw, &uh);         /* (the window's units, not the game's resolution) */
    tx = (double)*x * s_rw / uw;
    ty = (double)*y * s_rh / uh;
    *x = (int)floor((tx - m.bx) / m.ax);
    *y = (int)floor((ty - m.by) / m.ay);
}

static void map_ndc(xmap m, float *T)
{
    T[0] = (float)(2.0 * m.ax / s_rw);
    T[1] = (float)(2.0 * m.bx / s_rw - 1.0);
    T[2] = (float)(-2.0 * m.ay / s_rh);
    T[3] = (float)(1.0 - 2.0 * m.by / s_rh);
}

/* the clip window as a scissor in target pixels */
static void scissor(void)
{
    int x0 = s_st.clip_x0 < 0 ? 0 : s_st.clip_x0, y0 = s_st.clip_y0 < 0 ? 0 : s_st.clip_y0;
    int x1 = s_st.clip_x1 > s_w ? s_w : s_st.clip_x1, y1 = s_st.clip_y1 > s_h ? s_h : s_st.clip_y1;
    long px0, py0, px1, py1;
    xmap m = map_box();
    s_st.xf = 0;
    if (x1 <= x0 || y1 <= y0) {
        s_st.sx0 = s_st.sy0 = s_st.sx1 = s_st.sy1 = 0;
        return;
    }
    px0 = lround(m.ax * x0 + m.bx); px1 = lround(m.ax * x1 + m.bx);
    py0 = lround(m.ay * y0 + m.by); py1 = lround(m.ay * y1 + m.by);
    if (px0 < 0) px0 = 0;
    if (py0 < 0) py0 = 0;
    if (px1 > s_rw) px1 = s_rw;
    if (py1 > s_rh) py1 = s_rh;
    if (px1 <= px0 || py1 <= py0)
        px0 = py0 = px1 = py1 = 0;
    s_st.sx0 = (int32_t)px0; s_st.sy0 = (int32_t)py0;
    s_st.sx1 = (int32_t)px1; s_st.sy1 = (int32_t)py1;
}

void br_gfx_clear(uint32_t color, uint8_t alpha, uint16_t depth)
{
    uint32_t c = (argb(color) & 0x00FFFFFFu) | (uint32_t)alpha << 24;
    gllog("clear %08X %u %u", color, alpha, depth);
    scissor();
    brr_clear(c, depth / 65536.0f, 1, s_st.depth_mode != 0, &s_st);
}

void plat_text_swap(void);         /* script_game.c */

void br_gfx_present(void)
{
    gllog("present");
    plat_text_swap();
    map_ndc(map_box(), s_xt[0]);
    brr_xf((const float (*)[4])s_xt, 1);
    brr_present();
    brr_target(&s_rw, &s_rh);           /* the next frame's target */
    plat_pump(0);
}

/* ---- state ----------------------------------------------------------------------------- */
void br_gfx_color_combine(int32_t f, int32_t fa, int32_t l, int32_t o, int32_t inv)
{
    gllog("color_combine %u %u %u %u %u", f, fa, l, o, inv);
    s_st.cc_fn = f; s_st.cc_factor = fa; s_st.cc_local = l; s_st.cc_other = o; s_st.cc_invert = inv;
}
void br_gfx_alpha_combine(int32_t f, int32_t fa, int32_t l, int32_t o, int32_t inv)
{
    gllog("alpha_combine %u %u %u %u %u", f, fa, l, o, inv);
    s_st.ac_fn = f; s_st.ac_factor = fa; s_st.ac_local = l; s_st.ac_other = o; s_st.ac_invert = inv;
}
void br_gfx_blend(int32_t rs, int32_t rd, int32_t as, int32_t ad)
{
    gllog("blend %u %u %u %u", rs, rd, as, ad);
    s_st.blend_rgb_src = rs; s_st.blend_rgb_dst = rd; s_st.blend_a_src = as; s_st.blend_a_dst = ad;
}
void br_gfx_alpha_test(int32_t f) { gllog("alpha_test %d", f); s_st.atest_fn = f; }
void br_gfx_alpha_ref(uint8_t v) { gllog("alpha_ref %u", v); s_st.atest_ref = v; }
void br_gfx_constant(uint32_t v) { gllog("constant %08X", v); s_st.constant = argb(v); }
void br_gfx_cull(int32_t m) { gllog("cull %d", m); s_st.cull = m; }
void br_gfx_depth_mode(int32_t m) { gllog("depth_mode %u", m); s_depth_mode = m; s_st.depth_mode = m; }
void br_gfx_depth_func(int32_t f) { gllog("depth_func %d", f); s_st.depth_fn = f; }
void br_gfx_depth_mask(int32_t m) { gllog("depth_mask %d", m); s_st.depth_mask = m; }
void br_gfx_fog_mode(int32_t m) { gllog("fog_mode %d", m); s_st.fog_mode = m; }
void br_gfx_fog_color(uint32_t c) { s_st.fog_color = argb(c); }
void br_gfx_fog_table(const uint8_t ft[BR_GFX_FOG_TABLE_SIZE]) { memcpy(s_st.fog_table, ft, BR_GFX_FOG_TABLE_SIZE); }

/* table entry i covers w = 2^(3 + i/4) / (8 - i%4) */
static float fog_w(int i) { return (float)(pow(2.0, 3.0 + (double)(i >> 2)) / (8 - (i & 3))); }

void br_gfx_fog_linear(uint8_t ft[BR_GFX_FOG_TABLE_SIZE], float nearZ, float farZ)
{
    int i;
    for (i = 0; i < BR_GFX_FOG_TABLE_SIZE; i++) {
        float f = (fog_w(i) - nearZ) / (farZ - nearZ);
        ft[i] = (uint8_t)(f < 0 ? 0 : f > 1 ? 255 : f * 255.0f);
    }
}

/* the game sets up a second texture unit only when it has one; it has one */
void br_gfx_tex_combine(int32_t unit, int32_t rf, int32_t rfa, int32_t af, int32_t afa, int32_t ri, int32_t ai)
{
    gllog("tex_combine %u %u %u %u %u %u", rf, rfa, af, afa, ri, ai);
    if (unit != 0)
        return;
    s_st.tc_rgb_fn = rf; s_st.tc_rgb_factor = rfa; s_st.tc_alpha_fn = af;
    s_st.tc_alpha_factor = afa; s_st.tc_rgb_invert = ri; s_st.tc_alpha_invert = ai;
}
void br_gfx_tex_filter(int32_t unit, int32_t mn, int32_t mg)
{
    if (unit == 0) { s_st.min_filter = mn; s_st.mag_filter = mg; }
}
void br_gfx_tex_clamp(int32_t unit, int32_t s, int32_t t)
{
    if (unit == 0) { s_st.clamp_s = s; s_st.clamp_t = t; }
}

/* ---- textures ----------------------------------------------------------------------------- */
static int lod_side(int32_t l) { return 256 >> l; }

static void dims(int32_t lod, int32_t ar, int *w, int *h)
{
    int s = lod_side(lod);
    *w = *h = s;
    if (ar < 3)
        *h = s >> (3 - ar);       /* 8x1, 4x1, 2x1 */
    else if (ar > 3)
        *w = s >> (ar - 3);       /* 1x2, 1x4, 1x8 */
    if (*w < 1) *w = 1;
    if (*h < 1) *h = 1;
}

static int texel_bytes(int32_t f) { return f >= 8 ? 2 : 1; }

static uint32_t mem_required(int32_t small, int32_t large, int32_t ar, int32_t f)
{
    uint32_t n = 0;
    int32_t l;
    for (l = large; l <= small; l++) {
        int w, h;
        dims(l, ar, &w, &h);
        n += (uint32_t)(w * h * texel_bytes(f));
    }
    return (n + 7) & ~7u;
}

uint32_t br_gfx_tex_bytes(int32_t small, int32_t large, int32_t ar, int32_t f)
{
    return mem_required(small, large, ar, f);
}

uint32_t br_gfx_tex_lo(int32_t unit) { (void)unit; return 0; }
/* the top of the range: 128 KB below the end of the 4 MB, as the original
 * found it on the card it was written for (tools/brally/brbox_imports.py) */
uint32_t br_gfx_tex_hi(int32_t unit) { (void)unit; return TEX_RAM - 0x20000; }

static uint8_t s_tmem[TEX_RAM];

/* a decoded texture: the bytes [start, end) read as fmt at w x h, the
 * renderer's texture kept for them (s_kept[k]) */
typedef struct ptex { uint32_t start, end; int32_t fmt; int w, h; uint32_t id; int k; } ptex;
#define PTEX_MAX 2048
static ptex s_tex[PTEX_MAX];
static int s_ntex;

/* the renderer's textures, by the bytes they were decoded from: the same
 * bytes back at any address take the texture already made.  The font is one
 * texture downloaded again for every character drawn (br_font.c) and an
 * animated texture cycles through a few frames, so without this each would
 * be a new texture for the renderer every frame (and, with Vulkan, a wait
 * for the GPU).  The least recently used goes when they are all taken. */
typedef struct pkept { uint64_t key; int32_t fmt; int w, h; uint32_t id, used; } pkept;
#define PKEPT_MAX 1536
static pkept s_kept[PKEPT_MAX];
static uint32_t s_kept_clock;

static uint64_t bytes_key(const uint8_t *p, uint32_t n)
{
    uint64_t h = 14695981039346656037ull;          /* FNV-1a, 64 bits */
    uint32_t i;
    for (i = 0; i < n; i++)
        h = (h ^ p[i]) * 1099511628211ull;
    return h;
}

/* the slot kept for these bytes, or -1 */
static int kept_find(uint64_t key, int32_t fmt, int w, int h)
{
    int k;
    for (k = 0; k < PKEPT_MAX; k++)
        if (s_kept[k].id && s_kept[k].key == key && s_kept[k].fmt == fmt && s_kept[k].w == w && s_kept[k].h == h)
            return k;
    return -1;
}

/* a slot for a new texture: an empty one, else the least recently used,
 * freed with every decoded texture that named it */
static int kept_slot(void)
{
    int k, old = 0, i;
    for (k = 0; k < PKEPT_MAX; k++) {
        if (!s_kept[k].id)
            return k;
        if (s_kept_clock - s_kept[k].used > s_kept_clock - s_kept[old].used)   /* (by age: the clock wraps) */
            old = k;
    }
    for (i = 0; i < s_ntex; i++)
        if (s_tex[i].k == old) {
            s_tex[i] = s_tex[--s_ntex];
            i--;
        }
    brr_texture_free(s_kept[old].id);
    memset(&s_kept[old], 0, sizeof s_kept[old]);
    return old;
}

static void decode(int32_t f, const uint8_t *src, uint8_t *dst, int n)
{
    int i;
    for (i = 0; i < n; i++, dst += 4) {
        uint32_t v;
        switch (f) {
        case 0:  /* RGB_332 */
            v = src[i];
            dst[0] = (uint8_t)((v >> 5) * 255 / 7); dst[1] = (uint8_t)(((v >> 2) & 7) * 255 / 7);
            dst[2] = (uint8_t)((v & 3) * 255 / 3); dst[3] = 255;
            break;
        case 2:  /* ALPHA_8 */
            dst[0] = dst[1] = dst[2] = 255; dst[3] = src[i];
            break;
        case 3:  /* INTENSITY_8 */
            dst[0] = dst[1] = dst[2] = src[i]; dst[3] = 255;
            break;
        case 4:  /* ALPHA_INTENSITY_44 */
            dst[0] = dst[1] = dst[2] = (uint8_t)((src[i] & 15) * 17); dst[3] = (uint8_t)((src[i] >> 4) * 17);
            break;
        case 8:  /* ARGB_8332 */
            v = (uint32_t)src[2 * i] | (uint32_t)src[2 * i + 1] << 8;
            dst[0] = (uint8_t)(((v >> 5) & 7) * 255 / 7); dst[1] = (uint8_t)(((v >> 2) & 7) * 255 / 7);
            dst[2] = (uint8_t)((v & 3) * 255 / 3); dst[3] = (uint8_t)(v >> 8);
            break;
        case 10: /* RGB_565 */
            v = (uint32_t)src[2 * i] | (uint32_t)src[2 * i + 1] << 8;
            dst[0] = (uint8_t)((v >> 11) * 255 / 31); dst[1] = (uint8_t)(((v >> 5) & 63) * 255 / 63);
            dst[2] = (uint8_t)((v & 31) * 255 / 31); dst[3] = 255;
            break;
        case 11: /* ARGB_1555 */
            v = (uint32_t)src[2 * i] | (uint32_t)src[2 * i + 1] << 8;
            dst[0] = (uint8_t)(((v >> 10) & 31) * 255 / 31); dst[1] = (uint8_t)(((v >> 5) & 31) * 255 / 31);
            dst[2] = (uint8_t)((v & 31) * 255 / 31); dst[3] = (v & 0x8000) ? 255 : 0;
            break;
        case 12: /* ARGB_4444 */
            v = (uint32_t)src[2 * i] | (uint32_t)src[2 * i + 1] << 8;
            dst[0] = (uint8_t)(((v >> 8) & 15) * 17); dst[1] = (uint8_t)(((v >> 4) & 15) * 17);
            dst[2] = (uint8_t)((v & 15) * 17); dst[3] = (uint8_t)((v >> 12) * 17);
            break;
        case 13: /* ALPHA_INTENSITY_88 */
            dst[0] = dst[1] = dst[2] = src[2 * i]; dst[3] = src[2 * i + 1];
            break;
        default:
            dst[0] = dst[1] = dst[2] = dst[3] = 255;
            break;
        }
    }
}

/* the current source, re-read before a draw when a download changed its
 * bytes: the card samples memory, so a texture rewritten in place while
 * bound draws with its new contents */
static uint32_t s_src_start;
static BrGfxTexInfo s_src_info;
static int s_src_set, s_src_dirty;

static void tex_resolve(void);

void br_gfx_tex_upload(int32_t unit, uint32_t start, const BrGfxTexInfo *info)
{
    uint32_t n, end;
    int i;
    if (unit != 0 || !info || !info->data || start >= TEX_RAM)
        return;
    n = mem_required(info->smallLod, info->largeLod, info->aspectRatio, info->format);
    if (start + n > TEX_RAM)
        n = TEX_RAM - start;
    if (g_plat_tracing && s_gllog) {
        uint32_t hsh = 2166136261u, k;
        for (k = 0; k < n; k++)
            hsh = (hsh ^ ((const uint8_t *)info->data)[k]) * 16777619u;
        gllog("tex_upload start %u n %u fmt %u hash %08X", start, n, info->format, hsh);
    }
    memcpy(s_tmem + start, info->data, n);
    end = start + n;
    /* anything decoded from the bytes just overwritten is stale here (the
       renderer's texture stays kept for its bytes) */
    for (i = 0; i < s_ntex; i++)
        if (s_tex[i].id && s_tex[i].start < end && start < s_tex[i].end) {
            if (s_tex[i].id == s_st.texture)
                s_src_dirty = 1;
            s_tex[i] = s_tex[--s_ntex];
            i--;
        }
}

void br_gfx_tex_bind(int32_t unit, uint32_t start, const BrGfxTexInfo *info)
{
    if (info)
        gllog("tex_bind start %u fmt %u large %u aspect %u", start, info->format, info->largeLod, info->aspectRatio);
    if (unit != 0)
        return;
    s_st.texture = 0;
    s_src_set = 0;
    if (!info || start >= TEX_RAM)
        return;
    s_src_start = start;
    s_src_info = *info;
    s_src_set = 1;
    tex_resolve();
}

static void tex_resolve(void)
{
    int i, k, w, h;
    uint32_t n, start = s_src_start;
    uint8_t *rgba;
    uint64_t key;
    const BrGfxTexInfo *info = &s_src_info;
    s_src_dirty = 0;
    s_st.texture = 0;
    dims(info->largeLod, info->aspectRatio, &w, &h);
    s_st.tex_w = w;
    s_st.tex_h = h;
    for (i = 0; i < s_ntex; i++)
        if (s_tex[i].start == start && s_tex[i].fmt == info->format && s_tex[i].w == w && s_tex[i].h == h) {
            s_st.texture = s_tex[i].id;
            s_kept[s_tex[i].k].used = ++s_kept_clock;
            return;
        }
    n = (uint32_t)(w * h * texel_bytes(info->format));
    if (start + n > TEX_RAM)
        return;
    key = bytes_key(s_tmem + start, n);                     /* the largest level, as decoded */
    k = kept_find(key, info->format, w, h);
    if (k < 0) {
        rgba = (uint8_t *)malloc((size_t)w * (size_t)h * 4);
        if (!rgba)
            return;
        decode(info->format, s_tmem + start, rgba, w * h);
        k = kept_slot();
        s_kept[k].id = brr_texture(0, rgba, w, h);
        free(rgba);
        if (!s_kept[k].id)
            return;
        s_kept[k].key = key;
        s_kept[k].fmt = info->format;
        s_kept[k].w = w;
        s_kept[k].h = h;
    }
    s_kept[k].used = ++s_kept_clock;
    if (s_ntex == PTEX_MAX)
        s_tex[0] = s_tex[--s_ntex];
    s_tex[s_ntex].start = start;
    s_tex[s_ntex].end = start + mem_required(info->smallLod, info->largeLod, info->aspectRatio, info->format);
    s_tex[s_ntex].fmt = info->format;
    s_tex[s_ntex].w = w;
    s_tex[s_ntex].h = h;
    s_tex[s_ntex].id = s_kept[k].id;
    s_tex[s_ntex].k = k;
    s_st.texture = s_tex[s_ntex].id;
    s_ntex++;
}

/* ---- drawing ------------------------------------------------------------------------------ */
static void vert(brr_vertex *o, const BrGfxVertex *v)
{
    int big = s_st.tex_w > s_st.tex_h ? s_st.tex_w : s_st.tex_h;
    float su = big ? 256.0f * (float)s_st.tex_w / (float)big : 256.0f;
    float sv = big ? 256.0f * (float)s_st.tex_h / (float)big : 256.0f;
    float w = v->oow != 0.0f ? 1.0f / v->oow : 1.0f;
    o->x = v->x;
    o->y = s_lower_left ? (float)s_h - v->y : v->y;
    o->z = s_depth_mode == 2 ? (v->oow > 0 ? 1.0f - v->oow : 1.0f) : v->ooz / 65535.0f;
    o->oow = v->oow;
    o->r = v->r; o->g = v->g; o->b = v->b; o->a = v->a;
    o->s = v->tc[0].sow * w / su;
    o->t = v->tc[0].tow * w / sv;
}

/* Triangle setup drops a triangle with no area, and with culling on
 * drops the one facing away: the sign of its area in the coordinates the
 * application passed (before the lower-left origin turns y over).  The
 * start banners, the shadows and every other two-sided model depend on it;
 * drawing both faces left them to fight in the depth buffer. */
static int culled(const BrGfxVertex *a, const BrGfxVertex *b, const BrGfxVertex *c)
{
    float area = (b->x - a->x) * (c->y - a->y) - (c->x - a->x) * (b->y - a->y);
    if (area == 0.0f)
        return 1;
    return (s_st.cull == 1 /* negative */ && area < 0.0f)
        || (s_st.cull == 2 /* positive */ && area > 0.0f);
}

void br_gfx_triangle(const BrGfxVertex *a, const BrGfxVertex *b, const BrGfxVertex *c)
{
    brr_vertex v[3];
    gllog("triangle");
    gllog_vtx(a);
    gllog_vtx(b);
    gllog_vtx(c);
    if (culled(a, b, c))
        return;
    vert(&v[0], a);
    vert(&v[1], b);
    vert(&v[2], c);
    if (s_src_dirty && s_src_set)
        tex_resolve();
    scissor();
    brr_draw(&s_st, v, 3);
}

void br_gfx_polygon(int n, const BrGfxVertex vl[])
{
    brr_vertex v[3 * 64];
    int i, k = 0;
    gllog("polygon %d", n);
    for (i = 0; i < n && i < 16; i++)
        gllog_vtx(&vl[i]);
    if (s_src_dirty && s_src_set)
        tex_resolve();
    for (i = 1; i + 1 < n; i++) {
        if (culled(&vl[0], &vl[i], &vl[i + 1]))
            continue;
        if (k == 3 * 64) {
            scissor();
            brr_draw(&s_st, v, k);
            k = 0;
        }
        vert(&v[k++], &vl[0]);
        vert(&v[k++], &vl[i]);
        vert(&v[k++], &vl[i + 1]);
    }
    if (k) {
        scissor();
        brr_draw(&s_st, v, k);
    }
}

void br_gfx_pixels(uint32_t x, uint32_t y, uint32_t w, uint32_t h, int32_t stride, const void *data)
{
    int gy = s_lower_left ? s_h - (int)y - (int)h : (int)y, d[4];
    xmap m = map_box();
    gllog("pixels x %u y %u w %u h %u stride %d", x, y, w, h, stride);
    d[0] = (int)lround(m.ax * x + m.bx);
    d[1] = (int)lround(m.ay * gy + m.by);
    d[2] = (int)lround(m.ax * (x + w) + m.bx);
    d[3] = (int)lround(m.ay * (gy + (int)h) + m.by);
    brr_lfb_write((int)x, gy, (int)w, (int)h, (const uint16_t *)data, stride, d);
}
