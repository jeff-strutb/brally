/* light_game.c: what the lighting reads of the game's state (DESIGN.md
 * section 2.3), read-only.  Built with the core's flags (link64.sh), for its
 * types and globals; light.h is the plain interface the rest of the lighting
 * sees. */
#include <string.h>
#include "br_addr32.h"
#include "br_coretypes.h"
#include "br_mat.h"
#include "light.h"

/* the camera the display list draws through: its view x projection (the
 * "projection" slot BrGbiMatrix fills, row vectors) and the viewport that
 * takes clip space to the game's pixels: x = cx / cw * sx + ox, y the same
 * (br_dltrim.c, br_dlproj.c) */
void light_game_camera(LightCamera *c)
{
    memcpy(c->vp, &DAT_105ccd00, sizeof c->vp);
    c->sx = DAT_105ccd48;
    c->ox = DAT_105cd9f8;
    c->sy = DAT_105ccfdc;
    c->oy = DAT_105cd9fc;
}
