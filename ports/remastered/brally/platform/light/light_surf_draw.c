/* light_surf_draw.c: what every renderer needs to draw the Remastered textures
 * (light_surf.h): each uploaded texture's map, kept by hash; a draw's
 * constants; the photographed sets, read in the background.  Nothing here
 * knows a graphics API. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "light_surf.h"
#include "light_texmat.h"
#include "host.h"

/* ---- the maps -------------------------------------------------------------------------- */
#define ENTRIES 4096

typedef struct {
    uint32_t hash;
    int made;                       /* 1: a map; 0: the game's own look */
    int layout;
    light_surf_map map;
} Entry;

static Entry s_e[ENTRIES];
static int s_ne;

int light_surf_entry(uint32_t hash, int mat, const uint8_t *rgba, int w, int h)
{
    int i;
    for (i = 0; i < s_ne; i++)
        if (s_e[i].hash == hash)
            return s_e[i].made ? i : -1;
    if (s_ne >= ENTRIES || w <= 0 || h <= 0)
        return -1;
    i = s_ne;
    s_e[i].hash = hash;
    s_e[i].made = light_surf_build(mat, rgba, w, h, &s_e[i].map);
    s_e[i].layout = mat == LIGHT_MAT_BUILDING || mat == LIGHT_MAT_SIGN || mat == LIGHT_MAT_WOOD || mat == LIGHT_MAT_METAL;
    s_ne++;
    return s_e[i].made ? i : -1;
}

const light_surf_map *light_surf_entry_map(int entry)
{
    return entry >= 0 && entry < s_ne && s_e[entry].made ? &s_e[entry].map : NULL;
}

int light_surf_view(void)
{
    static int v = -1;
    if (v < 0)
        v = getenv("BR_SURF_VIEW") ? atoi(getenv("BR_SURF_VIEW")) : 0;
    return v;
}

/* ---- the constants ----------------------------------------------------------------------- */
static float luma(const float c[3]) { return 0.2126f * c[0] + 0.7152f * c[1] + 0.0722f * c[2]; }

/* Oklab (Ottosson 2020) from and to linear sRGB */
static void oklab(const float c[3], float o[3])
{
    float l = cbrtf(0.4122214708f * c[0] + 0.5363325363f * c[1] + 0.0514459929f * c[2]);
    float m = cbrtf(0.2119034982f * c[0] + 0.6806995451f * c[1] + 0.1073969566f * c[2]);
    float s = cbrtf(0.0883024619f * c[0] + 0.2817188376f * c[1] + 0.6299787005f * c[2]);
    o[0] = 0.2104542553f * l + 0.7936177850f * m - 0.0040720468f * s;
    o[1] = 1.9779984951f * l - 2.4285922050f * m + 0.4505937099f * s;
    o[2] = 0.0259040371f * l + 0.7827717662f * m - 0.8086757660f * s;
}
static void oklab_rgb(const float o[3], float c[3])
{
    float l = o[0] + 0.3963377774f * o[1] + 0.2158037573f * o[2];
    float m = o[0] - 0.1055613458f * o[1] - 0.0638541728f * o[2];
    float s = o[0] - 0.0894841775f * o[1] - 1.2914855480f * o[2];
    l = l * l * l; m = m * m * m; s = s * s * s;
    c[0] = +4.0767416621f * l - 3.3077115913f * m + 0.2309699292f * s;
    c[1] = -1.2684380046f * l + 2.6097574011f * m - 0.3413193965f * s;
    c[2] = -0.0041960863f * l - 0.7034186147f * m + 1.7076147010f * s;
}

void light_surf_uniforms(int entry, float albk, int lit, LightSurfU *u)
{
    const Entry *e = &s_e[entry];
    int r, k;
    memset(u, 0, sizeof *u);
    u->flags[0] = 1.0f;
    u->flags[1] = (float)e->layout;
    u->flags[2] = lit ? 0.0f : 1.0f;
    u->flags[3] = (float)light_surf_view();
    for (r = 0; r < e->map.n; r++) {
        int s = e->map.surf[r], nv;
        const light_surf_info *in = &k_light_surf[s];
        float c[3], y, t = -1.0f;
        /* the region's colour: its texels' mean; for the lighting, scaled as it scales the whole
           texture (0.85 and lit_albk: the texture's mean to its material's albedo), so its regions
           keep their light and dark against each other; for the game's own lighting, the texel */
        for (k = 0; k < 3; k++)
            c[k] = e->map.lin[r][k] * (lit ? 0.85f * albk : 1.0f);
        if (lit) {
            /* the surfaces whose albedo is known whatever the 1999 art's: road paint (retroreflective
               paint about 0.7 white, 0.45 yellow), snow 0.8, the foam on a beach 0.6 */
            switch (s) {
            case LIGHT_SURF_PAINT_WHITE: t = 0.70f; break;
            case LIGHT_SURF_PAINT_YELLOW: t = 0.45f; break;
            case LIGHT_SURF_SNOW: t = 0.80f; break;
            case LIGHT_SURF_FOAM: t = 0.60f; break;
            }
            y = luma(c);
            if (t > 0.0f && y > 1e-4f)
                for (k = 0; k < 3; k++)
                    c[k] *= t / y;
        }
        if (s == LIGHT_SURF_PAINT_WHITE) {   /* white paint is white: the art's tint (its baked light) out */
            y = luma(c);
            c[0] = c[1] = c[2] = y;
        }
        {   /* the surface's grade to Forza Horizon 6's: its luminance and its chroma (Oklab, the hue kept) */
            float o[3], g = lit ? in->lum : in->lum_o, ch = lit ? in->chroma : in->chroma_o;
            for (k = 0; k < 3; k++)
                c[k] *= g;
            oklab(c, o);
            o[1] *= ch;
            o[2] *= ch;
            oklab_rgb(o, c);
        }
        for (k = 0; k < 3; k++)
            c[k] = c[k] < 0.0f ? 0.0f : c[k] > (lit ? 0.9f : 1.0f) ? (lit ? 0.9f : 1.0f) : c[k];
        u->sa[r][0] = in->set[0] >= 0 ? (float)in->set[0]
                      : (s == LIGHT_SURF_PAINT_SIGN ? -2.0f : -1.0f);   /* a sign's print: flat; water, leaves, glass: the game's */
        u->sa[r][1] = in->set[0] >= 0 ? 1.0f / k_light_surf_set[in->set[0]].metres : 0.0f;
        u->sa[r][2] = in->detail;
        u->sa[r][3] = in->normal;
        nv = in->set[0] < 0 ? 0 : in->set[1] < 0 ? 1 : in->set[2] < 0 ? 2 : 3;
        for (k = 0; k < 3; k++)
            c[k] *= in->vbright[0];
        memcpy(u->sb[r], c, sizeof c);
        u->sb[r][3] = in->var;
        /* planks and brushed concrete keep their direction; ground turns at random */
        u->sc[r][0] = in->set[0] == 13 || in->set[0] == 11 ? 0.0f : 1.0f;
        u->sc[r][1] = s == LIGHT_SURF_PAINT_WHITE || s == LIGHT_SURF_PAINT_YELLOW || e->layout ? 0.0f : 0.06f;
        u->sc[r][2] = s == LIGHT_SURF_PAINT_WHITE || s == LIGHT_SURF_PAINT_YELLOW || e->layout ? 0.0f : 0.35f;
        u->sc[r][3] = (float)s;
        u->sv[r][0] = nv > 1 ? (float)in->set[1] : -1.0f;
        u->sv[r][1] = nv > 2 ? (float)in->set[2] : -1.0f;
        u->sv[r][2] = (float)nv;
        u->sv[r][3] = in->vmetres > 0.0f ? 1.0f / in->vmetres : 0.0f;
        u->sk[r][0] = nv > 1 ? 1.0f / k_light_surf_set[in->set[1]].metres : 0.0f;
        u->sk[r][1] = nv > 2 ? 1.0f / k_light_surf_set[in->set[2]].metres : 0.0f;
        u->sk[r][2] = in->vbright[1] / in->vbright[0];
        u->sk[r][3] = in->vbright[2] / in->vbright[0];
    }
}

/* ---- the sets -------------------------------------------------------------------------- */
static uint8_t **s_a, **s_b;
static int s_size;
static volatile int s_ready;          /* written once by the loader, after every texel */
static int s_released;                /* the texels freed (a GPU holds them) */
static host_mutex *s_lock;

static const char *sets_dir(char *buf, size_t n)
{
    const char *e = getenv("BR_SURFDIR");
    FILE *f;
    if (e)
        return e;
    if ((f = fopen("ports/common/surfaces/surfaces.txt", "r"))) {   /* the tree's (a build run from the repository) */
        fclose(f);
        return "ports/common/surfaces";
    }
    snprintf(buf, n, "%s/surfaces", host_resource_dir());   /* the app's (package_app.sh, ios/build.sh, the Windows zip) */
    return buf;
}

/* a baked set file ("BRSURF 1 w h channels" then the texels): them, or NULL */
static uint8_t *read_set(const char *path, int size, int ch)
{
    FILE *f = fopen(path, "rb");
    char line[64];
    int v, w, h, c;
    uint8_t *px;
    size_t n = (size_t)size * size * ch;
    if (!f)
        return NULL;
    if (!fgets(line, sizeof line, f) || sscanf(line, "BRSURF %d %d %d %d", &v, &w, &h, &c) != 4 || v != 1 || w != size || h != size || c != ch) {
        fclose(f);
        return NULL;
    }
    px = malloc(n);
    if (px && fread(px, 1, n, f) != n) {
        free(px);
        px = NULL;
    }
    fclose(f);
    return px;
}

static void *load_thread(void *arg)
{
    char buf[1100], path[1400];
    const char *dir = sets_dir(buf, sizeof buf);
    const char *sub = s_size == 2048 ? "baked" : "baked1k";
    int i;
    (void)arg;
    for (i = 0; i < k_light_surf_nset; i++) {
        snprintf(path, sizeof path, "%s/%s/%s_a.raw", dir, sub, k_light_surf_set[i].id);
        s_a[i] = read_set(path, s_size, 4);
        snprintf(path, sizeof path, "%s/%s/%s_b.raw", dir, sub, k_light_surf_set[i].id);
        s_b[i] = read_set(path, s_size, 2);
        if (!s_a[i] || !s_b[i]) {
            fprintf(stderr, "light: no surface set %s (ports/common/surfaces: fetch.py, bake.py): the Remastered textures are off\n", path);
            return NULL;
        }
    }
    host_mutex_lock(s_lock);
    s_ready = 1;
    host_mutex_unlock(s_lock);
    if (getenv("BR_LIGHT_STATS"))
        fprintf(stderr, "light: surface sets read: %d at %d from %s/%s\n", k_light_surf_nset, s_size, dir, sub);
    return NULL;
}

void light_surf_sets_load(int size)
{
    if (s_a)
        return;
    s_size = getenv("BR_SURF_1K") && atoi(getenv("BR_SURF_1K")) ? 1024 : size;
    s_a = calloc((size_t)k_light_surf_nset, sizeof *s_a);
    s_b = calloc((size_t)k_light_surf_nset, sizeof *s_b);
    s_lock = host_mutex_new();
    host_thread_start(load_thread, NULL);
}

int light_surf_sets_ready(void)
{
    int r;
    if (!s_lock)
        return 0;
    host_mutex_lock(s_lock);   /* (the lock orders the loader's texels before the flag) */
    r = s_ready == 1;
    host_mutex_unlock(s_lock);
    return r;
}

int light_surf_sets_size(void) { return s_size; }
const uint8_t *light_surf_set_a(int set) { return set >= 0 && set < k_light_surf_nset && s_a && !s_released ? s_a[set] : NULL; }
const uint8_t *light_surf_set_b(int set) { return set >= 0 && set < k_light_surf_nset && s_b && !s_released ? s_b[set] : NULL; }

/* the mip chains: level 0 the set's texels, each further level the rounded mean of 2 x 2 */
static uint8_t **s_mip[2];
static int s_levels;

int light_surf_set_levels(void)
{
    int n = s_size, l = 1;
    while (n > 1) {
        n >>= 1;
        l++;
    }
    return l;
}

const uint8_t *light_surf_set_level(int set, int k, int l, int *n)
{
    int ch = k ? 2 : 4, i;
    if (!light_surf_sets_ready() || set < 0 || set >= k_light_surf_nset || l < 0 || l >= light_surf_set_levels())
        return NULL;
    if (!s_mip[k]) {
        s_levels = light_surf_set_levels();
        s_mip[k] = calloc((size_t)k_light_surf_nset * s_levels, sizeof(uint8_t *));
    }
    *n = s_size >> l;
    if (l == 0)
        return k ? s_b[set] : s_a[set];
    if (!s_mip[k][set * s_levels + l]) {
        int m = s_size >> l, pn;
        const uint8_t *src = light_surf_set_level(set, k, l - 1, &pn);
        uint8_t *dst;
        int x, y, c;
        if (!src)
            return NULL;
        dst = malloc((size_t)m * m * ch);
        for (y = 0; y < m; y++)
            for (x = 0; x < m; x++)
                for (c = 0; c < ch; c++) {
                    int sum = src[((2 * y) * pn + 2 * x) * ch + c] + src[((2 * y) * pn + 2 * x + 1) * ch + c]
                            + src[((2 * y + 1) * pn + 2 * x) * ch + c] + src[((2 * y + 1) * pn + 2 * x + 1) * ch + c];
                    dst[(y * m + x) * ch + c] = (uint8_t)((sum + 2) / 4);
                }
        s_mip[k][set * s_levels + l] = dst;
    }
    (void)i;
    return s_mip[k][set * s_levels + l];
}

void light_surf_sets_release(void)
{
    int i, k, l;
    if (!light_surf_sets_ready())
        return;
    for (i = 0; i < k_light_surf_nset; i++) {
        free(s_a[i]);
        free(s_b[i]);
        s_a[i] = s_b[i] = NULL;
        for (k = 0; k < 2; k++)
            if (s_mip[k])
                for (l = 1; l < s_levels; l++) {
                    free(s_mip[k][i * s_levels + l]);
                    s_mip[k][i * s_levels + l] = NULL;
                }
    }
    s_released = 1;
}

int light_surf_entry_rgba(const uint8_t *rgba, int w, int h)
{
    uint32_t hs = 2166136261u;
    size_t i, n = (size_t)w * (size_t)h * 4;
    int lo = 0, hi = (int)(sizeof k_light_texmat / sizeof k_light_texmat[0]) - 1, mat = -1;
    for (i = 0; i < n; i += 4)   /* per texel, its RGBA8 as one little-endian word (texmat.csv's hash) */
        hs = (hs ^ ((uint32_t)rgba[i] | (uint32_t)rgba[i + 1] << 8 | (uint32_t)rgba[i + 2] << 16 | (uint32_t)rgba[i + 3] << 24)) * 16777619u;
    while (lo <= hi) {
        int m = (lo + hi) / 2;
        if (k_light_texmat[m].hash == hs) {
            mat = k_light_texmat[m].mat;
            break;
        }
        if (k_light_texmat[m].hash < hs)
            lo = m + 1;
        else
            hi = m - 1;
    }
    return light_surf_entry(hs, mat, rgba, w, h);
}
