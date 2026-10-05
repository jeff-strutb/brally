/* n64-cflags: -O1 */
/* piacs.c -- libultra's parallel interface access lock (io/piacs.c).
 */
#include "tgr/ultra.h"

/* -- declarations -- */
/* -- end declarations -- */

u32 __osPiAccessQueueEnabled = 0;
static OSMesg piAccessBuf[PI_Q_BUF_LEN];
OSMesgQueue __osPiAccessQueue;

/* WHAT IT DOES: Create the PI access queue holding one token. */
/* @implements 0x8026C5C0 tgr __osPiCreateAccessQueue */
void __osPiCreateAccessQueue(void)
{
	__osPiAccessQueueEnabled = 1;
	osCreateMesgQueue(&__osPiAccessQueue, piAccessBuf, PI_Q_BUF_LEN);
	osSendMesg(&__osPiAccessQueue, (OSMesg)NULL, OS_MESG_NOBLOCK);
}

/* WHAT IT DOES: Take the PI token (creating the queue the first time),
 * waiting for it. */
/* @implements 0x8026C610 tgr __osPiGetAccess */
void __osPiGetAccess(void)
{
	OSMesg dummyMesg;

	if (!__osPiAccessQueueEnabled)
		__osPiCreateAccessQueue();
	osRecvMesg(&__osPiAccessQueue, &dummyMesg, OS_MESG_BLOCK);
}

/* WHAT IT DOES: Give the PI token back. */
/* @implements 0x8026C654 tgr __osPiRelAccess */
void __osPiRelAccess(void)
{
	osSendMesg(&__osPiAccessQueue, (OSMesg)NULL, OS_MESG_NOBLOCK);
}
