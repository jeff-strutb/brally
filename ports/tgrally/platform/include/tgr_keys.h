/* tgr_keys.h: the keys and game pad buttons that are the N64 pad's (si.c),
 * as the options screen's Keys and Pad sets them (src/menus/keysmenu.c).
 * An input is one of the pad's buttons (A, B, Z, Start, L, R, the C buttons,
 * the d-pad) or one of its stick's four ways. */
#ifndef TGR_KEYS_H
#define TGR_KEYS_H

#define TGR_KEYS_COUNT 18

int  tgr_keys_shown(void);              /* 0 in a scripted run: the cartridge's screens alone */
const char *tgr_keys_name(int in);      /* "C Up" */
/* its keys ("X, RETURN") and pad buttons ("PAD A"), "none" for none */
void tgr_keys_labels(int in, char *keys, int nk, char *pad, int np);
void tgr_keys_bind(int in, int kind, int code);   /* kind 0 a key (a virtual key), 1 a pad button */
void tgr_keys_reset(void);              /* the defaults, kept */
/* wait for the next key or pad button pressed for an input (Escape: as it
 * was); while waiting, and until what ended it is let go, the game sees
 * nothing pressed */
void tgr_keys_capture(int in);
int  tgr_keys_capturing(void);          /* the input waited for, -1 none */

#endif
