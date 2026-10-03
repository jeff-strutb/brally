/* dx.c: the DirectX objects the game asks for.
 *
 *   DirectDraw   only what the start-up version probe exercises: the game
 *                draws through Glide. Enough for it to find DirectX 6.
 *   DirectInput  the keyboard and the mouse, from host events, and one
 *                joystick answered from the host's first game controller
 *                (host_pad_read), always reported, reading centred when
 *                none is connected, as the wasm lane's does.
 *   DirectSound  dsound.c.
 *   DirectPlay   not available: no network play.
 *
 * Each object is a COM object with the SDK's vtable order. Slots the game
 * never calls answer E_FAIL. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "plat.h"
#include "host.h"

#define NSLOT 32
typedef HRESULT (*slot_fn)(void);

typedef struct pobj {
    const void *vtbl;          /* first: the object is what the game holds */
    ULONG       refs;
    int         kind;
    int         acquired;
    int32_t     rmin[3], rmax[3]; /* a joystick's DIPROP_RANGE for X, Y, Z */
} pobj;

static HRESULT unimpl(void) { return E_FAIL; }

static HRESULT QI_self(pobj *o, REFIID iid, void **out)
{
    (void)iid;
    o->refs++;
    *out = o;
    return S_OK;
}
static ULONG AddRef_(pobj *o) { return ++o->refs; }
static ULONG Release_(pobj *o)
{
    if (o->refs > 0 && --o->refs == 0) {
        /* left allocated: the game may still hold a copy of the pointer */
        return 0;
    }
    return o->refs;
}

static void fill(void **v, int n)
{
    int i;
    for (i = 0; i < n; i++)
        v[i] = (void *)unimpl;
    v[0] = (void *)QI_self;
    v[1] = (void *)AddRef_;
    v[2] = (void *)Release_;
}

static pobj *pnew(void **vtbl, int kind)
{
    pobj *o = (pobj *)calloc(1, sizeof *o);
    o->vtbl = vtbl;
    o->refs = 1;
    o->kind = kind;
    return o;
}

/* ---- DirectDraw: the version probe --------------------------------------------- */
static void *s_dd_vt[NSLOT], *s_dds_vt[NSLOT];

static HRESULT dd_SetCooperativeLevel(pobj *o, HWND h, DWORD f) { (void)o; (void)h; (void)f; return S_OK; }
static HRESULT dd_CreateSurface(pobj *o, void *desc, void **out, void *outer)
{
    (void)o;
    (void)desc;
    (void)outer;
    *out = pnew(s_dds_vt, 2);
    return S_OK;
}

static HRESULT WINAPI plat_DirectDrawCreate(GUID *g, void **out, void *outer)
{
    (void)g;
    (void)outer;
    if (!s_dd_vt[0]) {
        fill(s_dd_vt, NSLOT);
        s_dd_vt[6] = (void *)dd_CreateSurface;
        s_dd_vt[20] = (void *)dd_SetCooperativeLevel;
        fill(s_dds_vt, NSLOT);
    }
    *out = pnew(s_dd_vt, 1);
    return S_OK;
}

/* ---- DirectInput ------------------------------------------------------------------------- */
extern uint8_t g_plat_dik[256];       /* win_user.c: scan codes held down */

enum { DEV_KEYBOARD = 10, DEV_MOUSE, DEV_JOYSTICK };

/* the joystick's instance GUID, {E3C58B00-9A2F-4D1B-8C5E-4252414C4C59} */
static const uint8_t k_joy_guid[16] = { 0x00, 0x8B, 0xC5, 0xE3, 0x2F, 0x9A, 0x1B, 0x4D,
                                        0x8C, 0x5E, 0x42, 0x52, 0x41, 0x4C, 0x4C, 0x59 };
static void *s_di_vt[NSLOT], *s_did_vt[NSLOT];

/* GUID_SysKeyboard {6F1D2B61-D5A0-11CF-BFC7-444553540000},
 * GUID_SysMouse    {6F1D2B60-D5A0-11CF-BFC7-444553540000} */
static int is_mouse_guid(const GUID *g) { return g && g->Data1 == 0x6F1D2B60u; }

static HRESULT di_CreateDevice(pobj *o, const GUID *g, void **out, void *outer)
{
    pobj *d;
    int i;
    (void)o;
    (void)outer;
    if (g && !memcmp(g, k_joy_guid, 16)) {
        d = pnew(s_did_vt, DEV_JOYSTICK);
        for (i = 0; i < 3; i++)
            d->rmax[i] = 65535;               /* DirectInput's default range */
        *out = d;
        PLOG("DirectInput: the joystick opened\n");
        return S_OK;
    }
    *out = pnew(s_did_vt, is_mouse_guid(g) ? DEV_MOUSE : DEV_KEYBOARD);
    return S_OK;
}

typedef BOOL (WINAPI *di_enum_fn)(const void *inst, void *ref);

/* the joystick, for DIDEVTYPE_JOYSTICK (4) or every device (0), unless only
 * force-feedback devices are wanted (DIEDFL_FORCEFEEDBACK) */
static HRESULT di_EnumDevices(pobj *o, DWORD type, void *cb, void *ref, DWORD flags)
{
    uint8_t inst[0x244];                      /* DIDEVICEINSTANCEA */
    uint32_t v;
    (void)o;
    if ((type != 0 && type != 4) || (flags & 0x100) || !cb)
        return S_OK;
    memset(inst, 0, sizeof inst);
    v = sizeof inst;
    memcpy(inst, &v, 4);                      /* dwSize */
    memcpy(inst + 4, k_joy_guid, 16);         /* guidInstance */
    memcpy(inst + 0x14, k_joy_guid, 16);      /* guidProduct */
    v = 0x0104;                               /* DIDEVTYPE_JOYSTICK, gamepad */
    memcpy(inst + 0x24, &v, 4);
    strcpy((char *)inst + 0x28, "Game Controller");
    strcpy((char *)inst + 0x12C, "Game Controller");
    ((di_enum_fn)cb)(inst, ref);
    return S_OK;
}

static HRESULT di_ok(void) { return S_OK; }

/* the joystick's DIPROP_RANGE (4), per axis by offset (DIPH_BYOFFSET) */
static HRESULT did_SetProperty(pobj *o, uintptr_t prop, const void *ph)
{
    uint32_t how, off;
    int32_t lo, hi;
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
    return S_OK;
}

static HRESULT did_Acquire(pobj *o)
{
    int was = o->acquired;
    o->acquired = 1;
    return was ? S_FALSE : S_OK;
}

static HRESULT did_Unacquire(pobj *o)
{
    int was = o->acquired;
    o->acquired = 0;
    return was ? S_OK : S_FALSE;
}

static HRESULT did_Poll(pobj *o) { return o->kind == DEV_JOYSTICK ? S_OK : S_FALSE; }

static HRESULT did_GetCapabilities(pobj *o, void *caps)
{
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
    return S_OK;
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
 * wasm lane does the same (host_dx.c abs_step). */
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

static HRESULT did_GetDeviceState(pobj *o, DWORD n, void *out)
{
    plat_pump(0);
    if (o->kind == DEV_JOYSTICK) {
        /* DIJOYSTATE / DIJOYSTATE2: lX lY lZ, ..., rgdwPOV at 0x20,
         * rgbButtons at 0x30 */
        host_pad g;
        float ax[3];
        uint8_t *p = (uint8_t *)out;
        uint32_t i;
        if (!o->acquired)
            return (HRESULT)0x8007000Cu;     /* DIERR_NOTACQUIRED */
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
        return S_OK;
    }
    if (o->kind == DEV_KEYBOARD) {
        memcpy(out, g_plat_dik, n < 256 ? n : 256);
    } else {
        /* DIMOUSESTATE: lX, lY, lZ, rgbButtons[4] -- the movement since the
         * last poll, and a press that came and went in between still counts */
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
    return S_OK;
}

static HRESULT did_GetDeviceData(pobj *o, DWORD cb, void *data, DWORD *n, DWORD flags)
{
    (void)o; (void)cb; (void)data; (void)flags;
    if (n)
        *n = 0;
    return S_OK;
}

static HRESULT WINAPI plat_DirectInputCreateA_(HINSTANCE h, DWORD v, LPVOID *out, LPUNKNOWN outer)
{
    (void)h;
    (void)v;
    (void)outer;
    if (!s_di_vt[0]) {
        fill(s_di_vt, NSLOT);
        s_di_vt[3] = (void *)di_CreateDevice;
        s_di_vt[4] = (void *)di_EnumDevices;
        s_di_vt[7] = (void *)di_ok;              /* Initialize */
        fill(s_did_vt, NSLOT);
        s_did_vt[3] = (void *)did_GetCapabilities;
        s_did_vt[6] = (void *)did_SetProperty;
        s_did_vt[7] = (void *)did_Acquire;
        s_did_vt[8] = (void *)did_Unacquire;
        s_did_vt[9] = (void *)did_GetDeviceState;
        s_did_vt[10] = (void *)did_GetDeviceData;
        s_did_vt[11] = (void *)di_ok;            /* SetDataFormat */
        s_did_vt[12] = (void *)di_ok;            /* SetEventNotification */
        s_did_vt[13] = (void *)di_ok;            /* SetCooperativeLevel */
        s_did_vt[25] = (void *)did_Poll;
    }
    *out = pnew(s_di_vt, 3);
    return S_OK;
}

HRESULT WINAPI DirectInputCreateA(HINSTANCE h, DWORD v, LPVOID *out, LPUNKNOWN outer)
{
    return plat_DirectInputCreateA_(h, v, out, outer);
}

/* ---- DirectSound, DirectPlay ------------------------------------------------------------- */
const GUID CLSID_DirectSound = { 0x47D4D946, 0x62E8, 0x11CF, { 0x93, 0xBC, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00 } };
const GUID IID_IDirectSound  = { 0x279AFA83, 0x4981, 0x11CE, { 0xA5, 0x21, 0x00, 0x20, 0xAF, 0x0B, 0xE5, 0x60 } };

/* plat_dsound_create: dsound.c */

/* ---- the DLLs the game loads by name ------------------------------------------------------ */
static const plat_export k_ddraw[] = { { "DirectDrawCreate", (void *)plat_DirectDrawCreate } };
static const plat_export k_dinput[] = { { "DirectInputCreateA", (void *)plat_DirectInputCreateA_ } };

void plat_dx_init(void)
{
    plat_register_module("ddraw.dll", k_ddraw, 1);
    plat_register_module("dinput.dll", k_dinput, 1);
}
