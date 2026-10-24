/* ent.h -- a race entity (0x78 bytes on the N64), an array at 0x803239A0:
 * the cars, then the rest.  One definition for every part of the game;
 * where the parts named a field differently, the names share it.
 */
#ifndef TGR_ENT_H
#define TGR_ENT_H

#include "tgr/vec.h"

struct BrCar;
struct BrPathSeg;

typedef struct BrRaceEnt {
    BrVec3 pos;                 /* 0x00  (an AI slot: its car's position when saved) */
    union {                     /* 0x0C */
        BrVec3 posPrev;
        BrVec3 prev;
    };
    BrVec3 vel;                 /* 0x18 */
    float x24;                  /* 0x24  the car's 0xFE4 */
    union {                     /* 0x28  where it is on its path (a ghost: its route) */
        struct BrPathSeg *seg;
        void *route;
    };
    int pt;                     /* 0x2C  and the point in it */
    union {                     /* 0x30 */
        float raceTime;
        float x30;
    };
    float x34;                  /* 0x34 */
    union {                     /* 0x38 */
        float lapTime;
        float x38;
    };
    union {                     /* 0x3C  from the track's time table */
        float x3c;
        float speed;
    };
    int laps;                   /* 0x40  times past gate 0 */
    union {                     /* 0x44  laps completed on the route */
        int x44;
        int laps2;
    };
    union {                     /* 0x48  gates passed */
        int x48;
        int gates;
    };
    union {                     /* 0x4C */
        int x4c;
        int gates2;
    };
    union {                     /* 0x50  distance run (for an entity that is not a car) */
        float progress;
        float dist;
    };
    union {                     /* 0x54  its race position, likewise */
        int rank;
        int x54;
    };
    char pad58[4];
    union {                     /* 0x5C  the body colour of the slot's car */
        unsigned char colour[3];
        unsigned char rgb[3];
    };
    char pad5f;
    union {                     /* 0x60  its car, 0 for an entity that is not a car */
        struct BrCar *car;
        char *x60;
    };
    union {                     /* 0x64  the car slot it stands for */
        int x64;
        int index;
    };
    unsigned int flags;         /* 0x68  bit 0: held at the start, bit 1: finished */
    char pad6c[0x74 - 0x6C];
    union {                     /* 0x74  which half of the field (0 or 1) */
        int group;
        int x74;
    };
} BrRaceEnt;

extern BrRaceEnt D_803239A0[];

#endif
