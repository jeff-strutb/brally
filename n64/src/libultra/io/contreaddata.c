/* n64-cflags: -O1 */
/* contreaddata.c -- libultra's controller reads (io/contreaddata.c).
 */
#include "tgr/ultra.h"

/* -- declarations -- */
void __osPackReadData(void);
/* -- end declarations -- */

/* WHAT IT DOES: Start reading every controller's buttons and stick
 * through the PIF (sending the read command block first unless it is the
 * last one sent, and waiting for that on mq), the block pre-filled with
 * 0xFF for the reply. */
/* @implements 0x80264760 tgr osContStartReadData */
s32 osContStartReadData(OSMesgQueue *mq)
{
	s32 ret = 0;
	s32 i;

	__osSiGetAccess();
	if (__osContLastCmd != CONT_CMD_READ_BUTTON) {
		__osPackReadData();
		ret = __osSiRawStartDma(OS_WRITE, __osContPifRam.ramarray);
		osRecvMesg(mq, NULL, OS_MESG_BLOCK);
	}
	for (i = 0; i < ARRLEN(__osContPifRam.ramarray) + 1; i++) {
		__osContPifRam.ramarray[i] = 0xFF;
	}
	__osContPifRam.pifstatus = 0;
	ret = __osSiRawStartDma(OS_READ, __osContPifRam.ramarray);
	__osContLastCmd = CONT_CMD_READ_BUTTON;
	__osSiRelAccess();
	return ret;
}

/* WHAT IT DOES: Unpack the controller reads into data[]: each pad's error
 * bits, and its buttons and stick when there is no error. */
/* @implements 0x80264824 tgr osContGetReadData */
void osContGetReadData(OSContPad *data)
{
	u8 *ptr;
	__OSContReadFormat readformat;
	int i;

	ptr = (u8 *)__osContPifRam.ramarray;
	for (i = 0; i < __osMaxControllers; i++, ptr += sizeof(__OSContReadFormat), data++) {
		readformat = *(__OSContReadFormat *)ptr;
		data->errno = CHNL_ERR(readformat);
		if (data->errno == 0) {
			data->button = readformat.button;
			data->stick_x = readformat.stick_x;
			data->stick_y = readformat.stick_y;
		}
	}
}

/* WHAT IT DOES: Build the PIF command block: a button-read command for
 * each controller, then the end marker. */
/* @implements 0x802648CC tgr __osPackReadData */
void __osPackReadData(void)
{
	u8 *ptr;
	__OSContReadFormat readformat;
	int i;

	ptr = (u8 *)__osContPifRam.ramarray;
	for (i = 0; i < ARRLEN(__osContPifRam.ramarray) + 1; i++) {
		__osContPifRam.ramarray[i] = 0;
	}

	__osContPifRam.pifstatus = CONT_CMD_EXE;
	readformat.dummy = CONT_CMD_NOP;
	readformat.txsize = CONT_CMD_READ_BUTTON_TX;
	readformat.rxsize = CONT_CMD_READ_BUTTON_RX;
	readformat.cmd = CONT_CMD_READ_BUTTON;
	readformat.button = 0xFFFF;
	readformat.stick_x = -1;
	readformat.stick_y = -1;
	for (i = 0; i < __osMaxControllers; i++) {
		*(__OSContReadFormat *)ptr = readformat;
		ptr += sizeof(__OSContReadFormat);
	}
	*ptr = CONT_CMD_END;
}
