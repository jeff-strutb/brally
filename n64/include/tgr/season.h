/* season.h -- the per-player season record (0x128 bytes), two of them at
 * 0x80324300.  Field names are given as they are understood; `xNN` fields
 * are known only by their offset so far.
 */
#ifndef TGR_SEASON_H
#define TGR_SEASON_H

typedef struct BrSeason {
    int state;                  /* 0x00 */
    unsigned char round;        /* 0x04 */
    unsigned char race;         /* 0x05 */
    char pad06[0x18];
    short points[55];           /* 0x1E  points per round */
    float x8c[16];              /* 0x8C  one per car */
    unsigned short unlocked;    /* 0xCC  bit n: car n may be picked */
    unsigned short xce;         /* 0xCE */
    unsigned short xd0;         /* 0xD0 */
    short padd2;
    int xd4;                    /* 0xD4 */
    int xd8;                    /* 0xD8 */
    int xdc;                    /* 0xDC */
    int xe0;                    /* 0xE0 */
    int xe4;                    /* 0xE4 */
    float xe8[16];              /* 0xE8  one per car */
} BrSeason;

extern BrSeason D_80324300[2];

#endif
