/* controls.c: the two players' controls (controls.h).  Kept as text in the
 * app's save folder, "controls.cfg": "p<player> <action> key <code>" and
 * "... pad <code>" a line; a missing file or line is the default:
 *
 *   player 1  keys: X accelerates and selects, Z brakes and goes back, C
 *             reverses, the arrows steer and move through the menus, W and Q
 *             change gear (R and L), I changes the view (C-up), K, J and L the
 *             other C buttons, Space Z, Escape pauses (Start); one key an
 *             action, none of them a key Boss Rally keeps for itself (the
 *             arrows, Return and Escape are its menus' own, the same meaning)
 *   player 2  keys: A accelerates, S brakes, D reverses, F and H steer, T and G
 *             up and down, R and E change gear, V the view, P pauses; Z and the
 *             other C buttons none (player 1 paints)
 *   both      pad: A accelerates, B brakes, X reverses, the d-pad steers and
 *             moves, RB and LB change gear, Y the view, Menu pauses, LT Z, R3,
 *             RT and L3 the other C buttons (the iPhone's touch controls send
 *             A, B and X: accelerator, brake, reverse)
 *
 * The file starts "version 2"; one without it holds the first controls (Boss
 * Rally's keys, which collided with the menus') and is set aside for these. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "host.h"
#include "controls.h"

static const char *const k_act[RM_ACT_COUNT] = {
    "Accelerate / Select", "Brake / Back", "Reverse", "Steer Left", "Steer Right", "Gear Up", "Gear Down",
    "Change View", "Up", "Down", "Pause / Start", "Z", "C Down", "C Left", "C Right",
};
static const char *const k_tag[RM_ACT_COUNT] = {
    "accel", "brake", "reverse", "left", "right", "gearup", "geardown", "view",
    "up", "down", "start", "z", "cdown", "cleft", "cright",
};
static const unsigned char k_key_def[2][RM_ACT_COUNT] = {
    /*  X     Z     C     <     >     W     Q     I     ^     v    Esc  Space   K     J     L */
    { 0x2D, 0x2C, 0x2E, 0xCB, 0xCD, 0x11, 0x10, 0x17, 0xC8, 0xD0, 0x01, 0x39, 0x25, 0x24, 0x26 },
    /*  A     S     D     F     H     R     E     V     T     G     P */
    { 0x1E, 0x1F, 0x20, 0x21, 0x23, 0x13, 0x12, 0x2F, 0x14, 0x22, 0x19, 0, 0, 0, 0 },
};
static const signed char k_pad_def[RM_ACT_COUNT] = { 0, 1, 2, 15, 13, 5, 4, 3, 12, 14, 7, 10, 9, 11, 8 };

static int s_key[2][RM_ACT_COUNT], s_pad[2][RM_ACT_COUNT];
static int s_loaded;

const char *rm_action_name(int act) { return act >= 0 && act < RM_ACT_COUNT ? k_act[act] : ""; }

void rm_bind_defaults(int player)
{
    int a;
    if (player < 0 || player > 1)
        return;
    for (a = 0; a < RM_ACT_COUNT; a++) {
        s_key[player][a] = k_key_def[player][a];
        s_pad[player][a] = k_pad_def[a];
    }
}

static void save(void);

void rm_bind_reset(int player)
{
    if (!s_loaded)
        rm_controls_load();
    rm_bind_defaults(player);
    save();
}

static void path(char *out, size_t n) { snprintf(out, n, "%s/controls.cfg", host_save_dir()); }

static void save(void)
{
    char p[1100];
    FILE *f;
    int pl, a;
    path(p, sizeof p);
    if (!(f = fopen(p, "w")))
        return;
    fprintf(f, "version 2\n");
    for (pl = 0; pl < 2; pl++)
        for (a = 0; a < RM_ACT_COUNT; a++) {
            fprintf(f, "p%d %s key %d\n", pl + 1, k_tag[a], s_key[pl][a]);
            fprintf(f, "p%d %s pad %d\n", pl + 1, k_tag[a], s_pad[pl][a]);
        }
    fclose(f);
}

void rm_controls_load(void)
{
    char p[1100], line[128], tag[32], kind[8];
    FILE *f;
    int pl, a, v, ours = 0;
    rm_bind_defaults(0);
    rm_bind_defaults(1);
    s_loaded = 1;
    path(p, sizeof p);
    if (!(f = fopen(p, "r")))
        return;
    while (fgets(line, sizeof line, f)) {
        if (!strncmp(line, "version 2", 9))
            ours = 1;
        if (!ours || sscanf(line, "p%d %31s %7s %d", &pl, tag, kind, &v) != 4 || pl < 1 || pl > 2)
            continue;
        for (a = 0; a < RM_ACT_COUNT; a++)
            if (!strcmp(tag, k_tag[a])) {
                if (!strcmp(kind, "key") && v >= 0 && v < 256)
                    s_key[pl - 1][a] = v;
                else if (!strcmp(kind, "pad") && v >= -1 && v < 16)
                    s_pad[pl - 1][a] = v;
            }
    }
    fclose(f);
    if (!ours)
        save();                         /* the first controls: these from now on */
}

int rm_bind(int player, int act, int kind)
{
    if (!s_loaded)
        rm_controls_load();
    if (player < 0 || player > 1 || act < 0 || act >= RM_ACT_COUNT)
        return kind == RM_BIND_KEY ? 0 : -1;
    return kind == RM_BIND_KEY ? s_key[player][act] : s_pad[player][act];
}

void rm_bind_set(int player, int act, int kind, int code)
{
    int (*t)[RM_ACT_COUNT] = kind == RM_BIND_KEY ? s_key : s_pad;
    int a;
    if (!s_loaded)
        rm_controls_load();
    if (player < 0 || player > 1 || act < 0 || act >= RM_ACT_COUNT)
        return;
    for (a = 0; a < RM_ACT_COUNT; a++)       /* one action a key or button */
        if (a != act && t[player][a] == code)
            t[player][a] = kind == RM_BIND_KEY ? 0 : -1;
    t[player][act] = code;
    save();
    if (getenv("RM_LOG"))
        fprintf(stderr, "remastered: player %d %s %s %d\n", player + 1, k_tag[act],
                kind == RM_BIND_KEY ? "key" : "pad", code);
}

/* ---- names ------------------------------------------------------------------------ */
static const struct { unsigned char dik; const char *name; } k_keys[] = {
    { 0x01, "ESCAPE" }, { 0x0E, "BACKSPACE" }, { 0x0F, "TAB" }, { 0x1C, "RETURN" }, { 0x1D, "LEFT CTRL" },
    { 0x2A, "LEFT SHIFT" }, { 0x36, "RIGHT SHIFT" }, { 0x38, "LEFT ALT" }, { 0x39, "SPACE" },
    { 0x9C, "PAD ENTER" }, { 0x9D, "RIGHT CTRL" }, { 0xB8, "RIGHT ALT" }, { 0xC7, "HOME" },
    { 0xC8, "UP ARROW" }, { 0xC9, "PAGE UP" }, { 0xCB, "LEFT ARROW" }, { 0xCD, "RIGHT ARROW" },
    { 0xCF, "END" }, { 0xD0, "DOWN ARROW" }, { 0xD1, "PAGE DOWN" }, { 0xD2, "INSERT" }, { 0xD3, "DELETE" },
    { 0x0C, "-" }, { 0x0D, "=" }, { 0x1A, "[" }, { 0x1B, "]" }, { 0x27, ";" }, { 0x28, "'" }, { 0x29, "`" },
    { 0x2B, "\\" }, { 0x33, "," }, { 0x34, "." }, { 0x35, "/" },
};
static const char k_row1[] = "1234567890", k_rowq[] = "QWERTYUIOP", k_rowa[] = "ASDFGHJKL", k_rowz[] = "ZXCVBNM";
static const char *const k_pads[16] = {
    "PAD A", "PAD B", "PAD X", "PAD Y", "PAD LB", "PAD RB", "PAD VIEW", "PAD MENU", "PAD L3", "PAD R3",
    "PAD LT", "PAD RT", "PAD UP", "PAD RIGHT", "PAD DOWN", "PAD LEFT",
};

const char *rm_bind_label(int kind, int code, char *buf, int n)
{
    unsigned i;
    if (kind == RM_BIND_PAD)
        snprintf(buf, (size_t)n, "%s", code >= 0 && code < 16 ? k_pads[code] : "-");
    else if (code >= 0x02 && code <= 0x0B)
        snprintf(buf, (size_t)n, "%c", k_row1[code - 0x02]);
    else if (code >= 0x10 && code <= 0x19)
        snprintf(buf, (size_t)n, "%c", k_rowq[code - 0x10]);
    else if (code >= 0x1E && code <= 0x26)
        snprintf(buf, (size_t)n, "%c", k_rowa[code - 0x1E]);
    else if (code >= 0x2C && code <= 0x32)
        snprintf(buf, (size_t)n, "%c", k_rowz[code - 0x2C]);
    else if (code >= 0x3B && code <= 0x44)
        snprintf(buf, (size_t)n, "F%d", code - 0x3A);
    else {
        snprintf(buf, (size_t)n, code ? "KEY %d" : "-", code);
        for (i = 0; i < sizeof k_keys / sizeof k_keys[0]; i++)
            if (k_keys[i].dik == code)
                snprintf(buf, (size_t)n, "%s", k_keys[i].name);
    }
    return buf;
}

/* ---- reading ------------------------------------------------------------------------ */
static int held(int player, int act, const unsigned char *dik, const host_pad *pad)
{
    int k = rm_bind(player, act, RM_BIND_KEY), b = rm_bind(player, act, RM_BIND_PAD);
    return (dik && k > 0 && dik[k]) || (pad && b >= 0 && (pad->buttons >> b) & 1);
}

static int stick(float v)                    /* -1..1 to the pad's -80..80, past a dead zone */
{
    const float dz = 0.08f;
    if (v > -dz && v < dz)
        return 0;
    v = v > 0 ? (v - dz) / (1 - dz) : (v + dz) / (1 - dz);
    if (v > 1)
        v = 1;
    if (v < -1)
        v = -1;
    return (int)(v * 80.0f);
}

void rm_player_drive(int player, const unsigned char *dik, const host_pad *pad, unsigned short *buttons,
                     int *sx, int *sy)
{
    unsigned short b = 0;
    int x = pad ? stick(pad->x) : 0, y = 0;
    if (held(player, RM_ACT_ACCEL, dik, pad))
        b |= RM_N64_A | RM_N64_DU;
    if (held(player, RM_ACT_REVERSE, dik, pad)) {
        b |= RM_N64_A | RM_N64_DD;
        y = -80;
    }
    if (held(player, RM_ACT_BRAKE, dik, pad))
        b |= RM_N64_B;
    if (held(player, RM_ACT_LEFT, dik, pad)) {
        b |= RM_N64_DL;
        x = -80;
    }
    if (held(player, RM_ACT_RIGHT, dik, pad)) {
        b |= RM_N64_DR;
        x = (b & RM_N64_DL) ? 0 : 80;
    }
    if (held(player, RM_ACT_GEAR_UP, dik, pad))
        b |= RM_N64_R;
    if (held(player, RM_ACT_GEAR_DOWN, dik, pad))
        b |= RM_N64_L;
    if (held(player, RM_ACT_VIEW, dik, pad))
        b |= RM_N64_CU;
    *buttons = b;
    *sx = x;
    *sy = y;
}

unsigned rm_player_actions(int player, const unsigned char *dik, const host_pad *pad)
{
    unsigned m = 0;
    int a, x = pad ? stick(pad->x) : 0;
    for (a = 0; a < RM_ACT_COUNT; a++)
        if (held(player, a, dik, pad))
            m |= 1u << a;
    if (x >= 60)
        m |= 1u << RM_ACT_RIGHT;
    if (x <= -60)
        m |= 1u << RM_ACT_LEFT;
    return m;
}

void rm_player_menu(int player, const unsigned char *dik, const host_pad *pad, unsigned short *buttons,
                    int *sx, int *sy)
{
    static const struct { int act; unsigned short b; } k_btn[] = {
        { RM_ACT_ACCEL, RM_N64_A }, { RM_ACT_BRAKE, RM_N64_B }, { RM_ACT_GEAR_UP, RM_N64_R },
        { RM_ACT_GEAR_DOWN, RM_N64_L }, { RM_ACT_VIEW, RM_N64_CU }, { RM_ACT_START, RM_N64_START },
        { RM_ACT_Z, RM_N64_Z }, { RM_ACT_C_DOWN, RM_N64_CD }, { RM_ACT_C_LEFT, RM_N64_CL },
        { RM_ACT_C_RIGHT, RM_N64_CR },
    };
    unsigned short b = 0;
    int x = pad ? stick(pad->x) : 0, y = pad ? -stick(pad->y) : 0;
    unsigned i;
    for (i = 0; i < sizeof k_btn / sizeof k_btn[0]; i++)
        if (held(player, k_btn[i].act, dik, pad))
            b |= k_btn[i].b;
    if (held(player, RM_ACT_LEFT, dik, pad))
        x = -80;
    if (held(player, RM_ACT_RIGHT, dik, pad))
        x = 80;
    if (held(player, RM_ACT_UP, dik, pad))
        y = 80;
    if (held(player, RM_ACT_DOWN, dik, pad))
        y = -80;
    *buttons = b;
    *sx = x;
    *sy = y;
}
