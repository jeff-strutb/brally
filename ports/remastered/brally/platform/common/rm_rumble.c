/* rm_rumble.c: Top Gear Rally's Rumble Pak in Boss Rally's races (Top Gear
 * Rally Remastered: its own races and Boss Rally's menus' alike).
 *
 * Top Gear Rally shakes each player's Rumble Pak in pulses.  An effect is so
 * many ticks on, so many off, for so many ticks in all, started by the car's
 * sound step (src/tgrally/audio/sndcar.c, BrSndCarStep) for its three knocks
 * and its surface impacts, and run by BrRumbleUpdate (src/tgrally/controls/
 * rumble.c) at the end of each pass of the mixer thread, which a 10 ms timer
 * wakes (BrMusicInit): the ticks are hundredths of a second, whatever the
 * frame rate.
 *
 * Boss Rally's sound step is Top Gear Rally's carried to the PC.  It still
 * starts the surface impacts' effects into the same six per-player shorts
 * (DAT_117a5f08..1c: running, motor on, ticks on, ticks off, ticks left in
 * the phase, ticks left in the effect), but nothing in the PC game runs them,
 * and its three knocks went to a force-feedback wheel instead.  The knocks are
 * the same bytes on the car, written by the same events with the same values
 * (128 and up):
 *
 *   Top Gear Rally        Boss Rally    written by
 *   sndHitA  car+0x346    car+0x362     two cars colliding (collresp.c / br_carcol.c)
 *   sndHitB  car+0x347    car+0x363     the body's hardest hit (body+0x1FF)
 *   sndHitC  car+0x34B    car+0x36C     a wheel landing (body+0x203 / +0x208)
 *   sndImpact car+0x34C   car+0x36D     the surface under the wheels
 *
 * So, here:
 *   rm_rumble_knocks  Top Gear Rally's three knock arms, in Boss Rally's sound
 *                     step where its own knocks are played.  Top Gear Rally's
 *                     knocks also took over the car's surface sound (state 1..3,
 *                     priority 100/90/80); Boss Rally plays them as one-shots
 *                     and its surface state never holds a knock, so the arms
 *                     read its state and keep their priorities for the frame
 *                     only, the car's sound left as Boss Rally plays it
 *   rm_rumble_frame   what BrRumbleUpdate reads of the game, from the game's
 *                     thread: the race step running and not the season's
 *                     ceremony (mode 5, as Top Gear Rally's), the pause, and
 *                     each player's car not played back from a recording (Top
 *                     Gear Rally: the pad's ghost; Boss Rally: the cases
 *                     BrCarWheelSteerStep applies a replay record)
 *   the pulse thread  BrRumbleUpdate every 10 ms, the Rumble Pak a player's
 *                     own pad (controls.h: the host's i-th), host_pad_rumble.
 *                     Top Gear Rally's rumble-off while a Controller Pak is
 *                     read (D_802A4BE8) has no counterpart: there is no pak
 *
 * BR_RUMBLE_LOG=1 (a check, with host/null's pads that log their motors): each
 * effect started, on stderr, with the time in milliseconds.
 *
 * Built with the core's flags, for its types. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "br_addr32.h"
#include "br_coretypes.h"
#include "slice3_41.h"       /* BrDriverCar, g_aBrRaceCar */
#include "br_race.h"         /* g_brRaceRules, g_brRaceNEntrant */
#include "br_gamestep.h"     /* g_pfnStep */
#include "rm_rumble.h"

typedef struct host_mutex host_mutex;   /* host.h (not includable beside the core's Win32 surface) */
typedef struct host_thread host_thread;
host_mutex  *host_mutex_new(void);
void         host_mutex_lock(host_mutex *m);
void         host_mutex_unlock(host_mutex *m);
host_thread *host_thread_start(void *(*fn)(void *), void *arg);
uint64_t     host_ticks_ns(void);
void         host_sleep_ms(uint32_t ms);
void         host_thread_interactive(void);
int          host_pad_can_rumble(int i);
int          host_pad_rumble(int i, int on);

#define TICK_NS   10000000u      /* the mixer timer's period (BrMusicInit: 10000 us) */

/* Top Gear Rally's names for Boss Rally's six shorts */
#define RUNNING   DAT_117a5f08   /* D_802A4BEC */
#define MOTOR     DAT_117a5f0c   /* D_802A4BF0 */
#define TICKS_ON  DAT_117a5f10   /* D_802A4BF4 */
#define TICKS_OFF DAT_117a5f14   /* D_802A4BF8 */
#define PHASE     DAT_117a5f18   /* D_802A4BFC */
#define LEFT      DAT_117a5f1c   /* D_802A4C00 */

static host_mutex *s_m;
static int s_race;               /* the race step runs, not the ceremony (game thread's) */
static int s_paused;
static int s_live[2];            /* the player's car is their own, not a recording's */
static int s_pak[2];             /* the player's pad can rumble (D_8031B1E8) */
static int s_on[2];              /* what the motor was last told */

static void init(void)
{
    if (!s_m)
        s_m = host_mutex_new();
}

void rm_rumble_lock(void)
{
    init();
    host_mutex_lock(s_m);
}

void rm_rumble_unlock(void) { host_mutex_unlock(s_m); }

/* ---- the effects -------------------------------------------------------------------- */
static void start(int slot, int on, int off, int len)
{
    if (slot < g_brRaceNEntrant) {
        PHASE[slot] = 0;
        MOTOR[slot] = 0;
        TICKS_ON[slot] = (short)on;
        TICKS_OFF[slot] = (short)off;
        LEFT[slot] = (short)len;
        RUNNING[slot] = 1;
        if (getenv("BR_RUMBLE_LOG"))
            fprintf(stderr, "RUMBLE %d start %d %d %d %.1f\n", slot, on, off, len, host_ticks_ns() / 1e6);
    }
}

void rm_rumble_knocks(BrDriverCar *pCar)
{
    int slot = pCar->f140;
    int level = pCar->f0F6C, prio = pCar->f0F70;
    int a = *(unsigned char *)&pCar->aBody[0].f01FE;
    int b = *(unsigned char *)&pCar->aBody[0].f01FF;
    int c = *(unsigned char *)&pCar->aBody[0].f0208;
    if (level < a || (a != 0 && prio < 100)) {
        level = a;
        prio = 100;
        start(slot, 5, 1, 30);
    }
    if (level < b || (b != 0 && prio < 90)) {
        level = b;
        prio = 90;
        start(slot, 100, 1, 30);
    }
    if (level < c || (c != 0 && prio < 80))
        start(slot, 3, 2, 20);
}

/* ---- the game's state, for the pulses ------------------------------------------------ */
/* a car the race step drives from a recording (BrCarWheelSteerStep's replay arms) */
static int played_back(BrDriverCar *c)
{
    return DAT_105ccb68[8] != 0 ||
           (g_brRaceRules.mode == 2 && c->f140 == 1 && c->pCtl->pHdr != 0) ||
           (g_brRaceRules.mode == 4 && c->f140 == 0 && c->pCtl->pHdr != 0);
}

static void *pulses(void *arg);

void rm_rumble_frame(void)
{
    static int started;
    int race = g_pfnStep == (BrGameStepFn)BrRaceStep && g_brRaceRules.mode != 5;
    int live[2] = { 0, 0 }, i;
    if (race)
        for (i = 0; i < g_BrCarCount; i++)
            if (g_aBrRaceCar[i].f140 >= 0 && g_aBrRaceCar[i].f140 < 2)
                live[g_aBrRaceCar[i].f140] = !played_back(&g_aBrRaceCar[i]);
    rm_rumble_lock();
    s_race = race;
    s_paused = g_BrX06909B4 != 0;
    s_live[0] = live[0];
    s_live[1] = live[1];
    rm_rumble_unlock();
    if (!started) {
        started = 1;
        host_thread_start(pulses, NULL);
    }
}

/* ---- the pulse thread: BrRumbleUpdate --------------------------------------------------- */
static void motor(int j, int on)
{
    if (s_on[j] != on && host_pad_rumble(j, on))
        s_on[j] = on;
}

/* the pads looked for (osMotorInit's part): one gone forgets its motor, so the
   first start after it is back is sent */
static int find(int i)
{
    s_pak[i] = host_pad_can_rumble(i);
    if (!s_pak[i])
        s_on[i] = 0;
    return s_pak[i];
}

static void all_off(void)
{
    host_pad_rumble(0, 0);
    host_pad_rumble(1, 0);
}

static void tick(void)
{
    static int count;
    int race, paused, live[2], i, j;
    rm_rumble_lock();
    race = s_race;
    paused = s_paused;
    live[0] = s_live[0];
    live[1] = s_live[1];
    rm_rumble_unlock();
    /* out of a race, or paused: no effect, the motors stopped, the pads looked for */
    if (!race || paused) {
        for (i = 0; i != 2; i++) {
            rm_rumble_lock();
            RUNNING[i] = 0;
            rm_rumble_unlock();
            if (find(i))
                motor(i, 0);
        }
    }
    if (!paused) {
        for (j = 0; j != 2; j++) {
            int to = -1;             /* the motor: -1 as it is */
            rm_rumble_lock();
            if (s_pak[j] && RUNNING[j] != 0 && live[j]) {
                if (PHASE[j] == 0) {
                    if (MOTOR[j] != 0) {
                        PHASE[j] = TICKS_OFF[j];
                        to = 0;
                    } else {
                        PHASE[j] = TICKS_ON[j];
                        to = 1;
                    }
                    MOTOR[j] ^= 1;
                } else {
                    PHASE[j]--;
                }
                if (LEFT[j] == 0) {
                    to = 0;
                    MOTOR[j] = 0;
                    PHASE[j] = 0;
                    RUNNING[j] = 0;
                } else {
                    LEFT[j]--;
                }
            }
            rm_rumble_unlock();
            if (to >= 0)
                motor(j, to);
        }
    }
    /* every 64 ticks the pads are looked for again */
    count = (count + 1) & 0x3f;
    if (count == 0)
        for (i = 0; i != 2; i++)
            find(i);
}

static void *pulses(void *arg)
{
    uint64_t next = host_ticks_ns() + TICK_NS, now;
    (void)arg;
    host_thread_interactive();   /* it keeps time: woken on time, as the mixer's timer */
    atexit(all_off);
    for (;;) {
        now = host_ticks_ns();
        if (now < next) {
            host_sleep_ms((uint32_t)((next - now + 999999u) / 1000000u));
            continue;
        }
        if (now - next > 10u * TICK_NS)
            next = now;              /* far behind (the machine slept): from now */
        next += TICK_NS;
        tick();
    }
    return NULL;
}
