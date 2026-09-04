/* br_fade.c -- drawing: the screen fade.
 *
 * RESPONSIBILITY: drawing/ -- turn geometry and images into pixels.
 *
 * Filed out of the address batches, which are not modules.  The fade is a
 * single level driven towards a target: this file collects the step that
 * walks it down, the calls that set a target, and the two tests that ask
 * whether the fade is still moving or has settled.
 */
#include <stdint.h>

#ifdef BR_MATCHING_BUILD

extern int DAT_104abb20;
extern float DAT_106e9d8c;
extern float _DAT_104abb24;
extern float kF300_S_S537;

/* WHAT IT DOES: fade a level down by one step per call and, once it reaches
 * the floor, snap it to zero and drop the pointer that was driving it. The
 * tail end of a fade-out. */
/* @implements 0x10014CB0 glide FUN_10014cb0 */
/* @n64 0x8021F1A4 located */
/* auto-filed from ghidra --refine; transforms: kF300_S_S537:float */

void FUN_10014cb0(void)

{
  if ((_DAT_104abb24 != kF300_S_S537) &&
     (_DAT_104abb24 = _DAT_104abb24 - DAT_106e9d8c, _DAT_104abb24 <= kF300_S_S537)) {
    _DAT_104abb24 = 0.0;
    DAT_104abb20 = 0;
  }
  return;
}

#endif /* BR_MATCHING_BUILD */
