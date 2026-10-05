/* n64-cflags: -O1 */
/* getcurrfaultthread.c -- libultra's faulted thread
 * (os/getcurrfaultthread.c).
 */
#include "tgr/ultra.h"

/* -- declarations -- */
/* -- end declarations -- */

/* WHAT IT DOES: The thread that last faulted. */
/* @implements 0x80266390 tgr __osGetCurrFaultedThread */
OSThread *__osGetCurrFaultedThread(void)
{
	return __osFaultedThread;
}
