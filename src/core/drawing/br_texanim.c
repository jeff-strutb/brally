/* br_texanim.c -- drawing: per-frame texture animation.
 *
 * Glide 0x1002CB49 (D3D 0x10033498). /Od-compiled, like its neighbours in
 * the 0x1002Cxxx..0x1002Exxx stretch: frame pointer, locals homed by name
 * hash (letters in frame order), record fields re-read through the full
 * table expression at every use.
 */
#ifdef BR_MATCHING_BUILD

extern int DAT_106ed6b0;
extern int BrG_0B380C;        /* 0x100B3014 */
extern int DAT_106eecf0;      /* texture record count */
extern int DAT_106eecf4;      /* texture record table, 0x24-byte records */
extern int DAT_100aa030;
extern int BrG_6909B4;        /* 0x105CCB5C */
extern int DAT_105ccb88;
extern int DAT_106ec768;      /* frame clock */
extern int DAT_106b7c7c;
extern char DAT_106ed570;
extern int (*DAT_118ed1bc)();
void *BrScratchRingAlloc(void);
int BrStubTrue();

#define REC(i)  (DAT_106eecf4 + (i) * 0x24)
#define DESC(i) (*(int *)(REC(i) + 8))

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

  e = (DAT_106ed6b0 != 0) && (BrG_0B380C != 2) && (BrG_0B380C != 8);
  for (f = 0; f < DAT_106eecf0; f = f + 1) {
    if (*(int *)REC(f) == 0) continue;
    if ((unsigned char)((*(unsigned int *)(REC(f) + 0x20) >> 0x14) & 1) != 0) {
      b = *(int *)(DESC(f) + 8 + (*(unsigned short *)(DESC(f) + 2) - 1) * 0xc) - *(int *)(DESC(f) + 8);
      g = DAT_106ec768 - (DAT_106ec768 / b) * b;
      if ((*(unsigned short *)(DESC(f) + 2) == 2) && (*(int *)(DESC(f) + 8) == -1)) continue;
      if ((BrG_6909B4 != 0) || (DAT_105ccb88 == 2)) continue;
      if ((e) && (((*(unsigned int *)(REC(f) + 0x20) >> 0x18) & 0xf) == 0xb)) {
        c = 1;
      }
      else {
        for (c = 1; c < *(unsigned short *)(DESC(f) + 2); c = c + 1) {
          if (g < *(unsigned int *)(DESC(f) + 8 + c * 0xc)) break;
        }
      }
      c = c - 1;
      d = *(int *)(DESC(f) + 0xc + c * 0xc);
      a = *(int *)(DESC(f) + 0x10 + c * 0xc);
      if (((*(unsigned int *)(REC(f) + 0x20) & 0x3ffff) != 0) && (d != -1)) {
        (*DAT_118ed1bc)((d >> 0x10) & 0xffff, d & 0xffff);
      }
upload:
      if ((*(int *)(REC(f) + 4) != 0) && (a != -1)) {
        /* Ghidra reads this as a six-argument allocation handed to the
         * stub. The allocator takes NOTHING (0x1002A840 reads globals), so
         * every one of the seven pushes is the stub's, the allocation first:
         * that is why the original has no `add esp` between the two calls
         * -- a genuinely nested call with arguments cleans its own stack
         * first under /Od, in every flag combination measured. */
        BrStubTrue(BrScratchRingAlloc(), 0, 0, DAT_106b7c7c + a, *(int *)(REC(f) + 4),
                   ((((*(unsigned int *)(REC(f) + 0x20) >> 0x18) & 0xf) != 1) ? 0x200 : 0x20),
                   &DAT_106ed570);
      }
    }
    else {
      if (DAT_100aa030 != DAT_106ed6b0) {
        a = (*(unsigned int *)(REC(f) + 8) & 0xfff) << 5;
        goto upload;
      }
    }
  }
  DAT_100aa030 = DAT_106ed6b0;
  return;
}

#undef REC
#undef DESC
#endif /* BR_MATCHING_BUILD */
