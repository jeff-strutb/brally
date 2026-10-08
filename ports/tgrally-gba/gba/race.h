/* race.h -- the race the GBA plays: the game's simulation (sim/) on tools/race.py's data */
#ifndef RACE_H
#define RACE_H
#include "../sim/sim.h"

extern const Track g_rt_track;
extern const fx g_rt_grip[72];
extern const uint8_t g_rt_cars[2][0x2090], g_rt_pads[2][0x15C];
extern const fx g_rt_camView[2][3];
extern const fx g_rt_lens;

void race_start(void);
void race_tick(uint32_t keys);
void race_phase_code(int k);             /* gba/main.c: phase k's code into IWRAM (its overlay) */           /* a game frame (1/30 s) of the cars, from the pad (KEYINPUT, active low) */
struct Frame;
void race_view(void *frame, int alpha);  /* the viewed car's camera as the renderer's Frame, alpha (0..256)
                                            of the way from the last tick but one to the last */
#define RACE_TICKS 1                     /* the game's ticks (1/30 s) in each of the GBA's */
void arc_init(const fx m[4][4], const fx view[3]);   /* arcade.c: the GBA's car physics */
void arc_tick(uint32_t keys);
void arc_camera(fx out[17], fx lens);
fx arc_speed_mph(void);
const uint16_t *race_cells(const void *frame);   /* the cells in view, as render_visible's list */
#endif
