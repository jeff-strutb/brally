/* br_gbitexscan.c -- the display-list texture-scan pre-pass.
 *
 * Filed out of slice2_16.c (an address batch, not a module) as each function
 * was matched.  The rest of the BrGbiTexScan family is still in slice2_16.c
 * and follows here as it matches.
 *
 * MATCHING NOTE: the original keeps this pass's state in fixed globals and
 * these functions take no state pointer.  The port threads a BrGbiTexScan
 * through instead so the state is reachable without absolute addresses, which
 * is why every function here carries both arms.
 */
#include "slice2_16.h"
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
