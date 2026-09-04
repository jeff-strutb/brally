/* br_pad.c -- the per-frame controller pass.
 *
 * RESPONSIBILITY: reading what the player is doing -- opening and closing the
 * sampling window each frame and walking the pad blocks.
 *
 * Moved here out of src/core/slice2_19.c (an address batch, not a module).
 * The bodies are byte-for-byte the text that was matched there; the layouts,
 * globals and prototypes they need all come from slice2_19.h.
 */
#include "slice2_19.h"

#ifdef BR_MATCHING_BUILD

extern int DAT_106b8090;
extern int DAT_106ec778;
extern int DAT_106ed630;
extern unsigned short _DAT_100b5598;
int FUN_10059e70();
extern char DAT_106ed708;
void BrPadFrameBegin(void);
int BrStubTrue();

/* WHAT IT DOES: run the per-pad input step: 0x1002CE5F once, then for each pad block
 * (base 0x106ED708, stride 0x15C, count 1 in this build) translate the raw pad state and
 * split the bit edges. thiscall callees via BR_THISCALL1. */
/* @implements 0x1002CE9A glide BrPadTranslateAll */

void BrPadTranslateAll(void)

{
  int i;
  BrPadFrameBegin();
  for (i = 0; i < 1; i++) {
    BrPadTranslate((BrPad *)((char *)&DAT_106ed708 + i*0x15c));
    BrBitEdgeSplit((BrBitPair *)((char *)&DAT_106ed708 + i*0x15c));
  }
}

/* WHAT IT DOES: once per frame, open the pad sampling window: on the first call set the
 * in-progress flag, zero the u16 latch at 0x100B5598 and trace the 0x106B8090 block. */
/* @implements 0x1002CE31 glide BrPadFrameInit */

void BrPadFrameInit(void)

{
  if (DAT_106ec778 == 0) {
    DAT_106ec778 = 1;
    _DAT_100b5598 = 0;
    BrStubTrue(&DAT_106b8090);
  }
  return;
}

/* WHAT IT DOES: begin the pad frame: init, trace, run 0x10059E70 on the 0x106ED630 block,
 * mark the u16 latch live and clear the in-progress flag. */
/* @implements 0x1002CE5F glide BrPadFrameBegin */

void BrPadFrameBegin(void)

{
  BrPadFrameInit();
  BrStubTrue(&DAT_106b8090,0,1);
  FUN_10059e70(&DAT_106ed630);
  _DAT_100b5598 = 1;
  DAT_106ec778 = 0;
  return;
}

#endif /* BR_MATCHING_BUILD */
