/* n64-cflags: -O1 */
/* jammesg.c -- libultra's urgent message send (os/jammesg.c).
 */
#include "tgr/ultra.h"

/* -- declarations -- */
/* -- end declarations -- */

/* WHAT IT DOES: Put a message at the front of a queue (waiting for room,
 * or -1 when not blocking) and wake the first thread waiting for one. */
/* @implements 0x8026B5D0 tgr osJamMesg */
s32 osJamMesg(OSMesgQueue *mq, OSMesg msg, s32 flag)
{
	register u32 saveMask;

	saveMask = __osDisableInt();

	while (MQ_IS_FULL(mq)) {
		if (flag == OS_MESG_BLOCK) {
			__osRunningThread->state = OS_STATE_WAITING;
			__osEnqueueAndYield(&mq->fullqueue);
		} else {
			__osRestoreInt(saveMask);
			return -1;
		}
	}

	mq->first = (mq->first + mq->msgCount - 1) % mq->msgCount;
	mq->msg[mq->first] = msg;
	mq->validCount++;

	if (mq->mtqueue->next != NULL) {
		osStartThread(__osPopThread(&mq->mtqueue));
	}

	__osRestoreInt(saveMask);
	return 0;
}
