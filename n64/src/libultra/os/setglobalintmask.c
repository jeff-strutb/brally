/* n64-cflags: -O1 */
/* setglobalintmask.c -- libultra's global interrupt mask set
 * (os/setglobalintmask.c).
 */
#include "tgr/ultra.h"

/* -- declarations -- */
/* -- end declarations -- */

/* WHAT IT DOES: Add interrupts to the global mask. */
/* @implements 0x8026E7D0 tgr __osSetGlobalIntMask */
void __osSetGlobalIntMask(OSHWIntr mask)
{
	register u32 saveMask = __osDisableInt();

	__OSGlobalIntMask |= mask;
	__osRestoreInt(saveMask);
}
