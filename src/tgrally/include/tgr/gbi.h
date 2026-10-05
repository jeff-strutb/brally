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
#define G_SETTIMG           0xfd
#define G_SETTILE           0xf5
#define G_LOADBLOCK         0xf3
#define G_LOADTLUT          0xf0
#define G_SETTILESIZE       0xf2
#define G_TEXTURE           0xbb
#define G_RDPLOADSYNC       0xe6
#define G_RDPTILESYNC       0xe8
#define G_TX_LDBLK_MAX_TXL  2047
#ifndef MIN
#define MIN(a, b)           ((a) < (b) ? (a) : (b))
#endif
#define G_TEXRECT           0xe4
#define G_RDPHALF_1         0xb4
#define G_RDPHALF_2         0xb3
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
#define gDPSetTextureImage(pkt, f, s, w, i) gSetImage(pkt, G_SETTIMG, f, s, w, i)
#define gDPLoadSync(pkt)    gDPNoParam(pkt, G_RDPLOADSYNC)
#define gDPTileSync(pkt)    gDPNoParam(pkt, G_RDPTILESYNC)
#define gDPSetTile(pkt, fmt, siz, line, tmem, tile, palette, cmt, maskt, shiftt, cms, masks, shifts) \
{                                                                       \
    Gfx *_g = (Gfx *)(pkt);                                             \
                                                                        \
    _g->words.w0 = _SHIFTL(G_SETTILE, 24, 8) | _SHIFTL(fmt, 21, 3) |    \
                   _SHIFTL(siz, 19, 2) | _SHIFTL(line, 9, 9) |          \
                   _SHIFTL(tmem, 0, 9);                                 \
    _g->words.w1 = _SHIFTL(tile, 24, 3) | _SHIFTL(palette, 20, 4) |     \
                   _SHIFTL(cmt, 18, 2) | _SHIFTL(maskt, 14, 4) |        \
                   _SHIFTL(shiftt, 10, 4) | _SHIFTL(cms, 8, 2) |        \
                   _SHIFTL(masks, 4, 4) | _SHIFTL(shifts, 0, 4);        \
}
#define gDPLoadBlock(pkt, tile, uls, ult, lrs, dxt)                     \
{                                                                       \
    Gfx *_g = (Gfx *)(pkt);                                             \
                                                                        \
    _g->words.w0 = (_SHIFTL(G_LOADBLOCK, 24, 8) | _SHIFTL(uls, 12, 12) | \
                    _SHIFTL(ult, 0, 12));                               \
    _g->words.w1 = (_SHIFTL(tile, 24, 3) |                              \
                    _SHIFTL((MIN(lrs, G_TX_LDBLK_MAX_TXL)), 12, 12) |   \
                    _SHIFTL(dxt, 0, 12));                               \
}
#define gDPLoadTLUTCmd(pkt, tile, count)                                \
{                                                                       \
    Gfx *_g = (Gfx *)pkt;                                               \
                                                                        \
    _g->words.w0 = _SHIFTL(G_LOADTLUT, 24, 8);                          \
    _g->words.w1 = _SHIFTL((tile), 24, 3) | _SHIFTL((count), 14, 10);   \
}
#define gDPLoadTileGeneric(pkt, c, tile, uls, ult, lrs, lrt)            \
{                                                                       \
    Gfx *_g = (Gfx *)(pkt);                                             \
                                                                        \
    _g->words.w0 = _SHIFTL(c, 24, 8) | _SHIFTL(uls, 12, 12) |           \
                   _SHIFTL(ult, 0, 12);                                 \
    _g->words.w1 = _SHIFTL(tile, 24, 3) | _SHIFTL(lrs, 12, 12) |        \
                   _SHIFTL(lrt, 0, 12);                                 \
}
#define gDPSetTileSize(pkt, t, uls, ult, lrs, lrt)                      \
    gDPLoadTileGeneric(pkt, G_SETTILESIZE, t, uls, ult, lrs, lrt)
#define gSPTexture(pkt, s, t, level, tile, on)                          \
{                                                                       \
    Gfx *_g = (Gfx *)(pkt);                                             \
                                                                        \
    _g->words.w0 = (_SHIFTL(G_TEXTURE, 24, 8) | _SHIFTL((level), 11, 3) | \
                    _SHIFTL((tile), 8, 3) | _SHIFTL((on), 0, 8));       \
    _g->words.w1 = (_SHIFTL((s), 16, 16) | _SHIFTL((t), 0, 16));        \
}
#define gImmp1(pkt, c, p0)                                              \
{                                                                       \
    Gfx *_g = (Gfx *)(pkt);                                             \
                                                                        \
    _g->words.w0 = _SHIFTL(c, 24, 8);                                   \
    _g->words.w1 = (unsigned int)(p0);                                  \
}
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

#define G_SETGEOMETRYMODE   0xb7
#define G_CLEARGEOMETRYMODE 0xb6
#define G_POPMTX            0xbd
#define G_SETBLENDCOLOR     0xf9
#define G_MW_LIGHTCOL       0x0a
#define G_MV_LOOKATY        0x82
#define G_MV_LOOKATX        0x84
#define G_MDSFT_ALPHACOMPARE 0
#define G_MDSFT_ALPHADITHER 4
#define G_MDSFT_TEXTFILT    12
#define G_MDSFT_TEXTLUT     14
#define G_MDSFT_TEXTLOD     16
#define G_MDSFT_TEXTDETAIL  17

#define gSPSetGeometryMode(pkt, word)   gImmp1(pkt, G_SETGEOMETRYMODE, word)
#define gSPClearGeometryMode(pkt, word) gImmp1(pkt, G_CLEARGEOMETRYMODE, word)
#define gSPPopMatrix(pkt, n)            gImmp1(pkt, G_POPMTX, n)
#define gDPSetBlendColor(pkt, r, g, b, a) \
    gDPSetColor(pkt, G_SETBLENDCOLOR, (_SHIFTL(r, 24, 8) | _SHIFTL(g, 16, 8) | _SHIFTL(b, 8, 8) | _SHIFTL(a, 0, 8)))
/* a light's colour goes to both of its copies (0x20 bytes per light) */
#define gSPLightColor(pkt, n, col)                                      \
{                                                                       \
    gMoveWd(pkt, G_MW_LIGHTCOL, ((n) - 1) * 0x20, col);                 \
    gMoveWd(pkt, G_MW_LIGHTCOL, ((n) - 1) * 0x20 + 4, col);             \
}
#define gSPLookAtX(pkt, l)  gDma1p(pkt, G_MOVEMEM, l, 16, G_MV_LOOKATX)
#define gSPLookAtY(pkt, l)  gDma1p(pkt, G_MOVEMEM, l, 16, G_MV_LOOKATY)
#define gDPSetAlphaCompare(pkt, type) \
    gSPSetOtherMode(pkt, G_SETOTHERMODE_L, G_MDSFT_ALPHACOMPARE, 2, type)
#define gDPSetAlphaDither(pkt, mode) \
    gSPSetOtherMode(pkt, G_SETOTHERMODE_H, G_MDSFT_ALPHADITHER, 2, mode)
#define gDPSetTextureFilter(pkt, type) \
    gSPSetOtherMode(pkt, G_SETOTHERMODE_H, G_MDSFT_TEXTFILT, 2, type)
#define gDPSetTextureLUT(pkt, type) \
    gSPSetOtherMode(pkt, G_SETOTHERMODE_H, G_MDSFT_TEXTLUT, 2, type)
#define gDPSetTextureLOD(pkt, type) \
    gSPSetOtherMode(pkt, G_SETOTHERMODE_H, G_MDSFT_TEXTLOD, 1, type)
#define gDPSetTextureDetail(pkt, type) \
    gSPSetOtherMode(pkt, G_SETOTHERMODE_H, G_MDSFT_TEXTDETAIL, 2, type)

#define G_MW_NUMLIGHT       0x02
#define G_MV_L0             0x86
#define NUMLIGHTS(n)        (0x80000000 | (((n) + 1) * 32))
#define gSPNumLights(pkt, n) gMoveWd(pkt, G_MW_NUMLIGHT, 0, NUMLIGHTS(n))
#define gSPLight(pkt, l, n) gDma1p(pkt, G_MOVEMEM, l, 16, ((n) - 1) * 2 + G_MV_L0)

#define G_MW_FOG            0x08
#define G_SETFOGCOLOR       0xf8
#define gSPFogFactor(pkt, fm, fo) \
    gMoveWd(pkt, G_MW_FOG, 0, (_SHIFTL(fm, 16, 16) | _SHIFTL(fo, 0, 16)))
#define gSPFogPosition(pkt, min, max) \
    gSPFogFactor(pkt, (500 * 0x100) / ((max) - (min)), (500 - (min)) * 0x100 / ((max) - (min)))
#define gDPSetFogColor(pkt, r, g, b, a) \
    gDPSetColor(pkt, G_SETFOGCOLOR, (_SHIFTL(r, 24, 8) | _SHIFTL(g, 16, 8) | _SHIFTL(b, 8, 8) | _SHIFTL(a, 0, 8)))

#define G_MW_CLIP           0x04
#define G_SETENVCOLOR       0xfb
#define gSPClipRatio1(pkt)                                              \
{                                                                       \
    gMoveWd(pkt, G_MW_CLIP, 0x04, 1);                                   \
    gMoveWd(pkt, G_MW_CLIP, 0x0c, 1);                                   \
    gMoveWd(pkt, G_MW_CLIP, 0x14, 0xffff);                              \
    gMoveWd(pkt, G_MW_CLIP, 0x1c, 0xffff);                              \
}
#define gSPClipRatio(pkt, r)                                            \
{                                                                       \
    gMoveWd(pkt, G_MW_CLIP, 0x04, r);                                   \
    gMoveWd(pkt, G_MW_CLIP, 0x0c, r);                                   \
    gMoveWd(pkt, G_MW_CLIP, 0x14, (unsigned short)-(r));                \
    gMoveWd(pkt, G_MW_CLIP, 0x1c, (unsigned short)-(r));                \
}
#define gDPSetEnvColor(pkt, r, g, b, a) \
    gDPSetColor(pkt, G_SETENVCOLOR, (_SHIFTL(r, 24, 8) | _SHIFTL(g, 16, 8) | _SHIFTL(b, 8, 8) | _SHIFTL(a, 0, 8)))

#define G_LOADTILE          0xf4
#define G_IM_FMT_IA         3
#define G_IM_SIZ_8b         1
#define G_MDSFT_TEXTPERSP   19
#define G_TP_NONE           (0 << G_MDSFT_TEXTPERSP)
#define G_TP_PERSP          (1 << G_MDSFT_TEXTPERSP)
#define G_CYC_2CYCLE        (1 << G_MDSFT_CYCLETYPE)
#define G_RM_PASS           0x0c080000
#define G_RM_XLU_SURF2      0x00104240
#ifndef MAX
#define MAX(a, b)           ((a) > (b) ? (a) : (b))
#endif
#define gDPSetTexturePersp(pkt, type) \
    gSPSetOtherMode(pkt, G_SETOTHERMODE_H, G_MDSFT_TEXTPERSP, 1, type)
#define gDPLoadTile(pkt, t, uls, ult, lrs, lrt) \
    gDPLoadTileGeneric(pkt, G_LOADTILE, t, uls, ult, lrs, lrt)
#define gSPTextureRectangle(pkt, xl, yl, xh, yh, tile, s, t, dsdx, dtdy) \
{                                                                       \
    Gfx *_g = (Gfx *)(pkt);                                             \
                                                                        \
    _g->words.w0 = (_SHIFTL(G_TEXRECT, 24, 8) | _SHIFTL(xh, 12, 12) |   \
                    _SHIFTL(yh, 0, 12));                                \
    _g->words.w1 = (_SHIFTL(tile, 24, 3) | _SHIFTL(xl, 12, 12) |        \
                    _SHIFTL(yl, 0, 12));                                \
    gImmp1(pkt, G_RDPHALF_1, (_SHIFTL(s, 16, 16) | _SHIFTL(t, 0, 16))); \
    gImmp1(pkt, G_RDPHALF_2, (_SHIFTL(dsdx, 16, 16) | _SHIFTL(dtdy, 0, 16))); \
}
/* the rectangle clamped at the top-left edges, its texture coordinates
 * advanced by what the clamp cut off */
#define gSPScisTextureRectangle(pkt, xl, yl, xh, yh, tile, s, t, dsdx, dtdy) \
{                                                                       \
    Gfx *_g = (Gfx *)(pkt);                                             \
                                                                        \
    _g->words.w0 = (_SHIFTL(G_TEXRECT, 24, 8) |                         \
                    _SHIFTL(MAX((s16)(xh), 0), 12, 12) |                \
                    _SHIFTL(MAX((s16)(yh), 0), 0, 12));                 \
    _g->words.w1 = (_SHIFTL((tile), 24, 3) |                            \
                    _SHIFTL(MAX((s16)(xl), 0), 12, 12) |                \
                    _SHIFTL(MAX((s16)(yl), 0), 0, 12));                 \
    gImmp1(pkt, G_RDPHALF_1,                                            \
           (_SHIFTL(((s) - (((s16)(xl) < 0) ?                           \
                            (((s16)(dsdx) < 0) ?                        \
                             (MAX((((s16)(xl) * (s16)(dsdx)) >> 7), 0)) : \
                             (MIN((((s16)(xl) * (s16)(dsdx)) >> 7), 0))) : 0)), \
                    16, 16) |                                           \
            _SHIFTL(((t) - (((yl) < 0) ?                                \
                            (((s16)(dtdy) < 0) ?                        \
                             (MAX((((s16)(yl) * (s16)(dtdy)) >> 7), 0)) : \
                             (MIN((((s16)(yl) * (s16)(dtdy)) >> 7), 0))) : 0)), \
                    0, 16)));                                           \
    gImmp1(pkt, G_RDPHALF_2, (_SHIFTL((dsdx), 16, 16) |                 \
                              _SHIFTL((dtdy), 0, 16)));                 \
}

#endif
