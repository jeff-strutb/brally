/* ear.c: the EAR "Interactive Around-Sound" engine the game loads by name
 * (earias.dll, earpds.dll), answered as the 32-bit lane answers it: a
 * working engine that makes no sound. Registration and start calls hand
 * back a non-zero id, InitializeEar succeeds, status queries report idle.
 * The game calls these through function pointers with their own argument
 * counts; each ignores its arguments. */
#include "plat.h"

#define EAR(name, value) static INT_PTR ear_##name(void) { return value; }
EAR(AAA_Validate, 1)
EAR(AssignHwnd, 1)
EAR(InitializeEar, 1)
EAR(GetLastError, 0)
EAR(ShowLastError, 0)
EAR(GetVersion, 0x100)
EAR(EarInactive, 0)
EAR(GetEventStatus, 0)
EAR(RegisterBank, 1)
EAR(RegisterChannel, 1)
EAR(RegisterEnvironment, 1)
EAR(RegisterMatrix, 1)
EAR(RegisterPreset, 1)
EAR(StartEvent, 1)
EAR(MixEvent, 1)
EAR(MoveEvent, 1)
EAR(StartTimer, 1)
EAR(UpdateEar, 1)
EAR(ResetEar, 1)
EAR(Zero, 0)

#define X(n, at, f) { "_EAR_DLL_" #n "@" #at, (void *)ear_##f }
static const plat_export k_ear[] = {
    X(AAA_Validate, 4, AAA_Validate), X(AssignHwnd, 4, AssignHwnd),
    X(InitializeEar, 4, InitializeEar), X(GetLastError, 0, GetLastError),
    X(ShowLastError, 0, ShowLastError), X(GetVersion, 0, GetVersion),
    X(EarInactive, 0, EarInactive), X(GetEventStatus, 8, GetEventStatus),
    X(RegisterBank, 8, RegisterBank), X(RegisterChannel, 16, RegisterChannel),
    X(RegisterEnvironment, 4, RegisterEnvironment), X(RegisterMatrix, 4, RegisterMatrix),
    X(RegisterPreset, 8, RegisterPreset), X(StartEvent, 4, StartEvent),
    X(MixEvent, 4, MixEvent), X(MoveEvent, 4, MoveEvent),
    X(StartTimer, 0, StartTimer), X(UpdateEar, 0, UpdateEar), X(ResetEar, 0, ResetEar),
    X(ShutDownTimer, 0, Zero), X(ShutDownPreset, 4, Zero), X(ShutDownMatrix, 4, Zero),
    X(ShutDownEvent, 8, Zero), X(ShutDownEnvironment, 4, Zero), X(ShutDownEar, 0, Zero),
    X(ShutDownChannel, 4, Zero), X(ShutDownBank, 4, Zero), X(SetUserDistanceUnit, 8, Zero),
    X(SetAttenuationLevel, 8, Zero), X(ClearChannel, 8, Zero), X(ChangeChannelControl, 8, Zero),
};

void plat_ear_init(void)
{
    plat_register_module("earias.dll", k_ear, (int)(sizeof k_ear / sizeof k_ear[0]));
    plat_register_module("earpds.dll", k_ear, (int)(sizeof k_ear / sizeof k_ear[0]));
}
