/* n64-cflags: -O1 */
/* gettime.c -- libultra's clock (os/gettime.c).
 */
#include "tgr/ultra.h"

/* -- declarations -- */
/* -- end declarations -- */

/* WHAT IT DOES: The time since boot in counter ticks: the kept time plus
 * the ticks since it was last brought up to date. */
/* @implements 0x8026B960 tgr osGetTime */
OSTime osGetTime(void)
{
	u32 tmptime;
	u32 elapseCount;
	OSTime currentCount;
	register u32 saveMask;

	saveMask = __osDisableInt();
	tmptime = osGetCount();
	elapseCount = tmptime - __osBaseCounter;
	currentCount = __osCurrentTime;
	__osRestoreInt(saveMask);
	return currentCount + elapseCount;
}
