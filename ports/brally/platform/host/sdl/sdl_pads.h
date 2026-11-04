/* sdl_pads.h: game controllers through SDL 3's gamepad API (sdl_pads.c), for
 * the hosts that read them so: host/linux (SDL is its whole host) and
 * host/windows (SDL for the controllers alone, the rest Win32). */
#ifndef SDL_PADS_H
#define SDL_PADS_H

#include "host.h"

/* SDL's gamepads up (SDL_INIT_GAMEPAD); own_thread: SDL reads them on a
 * thread of its own (Windows: no SDL event loop runs there) */
int  sdl_pads_init(int own_thread);
void sdl_pads_quit(void);
/* the n-th controller connected that is still there (0: the first), in the
 * XInput layout host.h describes; 0 when there is none (*p reads centred) */
int  sdl_pads_read(int n, host_pad *p);
/* the n-th's rumble: whether it has a motor, and its heavy one on or off */
int  sdl_pads_can_rumble(int n);
int  sdl_pads_rumble(int n, int on);

#endif
