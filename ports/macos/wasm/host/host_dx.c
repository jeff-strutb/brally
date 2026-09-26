/* host_dx.c -- the DirectX COM objects the game uses (port code).
 *
 * A COM object lives in game memory: [vtbl][ref][state]. Its vtable holds
 * host-function addresses (w_addr_of_host), so the game's own
 * `p->lpVtbl->Method(p, ...)` indirect calls land here. Method sets and
 * answers follow tools/brbox_com.py, the oracle the T3 build was certified
 * under: a DirectX 6 machine, keyboard + mouse, no joystick, DirectPlay with
 * no lobby. DirectSound buffers keep time but make no sound (music/audio is
 * an open decision, ports/README.md).
 */
#include "host.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define S_OK 0u
#define E_NOINTERFACE 0x80004002u
#define E_NOTIMPL 0x80004001u

extern _Thread_local const w_fentry *w_last;

/* ---- generic unmodelled-method stubs, one per argument count ---- */
static u32 unm(void) { fprintf(stderr, "*** unmodelled COM method %s\n", w_last ? w_last->name : "?"); return E_NOTIMPL; }
static u32 un1(u32 a) { (void)a; return unm(); }
static u32 un2(u32 a, u32 b) { (void)a; (void)b; return unm(); }
static u32 un3(u32 a, u32 b, u32 c) { (void)a; (void)b; (void)c; return unm(); }
static u32 un4(u32 a, u32 b, u32 c, u32 d) { (void)a; (void)b; (void)c; (void)d; return unm(); }
static u32 un5(u32 a, u32 b, u32 c, u32 d, u32 e) { (void)a; (void)b; (void)c; (void)d; (void)e; return unm(); }
static u32 un6(u32 a, u32 b, u32 c, u32 d, u32 e, u32 f) { (void)a; (void)b; (void)c; (void)d; (void)e; (void)f; return unm(); }
static u32 un7(u32 a, u32 b, u32 c, u32 d, u32 e, u32 f, u32 g) { (void)a; (void)b; (void)c; (void)d; (void)e; (void)f; (void)g; return unm(); }
static u32 un8(u32 a, u32 b, u32 c, u32 d, u32 e, u32 f, u32 g, u32 h) { (void)a; (void)b; (void)c; (void)d; (void)e; (void)f; (void)g; (void)h; return unm(); }
static u32 un10(u32 a, u32 b, u32 c, u32 d, u32 e, u32 f, u32 g, u32 h, u32 i, u32 j) { (void)a; (void)b; (void)c; (void)d; (void)e; (void)f; (void)g; (void)h; (void)i; (void)j; return unm(); }
static void *g_unm[11] = { 0, (void *)un1, (void *)un2, (void *)un3, (void *)un4, (void *)un5,
                           (void *)un6, (void *)un7, (void *)un8, 0, (void *)un10 };

typedef struct { const char *name; int nargs; void *fn; } meth;
typedef struct { u32 ref; void *st; const char *iface; } comhdr;
static comhdr g_obj[4096];
static int g_nobj;
static pthread_mutex_t g_cl = PTHREAD_MUTEX_INITIALIZER;

static const char *sig_n(int n)
{
    static const char *s[] = { "_i", "i_i", "ii_i", "iii_i", "iiii_i", "iiiii_i", "iiiiii_i",
                               "iiiiiii_i", "iiiiiiii_i", "iiiiiiiii_i", "iiiiiiiiii_i" };
    return s[n];
}

typedef struct { const char *iface; u32 vt; } vtcache;
static vtcache g_vt[32];
static int g_nvt;

static u32 com_new(const char *iface, const meth *m, int n, void *st)
{
    u32 vt = 0, obj;
    int i;
    for (i = 0; i < g_nvt; i++) if (g_vt[i].iface == iface) vt = g_vt[i].vt;
    if (!vt) {
        vt = hmem_alloc((u32)n * 4, 1);
        for (i = 0; i < n; i++) {
            char *nm = malloc(96);
            void *fn = m[i].fn ? m[i].fn : g_unm[m[i].nargs];
            snprintf(nm, 96, "%s::%s", iface, m[i].name);
            /* a distinct entry per slot, so the stub can name itself */
            HW32(vt + 4 * (u32)i, w_addr_of_host(fn, sig_n(m[i].nargs), nm));
            if (!m[i].fn) {
                /* w_addr_of_host dedups by fn; unmodelled slots must not */
            }
        }
        g_vt[g_nvt++] = (vtcache){ iface, vt };
    }
    obj = hmem_alloc(16, 1);
    pthread_mutex_lock(&g_cl);
    i = ++g_nobj;
    pthread_mutex_unlock(&g_cl);
    g_obj[i] = (comhdr){ 1, st, iface };
    HW32(obj, vt);
    HW32(obj + 4, (u32)i);
    return obj;
}
static comhdr *hdr(u32 obj) { return &g_obj[H32(obj + 4) & 4095]; }
static u32 m_addref(u32 t) { return ++hdr(t)->ref; }
static u32 m_release(u32 t) { comhdr *h = hdr(t); if (h->ref) h->ref--; return h->ref; }

/* GUIDs */
static int guid_is(u32 p, u32 d1, u16 d2, u16 d3, const char *tail8)
{
    const u8 *g = W_P(p);
    u8 t[8];
    int i;
    for (i = 0; i < 8; i++) {
        unsigned v;
        sscanf(tail8 + 2 * i, "%2x", &v);
        t[i] = (u8)v;
    }
    return (g[0] | g[1] << 8 | g[2] << 16 | (u32)g[3] << 24) == d1 &&
           (u16)(g[4] | g[5] << 8) == d2 && (u16)(g[6] | g[7] << 8) == d3 && !memcmp(g + 8, t, 8);
}
#define IS_IUNKNOWN(p) guid_is(p, 0x00000000, 0x0000, 0x0000, "C000000000000046")

/* ============================================================ DirectDraw */
static u32 dd_qi(u32 t, u32 iid, u32 out);
static u32 ok1(u32 t) { (void)t; return S_OK; }
static u32 ok3(u32 t, u32 a, u32 b) { (void)t; (void)a; (void)b; return S_OK; }
static u32 dd_create_surface(u32 t, u32 desc, u32 out, u32 unk);
static const meth DD[] = {
    { "QueryInterface", 3, (void *)dd_qi }, { "AddRef", 1, (void *)m_addref }, { "Release", 1, (void *)m_release },
    { "Compact", 1, 0 }, { "CreateClipper", 4, 0 }, { "CreatePalette", 5, 0 },
    { "CreateSurface", 4, (void *)dd_create_surface }, { "DuplicateSurface", 3, 0 },
    { "EnumDisplayModes", 5, 0 }, { "EnumSurfaces", 5, 0 }, { "FlipToGDISurface", 1, 0 },
    { "GetCaps", 3, 0 }, { "GetDisplayMode", 2, 0 }, { "GetFourCCCodes", 3, 0 },
    { "GetGDISurface", 2, 0 }, { "GetMonitorFrequency", 2, 0 }, { "GetScanLine", 2, 0 },
    { "GetVerticalBlankStatus", 2, 0 }, { "Initialize", 2, 0 }, { "RestoreDisplayMode", 1, (void *)ok1 },
    { "SetCooperativeLevel", 3, (void *)ok3 }, { "SetDisplayMode", 6, 0 },
    { "WaitForVerticalBlank", 3, 0 }, { "GetAvailableVidMem", 4, 0 },
};
static u32 dds_qi(u32 t, u32 iid, u32 out);
static const meth DDS[] = {
    { "QueryInterface", 3, (void *)dds_qi }, { "AddRef", 1, (void *)m_addref }, { "Release", 1, (void *)m_release },
    { "AddAttachedSurface", 2, 0 }, { "AddOverlayDirtyRect", 2, 0 }, { "Blt", 6, 0 }, { "BltBatch", 4, 0 },
    { "BltFast", 6, 0 }, { "DeleteAttachedSurface", 3, 0 }, { "EnumAttachedSurfaces", 3, 0 },
    { "EnumOverlayZOrders", 4, 0 }, { "Flip", 3, 0 }, { "GetAttachedSurface", 3, 0 }, { "GetBltStatus", 2, 0 },
    { "GetCaps", 2, 0 }, { "GetClipper", 2, 0 }, { "GetColorKey", 3, 0 }, { "GetDC", 2, 0 },
    { "GetFlipStatus", 2, 0 }, { "GetOverlayPosition", 3, 0 }, { "GetPalette", 2, 0 },
    { "GetPixelFormat", 2, 0 }, { "GetSurfaceDesc", 2, 0 }, { "Initialize", 3, 0 }, { "IsLost", 1, 0 },
    { "Lock", 5, 0 }, { "ReleaseDC", 2, 0 }, { "Restore", 1, 0 }, { "SetClipper", 2, 0 },
    { "SetColorKey", 3, 0 }, { "SetOverlayPosition", 3, 0 }, { "SetPalette", 2, 0 }, { "Unlock", 2, 0 },
    { "UpdateOverlay", 6, 0 }, { "UpdateOverlayDisplay", 2, 0 }, { "UpdateOverlayZOrder", 3, 0 },
    { "GetDDInterface", 2, 0 }, { "PageLock", 2, 0 }, { "PageUnlock", 2, 0 }, { "SetSurfaceDesc", 3, 0 },
    { "SetPrivateData", 5, 0 }, { "GetPrivateData", 4, 0 }, { "FreePrivateData", 2, 0 },
    { "GetUniquenessValue", 2, 0 }, { "ChangeUniquenessValue", 1, 0 },
};
#define N(a) (int)(sizeof a / sizeof a[0])
static u32 dd_create_surface(u32 t, u32 desc, u32 out, u32 unk)
{
    (void)t; (void)desc; (void)unk;
    HW32(out, com_new("IDirectDrawSurface", DDS, N(DDS), 0));
    return S_OK;
}
static u32 dd_qi(u32 t, u32 iid, u32 out)
{
    if (IS_IUNKNOWN(iid) || guid_is(iid, 0x6C14DB80, 0xA733, 0x11CE, "A5210020AF0BE560") ||
        guid_is(iid, 0xB3A6F3E0, 0x2B43, 0x11CF, "A2DE00AA00B93356")) {
        HW32(out, com_new("IDirectDraw", DD, N(DD), 0));
        return S_OK;
    }
    (void)t;
    HW32(out, 0);
    return E_NOINTERFACE;
}
static u32 dds_qi(u32 t, u32 iid, u32 out)
{
    if (IS_IUNKNOWN(iid) || guid_is(iid, 0x6C14DB81, 0xA733, 0x11CE, "A5210020AF0BE560") ||
        guid_is(iid, 0xDA044E00, 0x69B2, 0x11D0, "A1D500AA00B8DFBB") ||
        guid_is(iid, 0x0B2B8630, 0xAD35, 0x11D0, "8EA600609797EA5B")) {
        HW32(out, com_new("IDirectDrawSurface", DDS, N(DDS), 0));
        return S_OK;
    }
    (void)t;
    HW32(out, 0);
    return E_NOINTERFACE;
}
static u32 h_DirectDrawCreate(u32 guid, u32 out, u32 unk)
{
    (void)guid; (void)unk;
    HW32(out, com_new("IDirectDraw", DD, N(DD), 0));
    return S_OK;
}

/* =========================================================== DirectInput */
typedef struct { int kind; int acquired; } didev;   /* kind 1 keyboard, 2 mouse */
static u32 di_qi(u32 t, u32 iid, u32 out);
static u32 did_qi(u32 t, u32 iid, u32 out);
static u32 di_create_device(u32 t, u32 guid, u32 out, u32 unk);
static u32 di_enum(u32 t, u32 type, u32 cb, u32 ref, u32 fl) { (void)t; (void)type; (void)cb; (void)ref; (void)fl; return S_OK; }
static const meth DI[] = {
    { "QueryInterface", 3, (void *)di_qi }, { "AddRef", 1, (void *)m_addref }, { "Release", 1, (void *)m_release },
    { "CreateDevice", 4, (void *)di_create_device }, { "EnumDevices", 5, (void *)di_enum },
    { "GetDeviceStatus", 2, 0 }, { "RunControlPanel", 3, 0 }, { "Initialize", 3, 0 },
};
static u32 did_fmt(u32 t, u32 f) { (void)t; (void)f; return S_OK; }
static u32 did_prop(u32 t, u32 a, u32 b) { (void)t; (void)a; (void)b; return S_OK; }
static u32 did_acq(u32 t) { didev *d = hdr(t)->st; int w = d->acquired; d->acquired = 1; return w ? 1 : S_OK; }
static u32 did_unacq(u32 t) { didev *d = hdr(t)->st; int w = d->acquired; d->acquired = 0; return w ? S_OK : 1; }
static u32 did_poll(u32 t) { (void)t; return 1; }
static int g_mdx, g_mdy, g_mbtn;
void hdx_mouse(int dx, int dy, int btn) { g_mdx += dx; g_mdy += dy; g_mbtn = btn; }
static u32 did_state(u32 t, u32 n, u32 p)
{
    didev *d = hdr(t)->st;
    if (!d->acquired) return 0x8007000Cu;          /* DIERR_NOTACQUIRED */
    if (d->kind == 1) {
        u8 k[256];
        happ_dik_state(k);
        memcpy(W_P(p), k, n < 256 ? n : 256);
        return S_OK;
    }
    memset(W_P(p), 0, n);
    if (n >= 12) { HW32(p, g_mdx); HW32(p + 4, g_mdy); }
    if (n >= 16) { u8 *b = W_P(p + 12); b[0] = g_mbtn & 1 ? 0x80 : 0; b[1] = g_mbtn & 2 ? 0x80 : 0; }
    g_mdx = g_mdy = 0;
    return S_OK;
}
static u32 did_data(u32 t, u32 sz, u32 buf, u32 pn, u32 fl) { (void)t; (void)sz; (void)buf; (void)fl; if (pn) HW32(pn, 0); return S_OK; }
static u32 did_coop(u32 t, u32 h, u32 f) { (void)t; (void)h; (void)f; return S_OK; }
static u32 did_caps(u32 t, u32 p)
{
    u32 sz = H32(p);
    (void)t;
    memset(W_P(p + 4), 0, sz > 4 ? sz - 4 : 0);
    HW32(p + 4, 1);
    return S_OK;
}
static const meth DID[] = {
    { "QueryInterface", 3, (void *)did_qi }, { "AddRef", 1, (void *)m_addref }, { "Release", 1, (void *)m_release },
    { "GetCapabilities", 2, (void *)did_caps }, { "EnumObjects", 4, 0 }, { "GetProperty", 3, 0 },
    { "SetProperty", 3, (void *)did_prop }, { "Acquire", 1, (void *)did_acq }, { "Unacquire", 1, (void *)did_unacq },
    { "GetDeviceState", 3, (void *)did_state }, { "GetDeviceData", 5, (void *)did_data },
    { "SetDataFormat", 2, (void *)did_fmt }, { "SetEventNotification", 2, 0 },
    { "SetCooperativeLevel", 3, (void *)did_coop }, { "GetObjectInfo", 4, 0 }, { "GetDeviceInfo", 2, 0 },
    { "RunControlPanel", 3, 0 }, { "Initialize", 4, 0 }, { "CreateEffect", 5, 0 }, { "EnumEffects", 4, 0 },
    { "GetEffectInfo", 3, 0 }, { "GetForceFeedbackState", 2, 0 }, { "SendForceFeedbackCommand", 2, 0 },
    { "EnumCreatedEffectObjects", 4, 0 }, { "Escape", 2, 0 }, { "Poll", 1, (void *)did_poll },
    { "SendDeviceData", 5, 0 },
};
static u32 di_create_device(u32 t, u32 guid, u32 out, u32 unk)
{
    didev *d;
    int kind = 0;
    (void)t; (void)unk;
    if (guid_is(guid, 0x6F1D2B61, 0xD5A0, 0x11CF, "BFC7444553540000")) kind = 1;
    else if (guid_is(guid, 0x6F1D2B60, 0xD5A0, 0x11CF, "BFC7444553540000")) kind = 2;
    HLOG("DirectInput CreateDevice(%08X-...) -> %s\n", H32(guid), kind == 1 ? "keyboard" : kind == 2 ? "mouse" : "none");
    if (!kind) { HW32(out, 0); return 0x80040154u; }   /* DIERR_DEVICENOTREG */
    d = calloc(1, sizeof *d);
    d->kind = kind;
    HW32(out, com_new("IDirectInputDeviceA", DID, N(DID), d));
    return S_OK;
}
static u32 di_qi(u32 t, u32 iid, u32 out) { (void)iid; m_addref(t); HW32(out, t); return S_OK; }
static u32 did_qi(u32 t, u32 iid, u32 out)
{
    if (IS_IUNKNOWN(iid) || guid_is(iid, 0x5944E680, 0xC92E, 0x11CF, "BFC7444553540000") ||
        guid_is(iid, 0x5944E682, 0xC92E, 0x11CF, "BFC7444553540000")) {
        m_addref(t);
        HW32(out, t);
        return S_OK;
    }
    HW32(out, 0);
    return E_NOINTERFACE;
}
u32 h_DirectInputCreateA(u32 inst, u32 ver, u32 out, u32 unk)
{
    (void)inst; (void)ver; (void)unk;
    HW32(out, com_new("IDirectInputA", DI, N(DI), 0));
    return S_OK;
}

/* =========================================================== DirectSound */
typedef struct { u32 flags, size, mem, rate, align, playing, looping, pos; u64 t0; u8 fmt[18]; } dsbuf;
static u32 ds_init(u32 t, u32 g) { (void)t; (void)g; return S_OK; }
static u32 ds_coop(u32 t, u32 h, u32 l) { (void)t; (void)h; (void)l; return S_OK; }
static u32 ds_caps(u32 t, u32 p) { u32 sz = H32(p); (void)t; memset(W_P(p + 4), 0, sz > 4 ? sz - 4 : 0); HW32(p + 4, 0x0F0F); return S_OK; }
static u32 ds_spk(u32 t, u32 p) { (void)t; HW32(p, 4); return S_OK; }
static u32 ds_create(u32 t, u32 desc, u32 out, u32 unk);
static const meth DS[] = {
    { "QueryInterface", 3, 0 }, { "AddRef", 1, (void *)m_addref }, { "Release", 1, (void *)m_release },
    { "CreateSoundBuffer", 4, (void *)ds_create }, { "GetCaps", 2, (void *)ds_caps },
    { "DuplicateSoundBuffer", 3, 0 }, { "SetCooperativeLevel", 3, (void *)ds_coop }, { "Compact", 1, 0 },
    { "GetSpeakerConfig", 2, (void *)ds_spk }, { "SetSpeakerConfig", 2, 0 }, { "Initialize", 2, (void *)ds_init },
};
u32 h_timeGetTime(void);
static u32 dsb_cur(dsbuf *b)
{
    u64 p;
    if (!b->playing) return b->pos;
    p = b->pos + (u64)(h_timeGetTime() - b->t0) * b->rate * b->align / 1000;
    p -= p % b->align;
    if (p >= b->size) {
        if (b->looping) p %= b->size;
        else { b->playing = 0; b->pos = 0; return 0; }
    }
    return (u32)p;
}
static u32 dsb_pos(u32 t, u32 pp, u32 pw) { dsbuf *b = hdr(t)->st; u32 p = dsb_cur(b); if (pp) HW32(pp, p); if (pw) HW32(pw, p); return S_OK; }
static u32 dsb_status(u32 t, u32 p) { dsbuf *b = hdr(t)->st; dsb_cur(b); HW32(p, (b->playing ? 1 : 0) | (b->playing && b->looping ? 4 : 0)); return S_OK; }
static u32 dsb_caps(u32 t, u32 p) { dsbuf *b = hdr(t)->st; HW32(p + 4, b->flags); HW32(p + 8, b->size); HW32(p + 12, 0); return S_OK; }
static u32 dsb_getfmt(u32 t, u32 p, u32 n, u32 pw)
{
    dsbuf *b = hdr(t)->st;
    if (p) memcpy(W_P(p), b->fmt, n && n < 18 ? n : 18);
    if (pw) HW32(pw, 18);
    return S_OK;
}
static u32 dsb_setfmt(u32 t, u32 p)
{
    dsbuf *b = hdr(t)->st;
    memcpy(b->fmt, W_P(p), 16);
    b->rate = H32(p + 4);
    b->align = *(u16 *)W_P(p + 12) ? *(u16 *)W_P(p + 12) : 1;
    return S_OK;
}
static u32 dsb_lock(u32 t, u32 off, u32 n, u32 pp1, u32 pn1, u32 pp2, u32 pn2, u32 fl)
{
    dsbuf *b = hdr(t)->st;
    u32 n1;
    if (fl & 1) off = dsb_cur(b);
    if (fl & 2) { off = 0; n = b->size; }
    off %= b->size ? b->size : 1;
    if (n > b->size) n = b->size;
    n1 = n < b->size - off ? n : b->size - off;
    HW32(pp1, b->mem + off);
    HW32(pn1, n1);
    if (pp2) HW32(pp2, n > n1 ? b->mem : 0);
    if (pn2) HW32(pn2, n - n1);
    return S_OK;
}
static u32 dsb_unlock(u32 t, u32 a, u32 b, u32 c, u32 d) { (void)t; (void)a; (void)b; (void)c; (void)d; return S_OK; }
static u32 dsb_play(u32 t, u32 r1, u32 r2, u32 fl)
{
    dsbuf *b = hdr(t)->st;
    (void)r1; (void)r2;
    if (!b->playing) { b->playing = 1; b->t0 = h_timeGetTime(); }
    b->looping = fl & 1;
    return S_OK;
}
static u32 dsb_stop(u32 t) { dsbuf *b = hdr(t)->st; b->pos = dsb_cur(b); b->playing = 0; return S_OK; }
static u32 dsb_setpos(u32 t, u32 p) { dsbuf *b = hdr(t)->st; b->pos = p % (b->size ? b->size : 1); b->t0 = h_timeGetTime(); return S_OK; }
static u32 dsb_ok2(u32 t, u32 v) { (void)t; (void)v; return S_OK; }
static u32 dsb_get2(u32 t, u32 p) { (void)t; if (p) HW32(p, 0); return S_OK; }
static const meth DSB[] = {
    { "QueryInterface", 3, 0 }, { "AddRef", 1, (void *)m_addref }, { "Release", 1, (void *)m_release },
    { "GetCaps", 2, (void *)dsb_caps }, { "GetCurrentPosition", 3, (void *)dsb_pos },
    { "GetFormat", 4, (void *)dsb_getfmt }, { "GetVolume", 2, (void *)dsb_get2 }, { "GetPan", 2, (void *)dsb_get2 },
    { "GetFrequency", 2, (void *)dsb_get2 }, { "GetStatus", 2, (void *)dsb_status }, { "Initialize", 3, 0 },
    { "Lock", 8, (void *)dsb_lock }, { "Play", 4, (void *)dsb_play }, { "SetCurrentPosition", 2, (void *)dsb_setpos },
    { "SetFormat", 2, (void *)dsb_setfmt }, { "SetVolume", 2, (void *)dsb_ok2 }, { "SetPan", 2, (void *)dsb_ok2 },
    { "SetFrequency", 2, (void *)dsb_ok2 }, { "Stop", 1, (void *)dsb_stop }, { "Unlock", 5, (void *)dsb_unlock },
    { "Restore", 1, (void *)ok1 },
};
static u32 ds_create(u32 t, u32 desc, u32 out, u32 unk)
{
    dsbuf *b = calloc(1, sizeof *b);
    u32 wfx = H32(desc + 16);
    (void)t; (void)unk;
    b->flags = H32(desc + 4);
    b->size = H32(desc + 8);
    if (b->flags & 1) {                            /* DSBCAPS_PRIMARYBUFFER */
        static const u8 f[16] = { 1, 0, 2, 0, 0x22, 0x56, 0, 0, 0x88, 0x58, 1, 0, 4, 0, 16, 0 };
        memcpy(b->fmt, f, 16);
        b->size = 0x4000;
    } else {
        if (!wfx || !b->size) { free(b); HW32(out, 0); return 0x80070057u; }
        memcpy(b->fmt, W_P(wfx), 16);
    }
    b->rate = b->fmt[4] | b->fmt[5] << 8 | b->fmt[6] << 16 | (u32)b->fmt[7] << 24;
    b->align = (u32)(b->fmt[12] | b->fmt[13] << 8);
    if (!b->align) b->align = 1;
    b->mem = hmem_alloc(b->size, 1);
    HW32(out, com_new("IDirectSoundBuffer", DSB, N(DSB), b));
    return S_OK;
}

/* ============================================================ DirectPlay */
static u32 dpl_conn(u32 t, u32 a, u32 b, u32 c) { (void)t; (void)a; (void)b; (void)c; return 0x8877042Eu; } /* DPERR_NOTLOBBIED */
static u32 dpl_qi(u32 t, u32 iid, u32 out) { (void)iid; m_addref(t); HW32(out, t); return S_OK; }
static const meth DPL[] = {
    { "QueryInterface", 3, (void *)dpl_qi }, { "AddRef", 1, (void *)m_addref }, { "Release", 1, (void *)m_release },
    { "Connect", 4, 0 }, { "CreateAddress", 7, 0 }, { "EnumAddress", 5, 0 }, { "EnumAddressTypes", 5, 0 },
    { "EnumLocalApplications", 4, 0 }, { "GetConnectionSettings", 4, (void *)dpl_conn },
    { "ReceiveLobbyMessage", 6, 0 }, { "RunApplication", 5, 0 }, { "SendLobbyMessage", 5, 0 },
    { "SetConnectionSettings", 4, 0 }, { "SetLobbyMessageEvent", 4, 0 }, { "CreateCompoundAddress", 5, 0 },
    { "ConnectEx", 5, 0 }, { "RegisterApplication", 3, 0 }, { "UnregisterApplication", 3, 0 },
    { "WaitForConnectionSettings", 2, 0 },
};
static u32 dp_enumconn(u32 t, u32 g, u32 cb, u32 ref, u32 fl) { (void)t; (void)g; (void)cb; (void)ref; (void)fl; return S_OK; }
static u32 dp_close(u32 t) { (void)t; return S_OK; }
static const meth DP4[] = {
    { "QueryInterface", 3, (void *)dpl_qi }, { "AddRef", 1, (void *)m_addref }, { "Release", 1, (void *)m_release },
    { "AddPlayerToGroup", 3, 0 }, { "Close", 1, (void *)dp_close }, { "CreateGroup", 6, 0 }, { "CreatePlayer", 7, 0 },
    { "DeletePlayerFromGroup", 3, 0 }, { "DestroyGroup", 2, 0 }, { "DestroyPlayer", 2, 0 },
    { "EnumGroupPlayers", 6, 0 }, { "EnumGroups", 5, 0 }, { "EnumPlayers", 5, 0 }, { "EnumSessions", 6, 0 },
    { "GetCaps", 3, 0 }, { "GetGroupData", 5, 0 }, { "GetGroupName", 4, 0 }, { "GetMessageCount", 3, 0 },
    { "GetPlayerAddress", 4, 0 }, { "GetPlayerCaps", 4, 0 }, { "GetPlayerData", 5, 0 },
    { "GetPlayerName", 4, 0 }, { "GetSessionDesc", 3, 0 }, { "Initialize", 2, 0 }, { "Open", 3, 0 },
    { "Receive", 6, 0 }, { "Send", 6, 0 }, { "SetGroupData", 5, 0 }, { "SetGroupName", 4, 0 },
    { "SetPlayerData", 5, 0 }, { "SetPlayerName", 4, 0 }, { "SetSessionDesc", 3, 0 },
    { "AddGroupToGroup", 3, 0 }, { "CreateGroupInGroup", 7, 0 }, { "DeleteGroupFromGroup", 3, 0 },
    { "EnumConnections", 5, (void *)dp_enumconn }, { "EnumGroupsInGroup", 6, 0 },
    { "GetGroupConnectionSettings", 5, 0 }, { "InitializeConnection", 3, 0 }, { "SecureOpen", 5, 0 },
    { "SendChatMessage", 5, 0 }, { "SetGroupConnectionSettings", 4, 0 }, { "StartSession", 3, 0 },
    { "GetGroupFlags", 3, 0 }, { "GetGroupParent", 3, 0 }, { "GetPlayerAccount", 5, 0 },
    { "GetPlayerFlags", 3, 0 }, { "GetGroupOwner", 3, 0 }, { "SetGroupOwner", 3, 0 }, { "SendEx", 10, 0 },
    { "GetMessageQueue", 6, 0 }, { "CancelMessage", 3, 0 }, { "CancelPriority", 4, 0 },
};

u32 h_CoCreateInstance(u32 clsid, u32 outer, u32 ctx, u32 iid, u32 out)
{
    (void)outer; (void)ctx; (void)iid;
    if (guid_is(clsid, 0x2FE8F810, 0xB2A5, 0x11D0, "A7870000F803ABFC")) {
        HW32(out, com_new("IDirectPlayLobby3A", DPL, N(DPL), 0));
        return S_OK;
    }
    if (guid_is(clsid, 0xD1EB6D20, 0x8923, 0x11D0, "9D9700A0C90A43CB")) {
        HW32(out, com_new("IDirectPlay4A", DP4, N(DP4), 0));
        return S_OK;
    }
    if (guid_is(clsid, 0x47D4D946, 0x62E8, 0x11CF, "93BC444553540000")) {
        HW32(out, com_new("IDirectSound", DS, N(DS), 0));
        return S_OK;
    }
    fprintf(stderr, "CoCreateInstance: unknown CLSID\n");
    HW32(out, 0);
    return 0x80040154u;                                       /* REGDB_E_CLASSNOTREG */
}
/* dplayx.dll ordinal 4: DirectPlayCreate(lpGUID, lplpDP, pUnk) */
u32 h__4(u32 g, u32 out, u32 unk, u32 x, u32 y)
{
    (void)g; (void)unk; (void)x; (void)y;
    HW32(out, com_new("IDirectPlay4A", DP4, N(DP4), 0));
    return S_OK;
}

/* the exports ddraw.dll / dinput.dll answer GetProcAddress with */
u32 hdx_proc(const char *lib, const char *name)
{
    if (!strcasecmp(name, "DirectDrawCreate"))
        return w_addr_of_host((void *)h_DirectDrawCreate, "iii_i", "DirectDrawCreate");
    if (!strcasecmp(name, "DirectInputCreateA"))
        return w_addr_of_host((void *)h_DirectInputCreateA, "iiii_i", "DirectInputCreateA");
    (void)lib;
    return 0;
}
