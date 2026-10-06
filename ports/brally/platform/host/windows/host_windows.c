/* host_windows.c: the Windows host -- a window, the keyboard, mouse and game
 * controllers, sound out and decoded music. The counterpart of
 * host/macos/host_macos.m; host/win32 under it supplies time, threads,
 * files, network and processes.
 *
 * The game keeps its own loop on the main thread and pumps messages through
 * PeekMessage / GetMessage in the Win32 emulation, so this host never runs a
 * message loop of its own: host_poll_event drains the real queue. Frames
 * arrive as ARGB pixels through host_present (the software renderer), shown
 * scaled with StretchDIBits, or are drawn by the Vulkan renderer into the
 * window (host_win32_window).
 *
 *   keys      a key message carries the virtual key, and its scan code is the
 *             DirectInput code (an extended key adds 0x80)
 *   sound     WASAPI, shared mode, 32-bit float, on its own thread
 *   music     Media Foundation's source reader (FLAC is built in from
 *             Windows 10 on), resampled to the mixer's rate
 *   pads      XInput's first controller
 *
 * Directories (environment, else the defaults):
 *   BR_CDROOT    the CD's files     (default reference/brally/data\disc, then disc\ beside the exe)
 *   BR_GAMEDIR   the install        (default: the CD root)
 *   BR_SAVEDIR   saves and settings (default %APPDATA%\Boss Rally 64)
 *   BR_MUSICDIR  the CD's tracks    (default music\cd beside the exe)
 */
#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#define CINTERFACE
#include <windows.h>
#include <windowsx.h>
#include <initguid.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <shlobj.h>
#include <xinput.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "host.h"

static char s_cd[MAX_PATH], s_game[MAX_PATH], s_save[MAX_PATH], s_music[MAX_PATH];
static const char *s_app_dir = "Boss Rally 64", *s_app_title = "Boss Rally";

void host_set_app_name(const char *dir, const char *title)
{
    if (dir)
        s_app_dir = dir;
    if (title)
        s_app_title = title;
}

/* ---- process -------------------------------------------------------------------- */
static int is_dir(const char *p)
{
    DWORD a = GetFileAttributesA(p);
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}

static void exe_dir(char *out, size_t n)
{
    char *s;
    GetModuleFileNameA(NULL, out, (DWORD)n);
    s = strrchr(out, '\\');
    if (s)
        *s = 0;
}

void host_init(int argc, char **argv)
{
    const char *e;
    char here[MAX_PATH];
    (void)argc;
    (void)argv;
    exe_dir(here, sizeof here);
    e = getenv("BR_CDROOT");
    if (e)
        snprintf(s_cd, sizeof s_cd, "%s", e);
    else if (is_dir("reference/brally/data\\disc"))
        snprintf(s_cd, sizeof s_cd, "reference/brally/data\\disc");
    else
        snprintf(s_cd, sizeof s_cd, "%s\\disc", here);
    e = getenv("BR_GAMEDIR");
    snprintf(s_game, sizeof s_game, "%s", e ? e : s_cd);
    e = getenv("BR_SAVEDIR");
    if (e) {
        snprintf(s_save, sizeof s_save, "%s", e);
    } else {
        char app[MAX_PATH];
        if (SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, app) == S_OK)
            snprintf(s_save, sizeof s_save, "%s\\%s", app, s_app_dir);
        else
            snprintf(s_save, sizeof s_save, "%s\\save", here);
    }
    host_mkdir(s_save);
    e = getenv("BR_MUSICDIR");
    if (e)
        snprintf(s_music, sizeof s_music, "%s", e);
    else if (is_dir("build\\app\\extract\\music\\cd"))
        snprintf(s_music, sizeof s_music, "build\\app\\extract\\music\\cd");
    else
        snprintf(s_music, sizeof s_music, "%s\\music\\cd", here);
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    MFStartup(MF_VERSION, MFSTARTUP_LITE);
}

void host_shutdown(void)
{
    host_audio_close();
    host_window_close();
    MFShutdown();
}

const char *host_game_dir(void) { return s_game; }
const char *host_cd_dir(void)   { return s_cd; }
const char *host_save_dir(void) { return s_save; }
const char *host_music_dir(void) { return is_dir(s_music) ? s_music : NULL; }

/* ---- the event queue the window fills and host_poll_event drains ------------------- */
#define QMAX 256
static host_event s_q[QMAX];
static int s_qh, s_qt;
static uint8_t s_held[256];          /* by DirectInput code: held */

static void qpush(const host_event *e)
{
    int n = (s_qt + 1) % QMAX;
    if (n != s_qh) {
        s_q[s_qt] = *e;
        s_qt = n;
    }
}

static void key_event(WPARAM vk, LPARAM lp, int down)
{
    host_event e;
    int dik = (int)((lp >> 16) & 0xFF) | ((lp & (1 << 24)) ? 0x80 : 0);
    /* left and right modifiers: the virtual key names the side */
    if (vk == VK_SHIFT)
        dik = (MapVirtualKeyA((UINT)((lp >> 16) & 0xFF), MAPVK_VSC_TO_VK_EX) == VK_RSHIFT) ? 0x36 : 0x2A;
    if (dik == 0)
        return;
    if (s_held[dik] == down)
        return;                       /* auto-repeat */
    s_held[dik] = (uint8_t)down;
    memset(&e, 0, sizeof e);
    e.type = HOST_EV_KEY;
    e.vk = (int)vk;
    e.scan = dik;
    e.down = down;
    qpush(&e);
}

/* ---- the window ----------------------------------------------------------------- */
static HWND s_win;
static int s_w = 640, s_h = 480;

static void client_point(LPARAM lp, int *x, int *y)
{
    RECT r;
    GetClientRect(s_win, &r);
    *x = r.right > 0 ? GET_X_LPARAM(lp) * s_w / r.right : 0;
    *y = r.bottom > 0 ? GET_Y_LPARAM(lp) * s_h / r.bottom : 0;
}

static LRESULT CALLBACK wnd_proc(HWND h, UINT m, WPARAM wp, LPARAM lp)
{
    host_event e;
    memset(&e, 0, sizeof e);
    switch (m) {
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        key_event(wp, lp, 1);
        if (m == WM_SYSKEYDOWN && wp != VK_F4)
            return 0;                 /* Alt alone must not open the window menu */
        break;
    case WM_KEYUP:
    case WM_SYSKEYUP:
        key_event(wp, lp, 0);
        return 0;
    case WM_CHAR:
        if (wp < 0x80 && wp != 0x7F && (wp >= 0x20 || wp == 0x0D || wp == 0x08 || wp == 0x1B)) {
            e.type = HOST_EV_CHAR;
            e.ch = (int)wp;
            qpush(&e);
        }
        return 0;
    case WM_MOUSEMOVE:
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
        e.type = HOST_EV_MOUSE;
        client_point(lp, &e.x, &e.y);
        e.buttons = (wp & MK_LBUTTON) ? 1 : 0;
        qpush(&e);
        if (m == WM_LBUTTONDOWN)
            SetCapture(h);
        else if (m == WM_LBUTTONUP)
            ReleaseCapture();
        return 0;
    case WM_SETFOCUS:
    case WM_KILLFOCUS: {
        int i;
        if (m == WM_KILLFOCUS)        /* keys held when focus leaves go up */
            for (i = 0; i < 256; i++)
                if (s_held[i]) {
                    host_event k;
                    memset(&k, 0, sizeof k);
                    k.type = HOST_EV_KEY;
                    k.vk = (int)MapVirtualKeyA((UINT)(i & 0x7F), MAPVK_VSC_TO_VK);
                    k.scan = i;
                    k.down = 0;
                    s_held[i] = 0;
                    qpush(&k);
                }
        e.type = HOST_EV_FOCUS;
        e.down = m == WM_SETFOCUS;
        qpush(&e);
        return 0;
    }
    case WM_CLOSE:
        e.type = HOST_EV_CLOSE;
        qpush(&e);
        return 0;                     /* the game decides, through WM_CLOSE */
    case WM_ERASEBKGND:
        return 1;
    case WM_SETCURSOR:
        if (LOWORD(lp) == HTCLIENT) {     /* the game draws its own cursor */
            SetCursor(NULL);
            return TRUE;
        }
        break;
    }
    return DefWindowProcA(h, m, wp, lp);
}

int host_window_open(int width, int height, const char *title)
{
    WNDCLASSA wc;
    RECT r;
    DWORD style = WS_OVERLAPPEDWINDOW;
    if (s_win)
        return 1;
    s_w = width;
    s_h = height;
    memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = GetModuleHandleA(NULL);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = "BossRally64";
    RegisterClassA(&wc);
    SetProcessDPIAware();
    r.left = r.top = 0;
    r.right = width * 2;
    r.bottom = height * 2;
    AdjustWindowRect(&r, style, FALSE);
    s_win = CreateWindowA("BossRally64", title ? title : s_app_title, style, CW_USEDEFAULT, CW_USEDEFAULT,
                          r.right - r.left, r.bottom - r.top, NULL, NULL, wc.hInstance, NULL);
    if (!s_win)
        return 0;
    ShowWindow(s_win, SW_SHOW);
    SetForegroundWindow(s_win);
    return 1;
}

void host_window_close(void)
{
    if (s_win) {
        DestroyWindow(s_win);
        s_win = NULL;
    }
}

/* the window and module, for a renderer that draws into it (render/vulkan) */
void *host_win32_window(void) { return s_win; }
void *host_win32_instance(void) { return GetModuleHandleA(NULL); }

/* the window can be any shape; the renderer letterboxes what does not fill it */
void host_window_lock_aspect(int lock) { (void)lock; }

int host_window_visible(void)
{
    return s_win && IsWindowVisible(s_win) && !IsIconic(s_win);
}

void host_present(const uint32_t *argb, int w, int h)
{
    BITMAPINFO bi;
    RECT r;
    HDC dc;
    int dw, dh, ox, oy;
    if (!s_win || !argb)
        return;
    memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = sizeof bi.bmiHeader;
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;        /* top row first */
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    GetClientRect(s_win, &r);
    /* letterboxed, aspect kept */
    dw = r.right;
    dh = r.right * h / w;
    if (dh > r.bottom) {
        dh = r.bottom;
        dw = r.bottom * w / h;
    }
    ox = (r.right - dw) / 2;
    oy = (r.bottom - dh) / 2;
    dc = GetDC(s_win);
    SetStretchBltMode(dc, COLORONCOLOR);
    StretchDIBits(dc, ox, oy, dw, dh, 0, 0, w, h, argb, &bi, DIB_RGB_COLORS, SRCCOPY);
    ReleaseDC(s_win, dc);
}

int host_poll_event(host_event *ev, uint32_t wait_ms)
{
    MSG m;
    while (s_qh == s_qt) {
        if (PeekMessageA(&m, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&m);
            DispatchMessageA(&m);
            continue;
        }
        if (wait_ms == 0)
            return 0;
        if (MsgWaitForMultipleObjects(0, NULL, FALSE, wait_ms == 0xFFFFFFFFu ? INFINITE : wait_ms,
                                      QS_ALLINPUT) == WAIT_TIMEOUT)
            return 0;
        wait_ms = 0;
    }
    *ev = s_q[s_qh];
    s_qh = (s_qh + 1) % QMAX;
    return 1;
}

void host_message_box(const char *text, const char *caption)
{
    MessageBoxA(s_win, text ? text : "", caption ? caption : "", MB_OK);
}

/* ---- audio out: WASAPI ------------------------------------------------------------ */
/* KSDATAFORMAT_SUBTYPE_IEEE_FLOAT (ksmedia.h), spelled here */
static const GUID k_subtype_float = { 0x00000003, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71 } };
static IAudioClient        *s_ac;
static IAudioRenderClient  *s_rc;
static HANDLE               s_aev, s_athread;
static volatile LONG        s_astop;
static host_audio_fn        s_afn;
static void                *s_auser;
static UINT32               s_abuf;
static int                  s_ach;

static DWORD WINAPI audio_main(LPVOID p)
{
    float tmp[2 * 4096];
    (void)p;
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    while (!s_astop) {
        UINT32 pad = 0, n, k;
        BYTE *buf;
        WaitForSingleObject(s_aev, 100);
        if (FAILED(IAudioClient_GetCurrentPadding(s_ac, &pad)))
            continue;
        n = s_abuf - pad;
        while (n > 0) {
            UINT32 m = n > 4096 ? 4096 : n;
            if (FAILED(IAudioRenderClient_GetBuffer(s_rc, m, &buf)))
                break;
            s_afn(tmp, (int)m, s_auser);
            /* the mixer is stereo; the device may have more channels */
            for (k = 0; k < m; k++) {
                float *o = (float *)buf + (size_t)k * (size_t)s_ach;
                int c;
                o[0] = tmp[2 * k];
                if (s_ach > 1)
                    o[1] = tmp[2 * k + 1];
                for (c = 2; c < s_ach; c++)
                    o[c] = 0;
            }
            IAudioRenderClient_ReleaseBuffer(s_rc, m, 0);
            n -= m;
        }
    }
    return 0;
}

int host_audio_open(int rate, host_audio_fn fn, void *user)
{
    IMMDeviceEnumerator *en = NULL;
    IMMDevice *dev = NULL;
    WAVEFORMATEXTENSIBLE wf;
    WAVEFORMATEX *mix = NULL;
    if (s_ac)
        return 1;
    if (FAILED(CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, &IID_IMMDeviceEnumerator, (void **)&en)))
        return 0;
    if (FAILED(IMMDeviceEnumerator_GetDefaultAudioEndpoint(en, eRender, eConsole, &dev))) {
        IMMDeviceEnumerator_Release(en);
        return 0;
    }
    IMMDeviceEnumerator_Release(en);
    if (FAILED(IMMDevice_Activate(dev, &IID_IAudioClient, CLSCTX_ALL, NULL, (void **)&s_ac))) {
        IMMDevice_Release(dev);
        return 0;
    }
    IMMDevice_Release(dev);
    IAudioClient_GetMixFormat(s_ac, &mix);
    /* float at the mixer's rate; the shared-mode engine converts */
    memset(&wf, 0, sizeof wf);
    wf.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    wf.Format.nChannels = (WORD)(mix && mix->nChannels ? mix->nChannels : 2);
    wf.Format.nSamplesPerSec = (DWORD)rate;
    wf.Format.wBitsPerSample = 32;
    wf.Format.nBlockAlign = (WORD)(wf.Format.nChannels * 4);
    wf.Format.nAvgBytesPerSec = wf.Format.nSamplesPerSec * wf.Format.nBlockAlign;
    wf.Format.cbSize = 22;
    wf.Samples.wValidBitsPerSample = 32;
    wf.dwChannelMask = wf.Format.nChannels == 2 ? (SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT)
                       : mix && mix->wFormatTag == WAVE_FORMAT_EXTENSIBLE ? ((WAVEFORMATEXTENSIBLE *)mix)->dwChannelMask : 0;
    wf.SubFormat = k_subtype_float;
    if (mix)
        CoTaskMemFree(mix);
    s_ach = wf.Format.nChannels;
    if (FAILED(IAudioClient_Initialize(s_ac, AUDCLNT_SHAREMODE_SHARED,
                                       AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM |
                                       AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY,
                                       500000, 0, (WAVEFORMATEX *)&wf, NULL)) ||
        FAILED(IAudioClient_GetBufferSize(s_ac, &s_abuf)) ||
        FAILED(IAudioClient_GetService(s_ac, &IID_IAudioRenderClient, (void **)&s_rc))) {
        IAudioClient_Release(s_ac);
        s_ac = NULL;
        return 0;
    }
    s_aev = CreateEventA(NULL, FALSE, FALSE, NULL);
    IAudioClient_SetEventHandle(s_ac, s_aev);
    s_afn = fn;
    s_auser = user;
    s_astop = 0;
    IAudioClient_Start(s_ac);
    s_athread = CreateThread(NULL, 0, audio_main, NULL, 0, NULL);
    return 1;
}

void host_audio_close(void)
{
    if (!s_ac)
        return;
    InterlockedExchange(&s_astop, 1);
    SetEvent(s_aev);
    WaitForSingleObject(s_athread, INFINITE);
    CloseHandle(s_athread);
    IAudioClient_Stop(s_ac);
    IAudioRenderClient_Release(s_rc);
    IAudioClient_Release(s_ac);
    CloseHandle(s_aev);
    s_ac = NULL;
    s_rc = NULL;
}

/* ---- decoded music: Media Foundation ---------------------------------------------- */
struct host_stream {
    IMFSourceReader *rd;
    float           *buf;                    /* decoded, not yet handed out */
    int              n, at, cap;
    int              done;
};

host_stream *host_stream_open(const char *path, int rate)
{
    WCHAR wpath[MAX_PATH];
    IMFSourceReader *rd = NULL;
    IMFMediaType *mt = NULL;
    host_stream *s;
    if (!MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, MAX_PATH))
        return NULL;
    if (FAILED(MFCreateSourceReaderFromURL(wpath, NULL, &rd)))
        return NULL;
    /* what the mixer wants: float stereo at its rate (the reader converts) */
    MFCreateMediaType(&mt);
    IMFMediaType_SetGUID(mt, &MF_MT_MAJOR_TYPE, &MFMediaType_Audio);
    IMFMediaType_SetGUID(mt, &MF_MT_SUBTYPE, &MFAudioFormat_Float);
    IMFMediaType_SetUINT32(mt, &MF_MT_AUDIO_NUM_CHANNELS, 2);
    IMFMediaType_SetUINT32(mt, &MF_MT_AUDIO_SAMPLES_PER_SECOND, (UINT32)rate);
    IMFMediaType_SetUINT32(mt, &MF_MT_AUDIO_BITS_PER_SAMPLE, 32);
    IMFMediaType_SetUINT32(mt, &MF_MT_AUDIO_BLOCK_ALIGNMENT, 8);
    IMFMediaType_SetUINT32(mt, &MF_MT_AUDIO_AVG_BYTES_PER_SECOND, (UINT32)rate * 8);
    if (FAILED(IMFSourceReader_SetCurrentMediaType(rd, (DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, NULL, mt))) {
        IMFMediaType_Release(mt);
        IMFSourceReader_Release(rd);
        return NULL;
    }
    IMFMediaType_Release(mt);
    s = (host_stream *)calloc(1, sizeof *s);
    s->rd = rd;
    return s;
}

/* the next decoded sample into the stream's buffer: 0 at the end */
static int refill(host_stream *s)
{
    while (!s->done) {
        DWORD flags = 0;
        IMFSample *smp = NULL;
        IMFMediaBuffer *mb = NULL;
        BYTE *p;
        DWORD len = 0;
        if (FAILED(IMFSourceReader_ReadSample(s->rd, (DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, NULL,
                                              &flags, NULL, &smp)) ||
            (flags & MF_SOURCE_READERF_ENDOFSTREAM)) {
            if (smp)
                IMFSample_Release(smp);
            s->done = 1;
            break;
        }
        if (!smp)
            continue;
        if (SUCCEEDED(IMFSample_ConvertToContiguousBuffer(smp, &mb)) &&
            SUCCEEDED(IMFMediaBuffer_Lock(mb, &p, NULL, &len))) {
            int frames = (int)(len / 8);
            if (frames > s->cap) {
                s->cap = frames;
                s->buf = (float *)realloc(s->buf, (size_t)frames * 8);
            }
            memcpy(s->buf, p, (size_t)frames * 8);
            s->n = frames;
            s->at = 0;
            IMFMediaBuffer_Unlock(mb);
        }
        if (mb)
            IMFMediaBuffer_Release(mb);
        IMFSample_Release(smp);
        if (s->n > 0)
            return 1;
    }
    return 0;
}

int host_stream_read(host_stream *s, float *lr, int frames)
{
    int got = 0;
    while (got < frames) {
        int k;
        if (s->at >= s->n && !refill(s))
            break;
        k = s->n - s->at;
        if (k > frames - got)
            k = frames - got;
        memcpy(lr + 2 * got, s->buf + 2 * s->at, (size_t)k * 8);
        s->at += k;
        got += k;
    }
    return got;
}

void host_stream_close(host_stream *s)
{
    if (s) {
        IMFSourceReader_Release(s->rd);
        free(s->buf);
        free(s);
    }
}

/* ---- game controllers: XInput -------------------------------------------------- */
int host_pad_read(host_pad *o)
{
    XINPUT_STATE st;
    const XINPUT_GAMEPAD *g;
    unsigned b = 0;
    int u, r, d, l;
    DWORD i;
    memset(o, 0, sizeof *o);
    o->pov = -1;
    for (i = 0; i < XUSER_MAX_COUNT; i++)
        if (XInputGetState(i, &st) == ERROR_SUCCESS)
            break;
    if (i == XUSER_MAX_COUNT)
        return 0;
    g = &st.Gamepad;
    o->x = g->sThumbLX / 32767.0f;
    o->y = -g->sThumbLY / 32767.0f;
    if (o->x < -1) o->x = -1;
    if (o->y < -1) o->y = -1;
    o->z = (g->bRightTrigger - g->bLeftTrigger) / 255.0f;
    o->rx = g->sThumbRX / 32767.0f;
    o->ry = -g->sThumbRY / 32767.0f;
    if (o->rx < -1) o->rx = -1;
    if (o->ry < -1) o->ry = -1;
    if (g->wButtons & XINPUT_GAMEPAD_A) b |= 1u << 0;
    if (g->wButtons & XINPUT_GAMEPAD_B) b |= 1u << 1;
    if (g->wButtons & XINPUT_GAMEPAD_X) b |= 1u << 2;
    if (g->wButtons & XINPUT_GAMEPAD_Y) b |= 1u << 3;
    if (g->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) b |= 1u << 4;
    if (g->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) b |= 1u << 5;
    if (g->wButtons & XINPUT_GAMEPAD_BACK) b |= 1u << 6;
    if (g->wButtons & XINPUT_GAMEPAD_START) b |= 1u << 7;
    if (g->wButtons & XINPUT_GAMEPAD_LEFT_THUMB) b |= 1u << 8;
    if (g->wButtons & XINPUT_GAMEPAD_RIGHT_THUMB) b |= 1u << 9;
    if (g->bLeftTrigger > 127) b |= 1u << 10;
    if (g->bRightTrigger > 127) b |= 1u << 11;
    u = (g->wButtons & XINPUT_GAMEPAD_DPAD_UP) != 0;
    r = (g->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) != 0;
    d = (g->wButtons & XINPUT_GAMEPAD_DPAD_DOWN) != 0;
    l = (g->wButtons & XINPUT_GAMEPAD_DPAD_LEFT) != 0;
    b |= (unsigned)u << 12 | (unsigned)r << 13 | (unsigned)d << 14 | (unsigned)l << 15;
    o->buttons = b;
    if (u || r || d || l) {
        static const int ang[3][3] = { { 22500, 27000, 31500 }, { 18000, -1, 0 }, { 13500, 9000, 4500 } };
        o->pov = ang[r - l + 1][u - d + 1];
    }
    return 1;
}
