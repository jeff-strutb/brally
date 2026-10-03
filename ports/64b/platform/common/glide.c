/* glide.c: Glide 2 for the game, over the renderer interface (brr.h).
 *
 * The board answers as a Voodoo Graphics with one TMU and 4 MB on each of
 * the frame buffer and the TMU. Texture memory is modelled as the card has
 * it: a download copies bytes to TMU memory, and grTexSource interprets the
 * bytes at its address with the format and size IT is given -- the game
 * downloads with one description and sources with another (the large font
 * page). A decoded RGBA8 copy is cached per (address, format, size) and
 * dropped when a download overwrites its bytes. */
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "plat.h"
#include "glide.h"
#include "brr.h"

#define TMU_RAM   (4u << 20)

static brr_state s_st;
static int s_w = 640, s_h = 480, s_lower_left, s_cformat;
static int s_depth_mode;

/* ---- the call log ------------------------------------------------------------- */
/* BR_GLLOG=PATH: during the frames BR_TRACE_FRAMES=A:B selects (script.c),
 * the Glide calls one per line with the vertices expanded, in the wasm
 * lane's text (ports/macos/wasm/host/host_glide.m), so the two streams diff
 * call by call. */
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

static void gllog_vtx(const GrVertex *v)
{
    if (g_plat_tracing && s_gllog)
        fprintf(s_gllog, "  v %.4g %.4g z%.4g w%.4g st %.4g %.4g rgba %.4g %.4g %.4g %.4g\n",
                v->x, v->y, v->ooz, v->oow, v->tmuvtx[0].sow, v->tmuvtx[0].tow, v->r, v->g, v->b, v->a);
}

/* ---- colours: GrColor_t in the format grSstWinOpen chose, to ARGB ----------- */
static uint32_t argb(GrColor_t c)
{
    uint32_t a, r, g, b;
    switch (s_cformat) {
    case 1: /* ABGR */ a = c >> 24; b = (c >> 16) & 0xFF; g = (c >> 8) & 0xFF; r = c & 0xFF; break;
    case 2: /* RGBA */ r = c >> 24; g = (c >> 16) & 0xFF; b = (c >> 8) & 0xFF; a = c & 0xFF; break;
    case 3: /* BGRA */ b = c >> 24; g = (c >> 16) & 0xFF; r = (c >> 8) & 0xFF; a = c & 0xFF; break;
    default: return c;
    }
    return a << 24 | r << 16 | g << 8 | b;
}

/* ---- start-up ------------------------------------------------------------------- */
void grGlideInit(void) { plat_vclock_import();}
void grGlideShutdown(void) { plat_vclock_import(); brr_close(); }

FxBool grSstQueryHardware(GrHwConfiguration *hw)
{ plat_vclock_import();
    memset(hw, 0, sizeof *hw);
    hw->num_sst = 1;
    hw->SSTs[0].type = GR_SSTTYPE_VOODOO;
    hw->SSTs[0].sstBoard.VoodooConfig.fbRam = 4;
    hw->SSTs[0].sstBoard.VoodooConfig.fbiRev = 2;
    hw->SSTs[0].sstBoard.VoodooConfig.nTexelfx = 1;
    hw->SSTs[0].sstBoard.VoodooConfig.sliDetect = 0;
    hw->SSTs[0].sstBoard.VoodooConfig.tmuConfig[0].tmuRev = 1;
    hw->SSTs[0].sstBoard.VoodooConfig.tmuConfig[0].tmuRam = 4;
    return 1;
}

void grSstSelect(int which) { plat_vclock_import(); (void)which; }

static const short k_res[16][2] = {
    { 320, 200 }, { 320, 240 }, { 400, 256 }, { 512, 384 }, { 640, 200 }, { 640, 350 },
    { 640, 400 }, { 640, 480 }, { 800, 600 }, { 960, 720 }, { 856, 480 }, { 512, 256 },
    { 1024, 768 }, { 1280, 1024 }, { 1600, 1200 }, { 400, 300 }
};

FxBool grSstWinOpen(void *hwnd, GrScreenResolution_t res, GrScreenRefresh_t ref,
                    GrColorFormat_t cformat, GrOriginLocation_t org, int ncol, int naux)
{ plat_vclock_import();
    (void)hwnd; (void)ref; (void)ncol; (void)naux;
    if (res >= 0 && res < 16) {
        s_w = k_res[res][0];
        s_h = k_res[res][1];
    }
    s_cformat = cformat;
    s_lower_left = org == 1;
    s_st.clip_x0 = 0;
    s_st.clip_y0 = 0;
    s_st.clip_x1 = s_w;
    s_st.clip_y1 = s_h;
    s_st.blend_rgb_src = 4;     /* GR_BLEND_ONE */
    s_st.blend_rgb_dst = 0;     /* GR_BLEND_ZERO */
    s_st.blend_a_src = 4;
    s_st.blend_a_dst = 0;
    s_st.atest_fn = 7;          /* GR_CMP_ALWAYS */
    s_st.depth_fn = 1;          /* GR_CMP_LESS */
    s_st.depth_mask = 1;
    return brr_open(s_w, s_h);
}

void grSstWinClose(void) { plat_vclock_import();}

/* ---- frame ------------------------------------------------------------------------- */
void grClipWindow(FxU32 x0, FxU32 y0, FxU32 x1, FxU32 y1)
{ plat_vclock_import();
    gllog("grClipWindow %u %u %u %u", x0, y0, x1, y1);
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

void grBufferClear(GrColor_t color, GrAlpha_t alpha, FxU16 depth)
{ plat_vclock_import();
    uint32_t c = (argb(color) & 0x00FFFFFFu) | (uint32_t)alpha << 24;
    gllog("grBufferClear %08X %u %u", color, alpha, depth);
    brr_clear(c, depth / 65536.0f, 1, s_st.depth_mode != 0, &s_st);
}

void plat_text_swap(void);         /* script_game.c */

void grBufferSwap(int interval)
{ plat_vclock_import();
    (void)interval;
    gllog("grBufferSwap");
    plat_text_swap();
    brr_present();
    plat_pump(0);
}

int grBufferNumPending(void) { plat_vclock_import(); return 0; }

/* ---- state ----------------------------------------------------------------------------- */
void grColorCombine(GrCombineFunction_t f, GrCombineFactor_t fa, GrCombineLocal_t l, GrCombineOther_t o, FxBool inv)
{ plat_vclock_import();
    gllog("grColorCombine %u %u %u %u %u", f, fa, l, o, inv);
    s_st.cc_fn = f; s_st.cc_factor = fa; s_st.cc_local = l; s_st.cc_other = o; s_st.cc_invert = inv;
}
void grAlphaCombine(GrCombineFunction_t f, GrCombineFactor_t fa, GrCombineLocal_t l, GrCombineOther_t o, FxBool inv)
{ plat_vclock_import();
    gllog("grAlphaCombine %u %u %u %u %u", f, fa, l, o, inv);
    s_st.ac_fn = f; s_st.ac_factor = fa; s_st.ac_local = l; s_st.ac_other = o; s_st.ac_invert = inv;
}
void grAlphaBlendFunction(GrAlphaBlendFnc_t rs, GrAlphaBlendFnc_t rd, GrAlphaBlendFnc_t as, GrAlphaBlendFnc_t ad)
{ plat_vclock_import();
    gllog("grAlphaBlendFunction %u %u %u %u", rs, rd, as, ad);
    s_st.blend_rgb_src = rs; s_st.blend_rgb_dst = rd; s_st.blend_a_src = as; s_st.blend_a_dst = ad;
}
void grAlphaTestFunction(GrCmpFnc_t f) { plat_vclock_import(); gllog("grAlphaTestFunction %d", f); s_st.atest_fn = f; }
void grAlphaTestReferenceValue(GrAlpha_t v) { plat_vclock_import(); gllog("grAlphaTestReferenceValue %u", v); s_st.atest_ref = v; }
void grConstantColorValue(GrColor_t v) { plat_vclock_import(); gllog("grConstantColorValue %08X", v); s_st.constant = argb(v); }
void grCullMode(GrCullMode_t m) { plat_vclock_import(); gllog("grCullMode %d", m); s_st.cull = m; }
void grDepthBufferMode(GrDepthBufferMode_t m) { plat_vclock_import(); gllog("grDepthBufferMode %u", m); s_depth_mode = m; s_st.depth_mode = m; }
void grDepthBufferFunction(GrCmpFnc_t f) { plat_vclock_import(); gllog("grDepthBufferFunction %d", f); s_st.depth_fn = f; }
void grDepthMask(FxBool m) { plat_vclock_import(); gllog("grDepthMask %d", m); s_st.depth_mask = m; }
void grFogMode(GrFogMode_t m) { plat_vclock_import(); gllog("grFogMode %d", m); s_st.fog_mode = m; }
void grFogColorValue(GrColor_t c) { plat_vclock_import(); s_st.fog_color = argb(c); }
void grFogTable(const GrFog_t ft[GR_FOG_TABLE_SIZE]) { plat_vclock_import(); memcpy(s_st.fog_table, ft, GR_FOG_TABLE_SIZE); }

/* Glide's table entry i covers w = 2^(3 + i/4) / (8 - i%4) */
static float fog_w(int i) { return (float)(pow(2.0, 3.0 + (double)(i >> 2)) / (8 - (i & 3))); }

void guFogGenerateLinear(GrFog_t ft[GR_FOG_TABLE_SIZE], float nearZ, float farZ)
{ plat_vclock_import();
    int i;
    for (i = 0; i < GR_FOG_TABLE_SIZE; i++) {
        float f = (fog_w(i) - nearZ) / (farZ - nearZ);
        ft[i] = (GrFog_t)(f < 0 ? 0 : f > 1 ? 255 : f * 255.0f);
    }
}

void grTexCombine(GrChipID_t tmu, GrCombineFunction_t rf, GrCombineFactor_t rfa, GrCombineFunction_t af,
                  GrCombineFactor_t afa, FxBool ri, FxBool ai)
{ plat_vclock_import();
    gllog("grTexCombine %u %u %u %u %u %u", rf, rfa, af, afa, ri, ai);
    if (tmu != 0)
        return;
    s_st.tc_rgb_fn = rf; s_st.tc_rgb_factor = rfa; s_st.tc_alpha_fn = af;
    s_st.tc_alpha_factor = afa; s_st.tc_rgb_invert = ri; s_st.tc_alpha_invert = ai;
}
void grTexFilterMode(GrChipID_t tmu, GrTextureFilterMode_t mn, GrTextureFilterMode_t mg)
{ plat_vclock_import();
    if (tmu == 0) { s_st.min_filter = mn; s_st.mag_filter = mg; }
}
void grTexClampMode(GrChipID_t tmu, GrTextureClampMode_t s, GrTextureClampMode_t t)
{ plat_vclock_import();
    if (tmu == 0) { s_st.clamp_s = s; s_st.clamp_t = t; }
}
void grTexMipMapMode(GrChipID_t tmu, GrMipMapMode_t m, FxBool b) { plat_vclock_import(); (void)tmu; (void)m; (void)b; }
void grTexLodBiasValue(GrChipID_t tmu, float bias) { plat_vclock_import(); (void)tmu; (void)bias; }

/* ---- textures ----------------------------------------------------------------------------- */
static int lod_side(GrLOD_t l) { return 256 >> l; }

static void dims(GrLOD_t lod, GrAspectRatio_t ar, int *w, int *h)
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

static int texel_bytes(GrTextureFormat_t f) { return f >= 8 ? 2 : 1; }

static FxU32 mem_required(GrLOD_t small, GrLOD_t large, GrAspectRatio_t ar, GrTextureFormat_t f)
{
    FxU32 n = 0;
    GrLOD_t l;
    for (l = large; l <= small; l++) {
        int w, h;
        dims(l, ar, &w, &h);
        n += (FxU32)(w * h * texel_bytes(f));
    }
    return (n + 7) & ~7u;
}

FxU32 grTexCalcMemRequired(GrLOD_t small, GrLOD_t large, GrAspectRatio_t ar, GrTextureFormat_t f)
{ plat_vclock_import();
    return mem_required(small, large, ar, f);
}

FxU32 grTexTextureMemRequired(FxU32 evenOdd, GrTexInfo *info)
{ plat_vclock_import();
    (void)evenOdd;
    return mem_required(info->smallLod, info->largeLod, info->aspectRatio, info->format);
}

FxU32 grTexMinAddress(GrChipID_t tmu) { plat_vclock_import(); (void)tmu; return 0; }
FxU32 grTexMaxAddress(GrChipID_t tmu) { plat_vclock_import(); (void)tmu; return TMU_RAM - 8; }

static uint8_t s_tmem[TMU_RAM];

/* a decoded texture: the bytes [start, end) read as fmt at w x h */
typedef struct ptex { FxU32 start, end; GrTextureFormat_t fmt; int w, h; uint32_t id; } ptex;
#define PTEX_MAX 2048
static ptex s_tex[PTEX_MAX];
static int s_ntex;

static void decode(GrTextureFormat_t f, const uint8_t *src, uint8_t *dst, int n)
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
static FxU32 s_src_start;
static GrTexInfo s_src_info;
static int s_src_set, s_src_dirty;

static void tex_resolve(void);

void grTexDownloadMipMap(GrChipID_t tmu, FxU32 start, FxU32 evenOdd, GrTexInfo *info)
{ plat_vclock_import();
    FxU32 n, end;
    int i;
    (void)evenOdd;
    if (tmu != 0 || !info || !info->data || start >= TMU_RAM)
        return;
    n = mem_required(info->smallLod, info->largeLod, info->aspectRatio, info->format);
    if (start + n > TMU_RAM)
        n = TMU_RAM - start;
    if (g_plat_tracing && s_gllog) {
        uint32_t hsh = 2166136261u, k;
        for (k = 0; k < n; k++)
            hsh = (hsh ^ ((const uint8_t *)info->data)[k]) * 16777619u;
        gllog("grTexDownloadMipMap start %u n %u fmt %u hash %08X", start, n, info->format, hsh);
    }
    memcpy(s_tmem + start, info->data, n);
    end = start + n;
    /* anything decoded from the bytes just overwritten is stale */
    for (i = 0; i < s_ntex; i++)
        if (s_tex[i].id && s_tex[i].start < end && start < s_tex[i].end) {
            if (s_tex[i].id == s_st.texture)
                s_src_dirty = 1;
            brr_texture_free(s_tex[i].id);
            s_tex[i] = s_tex[--s_ntex];
            i--;
        }
}

void grTexSource(GrChipID_t tmu, FxU32 start, FxU32 evenOdd, GrTexInfo *info)
{ plat_vclock_import();
    (void)evenOdd;
    if (info)
        gllog("grTexSource start %u fmt %u large %u aspect %u", start, info->format, info->largeLod, info->aspectRatio);
    if (tmu != 0)
        return;
    s_st.texture = 0;
    s_src_set = 0;
    if (!info || start >= TMU_RAM)
        return;
    s_src_start = start;
    s_src_info = *info;
    s_src_set = 1;
    tex_resolve();
}

static void tex_resolve(void)
{
    int i, w, h;
    FxU32 n, start = s_src_start;
    uint8_t *rgba;
    const GrTexInfo *info = &s_src_info;
    s_src_dirty = 0;
    s_st.texture = 0;
    dims(info->largeLod, info->aspectRatio, &w, &h);
    s_st.tex_w = w;
    s_st.tex_h = h;
    for (i = 0; i < s_ntex; i++)
        if (s_tex[i].start == start && s_tex[i].fmt == info->format && s_tex[i].w == w && s_tex[i].h == h) {
            s_st.texture = s_tex[i].id;
            return;
        }
    n = (FxU32)(w * h * texel_bytes(info->format));
    if (start + n > TMU_RAM)
        return;
    rgba = (uint8_t *)malloc((size_t)w * (size_t)h * 4);
    if (!rgba)
        return;
    decode(info->format, s_tmem + start, rgba, w * h);     /* the largest level */
    if (s_ntex == PTEX_MAX) {
        brr_texture_free(s_tex[0].id);
        s_tex[0] = s_tex[--s_ntex];
    }
    s_tex[s_ntex].start = start;
    s_tex[s_ntex].end = start + mem_required(info->smallLod, info->largeLod, info->aspectRatio, info->format);
    s_tex[s_ntex].fmt = info->format;
    s_tex[s_ntex].w = w;
    s_tex[s_ntex].h = h;
    s_tex[s_ntex].id = brr_texture(0, rgba, w, h);
    s_st.texture = s_tex[s_ntex].id;
    s_ntex++;
    free(rgba);
}

/* ---- drawing ------------------------------------------------------------------------------ */
static void vert(brr_vertex *o, const GrVertex *v)
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
    o->s = v->tmuvtx[0].sow * w / su;
    o->t = v->tmuvtx[0].tow * w / sv;
}

/* Glide's triangle setup drops a triangle with no area, and with culling on
 * drops the one facing away: the sign of its area in the coordinates the
 * application passed (before the lower-left origin turns y over).  The
 * start banners, the shadows and every other two-sided model depend on it;
 * drawing both faces left them to fight in the depth buffer. */
static int culled(const GrVertex *a, const GrVertex *b, const GrVertex *c)
{
    float area = (b->x - a->x) * (c->y - a->y) - (c->x - a->x) * (b->y - a->y);
    if (area == 0.0f)
        return 1;
    return (s_st.cull == 1 /* GR_CULL_NEGATIVE */ && area < 0.0f)
        || (s_st.cull == 2 /* GR_CULL_POSITIVE */ && area > 0.0f);
}

void grDrawTriangle(const GrVertex *a, const GrVertex *b, const GrVertex *c)
{ plat_vclock_import();
    brr_vertex v[3];
    gllog("grDrawTriangle");
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
    brr_draw(&s_st, v, 3);
}

void grDrawPolygonVertexList(int n, const GrVertex vl[])
{ plat_vclock_import();
    brr_vertex v[3 * 64];
    int i, k = 0;
    gllog("grDrawPolygonVertexList %d", n);
    for (i = 0; i < n && i < 16; i++)
        gllog_vtx(&vl[i]);
    if (s_src_dirty && s_src_set)
        tex_resolve();
    for (i = 1; i + 1 < n; i++) {
        if (culled(&vl[0], &vl[i], &vl[i + 1]))
            continue;
        if (k == 3 * 64) {
            brr_draw(&s_st, v, k);
            k = 0;
        }
        vert(&v[k++], &vl[0]);
        vert(&v[k++], &vl[i]);
        vert(&v[k++], &vl[i + 1]);
    }
    if (k)
        brr_draw(&s_st, v, k);
}

FxBool grLfbWriteRegion(GrBuffer_t dst, FxU32 x, FxU32 y, GrLfbSrcFmt_t fmt, FxU32 w, FxU32 h,
                        FxI32 stride, void *data)
{ plat_vclock_import();
    (void)dst;
    gllog("grLfbWriteRegion buf %u x %u y %u fmt %u w %u h %u stride %d", dst, x, y, fmt, w, h, stride);
    if (fmt == 0) {              /* GR_LFB_SRC_FMT_565 */
        brr_lfb_write((int)x, s_lower_left ? s_h - (int)y - (int)h : (int)y, (int)w, (int)h,
                      (const uint16_t *)data, stride);
        return 1;
    }
    PLOG("grLfbWriteRegion: format %d not handled\n", fmt);
    return 0;
}
