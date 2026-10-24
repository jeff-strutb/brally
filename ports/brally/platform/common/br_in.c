/* br_in.c: the game's input devices (br_in.h) -- the keyboard and the mouse
 * from host events, and one joystick answered from the host's first game
 * controller (host_pad_read), always there, reading centred when none is
 * connected. No host device makes force feedback, so none is offered and
 * effects cannot be made. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "plat.h"
#include "host.h"
#include "br_in.h"

typedef struct pobj {
    int         kind;
    int         acquired;
    int32_t     rmin[3], rmax[3]; /* a joystick's axis ranges for X, Y, Z */
} pobj;

static pobj *pnew(int kind)
{
    pobj *o = (pobj *)calloc(1, sizeof *o);
    o->kind = kind;
    return o;
}

/* the input system the game opens before its devices */
void *br_in_open(void)
{
    static int s_in;
    return &s_in;
}

/* a device or effect let go of: left allocated, as the game may still hold
 * a copy of the pointer */
void br_in_release(void *obj) { (void)obj; }

/* force feedback: no host device offers it */
int32_t br_in_effect_new(void *dev, const void *guid, const void *params, void **eff)
{
    (void)dev; (void)guid; (void)params;
    if (eff)
        *eff = NULL;
    return BR_IN_FAIL;
}
int32_t br_in_effect_params(void *eff, const void *params, uint32_t flags) { (void)eff; (void)params; (void)flags; return BR_IN_FAIL; }
int32_t br_in_effect_start(void *eff, uint32_t n, uint32_t flags) { (void)eff; (void)n; (void)flags; return BR_IN_FAIL; }
int32_t br_in_effect_stop(void *eff) { (void)eff; return BR_IN_FAIL; }

extern uint8_t g_plat_dik[256];       /* br_os_window.c: scan codes held down */

enum { DEV_KEYBOARD = 10, DEV_MOUSE, DEV_JOYSTICK };

/* the joystick's instance GUID, {E3C58B00-9A2F-4D1B-8C5E-4252414C4C59} */
static const uint8_t k_joy_guid[16] = { 0x00, 0x8B, 0xC5, 0xE3, 0x2F, 0x9A, 0x1B, 0x4D,
                                        0x8C, 0x5E, 0x42, 0x52, 0x41, 0x4C, 0x4C, 0x59 };

/* GUID_SysKeyboard {6F1D2B61-D5A0-11CF-BFC7-444553540000},
 * GUID_SysMouse    {6F1D2B60-D5A0-11CF-BFC7-444553540000} */
static int is_mouse_guid(const void *g)
{
    uint32_t d1;
    if (!g)
        return 0;
    memcpy(&d1, g, 4);
    return d1 == 0x6F1D2B60u;
}

/* the device a GUID names: the joystick's instance (br_in_pads), the
 * system mouse, or else the keyboard */
int32_t br_in_create(const void *g, void **out)
{
    pobj *d;
    int i;
    if (g && !memcmp(g, k_joy_guid, 16)) {
        d = pnew(DEV_JOYSTICK);
        for (i = 0; i < 3; i++)
            d->rmax[i] = 65535;               /* DirectInput's default range */
        *out = d;
        PLOG("input: the joystick opened\n");
        return BR_IN_OK;
    }
    *out = pnew(is_mouse_guid(g) ? DEV_MOUSE : DEV_KEYBOARD);
    return BR_IN_OK;
}

/* the joystick, for type 4 (joysticks) or 0 (every device), unless only
 * force-feedback devices are wanted (flags 0x100): the host has none. The
 * description handed to cb is the 0x244-byte block the game reads its GUID
 * (+4) and name (+0x28) from. */
int32_t br_in_pads(uint32_t type, br_in_pad_fn cb, void *ref, uint32_t flags)
{
    uint8_t inst[0x244];
    uint32_t v;
    if ((type != 0 && type != 4) || (flags & 0x100) || !cb)
        return BR_IN_OK;
    memset(inst, 0, sizeof inst);
    v = sizeof inst;
    memcpy(inst, &v, 4);                      /* dwSize */
    memcpy(inst + 4, k_joy_guid, 16);         /* guidInstance */
    memcpy(inst + 0x14, k_joy_guid, 16);      /* guidProduct */
    v = 0x0104;                               /* a joystick: a gamepad */
    memcpy(inst + 0x24, &v, 4);
    strcpy((char *)inst + 0x28, "Game Controller");
    strcpy((char *)inst + 0x12C, "Game Controller");
    cb(inst, ref);
    return BR_IN_OK;
}

/* a device property: the joystick's axis range (4), per axis by its offset
 * in the state block (how 1); the others (dead zone, self-centring) are
 * taken and change nothing */
int32_t br_in_set_property(void *dev, uintptr_t prop, const void *ph)
{
    uint32_t how, off;
    int32_t lo, hi;
    pobj *o = (pobj *)dev;
    if (o->kind == DEV_JOYSTICK && (uint32_t)prop == 4 && ph) {   /* the core passes a 32-bit id */
        memcpy(&off, (const uint8_t *)ph + 8, 4);
        memcpy(&how, (const uint8_t *)ph + 12, 4);
        memcpy(&lo, (const uint8_t *)ph + 16, 4);
        memcpy(&hi, (const uint8_t *)ph + 20, 4);
        if (how == 1 && off <= 8 && !(off & 3)) {
            o->rmin[off / 4] = lo;
            o->rmax[off / 4] = hi;
        }
    }
    return BR_IN_OK;
}

/* the game's hold on a device: BR_IN_FALSE when it already had it */
int32_t br_in_acquire(void *dev)
{
    pobj *o = (pobj *)dev;
    int was = o->acquired;
    o->acquired = 1;
    return was ? BR_IN_FALSE : BR_IN_OK;
}

int32_t br_in_unacquire(void *dev)
{
    pobj *o = (pobj *)dev;
    int was = o->acquired;
    o->acquired = 0;
    return was ? BR_IN_OK : BR_IN_FALSE;
}

int32_t br_in_poll(void *dev) { return ((pobj *)dev)->kind == DEV_JOYSTICK ? BR_IN_OK : BR_IN_FALSE; }

/* what the device is: the block's size first, then attached (1), its type,
 * axes, buttons and POVs */
int32_t br_in_caps(void *dev, void *caps)
{
    pobj *o = (pobj *)dev;
    uint32_t sz, v[5] = { 1, 0, 0, 0, 0 };    /* DIDC_ATTACHED */
    memcpy(&sz, caps, 4);
    if (sz > 4)
        memset((uint8_t *)caps + 4, 0, sz - 4);
    if (o->kind == DEV_JOYSTICK) {
        v[1] = 0x0104;                        /* joystick, gamepad */
        v[2] = 3;                             /* axes */
        v[3] = 16;                            /* buttons */
        v[4] = 1;                             /* POVs */
    }
    memcpy((uint8_t *)caps + 4, v, sz >= 0x18 ? 20 : sz > 8 ? 4 : 0);
    return BR_IN_OK;
}

/* the controller now: BR_PADFAKE=x,y,z,buttons holds a fixed state instead
 * (checks without a controller attached) */
static void pad_now(host_pad *g)
{
    const char *fake = getenv("BR_PADFAKE");
    if (fake) {
        memset(g, 0, sizeof *g);
        g->pov = -1;
        sscanf(fake, "%f,%f,%f,%x", &g->x, &g->y, &g->z, &g->buttons);
        return;
    }
    host_pad_read(g);
}

static int s_mdx, s_mdy, s_mbtn, s_mlatch;
static int s_ax, s_ay, s_alive;               /* the window pointer the cursor follows */

/* The game keeps its own cursor and moves it only by the deltas it polls,
 * so the window pointer's own movement, fed as deltas, drifts from it (the
 * game clamps at its edges; the pointer leaves the window and comes back).
 * Instead, for a short while after the pointer moved, each poll hands the
 * game exactly the step from its cursor to the pointer; then it stops, so a
 * still mouse reads as still (the race reads the mouse as an axis). The
 * wasm lane did the same (host_dx.c abs_step). */
void plat_mouse_abs(int x, int y)
{
    s_ax = x;
    s_ay = y;
    s_alive = 30;
}

void plat_mouse_move(int dx, int dy)
{
    s_mdx += dx;
    s_mdy += dy;
}

void plat_mouse_button(int down)
{
    s_mbtn = down;
    s_mlatch |= down;
}

/* the device now, in the block the game reads: the keyboard's 256 scan
 * codes, the mouse's movement and buttons (x y z, 4 buttons), the
 * joystick's axes, POVs at 0x20 and buttons at 0x30 */
int32_t br_in_state(void *dev, uint32_t n, void *out)
{
    pobj *o = (pobj *)dev;
    plat_pump(0);
    if (o->kind == DEV_JOYSTICK) {
        host_pad g;
        float ax[3];
        uint8_t *p = (uint8_t *)out;
        uint32_t i;
        if (!o->acquired)
            return BR_IN_NOTACQUIRED;
        pad_now(&g);
        ax[0] = g.x;
        ax[1] = g.y;
        ax[2] = g.z;
        memset(out, 0, n);
        for (i = 0; i < 3 && i * 4 + 4 <= n; i++) {
            float v = ax[i] < -1 ? -1 : ax[i] > 1 ? 1 : ax[i];
            int32_t w = (int32_t)lroundf((float)o->rmin[i] + (v + 1) * 0.5f * (float)(o->rmax[i] - o->rmin[i]));
            memcpy(p + i * 4, &w, 4);
        }
        for (i = 0; i < 4 && 0x20 + i * 4 + 4 <= n; i++) {
            uint32_t pov = i == 0 && g.pov >= 0 ? (uint32_t)g.pov : 0xFFFFFFFFu;
            memcpy(p + 0x20 + i * 4, &pov, 4);
        }
        for (i = 0; i < 16 && 0x30 + i < n; i++)
            p[0x30 + i] = (g.buttons >> i) & 1 ? 0x80 : 0;
        return BR_IN_OK;
    }
    if (o->kind == DEV_KEYBOARD) {
        memcpy(out, g_plat_dik, n < 256 ? n : 256);
    } else {
        /* lX, lY, lZ, 4 buttons: the movement since the last poll, and a
         * press that came and went in between still counts */
        int32_t xyz[3];
        if (s_alive > 0) {
            int cx, cy;
            s_alive--;
            if (plat_game_cursor(&cx, &cy)) {
                s_mdx += s_ax - cx;
                s_mdy += s_ay - cy;
            }
        }
        xyz[0] = s_mdx;
        xyz[1] = s_mdy;
        xyz[2] = 0;
        uint8_t b[4] = { 0, 0, 0, 0 };
        memset(out, 0, n);
        b[0] = (s_mbtn | s_mlatch) ? 0x80 : 0;
        memcpy(out, xyz, n < 12 ? n : 12);
        if (n >= 16)
            memcpy((uint8_t *)out + 12, b, 4);
        s_mdx = s_mdy = 0;
        s_mlatch = 0;
    }
    return BR_IN_OK;
}

