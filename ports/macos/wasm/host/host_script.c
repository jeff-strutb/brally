/* host_script.c -- drive the Mac port from a brbox input script (port code).
 *
 * BR_SCRIPT=<file> replays a tools/brbox_scripts/*.txt timeline against the
 * port, with brbox_drive.py's semantics, so the port can be taken through
 * the same scenarios the original is certified under and the two compared
 * frame by frame.  A "frame" is one entry to BrAppFrame (0x1001CF80); w2c.py
 * emits a call to happ_frame() at the top of that function.
 *
 * Supported: sleep N, press KEY [N], hold KEY, release KEY, mouse X Y,
 * point X Y (absolute, the windowed pointer path), quit (as Cmd-Q),
 * click [N], wait/waitb ADDR OP VALUE [N], mark NAME, shot NAME, end,
 * window W H (resize the content to W x H points), fullscreen (toggle),
 * chord KEYS (a Command/Control chord such as ctrl+cmd+f, as a real key event),
 * version (Tab, the PC / N64 switch, as a real key event),
 * keycode MACVK [N] (a real key event by macOS virtual key code, held N frames),
 * autopilot on|off (brbox_drive.py's racing-line steering).
 * waittext TEXT [N] (until a string drawn last frame contains TEXT, `_` for
 * space), text (log what the last frame drew), files NAME (the
 * tools/brbox_saves/NAME fixture copied into the save directory before
 * boot). savefiles, joystick and tmu are accepted and do nothing here; peer
 * (a second machine) is not supported -- a script using it stops.
 *
 * BR_SHOTS=<dir> is where `shot NAME` writes NAME.ppm (default build/wasm/shots).
 * Progress goes to stderr as "script: frame F: <line>".
 */
#include "host.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/stat.h>
#include <dirent.h>
#include <ctype.h>

typedef struct { const char *name; u8 vk, dik; } key_t_;
static const key_t_ KEYS[] = {
    { "RETURN", 0x0D, 0x1C }, { "ENTER", 0x0D, 0x1C }, { "SPACE", 0x20, 0x39 },
    { "ESCAPE", 0x1B, 0x01 }, { "ESC", 0x1B, 0x01 }, { "TAB", 0x09, 0x0F },
    { "BACK", 0x08, 0x0E }, { "UP", 0x26, 0xC8 }, { "DOWN", 0x28, 0xD0 },
    { "LEFT", 0x25, 0xCB }, { "RIGHT", 0x27, 0xCD }, { "LSHIFT", 0x10, 0x2A },
    { "LCTRL", 0x11, 0x1D }, { "LALT", 0x12, 0x38 },
    { "F1", 0x70, 0x3B }, { "F2", 0x71, 0x3C }, { "F3", 0x72, 0x3D }, { "F4", 0x73, 0x3E },
    { "F5", 0x74, 0x3F }, { "F6", 0x75, 0x40 }, { "F7", 0x76, 0x41 }, { "F8", 0x77, 0x42 },
    { "F9", 0x78, 0x43 }, { "F10", 0x79, 0x44 },
    { "END", 0x23, 0xCF }, { "PGDN", 0x22, 0xD1 }, { "INSERT", 0x2D, 0xD2 },
    { "HOME", 0x24, 0xC7 }, { "PGUP", 0x21, 0xC9 }, { "DELETE", 0x2E, 0xD3 },
    { "RSHIFT", 0xA1, 0x36 }, { "RCTRL", 0xA3, 0x9D },
};

static int key_lookup(const char *n, u8 *vk, u8 *dik)
{
    static const char *rows[] = { "QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM" };
    static const u8 base[] = { 0x10, 0x1E, 0x2C };
    size_t i;
    for (i = 0; i < sizeof KEYS / sizeof KEYS[0]; i++)
        if (!strcasecmp(n, KEYS[i].name)) { *vk = KEYS[i].vk; *dik = KEYS[i].dik; return 1; }
    if (n[0] && !n[1]) {
        char c = (char)(n[0] >= 'a' && n[0] <= 'z' ? n[0] - 32 : n[0]);
        for (i = 0; i < 3; i++) {
            const char *p = strchr(rows[i], c);
            if (p) { *vk = (u8)c; *dik = (u8)(base[i] + (p - rows[i])); return 1; }
        }
        if (c >= '1' && c <= '9') { *vk = (u8)c; *dik = (u8)(0x02 + c - '1'); return 1; }
        if (c == '0') { *vk = '0'; *dik = 0x0B; return 1; }
    }
    return 0;
}

#define MAXSTEP 4096
typedef struct { char op[16]; char a[4][64]; int na, line; } step_t;
static step_t g_steps[MAXSTEP];
static int g_nsteps, g_pc, g_loaded;
static unsigned g_frame, g_sleep_until, g_wait_since;
static int g_mouse_phase, g_waiting;
static struct { unsigned frame; u8 vk, dik; int mouse; } g_rel[64];
static int g_nrel;
static char g_shots[1024];

static void load(void)
{
    const char *p = getenv("BR_SCRIPT");
    FILE *f;
    char ln[512];
    int line = 0;
    g_loaded = 1;
    if (!p) return;
    if (!(f = fopen(p, "r"))) { fprintf(stderr, "script: cannot open %s\n", p); exit(2); }
    snprintf(g_shots, sizeof g_shots, "%s", getenv("BR_SHOTS") ? getenv("BR_SHOTS") : "build/wasm/shots");
    mkdir(g_shots, 0755);
    while (fgets(ln, sizeof ln, f) && g_nsteps < MAXSTEP) {
        char *h = strchr(ln, '#');
        step_t *s = &g_steps[g_nsteps];
        line++;
        if (h) *h = 0;
        memset(s, 0, sizeof *s);
        s->na = sscanf(ln, "%15s %63s %63s %63s %63s", s->op, s->a[0], s->a[1], s->a[2], s->a[3]) - 1;
        if (s->na < 0) continue;
        s->line = line;
        g_nsteps++;
    }
    fclose(f);
    fprintf(stderr, "script: %s, %d steps\n", p, g_nsteps);
}

static void key(u8 vk, u8 dik, int down) { happ_key_script(dik, vk, down); }

/* autopilot on|off: brbox_drive.py's steer(), the same walk down the
 * player car's racing-line cursor (car+0xF8C node, +0xF90 point), holding
 * UP and LEFT/RIGHT toward a point ~12 m + 0.6 s ahead; car+0x10 points to
 * the car's LEFT, so a target on its positive side takes the LEFT arrow. */
#define AP_ENTRANTS 0x10AF0858u
enum { AP_UP, AP_DOWN, AP_LEFT, AP_RIGHT };
static const u8 AP_VK[4] = { 0x26, 0x28, 0x25, 0x27 }, AP_DIK[4] = { 0xC8, 0xD0, 0xCB, 0xCD };
static int g_ap, g_ap_down[4], g_ap_stuck, g_ap_reverse;

static float ap_f(u32 a) { float f; memcpy(&f, W_P(a), 4); return f; }

static void ap_keys(const int *want)
{
    int k;
    for (k = 0; k < 4; k++)
        if (want[k] != g_ap_down[k]) { key(AP_VK[k], AP_DIK[k], want[k]); g_ap_down[k] = want[k]; }
}

static void steer(void)
{
    u32 car = W_LD(u32, AP_ENTRANTS, 0);
    int want[4] = { 1, 0, 0, 0 };
    if (car) {
        u32 node = W_LD(u32, car + 0xF8C, 0), n = node, nxt;
        int k = (int)W_LD(u32, car + 0xF90, 0), cnt, i, guard;
        float px = ap_f(car + 0x30), py = ap_f(car + 0x34), rx = ap_f(car + 0x10), ry = ap_f(car + 0x14);
        float speed = hypotf(ap_f(car + 0x1024), ap_f(car + 0x1028)), t, dx, dy, d, lat;
        if (node) {
            t = 12.0f + speed * 0.6f;
            for (i = 0; i < 400; i++) {
                cnt = W_LD(u16, n + 0x14, 0);
                t -= ap_f(n + 0x40 + 0x28 * (u32)k + 0x24) - ap_f(n + 0x40 + 0x28 * (u32)(k + 1) + 0x24);
                if (++k >= cnt) {
                    /* the Mine layouts end the node list without looping back */
                    nxt = W_LD(u32, n, 0);
                    for (guard = 0; nxt && (W_LD(u16, nxt + 0x16, 0) & 1) && guard < 16; guard++)
                        nxt = W_LD(u32, nxt + 4, 0);
                    if (!nxt) { k = cnt - 1; break; }
                    n = nxt; k = 0;
                }
                if (t < 0) break;
            }
            dx = ap_f(n + 0x40 + 0x28 * (u32)k + 0x0C) - px;
            dy = ap_f(n + 0x40 + 0x28 * (u32)k + 0x10) - py;
            d = hypotf(dx, dy); if (d == 0) d = 1;
            lat = (rx * dx + ry * dy) / d;
            if (lat > 0.08f) want[AP_LEFT] = 1;
            else if (lat < -0.08f) want[AP_RIGHT] = 1;
            if (fabsf(lat) > 0.6f && speed > 25) want[AP_UP] = 0;
            /* stuck against something: back off with the wheel reversed */
            if (speed < 2.0f) g_ap_stuck++; else g_ap_stuck = 0;
            if (g_ap_stuck > 45 || g_ap_reverse > 0) {
                int l = want[AP_LEFT];
                if (g_ap_reverse == 0) g_ap_reverse = 40;
                g_ap_reverse--; g_ap_stuck = 0;
                want[AP_UP] = 0; want[AP_DOWN] = 1;
                want[AP_LEFT] = want[AP_RIGHT]; want[AP_RIGHT] = l;
            }
        }
    }
    ap_keys(want);
}

/* native/window.m; nothing to resize headless */
__attribute__((weak)) void hwindow_resize(int w, int h) { (void)w; (void)h; }
__attribute__((weak)) void hwindow_fullscreen(void) {}
__attribute__((weak)) void hwindow_chord(const char *spec) { (void)spec; }
__attribute__((weak)) void hwindow_keycode(int code, int down) { (void)code; (void)down; }

static int test(const step_t *s)
{
    u32 a = (u32)strtoul(s->a[0], 0, 0), v = (u32)strtoul(s->a[2], 0, 0);
    u32 x = !strcmp(s->op, "waitb") ? W_LD(u8, a, 0) : W_LD(u32, a, 0);
    const char *o = s->a[1];
    return !strcmp(o, "==") ? x == v : !strcmp(o, "!=") ? x != v :
           !strcmp(o, ">") ? x > v : !strcmp(o, ">=") ? x >= v :
           !strcmp(o, "<") ? x < v : !strcmp(o, "<=") ? x <= v : 0;
}

/* BR_TRACE_FRAMES=A:B -- function-entry tracing (BR_TRACE's output) for
 * main-loop frames A..B only, counted from boot whether or not a script runs. */
extern int w_tracing;
static void trace_window(void)
{
    static int init, a = -1, b = -1;
    static unsigned n;
    n++;
    if (!init) {
        const char *e = getenv("BR_TRACE_FRAMES");
        init = 1;
        if (e) sscanf(e, "%d:%d", &a, &b);
    }
    if (a >= 0) {
        w_tracing = (int)n >= a && (int)n <= b;
        if ((int)n == a || (int)n == b + 1) fprintf(stderr, "== frame %u\n", n);
    }
}

/* BR_DUMP=F:PATH -- at main-loop frame F, write the image's data area
 * (0x10077000..0x118F0000, the addresses brbox's run shares) to PATH. */
static void dump_window(void)
{
    static int init, at = -1;
    static char path[1024];
    static unsigned n;
    n++;
    if (!init) {
        const char *e = getenv("BR_DUMP");
        init = 1;
        if (e) sscanf(e, "%d:%1023s", &at, path);
    }
    if (at >= 0 && (int)n == at) {
        FILE *f = fopen(path, "wb");
        if (f) { fwrite(W_P(0x10077000u), 1, 0x118F0000u - 0x10077000u, f); fclose(f); }
        fprintf(stderr, "dump: frame %u -> %s\n", n, path);
    }
}

/* what the engine drew: BrTextEmitString (0x10015B10) calls htext_emit; a
 * buffer swap closes the frame (brbox_drive.py's text_now / text_last) */
#define TEXT_MAX 256
static char g_text_now[TEXT_MAX][96], g_text_last[TEXT_MAX][96];
static int g_ntext_now, g_ntext_last;

void htext_emit(u32 psz)
{
    if (psz && g_ntext_now < TEXT_MAX) {
        const char *p = (const char *)W_P(psz);
        snprintf(g_text_now[g_ntext_now++], 96, "%s", p);
    }
}

void htext_swap(void)
{
    memcpy(g_text_last, g_text_now, sizeof g_text_now);
    g_ntext_last = g_ntext_now;
    g_ntext_now = 0;
}

static int text_seen(const char *want)
{
    char w[96], t[96];
    int i, k;
    snprintf(w, sizeof w, "%s", want);
    for (k = 0; w[k]; k++) w[k] = w[k] == '_' ? ' ' : (char)tolower((u8)w[k]);
    for (i = 0; i < g_ntext_last; i++) {
        for (k = 0; g_text_last[i][k] && k < 95; k++) t[k] = (char)tolower((u8)g_text_last[i][k]);
        t[k] = 0;
        if (strstr(t, w)) return 1;
    }
    return 0;
}

/* `files NAME`: copy tools/brbox_saves/NAME/c/<path> into the save
 * directory under the flattened name the file layer looks for (vfs_resolve:
 * the path below the drive, backslashes as `_`, lower case). */
static void copy_tree(const char *dir, const char *rel, const char *save)
{
    DIR *d = opendir(dir);
    struct dirent *e;
    if (!d) return;
    while ((e = readdir(d))) {
        char p[1200], r[600], q[1300];
        struct stat st;
        if (e->d_name[0] == '.') continue;
        snprintf(p, sizeof p, "%s/%s", dir, e->d_name);
        snprintf(r, sizeof r, "%s%s%s", rel, *rel ? "_" : "", e->d_name);
        if (stat(p, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) { copy_tree(p, r, save); continue; }
        {
            char *k;
            FILE *in = fopen(p, "rb"), *out;
            for (k = r; *k; k++) *k = (char)tolower((u8)*k);
            snprintf(q, sizeof q, "%s/%s", save, r);
            out = fopen(q, "wb");
            if (in && out) {
                char buf[65536];
                size_t n;
                while ((n = fread(buf, 1, sizeof buf, in)) > 0) fwrite(buf, 1, n, out);
                fprintf(stderr, "script: fixture %s -> %s\n", p, q);
            }
            if (in) fclose(in);
            if (out) fclose(out);
        }
    }
    closedir(d);
}

void hscript_files(const char *save)
{
    const char *path = getenv("BR_SCRIPT"), *root = getenv("BR_ROOT");
    FILE *f;
    char line[512];
    if (!path || !(f = fopen(path, "r"))) return;
    while (fgets(line, sizeof line, f)) {
        char op[32], name[256];
        if (sscanf(line, "%31s %255s", op, name) == 2 && !strcmp(op, "files")) {
            char dir[1200];
            /* the fixture's c/bossrally is the game directory, which the
             * file layer keeps at the top of the save directory */
            snprintf(dir, sizeof dir, "%s/tools/brbox_saves/%s/c/bossrally", root ? root : ".", name);
            copy_tree(dir, "", save);
        }
    }
    fclose(f);
}

/* BR_SIMLOG=1: every 10th frame, what the race simulation holds for each
 * driver (g_aBrRaceDriver 0x10AF07F8, stride 0x80): position, running lap
 * time, lap and gate -- two runs of one script under BR_VCLOCK compared line
 * by line show whether anything drawn differently reached the game */
static void simlog(void)
{
    static int on = -1;
    int d;
    if (on < 0) on = getenv("BR_SIMLOG") != NULL;
    if (!on || g_frame % 10) return;
    fprintf(stderr, "sim %u", g_frame);
    for (d = 0; d < 8; d++) {
        u32 a = 0x10AF07F8u + (u32)d * 0x80u;
        if (!W_LD(u32, a, 0x60)) continue;
        fprintf(stderr, " | %d %.4f %.4f %.4f %.4f %d %d", d, W_LD(f32, a, 0x00), W_LD(f32, a, 0x04), W_LD(f32, a, 0x08),
                W_LD(f32, a, 0x30), (int)W_LD(u32, a, 0x40), (int)W_LD(u32, a, 0x4C));
    }
    fprintf(stderr, "\n");
}

void happ_frame(void)
{
    int i;
    simlog();
    trace_window();
    dump_window();
    if (!g_loaded) load();
    if (!g_nsteps) return;
    g_frame++;
    for (i = 0; i < g_nrel; ) {
        if (g_rel[i].frame <= g_frame) {
            if (g_rel[i].mouse == 2) hwindow_keycode(g_rel[i].dik, 0);
            else if (g_rel[i].mouse) hdx_mouse(0, 0, 0);
            else key(g_rel[i].vk, g_rel[i].dik, 0);
            g_rel[i] = g_rel[--g_nrel];
        } else i++;
    }
    if (g_ap) steer();
    while (g_pc < g_nsteps && g_frame >= g_sleep_until) {
        const step_t *s = &g_steps[g_pc];
        u8 vk, dik;
        if (!g_waiting)
            fprintf(stderr, "script: frame %u: %s %s %s %s\n", g_frame, s->op, s->a[0], s->a[1], s->a[2]);
        if (!strcmp(s->op, "sleep")) {
            g_sleep_until = g_frame + (unsigned)atoi(s->a[0]);
        } else if (!strcmp(s->op, "press") || !strcmp(s->op, "hold") || !strcmp(s->op, "release")) {
            if (!key_lookup(s->a[0], &vk, &dik)) {
                fprintf(stderr, "script line %d: unknown key %s\n", s->line, s->a[0]);
                exit(2);
            }
            if (!strcmp(s->op, "release")) key(vk, dik, 0);
            else key(vk, dik, 1);
            if (!strcmp(s->op, "press") && g_nrel < 64) {
                unsigned n = s->na > 1 ? (unsigned)atoi(s->a[1]) : 2;
                g_rel[g_nrel].frame = g_frame + n; g_rel[g_nrel].vk = vk;
                g_rel[g_nrel].dik = dik; g_rel[g_nrel].mouse = 0; g_nrel++;
                g_sleep_until = g_frame + n + 1;
            }
        } else if (!strcmp(s->op, "mouse")) {
            if (g_mouse_phase == 0) {            /* slam into the top-left corner */
                hdx_mouse(-100000, -100000, 0);
                g_mouse_phase = 1;
                return;
            }
            hdx_mouse(atoi(s->a[0]), atoi(s->a[1]), 0);
            g_mouse_phase = 0;
            g_sleep_until = g_frame + 2;
        } else if (!strcmp(s->op, "quit")) {
            g_happ_quit = 1;                   /* what Cmd-Q and the close box do */
        } else if (!strcmp(s->op, "window")) {
            hwindow_resize(atoi(s->a[0]), atoi(s->a[1]));   /* content size, points */
        } else if (!strcmp(s->op, "fullscreen")) {
            hwindow_fullscreen();              /* toggles, as the green button does */
        } else if (!strcmp(s->op, "chord")) {
            hwindow_chord(s->a[0]);            /* e.g. ctrl+cmd+f, through the menu bar */
        } else if (!strcmp(s->op, "keycode")) {
            /* keycode MACVK [N]: a real key event, held N frames (default 2) */
            unsigned n = s->na > 1 ? (unsigned)atoi(s->a[1]) : 2;
            hwindow_keycode((int)strtol(s->a[0], 0, 0), 1);
            if (g_nrel < 64) {
                g_rel[g_nrel].frame = g_frame + n; g_rel[g_nrel].dik = (u8)strtol(s->a[0], 0, 0);
                g_rel[g_nrel].mouse = 2; g_nrel++;
            }
            g_sleep_until = g_frame + n + 1;
        } else if (!strcmp(s->op, "version")) {
            hversion_script();                 /* Tab: the PC / N64 switch */
        } else if (!strcmp(s->op, "point")) {
            /* the windowed pointer path: an absolute 640x480 position */
            hdx_mouse_abs(atoi(s->a[0]), atoi(s->a[1]), 0);
            g_sleep_until = g_frame + 2;
        } else if (!strcmp(s->op, "click")) {
            unsigned n = s->na > 0 ? (unsigned)atoi(s->a[0]) : 2;
            hdx_mouse(0, 0, 1);
            if (g_nrel < 64) { g_rel[g_nrel].frame = g_frame + n; g_rel[g_nrel].mouse = 1; g_nrel++; }
            g_sleep_until = g_frame + n + 1;
        } else if (!strcmp(s->op, "wait") || !strcmp(s->op, "waitb")) {
            if (!test(s)) {
                unsigned lim = s->na > 3 ? (unsigned)atoi(s->a[3]) : 1800;
                if (!g_waiting) { g_waiting = 1; g_wait_since = g_frame; }
                if (g_frame - g_wait_since > lim) {
                    fprintf(stderr, "script line %d timed out\n", s->line);
                    exit(3);
                }
                return;
            }
            g_waiting = 0;
        } else if (!strcmp(s->op, "autopilot")) {
            static const int none[4];
            g_ap = !strcmp(s->a[0], "on");
            if (!g_ap) ap_keys(none);
        } else if (!strcmp(s->op, "mark")) {
            /* a checkpoint: already logged above */
        } else if (!strcmp(s->op, "shot")) {
            char p[1200];
            snprintf(p, sizeof p, "%s/%s.ppm", g_shots, s->a[0]);
            hglide_shot(p);
        } else if (!strcmp(s->op, "waittext")) {
            if (!text_seen(s->a[0])) {
                unsigned lim = s->na > 1 ? (unsigned)atoi(s->a[1]) : 1800;
                if (!g_waiting) { g_waiting = 1; g_wait_since = g_frame; }
                if (g_frame - g_wait_since > lim) {
                    fprintf(stderr, "script line %d: text '%s' never drawn\n", s->line, s->a[0]);
                    exit(3);
                }
                return;
            }
            g_waiting = 0;
        } else if (!strcmp(s->op, "text")) {
            int i;
            fprintf(stderr, "script: frame %u text:", g_frame);
            for (i = 0; i < g_ntext_last; i++) fprintf(stderr, " %s |", g_text_last[i]);
            fprintf(stderr, "\n");
        } else if (!strcmp(s->op, "files") || !strcmp(s->op, "savefiles") ||
                   !strcmp(s->op, "joystick") || !strcmp(s->op, "tmu")) {
            /* setup only (files: applied before boot) */
        } else if (!strcmp(s->op, "end")) {
            fprintf(stderr, "script: end at frame %u\n", g_frame);
            exit(0);
        } else {
            fprintf(stderr, "script line %d: '%s' is not supported by the Mac host yet\n", s->line, s->op);
            exit(2);
        }
        g_pc++;
    }
}
