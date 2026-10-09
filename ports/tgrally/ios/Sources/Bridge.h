/* Bridge.h: what the app's Swift sees of the game and its platform layer.
 * The game is libtgrally.a (ports/tgrally/link.sh, HOST=ios); the Swift
 * files are its host (ports/brally/platform/host/host.h). */
#include "host.h"
#include "tgr_touch.h"                 /* the menus by touch (platform/os/touch.c) */
#include "host_race.h"                 /* a race run by the other game */

/* the platform's main() (os/main.c, built as tgr_main): run on its own thread */
int tgr_main(int argc, char **argv);
/* 1 while a race is driven: touch is the wheel; else menu swipes and taps */
int tgr_view_driving(void);
/* the sound's low-pass cut, Hz (0: none; platform/audio/out.c) */
void tgr_audio_lowpass(int hz);
/* the engine that runs the races (src/racing/handoff.c) */
void tgr_race_engine(host_race_fn fn);
/* the player's speed, km/h, negative backing up (src/racing/handoff.c) */
float tgr_race_speed(void);

/* Boss Rally (ports/brally/platform/common/main.c, built as br_main; the
 * races run for Top Gear Rally's menus: platform/common/race_handoff.c) */
int  br_main(int argc, char **argv);
void br_race_serve(void);
int  br_race_run(const HostRace *race, HostRaceResult *res);
int  br_race_driving(void);
void br_race_key(int key);              /* BR_KEY_* */
