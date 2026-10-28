/* n64-cflags: -O1 */
/* eepromlongread.c -- libultra's multi-block EEPROM read (io/eepromlongread.c):
 * Europe and Japan only.
 */
#include "tgr/ultra.h"

/* -- declarations -- */
#define EEPROM_MAXBLOCKS 64
#define EEPROM_BLOCK_SIZE 8
s32 osEepromRead(OSMesgQueue *mq, u8 address, u8 *buffer);
extern OSTimer __osEepromTimer;
extern OSMesgQueue __osEepromTimerQ;
extern OSMesg __osEepromTimerMsg;
/* -- end declarations -- */

#if TGR_REGION_EUR || TGR_REGION_JPN
/* WHAT IT DOES: Read length bytes of EEPROM from block address on, one
 * 8-byte block at a time, waiting 12 ms after each block; an address past
 * the last block is -1, and the first failing block's error is returned. */
/* @implements eur:0x80262E40 tgr osEepromLongRead */
/* @implements jpn:0x802628E0 tgr osEepromLongRead */
s32 osEepromLongRead(OSMesgQueue *mq, u8 address, u8 *buffer, int length)
{
	s32 ret = 0;

	if (address > EEPROM_MAXBLOCKS)
		return -1;
	while (length > 0) {
		ret = osEepromRead(mq, address, buffer);
		if (ret != 0)
			return ret;
		length -= EEPROM_BLOCK_SIZE;
		address++;
		buffer += EEPROM_BLOCK_SIZE;
		osSetTimer(&__osEepromTimer, 12000 * osClockRate / 1000000, 0, &__osEepromTimerQ, &__osEepromTimerMsg);
		osRecvMesg(&__osEepromTimerQ, NULL, OS_MESG_BLOCK);
	}
	return ret;
}
#endif
