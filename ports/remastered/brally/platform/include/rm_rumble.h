/* rm_rumble.h: Top Gear Rally's Rumble Pak in Boss Rally's races (rm_rumble.c). */
#ifndef RM_RUMBLE_H
#define RM_RUMBLE_H
#ifdef __cplusplus
extern "C" {
#endif
struct BrDriverCar;
/* the sound step (BrSndCarStep), after its surface reset and before the knocks
   are played and cleared: Top Gear Rally's three knock effects for the car */
void rm_rumble_knocks(struct BrDriverCar *pCar);
/* the race step's sound pass holds this over its cars: the effects it starts
   and the pulse thread's own are the same six shorts */
void rm_rumble_lock(void);
void rm_rumble_unlock(void);
/* each frame of the game thread (plat_race_frame): whether the pads may run,
   which players' cars are their own */
void rm_rumble_frame(void);
#ifdef __cplusplus
}
#endif
#endif
