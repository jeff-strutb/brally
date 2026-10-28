/* n64-cflags: -O1 */
/* eepromwrite.c -- libultra's EEPROM block write and status
 * (io/eepromwrite.c): Europe and Japan only.
 */
#include "tgr/ultra.h"

/* -- declarations -- */
#define CONT_EEPROM 0x8000
#define CONT_EEPROM_BUSY 0x80
#define CONT_NO_RESPONSE_ERROR 0x8
#define EEPROM_MAXBLOCKS 64
#define CONT_CMD_WRITE_EEPROM 5
#define CONT_CMD_WRITE_EEPROM_TX 10
#define CONT_CMD_WRITE_EEPROM_RX 1
typedef struct {
	u8 txsize;
	u8 rxsize;
	u8 cmd;
	u8 address;
	u8 data[8];
} __OSContEepromFormat;
s32 __osEepStatus(OSMesgQueue *mq, OSContStatus *data);
void __osPackEepWriteData(u8 address, u8 *buffer);
/* -- end declarations -- */

#if TGR_REGION_EUR || TGR_REGION_JPN
OSPifRam __osEepPifRam;

/* WHAT IT DOES: Write one 8-byte block to the EEPROM: with SI access held,
 * check the EEPROM answers (else no response, returned with access still
 * held), wait while it is busy, send the write command block and read the
 * PIF's reply, answering the reply's error bits. */
/* @implements eur:0x8026AB40 tgr osEepromWrite */
/* @implements jpn:0x8026A5F0 tgr osEepromWrite */
s32 osEepromWrite(OSMesgQueue *mq, u8 address, u8 *buffer)
{
	s32 ret = 0;
	int i;
	u8 *ptr = (u8 *)&__osEepPifRam.ramarray;
	__OSContEepromFormat eepromformat;
	OSContStatus sdata;

	if (address > EEPROM_MAXBLOCKS)
		return -1;
	__osSiGetAccess();
	ret = __osEepStatus(mq, &sdata);
	if (ret != 0 || sdata.type != CONT_EEPROM)
		return CONT_NO_RESPONSE_ERROR;
	while (sdata.status & CONT_EEPROM_BUSY) {
		__osEepStatus(mq, &sdata);
	}
	__osPackEepWriteData(address, buffer);
	ret = __osSiRawStartDma(OS_WRITE, &__osEepPifRam);
	osRecvMesg(mq, NULL, OS_MESG_BLOCK);
	for (i = 0; i < ARRLEN(__osEepPifRam.ramarray) + 1; i++) {
		__osEepPifRam.ramarray[i] = CONT_CMD_NOP;
	}
	__osEepPifRam.pifstatus = 0;
	ret = __osSiRawStartDma(OS_READ, &__osEepPifRam);
	__osContLastCmd = CONT_CMD_WRITE_EEPROM;
	osRecvMesg(mq, NULL, OS_MESG_BLOCK);
	for (i = 0; i < 4; i++) {
		ptr++;
	}
	eepromformat = *(__OSContEepromFormat *)ptr;
	ret = CHNL_ERR(eepromformat);
	__osSiRelAccess();
	return ret;
}

/* WHAT IT DOES: Build the PIF command block for an EEPROM block write: the
 * write command, the block address and its 8 bytes, then the end marker. */
/* @implements eur:0x8026ACF0 tgr __osPackEepWriteData */
/* @implements jpn:0x8026A7A0 tgr __osPackEepWriteData */
void __osPackEepWriteData(u8 address, u8 *buffer)
{
	u8 *ptr = (u8 *)&__osEepPifRam.ramarray;
	__OSContEepromFormat eepromformat;
	int i;

	for (i = 0; i < ARRLEN(__osEepPifRam.ramarray) + 1; i++) {
		__osEepPifRam.ramarray[i] = CONT_CMD_NOP;
	}
	__osEepPifRam.pifstatus = CONT_CMD_EXE;
	eepromformat.txsize = CONT_CMD_WRITE_EEPROM_TX;
	eepromformat.rxsize = CONT_CMD_WRITE_EEPROM_RX;
	eepromformat.cmd = CONT_CMD_WRITE_EEPROM;
	eepromformat.address = address;
	for (i = 0; i < ARRLEN(eepromformat.data); i++) {
		eepromformat.data[i] = *buffer++;
	}
	for (i = 0; i < 4; i++) {
		*ptr++ = 0;
	}
	*(__OSContEepromFormat *)ptr = eepromformat;
	ptr += sizeof(__OSContEepromFormat);
	ptr[0] = CONT_CMD_END;
}

/* WHAT IT DOES: Ask the EEPROM for its status through the PIF and unpack
 * the reply into data (error bits, type, status); answers the DMA's or the
 * channel's error. */
/* @implements eur:0x8026ADFC tgr __osEepStatus */
/* @implements jpn:0x8026A8AC tgr __osEepStatus */
s32 __osEepStatus(OSMesgQueue *mq, OSContStatus *data)
{
	s32 ret = 0;
	int i;
	u8 *ptr = (u8 *)__osEepPifRam.ramarray;
	__OSContRequesFormat requestformat;

	for (i = 0; i < ARRLEN(__osEepPifRam.ramarray) + 1; i++) {
		__osEepPifRam.ramarray[i] = 0;
	}
	__osEepPifRam.pifstatus = CONT_CMD_EXE;
	ptr = (u8 *)__osEepPifRam.ramarray;
	for (i = 0; i < 4; i++) {
		*ptr++ = 0;
	}
	requestformat.align = CONT_CMD_NOP;
	requestformat.txsize = CONT_CMD_REQUEST_STATUS_TX;
	requestformat.rxsize = CONT_CMD_REQUEST_STATUS_RX;
	requestformat.poll = CONT_CMD_REQUEST_STATUS;
	requestformat.typeh = CONT_CMD_NOP;
	requestformat.typel = CONT_CMD_NOP;
	requestformat.status = CONT_CMD_NOP;
	requestformat.align1 = CONT_CMD_NOP;
	*(__OSContRequesFormat *)ptr = requestformat;
	ptr += sizeof(__OSContRequesFormat);
	*ptr = CONT_CMD_END;
	ret = __osSiRawStartDma(OS_WRITE, &__osEepPifRam);
	osRecvMesg(mq, NULL, OS_MESG_BLOCK);
	__osContLastCmd = CONT_CMD_WRITE_EEPROM;
	ret = __osSiRawStartDma(OS_READ, &__osEepPifRam);
	osRecvMesg(mq, NULL, OS_MESG_BLOCK);
	if (ret != 0)
		return ret;
	ptr = (u8 *)__osEepPifRam.ramarray;
	for (i = 0; i < 4; i++) {
		*ptr++ = 0;
	}
	requestformat = *(__OSContRequesFormat *)ptr;
	data->errno = CHNL_ERR(requestformat);
	data->type = (requestformat.typel << 8) | requestformat.typeh;
	data->status = requestformat.status;
	if (data->errno != 0)
		return data->errno;
	return 0;
}
#endif
