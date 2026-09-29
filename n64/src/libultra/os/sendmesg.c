/* n64-cflags: -O1 */
/* sendmesg.c -- libultra's message send (os/sendmesg.c).
 */
#include "tgr/ultra.h"

/* -- declarations -- */
/* -- end declarations -- */

/* WHAT IT DOES: Put a message at the back of a queue, waiting for room
 * (or failing with -1 when not blocking), and wake the first thread
 * waiting for a message. */
/* @implements 0x802654B0 tgr osSendMesg */
s32 osSendMesg(OSMesgQueue *mq, OSMesg msg, s32 flags)
{
	register u32 saveMask;
	register s32 last;

	saveMask = __osDisableInt();

	while (mq->validCount >= mq->msgCount) {
		if (flags == OS_MESG_BLOCK) {
			__osRunningThread->state = OS_STATE_WAITING;
			__osEnqueueAndYield(&mq->fullqueue);
		} else {
			__osRestoreInt(saveMask);
			return(-1);
		}
	}

	last = (mq->first + mq->validCount) % mq->msgCount;
	mq->msg[last] = msg;
	mq->validCount++;

	if (mq->mtqueue->next != NULL) {
		osStartThread(__osPopThread(&mq->mtqueue));
	}

	__osRestoreInt(saveMask);
	return(0);
}
