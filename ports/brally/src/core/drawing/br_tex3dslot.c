#include "br_dl.h"   /* br_globals: its objects */
/* br_tex3dslot.c -- drawing: allocate one Glide TMEM slot.
 *
 * 0x10028200 is the allocator 0x10027710 calls. The 0xD8-stride table at
 * 0x10661840 is the same one BrTex3dDownloadAt / FUN_10028420 / FUN_100281c0
 * already walk.
 *
 * RESIDUE, FIRSTDIV +0x0. Orig prologue is `mov eax,[idx]; sub esp,8;
 * cmp eax,0x400; push ebx,ebp,esi,edi; jae fail-at-end`. Recomp emits
 * `sub esp,8` first, inverts the 0x400 test to `jb ok` with an inline
 * fail, and shuffles the GrTexInfo store order. goto-fail helped the
 * TMEM-full path only. Not a first-compile leaf -- needs a store-order
 * pass against the orig fill block at 0x100282D0. */

#define _CRTIMP __declspec(dllimport)

/* grTexTextureMemRequired: prototype in br_funcs.h */
/* grTexMaxAddress: prototype in br_funcs.h */

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
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* WHAT IT DOES: claim one 0xD8-stride Glide texture slot. Fills the
 * GrTexInfo, asks the card how much TMEM the mip chain needs, then tries
 * the TMU's low water-mark and if that would pass 2MB or the TMU's max,
 * the high water-mark. Returns the new slot index, or -1 if the table is
 * full (1024) or the TMU is out of memory. */
/* @t4-pass 0x10028200 1 2026-09-07 probes 150 bytes 436 insns 108 regions 2 rows 6 census yes  (tools/brally/crank.py) */
/* @t4-pass 0x10028200 2 2026-09-07 probes 150 bytes 436 insns 108 regions 2 rows 6 census yes  (tools/brally/crank.py) */
/* @t4-pass 0x10028200 3 2026-09-10 probes 30 bytes 430 insns 105 regions 3 rows 3 census yes  (tools/brally/crank.py) */
/* @t4-pass 0x10028200 4 2026-09-10 probes 30 bytes 430 insns 105 regions 3 rows 3 census yes  (tools/brally/crank.py) */
/* @t3 0x10028200 2026-09-10 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 430/441 insns 105/108 rows 3+0 regions 3 oracle UNCLASSIFIED
 * @t3-effort passes 4 zero-movement 3 4
 * RESIDUE: three allocation singletons -- two spills of the water-mark
 * pair to stack slots the original keeps in registers, and one reload.
 * Every other row pairs.  The lever that closed the tail is below: the
 * high water-mark test is written success-arm-first, which is the
 * original's fall-through-on-jb layout; the `>= ... goto fail` spelling
 * inverts the branch and unanchors 115 B.
 * 30 compiles in the last pass, levers accepted: none that survived the
 * cluster rule; every candidate and score is in build/brally/win32/match/crank.log.
 * Do not reopen before the end-grind. */
/* @implements 0x10028200 glide FUN_10028200 */
int FUN_10028200(int tmu, unsigned int lod, int a2, int a3, int a4, int a5,
                 int a6, int a7, int a8, int a9, int a10, int a11,
                 int a12, float bias, int a14)
{
  unsigned int idx;
  int req;
  unsigned int start;
  unsigned int next;
  BrTexSlot *slot;

  idx = DAT_105d17ec;
  if (idx >= 0x400) {
    goto fail;
  }
  slot = &g_aBrTexSlot[idx];
  slot->info.smallLod = a6;
  slot->info.largeLod = a7;
  slot->info.aspectRatio = a8;
  slot->info.format = a4;
  slot->info.data = 0;
  req = grTexTextureMemRequired((int)(lod & 0xff), &slot->info);
  start = g_aBrTmuMem[tmu].next;
  next = start + req;
  if (next < grTexMaxAddress(tmu)
      && next < 0x200000u) {
    g_aBrTmuMem[tmu].next = next;
  } else {
    start = g_aBrTmuMem[tmu].hi;
    next = start + req;
    /* The high water-mark test is written with the SUCCESS arm first: the
     * original falls through on `jb` into the store and jumps away to the
     * failure exit. */
    if (next < grTexMaxAddress(tmu)) {
      g_aBrTmuMem[tmu].hi = next;
    } else {
      goto fail;
    }
  }
  slot->f00 = 0;
  slot->bLive = 1;
  slot->f08 = a2;
  slot->f0C = a3;
  slot->aspect = a8;
  slot->pData = 0;
  slot->format = a4;
  slot->mipMode = a5;
  slot->magFilter = a12;
  slot->minFilter = a11;
  slot->clampS = a9;
  slot->clampT = a10;
  slot->f30 = 0;
  slot->f34 = 0;
  slot->lodBias = (int)(bias * DAT_1007745c);
  slot->smallLod = a6;
  slot->largeLod = a7;
  slot->tmu = tmu;
  slot->evenOdd = (int)(lod & 0xff);
  slot->start = start;
  slot->lodBlend = a14;
  DAT_105d17ec = DAT_105d17ec + 1;
  return (int)idx;
fail:
  return -1;
}

