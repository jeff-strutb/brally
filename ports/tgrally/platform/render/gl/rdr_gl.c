/* rdr_gl.c: the renderer interface (render/rdr.h) on OpenGL 3.3 core, the
 * fallback for a PC whose GPU has no Vulkan driver.
 *
 * rdr_vulkan.c's design, call for call: the frame is drawn offscreen, 4x
 * multisampled with a depth buffer, at the window's resolution; the colour
 * combiner, the tiles' wrap, mirror and mask rules and the alpha compare run
 * in the fragment shader (rdr_vulkan's shaders/rcp.frag, the same lines in
 * GLSL 3.30) from each draw's RDP state; the blender's modes are
 * fixed-function blends; a finished frame is resolved, then scaled into the
 * window, letterboxed, through the VI's gamma when it is on.
 *
 * The target's rows are stored top first, as Vulkan's are (the clip-space
 * mapping is rcp.vert's, unchanged): scissors, gl_FragCoord and the
 * read-back need no flip; only the blit into the window turns it over.
 *
 * The context and the GL functions are Boss Rally's GL renderer's
 * (ports/brally/platform/render/gl: gl_ctx.c, gl_api.h). On the Mac the
 * context is offscreen (its windows draw with Metal): there it is a check. */
#include <stddef.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "../rdr.h"
#include "host.h"
#include "gl/gl_api.h"

/* ---- the draw's state: the shader's U block, std140 ------------------------------------ */
typedef struct TileU { float org[4]; int32_t size[4], wrap[4], cl[4]; } TileU;
typedef struct U {
    int32_t cc[4][4];
    float prim[4], env[4], fog[4], blendc[4];
    float plod, scale;                                /* scale: target pixels per N64 pixel */
    int32_t cycle, filter, fog_blend, alpha_cmp, ntex, balpha, lodn, seed;
    float fb[2];
    TileU tile[10];
} U;
_Static_assert(offsetof(U, prim) == 64 && offsetof(U, plod) == 128 && offsetof(U, fb) == 168 &&
               offsetof(U, tile) == 176 && sizeof(U) == 816, "U must match the shader's block");

#define SAMPLES   4                                   /* multisampling: the RDP's edge coverage */
#define RING_SIZE (16 << 20)
#define NTEX      10

/* ---- the shaders: rdr_vulkan's rcp.vert, rcp.frag, blit.* ------------------------------ */
#define U_BLOCK \
    "struct Tile { vec4 org; ivec4 size; ivec4 wrap; ivec4 cl; };\n" \
    "layout(std140) uniform U {\n" \
    "    ivec4 cc[4]; vec4 prim; vec4 env; vec4 fog; vec4 blendc;\n" \
    "    float plod; float scale; int cycle; int filt; int fog_blend; int alpha_cmp; int ntex; int balpha;\n" \
    "    int lodn; int seed; vec2 fb; Tile tile[10];\n" \
    "} u;\n"

static const char k_vs[] =
    "#version 330 core\n" U_BLOCK
    "layout(location = 0) in vec4 a_pos;\n"
    "layout(location = 1) in vec2 a_st;\n"
    "layout(location = 2) in vec4 a_rgba;\n"
    "out vec2 v_st;\n"
    "out vec4 v_rgba;\n"
    "void main()\n"
    "{\n"
    "    float w = a_pos.w;\n"
    /* depth: GL's -1..1 becomes Vulkan's (z + w) / 2 through the default depth range */
    "    gl_Position = vec4(a_pos.x / u.fb.x * 2.0 - w, a_pos.y / u.fb.y * 2.0 - w, a_pos.z, w);\n"
    "    v_st = a_st;\n"
    "    v_rgba = a_rgba;\n"
    "}\n";

static const char k_fs[] =
    "#version 330 core\n" U_BLOCK
    "uniform sampler2D tex[10];\n"
    "in vec2 v_st;\n"
    "in vec4 v_rgba;\n"
    "out vec4 o_col;\n"
    "float noise8(vec2 p, int sd)\n"
    "{\n"
    "    uint h = uint(p.x) * 1973u + uint(p.y) * 9277u + uint(sd) * 26699u;\n"
    "    h = (h ^ (h >> 13)) * 1274126177u;\n"
    "    return float((h ^ (h >> 16)) & 255u) / 255.0;\n"
    "}\n"
    "int wrapi(int i, int n, int clampv, int mirror, int mask, int cw)\n"
    "{\n"
    "    if (clampv != 0 && cw > 0)\n"
    "        i = clamp(i, 0, cw - 1);\n"
    "    if (mask > 0) {\n"
    "        int m = ((i % mask) + mask) % mask;\n"
    "        if (mirror != 0 && (((i < 0 ? -i - 1 : i) / mask) & 1) != 0)\n"
    "            m = mask - 1 - m;\n"
    "        return min(m, n - 1);\n"
    "    }\n"
    "    return clamp(i, 0, n - 1);\n"
    "}\n"
    "vec4 fetch(int k, ivec2 p)\n"
    "{\n"
    "    switch (k) {\n"
    "    case 0: return texelFetch(tex[0], p, 0);\n"
    "    case 1: return texelFetch(tex[1], p, 0);\n"
    "    case 2: return texelFetch(tex[2], p, 0);\n"
    "    case 3: return texelFetch(tex[3], p, 0);\n"
    "    case 4: return texelFetch(tex[4], p, 0);\n"
    "    case 5: return texelFetch(tex[5], p, 0);\n"
    "    case 6: return texelFetch(tex[6], p, 0);\n"
    "    case 7: return texelFetch(tex[7], p, 0);\n"
    "    case 8: return texelFetch(tex[8], p, 0);\n"
    "    default: return texelFetch(tex[9], p, 0);\n"
    "    }\n"
    "}\n"
    "vec4 texel(int k, vec2 st, int filt)\n"
    "{\n"
    "    Tile tl = u.tile[k];\n"
    "    float x = st.x * tl.org.z - tl.org.x, y = st.y * tl.org.w - tl.org.y;\n"
    "    int w = tl.size.x, h = tl.size.y;\n"
    "    if (filt == 0) {\n"
    "        int ix = wrapi(int(floor(x)), w, tl.wrap.x, tl.wrap.z, tl.size.z, tl.cl.x);\n"
    "        int iy = wrapi(int(floor(y)), h, tl.wrap.y, tl.wrap.w, tl.size.w, tl.cl.y);\n"
    "        return fetch(k, ivec2(ix, iy));\n"
    "    }\n"
    "    float fx = x - floor(x), fy = y - floor(y);\n"
    "    int x0 = int(floor(x)), y0 = int(floor(y));\n"
    "    int xa = wrapi(x0, w, tl.wrap.x, tl.wrap.z, tl.size.z, tl.cl.x), xb = wrapi(x0 + 1, w, tl.wrap.x, tl.wrap.z, tl.size.z, tl.cl.x);\n"
    "    int ya = wrapi(y0, h, tl.wrap.y, tl.wrap.w, tl.size.w, tl.cl.y), yb = wrapi(y0 + 1, h, tl.wrap.y, tl.wrap.w, tl.size.w, tl.cl.y);\n"
    "    vec4 p00 = fetch(k, ivec2(xa, ya)), p10 = fetch(k, ivec2(xb, ya)), p01 = fetch(k, ivec2(xa, yb)), p11 = fetch(k, ivec2(xb, yb));\n"
    "    return fx + fy < 1.0 ? p00 + fx * (p10 - p00) + fy * (p01 - p00)\n"
    "                         : p11 + (1.0 - fx) * (p01 - p11) + (1.0 - fy) * (p10 - p11);\n"
    "}\n"
    "vec4 inp(int c, vec4 comb, vec4 t0, vec4 t1, vec4 sh, float lfrac)\n"
    "{\n"
    "    switch (c) {\n"
    "    case 0: return comb;\n"
    "    case 1: return t0;\n"
    "    case 2: return t1;\n"
    "    case 3: return u.prim;\n"
    "    case 4: return sh;\n"
    "    case 5: return u.env;\n"
    "    case 6: return vec4(1.0);\n"
    "    case 8: return vec4(0.5);\n"
    "    case 11: return vec4(comb.a);\n"
    "    case 12: return vec4(t0.a);\n"
    "    case 13: return vec4(t1.a);\n"
    "    case 14: return vec4(u.prim.a);\n"
    "    case 15: return vec4(sh.a);\n"
    "    case 16: return vec4(u.env.a);\n"
    "    case 17: return vec4(lfrac);\n"
    "    case 18: return vec4(u.plod);\n"
    "    default: return vec4(0.0);\n"
    "    }\n"
    "}\n"
    "void main()\n"
    "{\n"
    "    vec4 t0 = vec4(0.0), t1 = vec4(0.0), comb = vec4(0.0);\n"
    "    float lfrac = 0.0;\n"
    "    if (u.lodn > 0) {\n"
    "        vec2 dx = dFdx(v_st), dy = dFdy(v_st);\n"
    "        float lod = max(max(abs(dx.x), abs(dx.y)), max(abs(dy.x), abs(dy.y))) * u.scale;\n"
    "        int level = 0;\n"
    "        if (lod >= 1.0) {\n"
    "            level = min(int(floor(log2(lod))), 7);\n"
    "            lfrac = min(lod / exp2(float(level)) - 1.0, 1.0);\n"
    "        }\n"
    "        int a = min(level, u.lodn - 1), b = min(level + 1, u.lodn - 1);\n"
    "        if (level >= u.lodn - 1)\n"
    "            lfrac = 1.0;\n"
    "        t0 = texel(2 + a, v_st, u.filt);\n"
    "        t1 = texel(2 + b, v_st, u.filt);\n"
    "    } else {\n"
    "        if ((u.ntex & 1) != 0)\n"
    "            t0 = texel(0, v_st, u.filt);\n"
    "        if ((u.ntex & 2) != 0)\n"
    "            t1 = texel(1, v_st, u.filt);\n"
    "    }\n"
    "    for (int c = 0; c < u.cycle; c++) {\n"
    "        ivec4 r = u.cc[c * 2], al = u.cc[c * 2 + 1];\n"
    "        vec3 rgb = (inp(r.x, comb, t0, t1, v_rgba, lfrac).rgb - inp(r.y, comb, t0, t1, v_rgba, lfrac).rgb) *\n"
    "                   inp(r.z, comb, t0, t1, v_rgba, lfrac).rgb + inp(r.w, comb, t0, t1, v_rgba, lfrac).rgb;\n"
    "        float alpha = (inp(al.x, comb, t0, t1, v_rgba, lfrac).a - inp(al.y, comb, t0, t1, v_rgba, lfrac).a) *\n"
    "                      inp(al.z, comb, t0, t1, v_rgba, lfrac).a + inp(al.w, comb, t0, t1, v_rgba, lfrac).a;\n"
    "        comb = clamp(vec4(rgb, alpha), 0.0, 1.0);\n"
    "    }\n"
    "    if (u.alpha_cmp == 1 && comb.a < u.blendc.a) discard;\n"
    "    if (u.alpha_cmp == 2 && comb.a < noise8(floor(gl_FragCoord.xy / u.scale), u.seed)) discard;\n"
    "    if (u.alpha_cmp == 3 && comb.a < 0.5) discard;\n"
    "    if (u.alpha_cmp == 4 && comb.a < 1.0 / 255.0) discard;\n"
    "    if (u.fog_blend != 0)\n"
    "        comb.rgb = mix(comb.rgb, u.fog.rgb, v_rgba.a);\n"
    "    if (u.balpha == 1)\n"
    "        comb.a = u.fog.a;\n"
    "    else if (u.balpha == 2)\n"
    "        comb.a = v_rgba.a;\n"
    "    o_col = comb;\n"
    "}\n";

/* the frame into the window: r is x, y, width, height in Vulkan's normalised
   coordinates (y down), turned over for GL's window (y up) */
static const char k_blit_vs[] =
    "#version 330 core\n"
    "uniform vec4 r;\n"
    "out vec2 v_uv;\n"
    "void main()\n"
    "{\n"
    "    vec2 q = vec2((gl_VertexID & 1) != 0 ? 1.0 : 0.0, (gl_VertexID & 2) != 0 ? 1.0 : 0.0);\n"
    "    gl_Position = vec4(r.x + q.x * r.z, -(r.y + q.y * r.w), 0.0, 1.0);\n"
    "    v_uv = q;\n"
    "}\n";

static const char k_blit_fs[] =
    "#version 330 core\n"
    "uniform sampler2D frame;\n"
    "uniform int gamma;\n"
    "in vec2 v_uv;\n"
    "out vec4 o_col;\n"
    "void main()\n"
    "{\n"
    "    vec4 c = texture(frame, v_uv);\n"
    "    if (gamma != 0)\n"
    "        c.rgb = sqrt(c.rgb);\n"
    "    o_col = c;\n"
    "}\n";

/* ---- state ------------------------------------------------------------------------------ */
static int      s_ok, s_failed;
static void    *s_window;                     /* the host's window (rdr_window) */
static GLuint   s_prog, s_blit, s_vao, s_ubo, s_vbo, s_lin_sampler;
static GLint    s_ualign, s_loc_r, s_loc_gamma;
static long     s_ubo_used, s_vbo_used;
static GLuint  *s_tex;                        /* handle -> texture; [0]: the empty 1x1 */
static int     *s_tex_w, *s_tex_h;
static int      s_ntex, s_captex;
static GLuint   s_bound[NTEX];
static GLuint   s_ms_fbo, s_ms_col, s_ms_dep, s_fbo, s_col;
static int      s_tw, s_th, s_first, s_in_frame, s_gamma;
static float    s_fb_w, s_fb_h, s_sx, s_sy, s_scale;
static uint32_t s_frames;                     /* the noise's seed */
/* the finished frames read back on the GL thread (rdr_frame_end) for rdr_frame_pixels, which
   runs on the retrace's thread (no context there): every frame headless, else once asked */
static host_mutex *s_pix_mu;
static uint32_t *s_last, *s_pixels;
static int      s_last_w, s_last_h, s_last_cap, s_pix_cap;
static volatile int s_want;

int rdr_covers(void) { return 0; }
int rdr_presents(void) { return 1; }
void rdr_vi(uint32_t ctrl) { s_gamma = (ctrl & 0x08) != 0; }

/* the host's window, on the main thread when it opens; the context is made on
 * it when the renderer starts (on the Mac, offscreen: its windows are Metal's) */
void rdr_window(void)
{
#if !defined(__APPLE__)
    s_window = host_window_handle();
#endif
}

/* ---- start-up ----------------------------------------------------------------------------- */
static GLuint shader(GLenum kind, const char *src)
{
    GLuint s = glCreateShader(kind);
    GLint ok = 0;
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetShaderInfoLog(s, sizeof log, NULL, log);
        fprintf(stderr, "rdr_gl: shader: %s\n", log);
        return 0;
    }
    return s;
}

static GLuint program(const char *vs, const char *fs)
{
    GLuint v = shader(GL_VERTEX_SHADER, vs), f = shader(GL_FRAGMENT_SHADER, fs), p;
    GLint ok = 0;
    if (!v || !f)
        return 0;
    p = glCreateProgram();
    glAttachShader(p, v);
    glAttachShader(p, f);
    glBindAttribLocation(p, 0, "a_pos");
    glBindAttribLocation(p, 1, "a_st");
    glBindAttribLocation(p, 2, "a_rgba");
    glBindFragDataLocation(p, 0, "o_col");
    glLinkProgram(p);
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetProgramInfoLog(p, sizeof log, NULL, log);
        fprintf(stderr, "rdr_gl: link: %s\n", log);
        return 0;
    }
    return p;
}

static GLuint tex_new(const uint8_t *rgba, int w, int h)
{
    GLuint t;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);   /* complete without mips */
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    return t;
}

int rdr_init(void)
{
    static const uint8_t empty[4] = { 0, 0, 0, 0 };
    char name[8];
    int k;
    if (s_ok)
        return 1;
    if (s_failed)
        return 0;
    s_failed = 1;
    if (!gl_ctx_open(s_window) || !gl_api_load())
        return 0;
    fprintf(stderr, "rdr_gl: %s (%s)%s\n", (const char *)glGetString(GL_RENDERER),
            (const char *)glGetString(GL_VERSION), s_window ? "" : ", offscreen");
    if (!(s_prog = program(k_vs, k_fs)) || !(s_blit = program(k_blit_vs, k_blit_fs)))
        return 0;
    glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &s_ualign);
    if (s_ualign < 16)
        s_ualign = 16;
    glUniformBlockBinding(s_prog, glGetUniformBlockIndex(s_prog, "U"), 0);
    glUseProgram(s_prog);
    for (k = 0; k < NTEX; k++) {
        snprintf(name, sizeof name, "tex[%d]", k);
        glUniform1i(glGetUniformLocation(s_prog, name), k);
    }
    glUseProgram(s_blit);
    glUniform1i(glGetUniformLocation(s_blit, "frame"), 0);
    s_loc_r = glGetUniformLocation(s_blit, "r");
    s_loc_gamma = glGetUniformLocation(s_blit, "gamma");

    glGenVertexArrays(1, &s_vao);
    glBindVertexArray(s_vao);
    glGenBuffers(1, &s_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
    glBufferData(GL_ARRAY_BUFFER, RING_SIZE, NULL, GL_STREAM_DRAW);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(RdrVtx), (void *)offsetof(RdrVtx, x));
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(RdrVtx), (void *)offsetof(RdrVtx, s));
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(RdrVtx), (void *)offsetof(RdrVtx, r));
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glGenBuffers(1, &s_ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, s_ubo);
    glBufferData(GL_UNIFORM_BUFFER, RING_SIZE, NULL, GL_STREAM_DRAW);

    glGenSamplers(1, &s_lin_sampler);                 /* the blit's: bilinear, clamped */
    glSamplerParameteri(s_lin_sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glSamplerParameteri(s_lin_sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glSamplerParameteri(s_lin_sampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glSamplerParameteri(s_lin_sampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    s_captex = 256;
    s_tex = (GLuint *)calloc((size_t)s_captex, sizeof *s_tex);
    s_tex_w = (int *)calloc((size_t)s_captex, sizeof *s_tex_w);
    s_tex_h = (int *)calloc((size_t)s_captex, sizeof *s_tex_h);
    s_tex[0] = tex_new(empty, 1, 1);
    s_tex_w[0] = s_tex_h[0] = 1;
    s_ntex = 1;
    s_pix_mu = host_mutex_new();
    glEnable(GL_DEPTH_CLAMP);                         /* the RSP clips on w, not on z */
    glDisable(GL_CULL_FACE);                          /* the RSP culls */
    s_ok = 1;
    s_failed = 0;
    return 1;
}

/* ---- textures ------------------------------------------------------------------------------ */
int rdr_texture(const uint8_t *rgba, int w, int h)
{
    int i;
    if (!s_ok || w <= 0 || h <= 0)
        return 0;
    for (i = 1; i < s_ntex; i++)
        if (!s_tex[i])
            break;
    if (i == s_ntex) {
        if (s_ntex == s_captex) {
            s_captex *= 2;
            s_tex = (GLuint *)realloc(s_tex, sizeof *s_tex * (size_t)s_captex);
            s_tex_w = (int *)realloc(s_tex_w, sizeof *s_tex_w * (size_t)s_captex);
            s_tex_h = (int *)realloc(s_tex_h, sizeof *s_tex_h * (size_t)s_captex);
        }
        s_ntex++;
    }
    glActiveTexture(GL_TEXTURE0);                     /* tex_new binds on unit 0 */
    s_bound[0] = 0;
    s_tex[i] = tex_new(rgba, w, h);
    s_tex_w[i] = w;
    s_tex_h[i] = h;
    return i;
}

void rdr_texture_free(int tex)
{
    int k;
    if (tex <= 0 || tex >= s_ntex || !s_tex[tex])
        return;
    for (k = 0; k < NTEX; k++)
        if (s_bound[k] == s_tex[tex])
            s_bound[k] = 0;
    glDeleteTextures(1, &s_tex[tex]);                 /* GL keeps it while queued draws read it */
    s_tex[tex] = 0;
}

/* ---- targets ------------------------------------------------------------------------------ */
static void targets_destroy(void)
{
    if (s_ms_fbo) {
        glDeleteFramebuffers(1, &s_ms_fbo);
        glDeleteFramebuffers(1, &s_fbo);
        glDeleteTextures(1, &s_ms_col);
        glDeleteTextures(1, &s_ms_dep);
        glDeleteTextures(1, &s_col);
    }
    s_ms_fbo = s_fbo = s_ms_col = s_ms_dep = s_col = 0;
}

static int targets_make(int w, int h)
{
    targets_destroy();
    glGenTextures(1, &s_ms_col);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, s_ms_col);
    glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, SAMPLES, GL_RGBA8, w, h, GL_TRUE);
    glGenTextures(1, &s_ms_dep);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, s_ms_dep);
    glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, SAMPLES, GL_DEPTH_COMPONENT32F, w, h, GL_TRUE);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0);
    glGenFramebuffers(1, &s_ms_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, s_ms_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, s_ms_col, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D_MULTISAMPLE, s_ms_dep, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "rdr_gl: the multisampled target is incomplete\n");
        return 0;
    }
    glActiveTexture(GL_TEXTURE0);
    s_bound[0] = 0;
    glGenTextures(1, &s_col);                         /* the resolved frame */
    glBindTexture(GL_TEXTURE_2D, s_col);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glGenFramebuffers(1, &s_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, s_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, s_col, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "rdr_gl: the frame target is incomplete\n");
        return 0;
    }
    s_tw = w;
    s_th = h;
    s_first = 1;
    return 1;
}

/* the window's drawing area (0 x 0 offscreen) */
static void window_size(int *w, int *h)
{
    *w = *h = 0;
    if (s_window && host_window_visible())
        host_window_pixels(w, h);
}

/* the target: the frame scaled to the largest area of its shape the window
 * holds (the whole window within two pixels), or by TGR_SCALE without one */
static void target_size(float fb_w, float fb_h, int *tw, int *th)
{
    int dw, dh;
    float k;
    window_size(&dw, &dh);
    if (dw > 0 && dh > 0) {
        k = fminf(dw / fb_w, dh / fb_h);
        k = k < 0.25f ? 0.25f : k;
    } else {
        const char *e = getenv("TGR_SCALE");
        k = e ? (float)atof(e) : 1.0f;
        k = k < 0.25f ? 0.25f : k > 16 ? 16 : k;
    }
    *tw = (int)(fb_w * k + 0.5f);
    *th = (int)(fb_h * k + 0.5f);
    if (dw > 0 && abs(*tw - dw) <= 2)
        *tw = dw;
    if (dh > 0 && abs(*th - dh) <= 2)
        *th = dh;
}

/* ---- frames -------------------------------------------------------------------------------- */
static void clear_depth(void)
{
    glDisable(GL_SCISSOR_TEST);
    glDepthMask(GL_TRUE);
    glClearDepth(1.0);
    glClear(GL_DEPTH_BUFFER_BIT);
}

void rdr_frame_begin(float fb_w, float fb_h)
{
    int tw, th;
    if (!s_ok)
        return;
    s_frames++;
    s_fb_w = fb_w;
    s_fb_h = fb_h;
    target_size(fb_w, fb_h, &tw, &th);
    if (!s_col || tw != s_tw || th != s_th)
        if (!targets_make(tw, th))
            return;
    s_sx = (float)s_tw / fb_w;
    s_sy = (float)s_th / fb_h;
    s_scale = s_sy;
    /* the rings start over, their old contents orphaned to the frames still reading them */
    glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
    glBufferData(GL_ARRAY_BUFFER, RING_SIZE, NULL, GL_STREAM_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, s_ubo);
    glBufferData(GL_UNIFORM_BUFFER, RING_SIZE, NULL, GL_STREAM_DRAW);
    s_vbo_used = s_ubo_used = 0;
    glBindFramebuffer(GL_FRAMEBUFFER, s_ms_fbo);
    glViewport(0, 0, s_tw, s_th);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    if (s_first) {                                    /* else the frame persists, as the N64's framebuffer does */
        glDisable(GL_SCISSOR_TEST);
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        s_first = 0;
    }
    clear_depth();
    s_in_frame = 1;
}

static void uniforms(const RdrState *st, U *u)
{
    int c, k;
    memset(u, 0, sizeof *u);
    for (c = 0; c < 2; c++)
        for (k = 0; k < 4; k++) {
            u->cc[c * 2][k] = st->cc.rgb[c][k];
            u->cc[c * 2 + 1][k] = st->cc.a[c][k];
        }
    memcpy(u->prim, st->prim, sizeof u->prim);
    memcpy(u->env, st->env, sizeof u->env);
    memcpy(u->fog, st->fog, sizeof u->fog);
    memcpy(u->blendc, st->blend, sizeof u->blendc);
    u->plod = st->prim_lod_frac;
    u->scale = s_scale;
    u->cycle = st->cycle;
    u->filter = st->filter;
    u->fog_blend = st->fog_blend;
    u->alpha_cmp = st->alpha_compare;
    u->balpha = st->blend_alpha;
    u->fb[0] = s_fb_w;
    u->fb[1] = s_fb_h;
    u->seed = (int)s_frames;
    u->lodn = st->lod_levels;
    for (k = 0; k < NTEX; k++) {
        const RdrTile *t = k < 2 ? &st->tile[k] : &st->lod[k - 2];
        if (k >= 2 && k - 2 >= st->lod_levels)
            break;
        if (!t->tex)
            continue;
        if (k < 2)
            u->ntex |= 1 << k;
        u->tile[k].org[0] = t->s0;
        u->tile[k].org[1] = t->t0;
        u->tile[k].org[2] = t->sscale;
        u->tile[k].org[3] = t->tscale;
        u->tile[k].size[0] = t->w;
        u->tile[k].size[1] = t->h;
        u->tile[k].size[2] = t->mask_s;
        u->tile[k].size[3] = t->mask_t;
        u->tile[k].wrap[0] = t->clamp_s;
        u->tile[k].wrap[1] = t->clamp_t;
        u->tile[k].wrap[2] = t->mirror_s;
        u->tile[k].wrap[3] = t->mirror_t;
        u->tile[k].cl[0] = t->clamp_w;
        u->tile[k].cl[1] = t->clamp_h;
    }
}

/* the draw's ten textures on units 0..9 (an unused slot reads the empty one) */
static void tex_bind(const RdrState *st)
{
    int k, h;
    for (k = 0; k < NTEX; k++) {
        h = k < 2 ? st->tile[k].tex : (k - 2 < st->lod_levels ? st->lod[k - 2].tex : 0);
        if (h <= 0 || h >= s_ntex || !s_tex[h])
            h = 0;
        if (s_bound[k] != s_tex[h]) {
            glActiveTexture(GL_TEXTURE0 + k);
            glBindTexture(GL_TEXTURE_2D, s_tex[h]);
            s_bound[k] = s_tex[h];
        }
    }
    glActiveTexture(GL_TEXTURE0);
}

/* n bytes into a ring at an aligned offset; a full ring is orphaned and starts over */
static long ring_put(GLenum kind, GLuint buf, long *used, long align, const void *src, long n)
{
    long off = (*used + align - 1) / align * align;
    void *m;
    glBindBuffer(kind, buf);
    if (off + n > RING_SIZE) {
        glBufferData(kind, RING_SIZE, NULL, GL_STREAM_DRAW);
        off = 0;
    }
    m = glMapBufferRange(kind, off, n, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT | GL_MAP_UNSYNCHRONIZED_BIT);
    if (m) {
        memcpy(m, src, (size_t)n);
        glUnmapBuffer(kind);
    } else
        glBufferSubData(kind, off, n, src);
    *used = off + n;
    return off;
}

static void draw(const RdrState *st, const RdrVtx *v, int n, int depth)
{
    U u;
    long uo, vo;
    int x0, y0, x1, y1, b;
    if (!s_in_frame || n <= 0 || n * (long)sizeof *v > RING_SIZE)
        return;
    x0 = (int)(st->scissor[0] * s_sx + 0.5f);
    y0 = (int)(st->scissor[1] * s_sy + 0.5f);
    x1 = (int)(st->scissor[2] * s_sx + 0.5f);
    y1 = (int)(st->scissor[3] * s_sy + 0.5f);
    x0 = x0 < 0 ? 0 : x0;
    y0 = y0 < 0 ? 0 : y0;
    x1 = x1 > s_tw ? s_tw : x1;
    y1 = y1 > s_th ? s_th : y1;
    if (x1 <= x0 || y1 <= y0)
        return;
    b = st->aa && st->cvg_x_alpha && !st->force_bl ? 4 : st->blend_mode & 3;
    uniforms(st, &u);
    uo = ring_put(GL_UNIFORM_BUFFER, s_ubo, &s_ubo_used, s_ualign, &u, sizeof u);
    vo = ring_put(GL_ARRAY_BUFFER, s_vbo, &s_vbo_used, sizeof *v, v, n * (long)sizeof *v);
    glUseProgram(s_prog);
    glBindVertexArray(s_vao);
    glBindBufferRange(GL_UNIFORM_BUFFER, 0, s_ubo, uo, sizeof u);
    tex_bind(st);
    glEnable(GL_SCISSOR_TEST);
    glScissor(x0, y0, x1 - x0, y1 - y0);              /* rows top first: no flip */
    /* depth: the RDP's test (decal: equal passes), its write; rects test nothing */
    if (depth) {
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(!st->z_test ? GL_ALWAYS : st->z_decal ? GL_LEQUAL : GL_LESS);
        glDepthMask(st->z_write ? GL_TRUE : GL_FALSE);
    } else
        glDisable(GL_DEPTH_TEST);
    if (b == RDR_BLEND_MEM)
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    else
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    if (b == RDR_BLEND_ALPHA || b == RDR_BLEND_ADD) {
        glEnable(GL_BLEND);
        glBlendEquation(GL_FUNC_ADD);
        glBlendFuncSeparate(GL_SRC_ALPHA, b == RDR_BLEND_ADD ? GL_ONE : GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE);
    } else
        glDisable(GL_BLEND);
    if (b == 4)                                       /* texture edges: alpha as coverage */
        glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    else
        glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    glDrawArrays(GL_TRIANGLES, (GLint)(vo / (long)sizeof *v), n);
}

void rdr_triangles(const RdrState *st, const RdrVtx *v, int n)
{
    draw(st, v, n, 1);
}

void rdr_rect(const RdrState *st, float x0, float y0, float x1, float y1, float s, float t, float dsdx,
              float dtdy, int fill, const float rgba[4])
{
    RdrVtx q[6];
    RdrState f;
    float xs[2] = { x0, x1 }, ys[2] = { y0, y1 };
    static const int order[6][2] = { { 0, 0 }, { 1, 0 }, { 0, 1 }, { 1, 0 }, { 1, 1 }, { 0, 1 } };
    int i;
    if (!s_in_frame)
        return;
    f = *st;
    if (fill == 1) {                                  /* a fill colour: the combiner passes it */
        memset(&f.cc, 0, sizeof f.cc);
        for (i = 0; i < 3; i++)
            f.cc.rgb[0][i] = f.cc.a[0][i] = RDR_CC_ZERO;
        f.cc.rgb[0][3] = f.cc.a[0][3] = RDR_CC_PRIM;
        memcpy(f.prim, rgba, sizeof f.prim);
        f.cycle = 1;
        f.tile[0].tex = f.tile[1].tex = 0;
        f.lod_levels = 0;
        f.blend_mode = RDR_BLEND_OPAQUE;
        f.alpha_compare = 0;
        f.fog_blend = 0;
    }
    for (i = 0; i < 6; i++) {
        RdrVtx *p = &q[i];
        memset(p, 0, sizeof *p);
        p->x = xs[order[i][0]];
        p->y = ys[order[i][1]];
        p->z = -1;
        p->w = 1;
        p->s = s + (p->x - x0) * dsdx;
        p->t = t + (p->y - y0) * dtdy;
    }
    draw(&f, q, 6, 0);
}

void rdr_clear_depth(void)
{
    if (s_in_frame)
        clear_depth();
}

void rdr_frame_end(void)
{
    int ww, wh;
    if (!s_in_frame)
        return;
    s_in_frame = 0;
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, s_ms_fbo);   /* the samples resolved into the frame */
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, s_fbo);
    glBlitFramebuffer(0, 0, s_tw, s_th, 0, 0, s_tw, s_th, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    if (!s_window || s_want) {                       /* the frame kept for rdr_frame_pixels */
        host_mutex_lock(s_pix_mu);
        if (s_tw * s_th > s_last_cap) {
            s_last_cap = s_tw * s_th;
            s_last = (uint32_t *)realloc(s_last, (size_t)s_last_cap * 4);
        }
        glBindFramebuffer(GL_READ_FRAMEBUFFER, s_fbo);
        glPixelStorei(GL_PACK_ALIGNMENT, 4);
        glReadPixels(0, 0, s_tw, s_th, GL_BGRA, GL_UNSIGNED_BYTE, s_last);   /* BGRA = 0xAARRGGBB, rows top first */
        s_last_w = s_tw;
        s_last_h = s_th;
        host_mutex_unlock(s_pix_mu);
    }
    window_size(&ww, &wh);
    if (ww > 0 && wh > 0) {
        float sx = 1, sy = 1;
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, ww, wh);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        if ((long)ww * s_th > (long)wh * s_tw)       /* letterbox to the frame's shape */
            sx = (float)((double)wh * s_tw / ((double)ww * s_th));
        else
            sy = (float)((double)ww * s_th / ((double)wh * s_tw));
        glUseProgram(s_blit);
        glUniform4f(s_loc_r, -sx, -sy, 2 * sx, 2 * sy);
        glUniform1i(s_loc_gamma, s_gamma);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, s_col);
        s_bound[0] = s_col;
        glBindSampler(0, s_lin_sampler);
        glBindVertexArray(s_vao);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindSampler(0, 0);
        gl_ctx_swap();
    }
}

/* the last finished frame (the copy rdr_frame_end kept; any thread) */
const uint32_t *rdr_frame_pixels(int *w, int *h)
{
    *w = *h = 0;
    if (!s_ok)
        return NULL;
    s_want = 1;
    host_mutex_lock(s_pix_mu);
    if (s_last_w > 0) {
        if (s_last_w * s_last_h > s_pix_cap) {
            s_pix_cap = s_last_w * s_last_h;
            s_pixels = (uint32_t *)realloc(s_pixels, (size_t)s_pix_cap * 4);
        }
        memcpy(s_pixels, s_last, (size_t)s_last_w * (size_t)s_last_h * 4);
        *w = s_last_w;
        *h = s_last_h;
    }
    host_mutex_unlock(s_pix_mu);
    return *w ? s_pixels : NULL;
}
