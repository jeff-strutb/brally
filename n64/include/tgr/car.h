/* car.h -- the car record (0x2090 bytes), four of them at 0x8031B760.
 * Only the fields a matched function touches are named; the rest is padding
 * sized by offset.
 */
#ifndef TGR_CAR_H
#define TGR_CAR_H

#include "tgr/season.h"
#include "tgr/vec.h"

typedef struct BrCar {
    char pad000[0x140];
    int slot;                   /* 0x140  index in the car array */
    char pad144[0x1CC - 0x144];
    BrVec3 pos1cc;              /* 0x1CC  position (four copies set together) */
    char pad1d8[0x268 - 0x1D8];
    BrVec3 pos268;              /* 0x268 */
    char pad274[0x2AC - 0x274];
    BrVec3 pos2ac;              /* 0x2AC */
    char pad2b8[0xE58 - 0x2B8];
    int xe58;                   /* 0xE58 */
    BrSeason *season;           /* 0xE5C  the player's season, 0 for others */
    char pade60[0xED8 - 0xE60];
    int xed8;                   /* 0xED8 */
    char padedc[0xFD8 - 0xEDC];
    BrVec3 posfd8;              /* 0xFD8 */
    char padfe4[0x1D88 - 0xFE4];
    int mtx[16];                /* 0x1D88 */
    char pad1dc8[0x205C - 0x1DC8];
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

#endif
