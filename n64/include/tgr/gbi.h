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

typedef struct {
    short ob[3];
    unsigned short flag;
    short tc[2];
    unsigned char cn[4];
} Vtx_t;

typedef union {
    Vtx_t v;
    long long force_structure_alignment;
} Vtx;

typedef struct {
    short vscale[4];
    short vtrans[4];
} Vp_t;

typedef union {
    Vp_t vp;
    long long force_structure_alignment;
} Vp;

#define G_MTX               0x01
#define G_MOVEMEM           0x03
#define G_SETOTHERMODE_L    0xb9
#define G_SETOTHERMODE_H    0xba
#define G_RDPPIPESYNC       0xe7
#define G_FILLRECT          0xf6
#define G_SETPRIMCOLOR      0xfa
#define G_SETFILLCOLOR      0xf7
#define G_SETCIMG           0xff
#define G_IM_FMT_RGBA       0
#define G_IM_SIZ_16b        2
#define G_MAXFBZ            0x3fff
#define GPACK_ZDZ(z, dz)    ((z) << 2 | (dz))
#define G_SETCOMBINE        0xfc
#define G_SETSCISSOR        0xed
#define G_SC_NON_INTERLACE  0
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
#define G_MV_VIEWPORT       0x80

#define G_MDSFT_RENDERMODE  3
#define G_MDSFT_RGBDITHER   6
#define G_MDSFT_CYCLETYPE   20

#define G_CYC_1CYCLE        (0 << G_MDSFT_CYCLETYPE)
#define G_CYC_FILL          (3 << G_MDSFT_CYCLETYPE)
#define G_CD_DISABLE        (3 << G_MDSFT_RGBDITHER)

/* render modes used so far, as libultra composes them */
#define G_RM_CLD_SURF       0x00404340
#define G_RM_CLD_SURF2      0x00104340
#define G_RM_OPA_SURF       0x0c084000
#define G_RM_OPA_SURF2      0x03024000

#define GPACK_RGBA5551(r, g, b, a) ((((r) << 8) & 0xf800) | (((g) << 3) & 0x7c0) | \
                                    (((b) >> 2) & 0x3e) | ((a) & 0x1))

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

#define gDPNoParam(pkt, cmd)                                            \
{                                                                       \
    Gfx *_g = (Gfx *)(pkt);                                             \
                                                                        \
    _g->words.w0 = _SHIFTL(cmd, 24, 8);                                 \
    _g->words.w1 = 0;                                                   \
}

#define gSPSetOtherMode(pkt, cmd, sft, len, data)                       \
{                                                                       \
    Gfx *_g = (Gfx *)(pkt);                                             \
                                                                        \
    _g->words.w0 = (_SHIFTL(cmd, 24, 8) | _SHIFTL(sft, 8, 8) |          \
                    _SHIFTL(len, 0, 8));                                \
    _g->words.w1 = (unsigned int)(data);                                \
}

#define gDPSetCombine(pkt, muxs0, muxs1)                                \
{                                                                       \
    Gfx *_g = (Gfx *)(pkt);                                             \
                                                                        \
    _g->words.w0 = _SHIFTL(G_SETCOMBINE, 24, 8) | _SHIFTL(muxs0, 0, 24); \
    _g->words.w1 = (unsigned int)(muxs1);                               \
}

#define gDPSetColor(pkt, c, d)                                          \
{                                                                       \
    Gfx *_g = (Gfx *)(pkt);                                             \
                                                                        \
    _g->words.w0 = _SHIFTL(c, 24, 8);                                   \
    _g->words.w1 = (unsigned int)(d);                                   \
}

#define gDPSetPrimColor(pkt, m, l, r, g, b, a)                          \
{                                                                       \
    Gfx *_g = (Gfx *)(pkt);                                             \
                                                                        \
    _g->words.w0 = (_SHIFTL(G_SETPRIMCOLOR, 24, 8) | _SHIFTL(m, 8, 8) | \
                    _SHIFTL(l, 0, 8));                                  \
    _g->words.w1 = (_SHIFTL(r, 24, 8) | _SHIFTL(g, 16, 8) |             \
                    _SHIFTL(b, 8, 8) | _SHIFTL(a, 0, 8));               \
}

#define gDPFillRectangle(pkt, ulx, uly, lrx, lry)                       \
{                                                                       \
    Gfx *_g = (Gfx *)(pkt);                                             \
                                                                        \
    _g->words.w0 = (_SHIFTL(G_FILLRECT, 24, 8) | _SHIFTL((lrx), 14, 10) | \
                    _SHIFTL((lry), 2, 10));                             \
    _g->words.w1 = (_SHIFTL((ulx), 14, 10) | _SHIFTL((uly), 2, 10));    \
}

#define gDPSetScissor(pkt, mode, ulx, uly, lrx, lry)                   \
{                                                                       \
    Gfx *_g = (Gfx *)(pkt);                                             \
                                                                        \
    _g->words.w0 = _SHIFTL(G_SETSCISSOR, 24, 8) |                       \
                   _SHIFTL((int)((float)(ulx) * 4.0f), 12, 12) |        \
                   _SHIFTL((int)((float)(uly) * 4.0f), 0, 12);          \
    _g->words.w1 = _SHIFTL(mode, 24, 2) |                               \
                   _SHIFTL((int)((float)(lrx) * 4.0f), 12, 12) |        \
                   _SHIFTL((int)((float)(lry) * 4.0f), 0, 12);          \
}

#define gDPPipeSync(pkt)            gDPNoParam(pkt, G_RDPPIPESYNC)
#define gDPSetFillColor(pkt, d)     gDPSetColor(pkt, G_SETFILLCOLOR, (d))
#define gSetImage(pkt, cmd, fmt, siz, width, i)                         \
{                                                                       \
    Gfx *_g = (Gfx *)(pkt);                                             \
                                                                        \
    _g->words.w0 = _SHIFTL(cmd, 24, 8) | _SHIFTL(fmt, 21, 3) |          \
                   _SHIFTL(siz, 19, 2) | _SHIFTL((width)-1, 0, 12);     \
    _g->words.w1 = (unsigned int)(i);                                   \
}
#define gDPSetColorImage(pkt, f, s, w, i)   gSetImage(pkt, G_SETCIMG, f, s, w, i)
#define gDPSetCycleType(pkt, type)  \
    gSPSetOtherMode(pkt, G_SETOTHERMODE_H, G_MDSFT_CYCLETYPE, 2, type)
#define gDPSetColorDither(pkt, mode) \
    gSPSetOtherMode(pkt, G_SETOTHERMODE_H, G_MDSFT_RGBDITHER, 2, mode)
#define gDPSetRenderMode(pkt, c0, c1) \
    gSPSetOtherMode(pkt, G_SETOTHERMODE_L, G_MDSFT_RENDERMODE, 29, (c0) | (c1))

/* one command given as its two words (a machine-converted draft; named
 * macros replace these as each command is identified) */
#define gRaw(pkt, a, b)                                                 \
{                                                                       \
    Gfx *_g = (Gfx *)(pkt);                                             \
                                                                        \
    _g->words.w0 = (unsigned int)(a);                                   \
    _g->words.w1 = (unsigned int)(b);                                   \
}

#define gSPMatrix(pkt, m, p)        gDma1p(pkt, G_MTX, m, sizeof(Mtx), p)
#define gSPViewport(pkt, v)         gDma1p((pkt), G_MOVEMEM, (v), sizeof(Vp), G_MV_VIEWPORT)
#define gSPDisplayList(pkt, dl)     gDma1p(pkt, G_DL, dl, 0, G_DL_PUSH)
#define gSPPerspNormalize(pkt, s)   gMoveWd(pkt, G_MW_PERSPNORM, 0, (s))

#endif
