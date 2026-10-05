/* n64-cflags: -O1 */
/* createmesgqueue.c -- libultra's message queue setup
 * (os/createmesgqueue.c).
 */
#include "tgr/ultra.h"

/* -- declarations -- */
/* -- end declarations -- */

/* WHAT IT DOES: Set up an empty message queue over msgCount slots, with no
 * threads waiting on it. */
/* @implements 0x80265410 tgr osCreateMesgQueue */
void osCreateMesgQueue(OSMesgQueue *mq, OSMesg *msg, s32 msgCount)
{
	mq->mtqueue = (OSThread *)&__osThreadTail;
	mq->fullqueue = (OSThread *)&__osThreadTail;
	mq->validCount = 0;
	mq->first = 0;
	mq->msgCount = msgCount;
	mq->msg = msg;
}
