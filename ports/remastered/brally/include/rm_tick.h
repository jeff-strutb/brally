/* rm_tick.h: Top Gear Rally Remastered runs Boss Rally's races at 60 ticks a
 * second where the game ran 30 (race_handoff.c; RM_TICK=30 the game's own).
 * What the game wrote in terms of its 30 a second goes through these, each the
 * game's own value exactly at 30:
 *   RM_TICKS(n)     n of the game's ticks, in ticks (a count, a timer)
 *   RM_PER_TICK(k)  a fraction taken every tick (a smoothing, a decay):
 *                   1 - (1 - k)^(30 / rate), the same fraction a second
 *   RM_STEP(x)      an amount added every tick: x * 30 / rate
 *   RM_MUL(s)       a factor applied every tick: s^(30 / rate)
 *   RM_DT           the tick's length, seconds (the game's 1/30)
 *   rm_tick_whole() 1 on the ticks that begin one of the game's (every tick at
 *                   30, every other at 60): a decision the game made once a
 *                   tick that must not come twice as often */
#ifndef RM_TICK_H
#define RM_TICK_H
#ifdef __cplusplus
extern "C" {
#endif
int   rm_tick_hz(void);
float rm_tick_dt(void);
float rm_per_tick(float k);
float rm_step(float x);
float rm_per_tick_mul(float s);
int   rm_tick_whole(void);
void  rm_replay_slot_hz(int slot, int hz);   /* br_replay.c: a slot's recording rate */
#ifdef __cplusplus
}
#endif
#define RM_TICKS(n)     ((n) * rm_tick_hz() / 30)
#define RM_TICKS30(n)   ((n) * 30 / rm_tick_hz())   /* n ticks, in the game's (the other way) */
#define RM_PER_TICK(k)  (rm_per_tick(k))
#define RM_STEP(x)      (rm_step(x))
#define RM_MUL(s)       (rm_per_tick_mul(s))
/* the share kept, of an every-tick blend that takes `take` (the game's `keep`
   exactly at 30, where keep is 1 - take as written) */
#define RM_KEEP(keep, take) (rm_tick_hz() == 30 ? (keep) : 1.0f - rm_per_tick(take))
#define RM_DT           (rm_tick_dt())
/* a ghost or replay file Remastered writes at 60 frames a second (the game's say "RGho") */
#define RM_GHOST_MAGIC60 "RG60"
#endif
