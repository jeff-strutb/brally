/* n64-cflags: -O1 */
/* yieldthread.c -- libultra's thread yield (os/yieldthread.c).
 */
#include "tgr/ultra.h"

/* -- declarations -- */
/* -- end declarations -- */

/* WHAT IT DOES: Let other runnable threads of the same priority run. */
/* @implements 0x8026E820 tgr osYieldThread */
void osYieldThread(void)
{
	register u32 saveMask = __osDisableInt();

	__osRunningThread->state = OS_STATE_RUNNABLE;
	__osEnqueueAndYield(&__osRunQueue);
	__osRestoreInt(saveMask);
}
