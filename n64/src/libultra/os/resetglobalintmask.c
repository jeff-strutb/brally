/* n64-cflags: -O1 */
/* resetglobalintmask.c -- libultra's global interrupt mask clear
 * (os/resetglobalintmask.c).
 */
#include "tgr/ultra.h"

/* -- declarations -- */
/* -- end declarations -- */

/* WHAT IT DOES: Take interrupts out of the global mask (never the RCP
 * summary bits). */
/* @implements 0x8026E6D0 tgr __osResetGlobalIntMask */
void __osResetGlobalIntMask(OSHWIntr mask)
{
	register u32 saveMask = __osDisableInt();

	__OSGlobalIntMask &= ~(mask & ~OS_IM_RCP);
	__osRestoreInt(saveMask);
}
