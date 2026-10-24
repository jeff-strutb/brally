/* tri.h -- a cached collision triangle (0x20 bytes on the N64): its plane,
 * its three corners (vertices in the track's data) and its triangle; the
 * cache holds 150 per grid slot at 0x80379F80.  One definition for every
 * part of the game; where the parts named a field differently, the names
 * share it.
 */
#ifndef TGR_TRI_H
#define TGR_TRI_H

#include "tgr/vec.h"

typedef struct BrTri {
    union {                     /* 0x00  normal and distance */
        float plane[4];
        struct {
            BrVec3 n;
            float d;
        };
        float nf[3];
    };
    union {                     /* 0x10  its corners */
        float *v[3];
        struct {
            void *v0, *v1, *v2;
        };
    };
    union {
        int x1c;
        struct {
            short id;           /* 0x1C  its triangle */
            unsigned char kind; /* 0x1E  its surface */
            char pad1f;
        };
        struct {
            short tri;
            short x1e;
        };
    };
} BrTri;

extern BrTri D_80379F80[4][150];        /* each grid slot's triangles */

/* A contact-list node (8 bytes on the N64): a triangle a body touches. */
typedef struct BrCrNode {
    BrTri *plane;
    struct BrCrNode *next;
} BrCrNode;

#endif
