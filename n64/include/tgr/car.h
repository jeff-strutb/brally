/* car.h -- the car record (0x2090 bytes), four of them at 0x8031B760.
 * Only the fields a matched function touches are named; the rest is padding
 * sized by offset.
 */
#ifndef TGR_CAR_H
#define TGR_CAR_H

#include "tgr/season.h"
#include "tgr/vec.h"

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

typedef struct BrCar {
    float mtx0[4][4];           /* 0x000  body matrix */
    float wheelMtx[4][4][4];    /* 0x040  one per wheel: the body's rotation, the wheel's position */
    int slot;                   /* 0x140  index in the car array */
    char pad144[0x1CC - 0x144];
    BrVec3 pos1cc;              /* 0x1CC  position (four copies set together) */
    char pad1d8[0x268 - 0x1D8];
    BrVec3 pos268;              /* 0x268 */
    char pad274[0x2AC - 0x274];
    BrVec3 pos2ac;              /* 0x2AC */
    char pad2b8[0x324 - 0x2B8];
    float x324[4];              /* 0x324  from the kind table */
    char pad334[0xDF8 - 0x334];
    char xdf8[0x1c];            /* 0xDF8  from the kind table */
    float xe14[5];              /* 0xE14  from the kind table */
    int xe28[2];                /* 0xE28  from the kind table */
    int xe30;                   /* 0xE30 */
    int xe34;                   /* 0xE34  from the kind table */
    char pade38[0xE58 - 0xE38];
    int xe58;                   /* 0xE58 */
    BrSeason *season;           /* 0xE5C  the player's season, 0 for others */
    char pade60[0xE6C - 0xE60];
    int xe6c;                   /* 0xE6C */
    char pade70[0xED8 - 0xE70];
    int xed8;                   /* 0xED8 */
    char padedc[0xF5C - 0xEDC];
    int xf5c;                   /* 0xF5C */
    int xf60;                   /* 0xF60 */
    char padf64[0xFD8 - 0xF64];
    BrVec3 posfd8;              /* 0xFD8 */
    char padfe4[0x1D88 - 0xFE4];
    int mtx[16];                /* 0x1D88 */
    char pad1dc8[0x1DE8 - 0x1DC8];
    BrCarCam *cam;              /* 0x1DE8  the camera in use */
    int x1dec;                  /* 0x1DEC */
    BrCarCam cams[4];           /* 0x1DF0 */
    char pad1f00[0x1F44 - 0x1F00];
    BrCarCam cam4;              /* 0x1F44 */
    char pad1f88[0x205C - 0x1F88];
    int kind;                   /* 0x205C */
    unsigned char colour[4];    /* 0x2060  body colour r, g, b and a fourth byte */
    char pad2064[4];
    int x2068;                  /* 0x2068 */
    char pad206c[0x2074 - 0x206C];
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
    char pad110[2];
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
