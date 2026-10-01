/* br_musiccmd.h -- audio: send a command to the live music path, and
 * write rows of the sound/entity table.
 *
 * Responsibility: sound and music.
 */
#ifndef BR_MUSICCMD_H
#define BR_MUSICCMD_H
#ifdef __cplusplus
extern "C" {  /* BR_CLINK_BEGIN: every original function has C linkage */
#endif

#include <stdint.h>

/* 0x100025C0  CD audio if that mode is on, otherwise the EAR mixer. */
/* BrDispatch_100025C0: prototype in br_funcs.h */
/* 0x10072B80 / 0x10072B10 / 0x10072A70  write a sound-table row with
 * slightly different index packing. */
/* BrWrap_10072B80: prototype in br_funcs.h */
/* BrWrap_10072B10: prototype in br_funcs.h */
/* BrWrap_10072A70: prototype in br_funcs.h */

/* 64-bit core: declared once, in br_globals.h or its struct's header */

#ifdef __cplusplus
}  /* BR_CLINK_END */
#endif
#endif
