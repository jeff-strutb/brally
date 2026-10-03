/* script.c: replay a brbox scenario script (a .txt in tools/brbox_scripts), for
 * any host.
 *
 *   BR_SCRIPT=<file>   the script
 *   BR_SHOTS=<dir>     where `shot NAME` writes NAME.png (default
 *                      build/portable/shots)
 *
 * The semantics are the 32-bit lane's (ports/macos/wasm/host/host_script.c)
 * so both lanes run the same scenarios and their shots compare: a frame is
 * one entry to BrAppFrame (0x1001CF80), which calls plat_app_frame().
 *
 * Supported: sleep N, press KEY [N], hold KEY, release KEY, mouse X Y (slam
 * to the top-left, then move by X,Y), point X Y (the same, as a position),
 * click [N], mark NAME, shot NAME, end, quit; and, from script_game.c,
 * wait/waitb, autopilot, waittext, text and files. savefiles, joystick and
 * tmu are accepted and do nothing here. Anything else (peer: a second
 * machine) stops the run (exit 2) rather than silently diverging from the
 * other lane. */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "plat.h"
#include "brr.h"

typedef struct { const char *name; int vk, dik; } keyname;

static const keyname k_keys[] = {
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

static int key_lookup(const char *n, int *vk, int *dik)
{
    static const char *rows[] = { "QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM" };
    static const int base[] = { 0x10, 0x1E, 0x2C };
    size_t i;
    for (i = 0; i < sizeof k_keys / sizeof k_keys[0]; i++)
        if (!strcasecmp(n, k_keys[i].name)) {
            *vk = k_keys[i].vk;
            *dik = k_keys[i].dik;
            return 1;
        }
    if (n[0] && !n[1]) {
        char c = (char)toupper((unsigned char)n[0]);
        for (i = 0; i < 3; i++) {
            const char *p = strchr(rows[i], c);
            if (p) {
                *vk = c;
                *dik = base[i] + (int)(p - rows[i]);
                return 1;
            }
        }
        if (c >= '1' && c <= '9') {
            *vk = c;
            *dik = 0x02 + (c - '1');
            return 1;
        }
        if (c == '0') {
            *vk = c;
            *dik = 0x0B;
            return 1;
        }
    }
    return 0;
}

typedef struct { char op[16]; char a[4][64]; int na, line; } stmt;
typedef struct { unsigned long frame; int vk, dik, mouse; } rel;

static stmt *s_prog;
static int s_n, s_pc, s_loaded, s_active, s_nrel, s_mouse_phase;
static rel s_rel[64];
static unsigned long s_frame, s_sleep_until;
static char s_shots[1024];

/* `peer SCRIPT`: a second copy of the game, driven by SCRIPT (beside this
 * one), as the other machine on the network -- its own saves and shots
 * under ours, its output in peer.log there. A copy that is itself the peer
 * ignores the line. It is stopped when this one exits. */
static intptr_t s_peer;

static void stop_peer(void)
{
    /* the other copy finishes its own script, as the oracle lets its peer */
    plat_peersync_end();
    if (s_peer)
        host_kill(s_peer, 30000);
    s_peer = 0;
}

static void start_peer(const char *path)
{
    static char e_script[1200], e_save[1200], e_shots[1200], e_sync[64], log[1200];
    const char *env[8];
    uint16_t sync;
    const char *slash;
    int i;
    if (getenv("BR_PEER"))
        return;
    for (i = 0; i < s_n; i++)
        if (!strcmp(s_prog[i].op, "peer") && s_prog[i].na >= 1)
            break;
    if (i == s_n)
        return;
    slash = strrchr(path, '/');
    snprintf(e_script, sizeof e_script, "BR_SCRIPT=%.*s%s", slash ? (int)(slash - path + 1) : 0, path, s_prog[i].a[0]);
    snprintf(e_save, sizeof e_save, "BR_SAVEDIR=%s/peer", host_save_dir());
    host_mkdir(e_save + strlen("BR_SAVEDIR="));
    snprintf(e_shots, sizeof e_shots, "BR_SHOTS=%s/peer", s_shots);
    snprintf(log, sizeof log, "%s/peer/peer.log", host_save_dir());
    env[0] = e_script;
    env[1] = e_save;
    env[2] = e_shots;
    env[3] = "BR_PEER=1";
    env[4] = "BR_VCLOCK_IMPORTS=1";     /* the oracle's clock: both copies */
    env[5] = NULL;
    plat_vclock_imports(1);
    if ((sync = plat_peersync_listen()) != 0) {      /* BR_VCLOCK: one timeline */
        snprintf(e_sync, sizeof e_sync, "BR_PEERSYNC=%u", sync);
        env[5] = e_sync;
        env[6] = NULL;
    }
    s_peer = host_spawn_self(env, log);
    fprintf(stderr, "script: peer %s %s\n", s_prog[i].a[0], s_peer ? "started" : "could not start");
    if (s_peer)
        atexit(stop_peer);
}

static void load(void)
{
    const char *path = getenv("BR_SCRIPT"), *d = getenv("BR_SHOTS");
    char line[512];
    FILE *f;
    int ln = 0;
    s_loaded = 1;
    snprintf(s_shots, sizeof s_shots, "%s", d ? d : "build/portable/shots");
    if (!path)
        return;
    if (!(f = fopen(path, "r"))) {
        fprintf(stderr, "script: cannot open %s\n", path);
        exit(2);
    }
    while (fgets(line, sizeof line, f)) {
        stmt st;
        char *h = strchr(line, '#'), *tok;
        ln++;
        if (h)
            *h = 0;
        memset(&st, 0, sizeof st);
        tok = strtok(line, " \t\r\n");
        if (!tok)
            continue;
        snprintf(st.op, sizeof st.op, "%s", tok);
        while (st.na < 4 && (tok = strtok(NULL, " \t\r\n")))
            snprintf(st.a[st.na++], sizeof st.a[0], "%s", tok);
        st.line = ln;
        s_prog = (stmt *)realloc(s_prog, (size_t)(s_n + 1) * sizeof *s_prog);
        s_prog[s_n++] = st;
    }
    fclose(f);
    s_active = 1;
    host_mkdir(s_shots);
    start_peer(path);
}

void plat_script_key(int vk, int dik, int down);
const void *plat_script_orig(unsigned va, unsigned n);
int  plat_script_orig_ptr(unsigned va, unsigned *out);
int  plat_script_text_seen(const char *want);
void plat_script_text_log(unsigned long frame);
void plat_script_autopilot(void);
void plat_script_autopilot_off(void);

static int s_ap, s_waiting;
static unsigned long s_wait_since;

static int wait_test(const char *op, char a[][64])
{
    unsigned va = (unsigned)strtoul(a[0], 0, 0), v = (unsigned)strtoul(a[2], 0, 0), x;
    const void *p = plat_script_orig(va, !strcmp(op, "waitb") ? 1 : 4);
    const char *o = a[1];
    if (!p && strcmp(op, "waitb") && plat_script_orig_ptr(va, &x))
        goto have;
    if (!p) {
        fprintf(stderr, "script: 0x%08X is not an address this build can read\n", va);
        exit(2);
    }
    x = !strcmp(op, "waitb") ? *(const unsigned char *)p : *(const unsigned *)p;
have:
    return !strcmp(o, "==") ? x == v : !strcmp(o, "!=") ? x != v :
           !strcmp(o, ">") ? x > v : !strcmp(o, ">=") ? x >= v :
           !strcmp(o, "<") ? x < v : !strcmp(o, "<=") ? x <= v : 0;
}

static void key(int vk, int dik, int down);
void plat_script_key(int vk, int dik, int down) { key(vk, dik, down); }

/* a key, and on a press of a typing key the character a keyboard would
 * send with it (lower case), as the wasm lane's script does -- a real host
 * gets the character from its own text input */
static void key(int vk, int dik, int down)
{
    host_event ev;
    memset(&ev, 0, sizeof ev);
    ev.type = HOST_EV_KEY;
    ev.vk = vk;
    ev.scan = dik;
    ev.down = down;
    plat_deliver(&ev);
    if (down && ((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9') ||
                 vk == 0x20 || vk == 0x0D || vk == 0x08 || vk == 0x1B)) {
        memset(&ev, 0, sizeof ev);
        ev.type = HOST_EV_CHAR;
        ev.ch = (vk >= 'A' && vk <= 'Z') ? vk + 32 : vk;
        plat_deliver(&ev);
    }
}

static void step(void)
{
    while (s_pc < s_n && s_frame >= s_sleep_until) {
        stmt *s = &s_prog[s_pc];
        int vk, dik;
        fprintf(stderr, "script: frame %lu: %s%s%s\n", s_frame, s->op, s->na ? " " : "", s->a[0]);
        if (!strcmp(s->op, "sleep")) {
            s_sleep_until = s_frame + (unsigned long)atoi(s->a[0]);
        } else if (!strcmp(s->op, "press") || !strcmp(s->op, "hold") || !strcmp(s->op, "release")) {
            if (!key_lookup(s->a[0], &vk, &dik)) {
                fprintf(stderr, "script line %d: unknown key %s\n", s->line, s->a[0]);
                exit(2);
            }
            key(vk, dik, strcmp(s->op, "release") != 0);
            if (!strcmp(s->op, "press") && s_nrel < 64) {
                unsigned long n = s->na > 1 ? (unsigned long)atoi(s->a[1]) : 2;
                s_rel[s_nrel].frame = s_frame + n;
                s_rel[s_nrel].vk = vk;
                s_rel[s_nrel].dik = dik;
                s_rel[s_nrel].mouse = 0;
                s_nrel++;
                s_sleep_until = s_frame + n + 1;
            }
        } else if (!strcmp(s->op, "mouse") || !strcmp(s->op, "point")) {
            if (s_mouse_phase == 0) {         /* slam into the top-left corner */
                plat_mouse_move(-100000, -100000);
                s_mouse_phase = 1;
                return;
            }
            plat_mouse_move(atoi(s->a[0]), atoi(s->a[1]));
            s_mouse_phase = 0;
            s_sleep_until = s_frame + 2;
        } else if (!strcmp(s->op, "click")) {
            unsigned long n = s->na > 0 ? (unsigned long)atoi(s->a[0]) : 2;
            plat_mouse_button(1);
            if (s_nrel < 64) {
                s_rel[s_nrel].frame = s_frame + n;
                s_rel[s_nrel].mouse = 1;
                s_nrel++;
            }
            s_sleep_until = s_frame + n + 1;
        } else if (!strcmp(s->op, "wait") || !strcmp(s->op, "waitb")) {
            if (!wait_test(s->op, s->a)) {
                unsigned long lim = s->na > 3 ? (unsigned long)atoi(s->a[3]) : 1800;
                if (!s_waiting) {
                    s_waiting = 1;
                    s_wait_since = s_frame;
                }
                if (s_frame - s_wait_since > lim) {
                    fprintf(stderr, "script line %d timed out\n", s->line);
                    exit(3);
                }
                return;
            }
            s_waiting = 0;
        } else if (!strcmp(s->op, "waittext")) {
            if (!plat_script_text_seen(s->a[0])) {
                unsigned long lim = s->na > 1 ? (unsigned long)atoi(s->a[1]) : 1800;
                if (!s_waiting) {
                    s_waiting = 1;
                    s_wait_since = s_frame;
                }
                if (s_frame - s_wait_since > lim) {
                    fprintf(stderr, "script line %d: text '%s' never drawn\n", s->line, s->a[0]);
                    exit(3);
                }
                return;
            }
            s_waiting = 0;
        } else if (!strcmp(s->op, "text")) {
            plat_script_text_log(s_frame);
        } else if (!strcmp(s->op, "autopilot")) {
            s_ap = !strcmp(s->a[0], "on");
            if (!s_ap)
                plat_script_autopilot_off();
        } else if (!strcmp(s->op, "files") || !strcmp(s->op, "savefiles") || !strcmp(s->op, "peer") ||
                   !strcmp(s->op, "joystick") || !strcmp(s->op, "tmu")) {
            /* setup only (files: applied before the game starts) */
        } else if (!strcmp(s->op, "mark")) {
            /* a checkpoint: logged above */
        } else if (!strcmp(s->op, "shot")) {
            char p[1200];
            snprintf(p, sizeof p, "%s/%s.png", s_shots, s->a[0]);
            if (!brr_shot(p))
                fprintf(stderr, "script line %d: this renderer cannot take shots\n", s->line);
        } else if (!strcmp(s->op, "end") || !strcmp(s->op, "quit")) {
            fprintf(stderr, "script: end at frame %lu\n", s_frame);
            exit(0);
        } else {
            fprintf(stderr, "script line %d: '%s' is not supported by this host yet\n", s->line, s->op);
            exit(2);
        }
        s_pc++;
    }
}

/* BR_DUMP=F:PATH -- at BrAppFrame entry F, write the original's data area
 * (0x10077000..0x118F0000) as this build holds it: every global whose layout
 * is the same in both builds, at its original address, the rest zero. The
 * wasm lane's BR_DUMP writes the same range from the original image, so the
 * two files compare byte for byte (ports/64b/tools/dumpdiff.py). */
extern const struct { unsigned va; const void *p; unsigned n; } g_brDumpMap[];

static void dump_window(void)
{
    static int init, at = -1;
    static char path[1024];
    static unsigned n;
    n++;
    if (!init) {
        const char *e = getenv("BR_DUMP");
        init = 1;
        if (e)
            sscanf(e, "%d:%1023s", &at, path);
    }
    if (at >= 0 && (int)n == at) {
        const unsigned lo = 0x10077000u, hi = 0x118F0000u;
        unsigned char *img = (unsigned char *)calloc(1, hi - lo);
        FILE *f;
        int i;
        if (!img)
            return;
        for (i = 0; g_brDumpMap[i].p; i++)
            if (g_brDumpMap[i].va >= lo && g_brDumpMap[i].va + g_brDumpMap[i].n <= hi)
                memcpy(img + (g_brDumpMap[i].va - lo), g_brDumpMap[i].p, g_brDumpMap[i].n);
        f = fopen(path, "wb");
        if (f) {
            fwrite(img, 1, hi - lo, f);
            fclose(f);
        }
        free(img);
        fprintf(stderr, "dump: frame %u -> %s\n", n, path);
    }
}

/* BR_TRACE_FRAMES=A:B -- the frames BR_GLLOG (glide.c) logs, counted at
 * BrAppFrame entry from boot as the wasm lane counts them */
extern int g_plat_tracing;

static void trace_window(void)
{
    static int init, a = -1, b = -1;
    static unsigned n;
    n++;
    if (!init) {
        const char *e = getenv("BR_TRACE_FRAMES");
        init = 1;
        if (e)
            sscanf(e, "%d:%d", &a, &b);
    }
    if (a >= 0) {
        g_plat_tracing = (int)n >= a && (int)n <= b;
        if ((int)n == a || (int)n == b + 1)
            fprintf(stderr, "== frame %u\n", n);
    }
}

/* BrAppFrame's entry: one script frame */
void plat_app_frame(void)
{
    int i;
    trace_window();
    dump_window();
    if (!s_loaded)
        load();
    if (!s_active)
        return;
    plat_vclock_frame();
    s_frame++;
    for (i = 0; i < s_nrel; i++) {
        if (s_rel[i].frame <= s_frame) {
            if (s_rel[i].mouse)
                plat_mouse_button(0);
            else
                key(s_rel[i].vk, s_rel[i].dik, 0);
            s_rel[i--] = s_rel[--s_nrel];
        }
    }
    if (s_ap)
        plat_script_autopilot();
    step();
}
