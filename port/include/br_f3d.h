/* br_f3d.h -- N64 F3D display-list walker (portable).
 *
 * Boss Rally's PC build is an N64 display-list interpreter: the game core
 * emits F3D command streams and each backend (DirectDraw, Glide -- and now
 * Metal) executes them. Car and track geometry lives in these lists rather
 * than in any flat mesh array, which is why scanning for vertex tables finds
 * nothing.
 *
 * Commands are 8 bytes, big-endian, 8-byte aligned:
 *   G_VTX  (0x04)  F3DEX: bits[15:10] = n, bits[7:1] = v0+n, w1 = segment addr
 *   G_TRI1 (0xBF)  w1 bytes 1..3 = vertex indices, each pre-multiplied by 2
 *   G_TRI2 (0xB1)  w0 bytes 1..3 and w1 bytes 1..3 = two triangles, same *2
 *   G_ENDDL(0xB8)  end of list
 *
 * The *2 on indices is the giveaway and the main validation hook: every index
 * byte in a genuine list is even.
 *
 * Microcode: **F3DEX, established from the game's own code**, not inferred.
 * The engine's G_VTX fixup at 0x1002C150 extracts the vertex count as
 * `(w0 >> 10) & 0x3F`, which is F3DEX's gSPVertex packing. Plain F3D puts
 * `16n-1` in the low byte and F3DEX2 uses bit 12; neither yields n here.
 *
 * CORRECTION: this file previously decoded G_VTX using F3D's layout
 * (`n = (byte3+1)/16`). That is WRONG for this game and systematically
 * UNDERCOUNTS: it disagrees with the correct decoding on 57 of 151 G_VTX
 * commands in ce.rca, reading 8 where the true count is 24 and 16 where it
 * is 32. Triangle decoding was unaffected (indices are pre-doubled either
 * way); only vertex counts were wrong.
 *
 * OPEN: with n up to 32 and (v0+n) observed as high as 63, the vertex cache
 * may be 64 entries rather than 32. BR_F3D_VTX_CACHE is left at 32 pending
 * evidence from the backend's vertex-load handler -- do not treat it as
 * settled.
 */
#ifndef BR_F3D_H
#define BR_F3D_H

/* The hardware vertex buffer G_VTX loads into. The Glide build's array is at
 * 0x105CE318 with a 104-byte stride; 64 is F3DEX's architectural maximum and is
 * used as the bound because the walker does not model the array itself. */
#define BR_F3D_VTX_SLOTS 64

#include <stddef.h>
#include <stdint.h>

#define BR_F3D_VTX_CACHE 32

enum {
    BR_G_VTX    = 0x04,
    BR_G_TRI1   = 0xBF,
    BR_G_TRI2   = 0xB1,
    BR_G_ENDDL  = 0xB8,
    BR_G_DL     = 0x06
};

typedef struct BrF3dStats {
    uint32_t cCommands;
    uint32_t cVtxLoads;
    uint32_t cVerticesLoaded;
    uint32_t cTriangles;
    uint32_t cUnknownOps;
    uint32_t cBadIndices;     /* odd, or beyond the vertex cache */
} BrF3dStats;

/* Walk one display list starting at pvList. Stops at G_ENDDL or after
 * cbMax bytes. Returns 0 if the list validated cleanly. */
int BrF3dWalk(const void *pvList, size_t cbMax, BrF3dStats *pStats);

/* Scan a whole file for display lists and accumulate statistics. */
int BrF3dScanFile(const void *pvData, size_t cbData, BrF3dStats *pStats);

#endif /* BR_F3D_H */
