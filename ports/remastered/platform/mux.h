/* mux.h: the games under one host (mux.c): Top Gear Rally from the USA
 * cartridge (it owns the window), Boss Rally, and Top Gear Rally from
 * Japan's (the Menus TGR (PAL/Japan)). */
#ifndef MUX_H
#define MUX_H

enum { MUX_TGR, MUX_BR, MUX_TGRP, MUX_COUNT };

/* the Top Gear Rally whose menus the player is in (MUX_TGR or MUX_TGRP):
 * its sound is heard, the other's is not */
void mux_tgr_active(int game);
int  mux_tgr_active_game(void);

/* the game the player sees, hears and drives from now on */
void mux_front(int game);
int  mux_front_game(void);

/* the music the Remaster Options chose (options.h RM_MUSIC_*): crossfaded to */
void mux_music(int which);

#endif
