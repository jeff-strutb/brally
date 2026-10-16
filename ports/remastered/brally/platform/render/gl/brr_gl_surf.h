/* brr_gl_surf.h: the Remastered textures' GPU side on OpenGL (BR_FLAG_TEXTURES;
 * light_surf.h), included by brr_gl.c after its texture helpers, whose statics
 * it uses. The Vulkan renderer's (render/vulkan/brr_vulkan_surf.h, shading
 * shaders/surf.glsl) in GLSL 3.30 and GL's objects:
 *
 *   units 2..6    a surface map's regions (RGBA8) twice, nearest and linear,
 *                 their variation (RG8), linear, and the sets' two arrays,
 *                 trilinear with 16x anisotropy where the driver has it: a
 *                 map's two textures made the first time a draw needs them,
 *                 stand-ins for every other draw
 *   the sets      read in the background (light_surf_sets_load, 2048); once
 *                 read, every level of every set (light_surf_set_level, the
 *                 CPU's mips) copied into two arrays, then the CPU's texels
 *                 freed */

#define SURF_ENTRIES 4096
#define SURF_UNIT    2                     /* the first of the five units */

/* surf.glsl, line for line (the sets through combined samplers, one unit each) */
#define SURF_GLSL \
"uniform sampler2D s_mapp;\n"       /* the regions, nearest */ \
"uniform sampler2D s_mapl;\n"       /* the regions, linear */ \
"uniform sampler2D s_varl;\n"       /* their own light and dark, linear */ \
"uniform sampler2DArray s_seta;\n"  /* the sets' albedo detail and height */ \
"uniform sampler2DArray s_setb;\n"  /* their normals' x and y */ \
"uint surfhash(int x, int y) {\n" \
"    uint h = uint(x) * 0x8da6b343u ^ uint(y) * 0xd8163841u;\n" \
"    h ^= h >> 13; h *= 0x2c1b3c6du; h ^= h >> 12; h *= 0x297a2d39u; h ^= h >> 15;\n" \
"    return h;\n" \
"}\n" \
"vec2 surfh2(ivec2 c) { uint h = surfhash(c.x, c.y); return vec2(float(h & 0xFFFFu), float(h >> 16)) / 65536.0; }\n" \
"float vhash(ivec2 c) { return float(surfhash(c.x, c.y) & 0xFFFFFFu) / 16777216.0; }\n" \
"float vnoise(vec2 q) {\n" \
"    vec2 fl = floor(q), f = q - fl; ivec2 i = ivec2(fl); f = f * f * (3.0 - 2.0 * f);\n" \
"    return mix(mix(vhash(i), vhash(i + ivec2(1, 0)), f.x), mix(vhash(i + ivec2(0, 1)), vhash(i + ivec2(1, 1)), f.x), f.y);\n" \
"}\n" \
"float vfbm(vec2 q) { return 0.5 * vnoise(q) + 0.3 * vnoise(q * 2.03 + 5.1) + 0.2 * vnoise(q * 4.11 + 9.7); }\n" \
"void trigrid(vec2 uv, out vec3 w, out ivec2 v1, out ivec2 v2, out ivec2 v3) {\n" \
"    vec2 sk = vec2(uv.x - 0.57735027 * uv.y, 1.15470054 * uv.y) * 2.0;\n" \
"    ivec2 b = ivec2(floor(sk)); vec2 f = sk - floor(sk); float z = 1.0 - f.x - f.y;\n" \
"    if (z > 0.0) { w = vec3(z, f.y, f.x); v1 = b; v2 = b + ivec2(0, 1); v3 = b + ivec2(1, 0); }\n" \
"    else { w = vec3(-z, 1.0 - f.y, 1.0 - f.x); v1 = b + ivec2(1, 1); v2 = b + ivec2(1, 0); v3 = b + ivec2(0, 1); }\n" \
"}\n" \
"struct SurfS { vec3 alb; float h; vec2 n; };\n" \
"SurfS hexsample(int slice, vec2 uv, vec2 dx, vec2 dy, float rot) {\n" \
"    vec3 w; ivec2 v[3]; trigrid(uv, w, v[0], v[1], v[2]);\n" \
"    w = pow(w, vec3(8.0)); w /= (w.x + w.y + w.z);\n" \
"    SurfS o; o.alb = vec3(0.0); o.h = 0.0; o.n = vec2(0.0);\n" \
"    for (int i = 0; i < 3; i++) {\n" \
"        if (w[i] < 0.02) continue;\n" \
"        vec2 r = surfh2(v[i]); float a = rot * r.x * 6.2831853; float c = cos(a), s = sin(a);\n" \
"        mat2 R = mat2(vec2(c, s), vec2(-s, c));\n" \
"        vec2 q = R * uv + r * 7.31;\n" \
"        vec4 A = textureGrad(s_seta, vec3(q, float(slice)), R * dx, R * dy);\n" \
"        vec2 B = textureGrad(s_setb, vec3(q, float(slice)), R * dx, R * dy).xy;\n" \
"        vec2 nt = vec2(B.x * 2.0 - 1.0, 1.0 - B.y * 2.0); nt = transpose(R) * nt;\n" \
"        o.alb += w[i] * A.rgb * 2.0; o.h += w[i] * A.a; o.n += w[i] * nt;\n" \
"    }\n" \
"    return o;\n" \
"}\n" \
"struct SurfR { vec3 alb; float h; vec3 dn; };\n" \
"SurfR surfone(int slice, float k, vec4 sa, vec4 sc, vec3 p, vec3 n, vec3 dp0, vec3 dp1) {\n" \
"    SurfR o;\n" \
"    vec3 an = abs(n); vec3 bw = an * an * an * an; bw /= (bw.x + bw.y + bw.z);\n" \
"    vec3 alb = vec3(0.0); float h = 0.0; vec3 dn = vec3(0.0); float tw = 0.0;\n" \
"    vec3 dpx = dp0 * k, dpy = dp1 * k;\n" \
"    if (bw.z > 0.02) {\n" \
"        float sg = n.z >= 0.0 ? 1.0 : -1.0;\n" \
"        SurfS s = hexsample(slice, vec2(p.x * k, p.y * sg * k), vec2(dpx.x, dpx.y * sg), vec2(dpy.x, dpy.y * sg), sc.x);\n" \
"        alb += bw.z * s.alb; h += bw.z * s.h; dn += bw.z * vec3(s.n.x, sg * s.n.y, 0.0); tw += bw.z;\n" \
"    }\n" \
"    if (bw.x > 0.02) {\n" \
"        float sg = n.x >= 0.0 ? 1.0 : -1.0;\n" \
"        SurfS s = hexsample(slice, vec2(p.y * sg * k, p.z * k), vec2(dpx.y * sg, dpx.z), vec2(dpy.y * sg, dpy.z), sc.x);\n" \
"        alb += bw.x * s.alb; h += bw.x * s.h; dn += bw.x * vec3(0.0, sg * s.n.x, s.n.y); tw += bw.x;\n" \
"    }\n" \
"    if (bw.y > 0.02) {\n" \
"        float sg = n.y >= 0.0 ? 1.0 : -1.0;\n" \
"        SurfS s = hexsample(slice, vec2(-p.x * sg * k, p.z * k), vec2(-dpx.x * sg, dpx.z), vec2(-dpy.x * sg, dpy.z), sc.x);\n" \
"        alb += bw.y * s.alb; h += bw.y * s.h; dn += bw.y * vec3(-sg * s.n.x, 0.0, s.n.y); tw += bw.y;\n" \
"    }\n" \
"    o.alb = 1.0 + (alb / tw - 1.0) * sa.z; o.h = h / tw; o.dn = dn / tw * sa.w;\n" \
"    return o;\n" \
"}\n" \
"vec2 surfripple(vec2 p, float tm) {\n" \
"    vec2 sl = vec2(0.0); float a[4] = float[4](0.3, 1.9, 3.4, 5.0), lw[4] = float[4](0.9, 1.6, 2.7, 4.3), am[4] = float[4](0.012, 0.02, 0.03, 0.045);\n" \
"    for (int i = 0; i < 4; i++) { vec2 d = vec2(cos(a[i]), sin(a[i])); float k = 6.2831853 / lw[i], w = sqrt(9.81 * k);\n" \
"        sl += am[i] * k * d * cos(k * dot(d, p) - w * tm); }\n" \
"    return sl;\n" \
"}\n" \
"SurfR surfset(vec4 sa, vec4 sv, vec4 sk, vec4 sc, vec3 p, vec3 n, vec3 dp0, vec3 dp1, vec2 st, vec2 ds0, vec2 ds1) {\n" \
"    SurfR o; o.alb = vec3(1.0); o.h = 0.5; o.dn = vec3(0.0);\n" \
"    if (sa.x == -3.0) { if (abs(n.z) > 0.6) o.dn = vec3(-surfripple(p.xy, u.smisc.y), 0.0) * sa.w; return o; }\n" \
"    if (sa.x < 0.0) return o;\n" \
"    if (u.smisc.z > 0.0) {\n" \
"        SurfS s = hexsample(int(sa.x + 0.5), st * u.smisc.z, ds0 * u.smisc.z, ds1 * u.smisc.z, sc.x);\n" \
"        o.alb = 1.0 + (s.alb - 1.0) * sa.z; o.h = s.h; return o; }\n" \
"    int nv = int(sv.z + 0.5);\n" \
"    if (nv < 2) return surfone(int(sa.x + 0.5), sa.y, sa, sc, p, n, dp0, dp1);\n" \
"    int sl[3] = int[3](int(sa.x + 0.5), int(max(sv.x, 0.0) + 0.5), int(max(sv.y, 0.0) + 0.5));\n" \
"    float kk[3] = float[3](sa.y, sk.x, sk.y), br[3] = float[3](1.0, sk.z, sk.w);\n" \
"    float fu = vfbm(p.xy * sv.w + sc.w * 37.0);\n" \
"    float f = clamp((fu - 0.5) * 2.6 + 0.5, 0.0, 1.0) * float(nv - 1);\n" \
"    int i = min(int(f), nv - 2); float t = f - float(i), w = 0.1, hk = 0.35;\n" \
"    if (t < 0.5 - w - hk || t > 0.5 + w + hk) { int j = t < 0.5 ? i : i + 1; o = surfone(sl[j], kk[j], sa, sc, p, n, dp0, dp1); o.alb *= br[j]; return o; }\n" \
"    SurfR A = surfone(sl[i], kk[i], sa, sc, p, n, dp0, dp1), B = surfone(sl[i + 1], kk[i + 1], sa, sc, p, n, dp0, dp1);\n" \
"    float wb = smoothstep(0.5 - w, 0.5 + w, t + (B.h - A.h) * hk);\n" \
"    o.alb = mix(A.alb * br[i], B.alb * br[i + 1], wb); o.h = mix(A.h, B.h, wb); o.dn = mix(A.dn, B.dn, wb);\n" \
"    return o;\n" \
"}\n" \
"float surflin(float c) { return c <= 0.04045 ? c / 12.92 : pow((c + 0.055) / 1.055, 2.4); }\n" \
"float surfsrgb(float c) { c = clamp(c, 0.0, 1.0); return c <= 0.0031308 ? 12.92 * c : 1.055 * pow(c, 1.0 / 2.4) - 0.055; }\n" \
"vec4 surfshade(vec2 st, vec3 p, vec3 n, vec3 ref, vec3 vc) {\n" \
"    vec4 P = texture(s_mapp, st), L = texture(s_mapl, st);\n" \
"    vec2 V = texture(s_varl, st).rg + 0.5;\n" \
"    int lo = int(P.r * 255.0 + 0.5), hi = int(P.g * 255.0 + 0.5); float t = L.b;\n" \
"    vec3 dp0 = dFdx(p), dp1 = dFdy(p); float ft = fwidth(t), fa = fwidth(L.a);\n" \
"    vec2 ds0 = dFdx(st), ds1 = dFdy(st);\n" \
"    vec3 orig = vec3(surflin(ref.r), surflin(ref.g), surflin(ref.b));\n" \
"    SurfR a = surfset(u.sa[lo], u.sv[lo], u.sk[lo], u.sc[lo], p, n, dp0, dp1, st, ds0, ds1);\n" \
"    bool lay = u.sflags.y > 0.5;\n" \
"    vec3 ca = u.sa[lo].x == -1.0 ? orig : lay ? orig * a.alb : u.sb[lo].rgb * (1.0 + (V.x - 1.0) * u.sb[lo].w) * a.alb;\n" \
"    vec3 alb = ca, dn = a.dn;\n" \
"    if (hi != lo) {\n" \
"        float soft = max(u.sc[lo].y, u.sc[hi].y), hk = max(u.sc[lo].z, u.sc[hi].z);\n" \
"        float fw = max(ft * 0.75, max(soft, 1e-3));\n" \
"        if (t > 0.5 - fw - hk) {\n" \
"            SurfR b = surfset(u.sa[hi], u.sv[hi], u.sk[hi], u.sc[hi], p, n, dp0, dp1, st, ds0, ds1);\n" \
"            float wb = smoothstep(0.5 - fw, 0.5 + fw, t + (b.h - a.h) * hk);\n" \
"            vec3 cb = u.sa[hi].x == -1.0 ? orig : lay ? orig * b.alb : u.sb[hi].rgb * (1.0 + (V.y - 1.0) * u.sb[hi].w) * b.alb;\n" \
"            alb = mix(ca, cb, wb); dn = mix(a.dn, b.dn, wb);\n" \
"        }\n" \
"    }\n" \
"    if (u.sflags.y < 0.5) alb *= 0.92 + 0.16 * vfbm(p.xy * (1.0 / 120.0) + 3.1);\n" \
"    if (u.sflags.w == 1.0) alb = 0.18 * a.alb;\n" \
"    if (u.sflags.w == 2.0) { float id = u.sc[(hi != lo && t > 0.5) ? hi : lo].w; float hh = fract(id * 0.618034);\n" \
"        alb = 0.5 * clamp(abs(fract(hh + vec3(0.0, 0.6667, 0.3333)) * 6.0 - 3.0) - 1.0, 0.0, 1.0); }\n" \
"    float sh = clamp(1.0 + 0.8 * dot(dn, vec3(0.42, 0.33, 0.84)), 0.6, 1.4);\n" \
"    vec3 c = u.sflags.w > 0.5 ? alb : alb * sh;\n" \
"    float al = u.smisc.x > 0.5 ? smoothstep(0.5 - max(fa * 0.75, 0.02), 0.5 + max(fa * 0.75, 0.02), L.a) : -1.0;\n" \
"    return vec4(surfsrgb(c.r), surfsrgb(c.g), surfsrgb(c.b), al);\n" \
"}\n"

typedef struct gsurf { GLuint map, var; } gsurf;

static GLuint  s_surf_smp[3];              /* nearest, linear, trilinear (anisotropic): all repeat */
static GLuint  s_seta, s_setb;
static int     s_surf_up;                  /* the sets: 0 not asked, 1 reading, 2 on the GPU, 3 failed */
static gsurf   s_surf[SURF_ENTRIES];
static GLuint  s_surf_mid, s_surf_mida[2]; /* the stand-ins: a 1 x 1 map, two 1-layer arrays */
static int     s_tsurf[TEX_MAX];           /* each texture's entry (light_surf_entry_rgba), -1 none */

static GLuint surf_tex2d(GLenum ifmt, GLenum fmt, int w, int h, const void *px)
{
    GLuint t;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexImage2D(GL_TEXTURE_2D, 0, (GLint)ifmt, w, h, 0, fmt, GL_UNSIGNED_BYTE, px);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    return t;
}

static GLuint surf_array(GLenum ifmt, GLenum fmt, int size, int layers, int levels)
{
    GLuint t;
    int l;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D_ARRAY, t);
    for (l = 0; l < levels; l++)
        glTexImage3D(GL_TEXTURE_2D_ARRAY, l, (GLint)ifmt, size >> l, size >> l, layers, 0, fmt, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_LEVEL, levels - 1);
    return t;
}

/* the samplers, the stand-ins, the units' names (brr_open, the program bound) */
static int surf_init(GLuint prog)
{
    static const uint8_t mid[4] = { 128, 128, 128, 255 };
    static const char *const names[5] = { "s_mapp", "s_mapl", "s_varl", "s_seta", "s_setb" };
    GLfloat amax = 1;
    int k, aniso = 0;
    GLint n = 0;
    glGetIntegerv(GL_NUM_EXTENSIONS, &n);
    for (k = 0; k < n; k++) {
        const char *e = (const char *)glGetStringi(GL_EXTENSIONS, (GLuint)k);
        if (e && (!strcmp(e, "GL_EXT_texture_filter_anisotropic") || !strcmp(e, "GL_ARB_texture_filter_anisotropic")))
            aniso = 1;
    }
    if (aniso)
        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &amax);
    glGenSamplers(3, s_surf_smp);
    for (k = 0; k < 3; k++) {
        glSamplerParameteri(s_surf_smp[k], GL_TEXTURE_MIN_FILTER, k == 0 ? GL_NEAREST : k == 1 ? GL_LINEAR : GL_LINEAR_MIPMAP_LINEAR);
        glSamplerParameteri(s_surf_smp[k], GL_TEXTURE_MAG_FILTER, k ? GL_LINEAR : GL_NEAREST);
        glSamplerParameteri(s_surf_smp[k], GL_TEXTURE_WRAP_S, GL_REPEAT);
        glSamplerParameteri(s_surf_smp[k], GL_TEXTURE_WRAP_T, GL_REPEAT);
        if (k == 2 && aniso)
            glSamplerParameterf(s_surf_smp[k], GL_TEXTURE_MAX_ANISOTROPY, amax < 16 ? amax : 16);
    }
    glActiveTexture(GL_TEXTURE0);
    s_surf_mid = surf_tex2d(GL_RGBA8, GL_RGBA, 1, 1, mid);
    for (k = 0; k < 2; k++) {
        s_surf_mida[k] = surf_array(k ? GL_RG8 : GL_RGBA8, k ? GL_RG : GL_RGBA, 1, 1, 1);
        glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, 1, 1, 1, k ? GL_RG : GL_RGBA, GL_UNSIGNED_BYTE, mid);
    }
    for (k = 0; k < 5; k++)
        glUniform1i(glGetUniformLocation(prog, names[k]), SURF_UNIT + k);
    for (k = 0; k < TEX_MAX; k++)
        s_tsurf[k] = -1;
    return 1;
}

/* the sets onto the GPU, once read */
static void surf_upload_sets(void)
{
    int size = light_surf_sets_size(), levels = light_surf_set_levels(), nset = k_light_surf_nset, k, i, l, n;
    glActiveTexture(GL_TEXTURE0);
    for (k = 0; k < 2; k++) {
        GLenum fmt = k ? GL_RG : GL_RGBA;
        GLuint t = surf_array(k ? GL_RG8 : GL_RGBA8, fmt, size, nset, levels);
        for (i = 0; i < nset; i++)
            for (l = 0; l < levels; l++) {
                const uint8_t *px = light_surf_set_level(i, k, l, &n);
                glTexSubImage3D(GL_TEXTURE_2D_ARRAY, l, 0, 0, i, n, n, 1, fmt, GL_UNSIGNED_BYTE, px);
            }
        if (k)
            s_setb = t;
        else
            s_seta = t;
    }
    light_surf_sets_release();
    s_surf_up = 2;
    if (getenv("BR_LIGHT_STATS"))
        fprintf(stderr, "brr: surface sets on the GPU: %d at %d, %d levels\n", nset, size, levels);
}

/* a frame with the textures on: the sets read (from the first), uploaded once read */
static void surf_frame(void)
{
    if (s_surf_up == 0) {
        light_surf_sets_load(2048);
        s_surf_up = 1;
    }
    if (s_surf_up == 1 && light_surf_sets_ready())
        surf_upload_sets();
}

/* an entry's textures, made on first use (the sets on the GPU): 1, or 0 for the stand-ins */
static int surf_entry(int e)
{
    const light_surf_map *m;
    gsurf *s;
    if (e < 0 || e >= SURF_ENTRIES || s_surf_up != 2)
        return 0;
    s = &s_surf[e];
    if (s->map)
        return 1;
    if (!(m = light_surf_entry_map(e)))
        return 0;
    glActiveTexture(GL_TEXTURE0);
    s->map = surf_tex2d(GL_RGBA8, GL_RGBA, m->w, m->h, m->map);
    s->var = surf_tex2d(GL_RG8, GL_RG, m->w, m->h, m->var);
    return 1;
}

/* units 2..6 for a draw: entry e's map, or the stand-ins (e < 0) */
static void surf_bind(int e)
{
    GLuint map = e >= 0 ? s_surf[e].map : s_surf_mid, var = e >= 0 ? s_surf[e].var : s_surf_mid;
    GLuint a = e >= 0 ? s_seta : s_surf_mida[0], b = e >= 0 ? s_setb : s_surf_mida[1];
    static GLuint bound[5];
    GLuint want[5];
    int k;
    want[0] = want[1] = map;
    want[2] = var;
    want[3] = a;
    want[4] = b;
    for (k = 0; k < 5; k++) {
        if (bound[k] == want[k])
            continue;
        glActiveTexture(GL_TEXTURE0 + SURF_UNIT + k);
        glBindTexture(k < 3 ? GL_TEXTURE_2D : GL_TEXTURE_2D_ARRAY, want[k]);
        if (!bound[k])
            glBindSampler((GLuint)(SURF_UNIT + k), s_surf_smp[k == 0 ? 0 : k < 3 ? 1 : 2]);
        bound[k] = want[k];
    }
    glActiveTexture(GL_TEXTURE0);
}
