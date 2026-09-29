/* n64-cflags: -O1 */
/* pidma.c -- libultra's cartridge DMA request (io/pidma.c).
 */
#include "tgr/ultra.h"

/* -- declarations -- */
/* -- end declarations -- */

/* WHAT IT DOES: Queue a cartridge DMA for the PI manager (jumped to the
 * front at high priority); -1 before the manager runs or when its queue is
 * full. */
/* @implements 0x80264650 tgr osPiStartDma */
s32 osPiStartDma(OSIoMesg *mb, s32 priority, s32 direction, u32 devAddr, void *dramAddr, u32 size, OSMesgQueue *mq)
{
	register s32 ret;

	if (!__osPiDevMgr.active)
		return -1;
	if (direction == OS_READ)
		mb->hdr.type = OS_MESG_TYPE_DMAREAD;
	else
		mb->hdr.type = OS_MESG_TYPE_DMAWRITE;

	mb->hdr.pri = priority;
	mb->hdr.retQueue = mq;
	mb->dramAddr = dramAddr;
	mb->devAddr = devAddr;
	mb->size = size;
	mb->piHandle = NULL;

	if (priority == OS_MESG_PRI_HIGH)
		ret = osJamMesg(osPiGetCmdQueue(), (OSMesg)mb, OS_MESG_NOBLOCK);
	else
		ret = osSendMesg(osPiGetCmdQueue(), (OSMesg)mb, OS_MESG_NOBLOCK);
	return ret;
}
