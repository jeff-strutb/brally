/* mux.h: the two games under one host (mux.c). */
#ifndef MUX_H
#define MUX_H

enum { MUX_TGR, MUX_BR, MUX_COUNT };

/* the game the player sees, hears and drives from now on */
void mux_front(int game);
int  mux_front_game(void);

#endif
