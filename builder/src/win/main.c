/* main.c: the release builder's Windows front end (Win32, common controls 6).
 *
 *   1 the game, and the folder to build it into
 *   2 the player's own dump of it (BIN and cue, or the ROM), checked by MD5
 *   3 the build (core/build.c), with progress
 *   4 the result: the game's folder, or what went wrong
 *
 * The game executables are plain files in the games folder beside it.
 *
 * Arguments fill in the choices for a scripted run, and --build starts it:
 *   --game br|tgr  --dest DIR  --bin FILE  --cue FILE  --rom FILE  --build */
#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <stdio.h>
#include <string.h>
#include "rb.h"

#define WM_RB_PROGRESS (WM_APP + 1)
#define WM_RB_CHECKED  (WM_APP + 2)
#define WM_RB_DONE     (WM_APP + 3)

enum { ID_BR = 100, ID_TGR, ID_DEST, ID_NEXT, ID_BACK, ID_BUILD, ID_CANCEL, ID_SHOW, ID_PLAY, ID_DONE,
       ID_PICK_BIN, ID_PICK_CUE, ID_PICK_ROM };
enum { K_BIN, K_CUE, K_ROM, K_N };

static HWND s_wnd, s_bar, s_status, s_build;
static HWND s_rowtext[K_N];
static HFONT s_font, s_bold, s_title, s_mono;
static HWND s_ctl[64];
static int s_nctl, s_game, s_page;
static wchar_t s_dest[MAX_PATH];
static wchar_t s_file[K_N][MAX_PATH];
static int s_ok[K_N], s_checking;
static volatile int s_cancel;
static wchar_t s_status_text[256];
static double s_fraction;
static char s_err[1024], s_out[2048];
static int s_result;

/* ---- UTF-8 <-> UTF-16 -------------------------------------------------------------------- */
static void to8(const wchar_t *w, char *out, int n) { WideCharToMultiByte(CP_UTF8, 0, w, -1, out, n, NULL, NULL); }
static void to16(const char *s, wchar_t *out, int n) { MultiByteToWideChar(CP_UTF8, 0, s, -1, out, n); }

int rb_host_payload(const char *name, const char *path, char *err, size_t errlen)
{
    wchar_t src[MAX_PATH], dst[2048], *s;
    GetModuleFileNameW(NULL, src, MAX_PATH);
    if ((s = wcsrchr(src, L'\\')) != NULL)
        *s = 0;
    swprintf(src + wcslen(src), MAX_PATH - wcslen(src), L"\\games\\%hs.exe", name);
    to16(path, dst, 2048);
    if (!CopyFileW(src, dst, FALSE)) {
        snprintf(err, errlen, "the builder's games folder is missing %s.exe; unzip the whole download and run the "
                 "builder from that folder", name);
        return 0;
    }
    return 1;
}

/* ---- layout ----------------------------------------------------------------------------------- */
static int dpi(int v) { return MulDiv(v, (int)GetDpiForWindow(s_wnd), 96); }

static void clear_page(void)
{
    int i;
    for (i = 0; i < s_nctl; i++)
        DestroyWindow(s_ctl[i]);
    s_nctl = 0;
    s_bar = s_status = s_build = NULL;
    memset(s_rowtext, 0, sizeof s_rowtext);
}

static HWND add(const wchar_t *cls, const wchar_t *text, DWORD style, int x, int y, int w, int h, int id, HFONT font)
{
    HWND c = CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style, dpi(x), dpi(y), dpi(w), dpi(h), s_wnd,
                             (HMENU)(INT_PTR)id, GetModuleHandleW(NULL), NULL);
    SendMessageW(c, WM_SETFONT, (WPARAM)font, TRUE);
    if (s_nctl < 64)
        s_ctl[s_nctl++] = c;
    return c;
}

static HWND text(const wchar_t *s, int x, int y, int w, int h, HFONT f) { return add(L"STATIC", s, SS_LEFT | SS_NOPREFIX, x, y, w, h, 0, f); }
static HWND button(const wchar_t *s, int x, int y, int w, int id, int def)
{
    return add(L"BUTTON", s, WS_TABSTOP | (def ? BS_DEFPUSHBUTTON : BS_PUSHBUTTON), x, y, w, 28, id, s_font);
}

/* ---- pages --------------------------------------------------------------------------------- */
static void page_choose(void)
{
    HWND r;
    s_page = 1;
    clear_page();
    text(L"Build a game", 24, 20, 540, 32, s_title);
    text(L"Rally Builder makes a native Windows build of a game from your own copy of it. "
         L"Choose the game, and where to put it.", 24, 60, 540, 40, s_font);
    r = add(L"BUTTON", L"Boss Rally (PC, 1999)", WS_TABSTOP | WS_GROUP | BS_AUTORADIOBUTTON, 24, 110, 400, 24, ID_BR, s_font);
    SendMessageW(r, BM_SETCHECK, s_game == RB_BOSS_RALLY, 0);
    r = add(L"BUTTON", L"Top Gear Rally (Nintendo 64, 1997)", WS_TABSTOP | BS_AUTORADIOBUTTON, 24, 138, 400, 24, ID_TGR, s_font);
    SendMessageW(r, BM_SETCHECK, s_game == RB_TOP_GEAR_RALLY, 0);
    text(L"Build into:", 24, 182, 200, 22, s_bold);
    add(L"STATIC", s_dest, SS_LEFT | SS_NOPREFIX | SS_PATHELLIPSIS | SS_SUNKEN, 24, 208, 430, 26, 0, s_mono);
    button(L"Choose...", 464, 207, 100, ID_DEST, 0);
    button(L"Continue", 464, 352, 100, ID_NEXT, 1);
}

static void update_build(void)
{
    int ok = s_game == RB_BOSS_RALLY ? s_ok[K_BIN] && s_ok[K_CUE] : s_ok[K_ROM];
    if (s_build)
        EnableWindow(s_build, ok && !s_checking);
}

static void row(int k, const wchar_t *title, const char *md5, int y, int id)
{
    wchar_t e[128];
    swprintf(e, 128, L"Expected MD5  %hs", md5);
    text(title, 24, y, 300, 22, s_bold);
    button(L"Choose...", 464, y - 3, 100, id, 0);
    text(e, 24, y + 24, 540, 18, s_mono);
    s_rowtext[k] = text(s_file[k][0] ? L"" : L"No file chosen", 24, y + 44, 540, 54, s_mono);
}

static void start_check(int k, const wchar_t *path);

static void page_dumps(void)
{
    s_page = 2;
    clear_page();
    if (s_game == RB_BOSS_RALLY) {
        text(L"Your copy of Boss Rally", 24, 20, 540, 32, s_title);
        text(L"The game's data is copyrighted, so the builder cannot include it. Provide a BIN/CUE image of the "
             L"retail Boss Rally CD (the data track and the 12 music tracks). Choosing either file finds the other beside it.",
             24, 58, 540, 56, s_font);
        row(K_BIN, L"Disc image (.bin)", RB_BR_BIN_MD5, 122, ID_PICK_BIN);
        row(K_CUE, L"Cue sheet (.cue)", RB_BR_CUE_MD5, 228, ID_PICK_CUE);
    } else {
        text(L"Your copy of Top Gear Rally", 24, 20, 540, 32, s_title);
        text(L"The game's data is copyrighted, so the builder cannot include it. Provide a ROM of the "
             L"Top Gear Rally (USA) cartridge, in .z64, .v64 or .n64 byte order.", 24, 58, 540, 56, s_font);
        row(K_ROM, L"Cartridge ROM", RB_TGR_ROM_MD5, 122, ID_PICK_ROM);
    }
    button(L"Back", 352, 352, 100, ID_BACK, 0);
    s_build = button(L"Build", 464, 352, 100, ID_BUILD, 1);
    {
        int k;
        for (k = 0; k < K_N; k++)
            if (s_file[k][0] && s_rowtext[k])
                start_check(k, s_file[k]);
    }
    update_build();
}

static void page_build(void)
{
    s_page = 3;
    clear_page();
    {
        wchar_t t[128];
        swprintf(t, 128, L"Building %hs", rb_game_name(s_game));
        text(t, 24, 20, 540, 32, s_title);
    }
    s_bar = add(PROGRESS_CLASSW, L"", 0, 24, 80, 540, 22, 0, s_font);
    SendMessageW(s_bar, PBM_SETRANGE32, 0, 1000);
    s_status = text(L"Starting", 24, 112, 540, 44, s_font);
    button(L"Cancel", 464, 352, 100, ID_CANCEL, 0);
}

static void page_result(void)
{
    wchar_t t[2048];
    s_page = 4;
    clear_page();
    if (s_result) {
        swprintf(t, 128, L"%hs is ready", rb_game_name(s_game));
        text(t, 24, 20, 540, 32, s_title);
        text(L"The build is complete. It carries everything it needs, so your disc image or ROM is no longer "
             L"required to play.", 24, 60, 540, 40, s_font);
        to16(s_out, t, 2048);
        add(L"STATIC", t, SS_LEFT | SS_NOPREFIX | SS_PATHELLIPSIS, 24, 110, 540, 22, 0, s_mono);
        button(L"Done", 240, 352, 100, ID_DONE, 0);
        button(L"Open Folder", 352, 352, 100, ID_SHOW, 0);
        button(L"Play", 464, 352, 100, ID_PLAY, 1);
    } else {
        text(L"The build did not finish", 24, 20, 540, 32, s_title);
        to16(s_err[0] ? s_err : "The build failed.", t, 2048);
        text(t, 24, 64, 540, 120, s_font);
        button(L"Quit", 352, 352, 100, ID_DONE, 0);
        button(L"Back", 464, 352, 100, ID_BACK, 1);
    }
}

/* ---- checks ------------------------------------------------------------------------------ */
typedef struct check_job { int k; wchar_t path[MAX_PATH]; rb_check c; } check_job;

static DWORD WINAPI check_thread(LPVOID p)
{
    check_job *j = (check_job *)p;
    char path[MAX_PATH * 3];
    to8(j->path, path, sizeof path);
    if (j->k == K_BIN)
        rb_check_bin(path, &j->c, NULL, NULL, NULL);
    else if (j->k == K_CUE)
        rb_check_cue(path, &j->c);
    else
        rb_check_rom(path, &j->c, NULL, NULL, NULL);
    PostMessageW(s_wnd, WM_RB_CHECKED, 0, (LPARAM)j);
    return 0;
}

static void start_check(int k, const wchar_t *path)
{
    check_job *j = calloc(1, sizeof *j);
    HANDLE h;
    j->k = k;
    wcsncpy(j->path, path, MAX_PATH - 1);
    wcsncpy(s_file[k], path, MAX_PATH - 1);
    s_ok[k] = 0;
    if (s_rowtext[k]) {
        wchar_t t[MAX_PATH + 32];
        swprintf(t, MAX_PATH + 32, L"%ls: checking...", wcsrchr(path, L'\\') ? wcsrchr(path, L'\\') + 1 : path);
        SetWindowTextW(s_rowtext[k], t);
    }
    s_checking++;
    update_build();
    h = CreateThread(NULL, 0, check_thread, j, 0, NULL);
    if (h)
        CloseHandle(h);
}

static void checked(check_job *j)
{
    s_checking--;
    if (!wcscmp(j->path, s_file[j->k])) {
        wchar_t t[1024];
        const wchar_t *name = wcsrchr(j->path, L'\\') ? wcsrchr(j->path, L'\\') + 1 : j->path;
        s_ok[j->k] = j->c.ok;
        swprintf(t, 1024, L"%ls\r\nYour file's MD5  %hs  %ls%ls%hs", name, j->c.md5,
                 j->c.ok ? L"\x2713 matches" : L"\x2717 does not match", j->c.note[0] ? L"\r\n" : L"", j->c.note);
        if (s_rowtext[j->k]) {
            SetWindowTextW(s_rowtext[j->k], t);
            InvalidateRect(s_rowtext[j->k], NULL, TRUE);
        }
    }
    free(j);
    update_build();
}

/* ---- pickers ---------------------------------------------------------------------------------- */
static int pick(int folder, wchar_t *out, const wchar_t *filter_name, const wchar_t *filter)
{
    IFileOpenDialog *d;
    IShellItem *it;
    PWSTR p;
    int ok = 0;
    if (FAILED(CoCreateInstance(&CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, &IID_IFileOpenDialog, (void **)&d)))
        return 0;
    if (folder) {
        DWORD o;
        IFileOpenDialog_GetOptions(d, &o);
        IFileOpenDialog_SetOptions(d, o | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
    } else {
        COMDLG_FILTERSPEC f[2] = { { filter_name, filter }, { L"All files", L"*.*" } };
        IFileOpenDialog_SetFileTypes(d, 2, f);
    }
    if (SUCCEEDED(IFileOpenDialog_Show(d, s_wnd)) && SUCCEEDED(IFileOpenDialog_GetResult(d, &it))) {
        if (SUCCEEDED(IShellItem_GetDisplayName(it, SIGDN_FILESYSPATH, &p))) {
            wcsncpy(out, p, MAX_PATH - 1);
            out[MAX_PATH - 1] = 0;
            CoTaskMemFree(p);
            ok = 1;
        }
        IShellItem_Release(it);
    }
    IFileOpenDialog_Release(d);
    return ok;
}

static void picked_pair(int k, const wchar_t *path)
{
    wchar_t other[MAX_PATH];
    const wchar_t *ext = wcsrchr(path, L'.');
    if (ext && !_wcsicmp(ext, L".cue")) {
        char p8[MAX_PATH * 3], b8[MAX_PATH * 3];
        start_check(K_CUE, path);
        to8(path, p8, sizeof p8);
        if (rb_cue_bin_path(p8, b8, sizeof b8)) {
            to16(b8, other, MAX_PATH);
            start_check(K_BIN, other);
        }
    } else if (k == K_CUE) {
        start_check(K_CUE, path);
    } else {
        start_check(K_BIN, path);
        wcsncpy(other, path, MAX_PATH - 5);
        if (wcsrchr(other, L'.'))
            *wcsrchr(other, L'.') = 0;
        wcscat(other, L".cue");
        if (GetFileAttributesW(other) != INVALID_FILE_ATTRIBUTES)
            start_check(K_CUE, other);
    }
}

/* ---- the build ------------------------------------------------------------------------------- */
static void progress(void *ctx, double f, const char *s)
{
    (void)ctx;
    s_fraction = f;
    if (s)
        to16(s, s_status_text, 256);
    PostMessageW(s_wnd, WM_RB_PROGRESS, 0, 0);
}

static DWORD WINAPI build_thread(LPVOID p)
{
    rb_job job = { 0 };
    char dest[MAX_PATH * 3], bin[MAX_PATH * 3], cue[MAX_PATH * 3], rom[MAX_PATH * 3];
    (void)p;
    to8(s_dest, dest, sizeof dest);
    to8(s_file[K_BIN], bin, sizeof bin);
    to8(s_file[K_CUE], cue, sizeof cue);
    to8(s_file[K_ROM], rom, sizeof rom);
    job.game = s_game;
    job.dest_dir = dest;
    job.bin = bin;
    job.cue = cue;
    job.rom = rom;
    s_err[0] = 0;
    s_result = rb_build(&job, progress, NULL, &s_cancel, s_err, sizeof s_err);
    rb_output_path(&job, s_out, sizeof s_out);
    PostMessageW(s_wnd, WM_RB_DONE, 0, 0);
    return 0;
}

static void build(void)
{
    rb_job job = { 0 };
    char dest[MAX_PATH * 3], out[2048];
    wchar_t msg[4096], o16[2048];
    int ours = 0;
    HANDLE h;
    to8(s_dest, dest, sizeof dest);
    job.game = s_game;
    job.dest_dir = dest;
    if (rb_output_exists(&job, &ours)) {
        rb_output_path(&job, out, sizeof out);
        to16(out, o16, 2048);
        if (!ours) {
            swprintf(msg, 4096, L"%ls already exists and was not made by Rally Builder, so it will not be replaced. "
                                L"Move it away or choose another folder.", o16);
            MessageBoxW(s_wnd, msg, L"Rally Builder", MB_ICONWARNING | MB_OK);
            return;
        }
        swprintf(msg, 4096, L"An earlier build is at %ls.\r\n\r\nReplace it? Your saved games are kept; they live in "
                            L"your AppData folder, not in the game's.", o16);
        if (MessageBoxW(s_wnd, msg, L"Rally Builder", MB_ICONQUESTION | MB_YESNO) != IDYES)
            return;
    }
    s_cancel = 0;
    page_build();
    h = CreateThread(NULL, 0, build_thread, NULL, 0, NULL);
    if (h)
        CloseHandle(h);
}

/* ---- the window ------------------------------------------------------------------------------ */
static LRESULT CALLBACK proc(HWND w, UINT m, WPARAM wp, LPARAM lp)
{
    switch (m) {
    case WM_CTLCOLORSTATIC: {
        HDC dc = (HDC)wp;
        HWND c = (HWND)lp;
        int k;
        SetBkMode(dc, TRANSPARENT);
        for (k = 0; k < K_N; k++)
            if (c == s_rowtext[k] && s_file[k][0] && !s_checking)
                SetTextColor(dc, s_ok[k] ? RGB(0, 128, 0) : RGB(192, 0, 0));
        return (LRESULT)GetSysColorBrush(COLOR_WINDOW);
    }
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case ID_BR: s_game = RB_BOSS_RALLY; break;
        case ID_TGR: s_game = RB_TOP_GEAR_RALLY; break;
        case ID_DEST:
            if (pick(1, s_dest, NULL, NULL))
                page_choose();
            break;
        case ID_NEXT: page_dumps(); break;
        case ID_BACK: if (s_page == 4) page_dumps(); else page_choose(); break;
        case ID_PICK_BIN: case ID_PICK_CUE: {
            wchar_t p[MAX_PATH];
            int k = LOWORD(wp) == ID_PICK_BIN ? K_BIN : K_CUE;
            if (pick(0, p, k == K_BIN ? L"Disc image (*.bin)" : L"Cue sheet (*.cue)", k == K_BIN ? L"*.bin" : L"*.cue"))
                picked_pair(k, p);
            break;
        }
        case ID_PICK_ROM: {
            wchar_t p[MAX_PATH];
            if (pick(0, p, L"N64 ROM (*.z64;*.v64;*.n64)", L"*.z64;*.v64;*.n64"))
                start_check(K_ROM, p);
            break;
        }
        case ID_BUILD: build(); break;
        case ID_CANCEL:
            s_cancel = 1;
            EnableWindow((HWND)lp, FALSE);
            if (s_status)
                SetWindowTextW(s_status, L"Stopping...");
            break;
        case ID_SHOW: case ID_PLAY: {
            wchar_t o[2048], exe[2200];
            to16(s_out, o, 2048);
            if (LOWORD(wp) == ID_SHOW) {
                ShellExecuteW(w, L"open", o, NULL, NULL, SW_SHOWNORMAL);
            } else {
                wchar_t name[128];
                to16(rb_game_name(s_game), name, 128);
                swprintf(exe, 2200, L"%ls\\%ls.exe", o, name);
                ShellExecuteW(w, L"open", exe, NULL, o, SW_SHOWNORMAL);
            }
            break;
        }
        case ID_DONE: DestroyWindow(w); break;
        }
        return 0;
    case WM_RB_CHECKED:
        checked((check_job *)lp);
        return 0;
    case WM_RB_PROGRESS:
        if (s_bar) {
            SendMessageW(s_bar, PBM_SETPOS, (WPARAM)(s_fraction * 1000), 0);
            if (!s_cancel)
                SetWindowTextW(s_status, s_status_text);
        }
        return 0;
    case WM_RB_DONE:
        page_result();
        return 0;
    case WM_CLOSE:
        if (s_page == 3) {                              /* stop the build first; it cleans up after itself */
            s_cancel = 1;
            return 0;
        }
        DestroyWindow(w);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(w, m, wp, lp);
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE prev, PWSTR cmd, int show)
{
    WNDCLASSW wc = { 0 };
    NONCLIENTMETRICSW ncm = { sizeof ncm };
    INITCOMMONCONTROLSEX icc = { sizeof icc, ICC_PROGRESS_CLASS | ICC_STANDARD_CLASSES };
    LOGFONTW lf;
    RECT r = { 0, 0, 588, 400 };
    MSG msg;
    PWSTR docs;
    UINT d;
    (void)prev, (void)cmd;
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    InitCommonControlsEx(&icc);
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (SUCCEEDED(SHGetKnownFolderPath(&FOLDERID_Documents, 0, NULL, &docs))) {
        swprintf(s_dest, MAX_PATH, L"%ls\\Games", docs);
        CoTaskMemFree(docs);
    }
    wc.lpfnWndProc = proc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
    wc.hIcon = LoadIconW(inst, L"APPICON");
    wc.lpszClassName = L"RallyBuilder";
    RegisterClassW(&wc);
    s_wnd = CreateWindowExW(0, L"RallyBuilder", L"Rally Builder", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                            CW_USEDEFAULT, CW_USEDEFAULT, 100, 100, NULL, NULL, inst, NULL);
    d = GetDpiForWindow(s_wnd);
    r.right = MulDiv(r.right, (int)d, 96);
    r.bottom = MulDiv(r.bottom, (int)d, 96);
    AdjustWindowRectExForDpi(&r, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE, 0, d);
    SetWindowPos(s_wnd, NULL, 0, 0, r.right - r.left, r.bottom - r.top, SWP_NOMOVE | SWP_NOZORDER);
    SystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS, sizeof ncm, &ncm, 0, d);
    s_font = CreateFontIndirectW(&ncm.lfMessageFont);
    lf = ncm.lfMessageFont;
    lf.lfWeight = FW_SEMIBOLD;
    s_bold = CreateFontIndirectW(&lf);
    lf.lfHeight = MulDiv(lf.lfHeight, 3, 2);
    s_title = CreateFontIndirectW(&lf);
    lf = ncm.lfMessageFont;
    wcscpy(lf.lfFaceName, L"Consolas");
    s_mono = CreateFontIndirectW(&lf);
    {
        int argc = 0, i, go = 0;
        LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        for (i = 1; argv && i < argc; i++) {
            const wchar_t *k = argv[i], *v = i + 1 < argc ? argv[i + 1] : L"";
            if (!wcscmp(k, L"--build")) { go = 1; continue; }
            if (!wcscmp(k, L"--game")) s_game = wcscmp(v, L"tgr") ? RB_BOSS_RALLY : RB_TOP_GEAR_RALLY;
            else if (!wcscmp(k, L"--dest")) wcsncpy(s_dest, v, MAX_PATH - 1);
            else if (!wcscmp(k, L"--bin")) { wcsncpy(s_file[K_BIN], v, MAX_PATH - 1); s_ok[K_BIN] = 1; }
            else if (!wcscmp(k, L"--cue")) { wcsncpy(s_file[K_CUE], v, MAX_PATH - 1); s_ok[K_CUE] = 1; }
            else if (!wcscmp(k, L"--rom")) { wcsncpy(s_file[K_ROM], v, MAX_PATH - 1); s_ok[K_ROM] = 1; }
            else continue;
            i++;
        }
        LocalFree(argv);
        page_choose();
        ShowWindow(s_wnd, show);
        if (go)
            build();                   /* a scripted run: the dumps were checked by whoever named them */
    }
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        if (!IsDialogMessageW(s_wnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    return 0;
}
