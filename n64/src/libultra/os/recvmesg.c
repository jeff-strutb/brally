/* n64-cflags: -O1 */
/* recvmesg.c -- libultra's message receive (os/recvmesg.c).
 */
#include "tgr/ultra.h"

/* -- declarations -- */
/* -- end declarations -- */

/* WHAT IT DOES: Take the message at the front of a queue, waiting for one
 * (or failing with -1 when not blocking), and wake the first thread
 * waiting for room. */
/* @implements 0x802642E0 tgr osRecvMesg */
s32 osRecvMesg(OSMesgQueue *mq, OSMesg *msg, s32 flags)
{
	register u32 saveMask;

	saveMask = __osDisableInt();

	while (MQ_IS_EMPTY(mq)) {
		if (flags == OS_MESG_NOBLOCK) {
			__osRestoreInt(saveMask);
			return(-1);
		} else {
			__osRunningThread->state = OS_STATE_WAITING;
			__osEnqueueAndYield(&mq->mtqueue);
		}
	}

	if (msg != NULL) {
		*msg = mq->msg[mq->first];
	}
	mq->first = (mq->first + 1) % mq->msgCount;
	mq->validCount--;

	if (mq->fullqueue->next != NULL) {
		osStartThread(__osPopThread(&mq->fullqueue));
	}

	__osRestoreInt(saveMask);
	return(0);
}
