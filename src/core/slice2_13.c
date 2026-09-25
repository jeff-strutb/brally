/* slice2_13.c -- decompiled from BRD3D.dll, pass-13 packet
 * (0x10008B90 - 0x100109A0). See slice2_13.h for the identification notes
 * and every GOTCHA; this file carries the DEVIATION notes.
 */

#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef BR_MATCHING_BUILD
/* Header prototype is cdecl; the original is __stdcall. */
#define BrFileWriteChecked BrFileWriteChecked_cdecl
#define BrDPlayThreadProc  BrDPlayThreadProc_cdecl_hdr
#endif
#include "slice2_13.h"
#ifdef BR_MATCHING_BUILD
#undef BrFileWriteChecked
#undef BrDPlayThreadProc
uint32_t __stdcall BrDPlayThreadProc(void *pvCtx);
#endif
#include "slice1_03.h"   /* BrAppMsg, BrAppMsgDispatch (= 0x1000BEA0) */

/* ==========================================================================
 * Cross-slice declarations
 * ========================================================================== */

/* 0x10008CC0 -- the printf-style error reporter both file helpers call.
 * It is in no packet in this slice.
 * XSLICE 0x10008CC0 */
extern void BrErrorf(const char *pszFmt, ...);

/* XSLICE 0x10071480 */
extern void BrSub10071480(uint32_t idPlayer);
/* XSLICE 0x10005FE0 */
extern void BrSub10005FE0(uint32_t idPlayer);
/* XSLICE 0x100360F0 */
extern void BrSub100360F0(void *pv1, uint32_t f0C, uint32_t f10,
                          uint32_t f08, uint32_t idTo);
/* XSLICE 0x1003CE80 */
extern void BrSub1003CE80(void);
/* 0x1000BAF0, the non-system message route. slice2_22 knows it as
 * APPMSG_HOSTSTARTED.
 * XSLICE 0x1000BAF0 */
extern void BrSub1000BAF0(void *pCtx, const void *pvData, uint32_t cbData,
                          uint32_t idFrom, uint32_t idTo);
/* 0x1003D0B0 -- "size it, allocate it, fill it" over state->pDPGlobal.
 * *ppvOut receives a GlobalAlloc'd + GlobalLock'ed record.
 * XSLICE 0x1003D0B0 */
extern int32_t BrSub1003D0B0(BrDPlay4Obj *pObj, void **ppvOut);

/* ==========================================================================
 * 1. 0x10008B90 / 0x10008BE0 / 0x10008C90 -- file and path helpers
 * ========================================================================== */

void BrPathBaseName(const char *pszPath, char *pszDst)
{
    size_t      cch = strlen(pszPath);
    const char *p;

    /* DEVIATION: the original computes `p = pszPath + strlen - 1` and only
     * skips the backward scan when that lands exactly on pszPath. For an
     * EMPTY string it lands on pszPath-1, the scan then reads pszPath[-2]
     * and walks down through memory until it happens to find a 0x5C or
     * faults. The empty string is guarded here instead. */
    if (cch == 0) {
        pszDst[0] = '\0';
        return;
    }

    p = pszPath + cch - 1;
    while (p != pszPath && p[-1] != '\\')
        --p;

    memcpy(pszDst, p, strlen(p) + 1);
}

/* @n64 0x8021A9B4 located */
FILE *BrFileOpenWrite(const char *pszPath)
{
    FILE *pFile = fopen(pszPath, "wb");

    if (pFile == NULL)
        BrErrorf("Error opening %s: %s", pszPath, strerror(errno));

    return pFile;
}


/* ==========================================================================
 * 2. 0x1000BA70 -- index walker
 * ========================================================================== */

static BrCursorState g_BrCursor;

BrCursorState *BrCursorGetState(void)
{
    return &g_BrCursor;
}

void BrCursorAdvance(void)
{
    BrCursorState *pSt  = &g_BrCursor;
    int32_t        step = pSt->step;

    if (step == 0)
        return;

    for (;;) {
        int32_t pos = pSt->pos + step;
        int32_t i;

        pSt->pos = pos;
        if (pos >= pSt->limit) {
            pos      = 0;
            pSt->pos = 0;
        }
        if (pos < 0) {
            pos      = pSt->limit - 1;
            pSt->pos = pos;
        }
        if (pos == 0)
            break;

        for (i = 0; i < pSt->cStops; ++i) {
            if (pos == (int32_t)pSt->aStops[i])
                goto stop;
        }
    }

stop:
    pSt->step = 0;
}

/* Section 4, the clip pool and BrPolyClipPlane / BrPolyClipTri, filed to
 * drawing/br_polyclip.c. */


/* ==========================================================================
 * 5. 0x1000F5C0 / 0x1000F620
 * ========================================================================== */

static BrGfxBanks    g_BrGfxBanks;
static BrGfxCounters g_BrGfxCounters;

BrGfxBanks *BrGfxGetBanks(void)
{
    return &g_BrGfxBanks;
}

void BrGfxSetBankPointers(void)
{
    BrGfxBanks *pB = &g_BrGfxBanks;
    int32_t     i  = pB->iBank;

    pB->p363FF0 = pB->p2E5EC8 = (char *)pB->pBase0 + (ptrdiff_t)i * 80000;
    pB->p364304 = pB->p3643BC = (char *)pB->pBase1 + (ptrdiff_t)i * 32000;
    pB->p2E5EC4 = pB->p363FF4 = (char *)pB->pBase2 + (ptrdiff_t)i * 256000;
}

BrGfxCounters *BrGfxGetCounters(void)
{
    return &g_BrGfxCounters;
}

/* @n64 0x80269410 located */
void BrGfxClearCounters(void)
{
    memset(g_BrGfxCounters.a364308, 0, sizeof(g_BrGfxCounters.a364308));
    memset(g_BrGfxCounters.a363F68, 0, sizeof(g_BrGfxCounters.a363F68));
}

/* ==========================================================================
 * NOT PORTED -- five functions of the packet, and why
 * ==========================================================================
 *
 * 0x1000C6E0  (932 bytes) display-list emitter. Four identical passes that
 *   append ~35 fixed 8-byte RDP/RSP commands to the cursor at 0x106C0680 and
 *   make five outside calls, one of them (0x1002F900) with sixteen stack
 *   arguments whose meaning is not established anywhere in this slice. It is
 *   transcribable but nothing in this packet can validate the transcription,
 *   and a silently mis-copied command word is worse than a missing function.
 *
 * 0x1000CA90  (322 bytes) per-entity visibility gate. Reaches five outside
 *   vector routines (0x1003B0E0, 0x10031D3F, 0x1003A950, 0x1003AFE0) and
 *   seven globals whose meaning is not established; the two indexed writes at
 *   0x10277E60 and 0x10277B68 are keyed off entity+0x140, which this packet
 *   never bounds. Skipped rather than guessed.
 *
 * 0x1000E950  (1576 bytes) the same kind of emitter as 0x1000C6E0, plus two
 *   float accumulators at 0x10575504 / 0x105754F8 fed through 0x1003AC90 and
 *   0x1003B020. Same reasoning.
 *
 * 0x1000EF80  (1246 bytes) walks a 10x10 grid of command lists looking for
 *   G_VTX (0x04) blocks and displaces their vertices. The vertex count is
 *   decoded as bits [15:10] of the command word, which is the F3DEX layout
 *   the contract records -- but the displacement is eight separate x87
 *   sequences with sign-dependent branches on the low nibble of a truncated
 *   angle, and getting one of them backwards would silently corrupt geometry.
 *   Not ported without a way to test it.
 *
 * 0x1000F480  (308 bytes) projects a bounding box corner pair into two
 *   16-bit screen rectangles. Nine values live on the x87 stack at once
 *   across six fxch pairs and four __ftol calls; the contract's own warning
 *   about untraceable x87 operand order applies exactly. Skipped.
 */
