/* n64-cflags: -O1 */
/* pimgr.c -- libultra's PI manager start (io/pimgr.c).
 */
#include "tgr/ultra.h"

/* -- declarations -- */
/* -- end declarations -- */

OSDevMgr __osPiDevMgr = {0};
static OSThread piThread;
static char piThreadStack[OS_PIM_STACKSIZE];
static OSMesgQueue piEventQueue;
static OSMesg piEventBuf[1];

/* WHAT IT DOES: Start the PI manager once: its command queue, the PI
 * event queue and access lock, and its thread (created at priority pri,
 * the caller raised to it meanwhile) running the device manager with raw
 * and handle-based cartridge DMA. */
/* @implements 0x80266760 tgr osCreatePiManager */
void osCreatePiManager(OSPri pri, OSMesgQueue *cmdQ, OSMesg *cmdBuf, s32 cmdMsgCnt)
{
	u32 savedMask;
	OSPri oldPri;
	OSPri myPri;

	if (__osPiDevMgr.active)
		return;

	osCreateMesgQueue(cmdQ, cmdBuf, cmdMsgCnt);
	osCreateMesgQueue(&piEventQueue, (OSMesg *)&piEventBuf, 1);
	if (!__osPiAccessQueueEnabled)
		__osPiCreateAccessQueue();
	osSetEventMesg(OS_EVENT_PI, &piEventQueue, (OSMesg)0x22222222);
	oldPri = -1;
	myPri = osGetThreadPri(NULL);
	if (myPri < pri) {
		oldPri = myPri;
		osSetThreadPri(NULL, pri);
	}
	savedMask = __osDisableInt();
	__osPiDevMgr.active = 1;
	__osPiDevMgr.thread = &piThread;
	__osPiDevMgr.cmdQueue = cmdQ;
	__osPiDevMgr.evtQueue = &piEventQueue;
	__osPiDevMgr.acsQueue = &__osPiAccessQueue;
	__osPiDevMgr.dma = osPiRawStartDma;
	__osPiDevMgr.edma = osEPiRawStartDma;
	osCreateThread(&piThread, 0, __osDevMgrMain, &__osPiDevMgr, &piThreadStack[OS_PIM_STACKSIZE], pri);
	osStartThread(&piThread);
	__osRestoreInt(savedMask);
	if (oldPri != -1) {
		osSetThreadPri(NULL, oldPri);
	}
}
