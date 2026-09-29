/* n64-cflags: -O1 */
/* visetspecial.c -- libultra's VI feature switches (io/visetspecial.c).
 */
#include "tgr/ultra.h"

/* -- declarations -- */
#define OS_VI_GAMMA_ON 0x0001
#define OS_VI_GAMMA_OFF 0x0002
#define OS_VI_GAMMA_DITHER_ON 0x0004
#define OS_VI_GAMMA_DITHER_OFF 0x0008
#define OS_VI_DIVOT_ON 0x0010
#define OS_VI_DIVOT_OFF 0x0020
#define OS_VI_DITHER_FILTER_ON 0x0040
#define OS_VI_DITHER_FILTER_OFF 0x0080
#define VI_CTRL_GAMMA_DITHER_ON 0x00004
#define VI_CTRL_GAMMA_ON 0x00008
#define VI_CTRL_DIVOT_ON 0x00010
#define VI_CTRL_ANTIALIAS_MASK 0x00300
#define VI_CTRL_DITHER_FILTER_ON 0x10000
/* -- end declarations -- */

/* WHAT IT DOES: Turn the VI's gamma, gamma dither, divot and dither filter
 * on or off from the next retrace (the dither filter replacing the mode's
 * anti-aliasing, and giving it back when turned off). */
/* @implements 0x80264490 tgr osViSetSpecialFeatures */
void osViSetSpecialFeatures(u32 func)
{
	register u32 saveMask = __osDisableInt();

	if ((func & OS_VI_GAMMA_ON) != 0) {
		__osViNext->control |= VI_CTRL_GAMMA_ON;
	}
	if ((func & OS_VI_GAMMA_OFF) != 0) {
		__osViNext->control &= ~VI_CTRL_GAMMA_ON;
	}
	if ((func & OS_VI_GAMMA_DITHER_ON) != 0) {
		__osViNext->control |= VI_CTRL_GAMMA_DITHER_ON;
	}
	if ((func & OS_VI_GAMMA_DITHER_OFF) != 0) {
		__osViNext->control &= ~VI_CTRL_GAMMA_DITHER_ON;
	}
	if ((func & OS_VI_DIVOT_ON) != 0) {
		__osViNext->control |= VI_CTRL_DIVOT_ON;
	}
	if ((func & OS_VI_DIVOT_OFF) != 0) {
		__osViNext->control &= ~VI_CTRL_DIVOT_ON;
	}
	if ((func & OS_VI_DITHER_FILTER_ON) != 0) {
		__osViNext->control |= VI_CTRL_DITHER_FILTER_ON;
		__osViNext->control &= ~VI_CTRL_ANTIALIAS_MASK;
	}
	if ((func & OS_VI_DITHER_FILTER_OFF) != 0) {
		__osViNext->control &= ~VI_CTRL_DITHER_FILTER_ON;
		__osViNext->control |= __osViNext->modep->comRegs.ctrl & VI_CTRL_ANTIALIAS_MASK;
	}
	__osViNext->state |= VI_STATE_CTRL_UPDATED;
	__osRestoreInt(saveMask);
}
