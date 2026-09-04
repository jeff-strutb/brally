/* br_gbitexscan.c -- the display-list texture-scan pre-pass.
 *
 * Filed out of slice2_16.c (an address batch, not a module) as each function
 * was matched.  The rest of the BrGbiTexScan family is still in slice2_16.c
 * and follows here as it matches; the tile setters, the texel-per-word
 * arithmetic and the two texture constructors arrived in the 2026-09-03
 * refile.
 *
 * MATCHING NOTE: the original keeps this pass's state in fixed globals and
 * these functions take no state pointer.  The port threads a BrGbiTexScan
 * through instead so the state is reachable without absolute addresses, which
 * is why every function here carries both arms.
 */
#ifdef BR_MATCHING_BUILD
/* Header still has the port (pSt, pCmd) shape; orig takes pCmd only. */
#define BrGbiTexScanOtherModeH   BrGbiTexScanOtherModeH_port
#define BrGbiTexScanOtherModeH0E BrGbiTexScanOtherModeH0E_port
/* Same for the four that arrived from slice2_16.c in the 2026-09-03 refile:
 * the header still carries the port's state-pointer shape. */
#define BrGbiTexCreate           BrGbiTexCreate_port
#define BrGbiSolidTexBuild       BrGbiSolidTexBuild_port
#endif
#include "slice2_16.h"
#ifdef BR_MATCHING_BUILD
#undef BrGbiTexScanOtherModeH
#undef BrGbiTexScanOtherModeH0E
#undef BrGbiTexCreate
#undef BrGbiSolidTexBuild
void BrGbiTexScanOtherModeH0E(const BrGfxWords *pCmd);
#endif
#include "br_dlshared.h"

#include <string.h>

/* 0x10029410 */
/* WHAT IT DOES: closes off a recognised texture-load run and replaces it
 * with a single command. The staged texture bytes are handed to the texture
 * cache; if the cache accepts them and gives back an id, the run's first
 * command is overwritten with a short "use texture id N, skip the next so-
 * many commands" instruction, so the several commands the N64 needed to load
 * a texture collapse into one on the PC. */
/* @d3donly 0x10029410 BrGbiTexScanFlush -- glide twin 0x10028B50 claimed by br_tex3d.c:br_tex3d_seam */
#ifdef BR_MATCHING_BUILD
void BrGbiTexScanFlush(BrGfxWords *pCmd)
{
    int id;

    if (g_brTexScanState == 0)
        return;
    if (g_brTexScanRunEnd == NULL)
        g_brTexScanRunEnd = pCmd;

    id = BrGbiCall10029470(g_brTexScanStage);
    if (id != -1) {
        BrGfxWords *pRun = g_brTexScanRunStart;
        pRun->w0 = ((uint32_t)id & 0x00FFFFFFu) | 0xDC000000u;
        pRun->w1 = (uint32_t)(int32_t)(g_brTexScanRunEnd - g_brTexScanRunStart);
    }
    g_brTexScanState = 0;
}
#else
void BrGbiTexScanFlush(BrGbiTexScan *pSt, BrGfxWords *pCmd)
{
    int id;

    if (pSt->state == 0)
        return;
    if (pSt->pRunEnd == NULL)
        pSt->pRunEnd = pCmd;

    id = BrGbiCall10029470(pSt->aStage);
    if (id != -1) {
        BrGfxWords *pRun = pSt->pRunStart;
        pRun->w0 = ((uint32_t)id & 0x00FFFFFFu) | 0xDC000000u;
        /* Length in 8-byte commands: the original does the pointer
         * subtraction and an arithmetic shift right by 3. */
        pRun->w1 = (uint32_t)(int32_t)(pSt->pRunEnd - pSt->pRunStart);
    }
    pSt->state = 0;
}
#endif

/* 0x10029E60 */
/* WHAT IT DOES: during the pre-pass that hunts for texture loads, notes
 * where the current run of commands ended, the first time anything ends it.
 * Later ends are ignored so the run keeps its original extent. */
/* @d3donly 0x10029E60 BrGbiTexScanMark -- glide twin 0x100293D0 claimed by br_tex3d.c:br_tex3d_end */
#ifdef BR_MATCHING_BUILD
/* @n64 0x8026C040 located */
void BrGbiTexScanMark(BrGfxWords *pCmd)
{
    if (g_brTexScanRunEnd == NULL)
        g_brTexScanRunEnd = pCmd;
}
#else
void BrGbiTexScanMark(BrGbiTexScan *pSt, BrGfxWords *pCmd)
{
    if (pSt->pRunEnd == NULL)
        pSt->pRunEnd = pCmd;
}
#endif

/* 0x10029F80  G_RDPLOADSYNC */
/* WHAT IT DOES: during the texture-load hunt, advances the state machine
 * when the expected wait-for-load command shows up after an image address
 * was set. */
/* @implements 0x10029F80 d3d BrGbiTexScanLoadSync */
#ifdef BR_MATCHING_BUILD
void BrGbiTexScanLoadSync(void)
{
    if (g_brTexScanState == 1)
        g_brTexScanState = 2;
}
#else
void BrGbiTexScanLoadSync(BrGbiTexScan *pSt)
{
    if (pSt->state == 1)
        pSt->state = 2;
}
#endif

/* 0x1002A000  G_RDPPIPESYNC */
/* WHAT IT DOES: during the texture-load hunt, advances the state machine
 * when the expected pipeline-wait command shows up after a palette load. */
/* @implements 0x1002A000 d3d BrGbiTexScanPipeSync */
#ifdef BR_MATCHING_BUILD
void BrGbiTexScanPipeSync(void)
{
    /* The nested test is the original's: it checks the state is non-zero
     * before comparing it to 7, which is redundant but is what it emits. */
    if (g_brTexScanState != 0) {
        if (g_brTexScanState == 7)
            g_brTexScanState = 8;
    }
}
#else
void BrGbiTexScanPipeSync(BrGbiTexScan *pSt)
{
    if (pSt->state == 7)
        pSt->state = 8;
}
#endif

/* 0x1002A020  G_RDPTILESYNC */
/* WHAT IT DOES: during the texture-load hunt, advances the state machine
 * when the expected tile-wait command shows up after a pixel or palette
 * load. */
/* @implements 0x1002A020 d3d BrGbiTexScanTileSync */
/* @n64 0x8026B860 located */
#ifdef BR_MATCHING_BUILD
void BrGbiTexScanTileSync(void)
{
    if (g_brTexScanState == 3 || g_brTexScanState == 7)
        g_brTexScanState = 4;
}
#else
void BrGbiTexScanTileSync(BrGbiTexScan *pSt)
{
    if (pSt->state == 3 || pSt->state == 7)
        pSt->state = 4;
}
#endif

/* 0x1002A210  G_SETOTHERMODE_H */
/* WHAT IT DOES: during the texture-load hunt, sorts a render-mode change
 * into texture filtering vs one other on/off setting. */
/* @implements 0x1002A210 d3d BrGbiTexScanOtherModeH */
#ifdef BR_MATCHING_BUILD
extern int DAT_106b7ab0;   /* 0x1057544C / f5544C */
void BrGbiTexScanOtherModeH(const BrGfxWords *pCmd)
{
    uint32_t sel = pCmd->w0 & 0xFF00u;

    if (sel == 0x0E00u) {
        BrGbiTexScanOtherModeH0E(pCmd);
        return;
    }
    if (sel == 0x1100u)
        DAT_106b7ab0 = (pCmd->w1 == 0x40000u) ? 1 : 0;
}
#else
void BrGbiTexScanOtherModeH(BrGbiTexScan *pSt, const BrGfxWords *pCmd)
{
    uint32_t sel = pCmd->w0 & 0xFF00u;

    if (sel == 0x0E00u) {
        BrGbiTexScanOtherModeH0E(pSt, pCmd);
        return;
    }
    if (sel == 0x1100u)
        pSt->f5544C = (pCmd->w1 == 0x40000u) ? 1 : 0;
}
#endif

/* 0x1002A250  the 0x0E arm of the above. */
/* WHAT IT DOES: records which of two filtering choices is in force. A zero
 * is ignored rather than treated as a third choice. */
/* @implements 0x1002A250 d3d BrGbiTexScanOtherModeH0E */
#ifdef BR_MATCHING_BUILD
extern int DAT_10697a44;   /* 0x105553DC / f5553DC */
void BrGbiTexScanOtherModeH0E(const BrGfxWords *pCmd)
{
    uint32_t v = pCmd->w1;

    if (v == 0)
        return;
    if (v == 0x8000u) {
        DAT_10697a44 = 0;
        return;
    }
    if (v == 0xC000u)
        DAT_10697a44 = 3;
}
#else
void BrGbiTexScanOtherModeH0E(BrGbiTexScan *pSt, const BrGfxWords *pCmd)
{
    uint32_t v = pCmd->w1;

    if (v == 0)
        return;
    if (v == 0x8000u)
        pSt->f5553DC = 0;
    else if (v == 0xC000u)
        pSt->f5553DC = 3;
}
#endif

/* 0x1002A040  G_SETTILE */
/* WHAT IT DOES: during the texture-load hunt, records everything one of the
 * eight texture slots is being configured with -- pixel format and size,
 * where it sits in texture memory, and how it wraps, mirrors or clamps in
 * each direction -- and notes the highest slot used. */
/* @implements 0x1002A040 d3d BrGbiTexScanSetTile */
/* @implements 0x100295B0 glide BrGbiTexScanSetTile */
#ifdef BR_MATCHING_BUILD
void BrGbiTexScanSetTile(const BrGfxWords *pCmd)
{
    int32_t tile = (int32_t)((pCmd->w1 >> 24) & 7u);

    /* Re-read w0/w1 each field — orig keeps pCmd in ecx and reloads. */
    g_brTexScanTiles[tile].fmt     = (int32_t)((pCmd->w0 >> 21) & 7u);
    g_brTexScanTiles[tile].siz     = (int32_t)((pCmd->w0 >> 19) & 3u);
    g_brTexScanTiles[tile].line    = (int32_t)(((pCmd->w0 >> 9) & 0x1FFu) << 3);
    g_brTexScanTiles[tile].tmem    = (int32_t)(pCmd->w0 & 0x1FFu);
    g_brTexScanTiles[tile].mirrorS = (int32_t)((pCmd->w1 >> 8)  & 1u);
    g_brTexScanTiles[tile].clampS  = (int32_t)((pCmd->w1 >> 9)  & 1u);
    g_brTexScanTiles[tile].mirrorT = (int32_t)((pCmd->w1 >> 18) & 1u);
    g_brTexScanTiles[tile].clampT  = (int32_t)((pCmd->w1 >> 19) & 1u);
    g_brTexScanTiles[tile].maskS   = (int32_t)((pCmd->w1 >> 4)  & 0xFu);
    g_brTexScanTiles[tile].maskT   = (int32_t)((pCmd->w1 >> 14) & 0xFu);
    g_brTexScanTiles[tile].shiftS  = (int32_t)(pCmd->w1 & 0xFu);
    g_brTexScanTiles[tile].shiftT  = (int32_t)((pCmd->w1 >> 10) & 0xFu);

    if (g_brTexScanState == 3 || g_brTexScanState == 4 ||
        g_brTexScanState == 7 || tile > g_brTexScanMaxTile)
        g_brTexScanMaxTile = tile;
    g_brTexScanState = 5;
}
#else
void BrGbiTexScanSetTile(BrGbiTexScan *pSt, const BrGfxWords *pCmd)
{
    uint32_t   w0 = pCmd->w0;
    uint32_t   w1 = pCmd->w1;
    int32_t    tile = (int32_t)((w1 >> 24) & 7u);
    BrGbiTile *p = &pSt->aTiles[tile];

    p->fmt     = (int32_t)((w0 >> 21) & 7u);
    p->siz     = (int32_t)((w0 >> 19) & 3u);
    p->line    = (int32_t)(((w0 >> 9) & 0x1FFu) << 3);
    p->tmem    = (int32_t)(w0 & 0x1FFu);
    p->mirrorS = (int32_t)((w1 >> 8)  & 1u);
    p->clampS  = (int32_t)((w1 >> 9)  & 1u);
    p->mirrorT = (int32_t)((w1 >> 18) & 1u);
    p->clampT  = (int32_t)((w1 >> 19) & 1u);
    p->maskS   = (int32_t)((w1 >> 4)  & 0xFu);
    p->maskT   = (int32_t)((w1 >> 14) & 0xFu);
    p->shiftS  = (int32_t)(w1 & 0xFu);
    p->shiftT  = (int32_t)((w1 >> 10) & 0xFu);

    /* maxTile is forced (not maximised) when a load is in flight. */
    if (pSt->state == 3 || pSt->state == 4 || pSt->state == 7 ||
        tile > pSt->maxTile)
        pSt->maxTile = tile;
    pSt->state = 5;
}
#endif

/* 0x1002A140  G_SETTILESIZE */
/* WHAT IT DOES: during the texture-load hunt, records which rectangle of the
 * image one of the eight texture slots covers. */
/* @implements 0x1002A140 d3d BrGbiTexScanSetTileSize */
/* @implements 0x100296B0 glide BrGbiTexScanSetTileSize */
#ifdef BR_MATCHING_BUILD
void BrGbiTexScanSetTileSize(const BrGfxWords *pCmd)
{
    int32_t tile = (int32_t)((pCmd->w1 >> 24) & 7u);

    g_brTexScanTiles[tile].uls = (int32_t)((pCmd->w0 >> 12) & 0xFFFu);
    g_brTexScanTiles[tile].ult = (int32_t)(pCmd->w0 & 0xFFFu);
    g_brTexScanTiles[tile].lrs = (int32_t)((pCmd->w1 >> 12) & 0xFFFu);
    g_brTexScanTiles[tile].lrt = (int32_t)(pCmd->w1 & 0xFFFu);
    /* LAST in the source, though the bytes put it between the lrs and lrt
     * stores: VC5 sinks the global store one tile store, so writing it
     * where the bytes show it lands one store too EARLY.  The `volatile`
     * that used to pin it here did not help -- /O2 reorders the ordinary
     * stores around a volatile one just the same. */
    g_brTexScanState = 6;
}
#else
void BrGbiTexScanSetTileSize(BrGbiTexScan *pSt, const BrGfxWords *pCmd)
{
    uint32_t   w0 = pCmd->w0;
    uint32_t   w1 = pCmd->w1;
    BrGbiTile *p = &pSt->aTiles[(w1 >> 24) & 7u];

    p->uls = (int32_t)((w0 >> 12) & 0xFFFu);
    p->ult = (int32_t)(w0 & 0xFFFu);
    pSt->state = 6;
    p->lrs = (int32_t)((w1 >> 12) & 0xFFFu);
    p->lrt = (int32_t)(w1 & 0xFFFu);
}
#endif

/* 0x10028C70 */
/* WHAT IT DOES: says how many texture pixels are packed into one machine
 * word for a given pixel-size code: 16 for the smallest, then 8, then 4, and
 * 2 for anything else. */
/* @implements 0x10028C70 d3d BrGbiTexelsPerWord */
/* @implements 0x10027F80 glide BrGbiTexelsPerWord */
int BrGbiTexelsPerWord(int siz)
{
    switch (siz) {
    case 0:  return 16;
    case 1:  return 8;
    case 2:  return 4;
    default: return 2;
    }
}

/* 0x1002A280 */
/* WHAT IT DOES: builds the backend's version of a texture from a record the
 * game already holds, translating the record's flag bits into a pixel format
 * and size and rounding the dimensions up to powers of two. It refuses if
 * the record has no source pixels or if one particular flag bit is set, so
 * it never fills in a record that was empty. */
/* @implements 0x1002A280 d3d BrGbiTexCreate */
/* @implements 0x100297F0 glide BrGbiTexCreate */
#ifdef BR_MATCHING_BUILD
extern BrGbiTexCreateFn g_pfn18AA0B0;   /* 0x118ED1C8 */
void BrGbiTexCreate(BrGbiTexRec *pRec, uintptr_t a2)
{
    uint8_t *p = (uint8_t *)pRec;
    uint32_t flags, sel, fmt, siz;

    /* Glide record: pTex +0x00, f04 +0x04, w +0x0C, h +0x0E, flags +0x20.
     * The header's packed BrGbiTexRec is the 64-bit port layout. */
    if (*(uint32_t *)p == 0)
        return;
    flags = *(uint32_t *)(p + 0x20);
    if ((flags & 0x100000u) != 0)
        return;

    sel = flags & 0x0F000000u;
    if (sel == 0x01000000u) {
        fmt = 0; siz = 2;
    } else if (sel == 0x04000000u) {
        fmt = 1; siz = 4;
    } else {
        fmt = 2; siz = 0;
    }

    *(void **)p = g_pfn18AA0B0((void *)*(uint32_t *)p,
                     *(uint32_t *)(p + 4),
                     (uint32_t)(1 << BrGbiSizeShift((int)*(uint16_t *)(p + 0x0C))),
                     (uint32_t)(1 << BrGbiSizeShift((int)*(uint16_t *)(p + 0x0E))),
                     fmt, siz,
                     (flags >> 31) & 1u, (flags >> 30) & 1u,
                     (flags >> 29) & 1u, (flags >> 28) & 1u,
                     0u, 0u, 1u, a2);
}
#else
void BrGbiTexCreate(BrGbiTexCreateFn pfn, BrGbiTexRec *pRec, uintptr_t a2)
{
    uint32_t flags, sel, fmt, siz;

    if (pRec->pTex == NULL)
        return;
    flags = pRec->flags;
    if ((flags & 0x100000u) != 0)
        return;

    sel = flags & 0x0F000000u;
    if (sel == 0x01000000u) {
        fmt = 0; siz = 2;
    } else if (sel == 0x04000000u) {
        fmt = 1; siz = 4;
    } else {
        fmt = 2; siz = 0;
    }

    pRec->pTex = pfn(pRec->pTex, pRec->f04,
                     (uint32_t)(1 << BrGbiSizeShift((int)pRec->w)),
                     (uint32_t)(1 << BrGbiSizeShift((int)pRec->h)),
                     fmt, siz,
                     (flags >> 31) & 1u, (flags >> 30) & 1u,
                     (flags >> 29) & 1u, (flags >> 28) & 1u,
                     0u, 0u, 1u, a2);
}
#endif

/* 0x1002A740 */
/* WHAT IT DOES: makes the flat 4x4 placeholder texture used wherever a real
 * texture is not available, filling it with a dim value in two of the
 * display modes and a brighter one otherwise, then handing it to the backend
 * as a real texture. */
/* @implements 0x1002A740 d3d BrGbiSolidTexBuild */
/* @implements 0x10029C70 glide BrGbiSolidTexBuild */
#ifdef BR_MATCHING_BUILD
extern int     DAT_10226e80;           /* mode, 0x10226E80 */
extern uint8_t DAT_105e1810[];         /* 16 texels, 0x105E1810 */
extern int     DAT_10697a4c;           /* pTex out, 0x10697A4C */
void BrGbiSolidTexBuild(void)
{
    uint8_t  fill;
    uint8_t *p;

    fill = (DAT_10226e80 == 2 || DAT_10226e80 == 3) ? 0x20u : 0x80u;
    /* Orig: eax = &texels[1], write [eax-1]..[eax+2], add 4, jl &texels[17].
     * Pointer compare is unsigned (jb); orig is signed (jl). */
    p = DAT_105e1810 + 1;
    do {
        p[-1] = fill;
        p[0]  = fill;
        p[1]  = fill;
        p[2]  = fill;
        p += 4;
    } while ((int)p < (int)(DAT_105e1810 + 0x11));

    DAT_10697a4c = (int)g_pfn18AA0B0(DAT_105e1810, 0u, 4u, 4u, 1u, 4u,
                                    0u, 0u, 1u, 1u, 0u, 0u, 1u, 0u);
}
#else
void BrGbiSolidTexBuild(BrGbiTexCreateFn pfn, BrGbiSolidTex *pSt)
{
    uint8_t fill = (pSt->mode == 2 || pSt->mode == 3) ? 0x20u : 0x80u;
    int     i;

    for (i = 0; i < 16; ++i)
        pSt->aTexels[i] = fill;

    pSt->pTex = pfn(pSt->aTexels, 0u, 4u, 4u, 1u, 4u,
                    0u, 0u, 1u, 1u, 0u, 0u, 1u, 0u);
}
#endif
