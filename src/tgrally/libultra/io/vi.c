/* n64-cflags: -O1 */
/* vi.c -- libultra's VI state setup (io/vi.c).
 */
#include "tgr/ultra.h"

/* -- declarations -- */
/* -- end declarations -- */

__OSViContext vi[2] = {0};
__OSViContext *__osViCurr = &vi[0];
__OSViContext *__osViNext = &vi[1];

/* WHAT IT DOES: Clear both VI states, point them at K0BASE, pick the
 * low-res non-interlaced mode for the TV standard, start blanked, wait for
 * the VI to reach the top lines and switch the state in. */
/* @implements 0x80268580 tgr __osViInit */
void __osViInit(void)
{
	bzero(vi, sizeof(vi));
	__osViCurr = &vi[0];
	__osViNext = &vi[1];
	__osViNext->retraceCount = 1;
	__osViCurr->retraceCount = 1;
	__osViNext->framep = (void *)K0BASE;
	__osViCurr->framep = (void *)K0BASE;
	if (osTvType == OS_TV_PAL) {
		__osViNext->modep = &osViModePalLan1;
	} else if (osTvType == OS_TV_MPAL) {
		__osViNext->modep = &osViModeMpalLan1;
	} else {
		__osViNext->modep = &osViModeNtscLan1;
	}
	__osViNext->state = VI_STATE_BLACK;
	__osViNext->control = __osViNext->modep->comRegs.ctrl;
	while (IO_READ(VI_CURRENT_REG) > 10)
		;
	IO_WRITE(VI_CONTROL_REG, 0);
	__osViSwapContext();
}
