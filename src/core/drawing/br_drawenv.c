/* br_drawenv.c -- see br_drawenv.h.  The track/environment display-list
 * emitter, from BRGlide.dll.
 *
 * NOT CLAIMED: two FP-dense inner loops are deferred as TODO pending
 * x87emu verification.  The mechanical put() sections and the guard
 * logic are transcribed from the Glide asm at 10010000.asm line 6936.
 */
#include "br_drawenv.h"
#include "br_drawcar.h"     /* g_BrDrawWheelAlt, g_BrDrawTrackFlags,
                             * g_BrDrawCombined, g_BrDrawScale, TK_* */
#include "slice1_05.h"      /* BrRdpSetCombineLERP, BrGfxWords        */
#include "slice2_14.h"      /* g_BrFpsScreenW/H                       */
#include "slice2_18.h"      /* BrG_6C0258, BrG_6C6624, BrG_6C1174    */
#include "slice2_21.h"      /* BrMtxInvert, BrMtxMul                  */
#include "br_racebegin.h"   /* g_brRaceBeginDifficulty                */
#include "br_mat.h"         /* BrMat4, BrVec3Project                  */
#include "br_vec.h"
#include "slice3_39.h"      /* BrMemFill                              */

#include <string.h>

/* ---- storage --------------------------------------------------------- */

int32_t   g_BrEnvSection;            /* 0x106EC798 */
int32_t   g_BrEnvFlagCount;          /* 0x106E8A18 */
uint16_t *g_BrEnvFlagIndices;        /* 0x106ED528 */
void     *g_BrEnvCamPtr;             /* 0x106ED520 */
uint32_t  g_BrEnvOthermode;          /* 0x106E72E8 */
void     *g_BrEnvLightDirs;          /* 0x104B15D0 */
uint32_t *g_BrEnvTexLookup;          /* 0x1184C460 */
uint32_t  g_BrEnvTexDefault;         /* 0x1184C478 */
int32_t   g_BrEnvSegCount;           /* 0x104ADD38 */
void     *g_BrEnvSegBase;            /* computed */
uint8_t  *g_BrEnvBitmap;             /* 0x104AF5C8 */
BrVec3    g_BrEnvFogDir;             /* 0x106E72A8 */

/* The put()/put_slot() helpers and the DL cursor live in br_drawcar.c;
 * they are file-static there.  This file cannot call them directly.
 * Instead, the function below uses the same inline cursor pattern. */
extern uint32_t *BrG_6C0680;         /* the DL cursor, slice2_18 */

static void env_put(uint32_t w0, uint32_t w1)
{
    uint32_t *p = BrG_6C0680;
    BrG_6C0680 += 2;
    p[0] = w0;
    p[1] = w1;
}

static BrGfxWords *env_put_slot(void)
{
    BrGfxWords *p = (BrGfxWords *)BrG_6C0680;
    BrG_6C0680 += 2;
    return p;
}

/* 0x10008D60 -- 1-byte trace function (a nop in both builds). */
static void env_trace(void) { }

/* ==================================================================== *
 * 0x10017110 -- the track/environment emitter.  2,039 bytes.
 *
 * NOT CLAIMED (@implements withheld).  Two FP-dense inner loops
 * (per-car projection at 0x10017461-0x100175F2, per-segment tile
 * computation at 0x10017699-0x100178BD) are deferred as TODO.
 *
 * Structure:
 *   1. Guard: return if both mode flags clear, or if any track flag
 *      in the g_BrEnvFlagIndices array has bit 0x10 set at offset 0x4C.
 *   2. Header: pipe sync, two-cycle, texture on, othermode, combiner,
 *      render mode, conditional DD/DC texture opcode, settile, clear
 *      geom, prim colour.
 *   3. Matrix setup: invert the combined matrix, zero its translation
 *      row, multiply by a projection scale, optionally clear the
 *      per-section bitmap.
 *   4. Per-car loop (0..16): project each car's light direction through
 *      the projection matrix, write a cross-shaped stamp into the
 *      visibility bitmap.  [TODO: x87emu]
 *   5. Scene colour + matrix multiply for fog direction.
 *   6. Per-segment loop: for each segment, transform its 3-component
 *      position through the projected matrix, compute tile coordinates,
 *      and emit a single 8-byte DL command.  [TODO: x87emu]
 *   7. Tail: pipe sync + othermode restore.
 * ==================================================================== */

/* Hand-matched from disassembly â 0x100597F0
 * Inlined memset: fills `count` bytes at `dst` with byte `c`
 * (broadcast to a dword, rep stosd for count/4, rep stosb for count&3). */

#pragma intrinsic(memset)

/* WHAT IT DOES: fill a block of memory with a repeated byte (the matching
 * body of the port's BrMemFill).  Argument ORDER is destination, count,
 * value -- not the C library's -- which is the trap at every call site. */
/* @implements 0x100597F0 glide FUN_100597f0 */
void FUN_100597f0(void *dst, unsigned count, int c)
{
  memset(dst, c, count);
}
