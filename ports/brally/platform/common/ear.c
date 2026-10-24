/* ear.c: the EAR "Interactive Around-Sound" engine (br_ear.h), which the
 * original loaded as a DLL (earias.dll, earpds.dll) and the port links in.
 * The game uses it for one thing that makes a sound: the CD channel, its
 * default music route (PlayMusic=2). That channel plays the disc's tracks
 * through audio.c:
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
#include "br_ear.h"

#define EAR(name, value) intptr_t br_ear_##name(void) { return value; }
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
EAR(ShutDownTimer, 0)
EAR(ShutDownPreset, 0)
EAR(ShutDownMatrix, 0)
EAR(ShutDownEvent, 0)
EAR(ShutDownEnvironment, 0)
EAR(ShutDownEar, 0)
EAR(ShutDownChannel, 0)
EAR(ShutDownBank, 0)
EAR(SetUserDistanceUnit, 0)

/* ---- the CD channel --------------------------------------------------------- */
#define EAR_CD_EVENT   0x4544

static void *s_ear_hwnd;
static intptr_t s_cd_ch = -1;          /* the channel the game registered as the CD */

intptr_t br_ear_AssignHwnd(void *h) { s_ear_hwnd = h; return 1; }

/* (channel, type, volume, ?): the game numbers its channels itself */
intptr_t br_ear_RegisterChannel(intptr_t ch, intptr_t type, intptr_t vol, intptr_t d)
{
    (void)d;
    if (type == 6) {
        s_cd_ch = ch;
        plat_cd_volume((float)vol / 10000.0f);
    }
    return 1;
}

intptr_t br_ear_ChangeChannelControl(intptr_t ch, intptr_t cmd)
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
        msg = br_os_message_id("EAR Interactive Around-Sound");
    if (s_ear_hwnd)
        br_os_post((HWND)s_ear_hwnd, msg, (WPARAM)EAR_CD_EVENT, 2);
}

/* the event block: +0x04 the track */
intptr_t br_ear_MixEvent(const void *ev)
{
    int track = ev ? *(const int32_t *)((const char *)ev + 4) : 0;
    if (track <= 0)
        return 1;
    plat_cd_play(track, track, ear_cd_end, NULL);
    return EAR_CD_EVENT;
}

intptr_t br_ear_ClearChannel(intptr_t ch, intptr_t b)
{
    (void)b;
    if (ch == s_cd_ch)
        plat_cd_stop();
    return 0;
}

intptr_t br_ear_SetAttenuationLevel(intptr_t ch, intptr_t level)
{
    if (ch == s_cd_ch)
        plat_cd_volume((float)level / 10000.0f);
    return 0;
}
