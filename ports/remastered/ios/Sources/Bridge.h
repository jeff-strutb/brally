/* Bridge.h: what the app's Swift sees of the games and the platform layer.
 * The games are libgames.a (ports/remastered/link.sh, HOST=ios); the Swift
 * files are the host under them (ports/brally/platform/host/host.h), shared
 * between the two by the multiplexer (ports/remastered/platform/mux.c). */
#include "host.h"
#include "host_race.h"                 /* BR_KEY_* */
#include "tgr_touch.h"                 /* the menus by touch (platform/os/touch.c) */
#include "mux.h"                       /* which game is in front */

/* the process (ports/remastered/platform/main.c, built as rm_main): run on its own thread */
int rm_main(int argc, char **argv);

/* Top Gear Rally */
/* 1 while a race is driven: touch is the wheel; else menu swipes and taps */
int tgr_view_driving(void);
/* the sound's low-pass cut, Hz (0: none; platform/audio/out.c) */
void tgr_audio_lowpass(int hz);
/* the player's speed, km/h, negative backing up (src/racing/handoff.c) */
float tgr_race_speed(void);

/* Boss Rally, running a race (platform/common/race_handoff.c) */
int  br_race_driving(void);
void br_race_key(int key);              /* BR_KEY_*: its pause menu */
