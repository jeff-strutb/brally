/* br_musiccmd.h -- audio: send a command to the live music path, and
 * write rows of the sound/entity table.
 *
 * Responsibility: sound and music.
 */
#ifndef BR_MUSICCMD_H
#define BR_MUSICCMD_H

#include <stdint.h>

/* 0x100025C0  CD audio if that mode is on, otherwise the EAR mixer. */
void BrDispatch_100025C0(void *pCmd);
/* 0x10072B80 / 0x10072B10 / 0x10072A70  write a sound-table row with
 * slightly different index packing. */
void BrWrap_10072B80(void *pTable, int index, int value);
void BrWrap_10072B10(void *pTable, int index, int value);
void BrWrap_10072A70(void *pTable, int index, int value);

extern uint32_t g_0940A4;

#endif
