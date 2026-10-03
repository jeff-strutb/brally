/* win_user.c: user32, gdi32, advapi32 and ole32 for the game.
 *
 * One window class table, the game's windows, one thread message queue and
 * the timers. Host events (platform/host) become the window messages the
 * game's window procedure handles: keys, characters, focus, close. */
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "plat.h"

/* ---- classes and windows ------------------------------------------------------- */
typedef struct pclass { char name[64]; WNDPROC proc; } pclass;
typedef struct pwnd {
    int     magic;
    WNDPROC proc;
    DWORD   style, exstyle;
    LONG    user;
    int     visible;
} pwnd;
#define WND_MAGIC 0x57494E44

static pclass s_class[16];
static int    s_nclass;
static pwnd  *s_main;
static int    s_active;

typedef struct {
    LPVOID    lpCreateParams;
    HINSTANCE hInstance;
    HMENU     hMenu;
    HWND      hwndParent;
    int       cy, cx, y, x;
    LONG      style;
    LPCSTR    lpszName, lpszClass;
    DWORD     dwExStyle;
} CREATESTRUCTA;

static pwnd *W(HWND h)
{
    pwnd *w = (pwnd *)h;
    return (w && w->magic == WND_MAGIC) ? w : NULL;
}

HWND plat_main_window(void) { return (HWND)s_main; }

ATOM WINAPI RegisterClassA(const WNDCLASSA *wc)
{
    if (s_nclass >= 16 || !wc || !wc->lpszClassName)
        return 0;
    snprintf(s_class[s_nclass].name, sizeof s_class[0].name, "%s", wc->lpszClassName);
    s_class[s_nclass].proc = wc->lpfnWndProc;
    return (ATOM)(0xC000 + s_nclass++);
}

static LRESULT send(pwnd *w, UINT m, WPARAM wp, LPARAM lp)
{
    return (w && w->proc) ? w->proc((HWND)w, m, wp, lp) : 0;
}

HWND WINAPI CreateWindowExA(DWORD ex, LPCSTR cls, LPCSTR title, DWORD style, int x, int y,
                            int cx, int cy, HWND parent, HMENU menu, HINSTANCE inst, LPVOID param)
{
    pwnd *w;
    CREATESTRUCTA cs;
    int i;
    for (i = 0; i < s_nclass; i++)
        if (cls && !strcasecmp(s_class[i].name, cls))
            break;
    if (i == s_nclass)
        return NULL;
    w = (pwnd *)calloc(1, sizeof *w);
    w->magic = WND_MAGIC;
    w->proc = s_class[i].proc;
    w->style = style;
    w->exstyle = ex;
    if (!s_main) {
        s_main = w;
        host_window_open(cx > 0 ? cx : 640, cy > 0 ? cy : 480, title);
    }
    memset(&cs, 0, sizeof cs);
    cs.lpCreateParams = param;
    cs.hInstance = inst;
    cs.hMenu = menu;
    cs.hwndParent = parent;
    cs.x = x;
    cs.y = y;
    cs.cx = cx;
    cs.cy = cy;
    cs.style = (LONG)style;
    cs.lpszName = title;
    cs.lpszClass = cls;
    cs.dwExStyle = ex;
    if (send(w, WM_CREATE, 0, (LPARAM)&cs) == -1) {
        free(w);
        if (s_main == w)
            s_main = NULL;
        return NULL;
    }
    if (style & WS_VISIBLE)
        ShowWindow((HWND)w, SW_SHOW);
    return (HWND)w;
}

LRESULT WINAPI DefWindowProcA(HWND h, UINT m, WPARAM wp, LPARAM lp)
{
    (void)wp;
    (void)lp;
    if (m == WM_CLOSE)
        PostQuitMessage(0);
    (void)h;
    return 0;
}

BOOL WINAPI ShowWindow(HWND h, int cmd)
{
    pwnd *w = W(h);
    if (!w)
        return FALSE;
    if (cmd != SW_HIDE && !w->visible) {
        w->visible = 1;
        send(w, WM_SHOWWINDOW, 1, 0);
        s_active = 1;
        send(w, WM_ACTIVATEAPP, 1, 0);
        send(w, WM_ACTIVATE, 1, 0);
        send(w, WM_SETFOCUS, 0, 0);
    }
    return TRUE;
}

BOOL WINAPI UpdateWindow(HWND h) { return W(h) != NULL; }
BOOL WINAPI IsWindow(HWND h) { return W(h) != NULL; }
BOOL WINAPI IsIconic(HWND h) { (void)h; return FALSE; }
BOOL WINAPI BringWindowToTop(HWND h) { return W(h) != NULL; }
BOOL WINAPI SetForegroundWindow(HWND h) { return W(h) != NULL; }
HWND WINAPI GetForegroundWindow(void) { return s_active ? (HWND)s_main : NULL; }
HWND WINAPI GetActiveWindow(void) { return s_active ? (HWND)s_main : NULL; }
HWND WINAPI GetDesktopWindow(void) { static int desk; return (HWND)&desk; }
HWND WINAPI GetLastActivePopup(HWND h) { return h; }
HWND WINAPI SetFocus(HWND h) { return h; }
HWND WINAPI FindWindowA(LPCSTR cls, LPCSTR title) { (void)cls; (void)title; return NULL; }
DWORD WINAPI GetWindowThreadProcessId(HWND h, LPDWORD pid) { (void)h; if (pid) *pid = 1; return 1; }
BOOL WINAPI InvalidateRect(HWND h, const RECT *r, BOOL e) { (void)r; (void)e; return W(h) != NULL; }

LONG WINAPI GetWindowLongA(HWND h, int i)
{
    pwnd *w = W(h);
    if (!w)
        return 0;
    switch (i) {
    case GWL_STYLE:    return (LONG)w->style;
    case GWL_EXSTYLE:  return (LONG)w->exstyle;
    case GWL_USERDATA: return w->user;
    }
    return 0;
}

/* ---- the message queue ------------------------------------------------------------- */
#define QN 512
static MSG s_q[QN];
static int s_qh, s_qt;
static int s_quit = -1;

static int qempty(void) { return s_qh == s_qt; }

static void qpush(HWND h, UINT m, WPARAM wp, LPARAM lp)
{
    int n = (s_qt + 1) % QN;
    if (n == s_qh)
        return;              /* full: the message is lost, as on Windows */
    s_q[s_qt].hwnd = h;
    s_q[s_qt].message = m;
    s_q[s_qt].wParam = wp;
    s_q[s_qt].lParam = lp;
    s_q[s_qt].time = plat_time_ms();
    s_qt = n;
}

BOOL WINAPI PostMessageA(HWND h, UINT m, WPARAM wp, LPARAM lp)
{
    qpush(h, m, wp, lp);
    return TRUE;
}

void WINAPI PostQuitMessage(int code) { s_quit = code; }

/* ---- timers -------------------------------------------------------------------------- */
typedef struct ptimer { HWND h; UINT_PTR id; UINT ms; TIMERPROC fn; DWORD due; int live; } ptimer;
static ptimer s_timer[16];

UINT_PTR WINAPI SetTimer(HWND h, UINT_PTR id, UINT ms, TIMERPROC fn)
{
    int i, free_ = -1;
    for (i = 0; i < 16; i++) {
        if (s_timer[i].live && s_timer[i].h == h && s_timer[i].id == id)
            break;
        if (!s_timer[i].live && free_ < 0)
            free_ = i;
    }
    if (i == 16)
        i = free_;
    if (i < 0)
        return 0;
    s_timer[i].h = h;
    s_timer[i].id = id ? id : (UINT_PTR)(i + 1);
    s_timer[i].ms = ms;
    s_timer[i].fn = fn;
    s_timer[i].due = plat_time_ms() + ms;
    s_timer[i].live = 1;
    return s_timer[i].id;
}

BOOL WINAPI KillTimer(HWND h, UINT_PTR id)
{
    int i;
    for (i = 0; i < 16; i++)
        if (s_timer[i].live && s_timer[i].h == h && s_timer[i].id == id) {
            s_timer[i].live = 0;
            return TRUE;
        }
    return FALSE;
}

static void timers(void)
{
    DWORD now = plat_time_ms();
    int i;
    for (i = 0; i < 16; i++) {
        ptimer *t = &s_timer[i];
        if (t->live && (int32_t)(now - t->due) >= 0) {
            t->due = now + t->ms;
            qpush(t->h, WM_TIMER, t->id, (LPARAM)t->fn);
        }
    }
}

/* ---- keys ----------------------------------------------------------------------------- */
static uint8_t s_key[256];        /* bit 7 down, bit 0 pressed since the last query */

SHORT WINAPI GetAsyncKeyState(int vk)
{
    SHORT r;
    vk &= 0xFF;
    r = (SHORT)((s_key[vk] & 0x80 ? 0x8000 : 0) | (s_key[vk] & 1));
    s_key[vk] &= 0x80;
    return r;
}

/* DirectInput's keyboard state reads the same table by scan code */
uint8_t g_plat_dik[256];

/* ---- host events -> messages ------------------------------------------------------------ */
void plat_deliver(const host_event *e)
{
    host_event ev = *e;
    {
        switch (ev.type) {
        case HOST_EV_KEY:
            if (ev.vk > 0 && ev.vk < 256)
                s_key[ev.vk] = ev.down ? (uint8_t)(0x80 | 1 | s_key[ev.vk]) : 0;
            if (ev.scan > 0 && ev.scan < 256)
                g_plat_dik[ev.scan] = ev.down ? 0x80 : 0;
            if (s_main)
                qpush((HWND)s_main, ev.down ? WM_KEYDOWN : WM_KEYUP, (WPARAM)ev.vk,
                      (LPARAM)(1 | ((ev.scan & 0xFF) << 16) | (ev.down ? 0 : (3u << 30))));
            break;
        case HOST_EV_CHAR:
            if (s_main)
                qpush((HWND)s_main, WM_CHAR, (WPARAM)ev.ch, 1);
            break;
        case HOST_EV_MOUSE: {
            /* the game reads the mouse through DirectInput: movement as the
             * change since the last position, and the left button */
            static int lx = -1, ly;
            if (lx >= 0)
                plat_mouse_move(ev.x - lx, ev.y - ly);
            lx = ev.x;
            ly = ev.y;
            plat_mouse_button(ev.buttons & 1);
            if (s_main)
                qpush((HWND)s_main, WM_MOUSEMOVE, (WPARAM)ev.buttons, (LPARAM)MAKELONG(ev.x, ev.y));
            break;
        }
        case HOST_EV_FOCUS:
            s_active = ev.down;
            if (s_main) {
                qpush((HWND)s_main, WM_ACTIVATEAPP, (WPARAM)ev.down, 0);
                qpush((HWND)s_main, WM_ACTIVATE, (WPARAM)(ev.down ? 1 : WA_INACTIVE), 0);
            }
            break;
        case HOST_EV_CLOSE:
            if (s_main)
                qpush((HWND)s_main, WM_CLOSE, 0, 0);
            break;
        }
    }
}

void plat_pump(uint32_t wait_ms)
{
    host_event ev;
    uint32_t w = wait_ms;
    if (w && plat_vclock_main()) {       /* virtual time: the wait costs its length */
        plat_vclock_advance((uint64_t)w * 1000u);
        w = 0;
    }
    while (host_poll_event(&ev, w)) {
        w = 0;
        plat_deliver(&ev);
    }
    timers();
}

static BOOL take(LPMSG m, UINT remove)
{
    if (qempty())
        return FALSE;
    *m = s_q[s_qh];
    if (remove & PM_REMOVE)
        s_qh = (s_qh + 1) % QN;
    return TRUE;
}

static BOOL quit_msg(LPMSG m)
{
    memset(m, 0, sizeof *m);
    m->message = WM_QUIT;
    m->wParam = (WPARAM)s_quit;
    return TRUE;
}

BOOL WINAPI PeekMessageA(LPMSG m, HWND h, UINT lo, UINT hi, UINT remove)
{
    (void)h;
    (void)lo;
    (void)hi;
    plat_pump(0);
    if (take(m, remove))
        return TRUE;
    if (s_quit >= 0) {
        quit_msg(m);
        if (remove & PM_REMOVE)
            s_quit = -1;
        return TRUE;
    }
    return FALSE;
}

BOOL WINAPI GetMessageA(LPMSG m, HWND h, UINT lo, UINT hi)
{
    (void)h;
    (void)lo;
    (void)hi;
    for (;;) {
        plat_pump(0);
        if (take(m, PM_REMOVE))
            return m->message != WM_QUIT;
        if (s_quit >= 0) {
            quit_msg(m);
            s_quit = -1;
            return FALSE;
        }
        plat_pump(5);
    }
}

BOOL WINAPI WaitMessage(void)
{
    while (qempty() && s_quit < 0)
        plat_pump(5);
    return TRUE;
}

BOOL WINAPI TranslateMessage(const MSG *m) { (void)m; return FALSE; }

LRESULT WINAPI DispatchMessageA(const MSG *m)
{
    if (m->message == WM_TIMER && m->lParam) {
        ((TIMERPROC)m->lParam)(m->hwnd, WM_TIMER, m->wParam, plat_time_ms());
        return 0;
    }
    return send(W(m->hwnd), m->message, m->wParam, m->lParam);
}

UINT WINAPI RegisterWindowMessageA(LPCSTR name)
{
    static char names[32][64];
    static int n;
    int i;
    for (i = 0; i < n; i++)
        if (!strcmp(names[i], name))
            return 0xC100 + i;
    if (n == 32)
        return 0;
    snprintf(names[n], sizeof names[0], "%s", name);
    return 0xC100 + n++;
}

/* ---- cursors, icons, message boxes ---------------------------------------------------------- */
HCURSOR WINAPI SetCursor(HCURSOR c) { (void)c; return NULL; }
HCURSOR WINAPI LoadCursorA(HINSTANCE h, LPCSTR n) { static int cur; (void)h; (void)n; return (HCURSOR)&cur; }
HICON   WINAPI LoadIconA(HINSTANCE h, LPCSTR n)   { static int ico; (void)h; (void)n; return (HICON)&ico; }

int WINAPI MessageBoxA(HWND h, LPCSTR text, LPCSTR caption, UINT flags)
{
    (void)h;
    (void)flags;
    host_message_box(text, caption);
    return IDOK;
}

/* ---- gdi: bitmaps loaded from .BMP files ------------------------------------------------------ */
typedef struct pbitmap {
    int      magic;
    LONG     type, cx, cy, stride;
    WORD     planes, bpp;
    uint8_t *bits;            /* bottom row first, as a DIB section */
} pbitmap;
#define BMP_MAGIC 0x424D5021

typedef struct {               /* the SDK's BITMAP at Win64 width */
    LONG   bmType, bmWidth, bmHeight, bmWidthBytes;
    WORD   bmPlanes, bmBitsPixel;
    LPVOID bmBits;
} PLAT_BITMAP;

static uint32_t le32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static uint16_t le16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }

HANDLE WINAPI LoadImageA(HINSTANCE inst, LPCSTR name, UINT type, int cx, int cy, UINT flags)
{
    char host[1024];
    FILE *f;
    uint8_t hdr[54];
    pbitmap *b;
    uint32_t off, size;
    (void)inst;
    (void)cx;
    (void)cy;
    if (type != IMAGE_BITMAP || !(flags & LR_LOADFROMFILE) || !plat_path(name, host, sizeof host))
        return NULL;        /* the executable carries no bitmap resources */
    f = fopen(host, "rb");
    if (!f)
        return NULL;
    if (fread(hdr, 1, 54, f) != 54 || hdr[0] != 'B' || hdr[1] != 'M') {
        fclose(f);
        return NULL;
    }
    b = (pbitmap *)calloc(1, sizeof *b);
    b->magic = BMP_MAGIC;
    off = le32(hdr + 10);
    b->cx = (LONG)le32(hdr + 18);
    b->cy = (LONG)le32(hdr + 22);
    b->planes = le16(hdr + 26);
    b->bpp = le16(hdr + 28);
    if (b->cy < 0)
        b->cy = -b->cy;
    b->stride = ((b->cx * b->bpp + 31) / 32) * 4;
    size = (uint32_t)b->stride * (uint32_t)b->cy;
    b->bits = (uint8_t *)calloc(1, size ? size : 1);
    fseek(f, (long)off, SEEK_SET);
    if (fread(b->bits, 1, size, f) != size)
        PLOG("LoadImageA(%s): short read\n", name);
    fclose(f);
    return b;
}

int WINAPI GetObjectA(HANDLE h, int n, LPVOID out)
{
    pbitmap *b = (pbitmap *)h;
    PLAT_BITMAP bm;
    (void)n;
    if (!b || b->magic != BMP_MAGIC || !out)
        return 0;
    bm.bmType = 0;
    bm.bmWidth = b->cx;
    bm.bmHeight = b->cy;
    bm.bmWidthBytes = b->stride;
    bm.bmPlanes = b->planes;
    bm.bmBitsPixel = b->bpp;
    bm.bmBits = b->bits;
    memcpy(out, &bm, sizeof bm);
    return (int)sizeof bm;
}

HGDIOBJ WINAPI GetStockObject(int i) { static int stock[8]; return &stock[i & 7]; }

BOOL WINAPI DeleteObject(HGDIOBJ h)
{
    pbitmap *b = (pbitmap *)h;
    if (b && b->magic == BMP_MAGIC) {
        b->magic = 0;
        free(b->bits);
        free(b);
    }
    return TRUE;
}

/* ---- registry: nothing installed; the game falls back to its defaults --------------------------- */
#define ERROR_FILE_NOT_FOUND 2L
LONG WINAPI RegOpenKeyExA(HKEY k, LPCSTR sub, DWORD o, DWORD sam, PHKEY out)
{
    (void)k;
    (void)sub;
    (void)o;
    (void)sam;
    if (out)
        *out = NULL;
    return ERROR_FILE_NOT_FOUND;
}
LONG WINAPI RegQueryValueExA(HKEY k, LPCSTR n, LPDWORD r, LPDWORD t, LPBYTE d, LPDWORD c)
{
    (void)k; (void)n; (void)r; (void)t; (void)d; (void)c;
    return ERROR_FILE_NOT_FOUND;
}
LONG WINAPI RegCloseKey(HKEY k) { (void)k; return ERROR_SUCCESS; }

/* ---- ole32 -------------------------------------------------------------------------------------- */
HRESULT plat_dsound_create(REFIID iid, LPVOID *out);    /* dx.c */
HRESULT plat_dplay_create(REFCLSID clsid, LPVOID *out);  /* dplay.c */

HRESULT WINAPI CoInitialize(LPVOID p) { (void)p; return S_OK; }
void    WINAPI CoUninitialize(void) {}

HRESULT WINAPI CoCreateInstance(REFCLSID clsid, LPUNKNOWN outer, DWORD ctx, REFIID iid, LPVOID *out)
{
    (void)outer;
    (void)ctx;
    if (out)
        *out = NULL;
    if (!memcmp(clsid, &CLSID_DirectSound, sizeof(GUID)))
        return plat_dsound_create(iid, out);
    if (plat_dplay_create(clsid, out) == 0)
        return 0;
    PLOG("CoCreateInstance(%08x-...): not available\n", clsid->Data1);
    return E_FAIL;
}
