/* light_surf.h: the Remastered textures (BR_FLAG_TEXTURES; docs/brally/
 * remaster/textures/DESIGN.md).
 *
 * Each game texture of a natural or built surface is read as a map of
 * surfaces: which texels are asphalt, a road's paint, grass, sand, rock, a
 * wall's plaster..., from their colours in the context of the texture's
 * material (light_texmat.h).  The map is made at four times the texture's
 * size with smooth borders, and the lit renderer draws each surface in a
 * photographed material laid over the world at its real size, in the
 * region's own colour.  Textures of other kinds (signs' art keeps its layout
 * too, in flat paint; effects, sky, lamps, water, cars keep the game's texels)
 * are left as they were.
 *
 * Every renderer draws them the same way, under either lighting: the
 * Remastered lighting takes the result as albedo (Metal's G-buffer), the
 * game's own drawing as the texel its combiners read (Metal, Vulkan, and the
 * software renderer through light_surf_shade, the reference the shaders
 * follow: brr_metal_surf.h, shaders/surf.glsl).
 *
 * Made here, when the game uploads a texture: nothing derived from the game's
 * data is shipped.  ports/remastered/tools/texregion.py is the reference the
 * maps follow; surfparity.sh holds the two to the same maps. */
#ifndef BR_LIGHT_SURF_H
#define BR_LIGHT_SURF_H

#include <stdint.h>

/* the surfaces, each one photographed material (light_surf.c k_light_surf) */
enum {
    LIGHT_SURF_NONE,            /* the game's own texels, lit as before */
    LIGHT_SURF_ASPHALT, LIGHT_SURF_PAINT_WHITE, LIGHT_SURF_PAINT_YELLOW, LIGHT_SURF_GRAVEL,
    LIGHT_SURF_DIRT, LIGHT_SURF_DIRT_RED, LIGHT_SURF_SAND, LIGHT_SURF_SAND_WET, LIGHT_SURF_FOAM,
    LIGHT_SURF_WATER, LIGHT_SURF_GRASS, LIGHT_SURF_GRASS_DRY, LIGHT_SURF_MOSS, LIGHT_SURF_ROCK_GREY,
    LIGHT_SURF_ROCK_BROWN, LIGHT_SURF_ROCK_RED, LIGHT_SURF_CONCRETE, LIGHT_SURF_PLASTER, LIGHT_SURF_BRICK,
    LIGHT_SURF_STONE, LIGHT_SURF_WOOD, LIGHT_SURF_METAL, LIGHT_SURF_RUST, LIGHT_SURF_SNOW, LIGHT_SURF_ICE,
    LIGHT_SURF_ROOF, LIGHT_SURF_LEAVES, LIGHT_SURF_PAINT_SIGN,
    LIGHT_SURF_COUNT
};

#define LIGHT_SURF_UP 4         /* the map's size: the texture's times this */
#define LIGHT_SURF_REGIONS 8    /* the most surfaces one texture's map holds */

/* a surface's material: its photographed sets (slices in the arrays; up to
 * three variants laid over the ground in patches, Forza's mix of one road's
 * fresh, worn and repaired stretches), and how it is drawn */
typedef struct {
    const char *name;
    int set[3];                 /* its variants: indices into k_light_surf_set (the first -1: no
                                   photographed detail; a later -1: fewer variants) */
    float vbright[3];           /* each variant's brightness against the region's colour */
    float vmetres;              /* the size of the patches the variants share the ground in */
    float detail;               /* the set's albedo detail kept: 1 as photographed, 0 flat */
    float normal;               /* its relief kept (normal map strength) */
    float var;                  /* how much of the game texture's own light and dark inside the region shows */
    float rough;                /* roughness, dry */
    float lum, chroma;          /* the grade to Forza Horizon 6's surfaces under the Remastered
                                   lighting: the region colour's luminance and Oklab chroma
                                   scaled (DESIGN.md section 4) */
    float lum_o, chroma_o;      /* the same, under the game's own lighting (its texel) */
} light_surf_info;

/* a photographed set: Poly Haven's id (CC0), its side in metres */
typedef struct { const char *id; float metres; } light_surf_set;

extern const light_surf_info k_light_surf[LIGHT_SURF_COUNT];
extern const light_surf_set k_light_surf_set[];
extern const int k_light_surf_nset;

/* a texture's surface map */
typedef struct {
    int w, h;                   /* the map's (LIGHT_SURF_UP times the texture's) */
    uint8_t *map;               /* w*h RGBA8: R the lower region, G the higher, B where the
                                   texel sits between them (0 the lower, 255 the higher,
                                   128 the border), A coverage */
    uint8_t *var;               /* w*h RG8: each of the two regions' own brightness against
                                   its mean, 128 + 255 * (v - 1), v in 0.5..1.5 */
    int n;                      /* its regions */
    uint8_t surf[LIGHT_SURF_REGIONS];       /* each region's surface */
    float lin[LIGHT_SURF_REGIONS][3];       /* and its texels' mean colour, linear */
} light_surf_map;

/* the map of a texture (RGBA8, w x h) of material mat (LIGHT_MAT_*, or -1
 * untagged): 1 and *out filled (light_surf_free), or 0 when the texture keeps
 * the game's own look */
int  light_surf_build(int mat, const uint8_t *rgba, int w, int h, light_surf_map *out);
void light_surf_free(light_surf_map *m);

/* ---- what a renderer needs (light_surf_draw.c) ---------------------------------------- */
/* a draw's constants, one block of float4s (Metal's buffer, Vulkan's std140 uniform, the
 * software renderer's argument) */
typedef struct LightSurfU {
    float sa[8][4];     /* per region: its first variant's slice (-1 the game's texel, -2 the region's flat colour,
                           -3 water: its colour and moving ripples), 1 / its metres, detail, relief */
    float sb[8][4];     /* the region's colour (linear) and how much of its own light and dark shows */
    float sc[8][4];     /* turn the set at random (1) or only move it (0), its border's softness, its height's weight at a border, the surface */
    float sv[8][4];     /* its second and third variants' slices (-1 none), how many variants, 1 / their patches' size in metres */
    float sk[8][4];     /* 1 / the second and third variants' metres, their brightness against the first's */
    float flags[4];     /* x 1: a map; y 1: a layout (the vertex colour's hue applies, lit); z 0: albedo for
                           the Remastered lighting, 1: the texel for the game's own; w a diagnostic view */
    float misc[4];      /* x 1: a cutout (foliage: the map's coverage is the alpha, crisp); y the game's clock,
                           seconds (water); z > 0: the detail in the texture's own space, that many tiles across
                           (foliage cards turn to the camera: world space would swim); w 0 */
} LightSurfU;

/* a texture as the game uploads it: its entry (its map made now, kept by hash for the run),
 * or -1 when it keeps the game's own look */
int  light_surf_entry(uint32_t hash, int mat, const uint8_t *rgba, int w, int h);
const light_surf_map *light_surf_entry_map(int entry);
/* the entry's constants: albk the lighting's albedo scale for the texture (lit), lit 1 for the
 * Remastered lighting's albedo, 0 for the game's own texel */
void light_surf_uniforms(int entry, float albk, int lit, LightSurfU *u);
/* BR_SURF_VIEW (diagnostic): 1 the sets' detail alone on grey; 2 each surface in its own colour
 * (its id times 0.618034 turns of hue, linear), everything else black */
int  light_surf_view(void);

/* the photographed sets (ports/common/surfaces, baked by bake.py): loading in the background
 * from the first call (once), at size 2048 or 1024 (BR_SURF_1K, the iPhone's); BR_SURFDIR the
 * folder, else the tree's ports/common/surfaces, else the app's resources/surfaces */
void light_surf_sets_load(int size);
/* 1 when loaded: each set's albedo detail and height (RGBA8) and normal (RG8), size x size */
int  light_surf_sets_ready(void);
int  light_surf_sets_size(void);
const uint8_t *light_surf_set_a(int set);
const uint8_t *light_surf_set_b(int set);
/* level l (0 the full size; each the mean of the last's 2 x 2) of a set's albedo (k 0, RGBA8)
 * or normal (k 1, RG8), made the first time it is asked for; its side in *n */
const uint8_t *light_surf_set_level(int set, int k, int l, int *n);
int  light_surf_set_levels(void);
/* the texels freed once a GPU holds them (the software renderer keeps them) */
void light_surf_sets_release(void);
/* a texture as the game uploads it (RGBA8): its texmat hash and material looked up
 * (light_texmat.h) and its entry (light_surf_entry), -1 for none */
int  light_surf_entry_rgba(const uint8_t *rgba, int w, int h);

/* ---- the reference shading (light_surf_shade.c) -------------------------------------- */
/* a texel of an entry's texture at map coordinates st, world point p, shaded normal n (unit),
 * dp0 and dp1 p's change across a pixel in x and y (ds0, ds1 st's), ref the game's texel (sRGB
 * 0..1, for a region of none), vc the vertex colour (0..1): rgb as u.flags.z asks (linear
 * albedo, or the texel, sRGB), dn the relief, *alpha a cutout's alpha (-1: the game's).  The
 * shaders do exactly this, in their own language */
void light_surf_shade(const LightSurfU *u, int entry, const float st[2], const float p[3], const float n[3],
                      const float dp0[3], const float dp1[3], const float ds0[2], const float ds1[2],
                      const float ref[3], const float vc[3], float out[3], float dn[3], float *alpha);

#endif
