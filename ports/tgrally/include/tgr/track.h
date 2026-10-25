/* track.h -- the loaded track.  The cartridge's track file is unpacked to
 * 0x80025C00 (D_80025C00, fixed memory), big-endian and built for that
 * address; BrTrackLoad converts what the game reads into the native records
 * below (tgr_track, platform/os/track.c).  The file stays where it was
 * unpacked for what the renderer reads: the display lists, their vertices
 * and the textures loaded into it.
 *
 * One definition for what the decomp's files each declared a part of
 * (BrTrackHdr, BrTrackFog, BrTrackGeom, BrTrackGates, BrTrackPaths,
 * BrRaceTrack, BrLoadHdr were views of this header; BrNode, of a path
 * segment).
 */
#ifndef TGR_TRACK_H
#define TGR_TRACK_H
#include "tgr/vec.h"

/* a track object */
typedef struct BrTrackObj {
    float m[4][4];              /* its matrix; m[0][0] is its scale, m[3] its position */
    float scale;                /* 1 / the length of its x axis (BrTrackLoad) */
    unsigned int dl;            /* its display list (a segment 6 address in the file) */
    unsigned short hide;        /* hidden when it meets D_8028C770 */
    unsigned short x4a;
    unsigned short flags;       /* 0x2000 unscaled; 0x10 a tunnel (no rain, no skids on snow);
                                 * 4 keeps the colour override; 8 set when patched */
    unsigned short tris;
    unsigned short vtxs;
    unsigned short x52;
} BrTrackObj;

/* one frame of an animated track texture */
typedef struct BrTexKey {
    int tex;                    /* texture's ROM offset, -1 for none */
    int pal;                    /* palette's ROM offset, -1 for none */
    unsigned int time;          /* when the next frame starts */
} BrTexKey;
typedef struct BrTexAnim {
    unsigned short x0;
    unsigned short n;           /* frames + 1 */
    unsigned int x4;
    unsigned int t0;            /* -1: a day/night pair */
    BrTexKey key[1];
} BrTexAnim;

/* a track texture in memory */
typedef struct BrTexSlot {
    unsigned char *dst;         /* the texture, in the file */
    unsigned char *palDst;      /* its palette */
    union {
        BrTexAnim *anim;        /* animated: its frames */
        unsigned int pal;       /* else a palette index */
    };
    unsigned int bits;          /* x20:4 fmt:4 x20b:3 animated:1 x20c:2 size:18, from the top */
} BrTexSlot;
#define BR_TEXSLOT_FMT(t)       (((t)->bits >> 24) & 0xF)     /* 1: 16-colour palette; 0xB: a night bank */
#define BR_TEXSLOT_ANIMATED(t)  (((t)->bits >> 20) & 1)
#define BR_TEXSLOT_SIZE(t)      ((t)->bits & 0x3FFFF)         /* texture bytes */

/* a collision triangle's record */
typedef struct BrTrackTri {
    unsigned short v[3];        /* its vertices */
    unsigned short surface;     /* its surface id */
} BrTrackTri;

/* a timing gate across the track */
typedef struct BrGate {
    float a[2];                 /* its two posts, x and y */
    float b[2];
    float award;                /* seconds it would grant (printed only) */
} BrGate;

/* one of the track's special objects */
typedef struct BrSpecial {
    union {
        int obj;                /* its track object */
        BrVec3 *path;           /* kinds 4 and 5: the airplane's path edge */
    };
    union {                     /* a count, a path, or an angle */
        int arg;
        float angle;
    };
    unsigned char kind;         /* 0-2 spinner (z, x, y axis), 3 airplane, 4/5 its
                                 * left/right path, 6 its trigger, 7 waterfall */
} BrSpecial;

/* a point on a path segment (0x28 bytes, as on the N64) */
typedef struct BrPathPt {
    BrVec3 left;                /* the track's left edge */
    BrVec3 pos;                 /* its centre */
    BrVec3 right;               /* its right edge */
    float dist;                 /* distance along the track */
} BrPathPt;

/* a path segment; the segments are also the scene tree's nodes (nodepass.c) */
typedef struct BrPathSeg {
    union {
        struct BrPathSeg *next;
        struct BrPathSeg *child;    /* as a scene-tree node */
    };
    union {
        struct BrPathSeg *alt;      /* taken when this one is closed */
        struct BrPathSeg *sibling;
    };
    unsigned char x0;               /* the grid cells it covers */
    union {
        unsigned char y0;
        unsigned char kind;         /* as a scene-tree node */
    };
    unsigned char x1, y1;
    unsigned short count;           /* points */
    unsigned short flags;           /* bit 0: closed; bit 15: visited (the scene pass) */
    BrPathPt before;                /* pt[-1], which the walks read at a segment's first point:
                                     * on the N64 the header's last 0x28 bytes */
    BrPathPt pt[1];
} BrPathSeg;
_Static_assert(__builtin_offsetof(BrPathSeg, pt) - __builtin_offsetof(BrPathSeg, before) == sizeof(BrPathPt),
               "pt[-1] is before");

/* the loaded track */
typedef struct BrTrackHdr {
    unsigned int size;          /* the file's packed size (BrTrackLoad) */
    unsigned int hdrSize;       /* 0x230 */
    BrTrackTri *tris;
    BrVec3 *verts;
    int nTex;
    BrTexSlot *tex;
    unsigned short *gridEntries;    /* the cells' entries, read through queues (D_80025C20) */
    unsigned short *gridCells;      /* [64 * 64 + 1]: each cell's first entry (D_80025C24) */
    float x28, x2c;
    float fogLo;                /* the altitude the fog starts at (also x38) */
    float fogHi;                /* and where it is full (also x3c) */
    BrVec3 start;               /* the start line */
    float heading;              /* facing along the start line */
    unsigned int x50;           /* the sky's display list (an address in the file) */
    char **names;               /* the objects' names, 0 when the build has none */
    BrTrackObj *objs;
    int nObjs;
    unsigned short *queue2;     /* the second queue table (D_80025C68) */
    unsigned short *x6c;        /* (D_80025C6C) */
    BrPathSeg *path;            /* the path's first segment (D_80025C70) */
    BrPathSeg **segs;           /* every segment */
    int nSegs;
    unsigned char fogColour[3]; /* the track's fog colour */
    BrVec3 *cams;               /* the camera checkpoints (D_80025C84) */
    int nCams;                  /* and their count (D_80025C88) */
    unsigned short *triggers;   /* zero-terminated trigger id lists */
    unsigned short *triTrigger; /* each triangle's list in triggers */
    unsigned char *surf;        /* per triangle: surface bits */
    BrGate gate[10];
    int nGates;
    BrSpecial specials[16];
    int nSpecials;
} BrTrackHdr;

extern unsigned char D_80025C00[];  /* the track file as unpacked (and, in the menus, the music) */
extern BrTrackHdr tgr_track;
void tgr_track_convert(void);       /* the native records from the file just unpacked */

#define BR_TRACK            (&tgr_track)
#define BR_TRACKOBJS()      (tgr_track.objs)
#endif
