/* rmcheat.c (Remastered): Europe's and Japan's main-menu cheat, in both Top Gear
 * Rally libraries, on the first player's own controls (ports/remastered/platform/
 * controls.h).  The cartridges watch the pad: L and R held, C-right pressed three
 * times (letting go of L and R starts the count again), and every car, track and
 * weather opens (src/menus/mainmenu.c).  Remastered watches the actions that play
 * those buttons' parts: Gear Down and Gear Up held, Change View pressed three times. */
#include <stddef.h>

#include "host.h"

const unsigned char *tgr_input_dik(void);   /* si.c: the keys held */
unsigned rm_player_actions(int player, const unsigned char *dik, const host_pad *pad);   /* the app's */
enum { GEAR_UP = 1 << 5, GEAR_DOWN = 1 << 6, VIEW = 1 << 7 };   /* controls.h's RM_ACT_* bits */

/* one main-menu frame: 1 when the third press lands */
int rm_cheat_unlock(void)
{
    static int bangs, was = 1;
    host_pad p;
    int has = host_pad_read_n(0, &p);
    unsigned m = rm_player_actions(0, tgr_input_dik(), has ? &p : NULL);
    int now = (m & VIEW) != 0, hit = 0;
    if ((m & (GEAR_UP | GEAR_DOWN)) == (GEAR_UP | GEAR_DOWN)) {
        if (now && !was && bangs < 3 && ++bangs == 3)
            hit = 1;
    } else {
        bangs = 0;
    }
    was = now;
    return hit;
}
