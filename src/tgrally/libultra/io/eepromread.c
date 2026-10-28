/* n64-cflags: -O1 */
/* eepromread.c -- libultra's EEPROM block read (io/eepromread.c): Europe
 * and Japan only.
 */
#include "tgr/ultra.h"

/* -- declarations -- */
#define CONT_EEPROM 0x8000
#define CONT_EEPROM_BUSY 0x80
#define CONT_NO_RESPONSE_ERROR 0x8
#define EEPROM_MAXBLOCKS 64
#define CONT_CMD_READ_EEPROM 4
#define CONT_CMD_READ_EEPROM_TX 2
#define CONT_CMD_READ_EEPROM_RX 8
typedef struct {
	u8 txsize;
	u8 rxsize;
	u8 cmd;
	u8 address;
	u8 data[8];
} __OSContEepromFormat;
extern OSPifRam __osEepPifRam;
s32 __osEepStatus(OSMesgQueue *mq, OSContStatus *data);
void __osPackEepReadData(u8 address);
/* -- end declarations -- */

#if TGR_REGION_EUR || TGR_REGION_JPN
/* WHAT IT DOES: Read one 8-byte block from the EEPROM into buffer: with SI
 * access held, check the EEPROM answers (else no response, returned with
 * access still held), wait while it is busy, send the read command block,
 * read the reply and copy its 8 bytes out when it has no error bits. */
/* @implements eur:0x8026B020 tgr osEepromRead */
/* @implements jpn:0x8026AAD0 tgr osEepromRead */
s32 osEepromRead(OSMesgQueue *mq, u8 address, u8 *buffer)
{
	s32 ret = 0;
	int i = 0;
	u8 *ptr = (u8 *)&__osEepPifRam.ramarray;
	OSContStatus sdata;
	__OSContEepromFormat eepromformat;

	if (address > EEPROM_MAXBLOCKS)
		return -1;
	__osSiGetAccess();
	ret = __osEepStatus(mq, &sdata);
	if (ret != 0 || sdata.type != CONT_EEPROM)
		return CONT_NO_RESPONSE_ERROR;
	while (sdata.status & CONT_EEPROM_BUSY) {
		__osEepStatus(mq, &sdata);
	}
	__osPackEepReadData(address);
	ret = __osSiRawStartDma(OS_WRITE, &__osEepPifRam);
	osRecvMesg(mq, NULL, OS_MESG_BLOCK);
	for (i = 0; i < ARRLEN(__osEepPifRam.ramarray) + 1; i++) {
		__osEepPifRam.ramarray[i] = CONT_CMD_NOP;
	}
	__osEepPifRam.pifstatus = 0;
	ret = __osSiRawStartDma(OS_READ, &__osEepPifRam);
	__osContLastCmd = CONT_CMD_READ_EEPROM;
	osRecvMesg(mq, NULL, OS_MESG_BLOCK);
	for (i = 0; i < 4; i++) {
		ptr++;
	}
	eepromformat = *(__OSContEepromFormat *)ptr;
	ret = CHNL_ERR(eepromformat);
	if (ret == 0) {
		for (i = 0; i < ARRLEN(eepromformat.data); i++) {
			*buffer++ = eepromformat.data[i];
		}
	}
	__osSiRelAccess();
	return ret;
}

/* WHAT IT DOES: Build the PIF command block for an EEPROM block read: the
 * read command and the block address, room for the 8 bytes, then the end
 * marker. */
/* @implements eur:0x8026B210 tgr __osPackEepReadData */
/* @implements jpn:0x8026ACC0 tgr __osPackEepReadData */
void __osPackEepReadData(u8 address)
{
	u8 *ptr = (u8 *)&__osEepPifRam.ramarray;
	__OSContEepromFormat eepromformat;
	int i;

	for (i = 0; i < ARRLEN(__osEepPifRam.ramarray) + 1; i++) {
		__osEepPifRam.ramarray[i] = CONT_CMD_NOP;
	}
	__osEepPifRam.pifstatus = CONT_CMD_EXE;
	eepromformat.txsize = CONT_CMD_READ_EEPROM_TX;
	eepromformat.rxsize = CONT_CMD_READ_EEPROM_RX;
	eepromformat.cmd = CONT_CMD_READ_EEPROM;
	eepromformat.address = address;
	for (i = 0; i < ARRLEN(eepromformat.data); i++) {
		eepromformat.data[i] = 0;
	}
	for (i = 0; i < 4; i++) {
		*ptr++ = 0;
	}
	*(__OSContEepromFormat *)ptr = eepromformat;
	ptr += sizeof(__OSContEepromFormat);
	ptr[0] = CONT_CMD_END;
}
#endif
