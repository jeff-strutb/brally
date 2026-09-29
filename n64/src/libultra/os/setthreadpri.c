/* n64-cflags: -O1 */
/* setthreadpri.c -- libultra's thread priority change
 * (os/setthreadpri.c).
 */
#include "tgr/ultra.h"

/* -- declarations -- */
/* -- end declarations -- */

/* WHAT IT DOES: Change a thread's priority (0: the running thread),
 * re-sorting it in its queue, and yield if another thread now outranks the
 * running one. */
/* @implements 0x802668F0 tgr osSetThreadPri */
void osSetThreadPri(OSThread *t, OSPri pri)
{
	register u32 saveMask = __osDisableInt();

	if (t == NULL)
		t = __osRunningThread;
	if (t->priority != pri) {
		t->priority = pri;
		if (t != __osRunningThread) {
			if (t->state != OS_STATE_STOPPED) {
				__osDequeueThread(t->queue, t);
				__osEnqueueThread(t->queue, t);
			}
		}
		if (__osRunningThread->priority < __osRunQueue->priority) {
			__osRunningThread->state = OS_STATE_RUNNABLE;
			__osEnqueueAndYield(&__osRunQueue);
		}
	}
	__osRestoreInt(saveMask);
}
