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
void race_view(void *frame);             /* the viewed car's camera as the renderer's Frame */
const uint16_t *race_cells(const void *frame);   /* the cells in view, as render_visible's list */
#endif
