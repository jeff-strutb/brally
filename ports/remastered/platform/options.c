/* options.c: the Remaster Options (options.h).  Kept as text in the app's save
 * folder, "remaster.cfg", one "name value" a line; a missing file or line is
 * the default (widescreen on, the N64's music, the Remastered lighting, no
 * frame rate shown).  Each game reads the frame rate's switch itself
 * (rm_option, from its own drawing). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "host.h"
#include "options.h"
#include "mux.h"
#include "tgr_view.h"                   /* Top Gear Rally's switches (its object's tgr_flag_set) */
#include "br_flags.h"                   /* Boss Rally's (its object's br_flag_set) */

static const struct { const char *name; int def; } k_opt[RM_OPT_COUNT] = {
    [RM_OPT_WIDESCREEN] = { "widescreen", 1 },
    [RM_OPT_MUSIC]      = { "music", RM_MUSIC_N64 },
    [RM_OPT_LIGHTING]   = { "lighting", 1 },
    [RM_OPT_FPS]        = { "fps", 0 },
};

static volatile int s_val[RM_OPT_COUNT];
static int s_loaded;

static void apply(int which)
{
    int v = s_val[which];
    switch (which) {
    case RM_OPT_WIDESCREEN:
        tgr_flag_set(TGR_FLAG_ANY_ASPECT, v);
        br_flag_set(BR_FLAG_ANY_ASPECT, v);
        break;
    case RM_OPT_MUSIC:
        mux_music(v);
        break;
    case RM_OPT_LIGHTING:
        br_flag_set(BR_FLAG_LIGHTING, v);
        break;
    }
}

static void path(char *out, size_t n)
{
    snprintf(out, n, "%s/remaster.cfg", host_save_dir());
}

static void save(void)
{
    char p[1100];
    FILE *f;
    int i;
    path(p, sizeof p);
    if (!(f = fopen(p, "w")))
        return;
    for (i = 0; i < RM_OPT_COUNT; i++)
        fprintf(f, "%s %d\n", k_opt[i].name, s_val[i]);
    fclose(f);
}

void rm_options_load(void)
{
    char p[1100], line[128];
    FILE *f;
    int i;
    for (i = 0; i < RM_OPT_COUNT; i++)
        s_val[i] = k_opt[i].def;
    path(p, sizeof p);
    if ((f = fopen(p, "r"))) {
        while (fgets(line, sizeof line, f)) {
            char name[64];
            int v;
            if (sscanf(line, "%63s %d", name, &v) != 2)
                continue;
            for (i = 0; i < RM_OPT_COUNT; i++)
                if (!strcmp(name, k_opt[i].name))
                    s_val[i] = i == RM_OPT_MUSIC ? (v == RM_MUSIC_PC ? RM_MUSIC_PC : RM_MUSIC_N64) : v != 0;
        }
        fclose(f);
    }
    {   /* RM_OPT_<NAME>=N for a check: that choice, not the kept one */
        char e[64];
        for (i = 0; i < RM_OPT_COUNT; i++) {
            const char *s, *n = k_opt[i].name;
            int k;
            snprintf(e, sizeof e, "RM_OPT_");
            for (k = 0; n[k] && k < 40; k++)
                e[7 + k] = (char)(n[k] - 'a' + 'A');
            e[7 + k] = 0;
            if ((s = getenv(e)))
                s_val[i] = atoi(s);
        }
    }
    s_loaded = 1;
    for (i = 0; i < RM_OPT_COUNT; i++)
        apply(i);
}

int rm_option(int which)
{
    if (!s_loaded)
        rm_options_load();
    return which >= 0 && which < RM_OPT_COUNT ? s_val[which] : 0;
}

void rm_option_set(int which, int value)
{
    if (!s_loaded)
        rm_options_load();
    if (which < 0 || which >= RM_OPT_COUNT)
        return;
    value = which == RM_OPT_MUSIC ? (value == RM_MUSIC_PC ? RM_MUSIC_PC : RM_MUSIC_N64) : value != 0;
    if (s_val[which] == value)
        return;
    s_val[which] = value;
    apply(which);
    save();
    if (getenv("RM_LOG"))
        fprintf(stderr, "remastered: option %s %d\n", k_opt[which].name, value);
}
