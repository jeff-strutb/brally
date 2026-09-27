/* gbi.h -- the display-list command words the game builds (F3DEX 1.x
 * encodings, as in libultra's gbi.h for the fast3D family), only the
 * macros matched code uses.
 */
#ifndef TGR_GBI_H
#define TGR_GBI_H

typedef struct {
    unsigned int w0;
    unsigned int w1;
} Gwords;

typedef union {
    Gwords words;
    long long force_structure_alignment;
} Gfx;

typedef struct { int m[16]; } Mtx;

#define G_MTX               0x01
#define G_DL                0x06
#define G_MOVEWORD          0xbc

#define G_MTX_NOPUSH        0x00
#define G_MTX_PUSH          0x04
#define G_MTX_MUL           0x00
#define G_MTX_LOAD          0x02
#define G_MTX_MODELVIEW     0x00
#define G_MTX_PROJECTION    0x01

#define G_DL_PUSH           0x00
#define G_DL_NOPUSH         0x01

#define G_MW_PERSPNORM      0x0e

#define _SHIFTL(v, s, w) \
    ((unsigned int)(((unsigned int)(v) & ((0x01 << (w)) - 1)) << (s)))

#define gDma1p(pkt, c, s, l, p)                                         \
{                                                                       \
    Gfx *_g = (Gfx *)(pkt);                                             \
                                                                        \
    _g->words.w0 = (_SHIFTL((c), 24, 8) | _SHIFTL((p), 16, 8) |         \
                    _SHIFTL((l), 0, 16));                               \
    _g->words.w1 = (unsigned int)(s);                                   \
}

#define gMoveWd(pkt, index, offset, data)                               \
{                                                                       \
    Gfx *_g = (Gfx *)(pkt);                                             \
                                                                        \
    _g->words.w0 = (_SHIFTL(G_MOVEWORD, 24, 8) | _SHIFTL((offset), 8, 16) | \
                    _SHIFTL((index), 0, 8));                            \
    _g->words.w1 = (unsigned int)(data);                                \
}

#define gSPMatrix(pkt, m, p)        gDma1p(pkt, G_MTX, m, sizeof(Mtx), p)
#define gSPDisplayList(pkt, dl)     gDma1p(pkt, G_DL, dl, 0, G_DL_PUSH)
#define gSPPerspNormalize(pkt, s)   gMoveWd(pkt, G_MW_PERSPNORM, 0, (s))

#endif
