/* car.h -- the car record (0x2090 bytes), four of them at 0x8031B760.
 * Only the fields a matched function touches are named; the rest is padding
 * sized by offset.
 */
#ifndef TGR_CAR_H
#define TGR_CAR_H

#include "tgr/season.h"
#include "tgr/vec.h"

/* A rigid-body state (0x44 bytes): the car keeps three (current, and two
 * copies the integrator steps between). */
typedef struct BrRbState {
    BrVec3 pos;                 /* 0x00 */
    BrVec3 vel;                 /* 0x0C */
    float q[4];                 /* 0x18  orientation */
    BrVec3 angVel;              /* 0x28 */
    char pad34[0x10];
} BrRbState;

/* A car's camera: its matrix and field of view (0x44 bytes). */
typedef struct BrCarCam {
    float mtx[4][4];
    float fov;                  /* 0x40  radians */
} BrCarCam;

/* A car kind's handling numbers (0x4C bytes each), a table at 0x8028B330. */
typedef struct BrCarKindParams {
    char x00[0x1c];             /* copied whole to the car at 0xDF8 */
    float x1c[5];               /* to 0xE14.. */
    int x30[2];                 /* to 0xE28.. */
    float x38[4];               /* to 0x324.. */
    int x48;                    /* to 0xE34 */
} BrCarKindParams;
extern BrCarKindParams D_8028B330[];

/* The record a car's +0xED0 points at; only its flags are known. */
typedef struct BrCarLink {
    char pad00[0x68];
    unsigned int flags;         /* 0x68  bits 0-1: the car is out of the race */
} BrCarLink;

typedef struct BrCar {
    float mtx0[4][4];           /* 0x000  body matrix */
    float wheelMtx[4][4][4];    /* 0x040  one per wheel: the body's rotation, the wheel's position */
    int slot;                   /* 0x140  index in the car array */
    char pad144[0x1C0 - 0x144];
    BrRbState st;               /* 0x1C0  the body's state */
    float stMtx[4][4];          /* 0x204  st's orientation as a matrix */
    char pad244[0x25C - 0x244];
    BrRbState stA;              /* 0x25C */
    BrRbState stB;              /* 0x2A0 */
    char pad2e4[0x324 - 0x2E4];
    float x324[4];              /* 0x324  from the kind table */
    BrVec3 x334;                /* 0x334  where the HUD arrow points */
    char pad340[0x344 - 0x340];
    unsigned char x344;         /* 0x344  a pending HUD arrow (0 = none) */
    char pad345[0xDF8 - 0x345];
    char xdf8[0x1c];            /* 0xDF8  from the kind table */
    float xe14[5];              /* 0xE14  from the kind table */
    int xe28[2];                /* 0xE28  from the kind table */
    int xe30;                   /* 0xE30 */
    int xe34;                   /* 0xE34  from the kind table */
    char pade38[0xE58 - 0xE38];
    int xe58;                   /* 0xE58 */
    BrSeason *season;           /* 0xE5C  the player's season, 0 for others */
    int xe60;                   /* 0xE60  from the season (or the ghost's header) */
    int xe64;                   /* 0xE64 */
    int xe68;                   /* 0xE68 */
    int xe6c;                   /* 0xE6C */
    char pade70[0xED0 - 0xE70];
    struct BrCarLink *link;     /* 0xED0 */
    int xed4;                   /* 0xED4  a countdown, one per frame */
    int xed8;                   /* 0xED8  the car's control function (camera step, AI) */
    char padedc[0xF48 - 0xEDC];
    int xf48;                   /* 0xF48  camera mode */
    int xf4c;                   /* 0xF4C  the camera keeps the car's up axis */
    BrVec3 posPrev;             /* 0xF50  last frame's position */
    int xf5c;                   /* 0xF5C */
    int xf60;                   /* 0xF60 */
    char padf64[0xF78 - 0xF64];
    int laps;                   /* 0xF78  laps completed */
    char padf7c[0xF80 - 0xF7C];
    float raceTime;             /* 0xF80  race clock, seconds */
    float lapTimes[5];          /* 0xF84  each lap's time */
    float xf98;                 /* 0xF98 */
    int xf9c;                   /* 0xF9C */
    float lapTime;              /* 0xFA0  current lap clock */
    float xfa4;                 /* 0xFA4  a countdown (mode 1 only) */
    char padfa8[0xFAC - 0xFA8];
    int xfac;                   /* 0xFAC */
    int msgA;                   /* 0xFB0  first message and its timer */
    float msgATime;             /* 0xFB4 */
    int msgB;                   /* 0xFB8  second message and its timer */
    float msgBTime;             /* 0xFBC */
    char xfc0[0x18];            /* 0xFC0  text for msgB (a formatted time) */
    BrVec3 velfd8;              /* 0xFD8  another velocity copy */
    char padfe4[0x1010 - 0xFE4];
    float x1010;                /* 0x1010  zeroed when the particle pool is reset */
    char pad1014[0x1D78 - 0x1014];
    BrVec3 pos1d78;             /* 0x1D78  another position copy */
    char pad1d84[0x1D88 - 0x1D84];
    int mtx[16];                /* 0x1D88 */
    char pad1dc8[0x1DCC - 0x1DC8];
    float heading;              /* 0x1DCC  of the camera, radians */
    char pad1dd0[0x1DE4 - 0x1DD0];
    float fog;                  /* 0x1DE4  fog amount at the car */
    BrCarCam *cam;              /* 0x1DE8  the camera in use */
    BrCarCam *cam2;             /* 0x1DEC  the camera it switches back to */
    BrCarCam cams[4];           /* 0x1DF0 */
    char pad1f00[0x1F44 - 0x1F00];
    BrCarCam cam4;              /* 0x1F44 */
    char pad1f88[0x1F90 - 0x1F88];
    float x1f90;                /* 0x1F90 */
    BrVec3 camTarget;           /* 0x1F94  where the camera looks */
    BrVec3 camPosA;             /* 0x1FA0  copies of the chase camera's start position */
    float x1fac;                /* 0x1FAC */
    char pad1fb0[0x1FB4 - 0x1FB0];
    BrVec3 camPosB;             /* 0x1FB4 */
    unsigned short x1fc0[32];   /* 0x1FC0  trigger ids the car has passed */
    int x2000;                  /* 0x2000  entries in x1fc0 */
    char pad2004[0x2058 - 0x2004];
    int x2058;                  /* 0x2058  the kind it was given (copied to kind) */
    int kind;                   /* 0x205C */
    unsigned char colour[4];    /* 0x2060  body colour r, g, b and a fourth byte */
    float x2064;                /* 0x2064 */
    int x2068;                  /* 0x2068 */
    char pad206c[0x2070 - 0x206C];
    unsigned char cellX;        /* 0x2070  the 32-unit track grid cell it is in (0..63) */
    unsigned char cellY;        /* 0x2071 */
    char pad2072[0x2074 - 0x2072];
    unsigned int *pad;          /* 0x2074  the slot's pad record */
    char *model;                /* 0x2078  the slot's model buffer */
    char pad207c[0x2090 - 0x207C];
} BrCar;

extern BrCar D_8031B760[4];

/* A car's loaded model (the head of its model buffer). */
typedef struct BrCarModelPart {
    void *a;
    unsigned short *b;          /* 0x04  palette (RGBA5551) for a paletted texture */
    char pad08[0x18];
    unsigned char fmt;          /* 0x20  low nibble 1: paletted */
    char pad21[3];
} BrCarModelPart;
typedef struct BrCarModel {
    char pad00[0x10];
    int nParts;                 /* 0x10 */
    BrCarModelPart *parts;      /* 0x14 */
    unsigned int *dl[3][10];    /* 0x18 */
    void *x90;
    void *x94;
    char pad98[0xbc - 0x98];
    unsigned int *dl2[3][3];    /* 0xBC  wheel display lists (0 when the model has none) */
    float wheel[4][3];          /* 0xE0  wheel positions in the body's frame */
    unsigned char decalPart[2]; /* 0x110  the parts carrying decals 0 and 1 (indexed up to 2) */
    unsigned char paintPart;    /* 0x112  the part carrying the paint texture */
    char pad113[0x11c - 0x113];
    void **x11c;                /* 0x11C */
} BrCarModel;

/* A car's model file (0x60 bytes each), a table at 0x8028AE0C. */
typedef struct BrCarModelRec {
    char pad00[0x14];
    int rom;                    /* 0x14  ROM address of the model file */
    int present;                /* 0x18  non-zero when the car is in this build */
    unsigned int size;          /* 0x1C  bytes, set when it is loaded */
    char pad20[0x58 - 0x20];
    float len;                  /* 0x58  body length, 1/256 units */
    float wid;                  /* 0x5C  body half-width */
} BrCarModelRec;

extern BrCarModelRec D_8028AE0C[];

#endif
