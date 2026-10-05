/* n64-cflags: -O1 */
/* startthread.c -- libultra's thread start (os/startthread.c).
 */
#include "tgr/ultra.h"

/* -- declarations -- */
/* -- end declarations -- */

/* WHAT IT DOES: Make a stopped or waiting thread runnable (a stopped one
 * that was waiting on a queue goes back on it, and that queue's first
 * thread runs instead), then dispatch or yield to a higher priority. */
/* @implements 0x802657C0 tgr osStartThread */
void osStartThread(OSThread *t)
{
	register u32 saveMask = __osDisableInt();

	switch (t->state) {
	case OS_STATE_WAITING:
		t->state = OS_STATE_RUNNABLE;
		__osEnqueueThread(&__osRunQueue, t);
		break;
	case OS_STATE_STOPPED:
		if (t->queue == NULL || t->queue == &__osRunQueue) {
			t->state = OS_STATE_RUNNABLE;
			__osEnqueueThread(&__osRunQueue, t);
		} else {
			t->state = OS_STATE_WAITING;
			__osEnqueueThread(t->queue, t);
			__osEnqueueThread(&__osRunQueue, __osPopThread(t->queue));
		}
		break;
	}

	if (__osRunningThread == NULL) {
		__osDispatchThread();
	} else if (__osRunningThread->priority < __osRunQueue->priority) {
		__osRunningThread->state = OS_STATE_RUNNABLE;
		__osEnqueueAndYield(&__osRunQueue);
	}

	__osRestoreInt(saveMask);
}
