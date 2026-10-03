/* ear.c: the EAR "Interactive Around-Sound" engine the game loads by name
 * (earias.dll, earpds.dll). The game uses it for one thing that makes a
 * sound: the CD channel, its default music route (PlayMusic=2). That
 * channel plays the disc's tracks through audio.c:
 *
 *   RegisterChannel(ch, 6, volume, ?)  channel ch is the CD
 *   ChangeChannelControl(ch, cmd)      0x10000020 / 0x10000040 the first and
 *                                      last track, 4 pause, 0xC resume
 *   MixEvent(event)                    play event->track (the event block's
 *                                      +0x04); the id it answers comes back
 *                                      with the end-of-track message
 *   ClearChannel(ch, ?)                stop (0 = done)
 *   SetAttenuationLevel(ch, level)     the volume, 0..10000
 *
 * and, when a track ends, the window gets the registered message "EAR
 * Interactive Around-Sound" with wParam the event id and lParam 2, which
 * the game answers by asking for the next track (br_input.c). Every other
 * entry point is a working engine that makes no sound: registration and
 * start calls hand back a non-zero id, status queries report idle. */
#include "plat.h"

#define EAR(name, value) static INT_PTR ear_##name(void) { return value; }
EAR(AAA_Validate, 1)
EAR(InitializeEar, 1)
EAR(GetLastError, 0)
EAR(ShowLastError, 0)
EAR(GetVersion, 0x100)
EAR(EarInactive, 0)
EAR(GetEventStatus, 0)
EAR(RegisterBank, 1)
EAR(RegisterEnvironment, 1)
EAR(RegisterMatrix, 1)
EAR(RegisterPreset, 1)
EAR(StartEvent, 1)
EAR(MoveEvent, 1)
EAR(StartTimer, 1)
EAR(UpdateEar, 1)
EAR(ResetEar, 1)
EAR(Zero, 0)

/* ---- the CD channel --------------------------------------------------------- */
#define EAR_CD_EVENT   0x4544

static HWND s_ear_hwnd;
static INT_PTR s_cd_ch = -1;          /* the channel the game registered as the CD */

static INT_PTR WINAPI ear_AssignHwnd2(HWND h) { s_ear_hwnd = h; return 1; }

/* (channel, type, volume, ?): the game numbers its channels itself */
static INT_PTR WINAPI ear_RegisterChannel2(INT_PTR ch, INT_PTR type, INT_PTR vol, INT_PTR d)
{
    (void)d;
    if (type == 6) {
        s_cd_ch = ch;
        plat_cd_volume((float)vol / 10000.0f);
    }
    return 1;
}

static INT_PTR WINAPI ear_ChangeChannelControl2(INT_PTR ch, INT_PTR cmd)
{
    int first, last;
    if (ch != s_cd_ch)
        return 0;
    plat_cd_tracks(&first, &last);
    switch ((uint32_t)cmd) {
    case 0x10000020: return first;
    case 0x10000040: return last;
    case 4:          plat_cd_pause(1); return 1;
    case 0xC:        plat_cd_pause(0); return 1;
    }
    return 0;
}

static void ear_cd_end(void *user)
{
    static UINT msg;
    (void)user;
    if (!msg)
        msg = RegisterWindowMessageA("EAR Interactive Around-Sound");
    if (s_ear_hwnd)
        PostMessageA(s_ear_hwnd, msg, (WPARAM)EAR_CD_EVENT, 2);
}

/* the event block: +0x04 the track */
static INT_PTR WINAPI ear_MixEvent2(const void *ev)
{
    int track = ev ? *(const int32_t *)((const char *)ev + 4) : 0;
    if (track <= 0)
        return 1;
    plat_cd_play(track, track, ear_cd_end, NULL);
    return EAR_CD_EVENT;
}

static INT_PTR WINAPI ear_ClearChannel2(INT_PTR ch, INT_PTR b)
{
    (void)b;
    if (ch == s_cd_ch)
        plat_cd_stop();
    return 0;
}

static INT_PTR WINAPI ear_SetAttenuationLevel2(INT_PTR ch, INT_PTR level)
{
    if (ch == s_cd_ch)
        plat_cd_volume((float)level / 10000.0f);
    return 0;
}

#define X(n, at, f) { "_EAR_DLL_" #n "@" #at, (void *)ear_##f }
static const plat_export k_ear[] = {
    X(AAA_Validate, 4, AAA_Validate), X(AssignHwnd, 4, AssignHwnd2),
    X(InitializeEar, 4, InitializeEar), X(GetLastError, 0, GetLastError),
    X(ShowLastError, 0, ShowLastError), X(GetVersion, 0, GetVersion),
    X(EarInactive, 0, EarInactive), X(GetEventStatus, 8, GetEventStatus),
    X(RegisterBank, 8, RegisterBank), X(RegisterChannel, 16, RegisterChannel2),
    X(RegisterEnvironment, 4, RegisterEnvironment), X(RegisterMatrix, 4, RegisterMatrix),
    X(RegisterPreset, 8, RegisterPreset), X(StartEvent, 4, StartEvent),
    X(MixEvent, 4, MixEvent2), X(MoveEvent, 4, MoveEvent),
    X(StartTimer, 0, StartTimer), X(UpdateEar, 0, UpdateEar), X(ResetEar, 0, ResetEar),
    X(ShutDownTimer, 0, Zero), X(ShutDownPreset, 4, Zero), X(ShutDownMatrix, 4, Zero),
    X(ShutDownEvent, 8, Zero), X(ShutDownEnvironment, 4, Zero), X(ShutDownEar, 0, Zero),
    X(ShutDownChannel, 4, Zero), X(ShutDownBank, 4, Zero), X(SetUserDistanceUnit, 8, Zero),
    X(SetAttenuationLevel, 8, SetAttenuationLevel2), X(ClearChannel, 8, ClearChannel2),
    X(ChangeChannelControl, 8, ChangeChannelControl2),
};

void plat_ear_init(void)
{
    plat_register_module("earias.dll", k_ear, (int)(sizeof k_ear / sizeof k_ear[0]));
    plat_register_module("earpds.dll", k_ear, (int)(sizeof k_ear / sizeof k_ear[0]));
}
