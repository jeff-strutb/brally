/* cdaudio.c: the game's CD music (br_snd.h's br_cd_*), over audio.c's CD.
 *
 * The drive answers as device 1 with its tracks counted from the files
 * host_music_dir() names; play runs from a track to a track and, when it
 * ends, posts the window the game named the message (0x3B9, wParam 1,
 * lParam the device) its window procedure answers by choosing the next
 * track. Positions are tracks. Every call but open fails with 0x101 while
 * the drive is closed. */
#include "plat.h"
#include "br_snd.h"

#define CD_DEVICE  1
#define CD_CLOSED  0x101
#define CD_ENDED   0x3B9

static int   s_open;
static void *s_notify;

static void cd_end(void *user)
{
    (void)user;
    if (s_notify)
        PostMessageA((HWND)s_notify, CD_ENDED, 1, CD_DEVICE);
}

int br_cd_open(uint32_t *pDevice, uint32_t *pLast)
{
    int first, last;
    plat_cd_tracks(&first, &last);
    s_open = 1;
    *pDevice = CD_DEVICE;
    *pLast = (uint32_t)last;
    return 0;
}

int br_cd_close(void)
{
    if (!s_open)
        return CD_CLOSED;
    plat_cd_stop();
    s_open = 0;
    return 0;
}

int br_cd_track(void)
{
    return s_open ? plat_cd_current() : 0;
}

int br_cd_play(void *notify, int from, int to)
{
    if (!s_open)
        return CD_CLOSED;
    s_notify = notify;
    plat_cd_play(from, to, cd_end, NULL);
    return 0;
}

int br_cd_stop(void)
{
    if (!s_open)
        return CD_CLOSED;
    plat_cd_stop();
    return 0;
}

int br_cd_pause(void)
{
    if (!s_open)
        return CD_CLOSED;
    plat_cd_pause(1);
    return 0;
}
