/* thunks.c: one game's way to the host.  Compiled once per game (ENGINE=tgr
 * or br) and linked into that game's object before its symbols are hidden
 * (link.sh): every call the game makes to the host for the window, input,
 * sound or its folders lands here and goes on to mux_<ENGINE>_<name>
 * (mux.c), so the multiplexer knows which game is asking whatever thread it
 * asks from.  Time, threads, files and the network are each game's own
 * copy of host/posix or host/win32 and do not pass through here. */
#include "host.h"

#define CAT3_(a, b, c) a##_##b##_##c
#define CAT3(a, b, c) CAT3_(a, b, c)
#define M(f) CAT3(mux, ENGINE, f)

#define FWD(ret, name, params, args) \
    ret M(name) params;              \
    ret name params { return M(name) args; }
#define FWDV(name, params, args)     \
    void M(name) params;             \
    void name params { M(name) args; }

FWDV(host_set_app_name, (const char *dir, const char *title), (dir, title))
FWDV(host_init, (int argc, char **argv), (argc, argv))
FWDV(host_shutdown, (void), ())
FWD(const char *, host_game_dir, (void), ())
FWD(const char *, host_cd_dir, (void), ())
FWD(const char *, host_save_dir, (void), ())
FWD(const char *, host_resource_dir, (void), ())
FWD(const char *, host_music_dir, (void), ())

FWD(int, host_window_open, (int width, int height, const char *title), (width, height, title))
FWDV(host_window_close, (void), ())
FWD(int, host_window_visible, (void), ())
FWDV(host_window_lock_aspect, (int lock), (lock))
FWD(int, host_poll_event, (host_event *ev, uint32_t wait_ms), (ev, wait_ms))
FWDV(host_message_box, (const char *text, const char *caption), (text, caption))
FWD(void *, host_window_handle, (void), ())
FWDV(host_window_pixels, (int *w, int *h), (w, h))
FWDV(host_present, (const uint32_t *argb, int w, int h), (argb, w, h))

FWD(int, host_audio_open, (int rate, host_audio_fn fn, void *user), (rate, fn, user))
FWDV(host_audio_close, (void), ())
FWD(host_stream *, host_stream_open, (const char *path, int rate), (path, rate))
FWD(int, host_stream_read, (host_stream *s, float *lr, int frames), (s, lr, frames))
FWDV(host_stream_close, (host_stream *s), (s))
FWD(int, host_pad_read, (host_pad *p), (p))
FWD(int, host_pad_read_n, (int i, host_pad *p), (i, p))
FWD(int, host_pad_can_rumble, (int i), (i))
FWD(int, host_pad_rumble, (int i, int on), (i, on))

#if defined(__APPLE__) && defined(MUX_METAL_LAYER)
/* the window's CAMetalLayer (host/macos, the iPhone app), for the Metal renderers */
FWD(void *, host_macos_metal_layer, (void), ())
FWDV(host_macos_layer_fit, (int *w, int *h), (w, h))
#endif
#ifdef _WIN32
/* the module and window (host/windows), for the Vulkan renderers */
FWD(void *, host_win32_instance, (void), ())
FWD(void *, host_win32_window, (void), ())
#endif
