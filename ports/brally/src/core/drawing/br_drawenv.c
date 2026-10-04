/* br_drawenv.c -- see br_drawenv.h.  The track/environment display-list
 * emitter, from BRGlide.dll.
 *
 * NOT CLAIMED: two FP-dense inner loops are deferred as TODO pending
 * x87emu verification.  The mechanical put() sections and the guard
 * logic are transcribed from the Glide asm at 10010000.asm line 6936.
 */
#include "br_trkhdr.h"   /* g_brTrkHdr, the loaded track header */
#include "br_drawenv.h"
#include "br_drawcar.h"     /* g_BrDrawWheelAlt, BR_PTR32(void *, g_brTrkHdr.aInstances),
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

/* 64-bit core: declared once, in br_globals.h or its struct's header */            /* 0x106EC798 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */          /* 0x106E8A18 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */        /* 0x106ED528 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */             /* 0x106ED520 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */          /* 0x106E72E8 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */          /* 0x104B15D0 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */          /* 0x1184C460 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */         /* 0x1184C478 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */           /* 0x104ADD38 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */            /* computed */
/* 64-bit core: declared once, in br_globals.h or its struct's header */             /* 0x104AF5C8 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */             /* 0x106E72A8 */

/* The put()/put_slot() helpers and the DL cursor live in br_drawcar.c;
 * they are file-static there.  This file cannot call them directly.
 * Instead, the function below uses the same inline cursor pattern. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */         /* the DL cursor, slice2_18 */

/* (port-only env_put removed) */


/* (port-only env_put_slot removed) */


/* 0x10008D60 -- 1-byte trace function (a nop in both builds). */
/* (port-only env_trace removed) */


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

/* Hand-matched from disassembly: 0x100597F0
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
