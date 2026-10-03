/* dx.c: the DirectX objects the game asks for.
 *
 *   DirectDraw   only what the start-up version probe exercises: the game
 *                draws through Glide. Enough for it to find DirectX 6.
 *   DirectInput  the keyboard and the mouse, from host events. No joysticks
 *                yet (EnumDevices finds none).
 *   DirectSound  dsound.c.
 *   DirectPlay   not available: no network play.
 *
 * Each object is a COM object with the SDK's vtable order. Slots the game
 * never calls answer E_FAIL. */
#include <stdlib.h>
#include <string.h>

#include "plat.h"

#define NSLOT 32
typedef HRESULT (*slot_fn)(void);

typedef struct pobj {
    const void *vtbl;          /* first: the object is what the game holds */
    ULONG       refs;
    int         kind;
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

enum { DEV_KEYBOARD = 10, DEV_MOUSE };
static void *s_di_vt[NSLOT], *s_did_vt[NSLOT];

/* GUID_SysKeyboard {6F1D2B61-D5A0-11CF-BFC7-444553540000},
 * GUID_SysMouse    {6F1D2B60-D5A0-11CF-BFC7-444553540000} */
static int is_mouse_guid(const GUID *g) { return g && g->Data1 == 0x6F1D2B60u; }

static HRESULT di_CreateDevice(pobj *o, const GUID *g, void **out, void *outer)
{
    (void)o;
    (void)outer;
    *out = pnew(s_did_vt, is_mouse_guid(g) ? DEV_MOUSE : DEV_KEYBOARD);
    return S_OK;
}

static HRESULT di_EnumDevices(pobj *o, DWORD type, void *cb, void *ref, DWORD flags)
{
    (void)o; (void)type; (void)cb; (void)ref; (void)flags;
    return S_OK;               /* no game controllers attached */
}

static HRESULT di_ok(void) { return S_OK; }

static int s_mdx, s_mdy, s_mbtn, s_mlatch;

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
    if (o->kind == DEV_KEYBOARD) {
        memcpy(out, g_plat_dik, n < 256 ? n : 256);
    } else {
        /* DIMOUSESTATE: lX, lY, lZ, rgbButtons[4] -- the movement since the
         * last poll, and a press that came and went in between still counts */
        int32_t xyz[3] = { s_mdx, s_mdy, 0 };
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
        s_did_vt[6] = (void *)di_ok;             /* SetProperty */
        s_did_vt[7] = (void *)di_ok;             /* Acquire */
        s_did_vt[8] = (void *)di_ok;             /* Unacquire */
        s_did_vt[9] = (void *)did_GetDeviceState;
        s_did_vt[10] = (void *)did_GetDeviceData;
        s_did_vt[11] = (void *)di_ok;            /* SetDataFormat */
        s_did_vt[12] = (void *)di_ok;            /* SetEventNotification */
        s_did_vt[13] = (void *)di_ok;            /* SetCooperativeLevel */
        s_did_vt[25] = (void *)di_ok;            /* Poll */
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
