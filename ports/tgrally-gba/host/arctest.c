/* arctest.c -- gba/arcade.c on the host, on the race's track (race_data.c): a drive from the
 * grid by a script of key changes ("FRAME:KEYS,..." as gbarun's, keys as the GBA's buttons
 * held: 1 A, 2 B, 16 right, 32 left), the car's place and speed each half second.
 *   arctest SCRIPT [FRAMES] */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../gba/world.h"
#include "../gba/race.h"
#include "../sim/simload.h"

extern volatile int32_t g_arc[4];
static Car s_car;
static Pad s_pad;
const Cell *g_cells_dummy;

int main(int argc, char **argv)
{
    char *spec = argc > 1 ? argv[1] : "";
    int frames = argc > 2 ? atoi(argv[2]) : 1800, f, keys = 0, i;
    fx view[3];
    g_track = &g_rt_track;
    sl_car_load(&s_car, &s_pad, g_rt_cars[0], g_rt_pads[0], 0);
    for (i = 0; i < 3; i++)
        view[i] = g_rt_camView[0][i];
    arc_init(s_car.body.m, view);
    for (f = 0; f < frames; f++) {
        char *p = spec;
        while (*p) {                                  /* the script's change at this frame */
            int at = atoi(p), k;
            char *c = strchr(p, ':');
            if (!c)
                break;
            k = atoi(c + 1);
            if (at == f)
                keys = k;
            p = strchr(c, ',');
            if (!p)
                break;
            p++;
        }
        arc_tick(~(uint32_t)keys & 0x3FF);
        if (f % 15 == 0)
            printf("%5.1f s  x %7.1f y %7.1f z %6.2f  %3d mph\n", f / 30.0, g_arc[0] / 16.0, g_arc[1] / 16.0,
                   g_arc[2] / 16.0, g_arc[3]);
    }
    return 0;
}
