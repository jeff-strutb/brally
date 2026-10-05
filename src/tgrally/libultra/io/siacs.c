/* n64-cflags: -O1 */
/* siacs.c -- libultra's serial interface access lock (io/siacs.c).
 */
#include "tgr/ultra.h"

/* -- declarations -- */
/* -- end declarations -- */

OSMesg siAccessBuf[SI_Q_BUF_LEN];
OSMesgQueue __osSiAccessQueue;
u32 __osSiAccessQueueEnabled = 0;

/* WHAT IT DOES: Create the SI access queue holding one token. */
/* @implements 0x80269410 tgr __osSiCreateAccessQueue */
void __osSiCreateAccessQueue(void)
{
	__osSiAccessQueueEnabled = 1;
	osCreateMesgQueue(&__osSiAccessQueue, siAccessBuf, SI_Q_BUF_LEN);
	osSendMesg(&__osSiAccessQueue, (OSMesg)NULL, OS_MESG_NOBLOCK);
}

/* WHAT IT DOES: Take the SI token (creating the queue the first time),
 * waiting for it. */
/* @implements 0x80269460 tgr __osSiGetAccess */
void __osSiGetAccess(void)
{
	OSMesg dummyMesg;

	if (!__osSiAccessQueueEnabled)
		__osSiCreateAccessQueue();
	osRecvMesg(&__osSiAccessQueue, &dummyMesg, OS_MESG_BLOCK);
}

/* WHAT IT DOES: Give the SI token back. */
/* @implements 0x802694A4 tgr __osSiRelAccess */
void __osSiRelAccess(void)
{
	osSendMesg(&__osSiAccessQueue, (OSMesg)NULL, OS_MESG_NOBLOCK);
}
