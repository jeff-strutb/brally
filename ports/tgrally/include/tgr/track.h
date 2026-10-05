/* track.h -- the loaded track: cartridge data, unpacked to 0x80025C00 and
 * kept big-endian as loaded (tgr_core.h).  Every field is a be16_t/be32_t
 * read through BE16/BE32/BEF/BEPTR; addresses inside are original ones
 * (the track is built for its load address and is never rebased).
 *
 * One definition for what the decomp's files each declared a part of
 * (BrTrackHdr, BrTrackFog, BrTrackGeom, BrTrackGates, BrTrackPaths,
 * BrRaceTrack, BrLoadHdr were views of this header).
 */
#ifndef TGR_TRACK_H
#define TGR_TRACK_H
#include "tgr/vec.h"

typedef struct { bef_t x, y, z; } BrVec3be;    /* a BrVec3 in cartridge data */

/* a track object (0x54 bytes) */
typedef struct BrTrackObj {
    bef_t m[4][4];              /* 0x00  its matrix; m[0][0] is its scale, m[3] its position */
    bef_t scale;                /* 0x40  1 / the length of its x axis (BrTrackLoad) */
    be32_t dl;                  /* 0x44  its display list (segment 6) */
    be16_t hide;                /* 0x48  hidden when it meets D_8028C770 */
    be16_t x4a;
    be16_t flags;               /* 0x4C  0x2000 unscaled; 0x10 a tunnel (no rain, no skids on snow);
                                 *       4 keeps the colour override; 8 set when patched */
    be16_t tris;                /* 0x4E */
    be16_t vtxs;                /* 0x50 */
    be16_t x52;
} BrTrackObj;

/* a track texture in RAM (0x24 bytes) */
typedef struct BrTexSlot {
    be32_t dst;                 /* 0x00  the texture */
    be32_t palDst;              /* 0x04  its palette */
    be32_t anim;                /* 0x08  animated: its frames (BrTexAnim); else a palette index */
    char pad0c[0x20 - 0x0C];
    be32_t bits;                /* 0x20  x20:4 fmt:4 x20b:3 animated:1 x20c:2 size:18, from the top */
} BrTexSlot;
#define BR_TEXSLOT_FMT(t)       ((BE32((t)->bits) >> 24) & 0xF)     /* 1: 16-colour palette; 0xB: a night bank */
#define BR_TEXSLOT_ANIMATED(t)  ((BE32((t)->bits) >> 20) & 1)
#define BR_TEXSLOT_SIZE(t)      (BE32((t)->bits) & 0x3FFFF)         /* texture bytes */

/* one frame of an animated track texture (0xC bytes) */
typedef struct BrTexKey {
    be32_t tex;                 /* texture's ROM offset, -1 for none */
    be32_t pal;                 /* palette's ROM offset, -1 for none */
    be32_t time;                /* when the next frame starts */
} BrTexKey;
typedef struct BrTexAnim {
    be16_t x0;
    be16_t n;                   /* 0x02  frames + 1 */
    be32_t x4;
    be32_t t0;                  /* 0x08  -1: a day/night pair */
    BrTexKey key[1];            /* 0x0C */
} BrTexAnim;

/* a collision triangle's record (8 bytes) */
typedef struct BrTrackTri {
    be16_t v[3];                /* its vertices */
    be16_t surface;             /* 0x06  its surface id */
} BrTrackTri;

/* a timing gate across the track (0x14 bytes) */
typedef struct BrGate {
    bef_t a[2];                 /* 0x00  its two posts, x and y */
    bef_t b[2];                 /* 0x08 */
    bef_t award;                /* 0x10  seconds it would grant (printed only) */
} BrGate;

/* one of the track's special objects (12 bytes) */
typedef struct BrSpecial {
    be32_t obj;                 /* 0x00  its track object */
    be32_t arg;                 /* 0x04  a count, a path, or an angle's bits */
    unsigned char kind;         /* 0x08  0-2 spinner (z, x, y axis), 3 airplane, 4/5 its
                                 *       left/right path, 6 its trigger, 7 waterfall */
    char pad09[3];
} BrSpecial;

/* a point on a path segment (0x28 bytes) */
typedef struct BrPathPt {
    BrVec3be left;              /* 0x00  the track's left edge */
    BrVec3be pos;               /* 0x0C  its centre */
    BrVec3be right;             /* 0x18  its right edge */
    bef_t dist;                 /* 0x24  distance along the track */
} BrPathPt;

/* a path segment */
typedef struct BrPathSeg {
    be32_t next;                /* 0x00  BrPathSeg */
    be32_t alt;                 /* 0x04  taken when this one is closed */
    char pad08[0x10 - 0x08];
    unsigned char x0, y0, x1, y1; /* 0x10  the grid cells it covers */
    be16_t count;               /* 0x14  points */
    be16_t flags;               /* 0x16  bit 0: closed */
    char pad18[0x40 - 0x18];
    BrPathPt pt[1];             /* 0x40 */
} BrPathSeg;

/* the loaded track's header (0x230 bytes) at 0x80025C00 */
typedef struct BrTrackHdr {
    be32_t x00;                 /* 0x00  BrTrackLoad keeps the packed size here until the unpack */
    be32_t hdrSize;             /* 0x04  0x230 */
    char pad08[0x0C - 0x08];
    be32_t tris;                /* 0x0C  BrTrackTri[]: 4 vertex indices per triangle */
    be32_t x10;
    be32_t verts;               /* 0x14  BrVec3be[] */
    be32_t nTex;                /* 0x18 */
    be32_t tex;                 /* 0x1C  BrTexSlot[nTex] */
    be32_t gridEntries;         /* 0x20  be16_t[]: the cells' entries, read through queues (D_80025C20) */
    be32_t gridCells;           /* 0x24  be16_t[64 * 64 + 1]: each cell's first entry (D_80025C24) */
    bef_t x28;                  /* 0x28 */
    bef_t x2c;
    char pad30[0x38 - 0x30];
    bef_t fogLo;                /* 0x38  the altitude the fog starts at (also x38) */
    bef_t fogHi;                /* 0x3C  and where it is full (also x3c) */
    BrVec3be start;             /* 0x40  the start line */
    bef_t heading;              /* 0x4C  facing along the start line */
    be32_t x50;                 /* 0x50  (D_80025C50) */
    char pad54[0x5C - 0x54];
    be32_t names;               /* 0x5C  the objects' names (char *[]), 0 when the build has none */
    be32_t objs;                /* 0x60  BrTrackObj[nObjs] */
    be32_t nObjs;               /* 0x64 */
    be32_t queue2;              /* 0x68  be16_t[]: the second queue table (D_80025C68) */
    be32_t x6c;                 /* 0x6C  be16_t[] (D_80025C6C) */
    be32_t path;                /* 0x70  the path's first segment (D_80025C70) */
    char pad74[0x78 - 0x74];
    be32_t segs;                /* 0x78  every segment (be32_t[nSegs]) */
    be32_t nSegs;               /* 0x7C */
    unsigned char fogColour[3]; /* 0x80  the track's fog colour */
    char pad83;
    be32_t cams;                /* 0x84  the camera checkpoints: BrVec3be[] (D_80025C84) */
    be32_t nCams;               /* 0x88  and their count (D_80025C88) */
    be32_t triggers;            /* 0x8C  zero-terminated trigger id lists (be16_t) */
    be32_t triTrigger;          /* 0x90  each triangle's list in triggers (be16_t) */
    be32_t surf;                /* 0x94  per triangle: surface bits */
    BrGate gate[10];            /* 0x98 */
    be32_t nGates;              /* 0x160 */
    BrSpecial specials[16];     /* 0x164 */
    be32_t nSpecials;           /* 0x224 */
    char pad228[0x230 - 0x228];
} BrTrackHdr;

extern BrTrackHdr D_80025C00;

#define BR_TRACK            (&D_80025C00)
#define BR_TRACKOBJS()      BEPTR(BrTrackObj *, D_80025C00.objs)

/* a cartridge vector or matrix into native floats */
static inline void br_vec3_from(float *out, const BrVec3be *v)
{
    out[0] = BEF(v->x);
    out[1] = BEF(v->y);
    out[2] = BEF(v->z);
}
/* a cartridge vector read natively, for code that hands it to the vector
 * routines: a short-lived copy (one of 32 in turn), never written through */
static inline BrVec3 *BRV(const BrVec3be *p)
{
    static BrVec3 ring[32];
    static int k;
    BrVec3 *v = &ring[k++ & 31];
    v->x = BEF(p->x);
    v->y = BEF(p->y);
    v->z = BEF(p->z);
    return v;
}
/* the same for a few cartridge floats (posts, rows: up to 4) and matrices */
static inline float *BRF(const bef_t *p)
{
    static float ring[32][4];
    static int k;
    float *v = ring[k++ & 31];
    v[0] = BEF(p[0]);
    v[1] = BEF(p[1]);
    v[2] = BEF(p[2]);
    v[3] = BEF(p[3]);
    return v;
}
static inline float (*BRM(const bef_t (*m)[4]))[4]
{
    static float ring[8][4][4];
    static int k;
    float (*v)[4] = ring[k++ & 7];
    int i, j;
    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++)
            v[i][j] = BEF(m[i][j]);
    return v;
}
static inline void br_vec3_to(BrVec3be *v, const float *in)
{
    SETF(v->x, in[0]);
    SETF(v->y, in[1]);
    SETF(v->z, in[2]);
}
static inline void br_mat4_to(bef_t (*m)[4], float (*in)[4])
{
    int i, j;
    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++)
            SETF(m[i][j], in[i][j]);
}
static inline void br_mat4_from(float (*out)[4], const bef_t (*m)[4])
{
    int i, j;
    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++)
            out[i][j] = BEF(m[i][j]);
}
#endif
