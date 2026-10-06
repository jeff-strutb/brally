/* br_trackrec54.c -- gamedata: track object record (0x54 bytes) fixup.
 *
 * BrTrackFixupRec54 byte-reverses one drawable object record of a track,
 * rebases its display-list reference and hands the list to the backend.
 *
 * Filed out of the address batch slice2_20.c; the preamble is slice2_20.c's,
 * carried whole. That file's header note:
 *
 * All field access here goes through memcpy-based helpers rather than casting
 * the file image to a struct pointer.  That is not defensive style, it is
 * required: the images are N64 data whose alignment the host cannot rely on,
 * and reading them by overlay would also make the byte order depend on the
 * host, which is exactly the bug this whole range exists to avoid.
 */

/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include <string.h>

/* The original takes ONE argument: `mov esi,[esp+0x10]` after three
 * pushes is arg1, and `mov [esp+0x14],3` later homes a LOCAL in the
 * arg-1 slot -- there is no second parameter.  The port added cbFile
 * for a bounds check the original does not have.  Rename the header
 * prototype out of the way for this probe; the real fix is to drop
 * cbFile from slice2_20.h. */
#define BrRcaFixup BrRcaFixup_hdrdecl
#include "slice2_20.h"
#undef BrRcaFixup
#include "br_seg.h"
#include "br_bits.h"
#include "br_vec.h"

/* ==========================================================================
 * Cross-slice dependencies
 * ========================================================================== */

/* XSLICE 0x1002B9D0 */
/* Stores its argument to the global at 0x10675540.  Purpose unknown; called
 * once at the head of each fixup pass (0 for cars, 1 for tracks). */
/* BrSegSetFlag: prototype in br_funcs.h */

/* XSLICE 0x1002B9E0 */
/* Byte-swap n u16s in place.  n <= 0 is a no-op. */
/* BrSwapU16Array: prototype in br_funcs.h */

/* XSLICE 0x1002BA00 */
/* Byte-swap n 8-byte records (four u16s each). */
extern void BrSwapRec8Array(void *pv, int n);

/* XSLICE 0x1002BA60 */
/* Byte-swap n Vec3s (stride 0x0C), i.e. n calls to BrSwapVec3. */
/* BrSwapVec3Array: prototype in br_funcs.h */

/* XSLICE 0x1002BA80 */
/* Byte-swap and rebase n records of stride 0x24 (body at 0x1002BAA0). */
/* BrSwapRec24Array: prototype in br_funcs.h */

/* XSLICE 0x1002BF40 */
/* Non-zero if pv is NULL or already in the registered display-list table at
 * 0x1067B550.  Callers use `== 0` to mean "not seen yet". */
/* BrDlIsRegistered: prototype in br_funcs.h */

/* XSLICE 0x1002BF80 */
/* Register and byte-swap a display list. */
/* BrDlRegister: prototype in br_funcs.h */
/* BrSegPtrFixup: prototype in br_funcs.h */

/* XSLICE 0x10074DC0 */
/* BrSub10074DC0: prototype in br_funcs.h */
/* XSLICE 0x10074E00 */
/* BrSub10074E00: prototype in br_funcs.h */
/* XSLICE 0x1003445A */
/* BrSub1003445A: prototype in br_funcs.h */
/* XSLICE 0x10035BD1 */
/* BrSub10035BD1: prototype in br_funcs.h */
/* XSLICE 0x10061010 */
extern void BrSub10061010(int iCar, int fPreview);
/* XSLICE 0x10037990 */
extern void BrSub10037990(const char *pszPath);

/* XSLICE 0x1003B170 */
/* One vector in, one float out.  Used here and at 0x10037B10 as `if (f != 0)
 * r = 1/f`, so almost certainly a length -- not verified in this packet. */
extern float BrVec3Len(const BrVec3 *pV);

/* XSLICE 0x1003BD50 */
extern int BrRand(void);

/* Checked stdio wrappers -- names and signatures as already declared by
 * slice1_01.h.  NOTE the FILE ** (not FILE *): the originals dereference it. */
/* XSLICE 0x10003170 */
/* BrChkFRead: prototype in br_funcs.h */
/* XSLICE 0x10003320 */
/* BrChkFileExists: prototype in br_funcs.h */
/* XSLICE 0x10002FE0 */
/* BrChkFReadOpen: prototype in br_funcs.h */
/* XSLICE 0x10002F90 */
/* BrChkFileSize: prototype in br_funcs.h */
/* XSLICE 0x10003290 */
/* BrChkFClose: prototype in br_funcs.h */
/* XSLICE 0x1007C830 */
extern int BrSprintf(char *pDst, const char *pszFmt, ...);
/* XSLICE 0x10008CF0 */
/* BrFatal: prototype in br_funcs.h */

/* Backend dispatch, three function pointers in the DLL's data segment.  All
 * cdecl.  Handles are kept as uint32_t because the originals are 32-bit
 * values living inside the file image. */
/* XSLICE 0x118AA084 */
/* XSLICE 0x118AA0C4 */
extern void (*g_pfn18AA0C4)(void *pv);
/* XSLICE 0x118AA0C8 */
extern void (*g_pfn18AA0C8)(void *pRec, int flag);
/* XSLICE 0x118AA0CC */
extern void (*g_pfn18AA0CC)(void *pTable, int cRecords);

/* Plain globals. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* Particle-style pool, see slice2_20.h. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */


/* ==========================================================================
 * 0x10038010 / 0x10037FE0 -- the 0x54-byte object records
 * ========================================================================== */

/* WHAT IT DOES: prepares one drawable object of a track: turns all its
 * numbers round, rebases the one reference it carries -- which points at the
 * object's drawing commands -- and then registers those commands and hands
 * them to the graphics backend.
 *
 * The original UNROLLS all eighteen dword reversals and six u16 swaps
 * inline -- no loop, no helper -- and re-reads the +0x44 slot for both the
 * register call and the texture scan (re-deref idiom, docs/brally/VC5-IDIOMS.md).
 *
 * NOT MATCHING by 24 bytes in the u16 window +0x1C0..0x1F0: the original
 * loads each pair's LOW byte first, VC5 here loads the highs first.  Three
 * spellings (or-order, statement split) compile byte-identical, so the load
 * order is scheduler-canonical -- allocator-residue class, do not grind. */
/* @t4-pass 0x100316D0 1 2026-09-07 probes 107 bytes 563 insns 192 regions 1 rows 0 census yes  (tools/brally/crank.py) */
/* @t4-pass 0x100316D0 2 2026-09-07 probes 107 bytes 563 insns 192 regions 1 rows 0 census yes  (tools/brally/crank.py) */
/* @t3 0x100316D0 2026-09-09 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 563/563 insns 192/192 rows 0+0 regions 1 oracle UNCLASSIFIED
 * @t3-effort passes 2 zero-movement 1 2
 * residue is scheduler-canonical u16 pair load order (see the NOT MATCHING
 * note above: three spellings compile byte-identical); identical multiset,
 * size-exact; two crank census passes at these numbers.
 * Do not reopen before the end-grind. */
/* @implements 0x100316D0 glide BrTrackFixupRec54 */
void BrTrackFixupRec54(void *pvRec)
{
    uint8_t *p = (uint8_t *)pvRec;
    uint8_t  t, u;

    t = p[0x00]; u = p[0x03]; p[0x03] = t; p[0x00] = u;
    t = p[0x01]; u = p[0x02]; p[0x02] = t; p[0x01] = u;
    t = p[0x04]; u = p[0x07]; p[0x07] = t; p[0x04] = u;
    t = p[0x05]; u = p[0x06]; p[0x06] = t; p[0x05] = u;
    t = p[0x08]; u = p[0x0B]; p[0x0B] = t; p[0x08] = u;
    t = p[0x09]; u = p[0x0A]; p[0x0A] = t; p[0x09] = u;
    t = p[0x0C]; u = p[0x0F]; p[0x0F] = t; p[0x0C] = u;
    t = p[0x0D]; u = p[0x0E]; p[0x0E] = t; p[0x0D] = u;
    t = p[0x10]; u = p[0x13]; p[0x13] = t; p[0x10] = u;
    t = p[0x11]; u = p[0x12]; p[0x12] = t; p[0x11] = u;
    t = p[0x14]; u = p[0x17]; p[0x17] = t; p[0x14] = u;
    t = p[0x15]; u = p[0x16]; p[0x16] = t; p[0x15] = u;
    t = p[0x18]; u = p[0x1B]; p[0x1B] = t; p[0x18] = u;
    t = p[0x19]; u = p[0x1A]; p[0x1A] = t; p[0x19] = u;
    t = p[0x1C]; u = p[0x1F]; p[0x1F] = t; p[0x1C] = u;
    t = p[0x1D]; u = p[0x1E]; p[0x1E] = t; p[0x1D] = u;
    t = p[0x20]; u = p[0x23]; p[0x23] = t; p[0x20] = u;
    t = p[0x21]; u = p[0x22]; p[0x22] = t; p[0x21] = u;
    t = p[0x24]; u = p[0x27]; p[0x27] = t; p[0x24] = u;
    t = p[0x25]; u = p[0x26]; p[0x26] = t; p[0x25] = u;
    t = p[0x28]; u = p[0x2B]; p[0x2B] = t; p[0x28] = u;
    t = p[0x29]; u = p[0x2A]; p[0x2A] = t; p[0x29] = u;
    t = p[0x2C]; u = p[0x2F]; p[0x2F] = t; p[0x2C] = u;
    t = p[0x2D]; u = p[0x2E]; p[0x2E] = t; p[0x2D] = u;
    t = p[0x30]; u = p[0x33]; p[0x33] = t; p[0x30] = u;
    t = p[0x31]; u = p[0x32]; p[0x32] = t; p[0x31] = u;
    t = p[0x34]; u = p[0x37]; p[0x37] = t; p[0x34] = u;
    t = p[0x35]; u = p[0x36]; p[0x36] = t; p[0x35] = u;
    t = p[0x38]; u = p[0x3B]; p[0x3B] = t; p[0x38] = u;
    t = p[0x39]; u = p[0x3A]; p[0x3A] = t; p[0x39] = u;
    t = p[0x3C]; u = p[0x3F]; p[0x3F] = t; p[0x3C] = u;
    t = p[0x3D]; u = p[0x3E]; p[0x3E] = t; p[0x3D] = u;
    t = p[0x40]; u = p[0x43]; p[0x43] = t; p[0x40] = u;
    t = p[0x41]; u = p[0x42]; p[0x42] = t; p[0x41] = u;
    t = p[0x44]; u = p[0x47]; p[0x47] = t; p[0x44] = u;
    t = p[0x45]; u = p[0x46]; p[0x46] = t; p[0x45] = u;

    BrSegPtrFixup((uint32_t *)(void *)(p + 0x44));

    *(uint16_t *)(p + 0x48) = (uint16_t)((p[0x48] << 8) | p[0x49]);
    *(uint16_t *)(p + 0x4A) = (uint16_t)((p[0x4A] << 8) | p[0x4B]);
    *(uint16_t *)(p + 0x4C) = (uint16_t)((p[0x4C] << 8) | p[0x4D]);
    *(uint16_t *)(p + 0x4E) = (uint16_t)((p[0x4E] << 8) | p[0x4F]);
    *(uint16_t *)(p + 0x50) = (uint16_t)((p[0x50] << 8) | p[0x51]);
    *(uint16_t *)(p + 0x52) = (uint16_t)((p[0x52] << 8) | p[0x53]);

    BrF3DListFixup(BR_AT32(void *, (p + 0x44)));
    BrDlOwnerFixup(p);
    BrFontSetRenderDst(1);
    g_pfn18AA0C4(BR_AT32(void *, (p + 0x44)));
}
