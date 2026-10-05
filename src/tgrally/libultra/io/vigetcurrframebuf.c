/* n64-cflags: -O1 */
/* vigetcurrframebuf.c -- libultra's shown frame buffer
 * (io/vigetcurrframebuf.c).
 */
#include "tgr/ultra.h"

/* -- declarations -- */
/* -- end declarations -- */

/* WHAT IT DOES: The frame buffer being shown now. */
/* @implements 0x80264B20 tgr osViGetCurrentFramebuffer */
void *osViGetCurrentFramebuffer(void)
{
	register u32 saveMask = __osDisableInt();
	void *framep = __osViCurr->framep;

	__osRestoreInt(saveMask);
	return framep;
}
