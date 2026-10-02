/* br_save.c -- the ".BRF" championship-season save file.  See br_save.h.
 *
 * Read off the writer (0x100709A0 D3D / 0x10069930 Glide) and the reader
 * (0x10070610 / 0x100695C0), both of which state the layout in full; the two
 * agree on every field, including which of the six adjacent option dwords is
 * left out.  No part of this was inferred from a file image -- there is no
 * retail .BRF to infer from.
 *
 * Everything that crosses the file boundary is decoded byte-wise.  The image
 * is never overlaid on a struct, and BrBrfSeason is a host object whose size
 * is nobody's business but this host's.
 */

/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
/* slice4_52.h declares the port's void copy of the writer under the same
 * name; the matching definition at the end of this file returns char. */
#define BrMenuSub100709A0 BrMenuSub100709A0_port
#include "slice3_41.h"   /* br_globals: its objects */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "br_save.h"
#include "slice1_01.h"   /* BrAdler32 -- 0x10001000, zlib adler32 verbatim */
#undef BrMenuSub100709A0

/* Compile-time check that the layout constants still compose to the file the
 * two functions describe.  C99 has no _Static_assert here; the tree uses the
 * negative-array-size trick (port/tests/test_layout.c). */
typedef char br_save_assert_size[(BR_BRF_FILE_SIZE == 0x29C) ? 1 : -1];
typedef char br_save_assert_tail[(BR_BRF_TAIL_FROM_END == 0x94) ? 1 : -1];
typedef char br_save_assert_blk[(BR_SEASON_BLOCK_SIZE == 0x200) ? 1 : -1];

/* ==========================================================================
 * byte-wise integer access
 *
 * The original fwrite()s and fread()s a dword directly, so the file carries
 * the 32-bit x86 build's own layout: little-endian.  Spelled out rather than
 * memcpy'd so the convention is stated once, here, and not implied by the
 * host's.
 * ========================================================================== */

/* (port-only BrBrfRd32 removed) */


/* (port-only BrBrfWr32 removed) */


/* ==========================================================================
 * the checksum
 * ==========================================================================
 *
 * The original is two calls, not one:
 *
 *     push 0 / push 0 / push 0 / call adler32     -> the seed, which is 1
 *     push 0x200 / push block / push seed / call adler32
 *
 * The first call is the documented "give me the initial value" form -- pBuf
 * NULL returns 1 and ignores the adler argument entirely (slice1_01.h).  It is
 * kept rather than folded to a literal 1 because that is the fact the original
 * relies on, and BrAdler32 is where it is established.
 */
/* @n64 0x8023DF4C located */
/* (port-only BrBrfChecksum removed) */


/* ==========================================================================
 * the layout
 * ========================================================================== */

/* (port-only BrBrfEncode removed) */


/* (port-only BrBrfDecode removed) */


/* ==========================================================================
 * the host seam
 *
 * Win32 in the original only in the sense that it is MSVCRT stdio: fopen /
 * fread / fseek / ftell / fclose, with the mode strings "rb" (0x1007B0E0 in
 * Glide) and "wb" (0x1007B600).  Nothing here needs a Win32 type.
 * ========================================================================== */

/* (port-only BrBrfReadFile removed) */


/* (port-only BrBrfWriteFile removed) */


/* ==========================================================================
 * what the load screen's file list is built from
 * ========================================================================== */

/* (port-only BrBrfReadName removed) */


/* (port-only BrBrfSlotIndex removed) */


/* (port-only BrBrfFileName removed) */


/* 64-bit core: declared once, in br_globals.h or its struct's header */            /* the .BRF path */
/* 64-bit core: declared once, in br_globals.h or its struct's header */            /* "wb" */
/* 64-bit core: declared once, in br_globals.h or its struct's header */               /* the file magic */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* the 0x80-byte tail */
/* 64-bit core: declared once, in br_globals.h or its struct's header */ /* the 0x200-byte season block */

/* WHAT IT DOES: write the championship-season save file (the layout
 * BrBrfEncode above describes): the magic, an adler32 over the season block,
 * the block, five option dwords and the 0x80-byte tail.  Returns 0 without
 * finishing if the file cannot be opened or a checked write comes up short;
 * the five option dwords are written unchecked, as in the original. */
/* @implements 0x10069930 glide BrMenuSub100709A0 */
char BrMenuSub100709A0(void)
{
  FILE *fp;
  unsigned long sum;

  sum = BrAdler32(0, 0, 0);
  sum = BrAdler32(sum, (*(unsigned char * *)((char *)&g_aBrRaceCar + 0xE8C)) /* BR_LP64_BYTE_VIEW */, 0x200);
  fp = fopen(DAT_117a6030, DAT_1007b600);
  if (fp == NULL) {
    return 0;
  }
  if (fwrite(&(*(int *)&DAT_100b51e4[952]), 1, 4, fp) != 4) {
    fclose(fp);
    return 0;
  }
  if (fwrite(&sum, 1, 4, fp) != 4) {
    fclose(fp);
    return 0;
  }
  if (fwrite((*(unsigned char * *)((char *)&g_aBrRaceCar + 0xE8C)) /* BR_LP64_BYTE_VIEW */, 1, 0x200, fp) != 0x200) {
    fclose(fp);
    return 0;
  }
  fwrite(&(*(int *)&DAT_10ac5d60), 4, 1, fp);
  fwrite(&(*(int *)&DAT_100abdec), 4, 1, fp);
  fwrite(&(*(int *)&DAT_100abdf0), 4, 1, fp);
  fwrite(&g_brSel0ABDF4, 4, 1, fp);
  fwrite(&(*(int *)&g_i0AC65C), 4, 1, fp);
  if (fwrite((*(unsigned char (*)[])((char *)&g_aBrRaceCar + 0x2AE8)) /* BR_LP64_BYTE_VIEW */, 1, 0x80, fp) != 0x80) {
    fclose(fp);
    return 0;
  }
  fclose(fp);
  return 1;
}
