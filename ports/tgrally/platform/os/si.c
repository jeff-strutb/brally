/* si.c: the controllers, the Controller Pak and the Rumble Pak.
 *
 * Input is either a script (n64box's format: `frame buttons [x y]` lines,
 * `p2 ...` for port 2, `pak`, `rumble`, `frames N`) or the host's keyboard
 * and game pad.  The pads are sampled once per retrace, so a run reads the
 * same input at the same frames as the original under n64box. */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "host.h"
#include "plat.h"
#include "tgr_keys.h"

enum { B_A = 0x8000, B_B = 0x4000, B_Z = 0x2000, B_START = 0x1000, B_DU = 0x0800, B_DD = 0x0400,
       B_DL = 0x0200, B_DR = 0x0100, B_L = 0x0020, B_R = 0x0010, B_CU = 0x0008, B_CD = 0x0004,
       B_CL = 0x0002, B_CR = 0x0001 };

typedef struct { int frame; uint16_t b; int8_t x, y; } Ev;
static Ev *s_ev[2];
static int s_nev[2];
static int s_pads = 1;
static int s_pak, s_rumble;
static int s_scripted;
static struct { uint16_t b; int8_t x, y; } s_now[4];

int tgr_pads(void) { return s_pads; }

static uint16_t bits_of(const char *s)
{
    static const struct { const char *n; uint16_t b; } names[] = {
        {"A", B_A}, {"B", B_B}, {"Z", B_Z}, {"START", B_START}, {"DU", B_DU}, {"DD", B_DD},
        {"DL", B_DL}, {"DR", B_DR}, {"L", B_L}, {"R", B_R}, {"CU", B_CU}, {"CD", B_CD},
        {"CL", B_CL}, {"CR", B_CR}};
    char buf[128], *tok, *save;
    uint16_t b = 0;
    unsigned i;
    if (strcmp(s, "-") == 0)
        return 0;
    snprintf(buf, sizeof buf, "%s", s);
    for (tok = strtok_r(buf, "+", &save); tok; tok = strtok_r(NULL, "+", &save))
        for (i = 0; i < sizeof names / sizeof *names; i++)
            if (strcmp(tok, names[i].n) == 0)
                b |= names[i].b;
    return b;
}

static int ev_cmp(const void *a, const void *b)
{
    return ((const Ev *)a)->frame - ((const Ev *)b)->frame;
}

void tgr_script_load(const char *path)
{
    FILE *f = fopen(path, "r");
    char line[256];
    int p;
    if (!f) {
        fprintf(stderr, "tgr: no script %s\n", path);
        exit(1);
    }
    s_scripted = 1;
    while (fgets(line, sizeof line, f)) {
        char *c = strchr(line, '#'), *w[5];
        int n = 0, port = 0;
        Ev e;
        if (c)
            *c = 0;
        for (c = strtok(line, " \t\r\n"); c && n < 5; c = strtok(NULL, " \t\r\n"))
            w[n++] = c;
        if (!n)
            continue;
        if (strcmp(w[0], "pak") == 0) { s_pak = 1; continue; }
        if (strcmp(w[0], "rumble") == 0) { s_rumble = 1; continue; }
        if (strcmp(w[0], "frames") == 0) {
            if (n > 1 && !g_tgr.frames)
                g_tgr.frames = atoi(w[1]);
            continue;
        }
        if (strcmp(w[0], "p2") == 0) {
            port = 1;
            memmove(w, w + 1, sizeof w - sizeof *w);
            n--;
        }
        if (n < 2)
            continue;
        e.frame = atoi(w[0]);
        e.b = bits_of(w[1]);
        e.x = n > 2 ? (int8_t)atoi(w[2]) : 0;
        e.y = n > 3 ? (int8_t)atoi(w[3]) : 0;
        s_ev[port] = (Ev *)realloc(s_ev[port], (size_t)(s_nev[port] + 1) * sizeof(Ev));
        s_ev[port][s_nev[port]++] = e;
    }
    fclose(f);
    for (p = 0; p < 2; p++)
        if (s_nev[p])
            qsort(s_ev[p], (size_t)s_nev[p], sizeof(Ev), ev_cmp);   /* stable enough: frames are distinct */
    s_pads = s_nev[1] ? 2 : 1;
}

/* ---- the keys and pad buttons for the N64 pad (the options screen's Keys and
 * Pad: src/menus/keysmenu.c) ----------------------------------------------------
 * Each of the pad's buttons and its stick's four ways has up to two keys
 * (Win32 virtual keys, as the host's key events carry them; 0 none) and up to
 * two game pad buttons (host_pad's numbering, 0..15; -1 none).  The left stick
 * is always the stick and the right the C buttons.  Kept as text in the save
 * folder, "keys.cfg": "<input> key K1 K2" and "<input> pad B1 B2" a line; a
 * missing file or line is the default. */
enum { K_STICK_U = 14, K_STICK_D, K_STICK_L, K_STICK_R };
static const struct {
    const char *name, *tag;
    uint16_t b;
    uint8_t key[2];
    int8_t pad[2];
} k_in[TGR_KEYS_COUNT] = {
    { "A", "a", B_A, { 'X', 0x0D }, { 0, -1 } },                 /* X or Return; pad A */
    { "B", "b", B_B, { 'Z', 0 }, { 1, -1 } },
    { "Z", "z", B_Z, { ' ', 0 }, { 10, -1 } },                    /* left trigger */
    { "Start", "start", B_START, { 0x1B, 'P' }, { 7, -1 } },      /* Escape or P; the menu button */
    { "L", "l", B_L, { 'Q', 0 }, { 4, -1 } },
    { "R", "r", B_R, { 'W', 'E' }, { 5, 11 } },                   /* right shoulder or trigger */
    { "C Up", "cu", B_CU, { 'I', 0 }, { 3, -1 } },                /* Y */
    { "C Down", "cd", B_CD, { 'K', 0 }, { 9, -1 } },              /* the right stick pressed: the chase view */
    { "C Left", "cl", B_CL, { 'J', 0 }, { 2, -1 } },              /* X */
    { "C Right", "cr", B_CR, { 'L', 0 }, { 8, -1 } },             /* the left stick pressed */
    { "D Pad Up", "du", B_DU, { 0, 0 }, { 12, -1 } },
    { "D Pad Down", "dd", B_DD, { 0, 0 }, { 14, -1 } },
    { "D Pad Left", "dl", B_DL, { 0, 0 }, { 15, -1 } },
    { "D Pad Right", "dr", B_DR, { 0, 0 }, { 13, -1 } },
    { "Stick Up", "su", 0, { 0x26, 0 }, { -1, -1 } },             /* the arrows */
    { "Stick Down", "sd", 0, { 0x28, 0 }, { -1, -1 } },
    { "Stick Left", "sl", 0, { 0x25, 0 }, { -1, -1 } },
    { "Stick Right", "sr", 0, { 0x27, 0 }, { -1, -1 } },
};
static int s_key[TGR_KEYS_COUNT][2], s_padb[TGR_KEYS_COUNT][2];
static int s_keys_loaded;
static uint8_t s_vk[256];               /* the virtual keys held */
static uint16_t s_keys;                 /* what they hold of the pad */
static int s_kx, s_ky;
static int s_cap = -1;                  /* the input awaiting a key or button (keysmenu.c), -1 none */
static int s_cap_hold;                  /* after a wait: no input until everything is let go */
static unsigned s_cap_padwas;           /* the pad's buttons when the wait began */
static unsigned s_pad_now;              /* the pad's buttons at the last retrace */

static void keys_path(char *out, size_t n) { snprintf(out, n, "%s/keys.cfg", host_save_dir()); }

static void keys_defaults(int in)
{
    s_key[in][0] = k_in[in].key[0];
    s_key[in][1] = k_in[in].key[1];
    s_padb[in][0] = k_in[in].pad[0];
    s_padb[in][1] = k_in[in].pad[1];
}

static void keys_save(void)
{
    char p[1100];
    FILE *f;
    int in;
    if (!host_save_dir())
        return;
    keys_path(p, sizeof p);
    if (!(f = fopen(p, "w")))
        return;
    for (in = 0; in < TGR_KEYS_COUNT; in++) {
        fprintf(f, "%s key %d %d\n", k_in[in].tag, s_key[in][0], s_key[in][1]);
        fprintf(f, "%s pad %d %d\n", k_in[in].tag, s_padb[in][0], s_padb[in][1]);
    }
    fclose(f);
}

static void keys_load(void)
{
    char p[1100], line[128], tag[32], kind[8];
    FILE *f;
    int in, v1, v2;
    s_keys_loaded = 1;
    for (in = 0; in < TGR_KEYS_COUNT; in++)
        keys_defaults(in);
    if (!host_save_dir())
        return;
    keys_path(p, sizeof p);
    if (!(f = fopen(p, "r")))
        return;
    while (fgets(line, sizeof line, f))
        if (sscanf(line, "%31s %7s %d %d", tag, kind, &v1, &v2) == 4)
            for (in = 0; in < TGR_KEYS_COUNT; in++)
                if (!strcmp(tag, k_in[in].tag)) {
                    if (!strcmp(kind, "key") && v1 >= 0 && v1 < 256 && v2 >= 0 && v2 < 256) {
                        s_key[in][0] = v1;
                        s_key[in][1] = v2;
                    } else if (!strcmp(kind, "pad") && v1 >= -1 && v1 < 16 && v2 >= -1 && v2 < 16) {
                        s_padb[in][0] = v1;
                        s_padb[in][1] = v2;
                    }
                }
    fclose(f);
}

/* the pad's buttons and stick from the keys held */
static void keys_apply(void)
{
    int in, k, held[TGR_KEYS_COUNT];
    s_keys = 0;
    for (in = 0; in < TGR_KEYS_COUNT; in++) {
        held[in] = 0;
        for (k = 0; k < 2; k++)
            if (s_key[in][k] && s_vk[s_key[in][k]])
                held[in] = 1;
        if (held[in])
            s_keys |= k_in[in].b;
    }
    s_ky = held[K_STICK_U] ? 80 : held[K_STICK_D] ? -80 : 0;
    s_kx = held[K_STICK_L] ? -80 : held[K_STICK_R] ? 80 : 0;
}

/* nothing held of the keyboard or the pad: a wait's last press let go */
static int all_up(void)
{
    int i;
    for (i = 0; i < 256; i++)
        if (s_vk[i])
            return 0;
    return s_pad_now == 0;
}

void tgr_input_key(int vk, int down)
{
    if (vk <= 0 || vk > 255)
        return;
    if (!s_keys_loaded)
        keys_load();
    s_vk[vk] = (uint8_t)(down != 0);
    if (s_cap >= 0 && down) {               /* the awaited key (Escape: as it was) */
        if (vk != 0x1B)
            tgr_keys_bind(s_cap, 0, vk);
        s_cap = -1;
        s_cap_hold = 1;
    }
    keys_apply();
}

/* everything released (the window lost the keyboard: its key-ups go elsewhere) */
void tgr_input_release(void)
{
    memset(s_vk, 0, sizeof s_vk);
    s_keys = 0;
    s_kx = s_ky = 0;
}

/* ---- for the Keys and Pad screen (src/menus/keysmenu.c) ---- */
int tgr_keys_shown(void) { return !s_scripted; }

const char *tgr_keys_name(int in) { return in >= 0 && in < TGR_KEYS_COUNT ? k_in[in].name : ""; }

/* a key's name, as the screen shows it */
static void key_name(int vk, char *buf, size_t n)
{
    static const struct { uint8_t vk; const char *name; } k_names[] = {
        { 0x08, "BACKSPACE" }, { 0x09, "TAB" }, { 0x0D, "RETURN" }, { 0x10, "SHIFT" }, { 0x11, "CTRL" },
        { 0x12, "ALT" }, { 0x14, "CAPS LOCK" }, { 0x1B, "ESCAPE" }, { 0x20, "SPACE" }, { 0x21, "PAGE UP" },
        { 0x22, "PAGE DOWN" }, { 0x23, "END" }, { 0x24, "HOME" }, { 0x25, "LEFT ARROW" }, { 0x26, "UP ARROW" },
        { 0x27, "RIGHT ARROW" }, { 0x28, "DOWN ARROW" }, { 0x2D, "INSERT" }, { 0x2E, "DELETE" },
        { 0x6A, "NUM *" }, { 0x6B, "NUM +" }, { 0x6D, "NUM -" }, { 0x6E, "NUM ." }, { 0x6F, "NUM /" },
        { 0xBA, ";" }, { 0xBB, "=" }, { 0xBC, "," }, { 0xBD, "-" }, { 0xBE, "." }, { 0xBF, "/" }, { 0xC0, "`" },
        { 0xDB, "[" }, { 0xDC, "\\" }, { 0xDD, "]" }, { 0xDE, "'" },
    };
    unsigned i;
    if ((vk >= '0' && vk <= '9') || (vk >= 'A' && vk <= 'Z')) {
        snprintf(buf, n, "%c", vk);
        return;
    }
    if (vk >= 0x60 && vk <= 0x69) {
        snprintf(buf, n, "NUM %d", vk - 0x60);
        return;
    }
    if (vk >= 0x70 && vk <= 0x87) {
        snprintf(buf, n, "F%d", vk - 0x6F);
        return;
    }
    snprintf(buf, n, "KEY %d", vk);
    for (i = 0; i < sizeof k_names / sizeof k_names[0]; i++)
        if (k_names[i].vk == vk)
            snprintf(buf, n, "%s", k_names[i].name);
}

/* an input's keys ("X, RETURN") and pad buttons ("PAD A"); "none" for none */
void tgr_keys_labels(int in, char *keys, int nk, char *pad, int np)
{
    static const char *const k_pads[16] = {
        "PAD A", "PAD B", "PAD X", "PAD Y", "PAD LB", "PAD RB", "PAD VIEW", "PAD MENU", "PAD L3", "PAD R3",
        "PAD LT", "PAD RT", "PAD UP", "PAD RIGHT", "PAD DOWN", "PAD LEFT",
    };
    char a[24], b[24];
    if (!s_keys_loaded)
        keys_load();
    if (in < 0 || in >= TGR_KEYS_COUNT)
        return;
    a[0] = b[0] = 0;
    if (s_key[in][0])
        key_name(s_key[in][0], a, sizeof a);
    if (s_key[in][1])
        key_name(s_key[in][1], b, sizeof b);
    snprintf(keys, (size_t)nk, "%s%s%s", a[0] ? a : b[0] ? "" : "none", a[0] && b[0] ? ", " : "", b);
    snprintf(pad, (size_t)np, "%s%s%s", s_padb[in][0] >= 0 ? k_pads[s_padb[in][0]] : s_padb[in][1] >= 0 ? "" : "none",
             s_padb[in][0] >= 0 && s_padb[in][1] >= 0 ? ", " : "", s_padb[in][1] >= 0 ? k_pads[s_padb[in][1]] : "");
}

/* an input takes this key (kind 0) or pad button (1), alone; whatever other
   input had it lets it go */
void tgr_keys_bind(int in, int kind, int code)
{
    int o, k;
    if (!s_keys_loaded)
        keys_load();
    if (in < 0 || in >= TGR_KEYS_COUNT)
        return;
    for (o = 0; o < TGR_KEYS_COUNT; o++)
        for (k = 0; k < 2; k++)
            if (kind == 0 && s_key[o][k] == code)
                s_key[o][k] = 0;
            else if (kind == 1 && s_padb[o][k] == code)
                s_padb[o][k] = -1;
    if (kind == 0) {
        s_key[in][0] = code;
        s_key[in][1] = 0;
    } else {
        s_padb[in][0] = code;
        s_padb[in][1] = -1;
    }
    keys_save();
    keys_apply();
}

void tgr_keys_reset(void)
{
    int in;
    for (in = 0; in < TGR_KEYS_COUNT; in++)
        keys_defaults(in);
    s_keys_loaded = 1;
    keys_save();
    keys_apply();
}

/* wait for the next key or pad button for an input (-1: stop waiting); while
   waiting, and until what ended it is let go, the game sees nothing pressed */
void tgr_keys_capture(int in)
{
    s_cap = in;
    s_cap_padwas = s_pad_now;
    s_cap_hold = 1;
}

int tgr_keys_capturing(void) { return s_cap; }

/* a modern stick (a circle, -1..1) as the N64's: past a small dead zone, the
 * travel rescaled to the full range and reaching the N64 stick's octagonal
 * gate, 80 at the four cardinal points and 70 on each axis at the diagonals
 * (a diagonal on a round stick would otherwise only reach 57) */
static void n64_stick(float fx, float fy, int *ox, int *oy)
{
    const float dz = 0.12f;
    float r = sqrtf(fx * fx + fy * fy), ax = fabsf(fx), ay = fabsf(fy), t, g, k;
    *ox = *oy = 0;
    if (r <= dz)
        return;
    /* the gate's edge in this octant runs from (80, 0) to (70, 70), the line
       x = 80 - y / 7 (or mirrored); met by the stick's direction y = t x, it
       lies 80 / (1 + t / 7) along x: the octagon's radius that way */
    t = ax >= ay ? ay / ax : ax / ay;
    g = 80.0f / (1.0f + t / 7.0f) * sqrtf(1.0f + t * t);
    k = (r > 1 ? 1 : (r - dz) / (1 - dz)) * g / r;   /* the travel past the dead zone, to the gate */
    *ox = (int)lroundf(fx * k);
    *oy = (int)lroundf(fy * k);
}

void tgr_input_frame(uint32_t frame)
{
    int p, i;
    if (s_scripted) {
        for (p = 0; p < 2; p++) {
            s_now[p].b = 0;
            s_now[p].x = s_now[p].y = 0;
            for (i = 0; i < s_nev[p] && s_ev[p][i].frame <= (int)frame; i++) {
                s_now[p].b = s_ev[p][i].b;
                s_now[p].x = s_ev[p][i].x;
                s_now[p].y = s_ev[p][i].y;
            }
        }
        return;
    }
    {
        host_pad hp;
        int x = s_kx, y = s_ky;
        uint16_t b = s_keys;
        if (!s_keys_loaded)
            keys_load();
        s_pad_now = 0;
        if (host_pad_read(&hp)) {
            int px, py, in, k;
            s_pad_now = hp.buttons & 0xFFFF;
            n64_stick(hp.x, -hp.y, &px, &py);
            if (px || py) {
                x = px;
                y = py;
            }
            if (hp.rx > 0.5f) b |= B_CR;              /* the right stick: the C buttons */
            if (hp.rx < -0.5f) b |= B_CL;
            if (hp.ry > 0.5f) b |= B_CD;
            if (hp.ry < -0.5f) b |= B_CU;
            for (in = 0; in < TGR_KEYS_COUNT; in++)   /* the buttons, as Keys and Pad has them */
                for (k = 0; k < 2; k++)
                    if (s_padb[in][k] >= 0 && (hp.buttons >> s_padb[in][k]) & 1) {
                        b |= k_in[in].b;
                        if (in == K_STICK_U) y = 80;
                        if (in == K_STICK_D) y = -80;
                        if (in == K_STICK_L) x = -80;
                        if (in == K_STICK_R) x = 80;
                    }
        }
        if (s_cap >= 0) {                             /* a button awaited (keysmenu.c) */
            unsigned fresh = s_pad_now & ~s_cap_padwas;
            int i;
            for (i = 0; i < 16; i++)
                if ((fresh >> i) & 1) {
                    tgr_keys_bind(s_cap, 1, i);
                    s_cap = -1;
                    break;
                }
            s_cap_padwas &= s_pad_now;               /* (a button let go may be pressed again) */
        }
        if (s_cap_hold && s_cap < 0 && all_up())
            s_cap_hold = 0;
        if (s_cap >= 0 || s_cap_hold) {               /* waiting, or what ended it still held: */
            b = 0;                                    /* the game sees nothing pressed */
            x = y = 0;
        }
        s_now[0].b = b;
        s_now[0].x = (int8_t)x;
        s_now[0].y = (int8_t)y;
    }
}

/* ---- libultra ---------------------------------------------------------------- */
int32_t osContInit(OSMesgQueue *mq, uint8_t *bitpattern, OSContStatus *status)
{
    int i;
    (void)mq;
    *bitpattern = (uint8_t)((1 << s_pads) - 1);
    for (i = 0; i < 4; i++) {
        status[i].type = i < s_pads ? 0x0500 : 0;     /* n64box's bytes 05 00 */
        status[i].status = 0;
        status[i].errnum = i < s_pads ? 0 : 8;
    }
    return 0;
}

int32_t osContStartReadData(OSMesgQueue *mq)
{
    (void)mq;
    tgr_os_lock();
    tgr_post_event_at(tgr_count(), OS_EVENT_SI);
    tgr_os_unlock();
    return 0;
}

void osContGetReadData(OSContPad *pad)
{
    int i;
    if (s_scripted)
        tgr_input_frame(tgr_frame());   /* the script's input at this frame, as n64box reads it */
    for (i = 0; i < 4; i++) {
        if (i < s_pads) {
            pad[i].button = s_now[i].b;
            pad[i].stick_x = s_now[i].x;
            pad[i].stick_y = s_now[i].y;
            pad[i].errnum = 0;
        } else {
            memset(&pad[i], 0, sizeof pad[i]);
            pad[i].errnum = 8;
        }
    }
}

/* ---- the Controller Pak: absent unless a script plugs one in -------------------- */
int32_t osPfsIsPlug(OSMesgQueue *mq, uint8_t *pattern)
{
    (void)mq;
    *pattern = (s_pak || s_rumble) ? 1 : 0;
    return 0;
}

int32_t osPfsInit(OSMesgQueue *mq, OSPfs *pfs, int32_t channel)
{
    if (!s_pak)
        return s_rumble ? PFS_ERR_ID_FATAL : PFS_ERR_NOPACK;
    pfs->status = 0;
    pfs->queue = tgr_addr32(mq);
    pfs->channel = channel;
    {
        int i;
        for (i = 0; i < 32; i++)
            pfs->id[i] = (uint8_t)(0x40 + i);
    }
    return 0;
}

int32_t osPfsInitPak(OSMesgQueue *mq, OSPfs *pfs, int32_t channel) { return osPfsInit(mq, pfs, channel); }
int32_t osPfsRepairId(OSPfs *pfs) { (void)pfs; return s_pak ? 0 : PFS_ERR_NOPACK; }
int32_t osPfsChecker(OSPfs *pfs) { (void)pfs; return s_pak ? 0 : PFS_ERR_NOPACK; }

/* the pak's notes: in a scripted run an empty pak whose saves last the run
 * (as n64box models it); in play, a pak kept in a file in the host's save
 * folder, written after every change */
#define PAK_BYTES (123 * 256)
#define PAK_NOTES 16
static struct { int used; uint16_t company; uint32_t game; uint8_t name[16], ext[4]; uint8_t *data; int size; }
    s_notes[PAK_NOTES];

static char s_pak_path[1024];

static void put32(FILE *f, uint32_t v) { fputc((int)(v >> 24), f); fputc((int)(v >> 16) & 255, f); fputc((int)(v >> 8) & 255, f); fputc((int)v & 255, f); }
static uint32_t get32(FILE *f)
{
    uint32_t v = 0;
    int k;
    for (k = 0; k < 4; k++)
        v = v << 8 | (uint32_t)(fgetc(f) & 255);
    return v;
}

/* the file: "TGRPAK1\n", the note count, then each note's company, game,
 * name, extension, size and bytes (big-endian numbers) */
static void pak_save(void)
{
    char tmp[1100];
    FILE *f;
    int i, n = 0;
    if (!s_pak_path[0])
        return;
    snprintf(tmp, sizeof tmp, "%s.tmp", s_pak_path);
    f = fopen(tmp, "wb");
    if (!f)
        return;
    for (i = 0; i < PAK_NOTES; i++)
        n += s_notes[i].used;
    fwrite("TGRPAK1\n", 1, 8, f);
    put32(f, (uint32_t)n);
    for (i = 0; i < PAK_NOTES; i++) {
        if (!s_notes[i].used)
            continue;
        put32(f, s_notes[i].company);
        put32(f, s_notes[i].game);
        fwrite(s_notes[i].name, 1, 16, f);
        fwrite(s_notes[i].ext, 1, 4, f);
        put32(f, (uint32_t)s_notes[i].size);
        fwrite(s_notes[i].data, 1, (size_t)s_notes[i].size, f);
    }
    if (fclose(f) == 0)
        host_rename(tmp, s_pak_path);
}

static void pak_clear(void)
{
    int i;
    for (i = 0; i < PAK_NOTES; i++) {
        free(s_notes[i].data);
        memset(&s_notes[i], 0, sizeof s_notes[i]);
    }
}

/* the notes from a pak file: 1 when it was read whole */
static int pak_load(const char *path)
{
    char magic[8];
    FILE *f = fopen(path, "rb");
    uint32_t n = 0, i = 0;
    if (!f)
        return 0;
    if (fread(magic, 1, 8, f) == 8 && !memcmp(magic, "TGRPAK1\n", 8)) {
        n = get32(f);
        for (i = 0; i < n && i < PAK_NOTES; i++) {
            s_notes[i].company = (uint16_t)get32(f);
            s_notes[i].game = get32(f);
            if (fread(s_notes[i].name, 1, 16, f) != 16 || fread(s_notes[i].ext, 1, 4, f) != 4)
                break;
            s_notes[i].size = (int)get32(f);
            if (s_notes[i].size <= 0 || s_notes[i].size > PAK_BYTES)
                break;
            s_notes[i].data = (uint8_t *)calloc(1, (size_t)s_notes[i].size);
            if (fread(s_notes[i].data, 1, (size_t)s_notes[i].size, f) != (size_t)s_notes[i].size)
                break;
            s_notes[i].used = 1;                      /* only a note read whole */
        }
        if (i < PAK_NOTES && !s_notes[i].used) {      /* a file cut short: its last note is dropped */
            free(s_notes[i].data);
            memset(&s_notes[i], 0, sizeof s_notes[i]);
        }
    } else {
        n = 1;                                        /* not a pak */
    }
    fclose(f);
    return i == n;
}

/* in play (no script) a Controller Pak is plugged into port 1, kept in
 * <save folder>/controller-pak-1.bin */
void tgr_pak_init(void)
{
    const char *dir;
    char tmp[1100];
    if (s_scripted) {
        /* TGR_PAK_FILE: the scripted run's pak kept in that file (a test of the
           saves themselves; the script still plugs and unplugs it) */
        const char *e = getenv("TGR_PAK_FILE");
        if (e) {
            snprintf(s_pak_path, sizeof s_pak_path, "%s", e);
            pak_load(s_pak_path);
        }
        return;
    }
    dir = host_save_dir();
    if (!dir)
        return;
    host_mkdir(dir);
    snprintf(s_pak_path, sizeof s_pak_path, "%s/controller-pak-1.bin", dir);
    s_pak = 1;
    /* a whole .tmp left beside the pak is its newest state, written but not
       moved over it: a run stopped between the two, or a Windows build whose
       rename would not replace the pak (every save after the first) */
    snprintf(tmp, sizeof tmp, "%s.tmp", s_pak_path);
    if (pak_load(tmp)) {
        host_rename(tmp, s_pak_path);
        return;
    }
    pak_clear();
    pak_load(s_pak_path);
}

static int note_find(uint16_t company, uint32_t game, uint8_t *name, uint8_t *ext)
{
    int i;
    for (i = 0; i < PAK_NOTES; i++)
        if (s_notes[i].used && s_notes[i].company == company && s_notes[i].game == game &&
            !memcmp(s_notes[i].name, name, 16) && !memcmp(s_notes[i].ext, ext, 4))
            return i;
    return -1;
}

int32_t osPfsFindFile(OSPfs *pfs, uint16_t company, uint32_t game, uint8_t *name, uint8_t *ext, int32_t *file_no)
{
    int i;
    (void)pfs;
    if (!s_pak)
        return PFS_ERR_NOPACK;
    i = note_find(company, game, name, ext);
    if (i < 0)
        return PFS_ERR_INVALID;
    *file_no = i;
    return 0;
}

int32_t osPfsAllocateFile(OSPfs *pfs, uint16_t company, uint32_t game, uint8_t *name, uint8_t *ext,
                          int32_t length, int32_t *file_no)
{
    int i, used = 0;
    (void)pfs;
    if (!s_pak)
        return PFS_ERR_NOPACK;
    for (i = 0; i < PAK_NOTES; i++)
        if (s_notes[i].used)
            used += s_notes[i].size;
    for (i = 0; i < PAK_NOTES && s_notes[i].used; i++)
        ;
    if (i == PAK_NOTES || used + length > PAK_BYTES)
        return PFS_DATA_FULL;
    s_notes[i].used = 1;
    s_notes[i].company = company;
    s_notes[i].game = game;
    memcpy(s_notes[i].name, name, 16);
    memcpy(s_notes[i].ext, ext, 4);
    s_notes[i].size = (length + 255) & ~255;
    s_notes[i].data = (uint8_t *)calloc(1, (size_t)s_notes[i].size);
    *file_no = i;
    pak_save();
    return 0;
}

int32_t osPfsDeleteFile(OSPfs *pfs, uint16_t company, uint32_t game, uint8_t *name, uint8_t *ext)
{
    int i;
    (void)pfs;
    if (!s_pak)
        return PFS_ERR_NOPACK;
    i = note_find(company, game, name, ext);
    if (i < 0)
        return PFS_ERR_INVALID;
    free(s_notes[i].data);
    memset(&s_notes[i], 0, sizeof s_notes[i]);
    pak_save();
    return 0;
}

int32_t osPfsReadWriteFile(OSPfs *pfs, int32_t file_no, uint8_t flag, int32_t offset, int32_t size, uint8_t *data)
{
    (void)pfs;
    if (!s_pak)
        return PFS_ERR_NOPACK;
    if (file_no < 0 || file_no >= PAK_NOTES || !s_notes[file_no].used || offset + size > s_notes[file_no].size)
        return PFS_ERR_INVALID;
    if (flag == 0)
        memcpy(data, s_notes[file_no].data + offset, (size_t)size);
    else {
        memcpy(s_notes[file_no].data + offset, data, (size_t)size);
        pak_save();
    }
    return 0;
}

int32_t osPfsFreeBlocks(OSPfs *pfs, int32_t *bytes)
{
    int i, used = 0;
    (void)pfs;
    if (!s_pak)
        return PFS_ERR_NOPACK;
    for (i = 0; i < PAK_NOTES; i++)
        if (s_notes[i].used)
            used += s_notes[i].size;
    *bytes = PAK_BYTES - used;
    return 0;
}

int32_t osPfsNumFiles(OSPfs *pfs, int32_t *max_files, int32_t *files_used)
{
    int i, n = 0;
    (void)pfs;
    if (!s_pak)
        return PFS_ERR_NOPACK;
    for (i = 0; i < PAK_NOTES; i++)
        n += s_notes[i].used;
    *max_files = PAK_NOTES;
    *files_used = n;
    return 0;
}

int32_t osPfsFileState(OSPfs *pfs, int32_t file_no, OSPfsState *st)
{
    (void)pfs;
    if (!s_pak)
        return PFS_ERR_NOPACK;
    if (file_no < 0 || file_no >= PAK_NOTES || !s_notes[file_no].used)
        return PFS_ERR_INVALID;
    st->file_size = (uint32_t)s_notes[file_no].size;
    st->game_code = s_notes[file_no].game;
    st->company_code = s_notes[file_no].company;
    memcpy(st->ext_name, s_notes[file_no].ext, 4);
    memcpy(st->game_name, s_notes[file_no].name, 16);
    return 0;
}

/* ---- the Rumble Pak ------------------------------------------------------------ */
int32_t osMotorInit(OSMesgQueue *mq, OSPfs *pfs, int32_t channel)
{
    if (!s_rumble || channel != 0)
        return PFS_ERR_NOPACK;
    pfs->status = 0;
    pfs->queue = tgr_addr32(mq);
    pfs->channel = 0;
    pfs->activebank = 0xFF;
    return 0;
}

int32_t osMotorStart(OSPfs *pfs) { (void)pfs; return 0; }
int32_t osMotorStop(OSPfs *pfs) { (void)pfs; return 0; }
