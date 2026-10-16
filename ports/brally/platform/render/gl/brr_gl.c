/* brr_gl.c: the renderer backend on OpenGL 3.3 core, the fallback for a PC
 * whose GPU has no Vulkan driver (every GPU of the last decade has one of
 * Metal, Vulkan or OpenGL 3.3).
 *
 * The same design as the Vulkan backend (render/vulkan), whose shaders these
 * are in GLSL 3.30: the game's frame drawn into an offscreen colour and depth
 * target at the window's size, then into the window, aspect kept, sharp
 * bilinear.
 *
 *   combiners   the fragment shader evaluates grColorCombine, grAlphaCombine
 *               and TMU0's grTexCombine, the alpha test and table fog (the
 *               software renderer's equations)
 *   depth       written by the shader at the Voodoo's precision (the W-buffer
 *               word or the Z-buffer's 16 bits)
 *   map table   this frame's (glide.c) in a texture buffer the vertex stage
 *               reads by the draw's entry
 *   state       one uniform block a draw, from a ring buffer (orphaned each
 *               frame); vertices the same
 *
 * GL's framebuffer has its origin at the bottom: the target's rows run up,
 * scissors and LFB writes are turned over to match, and shots turned back.
 * The context is gl_ctx.c's: on Windows the host's window, offscreen under
 * BR_VCLOCK or without one. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "brr.h"
#include "brr_png.h"
#include "host.h"
#include "gl_api.h"

/* the shader's uniform block, std140 (the Vulkan renderer's U, glide_u.glsl) */
typedef struct U {
    int32_t cc_fn, cc_factor, cc_local, cc_other, cc_invert;
    int32_t ac_fn, ac_factor, ac_local, ac_other, ac_invert;
    int32_t tc_rgb_fn, tc_rgb_factor, tc_alpha_fn, tc_alpha_factor, tc_rgb_invert, tc_alpha_invert;
    int32_t has_tex, atest_fn, fog_mode, depth_mode;
    float atest_ref, vw, vh, xf;
    float konst[4], fog_color[4];
    float fog[64];
} U;

typedef struct V { float pos[2]; float z, oow; float col[4]; float st[2]; } V;

#define TEX_MAX   4096
#define RING      (8u << 20)

static int      s_w, s_h;                 /* the game's frame, Glide's coordinates */
static int      s_rw, s_rh;               /* the target: the window's pixels */
static int      s_offscreen;              /* BR_VCLOCK: never wait for the display */
static int      s_open;
static GLuint   s_prog, s_sharp, s_vao, s_vbo, s_ubo, s_xtbuf, s_xttex;
static GLuint   s_fbo, s_col, s_dep;
static GLuint   s_smp[2][2][2];           /* linear, clamp s, clamp t */
static GLuint   s_tex[TEX_MAX];
static GLuint   s_white;
static uint32_t s_next_id = 1;
static long     s_vused, s_uused;         /* this frame's use of the rings, bytes */
static GLint    s_ualign = 256;
static GLint    s_loc_tex, s_loc_xt, s_loc_sharp;
static unsigned long s_frame;
static long     s_shot_frame = -1;
static char     s_shot_path[1024];
static uint8_t *s_read;

/* ---- the shaders (render/vulkan/shaders, in GLSL 3.30) --------------------------------- */
#define U_GLSL \
"layout(std140) uniform U {\n" \
"    int cc_fn, cc_factor, cc_local, cc_other, cc_invert;\n" \
"    int ac_fn, ac_factor, ac_local, ac_other, ac_invert;\n" \
"    int tc_rgb_fn, tc_rgb_factor, tc_alpha_fn, tc_alpha_factor, tc_rgb_invert, tc_alpha_invert;\n" \
"    int has_tex, atest_fn, fog_mode, depth_mode;\n" \
"    float atest_ref, vw, vh, xf;\n" \
"    vec4 konst, fog_color;\n" \
"    vec4 fog[16];\n" \
"} u;\n"

static const char *const k_vs =
"#version 330 core\n"
U_GLSL
"layout(location = 0) in vec2 a_pos;\n"
"layout(location = 1) in float a_z;\n"
"layout(location = 2) in float a_oow;\n"
"layout(location = 3) in vec4 a_col;\n"
"layout(location = 4) in vec2 a_st;\n"
"noperspective out vec4 v_col;\n"
"out vec2 v_st;\n"
"noperspective out float v_oow;\n"
"noperspective out float v_z;\n"
/* the screen map (glide.c): the game's pixels to the target through entry u.xf of this frame's table */
"uniform samplerBuffer u_xt;\n"
"void main() {\n"
"    float w = a_oow > 0.0 ? 1.0 / a_oow : 1.0;\n"
"    vec4 T = texelFetch(u_xt, int(u.xf));\n"
"    vec2 ndc = vec2(a_pos.x * T.x + T.y, a_pos.y * T.z + T.w);\n"
"    gl_Position = vec4(ndc * w, clamp(a_z, 0.0, 1.0) * w, w);\n"
"    v_col = clamp(a_col / 255.0, 0.0, 1.0);\n"
"    v_st = a_st;\n"
"    v_oow = a_oow;\n"
"    v_z = a_z;\n"
"}\n";

static const char *const k_fs =
"#version 330 core\n"
U_GLSL
"uniform sampler2D t_tex;\n"
"noperspective in vec4 v_col;\n"
"in vec2 v_st;\n"
"noperspective in float v_oow;\n"
"noperspective in float v_z;\n"
"layout(location = 0) out vec4 o_col;\n"
"float comb(int fn, float f, float l, float la, float ot, int inv) {\n"
"    float r;\n"
"    if (fn == 0x0) r = 0.0; else if (fn == 0x1) r = l; else if (fn == 0x2) r = la; else if (fn == 0x3) r = f * ot;\n"
"    else if (fn == 0x4) r = f * ot + l; else if (fn == 0x5) r = f * ot + la; else if (fn == 0x6) r = f * (ot - l);\n"
"    else if (fn == 0x7) r = f * (ot - l) + l; else if (fn == 0x8) r = f * (ot - l) + la; else if (fn == 0x9) r = -f * l + l;\n"
"    else if (fn == 0x10) r = -f * l + la; else r = l;\n"
"    r = clamp(r, 0.0, 1.0);\n"
"    return inv != 0 ? 1.0 - r : r;\n"
"}\n"
"float fac(int k, float l, float la, float oa, float ta) {\n"
"    if (k == 0) return 0.0; if (k == 1) return l; if (k == 2) return oa; if (k == 3) return la; if (k == 4) return ta;\n"
"    if (k == 5) return 0.0; if (k == 8) return 1.0; if (k == 9) return 1.0 - l; if (k == 10) return 1.0 - oa;\n"
"    if (k == 11) return 1.0 - la; if (k == 12) return 1.0 - ta; if (k == 13) return 1.0; return 0.0;\n"
"}\n"
"bool cmpf(int fn, float a, float b) {\n"
"    if (fn == 0) return false; if (fn == 1) return a < b; if (fn == 2) return a == b; if (fn == 3) return a <= b;\n"
"    if (fn == 4) return a > b; if (fn == 5) return a != b; if (fn == 6) return a >= b; return true;\n"
"}\n"
"float fogv(int i) { return u.fog[i >> 2][i & 3]; }\n"
"float fogw(int i) { return pow(2.0, 3.0 + float(i >> 2)) / float(8 - (i & 3)); }\n"
"float fogt(float w) {\n"
"    if (w <= fogw(0)) return fogv(0);\n"
"    for (int i = 0; i < 63; i++) {\n"
"        float w0 = fogw(i), w1 = fogw(i + 1);\n"
"        if (w < w1) { float k = (w - w0) / (w1 - w0); return fogv(i) * (1.0 - k) + fogv(i + 1) * k; }\n"
"    }\n"
"    return fogv(63);\n"
"}\n"
/* the highest set bit (GLSL 3.30 has no findMSB) */
"int msb(uint t) { int r = 0; if (t >= 0x10000u) { t >>= 16; r += 16; } if (t >= 0x100u) { t >>= 8; r += 8; }\n"
"    if (t >= 0x10u) { t >>= 4; r += 4; } if (t >= 4u) { t >>= 2; r += 2; } if (t >= 2u) r += 1; return r; }\n"
/* the Voodoo's 16-bit W-buffer word (the Vulkan renderer's wfloat) */
"uint wfloat(float oow) {\n"
"    if (oow >= 1.0) return 0u;\n"
"    if (oow <= 0.0) return 0xFFFFu;\n"
"    uint t = uint(min(oow * 4294967296.0, 4294967040.0));\n"
"    if (t == 0u) return 0xFFFFu;\n"
"    int e = 31 - msb(t);\n"
"    uint m = e <= 19 ? (~t >> uint(19 - e)) : (~t << uint(e - 19));\n"
"    uint w = (uint(e) << 12) | (m & 0xFFFu);\n"
"    return w < 0xFFFFu ? w + 1u : w;\n"
"}\n"
"void main() {\n"
"    vec4 tex = vec4(0.0);\n"
"    if (u.has_tex != 0) {\n"
"        vec4 tx = texture(t_tex, v_st);\n"
"        tex.r = comb(u.tc_rgb_fn, fac(u.tc_rgb_factor, tx.r, tx.a, 0.0, tx.a), tx.r, tx.a, 0.0, u.tc_rgb_invert);\n"
"        tex.g = comb(u.tc_rgb_fn, fac(u.tc_rgb_factor, tx.g, tx.a, 0.0, tx.a), tx.g, tx.a, 0.0, u.tc_rgb_invert);\n"
"        tex.b = comb(u.tc_rgb_fn, fac(u.tc_rgb_factor, tx.b, tx.a, 0.0, tx.a), tx.b, tx.a, 0.0, u.tc_rgb_invert);\n"
"        tex.a = comb(u.tc_alpha_fn, fac(u.tc_alpha_factor, tx.a, tx.a, 0.0, tx.a), tx.a, tx.a, 0.0, u.tc_alpha_invert);\n"
"    }\n"
"    vec4 it = v_col, k = u.konst;\n"
"    vec4 cl = u.cc_local == 1 ? k : it;\n"
"    vec4 co = u.cc_other == 1 ? tex : (u.cc_other == 2 ? k : it);\n"
"    float al = u.ac_local == 1 ? k.a : it.a;\n"
"    float ao = u.ac_other == 1 ? tex.a : (u.ac_other == 2 ? k.a : it.a);\n"
"    vec4 o;\n"
"    o.a = comb(u.ac_fn, fac(u.ac_factor, al, al, ao, tex.a), al, al, ao, u.ac_invert);\n"
"    o.r = comb(u.cc_fn, fac(u.cc_factor, cl.r, al, co.a, tex.a), cl.r, al, co.r, u.cc_invert);\n"
"    o.g = comb(u.cc_fn, fac(u.cc_factor, cl.g, al, co.a, tex.a), cl.g, al, co.g, u.cc_invert);\n"
"    o.b = comb(u.cc_fn, fac(u.cc_factor, cl.b, al, co.a, tex.a), cl.b, al, co.b, u.cc_invert);\n"
"    if (u.atest_fn != 7 && !cmpf(u.atest_fn, floor(o.a * 255.0 + 0.5), u.atest_ref)) discard;\n"
"    float f = 0.0;\n"
"    if (u.fog_mode == 1) f = it.a;\n"
"    else if (u.fog_mode == 2) f = fogt(v_oow > 0.0 ? 1.0 / v_oow : 65536.0);\n"
"    else if (u.fog_mode == 3) f = clamp(v_z, 0.0, 1.0);\n"
"    if (f > 0.0) o.rgb += (u.fog_color.rgb - o.rgb) * f;\n"
"    o_col = o;\n"
"    gl_FragDepth = (u.depth_mode == 2 || u.depth_mode == 4) ? float(wfloat(v_oow)) / 65536.0\n"
"                                     : floor(clamp(v_z * 65535.0, 0.0, 65535.0)) / 65536.0;\n"
"}\n";

/* the frame into the window, sharp bilinear: whole texels at any scale, blended only
   across the one window pixel a texel edge falls in (shaders/sharp.*) */
static const char *const k_sharp_vs =
"#version 330 core\n"
"out vec2 v_uv;\n"
"void main() { vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2); v_uv = p; gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0); }\n";
static const char *const k_sharp_fs =
"#version 330 core\n"
"uniform sampler2D t_tex;\n"
"in vec2 v_uv;\n"
"layout(location = 0) out vec4 o_col;\n"
"void main() {\n"
"    vec2 ts = vec2(textureSize(t_tex, 0));\n"
"    vec2 px = v_uv * ts - 0.5;\n"
"    vec2 i = floor(px), f = px - i, d = max(fwidth(px), vec2(1e-5));\n"
"    f = clamp((f - 0.5) / d + 0.5, 0.0, 1.0);\n"
"    o_col = vec4(texture(t_tex, (i + 0.5 + f) / ts).rgb, 1.0);\n"
"}\n";

static GLuint shader(GLenum kind, const char *src)
{
    GLuint s = glCreateShader(kind);
    GLint ok = 0;
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[4096];
        glGetShaderInfoLog(s, sizeof log, NULL, log);
        fprintf(stderr, "brr: gl shader: %s\n", log);
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
    glLinkProgram(p);
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[4096];
        glGetProgramInfoLog(p, sizeof log, NULL, log);
        fprintf(stderr, "brr: gl link: %s\n", log);
        return 0;
    }
    return p;
}

/* ---- targets ----------------------------------------------------------------------- */
static int targets(void)
{
    if (!s_fbo)
        glGenFramebuffers(1, &s_fbo);
    if (s_col)
        glDeleteTextures(1, &s_col);
    if (s_dep)
        glDeleteTextures(1, &s_dep);
    glGenTextures(1, &s_col);
    glBindTexture(GL_TEXTURE_2D, s_col);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, s_rw, s_rh, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenTextures(1, &s_dep);
    glBindTexture(GL_TEXTURE_2D, s_dep);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, s_rw, s_rh, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, s_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, s_col, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, s_dep, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "brr: gl target %dx%d incomplete\n", s_rw, s_rh);
        return 0;
    }
    glViewport(0, 0, s_rw, s_rh);
    glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glClearColor(0, 0, 0, 1);
    glClearDepth(1.0);
    glDepthMask(GL_TRUE);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    free(s_read);
    s_read = (uint8_t *)malloc((size_t)s_rw * (size_t)s_rh * 4);
    return 1;
}

/* the window's size in pixels: the target's (offscreen: the game's) */
static void window_size(int *w, int *h)
{
    *w = *h = 0;
    if (!s_offscreen && host_window_visible())
        host_window_pixels(w, h);
    if (*w < 1 || *h < 1) {
        *w = s_rw;
        *h = s_rh;
    }
}

/* ---- start-up ------------------------------------------------------------------------ */
int brr_open(int width, int height)
{
    const char *e;
    int f, cs, ct;
    static const uint8_t white[4] = { 255, 255, 255, 255 };
    s_w = s_rw = width;
    s_h = s_rh = height;
    s_offscreen = getenv("BR_VCLOCK") != NULL;
    if (!gl_ctx_open(s_offscreen ? NULL : host_window_handle()) || !gl_api_load())
        return 0;
    fprintf(stderr, "brr: gl renderer %dx%d on %s (%s)\n", s_w, s_h, (const char *)glGetString(GL_RENDERER),
            (const char *)glGetString(GL_VERSION));
    if (!(s_prog = program(k_vs, k_fs)) || !(s_sharp = program(k_sharp_vs, k_sharp_fs)))
        return 0;
    glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &s_ualign);
    if (s_ualign < 16)
        s_ualign = 16;
    glUniformBlockBinding(s_prog, glGetUniformBlockIndex(s_prog, "U"), 0);
    s_loc_tex = glGetUniformLocation(s_prog, "t_tex");
    s_loc_xt = glGetUniformLocation(s_prog, "u_xt");
    s_loc_sharp = glGetUniformLocation(s_sharp, "t_tex");
    glUseProgram(s_prog);
    glUniform1i(s_loc_tex, 0);
    glUniform1i(s_loc_xt, 1);
    glUseProgram(s_sharp);
    glUniform1i(s_loc_sharp, 0);
    glGenVertexArrays(1, &s_vao);
    glBindVertexArray(s_vao);
    glGenBuffers(1, &s_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
    glBufferData(GL_ARRAY_BUFFER, RING, NULL, GL_STREAM_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(V), (const void *)0);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof(V), (const void *)8);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(V), (const void *)12);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(V), (const void *)16);
    glVertexAttribPointer(4, 2, GL_FLOAT, GL_FALSE, sizeof(V), (const void *)32);
    for (f = 0; f < 5; f++)
        glEnableVertexAttribArray((GLuint)f);
    glGenBuffers(1, &s_ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, s_ubo);
    glBufferData(GL_UNIFORM_BUFFER, RING, NULL, GL_STREAM_DRAW);
    glGenBuffers(1, &s_xtbuf);
    glBindBuffer(GL_TEXTURE_BUFFER, s_xtbuf);
    {   /* until a frame sets it: the identity (the game's pixels, y down, to y-up device coordinates) */
        static float t[BRR_XF_MAX][4];
        int k;
        for (k = 0; k < BRR_XF_MAX; k++) {
            t[k][0] = 2.0f / (float)s_w; t[k][1] = -1.0f;
            t[k][2] = -2.0f / (float)s_h; t[k][3] = 1.0f;
        }
        glBufferData(GL_TEXTURE_BUFFER, sizeof t, t, GL_DYNAMIC_DRAW);
    }
    glGenTextures(1, &s_xttex);
    glActiveTexture(GL_TEXTURE0 + 1);
    glBindTexture(GL_TEXTURE_BUFFER, s_xttex);
    glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, s_xtbuf);
    glActiveTexture(GL_TEXTURE0);
    for (f = 0; f < 2; f++)
        for (cs = 0; cs < 2; cs++)
            for (ct = 0; ct < 2; ct++) {
                glGenSamplers(1, &s_smp[f][cs][ct]);
                glSamplerParameteri(s_smp[f][cs][ct], GL_TEXTURE_MIN_FILTER, f ? GL_LINEAR : GL_NEAREST);
                glSamplerParameteri(s_smp[f][cs][ct], GL_TEXTURE_MAG_FILTER, f ? GL_LINEAR : GL_NEAREST);
                glSamplerParameteri(s_smp[f][cs][ct], GL_TEXTURE_WRAP_S, cs ? GL_CLAMP_TO_EDGE : GL_REPEAT);
                glSamplerParameteri(s_smp[f][cs][ct], GL_TEXTURE_WRAP_T, ct ? GL_CLAMP_TO_EDGE : GL_REPEAT);
            }
    glGenTextures(1, &s_white);
    glBindTexture(GL_TEXTURE_2D, s_white);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glDisable(GL_CULL_FACE);
    if (!targets())
        return 0;
    e = getenv("BR_SHOT");
    if (e && sscanf(e, "%ld:%1023s", &s_shot_frame, s_shot_path) != 2)
        s_shot_frame = -1;
    s_open = 1;
    return 1;
}

void brr_close(void)
{
    if (s_open)
        glFinish();
}

/* ---- textures ------------------------------------------------------------------------ */
uint32_t brr_texture(uint32_t tid, const uint8_t *rgba, int w, int h)
{
    if (tid == 0) {
        for (tid = s_next_id; tid < TEX_MAX && s_tex[tid]; tid++)
            ;
        if (tid >= TEX_MAX)
            for (tid = 1; tid < TEX_MAX && s_tex[tid]; tid++)
                ;
        if (tid >= TEX_MAX)
            return 0;
        s_next_id = tid + 1;
    }
    if (tid >= TEX_MAX || w <= 0 || h <= 0)
        return 0;
    if (!s_tex[tid])
        glGenTextures(1, &s_tex[tid]);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, s_tex[tid]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    return tid;
}

void brr_texture_free(uint32_t tid)
{
    if (tid && tid < TEX_MAX && s_tex[tid]) {
        glDeleteTextures(1, &s_tex[tid]);
        s_tex[tid] = 0;
        if (tid < s_next_id)
            s_next_id = tid;
    }
}

/* ---- drawing ------------------------------------------------------------------------- */
static GLenum bf(int k, int src)
{
    switch (k) {
    case 0:   return GL_ZERO;
    case 1:   return GL_SRC_ALPHA;
    case 2:   return src ? GL_DST_COLOR : GL_SRC_COLOR;
    case 3:   return GL_ONE;                    /* no destination alpha: it reads as one */
    case 4:   return GL_ONE;
    case 5:   return GL_ONE_MINUS_SRC_ALPHA;
    case 6:   return src ? GL_ONE_MINUS_DST_COLOR : GL_ONE_MINUS_SRC_COLOR;
    case 7:   return GL_ZERO;
    case 0xF: return GL_ZERO;                   /* min(src alpha, 1 - dst alpha), dst alpha one */
    default:  return GL_ONE;
    }
}

static const GLenum k_cmp[8] = { GL_NEVER, GL_LESS, GL_EQUAL, GL_LEQUAL, GL_GREATER, GL_NOTEQUAL, GL_GEQUAL, GL_ALWAYS };

/* a scissor in target pixels (y down), turned to GL's (y up); 0 when empty */
static int scissor(int x0, int y0, int x1, int y1)
{
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > s_rw) x1 = s_rw;
    if (y1 > s_rh) y1 = s_rh;
    if (x1 <= x0 || y1 <= y0)
        return 0;
    glEnable(GL_SCISSOR_TEST);
    glScissor(x0, s_rh - y1, x1 - x0, y1 - y0);
    return 1;
}

/* bytes into a ring at a range this frame has not used: mapped unsynchronised, so the driver
   never waits for (or flushes) the draws still reading the ranges before it (a plain
   glBufferSubData made the Mac's driver submit its queue at every draw) */
static void ring_write(GLenum kind, GLuint buf, long off, long n, const void *src)
{
    void *m;
    glBindBuffer(kind, buf);
    m = glMapBufferRange(kind, off, n, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT | GL_MAP_UNSYNCHRONIZED_BIT);
    if (m) {
        memcpy(m, src, (size_t)n);
        glUnmapBuffer(kind);
    } else
        glBufferSubData(kind, off, n, src);
}

static void target_bind(void)
{
    glBindFramebuffer(GL_FRAMEBUFFER, s_fbo);
    glViewport(0, 0, s_rw, s_rh);
}

void brr_clear(uint32_t argb, float depth, int colour, int depthbuf, const brr_state *clip)
{
    if (!colour && !depthbuf)
        return;
    target_bind();
    if (!scissor(clip->sx0, clip->sy0, clip->sx1, clip->sy1))
        return;
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_TRUE);
    glClearColor(((argb >> 16) & 0xFF) / 255.0f, ((argb >> 8) & 0xFF) / 255.0f, (argb & 0xFF) / 255.0f, 1.0f);
    glClearDepth(depth < 0 ? 0 : depth > 1 ? 1 : depth);
    glClear((colour ? GL_COLOR_BUFFER_BIT : 0) | (depthbuf ? GL_DEPTH_BUFFER_BIT : 0));
}

void brr_draw(const brr_state *st, const brr_vertex *v, int n)
{
    U u;
    V *o;
    long uo, vo;
    int i, src = st->blend_rgb_src & 15, dst = st->blend_rgb_dst & 15;
    GLuint t;
    if (n < 3)
        return;
    memset(&u, 0, sizeof u);
    u.cc_fn = st->cc_fn; u.cc_factor = st->cc_factor; u.cc_local = st->cc_local;
    u.cc_other = st->cc_other; u.cc_invert = st->cc_invert;
    u.ac_fn = st->ac_fn; u.ac_factor = st->ac_factor; u.ac_local = st->ac_local;
    u.ac_other = st->ac_other; u.ac_invert = st->ac_invert;
    u.tc_rgb_fn = st->tc_rgb_fn; u.tc_rgb_factor = st->tc_rgb_factor;
    u.tc_alpha_fn = st->tc_alpha_fn; u.tc_alpha_factor = st->tc_alpha_factor;
    u.tc_rgb_invert = st->tc_rgb_invert; u.tc_alpha_invert = st->tc_alpha_invert;
    u.has_tex = st->texture != 0;
    u.atest_fn = st->atest_fn;
    u.atest_ref = st->atest_ref;
    u.fog_mode = st->fog_mode & 0xFF;
    u.depth_mode = st->depth_mode;
    u.vw = (float)s_w;
    u.vh = (float)s_h;
    u.xf = (float)(st->xf >= 0 && st->xf < BRR_XF_MAX ? st->xf : 0);
    u.konst[0] = ((st->constant >> 16) & 0xFF) / 255.0f;
    u.konst[1] = ((st->constant >> 8) & 0xFF) / 255.0f;
    u.konst[2] = (st->constant & 0xFF) / 255.0f;
    u.konst[3] = (st->constant >> 24) / 255.0f;
    u.fog_color[0] = ((st->fog_color >> 16) & 0xFF) / 255.0f;
    u.fog_color[1] = ((st->fog_color >> 8) & 0xFF) / 255.0f;
    u.fog_color[2] = (st->fog_color & 0xFF) / 255.0f;
    for (i = 0; i < 64; i++)
        u.fog[i] = st->fog_table[i] / 255.0f;
    /* the rings: this frame's state and vertices (orphaned at the frame's start) */
    uo = (s_uused + s_ualign - 1) / s_ualign * s_ualign;
    vo = (s_vused + (long)sizeof(V) - 1) / (long)sizeof(V) * (long)sizeof(V);
    if (uo + (long)sizeof u > (long)RING || vo + (long)n * (long)sizeof(V) > (long)RING)
        return;
    s_uused = uo + (long)sizeof u;
    s_vused = vo + (long)n * (long)sizeof(V);
    o = (V *)malloc((size_t)n * sizeof *o);
    if (!o)
        return;
    for (i = 0; i < n; i++) {
        o[i].pos[0] = v[i].x;
        o[i].pos[1] = v[i].y;
        o[i].z = v[i].z;
        o[i].oow = v[i].oow;
        o[i].col[0] = v[i].r;
        o[i].col[1] = v[i].g;
        o[i].col[2] = v[i].b;
        o[i].col[3] = v[i].a;
        o[i].st[0] = v[i].s;
        o[i].st[1] = v[i].t;
    }
    target_bind();
    if (!scissor(st->sx0, st->sy0, st->sx1, st->sy1)) {
        free(o);
        return;
    }
    ring_write(GL_ARRAY_BUFFER, s_vbo, vo, (long)n * (long)sizeof(V), o);
    free(o);
    ring_write(GL_UNIFORM_BUFFER, s_ubo, uo, (long)sizeof u, &u);
    glBindBufferRange(GL_UNIFORM_BUFFER, 0, s_ubo, uo, sizeof u);
    glUseProgram(s_prog);
    glBindVertexArray(s_vao);
    t = st->texture && st->texture < TEX_MAX && s_tex[st->texture] ? s_tex[st->texture] : s_white;
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, t);
    glBindSampler(0, s_smp[(st->mag_filter || st->min_filter) ? 1 : 0][st->clamp_s ? 1 : 0][st->clamp_t ? 1 : 0]);
    if (st->depth_mode) {
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(k_cmp[st->depth_fn & 7]);
        glDepthMask(st->depth_mask ? GL_TRUE : GL_FALSE);
    } else {
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
    }
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);   /* the board keeps no destination alpha */
    if (!(src == 4 && dst == 0)) {
        glEnable(GL_BLEND);
        glBlendEquation(GL_FUNC_ADD);
        glBlendFuncSeparate(bf(src, 1), bf(dst, 0), GL_ZERO, GL_ONE);
    } else
        glDisable(GL_BLEND);
    glDrawArrays(GL_TRIANGLES, (GLint)(vo / (long)sizeof(V)), n);
}

/* ---- LFB writes -------------------------------------------------------------------------- */
/* the pixels as RGBA8, rows turned over (GL's rows run up) */
static uint8_t *lfb_rgba(int w, int h, const uint16_t *p, int stride)
{
    uint8_t *px = (uint8_t *)malloc((size_t)w * (size_t)h * 4);
    int i, j;
    if (!px)
        return NULL;
    for (j = 0; j < h; j++) {
        const uint16_t *r = (const uint16_t *)((const uint8_t *)p + (size_t)j * (size_t)stride);
        uint8_t *o = px + (size_t)(h - 1 - j) * (size_t)w * 4;
        for (i = 0; i < w; i++) {
            uint32_t c = r[i];
            o[i * 4 + 0] = (uint8_t)((c >> 11) * 255 / 31);
            o[i * 4 + 1] = (uint8_t)(((c >> 5) & 63) * 255 / 63);
            o[i * 4 + 2] = (uint8_t)((c & 31) * 255 / 31);
            o[i * 4 + 3] = 255;
        }
    }
    return px;
}

void brr_lfb_write(int x, int y, int w, int h, const uint16_t *p, int stride, const int d[4])
{
    uint8_t *px;
    if (w <= 0 || h <= 0 || x < 0 || y < 0 || x + w > s_w || y + h > s_h)
        return;
    if (!(px = lfb_rgba(w, h, p, stride)))
        return;
    if (d[0] == x && d[1] == y && d[2] == x + w && d[3] == y + h && d[3] <= s_rh) {   /* its own pixels: copied straight in */
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, s_col);
        glTexSubImage2D(GL_TEXTURE_2D, 0, x, s_rh - (y + h), w, h, GL_RGBA, GL_UNSIGNED_BYTE, px);
    } else {   /* placed anywhere else: the region as a texture, drawn over rectangle d, sharp bilinear */
        GLuint t;
        glActiveTexture(GL_TEXTURE0);
        glGenTextures(1, &t);
        glBindTexture(GL_TEXTURE_2D, t);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
        target_bind();
        glViewport(d[0], s_rh - d[3], d[2] - d[0], d[3] - d[1]);
        if (scissor(d[0], d[1], d[2], d[3])) {
            glDisable(GL_DEPTH_TEST);
            glDisable(GL_BLEND);
            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
            glUseProgram(s_sharp);
            glBindVertexArray(s_vao);
            glActiveTexture(GL_TEXTURE0);
            glBindSampler(0, s_smp[1][1][1]);
            glDrawArrays(GL_TRIANGLES, 0, 3);
        }
        glViewport(0, 0, s_rw, s_rh);
        glDeleteTextures(1, &t);
    }
    free(px);
}

/* ---- the frame ------------------------------------------------------------------------- */
void brr_target(int *w, int *h)
{
    *w = s_rw;
    *h = s_rh;
}

void brr_xf(const float (*t)[4], int n)
{
    if (!s_open)
        return;
    if (n > BRR_XF_MAX)
        n = BRR_XF_MAX;
    glBindBuffer(GL_TEXTURE_BUFFER, s_xtbuf);
    glBufferSubData(GL_TEXTURE_BUFFER, 0, (GLsizeiptr)n * 16, t);
}

/* the target's pixels, top row first (shots) */
static void read_target(void)
{
    size_t row = (size_t)s_rw * 4;
    int j;
    uint8_t *tmp = (uint8_t *)malloc(row);
    glBindFramebuffer(GL_FRAMEBUFFER, s_fbo);
    glReadPixels(0, 0, s_rw, s_rh, GL_RGBA, GL_UNSIGNED_BYTE, s_read);
    for (j = 0; j < s_rh / 2 && tmp; j++) {
        memcpy(tmp, s_read + (size_t)j * row, row);
        memcpy(s_read + (size_t)j * row, s_read + (size_t)(s_rh - 1 - j) * row, row);
        memcpy(s_read + (size_t)(s_rh - 1 - j) * row, tmp, row);
    }
    free(tmp);
}

void brr_present(void)
{
    int ww, wh;
    window_size(&ww, &wh);
    if (!s_offscreen && host_window_visible()) {   /* into the window, aspect kept, sharp bilinear */
        double k = fmin((double)ww / s_rw, (double)wh / s_rh);
        int dw = (int)lround(s_rw * k), dh = (int)lround(s_rh * k);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDisable(GL_SCISSOR_TEST);
        glViewport(0, 0, ww, wh);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        glViewport((ww - dw) / 2, (wh - dh) / 2, dw, dh);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
        glUseProgram(s_sharp);
        glBindVertexArray(s_vao);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, s_col);
        glBindSampler(0, s_smp[1][1][1]);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        gl_ctx_swap();
    }
    if ((long)(s_frame + 1) == s_shot_frame) {
        read_target();
        if (brr_png_write(s_shot_path, s_read, s_rw, s_rh, s_rw * 4, BRR_PNG_RGBA))
            fprintf(stderr, "brr: frame %ld -> %s\n", s_shot_frame, s_shot_path);
    }
    s_frame++;
    /* the rings start again (orphaned: the driver keeps the last frame's while it draws) */
    glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
    glBufferData(GL_ARRAY_BUFFER, RING, NULL, GL_STREAM_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, s_ubo);
    glBufferData(GL_UNIFORM_BUFFER, RING, NULL, GL_STREAM_DRAW);
    s_vused = s_uused = 0;
    if (!s_offscreen && host_window_visible() && (ww != s_rw || wh != s_rh)) {   /* the next frame at the window's size */
        s_rw = ww;
        s_rh = wh;
        targets();
    }
}

int brr_shot(const char *path)
{
    if (!s_open)
        return 0;
    read_target();
    return brr_png_write(path, s_read, s_rw, s_rh, s_rw * 4, BRR_PNG_RGBA);
}
