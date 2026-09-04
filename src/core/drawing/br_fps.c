/* br_fps.c -- drawing: the on-screen frame-rate readout.
 *
 * RESPONSIBILITY: drawing/ -- turn geometry and images into pixels.
 *
 * Filed out of slice2_14.c, an address batch and not a module.  The thunk at
 * glide 0x10011D10 comes with it: it is 0x190 bytes from the readout in the
 * original and so almost certainly the same translation unit, and it has
 * nowhere better to go.
 *
 * slice2_14.c's preamble is carried over verbatim.  An include set that
 * looks redundant has already been shown elsewhere in this module to move
 * VC5's register allocation (see br_rdpmode.c), so nothing here is trimmed
 * on the grounds that it is unused.
 */

#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include "slice2_14.h"
#include "slice2_17.h"   /* BrPropList, BrScenePropsDraw (0x1002FB20)          */
#include "slice3_40.h"   /* BrNode, BrPathPoint, BrG_6C7CB8 -- the AI path root */
#include "slice1_03.h"   /* BrTextDraw                                          */
#include "slice6_78.h"   /* BrTextSetSize, BrTextAlignCentre                    */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/* Layout guards: both of these strides are load-bearing (0x20 is the index
 * scale in 0x10010BF0, 4 is the element size passed to qsort at 0x10010E9E). */
typedef char br_assert_scrpt_stride[(sizeof(BrScrPt) == 0x20) ? 1 : -1];
typedef char br_assert_cell_stride[(sizeof(BrSortCell) == 4) ? 1 : -1];
typedef char br_assert_depthref[(sizeof(BrDepthRef) >= 0x3C) ? 1 : -1];

/* ================================================================== */
/* 0x10011EA0 (glide) / 0x10014930 (d3d) -- the FPS readout.          */
/* ================================================================== */

void    *g_BrFpsGuard;                  /* glide 0x10B73538 */
int32_t  g_BrFpsGateA;                  /* glide 0x100A935C */
int32_t  g_BrFpsCountA;                 /* glide 0x100A9358 */
int32_t *g_BrFpsSamplesA;               /* glide 0x105BC900 */
float    g_BrFpsValueA;                 /* glide 0x100A64B0 */
int32_t  g_BrFpsGateB;                  /* glide 0x100B4C2C */
int32_t  g_BrFpsCountB;                 /* glide 0x100B4C28 */
int32_t *g_BrFpsSamplesB;               /* glide 0x10B73348 */
float    g_BrFpsValueB;                 /* glide 0x100A64B4 */
int32_t  g_BrFpsScreenW;                /* glide 0x100A7514 */
int32_t  g_BrFpsScreenH;                /* glide 0x100A7518 */

/* WHAT IT DOES: build the on-screen performance readout -- averages several
 * running sample tables and formats them into a line of text. The debug
 * frame-rate display. Does nothing at all if the readout is switched off. */
/* @implements 0x10011EA0 glide BrFpsReadout */
void BrFpsReadout(void)
{
    char buf[256];

    if (g_BrFpsGuard == NULL)
        return;

    /* Each accumulator runs even when count <= 0 (divides by the 0.0f
     * seed). Samples are a table at a fixed address, added as unsigned. */
    if (g_BrFpsGateA == 0) {
        float sum = 0.0f;
        int n = g_BrFpsCountA;
        if (n > 0) {
#ifdef BR_MATCHING_BUILD
            int32_t *p = (int32_t *)&g_BrFpsSamplesA;
#else
            int32_t *p = g_BrFpsSamplesA;
#endif
            do {
                sum += (unsigned)*p++;
            } while (--n);
        }
        g_BrFpsValueA = ((float)g_BrFpsCountA * 1000.0f) / sum;
    }

    if (g_BrFpsGateB == 0) {
        float sum = 0.0f;
        int n = g_BrFpsCountB;
        if (n > 0) {
#ifdef BR_MATCHING_BUILD
            int32_t *p = (int32_t *)&g_BrFpsSamplesB;
#else
            int32_t *p = g_BrFpsSamplesB;
#endif
            do {
                sum += (unsigned)*p++;
            } while (--n);
        }
        g_BrFpsValueB = ((float)g_BrFpsCountB * 1000.0f) / sum;
    }

    sprintf(buf, "%6.2f FPS", (double)g_BrFpsValueB);
    BrTextSetSize(0x0F);
    BrTextAlignCentre();
    BrTextDraw(buf, g_BrFpsScreenW / 2, g_BrFpsScreenH - 10);
}

/* ── Ghidra-matched functions ─────────────────────────── */
#ifdef BR_MATCHING_BUILD
int FUN_1006e590();

/* WHAT IT DOES: thunk — forwards to the shared no-op at 0x1006E590. */
/* @implements 0x10011D10 glide BrThunk11D10 */
/* @n64 0x802288B4 exact */

int BrThunk11D10(void)

{
  FUN_1006e590();
  return;
}

extern short DAT_10396ef8;
extern short DAT_10396efc;
extern short DAT_10396f00;
extern int DAT_10396f04;


#endif /* BR_MATCHING_BUILD */
