/* flags.c: the behaviour flags and their profiles (br_flags.h). */
#include <stdlib.h>
#include <string.h>

#include "plat.h"
#include "br_flags.h"

enum { PROFILE_ORIGINAL, PROFILE_REMASTERED, PROFILE_COUNT };

static const struct {
    const char *name;                   /* BR_FLAG_<name> overrides it */
    unsigned char value[PROFILE_COUNT]; /* original, remastered */
} k_flags[BR_FLAG_COUNT] = {
    [BR_FLAG_ANY_ASPECT]        = { "ANY_ASPECT",        { 0, 1 } },
    [BR_FLAG_MENU_MUSIC_RESUME] = { "MENU_MUSIC_RESUME", { 0, 1 } },
};

static const char *const k_profiles[PROFILE_COUNT] = { "original", "remastered" };

static int s_profile = -1;
static unsigned char s_value[BR_FLAG_COUNT];

static void load(void)
{
    const char *p = getenv("BR_PROFILE");
    char name[64];
    int i;
    s_profile = PROFILE_REMASTERED;
    if (p && strcmp(p, "original") == 0)
        s_profile = PROFILE_ORIGINAL;
    for (i = 0; i < BR_FLAG_COUNT; i++) {
        const char *e;
        snprintf(name, sizeof name, "BR_FLAG_%s", k_flags[i].name);
        e = getenv(name);
        s_value[i] = e ? atoi(e) != 0 : k_flags[i].value[s_profile];
        PLOG("flags: %s %s\n", k_flags[i].name, s_value[i] ? "on" : "off");
    }
    PLOG("flags: profile %s\n", k_profiles[s_profile]);
}

int plat_flag(int flag)
{
    if (s_profile < 0)
        load();
    return flag >= 0 && flag < BR_FLAG_COUNT ? s_value[flag] : 0;
}

const char *plat_profile(void)
{
    if (s_profile < 0)
        load();
    return k_profiles[s_profile];
}
