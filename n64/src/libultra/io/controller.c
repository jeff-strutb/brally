/* n64-cflags: -O1 */
/* controller.c -- libultra's controller setup (io/controller.c).
 */
#include "tgr/ultra.h"

/* -- declarations -- */
void __osPackRequestData(u8 cmd);
void __osContGetInitData(u8 *pattern, OSContStatus *data);
/* -- end declarations -- */

u32 __osContinitialized = 0;
OSPifRam __osContPifRam;
u8 __osContLastCmd;
u8 __osMaxControllers;
OSTimer __osEepromTimer;
OSMesgQueue __osEepromTimerQ;
OSMesg __osEepromTimerMsg;

/* WHAT IT DOES: Initialise the controllers once: wait until half a second
 * after power-on, ask every port for its status through the PIF (waiting
 * on mq for each DMA), report which ports have a controller and each one's
 * type and status, and set up the SI access queue. */
/* @implements 0x80265910 tgr osContInit */
s32 osContInit(OSMesgQueue *mq, u8 *bitpattern, OSContStatus *data)
{
	OSMesg dummy;
	s32 ret = 0;
	OSTime t;
	OSTimer mytimer;
	OSMesgQueue timerMesgQueue;

	if (__osContinitialized)
		return 0;
	__osContinitialized = 1;
	t = osGetTime();
	if (t < 500000 * osClockRate / 1000000) {
		osCreateMesgQueue(&timerMesgQueue, &dummy, 1);
		osSetTimer(&mytimer, 500000 * osClockRate / 1000000 - t, 0, &timerMesgQueue, &dummy);
		osRecvMesg(&timerMesgQueue, &dummy, OS_MESG_BLOCK);
	}
	__osMaxControllers = MAXCONTROLLERS;
	__osPackRequestData(CONT_CMD_REQUEST_STATUS);
	ret = __osSiRawStartDma(OS_WRITE, __osContPifRam.ramarray);
	osRecvMesg(mq, &dummy, OS_MESG_BLOCK);
	ret = __osSiRawStartDma(OS_READ, __osContPifRam.ramarray);
	osRecvMesg(mq, &dummy, OS_MESG_BLOCK);
	__osContGetInitData(bitpattern, data);
	__osContLastCmd = CONT_CMD_REQUEST_STATUS;
	__osSiCreateAccessQueue();
	osCreateMesgQueue(&__osEepromTimerQ, &__osEepromTimerMsg, 1);
	return ret;
}

/* WHAT IT DOES: Unpack the status replies: each port's error bits, and
 * for a present controller its type and status and its bit in *pattern. */
/* @implements 0x80265B08 tgr __osContGetInitData */
void __osContGetInitData(u8 *pattern, OSContStatus *data)
{
	u8 *ptr;
	__OSContRequesFormat requestformat;
	int i;
	u8 bits;

	bits = 0;
	ptr = (u8 *)__osContPifRam.ramarray;
	for (i = 0; i < __osMaxControllers; i++, ptr += sizeof(requestformat), data++) {
		requestformat = *(__OSContRequesFormat *)ptr;
		data->errno = CHNL_ERR(requestformat);
		if (data->errno == 0) {
			data->type = (requestformat.typel << 8) | requestformat.typeh;
			data->status = requestformat.status;
			bits |= 1 << i;
		}
	}
	*pattern = bits;
}

/* WHAT IT DOES: Build the PIF command block: the given status command for
 * each controller, then the end marker. */
/* @implements 0x80265BD8 tgr __osPackRequestData */
void __osPackRequestData(u8 cmd)
{
	u8 *ptr;
	__OSContRequesFormat requestformat;
	int i;

	for (i = 0; i < ARRLEN(__osContPifRam.ramarray) + 1; i++) {
		__osContPifRam.ramarray[i] = 0;
	}
	__osContPifRam.pifstatus = CONT_CMD_EXE;
	ptr = (u8 *)__osContPifRam.ramarray;
	requestformat.align = CONT_CMD_NOP;
	requestformat.txsize = CONT_CMD_REQUEST_STATUS_TX;
	requestformat.rxsize = CONT_CMD_REQUEST_STATUS_RX;
	requestformat.poll = cmd;
	requestformat.typeh = CONT_CMD_NOP;
	requestformat.typel = CONT_CMD_NOP;
	requestformat.status = CONT_CMD_NOP;
	requestformat.align1 = CONT_CMD_NOP;

	for (i = 0; i < __osMaxControllers; i++) {
		*(__OSContRequesFormat *)ptr = requestformat;
		ptr += sizeof(requestformat);
	}
	*ptr = CONT_CMD_END;
}
