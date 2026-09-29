/* host_script.c -- drive the Mac port from a brbox input script (port code).
 *
 * BR_SCRIPT=<file> replays a tools/brbox_scripts/*.txt timeline against the
 * port, with brbox_drive.py's semantics, so the port can be taken through
 * the same scenarios the original is certified under and the two compared
 * frame by frame.  A "frame" is one entry to BrAppFrame (0x1001CF80); w2c.py
 * emits a call to happ_frame() at the top of that function.
 *
 * Supported: sleep N, press KEY [N], hold KEY, release KEY, mouse X Y,
 * point X Y (absolute, the windowed pointer path),
 * click [N], wait/waitb ADDR OP VALUE [N], mark NAME, shot NAME, end.
 * Not yet: autopilot, waittext, text, peer, files, savefiles, tmu,
 * joystick -- a script using one stops with a message naming it.
 *
 * BR_SHOTS=<dir> is where `shot NAME` writes NAME.ppm (default build/wasm/shots).
 * Progress goes to stderr as "script: frame F: <line>".
 */
#include "host.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

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

void happ_frame(void)
{
    int i;
    trace_window();
    dump_window();
    if (!g_loaded) load();
    if (!g_nsteps) return;
    g_frame++;
    for (i = 0; i < g_nrel; ) {
        if (g_rel[i].frame <= g_frame) {
            if (g_rel[i].mouse) hdx_mouse(0, 0, 0);
            else key(g_rel[i].vk, g_rel[i].dik, 0);
            g_rel[i] = g_rel[--g_nrel];
        } else i++;
    }
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
        } else if (!strcmp(s->op, "mark")) {
            /* a checkpoint: already logged above */
        } else if (!strcmp(s->op, "shot")) {
            char p[1200];
            snprintf(p, sizeof p, "%s/%s.ppm", g_shots, s->a[0]);
            hglide_shot(p);
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
