/* slice1_05.h -- Boss Rally (BRD3D.dll) decompilation, a later pass.
 *
 * Address range 0x1002B280 .. 0x100360F0.
 *
 * Six unrelated clusters ended up in this range:
 *
 *   1. the N64 vertex cache          0x1002BD50 0x1002BDD0 0x1002BE30 0x1002BF00
 *   2. F3DEX display-list patching   0x1002C150 0x1002C190 0x1002C1B0
 *   3. the RDP colour combiner       0x1002F900 0x1002FAC0 0x1002FAF0
 *   4. 4x4 matrix helpers            0x100306C0 0x10031140
 *   5. assorted setters / lists      0x1002B280 0x1002C1F0 0x1002F460 0x10034C32..
 *   6. the peer table                0x10035FE0 0x10036030
 *
 * General deviations that apply to the whole file (each is repeated at the
 * site where it matters):
 *
 *   - The original addresses its state through fixed globals. Following the
 *     convention already set by br_seg.h / br_pool.h / br_span.h, that state
 *     is passed in as a struct instead. The global address of every field is
 *     recorded in a comment so the mapping is not lost.
 *   - The x87 computes at 53-bit precision here, NOT 80-bit -- the CRT's
 *     control word is 0x027F, see CONVENTIONS.md. This entry used to claim
 *     80-bit and that "nothing here reproduces that", which turned a
 *     reproducible property into a blanket exemption. A C `double` models an
 *     unspilled intermediate exactly; a value the original stores to a 32-bit
 *     slot is rounded to `float` at that point and nowhere else. The
 *     original's summation ORDER is preserved.
 *   - Field names are positional (fNN = offset 0xNN) wherever the meaning
 *     could not be established from the code.
 */
#ifndef SLICE1_05_H
#define SLICE1_05_H

#include <stdint.h>

#include "br_mat.h"     /* BrMat4 */
#include "br_seg.h"     /* BrSegMap, BrSegFixup */

/* ------------------------------------------------------------------ */
/* A pair of display-list command words. N64 Gfx commands are 8 bytes;
 * by the time these routines see them the payload has already been
 * byte-swapped to host order, so w0/w1 are plain host u32s and the
 * per-byte routines below index bytes in LITTLE-endian positions.      */
typedef struct BrGfxWords { uint32_t w0, w1; } BrGfxWords;

/* ================================================================== */
/* 1. N64 vertex cache                                                */
/* ================================================================== */

/* An N64 Vtx is 16 bytes:
 *      +0x00 s16 x   +0x02 s16 y   +0x04 s16 z   +0x06 u16 flag
 *      +0x08 s16 s   +0x0A s16 t
 *      +0x0C s8      +0x0D s8      +0x0E s8      +0x0F s8   (never read)
 * The expanded form is 8 floats (32 bytes): x, y, z, s, t and the three
 * signed bytes scaled by 1/128. */
#define BR_VTX_SRC_SIZE      16
#define BR_VTX_OUT_FLOATS     8

/* 0x1008F440 = 0x3C000000 = 1/128, read out of the DLL's .rdata. It is the
 * exact inverse of the 128.0 that BrPackNormalByte (br_vecd.h, 0x10030DE0)
 * multiplies by, so these two are an encode/decode pair. */
#define BR_VTX_NORMAL_SCALE  0.0078125f

#define BR_VTX_CACHE_MAX     0x800

typedef struct BrVtxCacheEntry {
    void  *pSrc;    /* +0x00 of the record, table base 0x10675548 */
    int    count;   /* +0x04 */
    float *pOut;    /* +0x08 */
} BrVtxCacheEntry;

typedef struct BrVtxCache {
    BrVtxCacheEntry aEntries[BR_VTX_CACHE_MAX]; /* 0x10675548, 12 bytes each */
    int             nEntries;                   /* 0x1067B54C */
    float          *pCursor;                    /* 0x100A81C8 -- write cursor */
    uint32_t        nVerts;                     /* 0x1067D550 -- running total */
} BrVtxCache;

/* 0x1002BDD0  Byte-swap `count` 16-byte vertices in place.
 *
 * Swaps the SIX u16s at offsets 0x00..0x0B and leaves 0x0C..0x0F alone --
 * the trailing four bytes are per-byte data and need no swap. The count is
 * fixed at six shorts by full unrolling in the original, so this is a Vtx
 * swapper specifically, not a general byte-swap loop.
 *
 * count <= 0 is a no-op. */
void BrVtxSwap(void *pVerts, int count);

/* 0x1002BE30  Expand `count` N64 vertices into 8-float records at the
 * cache's write cursor. Returns the cursor value from BEFORE the call (the
 * start of the emitted block) and advances it by 8 floats per vertex; also
 * adds `count` to pCache->nVerts.
 *
 * The u16 at source offset 0x06 (the Vtx flag) and the s8 at 0x0F are
 * dropped -- the expanded record is x,y,z,s,t,n0,n1,n2.
 *
 * count <= 0 returns the unchanged cursor and touches nothing.
 *
 * The source is read little-endian byte-by-byte rather than by struct
 * overlay, which reproduces exactly what the x86 original does on any host. */
float *BrVtxExpand(BrVtxCache *pCache, const void *pVerts, int count);

/* 0x1002BF00  Append (pSrc, count) -> pOut to the cache table.
 *
 * GOTCHA: at BR_VTX_CACHE_MAX entries the insert is silently DROPPED -- no
 * error, no eviction. Past that point every lookup misses and every request
 * re-converts, which also means the source data gets byte-swapped again
 * (see BrVtxCacheResolve). */
void BrVtxCacheInsert(BrVtxCache *pCache, void *pSrc, int count, float *pOut);

/* 0x1002BD50  Replace *ppVerts (an N64 vertex block) with the expanded
 * float block, converting and registering it on a miss.
 *
 * The cache key is the PAIR (pointer, count); the same pointer with a
 * different count is a miss.
 *
 * GOTCHA: on a miss the source block is byte-swapped IN PLACE. If the same
 * memory is ever resolved twice under different counts -- or after the table
 * has filled up -- it gets swapped a second time and is corrupted. The
 * original has no guard against this.
 *
 * GOTCHA: the guard on count is `count != 0`, not `count > 0`. A negative
 * count therefore reaches the miss path, converts nothing, and installs a
 * table entry whose count is negative. Preserved.
 *
 * Both bail-outs (*ppVerts == NULL, count == 0) leave *ppVerts untouched. */
void BrVtxCacheResolve(BrVtxCache *pCache, void **ppVerts, int count);

/* ================================================================== */
/* 2. F3DEX display-list patching                                     */
/* ================================================================== */

/* 0x1002C150  Patch the address word of an F3DEX G_VTX command and return
 * the vertex count from w0.
 *
 *   w1 = (segment base & 0xFF000000) | (w1 & 0x00FFFFFF)   -- the original
 *        does this as base ^ ((base ^ w1) & 0xFFFFFF), same thing
 *   BrSegFixup(map, &w1)                                   -- 0x1002B970
 *   return (w0 >> 10) & 0x3F
 *
 * GOTCHA: the prefix is merged BEFORE BrSegFixup runs, so a zero w1 does not
 * survive as null -- it becomes the segment base and then rebases to
 * hostBase. BrSegFixup's "null stays null" rule is unreachable from here.
 *
 * `(w0 >> 10) & 0x3F` is the F3DEX (not plain F3D) G_VTX vertex count:
 * F3DEX packs w0 as G_VTX<<24 | n<<10 | ((v0+n)*2)<<1. This pins the
 * microcode variant, which br_f3d.h could only guess at.
 *
 * DEVIATION: the original continues straight into
 *      BrVtxCacheResolve(cache, (void **)&w1, n)
 * i.e. it keeps the host vertex pointer in the same 32-bit w1 slot. A host
 * pointer does not fit in 32 bits, so that final step is left to the caller:
 *
 *      n = BrF3DVtxFixup(map, cmd);
 *      pv = <host memory for cmd->w1>;
 *      BrVtxCacheResolve(cache, &pv, (int)n);
 */
unsigned BrF3DVtxFixup(const BrSegMap *pMap, BrGfxWords *pCmd);

/* 0x1002C190  G_TRI1: halve the three vertex-index bytes at BYTE offsets
 * 4, 5, 6 (the low three bytes of w1). F3DEX stores indices pre-multiplied
 * by 2; this undoes that. The shift is logical, so an odd byte rounds down. */
void BrF3DTri1Fixup(void *pCmd);

/* 0x1002C1B0  G_TRI2: the same halving applied to byte offsets 0, 1, 2 (low
 * three bytes of w0) and then 4, 5, 6 (low three bytes of w1). */
void BrF3DTri2Fixup(void *pCmd);

/* ================================================================== */
/* 3. RDP colour combiner                                             */
/* ================================================================== */

/* 0x1002FAF0 / 0x1002FAC0  Translate one combiner token into an RDP mux
 * code. The engine's tokens are:
 *
 *      0            the literal 0
 *      1            the literal 1
 *      1000 + n     mux source n, numbered as G_CCMUX_*
 *
 * The two tables differ because the RDP encodes "0" and "1" differently in
 * the colour and alpha muxes:
 *
 *      token   colour (0x1002FAF0)   alpha (0x1002FAC0)
 *      0       31  (G_CCMUX_0)       7  (G_ACMUX_0)
 *      1        6  (G_CCMUX_1)       6  (G_ACMUX_1)
 *      1013    13  (LOD_FRACTION)    0  <-- special-cased
 *      other   token - 1000          token - 1000
 *
 * GOTCHA: 1013 is the ONLY value with an asymmetric mapping. LOD_FRACTION is
 * 13 in the colour mux but 0 in the alpha mux, and 13 does not fit the
 * alpha mux's 3 bits, so the alpha converter carries a dedicated branch for
 * it. Anything else out of range is passed through unmasked and gets
 * truncated by the packer.
 *
 * GOTCHA: no validation. An unrecognised token below 1000 yields a large
 * negative number, which the packer then truncates to whatever fits. */
int BrRdpCCMux(int token);
int BrRdpACMux(int token);

/* 0x1002F900  Build a G_SETCOMBINE command from sixteen tokens.
 *
 * The argument order is libultra's gsDPSetCombineLERP, recovered field by
 * field from the shift chain, and every one of the sixteen is routed through
 * the colour or the alpha converter exactly as its RDP slot requires:
 *
 *   w0: a0[23:20] c0[19:15] Aa0[14:12] Ac0[11:9] a1[8:5] c1[4:0]
 *   w1: b0[31:28] b1[27:24] Aa1[23:21] Ac1[20:18] d0[17:15] Ab0[14:12]
 *       Ad0[11:9] d1[8:6] Ab1[5:3] Ad1[2:0]
 *
 * GOTCHA: the 0xFC command byte is never OR'd in explicitly. The chain seeds
 * w0 with `(a0 & 0xF) | 0xFFFFFFC0` and then shifts left by 20 in total, so
 * the top byte falls out of the ones-fill. Reproduced as written rather than
 * "cleaned up" -- the two are only equal because the total shift is exactly
 * 20.
 *
 * GOTCHA: b0 is the one field NOT masked before shifting; it relies on the
 * 28-bit shift to discard the excess. */
void BrRdpSetCombineLERP(BrGfxWords *pOut,
                         int a0,  int b0,  int c0,  int d0,
                         int Aa0, int Ab0, int Ac0, int Ad0,
                         int a1,  int b1,  int c1,  int d1,
                         int Aa1, int Ab1, int Ac1, int Ad1);

/* ================================================================== */
/* 4. 4x4 matrix helpers                                              */
/* ================================================================== */

/* 0x100306C0  pOut = pA * pB, row-major, full 4x4 (unlike br_mat.h's
 * BrMat4MulVec3 pair this one does use the fourth row and column).
 *
 * Argument order is (a, b, out) -- destination LAST, the opposite of every
 * routine in br_vec.h. Preserved.
 *
 * pOut may alias either input: the original detects that and computes into a
 * 0x40-byte stack buffer, then copies 16 dwords out.
 *
 * GOTCHA: only pA and pB are null-checked. pOut == NULL with valid inputs
 * writes through a null pointer, exactly as the original does.
 *
 * GOTCHA: the two code paths add the four products in DIFFERENT orders --
 * the direct path does ((a2*b2 + a3*b3) + a0*b0) + a1*b1, the aliased path
 * does ((a3*b3 + a1*b1) + a0*b0) + a2*b2. Both orders are reproduced, since
 * float addition is not associative and the choice is observable. */
void BrMat4Mul(const BrMat4 *pA, const BrMat4 *pB, BrMat4 *pOut);

/* 0x10031140  Build a translation matrix: identity with row 3 = (tx,ty,tz,1).
 * Row-vector convention, matching BrMat4Frustum and BrMat4Scale in br_mat.h
 * (0x100310F0, immediately before this one). */
void BrMat4Translate(BrMat4 *pM, float tx, float ty, float tz);

/* ================================================================== */
/* 5. Assorted setters, lists and lookups                             */
/* ================================================================== */

/* 0x1002B280  Store one value into BOTH fields.
 * The two globals sit 8 bytes apart (0x10575510 and 0x10575518) with an
 * unrelated dword between them; they are packed here. */
typedef struct BrCursorPair {
    void *f10;   /* 0x10575510 */
    void *f18;   /* 0x10575518 */
} BrCursorPair;
#ifdef BR_MATCHING_BUILD
/* The original takes ONE argument and stores it to the two absolute addresses
 * directly -- there is no pair object and no second parameter.  BrCursorPair
 * is a port-side convenience that packs two globals 8 bytes apart into
 * adjacent fields, which is why the port emits [ecx] / [ecx+4] where the
 * original emits two absolute stores. */
extern void *g_brCursor575510;   /* 0x10575510 */
extern void *g_brCursor575518;   /* 0x10575518 */
void BrCursorPairSet(void *pv);
#else
void BrCursorPairSet(BrCursorPair *pPair, void *pv);
#endif

/* 0x1002C1F0  Append a pointer to a flat list.
 * GOTCHA: identical silent-drop-when-full behaviour to the vertex cache; the
 * capacity is again 0x800. This is a DIFFERENT list from the vertex cache
 * (count at 0x1067B548, array at 0x1067B550) even though the two counters
 * are adjacent in memory. */
#define BR_PTRLIST_MAX 0x800
typedef struct BrPtrList {
    int    n;                      /* 0x1067B548 */
    void  *ap[BR_PTRLIST_MAX];     /* 0x1067B550 */
} BrPtrList;
void BrPtrListAdd(BrPtrList *pList, void *pv);

/* 0x1002F460  Two-output table lookup.
 *
 * index = f04 * 12 + f05, into a table of 2-byte records (0x100B3820).
 * Record byte 0 becomes output A (0x100B380C), byte 1 output B (0x104BBE08).
 *
 * If bit 0 of f00 is set, output A is folded by six: a >= 6 ? a - 6 : a + 6.
 * Twelve entries per row and a fold by six make this a half-turn on a
 * 12-position dial, but the dial's meaning is not established.
 *
 * GOTCHA: output B is computed from a FRESHLY recomputed index, so the fold
 * never reaches it. The original literally reloads f04/f05 and redoes the
 * multiply rather than reusing the register.
 *
 * GOTCHA: the index is not bounds-checked at all. */
typedef struct BrSelInput {
    unsigned char f00;   /* bit 0 selects the fold */
    unsigned char f01, f02, f03;
    unsigned char f04;
    unsigned char f05;
} BrSelInput;
void BrSelLookup(const BrSelInput *pIn, const unsigned char (*aTable)[2],
                 int *pOutA, int *pOutB);

/* 0x10034C32 .. 0x10034CA8  A run of tiny accessors over six globals.
 *
 * GOTCHA -- IMPORTANT FOR THE WHOLE RANGE: two of the callees in this area
 * are stubs in the shipped DLL.
 *      0x10008B80  is a bare `ret` (1 byte), yet it is called with 1, 3 and
 *                  6 arguments from 0x10034812.
 *      0x100378A0  is `mov dword ptr [0x106C7C44], 1 / ret` (11 bytes). It
 *                  is called as (dst, src, 4) and ignores all three.
 * So BrHookTakeA/B below look like "copy 4 bytes then read the global" but
 * in this build they only set a flag. That is faithful; the copy semantics
 * the call sites were written against are not recoverable from this binary.
 */
/* THIS STRUCT IS A PORT-SIDE GATHERING, NOT AN OBJECT THE GAME HAS.  Every
 * one of these accessors writes a FIXED GLOBAL (`mov [0x106C1608],eax`), not
 * a member through a `this`, so nothing in the original ties the six
 * addresses together.  It is kept because the call sites are written against
 * it; do not read it as evidence about the game's layout.
 *
 * `pfnC` HAS BEEN REMOVED from it. 0x106C0964 is the same original dword as
 * BRGlide 0x106E79F4 -- shared.csv pairs its three accessors 0x10034C51 /
 * 0x10034C66 / 0x10034C73 with 0x1002E302 / 0x1002E317 / 0x1002E324 as
 * byte-identical -- and br_gamestep.c owns it. Modelling it here as well made
 * three host objects for one slot. */
typedef struct BrHooks {
    void  *pfA;             /* 0x106C198C */
    void  *pfB;             /* 0x106C1608 */
    uint32_t g0938;         /* 0x106C0938 */
    uint32_t g3300;         /* 0x106C3300 */
    int      f7C44;         /* 0x106C7C44 -- set to 1 by the stub 0x100378A0 */
} BrHooks;

void BrHookNopA(void);                                /* 0x10034C32, empty */
void BrHookSetA(BrHooks *pH, void *pv);               /* 0x10034C37 */
void BrHookSetB(BrHooks *pH, void *pv);               /* 0x10034C44 */
/* 0x10034C66 / 0x10034C73  The game-step slot's setter and invoker. BOTH
 * IGNORE pH -- the originals address 0x106C0964 directly and the setter takes
 * one plain cdecl argument -- and both forward to br_gamestep.c, which holds
 * the one body and the one copy of the storage. The parameter survives only
 * so the call sites read unchanged.
 *
 * GOTCHA, CORRECTED: this header used to say "no null check in the original",
 * which is true of the original and no longer true of the port. The surviving
 * body returns without calling when nothing is installed, so the harness can
 * report an empty slot instead of faulting. */
void BrHookSetC(BrHooks *pH, void (*pfn)(void));
void BrHookCallC(const BrHooks *pH);
void BrHookNopB(void);                                /* 0x10034C83, empty */
/* 0x10034C88  The original passes (&g0938, (char*)pSrc + 4, 4) to the stub.
 * 0x10034CA8  The original passes (&g3300, pSrc, 4) to the stub.
 * Both then return the global. pSrc is unused here for the reason above. */
uint32_t BrHookTakeA(BrHooks *pH, const void *pSrc);
uint32_t BrHookTakeB(BrHooks *pH, const void *pSrc);

/* ================================================================== */
/* 6. Peer table                                                      */
/* ================================================================== */

/* The table lives at 0x11786828: sixteen records of 0x96C bytes, ending at
 * 0x1178FEE8. Only three fields are touched by the routines below. */
#define BR_PEER_COUNT   16
#define BR_PEER_STRIDE  0x96C
#define BR_PEER_STATE_MASK 0x3Fu

typedef struct BrPeer {
    uint32_t      hMutex;        /* +0x000 -- Win32 mutex handle in the original */
    uint32_t      f04;           /* +0x004 -- the id BrPeerFind matches on */
    unsigned char pad08[0x24];
    uint32_t      f2C;           /* +0x02C -- low 6 bits are the state */
    unsigned char pad30[0x93C];
} BrPeer;

/* 0x10036030  Find the table index for `id`.
 *
 *   id == 1                     -> 0        (record 0 is reserved; the scans
 *                                            below never look at it)
 *   an in-use record with f04 == id -> its index, 1..15
 *   otherwise the first free record  -> its index, 1..15
 *   nothing free                -> -1
 *
 * "in use" is (f2C & 0x3F) != 0; "free" is (f2C & 0x3F) == 0. The original
 * compares only the low byte, which is the same test since the mask is 0x3F.
 *
 * GOTCHA: the return value is an index where 0, -1 and 1..15 all mean
 * different things, and 0 is reachable ONLY through the id == 1 shortcut.
 * A caller that treats 0 as "not found" is wrong.
 *
 * DEVIATION: the original takes and releases each record's mutex around
 * every probe (WaitForSingleObject / ReleaseMutex). Those calls do not
 * affect the value returned and are dropped here; a threaded port has to put
 * equivalent locking back. */
int BrPeerFind(const BrPeer *aPeers, uint32_t id);

/* 0x10035FE0  thiscall. Reset three fields of a 348-byte record and cache
 * its own array index plus a pointer into a parallel 6-byte-record array.
 *
 * The original recovers the index by subtracting the array base 0x106C6678
 * and dividing by 348 -- as a signed multiply by 0x2F149903 followed by
 * `sar 6` plus the usual sign fixup, which is exactly a divide by 348. 348
 * is 0x15C, and +0x154 / +0x158 are the record's last two dwords, which is
 * the cross-check that the stride really is the record size.
 *
 * DEVIATION: the index is computed as a C pointer difference instead, so the
 * struct below does not have to be exactly 348 bytes on a 64-bit host. */
typedef struct BrEntRec { unsigned char b[6]; } BrEntRec;  /* 0x106C65A0 */

typedef struct BrEnt {
    unsigned char pad000[0x2C];
    uint32_t      f2C;
    uint32_t      f30;
    unsigned char pad034[0x10];
    uint32_t      f44;
    unsigned char pad048[0x10C];
    int32_t       f154;      /* own index */
    BrEntRec     *f158;      /* &pRecs[f154] */
} BrEnt;

/* Glide storage: pad blocks at 0x106ED708 (stride 0x15C), records at
 * 0x106ED630 (stride 6).  The original passes only the entity -- in ECX,
 * one register arg == __fastcall exactly (see thiscall-via-fastcall). */
extern BrEnt    g_aBrEnts[];      /* 0x106ED708 */
extern BrEntRec g_aBrEntRecs[];   /* 0x106ED630 */

void __fastcall BrEntInit(BrEnt *pEnt);

#endif /* SLICE1_05_H */
