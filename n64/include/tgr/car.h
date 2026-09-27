/* car.h -- the car record (0x2090 bytes), four of them at 0x8031B760.
 * Only the fields a matched function touches are named; the rest is padding
 * sized by offset.
 */
#ifndef TGR_CAR_H
#define TGR_CAR_H

#include "tgr/season.h"

typedef struct BrCar {
    char pad000[0x140];
    int slot;                   /* 0x140  index in the car array */
    char pad144[0xE58 - 0x144];
    int xe58;                   /* 0xE58 */
    BrSeason *season;           /* 0xE5C  the player's season, 0 for others */
    char pade60[0xED8 - 0xE60];
    int xed8;                   /* 0xED8 */
    char padedc[0x1D88 - 0xEDC];
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
