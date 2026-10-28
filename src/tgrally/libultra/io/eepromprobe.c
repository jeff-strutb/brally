/* n64-cflags: -O1 */
/* eepromprobe.c -- libultra's EEPROM probe (io/eepromprobe.c): Europe and
 * Japan only, which save to an EEPROM beside the Controller Pak.
 */
#include "tgr/ultra.h"

/* -- declarations -- */
#define CONT_EEPROM 0x8000
#define EEPROM_TYPE_4K 1
s32 __osEepStatus(OSMesgQueue *mq, OSContStatus *data);
/* -- end declarations -- */

#if TGR_REGION_EUR || TGR_REGION_JPN
/* WHAT IT DOES: Answer whether the cartridge has a 4 Kbit EEPROM: ask the
 * PIF for its status (with SI access held) and look for the EEPROM bit. */
/* @implements eur:0x80262DD0 tgr osEepromProbe */
/* @implements jpn:0x80262870 tgr osEepromProbe */
s32 osEepromProbe(OSMesgQueue *mq)
{
	s32 ret = 0;
	OSContStatus sdata;

	__osSiGetAccess();
	ret = __osEepStatus(mq, &sdata);
	if (ret == 0 && (sdata.type & CONT_EEPROM) != 0) {
		ret = EEPROM_TYPE_4K;
	} else {
		ret = 0;
	}
	__osSiRelAccess();
	return ret;
}
#endif
