/* slice1_01.h -- BRD3D.dll 0x10001000-0x10004910, agent 01.
 *
 * What survived triage from this range:
 *
 *   0x10001000  zlib adler32 (verbatim, inlined into the DLL)
 *   0x10002A20  the CD-audio volume rescale (the only portable part of it)
 *   0x10002DE0  64x64 grid sample, returns base+delta packed in one dword
 *   0x10002EF0  u16 table cursor: read-and-advance
 *   0x10003460  elapsed milliseconds -> 30 Hz tick count
 *   0x100030E0  FCHK_FRead   ) named from their own error strings, which are
 *   0x10003170  CHK_FRead    ) still in .rdata at 0x10094158 / 0x10094198 /
 *   0x10003320  CHK_FileExists  0x10094204 / 0x10094218 / 0x10094254
 *   0x10003390  CHK_AllocateMemory
 *   0x100033F0  CHK_ReAllocateMemory
 *
 * The rest of the range is the MCI/`cdaudio` music module and the network
 * command marshaller; both are Win32- and SEH-bound. See the report.
 *
 * House style note: where the original reached for a fixed global (a table
 * base, a time origin), that global becomes an explicit parameter here, the
 * same way br_seg.h and br_pool.h handle theirs. Each such spot is marked
 * DEVIATION in the .c.
 */
#ifndef SLICE1_01_H
#define SLICE1_01_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/* ---------------------------------------------------------------- 0x10001000
 * zlib's adler32, compiled into the DLL with DO16 unrolling, NMAX = 0x15B0
 * (5552) and BASE = 0xFFF1 (65521). Bit-identical to zlib.
 *
 * Reserved input: pBuf == NULL returns 1 (the zlib seed) and ignores `adler`
 * entirely -- that is how callers ask for the initial value. len == 0 instead
 * returns `adler` unchanged, so the two "empty" cases differ.
 */
unsigned long BrAdler32(unsigned long adler, const unsigned char *pBuf,
                        unsigned int len);

/* ---------------------------------------------------------------- 0x10002A20
 * (10000 * (vol & 0xFF)) / 255  -- maps an 8-bit volume onto the 0..10000
 * range the audio backend is initialised with at 0x10002260 (the 0x2710 push).
 *
 * This is the whole of 0x10002A20 that is not a call through the backend
 * function-pointer table at 0x10575454; the rest of that function is not
 * reproduced here. The original masks to 8 bits FIRST, so 256 maps to 0, not
 * to full volume.
 */
int BrCdVolumeScale(int vol);

/* ---------------------------------------------------------------- 0x10002DE0
 * Sample a 64x64 grid of u16 at world position (x, y) and return the cell
 * value together with the step to its right-hand neighbour, packed:
 *
 *     bits  0..15 : grid[idx]                  (call it t0)
 *     bits 16..31 : (grid[idx+1] - t0) & 0xFFFF
 *
 * idx = (row << 6) + col, col from x, row from y, each floor(v / 32).
 * Cells are 32 world units, so the grid covers [0, 2048).
 *
 * Returns 0 -- indistinguishable from a legitimately zero cell -- when any of
 * x, y falls outside [0, 2048). NaN also returns 0.
 *
 * pGrid was the global at 0x106C7C6C.
 */
uint32_t BrGrid64Sample(const uint16_t *pGrid, float x, float y);

/* ---------------------------------------------------------------- 0x10002EF0
 * A read cursor over a u16 table: two 16-bit fields, a position and a
 * remaining count, packed into one dword and rewritten in a single go.
 */
typedef struct BrU16Cursor {
    uint16_t pos;        /* +0x00  index into the table, post-incremented */
    uint16_t remaining;  /* +0x02  entries left; 0 means exhausted */
} BrU16Cursor;

/* Return pTable[pos] and advance. When remaining == 0 the cursor is left
 * untouched and 0 is returned -- 0 is NOT a reserved value in the table, so
 * "exhausted" and "the entry really is 0" look the same to the caller.
 *
 * pTable was the global at 0x106C7C68 (adjacent to the grid base used by
 * BrGrid64Sample).
 */
uint16_t BrU16CursorNext(const uint16_t *pTable, BrU16Cursor *pCur);

/* ---------------------------------------------------------------- 0x10003460
 * Elapsed milliseconds -> 30 Hz ticks, by the original's own formula:
 *
 *     3 * (ms / 100) + (ms % 100) / 33
 *
 * NOT ms * 30 / 1000. The second term saturates at 3, so ms = 99 and ms = 100
 * both give 3; the sequence repeats a value once every 100 ms rather than
 * being strictly monotonic in steps. The identical code appears at 0x10075100.
 *
 * The original took no arguments: it read the current tick from 0x118AB118
 * (via 0x100750F0) and subtracted the origin at 0x10220DD8.
 */
uint32_t BrTicks30FromMs(uint32_t elapsedMs);

/* ---------------------------------------------------------------------------
 * CHK_* -- the checked stdio/heap wrappers.
 *
 * WARNING: on failure these print and then call exit(1), exactly as the
 * original does (0x1007CC00 is `exit`). They are not recoverable.
 *
 * WARNING: the FILE argument is a FILE **, not a FILE *. Both read wrappers
 * dereference it (`mov ecx, [eax]`) before handing it to fread.
 */

/* Non-zero enables the CHK_FileExists trace; the global at 0x10220CE0. */
extern int BrChkVerbose;

/* 0x100030E0  FCHK_FRead.
 * Returns 1 on a full read (and on a zero-length request), 0 when fread
 * returns nothing at all, and exits on a short-but-nonzero read. */
int BrFChkFRead(void *pDst, size_t size, size_t count, FILE **ppFile);

/* 0x10003170  CHK_FRead. Wraps the above and exits on the 0 case too.
 * Returns pDst. */
void *BrChkFRead(void *pDst, size_t size, size_t count, FILE **ppFile);

/* 0x10003320  CHK_FileExists. Opens "rb" and closes again; 1 if it opened. */
int BrChkFileExists(const char *pPath);

/* 0x10003390  CHK_AllocateMemory. size == 0 returns NULL without allocating
 * and without complaining. pWhat is only used in the failure message. */
void *BrChkAlloc(size_t size, const char *pWhat);

/* 0x100033F0  CHK_ReAllocateMemory.
 * GOTCHA: the size == 0 test happens AFTER the realloc, so a
 * (NULL, 0) call still allocates and then drops the pointer. Preserved. */
void *BrChkRealloc(void *pMem, size_t size, const char *pWhat);

#endif /* SLICE1_01_H */
