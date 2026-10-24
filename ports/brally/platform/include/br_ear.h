/* br_ear.h: the EAR "Interactive Around-Sound" engine (platform/common/ear.c),
 * linked in where the original loaded earias.dll or earpds.dll. The game
 * keeps a table of its thirty-one entry points (BrEarLoad) and calls the
 * engine through it. */
#ifndef BR_EAR_H
#define BR_EAR_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
intptr_t br_ear_AAA_Validate(void);
intptr_t br_ear_AssignHwnd(void *h);
intptr_t br_ear_ChangeChannelControl(intptr_t ch, intptr_t cmd);
intptr_t br_ear_ClearChannel(intptr_t ch, intptr_t b);
intptr_t br_ear_EarInactive(void);
intptr_t br_ear_GetEventStatus(void);
intptr_t br_ear_GetLastError(void);
intptr_t br_ear_GetVersion(void);
intptr_t br_ear_InitializeEar(void);
intptr_t br_ear_MixEvent(const void *ev);
intptr_t br_ear_MoveEvent(void);
intptr_t br_ear_RegisterBank(void);
intptr_t br_ear_RegisterChannel(intptr_t ch, intptr_t type, intptr_t vol, intptr_t d);
intptr_t br_ear_RegisterEnvironment(void);
intptr_t br_ear_RegisterMatrix(void);
intptr_t br_ear_RegisterPreset(void);
intptr_t br_ear_ResetEar(void);
intptr_t br_ear_SetAttenuationLevel(intptr_t ch, intptr_t level);
intptr_t br_ear_SetUserDistanceUnit(void);
intptr_t br_ear_ShowLastError(void);
intptr_t br_ear_ShutDownBank(void);
intptr_t br_ear_ShutDownChannel(void);
intptr_t br_ear_ShutDownEar(void);
intptr_t br_ear_ShutDownEnvironment(void);
intptr_t br_ear_ShutDownEvent(void);
intptr_t br_ear_ShutDownMatrix(void);
intptr_t br_ear_ShutDownPreset(void);
intptr_t br_ear_StartEvent(void);
intptr_t br_ear_StartTimer(void);
intptr_t br_ear_ShutDownTimer(void);
intptr_t br_ear_UpdateEar(void);
#ifdef __cplusplus
}
#endif
#endif
