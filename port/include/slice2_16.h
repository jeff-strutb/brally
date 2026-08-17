/* slice2_16.h -- Boss Rally (BRD3D.dll) decompilation, a later pass.
 *
 * Address range 0x1001CD60 .. 0x1002BC90.
 *
 * Almost everything in this packet belongs to the engine's N64 GBI layer.
 * Three clusters:
 *
 *   1. F3D command handlers (0x1001CD60 .. 0x100242F0) plus the display-list
 *      interpreter itself (0x10024A90). Each handler has the uniform shape
 *      `Gfx *handler(Gfx *pCmd)` and returns the address of the next command,
 *      which is what lets 0x10024A90 drive them from a 256-entry table.
 *
 *   2. A second, non-executing pass over a display list (0x100290E0 and its
 *      leaves 0x10029410 .. 0x1002A250) that recognises texture-load
 *      sequences and rewrites them, plus the two texture-upload thunks
 *      0x10028BF0 / 0x1002A280.
 *
 *   3. A screen-wipe / fade animator (0x1002AF10 .. 0x1002B670) that emits
 *      its own display-list commands, and a group of .rca byte-swap /
 *      pointer-fixup helpers (0x1002B930 .. 0x1002BC90).
 *
 * ---------------------------------------------------------------------
 * How the three jump tables were resolved
 * ---------------------------------------------------------------------
 * 0x10024150, 0x100242F0 and 0x100290E0 all dispatch through `jmp [reg*4 +
 * table]` with a preceding byte-index table. The disassembly listing does not
 * contain those tables, so they were read directly out of orig/BRD3D.dll
 * (.text RVA 0x1000 -> file 0x400). The recovered mappings are reproduced
 * verbatim in the .c file and they agree exactly with libultra's F3D GBI:
 *
 *   0x10024150  G_MOVEMEM: 0x80 viewport, 0x82/0x84 lookat, 0x86..0x94
 *               lights 0..7, 0x9E matrix; everything else falls through.
 *   0x100242F0  G_MOVEWORD: 0x02 numlights, 0x0A lightcol, 0x08 and 0x0E
 *               reach the table but land on the default arm.
 *   0x100290E0  full F3D opcode set (0x04 G_VTX, 0xB1/0xBF tris, 0xB8 enddl,
 *               0xB9/0xBA othermode, 0xBB texture, 0xE6/0xE7/0xE8 syncs,
 *               0xF0 loadtlut, 0xF2 settilesize, 0xF3 loadblock, 0xF5
 *               settile, 0xFA/0xFB/0xFC prim/env/combine, 0xFD settimg).
 *
 * That the light stride in G_MOVEWORD is `(offset >> 5) << 4` -- exactly
 * F3D's G_MWO_aLIGHT_n spacing of 0x20 mapping onto a 16-byte record -- is
 * independent confirmation of the same GBI.
 *
 * The .rdata float constants were likewise read out of the DLL rather than
 * guessed; each is quoted at the point of use with its address.
 *
 * ---------------------------------------------------------------------
 * File-wide deviations
 * ---------------------------------------------------------------------
 *   - The original reaches all of its state through fixed globals. Following
 *     br_seg.h / br_pool.h / slice1_05.h, that state is passed in as structs;
 *     every field records the global it stands for.
 *   - Consequently the ported handlers take an extra leading state pointer
 *     and therefore do NOT have the uniform `Gfx *(*)(Gfx *)` shape the
 *     original's dispatch table needs. BrGbiRun (0x10024A90) still models the
 *     original table exactly; wiring the ported handlers into it needs a
 *     per-handler thunk that supplies the state.
 *   - Where the original stores a 32-bit machine word that is really a
 *     pointer (display-list branch targets, .rca fixups) the port keeps the
 *     u32 and, where it must dereference, goes through a caller-supplied
 *     resolver. Noted at each site.
 *   - x87 computes at 80-bit internal precision. That is not reproduced;
 *     results are computed in float/double. Where the original's evaluation
 *     ORDER is observable it is preserved.
 *
 *     THE SECOND HALF OF THIS CLAUSE HAS BEEN DELETED, AND WHAT IT SAID IS
 *     WORTH RECORDING. It read: "...and its comparison flags treat unordered
 *     as 'less or equal'. Neither is reproduced; ... NaN inputs will take
 *     different branches."
 *
 *     That is a FILE-WIDE PRE-AUTHORISATION for a defect class this project
 *     has now found live SIX times, and it was self-contradicted: the note
 *     over BrGbiLightVertex treats an identical `test ah,1` mismatch as a
 *     real bug and repairs it, in this same file. One clause waived what
 *     another fixed.
 *
 *     CONVENTIONS.md's rule is a deviation documented AT THE SITE WITH A
 *     REASON. A file-wide waiver is not that. It converts an unbounded number
 *     of unexamined divergences into "documented" ones, and its practical
 *     effect was that an auditor had to argue past it rather than just read
 *     the code -- so 24 of this file's 31 x87 compare sites went unchecked
 *     until someone did.
 *
 *     All 31 have now been derived from the flag masks. Where a comparison
 *     genuinely cannot be reproduced it is documented at its own site.
 *   - Fields whose meaning could not be established are named positionally
 *     (fNN = the low bytes of the global's address).
 */
#ifndef SLICE2_16_H
#define SLICE2_16_H

#include <stddef.h>
#include <stdint.h>

#include "br_bits.h"    /* BrSwapVec3 (0x100383C0) */
#include "br_mat.h"     /* BrMat4 */
#include "br_seg.h"     /* BrSegMap, BrSegFixup (0x1002B970) */
#include "slice1_05.h"  /* BrGfxWords, BrMat4Mul, BrRdpSetCombineLERP,
                         * BrVtxCache, BrPtrList */

/* ================================================================== */
/* Cross-slice imports                                                */
/* ================================================================== */

/* Called after every geometry-mode change (0x1001E790, 0x10020F20). Takes no
 * arguments; it reads the mode globals directly. */
/* XSLICE 0x1001E7C0 */
extern void BrGbiGeoModeChanged(void);

/* Display-list stack overflow handler. The original does `push 1 / call /
 * add esp,4` and then falls through as if it returned, so it is modelled as
 * a returning call rather than as noreturn. */
/* XSLICE 0x1007CC00 */
extern void BrGbiStackOverflow(int code);

/* Sub-handler reached from 0x10020F50 when the selector byte is 0. */
/* XSLICE 0x100243D0 */
extern BrGfxWords *BrGbiCall100243D0(BrGfxWords *pCmd);

/* Applied after 0x10020F80 latches w1 into BrGbiState.f1694. */
/* XSLICE 0x10020FA0 */
extern void BrGbiCall10020FA0(uint32_t w1);

/* Tile-rectangle sink shared by 0x10021510 and 0x10021B80. The argument
 * order is the original's: (lrs, lrt, uls, ult, tile) -- the LOWER-RIGHT
 * pair comes FIRST. */
/* XSLICE 0x10021560 */
extern void BrGbiCall10021560(int lrs, int lrt, int uls, int ult, int tile);

/* G_MOVEMEM index 0x80 (viewport). Returns the next command. */
/* XSLICE 0x10024260 */
extern BrGfxWords *BrGbiCall10024260(BrGfxWords *pCmd);

/* Registers the bytes staged in BrGbiTexScan.aStage and returns an id, or
 * -1 if it declines. */
/* XSLICE 0x10029470 */
extern int BrGbiCall10029470(const void *pStage);

/* Released at the end of every 0x1002BAA0 record. */
/* XSLICE 0x10075330 */
extern void BrGbiCall10075330(void *pv);

/* ================================================================== */
/* 1. F3D command handlers                                            */
/* ================================================================== */

/* --- tile size (0x1001CF30) ----------------------------------------
 * G_SETTILESIZE, opcode 0xF2 -- NOT G_SETSCISSOR, which is what this was
 * called until the display-list opcode audit.  It is a NAMING defect only:
 * the arithmetic below was right all along.
 *
 * THE EVIDENCE, and it is the dispatch table rather than the shape:
 *   - BRD3D's table at 0x100A79F0 holds 0x1001CF30 in slot 0xF2.  Slots 0xE2
 *     and 0xED -- the two that really are the scissor -- hold 0x1001CE70 and
 *     0x1001CDA0, two different, longer functions (191 and 197 bytes) that
 *     end in a clip-window call.  0x1001CF30 calls nothing at all.
 *   - Its Glide counterpart is 0x1001EC30, the same 178 bytes, which
 *     BRGlide's table at 0x100A9A58 also holds in slot 0xF2, and which
 *     br_dl.c transcribes as br_dl_settilesize.  The two are the same
 *     function under the two builds' addresses; do not give them a third
 *     name.
 *   - A scissor has no `(lr - ul + 4) >> 2` extent.  A tile does: that is
 *     the texel width and height of the loaded tile.
 *
 * Each of the four 12-bit 10.2 fields is sign-extended by hand:
 * `if (v >= 0x800) v -= 0x1000`.  tileW and tileH are the texel extents,
 * (lr - ul + 4) >> 2, with an ARITHMETIC shift (`sar eax,2` at 0x1001CFCB)
 * so negative extents stay negative. */
typedef struct BrGbiTileSize {
    int32_t uls;    /* 0x118AA080  (w0 >> 12) & 0xFFF, sign-extended */
    int32_t ult;    /* 0x11829838   w0        & 0xFFF, sign-extended */
    int32_t lrs;    /* 0x1182983C  (w1 >> 12) & 0xFFF, sign-extended */
    int32_t lrt;    /* 0x118A9870   w1        & 0xFFF, sign-extended */
    int32_t tileW;  /* 0x11829840  (lrs - uls + 4) >> 2 */
    int32_t tileH;  /* 0x118AA094  (lrt - ult + 4) >> 2 */
} BrGbiTileSize;

/* --- geometry mode (0x1001E790 clear, 0x10020F20 set) --------------
 * `prev` is written with the value the mode had BEFORE the change, on both
 * paths, and 0x1001E7C0 is called afterwards. */
typedef struct BrGbiGeoMode {
    uint32_t cur;    /* 0x104C5178 */
    uint32_t prev;   /* 0x104C517C */
} BrGbiGeoMode;

/* --- display-list stack (0x10020D60 push, 0x10020DA0 pop) ----------
 * GOTCHA: the overflow guard fires on `count + 1 == 10`, i.e. while the
 * stack still has a free slot, and the store happens anyway afterwards. The
 * array therefore genuinely holds ten entries but the tenth push is always
 * reported. */
#define BR_GBI_DL_STACK_MAX 10
typedef struct BrGbiDLStack {
    BrGfxWords *ap[BR_GBI_DL_STACK_MAX];   /* 0x104C16A8 */
    int32_t     n;                         /* 0x104C01A4 */
} BrGbiDLStack;

/* --- matrix stack (0x10020DC0 G_MTX, 0x10020EF0 G_POPMTX) ----------
 * The stack index lives in `top` and is a RING of ten: push does
 * `if (top == 10) top = 0; ++top` and pop does `--top; if (top == 0)
 * top = 10`. top == 0 additionally means "no modelview" -- the two places
 * that build a pointer from it substitute NULL.
 *
 * GOTCHA (reproduced): the projection matrix is at 0x104BBEC0 and the stack
 * base at 0x104BBED0, only 16 bytes later, so slot 0 of the stack overlaps
 * the projection matrix's last three rows. Slot 0 is written only by a
 * G_MTX load/mul without G_MTX_PUSH taken while top == 0. `aWords` keeps the
 * two aliased exactly as in the original; use the accessors below rather
 * than adding separate members. */
#define BR_GBI_MTX_SLOTS      11      /* indices 0..10 */
#define BR_GBI_MTX_STACK_OFF  4       /* floats: 0x104BBED0 - 0x104BBEC0 */
#define BR_GBI_MTX_WORDS      (BR_GBI_MTX_STACK_OFF + BR_GBI_MTX_SLOTS * 16)

typedef struct BrGbiMtxState {
    float   aWords[BR_GBI_MTX_WORDS];  /* 0x104BBEC0 upward */
    BrMat4  combined;                  /* 0x104C4D10 */
    int32_t top;                       /* 0x100A79DC */
    int32_t f5180;                     /* 0x104C5180, cleared on every change */
} BrGbiMtxState;

BrMat4 *BrGbiMtxProj(BrGbiMtxState *pSt);            /* 0x104BBEC0 */
BrMat4 *BrGbiMtxSlot(BrGbiMtxState *pSt, int index); /* 0x104BBED0 + i*0x40 */

/* --- lights ---------------------------------------------------------
 * Eight 16-byte records at 0x104BBE38. G_MOVEMEM 0x86..0x94 replaces a whole
 * record; G_MOVEWORD 0x0A writes three bytes at +0 (offset nibble 0) or at
 * +4 (offset nibble 4). */
#define BR_GBI_LIGHT_SLOTS 8
#define BR_GBI_LIGHT_SIZE  16
typedef struct BrGbiLights {
    uint8_t aRaw[BR_GBI_LIGHT_SLOTS * BR_GBI_LIGHT_SIZE];  /* 0x104BBE38 */
} BrGbiLights;

/* --- per-vertex lighting constants (0x10022350) --------------------- */
typedef struct BrGbiLightState {
    int32_t numLights;   /* 0x104BC190, set by G_MOVEWORD 0x02 */
    float   dir[3];      /* 0x104C15DC, 0x104C15E0, 0x104C15E4 */
    float   scale[3];    /* 0x104C15D0, 0x104C15D4, 0x104C15D8 */
    float   ambient[3];  /* 0x104C15E8, 0x104C15EC, 0x104C15F0 */
    /* GOTCHA: the "lighting disabled" fallback does NOT use `ambient`; it
     * copies three globals that are not even contiguous. */
    float   off[3];      /* 0x104C5154, 0x104C5160, 0x104C1690 */
} BrGbiLightState;

/* --- the aggregate ------------------------------------------------- */
typedef struct BrGbiState {
    BrGbiTileSize   tile;
    BrGbiGeoMode    geo;
    BrGbiDLStack    dl;
    BrGbiMtxState   mtx;
    BrGbiLights     lights;
    BrGbiLightState light;
    uint32_t        f0A79E8;   /* 0x100A79E8, latched by 0x1001CD60 */
    uint32_t        f4C5174;   /* 0x104C5174, latched by 0x1001CD80 */
    uint32_t        f1694;     /* 0x104C1694, latched by 0x10020F80 */
    uint32_t        f1698;     /* 0x104C1698, G_MOVEMEM 0x82 */
    uint32_t        f169C;     /* 0x104C169C, G_MOVEMEM 0x84 */
} BrGbiState;

/* 0x1001CD60  f0A79E8 = w1. */
BrGfxWords *BrGbiSet0A79E8(BrGbiState *pSt, BrGfxWords *pCmd);
/* 0x1001CD80  f4C5174 = w1. */
BrGfxWords *BrGbiSet4C5174(BrGbiState *pSt, BrGfxWords *pCmd);
/* 0x1001CF30  G_SETTILESIZE, opcode 0xF2 -- see BrGbiTileSize above for
 * why this is not the scissor.  Glide 0x1001EC30 == br_dl.c's
 * br_dl_settilesize is the same function. */
BrGfxWords *BrGbiSetTileSize(BrGbiState *pSt, BrGfxWords *pCmd);
/* 0x1001E790  G_CLEARGEOMETRYMODE: cur &= ~w1. */
BrGfxWords *BrGbiClearGeometryMode(BrGbiState *pSt, BrGfxWords *pCmd);
/* 0x10020F20  G_SETGEOMETRYMODE:   cur |= w1. */
BrGfxWords *BrGbiSetGeometryMode(BrGbiState *pSt, BrGfxWords *pCmd);

/* 0x10020D60  G_DL. Pushes the following command unless w0 bits[23:16] are
 * non-zero (G_DL_NOPUSH) and returns w1 -- the branch target -- NOT the
 * next command.
 *
 * DEVIATION: the original returns w1 reinterpreted as a code pointer. Here
 * it is returned as (BrGfxWords *)(uintptr_t)w1, which only round-trips on
 * hosts where display lists live below 4GB. Compare numerically, do not
 * dereference, unless the caller has arranged otherwise. */
BrGfxWords *BrGbiDList(BrGbiState *pSt, BrGfxWords *pCmd);
/* 0x10020DA0  G_ENDDL. Pops; returns NULL when the stack is empty, which is
 * what stops the interpreter at 0x10024A90. Ignores its argument entirely --
 * the original takes none. */
BrGfxWords *BrGbiEndDList(BrGbiState *pSt);

/* 0x10020DC0  G_MTX. w0 bit 16 = projection, bit 17 = load (else multiply),
 * bit 18 = push. Always finishes by recomputing combined = modelview * proj.
 *
 * GOTCHA: `push` is honoured only on the modelview paths; a projection load
 * or multiply ignores it.
 *
 * DEVIATION: the original takes the source matrix from w1, a 32-bit address.
 * It is an explicit parameter here so the routine works on a 64-bit host;
 * pass the memory w1 names. */
BrGfxWords *BrGbiMatrix(BrGbiState *pSt, BrGfxWords *pCmd, const BrMat4 *pIn);
/* 0x10020EF0  G_POPMTX. Ring decrement; 0 stays 0. */
BrGfxWords *BrGbiPopMatrix(BrGbiState *pSt, BrGfxWords *pCmd);

/* 0x10020F50  Dispatch on the SIGN-EXTENDED byte w0 bits[23:16]:
 * 0 -> 0x100243D0, 3 -> 0x10020F80, anything else -> next command. */
BrGfxWords *BrGbiDispatch10020F50(BrGbiState *pSt, BrGfxWords *pCmd);
/* 0x10020F80  f1694 = w1, then 0x10020FA0(w1). */
BrGfxWords *BrGbiSet4C1694(BrGbiState *pSt, BrGfxWords *pCmd);

/* 0x10021510  Unpacks (uls,ult) from w0 and (lrs,lrt,tile) from w1 and hands
 * them to 0x10021560 unscaled.
 *
 * GOTCHA: this handler returns pCmd + 3 commands (0x18 bytes), not pCmd + 1.
 * The original adds 0x10 to the cursor up front and 8 more at the end while
 * reading its operands through negative displacements. Preserved. */
BrGfxWords *BrGbiTileRect(BrGbiState *pSt, BrGfxWords *pCmd);
/* 0x10021B80  Same five operands, each of the four coordinates multiplied by
 * four (integer texel -> 10.2). Returns pCmd + 1 command. */
BrGfxWords *BrGbiTileRectS(BrGbiState *pSt, BrGfxWords *pCmd);

/* 0x10022350  Per-vertex directional light.
 *
 *   t = src[5]*dir[0] + src[6]*dir[1] + src[7]*dir[2]     (floats at
 *       +0x14, +0x18, +0x1C; the original's add order is preserved)
 *
 * If numLights == 0, dst[7..9] = off[0..2] and nothing is computed.
 * If t < 0, dst[7..9] = ambient[0..2].
 * Otherwise dst[7+i] = min(t*scale[i] + ambient[i], 1.0f), where the "clamp"
 * substitutes the literal 1.0f (0x3F800000) rather than the limit it
 * compared against -- the limit at 0x1008F3C4 happens to be 1.0f too.
 *
 * dst indices 7,8,9 are the floats at +0x1C, +0x20, +0x24. */
void BrGbiLightVertex(const BrGbiLightState *pSt, const float *pSrc, float *pDst);

/* 0x10022DC0  Clip codes for one clip-space vertex.
 *
 * w = v[6] (+0x18); the three coordinates are v[3] (+0x0C), v[1] (+0x04) and
 * v[2] (+0x08), tested IN THAT ORDER:
 *
 *   0x01  w        < 0
 *   0x02  v[3] + w < 0     0x04  w - v[3] < 0
 *   0x08  v[1] + w < 0     0x10  w - v[1] < 0
 *   0x20  v[2] + w < 0     0x40  w - v[2] < 0
 *
 * GOTCHA: the coordinate order is 0x0C, 0x04, 0x08 -- not 0x04, 0x08, 0x0C. */
int BrGbiClipCodes(const float *pVert);

/* 0x10024150  G_MOVEMEM, and 0x100242F0  G_MOVEWORD.
 * Same DEVIATION as BrGbiMatrix: `pSrc` is the memory w1 names. G_MOVEWORD
 * needs none, so it keeps the original's two arguments. */
BrGfxWords *BrGbiMoveMem(BrGbiState *pSt, BrGfxWords *pCmd, const void *pSrc);
BrGfxWords *BrGbiMoveWord(BrGbiState *pSt, BrGfxWords *pCmd);
/* 0x10024240  G_MOVEMEM 0x9E: copy 16 dwords from w1 into `combined`. */
BrGfxWords *BrGbiMoveMemMatrix(BrGbiState *pSt, BrGfxWords *pCmd,
                               const void *pSrc);

/* 0x10024A90  The display-list interpreter.
 *
 *   while (pCmd) pCmd = apTable[(pCmd->w0 >> 24) & 0xFF](pCmd);
 *
 * apTable is the 256-entry table at 0x100A79F0; it is a parameter here only
 * so the port has no hardcoded absolute address. A NULL start pointer is
 * checked before the first dispatch. */
typedef BrGfxWords *(*BrGbiHandler)(BrGfxWords *pCmd);
void BrGbiRun(const BrGbiHandler *apTable, BrGfxWords *pCmd);

/* ================================================================== */
/* 2. Texture-load scanning pass                                      */
/* ================================================================== */

/* Per-tile record, 0x40 bytes at 0x105551D8 + tile*0x40. The first twelve
 * fields come from G_SETTILE (0x1002A040), the last four from G_SETTILESIZE
 * (0x1002A140). Field names follow the RDP SETTILE encoding, which the bit
 * positions match exactly. */
typedef struct BrGbiTile {
    int32_t fmt;      /* +0x00  (w0 >> 21) & 7   */
    int32_t siz;      /* +0x04  (w0 >> 19) & 3   */
    int32_t line;     /* +0x08  ((w0 >> 9) & 0x1FF) << 3 */
    int32_t tmem;     /* +0x0C   w0 & 0x1FF      */
    int32_t mirrorS;  /* +0x10  (w1 >> 8)  & 1   */
    int32_t clampS;   /* +0x14  (w1 >> 9)  & 1   */
    int32_t mirrorT;  /* +0x18  (w1 >> 18) & 1   */
    int32_t clampT;   /* +0x1C  (w1 >> 19) & 1   */
    int32_t maskS;    /* +0x20  (w1 >> 4)  & 0xF */
    int32_t maskT;    /* +0x24  (w1 >> 14) & 0xF */
    int32_t shiftS;   /* +0x28   w1 & 0xF        */
    int32_t shiftT;   /* +0x2C  (w1 >> 10) & 0xF */
    int32_t uls;      /* +0x30  (w0 >> 12) & 0xFFF */
    int32_t ult;      /* +0x34   w0 & 0xFFF        */
    int32_t lrs;      /* +0x38  (w1 >> 12) & 0xFFF */
    int32_t lrt;      /* +0x3C   w1 & 0xFFF        */
} BrGbiTile;

#define BR_GBI_TILE_COUNT 8
/* Fixed staging buffer at 0x104C51A8. The original's size is not visible in
 * the code; a G_LOADBLOCK can ask for up to 2*0x1000 bytes, so that is used
 * here and the copy is clamped to it (see the DEVIATION at 0x10029FA0). */
#define BR_GBI_STAGE_SIZE 8192

/* The scan's state machine. Values of `state` observed in the code:
 *   0  idle
 *   1  G_SETTIMG seen                       (0x10029EB0)
 *   2  ... followed by G_RDPLOADSYNC        (0x10029F80)
 *   3  ... followed by G_LOADBLOCK          (0x10029FA0)
 *   4  ... followed by G_RDPTILESYNC        (0x1002A020)
 *   5  ... followed by G_SETTILE            (0x1002A040)
 *   6  ... followed by G_SETTILESIZE        (0x1002A140)
 *   7  G_SETTIMG then G_LOADTLUT            (0x10029F10)
 *   8  ... followed by G_RDPPIPESYNC        (0x1002A000)
 * State 0/3/6 re-arm on a fresh G_SETTIMG; only state 0 records the command
 * address, which is what makes the rewrite at 0x10029410 target the FIRST
 * command of the run. */
typedef struct BrGbiTexScan {
    int32_t     state;      /* 0x104D51AC */
    int32_t     timgSiz;    /* 0x104D51B4  (w0 >> 19) & 3 from G_SETTIMG */
    uint32_t    timgAddr;   /* 0x104D51BC   w1        from G_SETTIMG */
    uint32_t    srcSeen;    /* 0x10575434 */
    BrGfxWords *pRunStart;  /* 0x105553FC  first command of the run */
    BrGfxWords *pRunEnd;    /* 0x10575438  first command after it */
    int32_t     maxTile;    /* 0x10575430 */
    int32_t     f5544C;     /* 0x1057544C */
    int32_t     f575448;    /* 0x10575448 */
    int32_t     f575414;    /* 0x10575414 */
    int32_t     f575444;    /* 0x10575444 */
    int32_t     f575440;    /* 0x10575440 */
    int32_t     f5553E8;    /* 0x105553E8  (w0 >> 8)  & 7 from G_TEXTURE */
    int32_t     f5553E0;    /* 0x105553E0  (w0 >> 11) & 7 from G_TEXTURE */
    int32_t     f5553DC;    /* 0x105553DC */
    uint32_t    stageLen;   /* 0x105553EC  bytes the G_LOADBLOCK asked for */
    uint32_t    stageSrc;   /* 0x104C51A4 */
    uint8_t     aStage[BR_GBI_STAGE_SIZE];  /* 0x104C51A8 */
    uint8_t    *pTlutDst;   /* 0x100A7DF0 (a pointer variable, not an array) */
    uint8_t     prim[4];    /* 0x10555400, 0x10575410, 0x105553D8, 0x105551D0
                             * -- w1 bytes 3,2,1,0. Four unrelated globals. */
    uint8_t     env[4];     /* 0x104D51B0, 0x105551D4, 0x104D51A8, 0x104C51A0 */
    BrGbiTile   aTiles[BR_GBI_TILE_COUNT];  /* 0x105551D8 */
    /* DEVIATION: G_LOADTLUT and G_LOADBLOCK dereference timgAddr, a 32-bit
     * address. BrGbiTexScanRun goes through this hook so it works on a
     * 64-bit host; leave it NULL to get the original's raw reinterpretation
     * of the word as an address. */
    const void *(*pfnData)(void *pUser, uint32_t addr);
    void       *pUser;
} BrGbiTexScan;

/* 0x100290E0  Walk a display list, recognising texture-load runs. Stops at
 * G_ENDDL (0xB8) or immediately if pCmd is NULL. Clears five state globals
 * first. */
void BrGbiTexScanRun(BrGbiTexScan *pSt, BrGfxWords *pCmd);

/* Resolve `addr` the way BrGbiTexScanRun does; exposed so callers can stage
 * the same sources when they drive the leaves themselves. */
const void *BrGbiTexScanData(BrGbiTexScan *pSt, uint32_t addr);

/* 0x10029410  Close a run: register aStage and, if accepted, overwrite the
 * run's first command with `0xDC000000 | (id & 0xFFFFFF)` and its w1 with
 * the run length in 8-byte commands. Reached from G_VTX / G_TRI1 / G_TRI2. */
void BrGbiTexScanFlush(BrGbiTexScan *pSt, BrGfxWords *pCmd);
/* 0x10029E60  Remember the first command that ends the run. Default arm. */
void BrGbiTexScanMark(BrGbiTexScan *pSt, BrGfxWords *pCmd);
/* 0x10029E80  G_TEXTURE. */
void BrGbiTexScanTexture(BrGbiTexScan *pSt, const BrGfxWords *pCmd);
/* 0x10029EB0  G_SETTIMG. */
void BrGbiTexScanSetImg(BrGbiTexScan *pSt, BrGfxWords *pCmd);
/* 0x10029F10  G_LOADTLUT: copies ((lrs-uls)+1) * ((lrt-ult)+1) * 2 bytes
 * from timgAddr to pTlutDst. DEVIATION: the length is entirely data-driven
 * and, exactly as in the original, unchecked -- pTlutDst must be big enough.
 * The source is reached through the resolver below. */
void BrGbiTexScanLoadTlut(BrGbiTexScan *pSt, const BrGfxWords *pCmd,
                          const void *pSrc);
/* 0x10029F80  G_RDPLOADSYNC. */
void BrGbiTexScanLoadSync(BrGbiTexScan *pSt);
/* 0x10029FA0  G_LOADBLOCK: stages 2*((lrs-uls)+1) bytes from `pSrc`.
 * DEVIATION: clamped to BR_GBI_STAGE_SIZE; `stageLen` still records the
 * unclamped request so the value the original would have published is not
 * lost. A negative span (lrs < uls) yields a negative request, which the
 * original would have turned into a ~4GB rep movsd. */
void BrGbiTexScanLoadBlock(BrGbiTexScan *pSt, const BrGfxWords *pCmd,
                           const void *pSrc);
/* 0x1002A000  G_RDPPIPESYNC.  0x1002A020  G_RDPTILESYNC. */
void BrGbiTexScanPipeSync(BrGbiTexScan *pSt);
void BrGbiTexScanTileSync(BrGbiTexScan *pSt);
/* 0x1002A040  G_SETTILE.  0x1002A140  G_SETTILESIZE. */
void BrGbiTexScanSetTile(BrGbiTexScan *pSt, const BrGfxWords *pCmd);
void BrGbiTexScanSetTileSize(BrGbiTexScan *pSt, const BrGfxWords *pCmd);
/* 0x1002A1A0  G_SETOTHERMODE_L, only when w0 bits[15:8] == 0x03. */
void BrGbiTexScanOtherModeL(BrGbiTexScan *pSt, const BrGfxWords *pCmd);
/* 0x1002A210  G_SETOTHERMODE_H, w0 bits[15:8] of 0x0E or 0x11. */
void BrGbiTexScanOtherModeH(BrGbiTexScan *pSt, const BrGfxWords *pCmd);
/* 0x1002A250  the 0x0E arm of the above. */
void BrGbiTexScanOtherModeH0E(BrGbiTexScan *pSt, const BrGfxWords *pCmd);

/* ================================================================== */
/* 2b. Texture upload thunks                                          */
/* ================================================================== */

/* 0x10027C00  ceil(log2(n)) for n in 1..0x80, saturating at 8 and returning
 * 0 for every n <= 1 (SIGNED compares, so negatives give 0). */
int BrGbiSizeShift(int n);

/* 0x10028C70  Texels per 64-bit TMEM word by size code: 0->16, 1->8, 2->4,
 * anything else (including negatives) -> 2. */
int BrGbiTexelsPerWord(int siz);

/* 0x10028BF0  Thunk around the backend blit at 0x118AA0AC. It forwards its
 * fourteen arguments and INSERTS a fifteenth between a4 and a5:
 *
 *     ((1 << BrGbiSizeShift(a3)) / BrGbiTexelsPerWord(a5)) * 8
 *
 * i.e. the row pitch in bytes for a power-of-two-rounded width a3 at size
 * code a5. DEVIATION: the function pointer is a parameter (the original
 * reads the global) and the machine words are typed uintptr_t so that any
 * of them that is really a pointer survives on a 64-bit host. */
typedef void (*BrGbiBlitFn)(uintptr_t a1, uintptr_t a2, uintptr_t a3,
                            uintptr_t a4, uintptr_t pitch, uintptr_t a5,
                            uintptr_t a6, uintptr_t a7, uintptr_t a8,
                            uintptr_t a9, uintptr_t a10, uintptr_t a11,
                            uintptr_t a12, uintptr_t a13, uintptr_t a14);
void BrGbiBlit(BrGbiBlitFn pfn,
               uintptr_t a1, uintptr_t a2, uintptr_t a3, uintptr_t a4,
               uintptr_t a5, uintptr_t a6, uintptr_t a7, uintptr_t a8,
               uintptr_t a9, uintptr_t a10, uintptr_t a11, uintptr_t a12,
               uintptr_t a13, uintptr_t a14);

/* The backend texture constructor at 0x118AA0B0, shared by 0x1002A280 and
 * 0x1002A740. Fourteen arguments; the port makes it a parameter. */
typedef void *(*BrGbiTexCreateFn)(void *pSrc, uintptr_t a2,
                                  uint32_t w, uint32_t h,
                                  uint32_t fmt, uint32_t siz,
                                  uint32_t b31, uint32_t b30,
                                  uint32_t b29, uint32_t b28,
                                  uint32_t a11, uint32_t a12,
                                  uint32_t a13, uintptr_t a14);

/* The record 0x1002A280 works on. Field offsets in the original are quoted;
 * they are not reproduced literally because pTex widens on a 64-bit host. */
typedef struct BrGbiTexRec {
    void    *pTex;    /* +0x00  in and out */
    uintptr_t f04;    /* +0x04 */
    uint16_t w;       /* +0x0C */
    uint16_t h;       /* +0x0E */
    uint32_t flags;   /* +0x20 */
} BrGbiTexRec;

/* 0x1002A280  (Re)create pRec->pTex unless pTex is already NULL or
 * flags bit 20 is set -- both are early-outs, so a NULL pTex is NEVER
 * filled in by this routine.
 *
 * flags bits[27:24] pick a (fmt, siz) pair:
 *     0x1 -> (0, 2)     0x4 -> (1, 4)     anything else -> (2, 0)
 * and bits 31,30,29,28 are passed on individually. Width and height are
 * rounded up to powers of two via BrGbiSizeShift. */
void BrGbiTexCreate(BrGbiTexCreateFn pfn, BrGbiTexRec *pRec, uintptr_t a2);

/* 0x1002A740  Build the 4x4 solid placeholder texture.
 *
 * The 16 bytes at 0x104D51C0 are filled with 0x20 when the mode global
 * 0x1022B350 is 2 or 3, and with 0x80 otherwise, then handed to the same
 * constructor as (w=4, h=4, fmt=1, siz=4, 0,0,1,1, 0,0,1, 0).
 *
 * GOTCHA: the fill loop starts at 0x104D51C1 and writes [eax-1]..[eax+2],
 * so it really does cover 0x104D51C0..0x104D51CF -- sixteen bytes, not
 * seventeen. */
typedef struct BrGbiSolidTex {
    int32_t  mode;        /* 0x1022B350 */
    uint8_t  aTexels[16]; /* 0x104D51C0 */
    void    *pTex;        /* 0x105553E4 */
} BrGbiSolidTex;
void BrGbiSolidTexBuild(BrGbiTexCreateFn pfn, BrGbiSolidTex *pSt);

/* ================================================================== */
/* 3. Screen wipe / fade                                              */
/* ================================================================== */

/* .rdata constants used throughout this group, read out of the DLL:
 *   0x1008F410 = 0.0f    0x1008F414 = 0.1f    0x1008F418 = 0.7f
 *   0x1008F41C = 255.0f  0x1008F420 = 1.0f    0x1008F430 = -1.0f
 *   0x1008F428 = 0.0  (double)   0x1008F438 = 255.0 (double) */

typedef struct BrFadeState {
    /* --- display-list emission --- */
    BrGfxWords *pCmd;        /* 0x106C0680  write cursor, bumped by 8 */
    int32_t     span;        /* 0x106C0684 */
    uint32_t    otherModeH;  /* 0x106C0688 */
    int32_t     width;       /* 0x106C299C */
    int32_t     shift;       /* 0x106C65E4 */
    int32_t     parity;      /* 0x106C65EC */
    int32_t     rectIdx;     /* 0x106C5708 */
    float       dt;          /* 0x106C2CFC */
    /* --- wipe --- */
    int32_t     pos;         /* 0x10575500 */
    int32_t     f5754FC;     /* 0x105754FC */
    int32_t     pos2;        /* 0x10575508 */
    int32_t     f57550C;     /* 0x1057550C */
    float       target;      /* 0x10575510 */
    float       rate;        /* 0x10575514 */
    float       value;       /* 0x10575518 */
    int32_t     kick;        /* 0x10575524 */
    int32_t     bounce;      /* 0x10575530 */
    int32_t     aPos2[2];    /* 0x105754F0, indexed by parity */
    int32_t     aPos[2];     /* 0x105754E8, indexed by parity */
    int32_t     bars;        /* 0x100A81BC */
    /* --- two independent 0..1 ramps --- */
    float       rateA;       /* 0x10575520 */
    float       curA;        /* 0x100A81B8 */
    float       tgtA;        /* 0x100A81B4 */
    int32_t     kickA;       /* 0x1057552C */
    uint8_t     outA;        /* 0x100BBAE4 */
    float       rateB;       /* 0x1057551C */
    float       curB;        /* 0x100A81B0 */
    float       tgtB;        /* 0x100A81AC */
    int32_t     kickB;       /* 0x10575528 */
    uint8_t     outB;        /* 0x100BBADC */
    /* --- 0x1002AEA0 / 0x1002AEC0 --- */
    int32_t     refCount;    /* 0x105754E0 */
    void      (*pfnRelease)(void);  /* 0x10575474 */
    int32_t     srcC0;       /* 0x100A81C0 */
    int32_t     srcC4;       /* 0x100A81C4 */
} BrFadeState;

/* NOTE (cross-slice): slice1_05.h models the SAME two globals 0x10575510 and
 * 0x10575518 as BrCursorPair, a pair of void *. Everything in this packet
 * reads them as floats (fld / fcomp / fstp). The two views cannot both be
 * right; 0x1002B280 -- which stores one value into both -- is consistent
 * with either, so the float reading here should be taken as the stronger
 * evidence. Flagged rather than silently changed. */

/* 0x1002AEA0  Decrement refCount and call pfnRelease when it reaches zero.
 * Always returns 1. GOTCHA: the test is `== 0` after the decrement, so a
 * refCount that is already 0 wraps to -1 and never fires again. */
int BrFadeRelease(BrFadeState *pSt);

/* 0x1002AEC0  pos = srcC0; f5754FC = srcC4. The function begins with a
 * `jmp` over eleven nops -- a patch pad, reproduced only as this comment. */
void BrFadeLatch(BrFadeState *pSt);

/* 0x1002AF10  Emit the sprite pass for one rectangle record.
 *
 * Returns immediately if `alpha` < 0.1f; clamps `alpha` down to 0.7f when it
 * exceeds 0.7f. `pRecs` is an array of 88-byte records, index `rectIdx`; the
 * four dwords at +0x00, +0x04, +0x08 and +0x0C are combined into a pair of
 * 0xE1-tagged commands. */
#define BR_FADE_RECT_DWORDS 22   /* 88 bytes */
void BrFadeDrawSprite(BrFadeState *pSt, const uint32_t *pRecs, float alpha);

/* 0x1002B130  Aim the wipe at `to` with speed `over`.
 * 0x1002B1C0 / 0x1002B220  Aim ramp A / ramp B at `to` with speed `over`.
 * GOTCHA: all three divide a CONSTANT by `over` (1.0f when moving forward,
 * -1.0f when moving back); `over` is a duration, not a rate, and a zero
 * `over` yields an infinity.
 *
 * GOTCHA: 0x1002B130's backward path first checks for a bounce, and its
 * guard is `value != 1.0f && rate > 0.0f`. Reading that test the other way
 * round -- which the `test ah,0x40 / jne` pairing invites -- inverts it. */
void BrFadeSetTarget(BrFadeState *pSt, float to, float over);
void BrFadeSetTargetA(BrFadeState *pSt, float to, float over);
void BrFadeSetTargetB(BrFadeState *pSt, float to, float over);

/* 0x1002B2A0  rate < 0 or bounce set.
 * 0x1002B2D0  value == target and bounce clear.
 * 0x1002B300  rate < 0 and value == 0 and bounce clear. */
int BrFadeIsClosing(const BrFadeState *pSt);
int BrFadeIsSettled(const BrFadeState *pSt);
int BrFadeIsShut(const BrFadeState *pSt);

/* 0x1002B340  Emit the wipe bars. Does nothing while value == 1.0f. */
void BrFadeDrawBars(BrFadeState *pSt);

/* 0x1002B670  Advance the wipe and both ramps by one frame and publish the
 * two 0..255 outputs.
 *
 * GOTCHA: the wipe's overshoot test and the ramps' overshoot tests differ at
 * EQUALITY. The wipe clamps (and fires the bounce) when value >= target;
 * each ramp leaves cur alone when cur == tgt. Reproduced. */
void BrFadeTick(BrFadeState *pSt);

/* ================================================================== */
/* 4. .rca byte-swap and fixup helpers                                */
/* ================================================================== */

/* 0x1002B930  Copy 32 bytes. Argument order is (destination, source). */
void BrCopy8Words(void *pDst, const void *pSrc);

/* 0x1002B9C0  Reset both counters that 0x1002B9A0 leaves alone: the vertex
 * cache's entry count (0x1067B54C) and the pointer list's count
 * (0x1067B548), both owned by slice1_05. */
void BrRcaResetCounts(BrVtxCache *pCache, BrPtrList *pList);

/* 0x1002B9E0  Byte-swap `count` u16s in place.  DEFINED IN br_bits.c, which
 * also carries BRGlide's 0x10018A50 for it -- br_track.c had its own copy of
 * the same 29 bytes.  Declared there; re-declared here only so this header
 * stays a complete index of the range. */
#include "br_bits.h"
/* 0x1002BA20  Byte-swap the four u16s of one 8-byte record.
 * 0x1002BA00  ... over an array of them. */
void BrSwapU16x4(void *pv);
void BrSwapU16x4Array(void *pv, int count);
/* 0x1002BA60  BrSwapVec3 (0x100383C0) over an array of 12-byte records. */
void BrSwapVec3Array(void *pv, int count);

/* Everything 0x1002BAA0 needs that the original reads from globals.
 *
 * DEVIATION: the two 32-bit values the record carries at +0x00 and +0x04 are
 * dereferenced by the original after BrSegFixup has turned them into host
 * addresses. A 32-bit value cannot be a host address here, so the port
 * routes both through `pfnResolve`, which is handed the fixed-up word and
 * returns the memory it names (or NULL). Pass a resolver that adds the same
 * offsets BrSegFixup used and this behaves exactly like the original. */
typedef void *(*BrRcaResolveFn)(void *pUser, uint32_t addr);

typedef struct BrRcaFixup {
    BrSegMap      *pSeg;        /* passed on to BrSegFixup */
    int32_t        enable;      /* 0x10675540; 0 skips all the copying */
    uint8_t       *pBlob;       /* 0x10690BEC */
    BrRcaResolveFn pfnResolve;
    void          *pUser;
} BrRcaFixup;

/* The 0x24-byte record 0x1002BAA0 rewrites. */
#define BR_RCA_REC_SIZE 0x24

/* 0x1002BAA0  Byte-swap a record and pull its two payloads in.
 *
 * Byte-swaps u32s at +0x00, +0x04, +0x08 and +0x20 and u16s at +0x0C, +0x0E,
 * +0x10, +0x12, +0x14, +0x16, +0x24-relative nothing else; bytes 0x18..0x1F
 * are left alone. +0x00 and +0x04 are then segment-fixed.
 *
 * With bit 20 of the swapped +0x20 word SET, +0x08 is also segment-fixed and
 * treated as a pointer to a mesh header, which 0x1002BC90 byte-swaps; the
 * header's entry 0 (or entry 1, when its +0x02 count is 2 and its +0x08 word
 * is -1) supplies two blob offsets. With bit 20 CLEAR, +0x08 is instead a
 * 12-bit index scaled by 32 into the blob.
 *
 * The second payload's length is 0x20 bytes when the swapped +0x20 word's
 * bits[27:24] are exactly 0x1, and 0x200 otherwise -- an `neg / sbb` idiom,
 * so it really is an equality test and not a comparison. */
void BrRcaFixupRecord(const BrRcaFixup *pCtx, void *pRec);
/* 0x1002BA80  ... over an array of them. */
void BrRcaFixupArray(const BrRcaFixup *pCtx, void *pv, int count);

/* 0x1002BC90  Byte-swap a mesh header in place: the u16 at +0x02 (the entry
 * count), the u32 at +0x04, then three u32s per entry at +0x08, +0x0C and
 * +0x10 with a stride of 12. Bytes +0x00 and +0x01 are untouched, and the
 * loop bound is re-read from +0x02 on every iteration. */
void BrRcaSwapMesh(void *pv);

#endif /* SLICE2_16_H */
