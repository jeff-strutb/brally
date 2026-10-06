/* script_game.c: the script commands that look into the game -- for any host.
 *
 * script.c replays input; these are the parts of the brbox scenario
 * language that need the game's own state, kept apart so the rest of the
 * platform layer stays independent of the core's types:
 *
 *   wait/waitb ADDR OP VALUE [N]  a dword/byte at an ORIGINAL address (the
 *                     32-bit lane's numbers), read through the BR_DUMP map
 *   autopilot on|off  the player's racing-line steering (brbox_drive.py)
 *   waittext/text     the strings BrTextEmitString drew last frame
 *   files NAME        tools/brally/brbox_saves/NAME copied into the save directory
 *                     before the game starts
 *
 * Semantics follow ports/brally-wasm/wasm/host/host_script.c so both lanes run
 * the same scenarios. */
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* built with the core's flags for its types, but these are host paths:
 * the real fopen, not br_crt.h's game-directory one */
#undef fopen

const char *host_save_dir(void);   /* host.h */
typedef struct host_dir host_dir;  /* host.h */
host_dir   *host_dir_open(const char *path);
const char *host_dir_next(host_dir *d, int *is_dir, uint64_t *size);
void        host_dir_close(host_dir *d);
#include "br_addr32.h"
#include "br_coretypes.h"
#include "br_cartypes.h"
#include "slice3_41.h"     /* BrDriverCar */
#include "slice3_39.h"     /* g_pBrAA2E80: the menu navigation record */

extern const struct { unsigned va; const void *p; unsigned n; } g_brDumpMap[];
extern BrDriver g_aBrRaceDriver[];

/* ---- original addresses -------------------------------------------------------- */
const void *plat_script_orig(unsigned va, unsigned n)
{
    int i;
    for (i = 0; g_brDumpMap[i].p; i++)
        if (va >= g_brDumpMap[i].va && va + n <= g_brDumpMap[i].va + g_brDumpMap[i].n)
            return (const char *)g_brDumpMap[i].p + (va - g_brDumpMap[i].va);
    return NULL;
}

/* a pointer global's value as the original's address: a core function by
 * its original address, a global by the original address of the byte it
 * points at; 0 when it is null or points anywhere else */
extern const struct { unsigned va; const void *p; } g_brPtrMap[], g_brFnMap[];

int plat_script_orig_ptr(unsigned va, unsigned *out)
{
    int i;
    const void *v;
    for (i = 0; g_brPtrMap[i].p; i++)
        if (g_brPtrMap[i].va == va)
            break;
    if (!g_brPtrMap[i].p)
        return 0;
    v = *(const void *const *)g_brPtrMap[i].p;
    *out = 0;
    if (!v)
        return 1;
    for (i = 0; g_brFnMap[i].p; i++)
        if (g_brFnMap[i].p == v) {
            *out = g_brFnMap[i].va;
            return 1;
        }
    for (i = 0; g_brDumpMap[i].p; i++)
        if ((const char *)v >= (const char *)g_brDumpMap[i].p &&
            (const char *)v < (const char *)g_brDumpMap[i].p + g_brDumpMap[i].n) {
            *out = g_brDumpMap[i].va + (unsigned)((const char *)v - (const char *)g_brDumpMap[i].p);
            return 1;
        }
    return 1;
}

/* ---- text drawn ---------------------------------------------------------------- */
#define TEXT_MAX 256
static char s_now[TEXT_MAX][96], s_last[TEXT_MAX][96];
static int s_nnow, s_nlast;

void plat_text_emit(const char *psz)
{
    if (psz && s_nnow < TEXT_MAX)
        snprintf(s_now[s_nnow++], sizeof s_now[0], "%s", psz);
}

void plat_text_swap(void)
{
    memcpy(s_last, s_now, sizeof s_now);
    s_nlast = s_nnow;
    s_nnow = 0;
}

int plat_script_text_seen(const char *want)
{
    char w[96], t[96];
    int i, k;
    snprintf(w, sizeof w, "%s", want);
    for (k = 0; w[k]; k++)
        w[k] = w[k] == '_' ? ' ' : (char)tolower((unsigned char)w[k]);
    for (i = 0; i < s_nlast; i++) {
        for (k = 0; s_last[i][k] && k < 95; k++)
            t[k] = (char)tolower((unsigned char)s_last[i][k]);
        t[k] = 0;
        if (strstr(t, w))
            return 1;
    }
    return 0;
}

void plat_script_text_log(unsigned long frame)
{
    int i;
    fprintf(stderr, "script: frame %lu text:", frame);
    for (i = 0; i < s_nlast; i++)
        fprintf(stderr, " %s |", s_last[i]);
    fprintf(stderr, "\n");
}

/* ---- autopilot ----------------------------------------------------------------- */
/* brbox_drive.py's steer(): walk the player car's racing-line cursor about
 * 12 m + 0.6 s ahead and hold UP plus LEFT or RIGHT toward that point; the
 * car's frame row 1 points to its LEFT. Stuck against something, back off
 * with the wheel reversed. */
enum { AP_UP, AP_DOWN, AP_LEFT, AP_RIGHT };
static const int k_ap_vk[4] = { 0x26, 0x28, 0x25, 0x27 }, k_ap_dik[4] = { 0xC8, 0xD0, 0xCB, 0xCD };
static int s_ap_down[4], s_ap_stuck, s_ap_reverse;

void plat_script_key(int vk, int dik, int down);   /* script.c */

void plat_script_autopilot_keys(const int *want)
{
    int k;
    for (k = 0; k < 4; k++)
        if (want[k] != s_ap_down[k]) {
            plat_script_key(k_ap_vk[k], k_ap_dik[k], want[k]);
            s_ap_down[k] = want[k];
        }
}

void plat_script_autopilot_off(void)
{
    static const int none[4];
    plat_script_autopilot_keys(none);
}

void plat_script_autopilot(void)
{
    BrDriverCar *car = g_aBrRaceDriver[0].pCar;
    int want[4] = { 1, 0, 0, 0 };
    if (car && car->pNode.p) {
        const BrAiPathNode *n = car->pNode.p, *nxt;
        int k = (int)car->iPt.v, cnt, i, guard;
        float px = car->pos.x, py = car->pos.y, rx = car->right.x, ry = car->right.y;
        float speed = hypotf(car->f1024.x, car->f1024.y), t, dx, dy, d, lat;
        t = 12.0f + speed * 0.6f;
        for (i = 0; i < 400; i++) {
            cnt = n->count;
            t -= n->aPt[k].arc - n->aPt[k + 1].arc;
            if (++k >= cnt) {
                /* the Mine layouts end the node list without looping back */
                nxt = BR_PTR32(const BrAiPathNode *, n->aNext);
                for (guard = 0; nxt && (nxt->flags & 1) && guard < 16; guard++)
                    nxt = BR_PTR32(const BrAiPathNode *, nxt->aSib);
                if (!nxt) {
                    k = cnt - 1;
                    break;
                }
                n = nxt;
                k = 0;
            }
            if (t < 0)
                break;
        }
        dx = n->aPt[k].centre.x - px;
        dy = n->aPt[k].centre.y - py;
        d = hypotf(dx, dy);
        if (d == 0)
            d = 1;
        lat = (rx * dx + ry * dy) / d;
        if (lat > 0.08f)
            want[AP_LEFT] = 1;
        else if (lat < -0.08f)
            want[AP_RIGHT] = 1;
        if (fabsf(lat) > 0.6f && speed > 25)
            want[AP_UP] = 0;
        if (speed < 2.0f)
            s_ap_stuck++;
        else
            s_ap_stuck = 0;
        if (s_ap_stuck > 45 || s_ap_reverse > 0) {
            int l = want[AP_LEFT];
            if (s_ap_reverse == 0)
                s_ap_reverse = 40;
            s_ap_reverse--;
            s_ap_stuck = 0;
            want[AP_UP] = 0;
            want[AP_DOWN] = 1;
            want[AP_LEFT] = want[AP_RIGHT];
            want[AP_RIGHT] = l;
        }
    }
    plat_script_autopilot_keys(want);
}

/* ---- fixtures ------------------------------------------------------------------ */
/* `files NAME`: copy tools/brally/brbox_saves/NAME/c/bossrally/<path> into the save
 * directory under the flattened name the file layer uses (crt.c overlay():
 * the path below the game directory, backslashes as `_`, lower case). */
static void copy_tree(const char *dir, const char *rel, const char *save)
{
    host_dir *d = host_dir_open(dir);
    const char *name;
    int is_dir;
    if (!d)
        return;
    while ((name = host_dir_next(d, &is_dir, NULL)) != NULL) {
        char p[1200], r[600], q[1300], *k;
        if (name[0] == '.')
            continue;
        snprintf(p, sizeof p, "%s/%s", dir, name);
        snprintf(r, sizeof r, "%s%s%s", rel, *rel ? "_" : "", name);
        if (is_dir) {
            copy_tree(p, r, save);
            continue;
        }
        for (k = r; *k; k++)
            *k = (char)tolower((unsigned char)*k);
        snprintf(q, sizeof q, "%s/%s", save, r);
        {
            FILE *in = fopen(p, "rb"), *out = fopen(q, "wb");
            if (in && out) {
                char buf[65536];
                size_t n;
                while ((n = fread(buf, 1, sizeof buf, in)) > 0)
                    fwrite(buf, 1, n, out);
                fprintf(stderr, "script: fixture %s -> %s\n", p, q);
            }
            if (in)
                fclose(in);
            if (out)
                fclose(out);
        }
    }
    host_dir_close(d);
}

void plat_script_files(void)
{
    const char *path = getenv("BR_SCRIPT");
    FILE *f;
    char line[512];
    if (!path || !(f = fopen(path, "r")))
        return;
    while (fgets(line, sizeof line, f)) {
        char op[32], name[256], dir[1200];
        if (sscanf(line, "%31s %255s", op, name) == 2 && !strcmp(op, "files")) {
            snprintf(dir, sizeof dir, "tools/brally/brbox_saves/%s/c/bossrally", name);
            copy_tree(dir, "", host_save_dir());
        }
    }
    fclose(f);
}

/* where the game's menu cursor is: the navigation record's x and y
 * (0x10AC61E0 points at it; 0 before it exists) */
int plat_game_cursor(int *x, int *y)
{
    const int32_t *p = (const int32_t *)(const void *)g_pBrAA2E80;
    if (!p)
        return 0;
    *x = p[0];
    *y = p[1];
    return 1;
}
