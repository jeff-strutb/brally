#include "br_trkhdr.h"   /* g_brTrkHdr, the loaded track header */
#include "slice1_05.h"   /* br_globals: its objects */
/* br_texanim.c -- drawing: per-frame texture animation.
 *
 * Glide 0x1002CB49 (D3D 0x10033498). /Od-compiled, like its neighbours in
 * the 0x1002Cxxx..0x1002Exxx stretch: frame pointer, locals homed by name
 * hash (letters in frame order), record fields re-read through the full
 * table expression at every use.
 */

/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */        /* 0x100B3014 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */      /* texture record count */
/* 64-bit core: declared once, in br_globals.h or its struct's header */      /* texture record table, 0x24-byte records */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */        /* 0x105CCB5C */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */      /* frame clock */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* BrScratchRingAlloc: prototype in br_funcs.h */
/* BrStubTrue: prototype in br_funcs.h */

#define REC(i)  (BR_PTR32(uint8_t *, g_brTrkHdr.aSections) + (i) * 0x24)

/* WHAT IT DOES: advance every animated texture one frame. For each live
 * record whose descriptor is a key list, take the frame clock modulo the
 * list's period, find the key it falls in (or key 1 outright for kind 0xB
 * when the animate-all mode is on), notify the palette hook with that key's
 * texture id, and re-upload the key's pixels through the scratch ring.
 * Records without a key list are re-uploaded once when the mode global
 * changes. Records with a placeholder (2, -1) list, and every record while
 * the game is paused or in state 2, are skipped. */
/* @implements 0x1002CB49 glide BrTexAnimStep */

void BrTexAnimStep(void)

{
  /* a off (-4)  b period (-8)  c key (-0xc)  d tex id (-0x10)
   * e animate-all (-0x14)  f record (-0x18)  g phase (-0x1c) */
  int a;
  unsigned int b;
  int c;
  int d;
  int e;
  int f;
  unsigned int g;

  e = ((*(int *)((char *)&g_aBrEntRecs + 0x80)) != 0) && ((*(int *)&g_Br0B380C) != 2) && ((*(int *)&g_Br0B380C) != 8);
  for (f = 0; f < g_brTrkHdr.cSections; f = f + 1) {
    if (*(int *)REC(f) == 0) continue;
    if ((unsigned char)((*(unsigned int *)(REC(f) + 0x20) >> 0x14) & 1) != 0) {
      b = *(int *)(((*(int *)(REC((f)) + 8))) + 8 + (*(unsigned short *)(((*(int *)(REC((f)) + 8))) + 2) - 1) * 0xc) - *(int *)(((*(int *)(REC((f)) + 8))) + 8);
      g = DAT_106ec768 - (DAT_106ec768 / b) * b;
      if ((*(unsigned short *)(((*(int *)(REC((f)) + 8))) + 2) == 2) && (*(int *)(((*(int *)(REC((f)) + 8))) + 8) == -1)) continue;
      if (((*(int *)&g_BrX06909B4) != 0) || ((*(int *)&DAT_105ccb68[8]) == 2)) continue;
      if ((e) && (((*(unsigned int *)(REC(f) + 0x20) >> 0x18) & 0xf) == 0xb)) {
        c = 1;
      }
      else {
        for (c = 1; c < *(unsigned short *)(((*(int *)(REC((f)) + 8))) + 2); c = c + 1) {
          if (g < *(unsigned int *)(((*(int *)(REC((f)) + 8))) + 8 + c * 0xc)) break;
        }
      }
      c = c - 1;
      d = *(int *)(((*(int *)(REC((f)) + 8))) + 0xc + c * 0xc);
      a = *(int *)(((*(int *)(REC((f)) + 8))) + 0x10 + c * 0xc);
      if (((*(unsigned int *)(REC(f) + 0x20) & 0x3ffff) != 0) && (d != -1)) {
        (*(*(int (**)())&g_BrDrawModelDlHook))((d >> 0x10) & 0xffff, d & 0xffff);
      }
upload:
      if ((*(int *)(REC(f) + 4) != 0) && (a != -1)) {
        /* Ghidra reads this as a six-argument allocation handed to the
         * stub. The allocator takes NOTHING (0x1002A840 reads globals), so
         * every one of the seven pushes is the stub's, the allocation first:
         * that is why the original has no `add esp` between the two calls
         * -- a genuinely nested call with arguments cleans its own stack
         * first under /Od, in every flag combination measured. */
        BrStubTrue();
      }
    }
    else {
      if (DAT_100aa030 != (*(int *)((char *)&g_aBrEntRecs + 0x80))) {
        a = (*(unsigned int *)(REC(f) + 8) & 0xfff) << 5;
        goto upload;
      }
    }
  }
  DAT_100aa030 = (*(int *)((char *)&g_aBrEntRecs + 0x80));
  return;
}

#undef REC
#undef DESC
