/* sdl_pads.c: game controllers through SDL 3's gamepad API (sdl_pads.h).
 *
 * Every controller SDL knows: Xbox (XInput, GameInput), PlayStation
 * (DualShock 4, DualSense), Switch Pro and Joy-Cons, the Steam Deck's own and
 * the Steam Controller (through Steam Input, or SDL's own HIDAPI drivers
 * without Steam), and the thousands its controller database maps. Each in
 * the XInput layout host.h describes: SDL names the face buttons by place
 * (south, east, west, north), which are A, B, X and Y on an Xbox pad, so a
 * PlayStation pad's cross is A and a Nintendo pad's B is A, as the game's
 * prompts expect from where the button sits.
 *
 * The controllers are kept in the order they were connected: the first is
 * the one still there that came first, as XInput's lowest slot is. A
 * controller connected while the game runs is picked up at the next read
 * (the list is looked at again at most every 250 ms), one that goes is
 * dropped from the list at once. Reads may come from any thread (Top Gear
 * Rally's SI thread, Boss Rally's game thread): the list is kept under a
 * lock, and SDL's own reads under its. */
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sdl_pads.h"

#define PADS 8
static SDL_Gamepad *s_pads[PADS];
static int s_npads, s_up;
static SDL_Mutex *s_m;
static Uint64 s_looked;                /* when the list was last looked at (ms) */

int sdl_pads_init(int own_thread)
{
    if (s_up)
        return 1;
    /* read with the window in the background too, as XInput does */
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    if (own_thread)
        SDL_SetHint(SDL_HINT_JOYSTICK_THREAD, "1");
    if (!SDL_InitSubSystem(SDL_INIT_GAMEPAD)) {
        fprintf(stderr, "host: gamepads: %s\n", SDL_GetError());
        return 0;
    }
    s_m = SDL_CreateMutex();
    s_up = 1;
    return 1;
}

void sdl_pads_quit(void)
{
    int i;
    if (!s_up)
        return;
    SDL_LockMutex(s_m);
    for (i = 0; i < s_npads; i++)
        SDL_CloseGamepad(s_pads[i]);
    s_npads = 0;
    SDL_UnlockMutex(s_m);
    SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
    s_up = 0;
}

/* under s_m: the list brought up to date with what SDL sees, the order kept */
static void look(void)
{
    SDL_JoystickID *ids;
    Uint64 now = SDL_GetTicks();
    int n = 0, i, k;
    SDL_UpdateGamepads();
    /* one gone is dropped now; one come is looked for every 250 ms */
    for (i = 0; i < s_npads;)
        if (!SDL_GamepadConnected(s_pads[i])) {
            SDL_CloseGamepad(s_pads[i]);
            memmove(&s_pads[i], &s_pads[i + 1], (size_t)(s_npads - i - 1) * sizeof s_pads[0]);
            s_npads--;
        } else {
            i++;
        }
    if (s_looked && now - s_looked < 250)
        return;
    s_looked = now;
    ids = SDL_GetGamepads(&n);
    for (k = 0; ids && k < n && s_npads < PADS; k++) {
        SDL_Gamepad *g;
        for (i = 0; i < s_npads; i++)
            if (SDL_GetGamepadID(s_pads[i]) == ids[k])
                break;
        if (i < s_npads || !(g = SDL_OpenGamepad(ids[k])))
            continue;
        s_pads[s_npads++] = g;
        if (getenv("BR_PAD_LOG"))
            fprintf(stderr, "host: pad %d: %s (%04x:%04x)%s\n", s_npads - 1, SDL_GetGamepadName(g),
                    SDL_GetGamepadVendor(g), SDL_GetGamepadProduct(g),
                    SDL_GetBooleanProperty(SDL_GetGamepadProperties(g), SDL_PROP_GAMEPAD_CAP_RUMBLE_BOOLEAN, false)
                        ? ", rumble" : "");
    }
    SDL_free(ids);
    /* (nothing drains SDL's event queue on Windows: what the reads put there goes) */
    SDL_FlushEvents(SDL_EVENT_JOYSTICK_AXIS_MOTION, SDL_EVENT_GAMEPAD_STEAM_HANDLE_UPDATED);
}

static float axis(SDL_Gamepad *g, SDL_GamepadAxis a)
{
    float v = SDL_GetGamepadAxis(g, a) / 32767.0f;
    return v < -1 ? -1 : v;
}

int sdl_pads_read(int n, host_pad *o)
{
    SDL_Gamepad *g;
    unsigned b = 0;
    int u, r, d, l;
    float lt, rt;
    memset(o, 0, sizeof *o);
    o->pov = -1;
    if (!s_up)
        return 0;
    SDL_LockMutex(s_m);
    look();
    if (n < 0 || n >= s_npads) {
        SDL_UnlockMutex(s_m);
        return 0;
    }
    g = s_pads[n];
    o->x = axis(g, SDL_GAMEPAD_AXIS_LEFTX);
    o->y = axis(g, SDL_GAMEPAD_AXIS_LEFTY);          /* SDL's: positive downward, as host_pad's */
    o->rx = axis(g, SDL_GAMEPAD_AXIS_RIGHTX);
    o->ry = axis(g, SDL_GAMEPAD_AXIS_RIGHTY);
    lt = axis(g, SDL_GAMEPAD_AXIS_LEFT_TRIGGER);
    rt = axis(g, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);
    o->z = rt - lt;
    if (SDL_GetGamepadButton(g, SDL_GAMEPAD_BUTTON_SOUTH)) b |= 1u << 0;
    if (SDL_GetGamepadButton(g, SDL_GAMEPAD_BUTTON_EAST)) b |= 1u << 1;
    if (SDL_GetGamepadButton(g, SDL_GAMEPAD_BUTTON_WEST)) b |= 1u << 2;
    if (SDL_GetGamepadButton(g, SDL_GAMEPAD_BUTTON_NORTH)) b |= 1u << 3;
    if (SDL_GetGamepadButton(g, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER)) b |= 1u << 4;
    if (SDL_GetGamepadButton(g, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER)) b |= 1u << 5;
    if (SDL_GetGamepadButton(g, SDL_GAMEPAD_BUTTON_BACK)) b |= 1u << 6;
    if (SDL_GetGamepadButton(g, SDL_GAMEPAD_BUTTON_START)) b |= 1u << 7;
    if (SDL_GetGamepadButton(g, SDL_GAMEPAD_BUTTON_LEFT_STICK)) b |= 1u << 8;
    if (SDL_GetGamepadButton(g, SDL_GAMEPAD_BUTTON_RIGHT_STICK)) b |= 1u << 9;
    if (lt > 0.5f) b |= 1u << 10;
    if (rt > 0.5f) b |= 1u << 11;
    u = SDL_GetGamepadButton(g, SDL_GAMEPAD_BUTTON_DPAD_UP);
    r = SDL_GetGamepadButton(g, SDL_GAMEPAD_BUTTON_DPAD_RIGHT);
    d = SDL_GetGamepadButton(g, SDL_GAMEPAD_BUTTON_DPAD_DOWN);
    l = SDL_GetGamepadButton(g, SDL_GAMEPAD_BUTTON_DPAD_LEFT);
    SDL_UnlockMutex(s_m);
    b |= (unsigned)u << 12 | (unsigned)r << 13 | (unsigned)d << 14 | (unsigned)l << 15;
    o->buttons = b;
    if (u || r || d || l) {
        static const int ang[3][3] = { { 22500, 27000, 31500 }, { 18000, -1, 0 }, { 13500, 9000, 4500 } };
        o->pov = ang[r - l + 1][u - d + 1];
    }
    return 1;
}

int sdl_pads_can_rumble(int n)
{
    int r;
    if (!s_up)
        return 0;
    SDL_LockMutex(s_m);
    r = n >= 0 && n < s_npads &&
        SDL_GetBooleanProperty(SDL_GetGamepadProperties(s_pads[n]), SDL_PROP_GAMEPAD_CAP_RUMBLE_BOOLEAN, false);
    SDL_UnlockMutex(s_m);
    return r;
}

/* the heavy (low-frequency) motor at full, held until changed, as XInput's
 * (SDL's longest, 65 seconds, past any the game asks for) */
int sdl_pads_rumble(int n, int on)
{
    int r = 0;
    if (!s_up)
        return 0;
    SDL_LockMutex(s_m);
    if (n >= 0 && n < s_npads)
        r = SDL_RumbleGamepad(s_pads[n], on ? 0xFFFF : 0, 0, on ? 0xFFFF : 0);
    SDL_UnlockMutex(s_m);
    return r;
}
