/* flags.c: Remastered's switches in Boss Rally (br_flags.h). */
#include <stdlib.h>
#include <string.h>

#include "plat.h"
#include "br_flags.h"

static const char *const k_names[BR_FLAG_COUNT] = {   /* BR_FLAG_<name>=0|1 sets it at start */
    [BR_FLAG_ANY_ASPECT] = "ANY_ASPECT",
    [BR_FLAG_VIEW_UNLIMITED] = "VIEW_UNLIMITED",
};

/* on until the app sets them */
static const unsigned char k_def[BR_FLAG_COUNT] = { [BR_FLAG_ANY_ASPECT] = 1, [BR_FLAG_VIEW_UNLIMITED] = 1 };

static int s_loaded;
static volatile unsigned char s_value[BR_FLAG_COUNT];

static void load(void)
{
    char name[64];
    int i;
    for (i = 0; i < BR_FLAG_COUNT; i++) {
        const char *e;
        snprintf(name, sizeof name, "BR_FLAG_%s", k_names[i]);
        e = getenv(name);
        s_value[i] = e ? atoi(e) != 0 : k_def[i];
        PLOG("flags: %s %s\n", k_names[i], s_value[i] ? "on" : "off");
    }
    s_loaded = 1;
}

void br_flag_set(int flag, int on)
{
    if (!s_loaded)
        load();
    if (flag >= 0 && flag < BR_FLAG_COUNT)
        s_value[flag] = on != 0;
}

int plat_flag(int flag)
{
    if (!s_loaded)
        load();
    return flag >= 0 && flag < BR_FLAG_COUNT ? s_value[flag] : 0;
}
