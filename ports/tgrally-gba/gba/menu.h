/* menu.h -- the main menu's data (tools/menu.py writes menu_data.c) */
#ifndef MENU_H
#define MENU_H
#include <stdint.h>
#include "world.h"
#include "hud.h"
#include "sound.h"

typedef struct { int16_t x, y, z; int8_t nx, ny, nz, pad; } MVert;   /* model units; the normal, 127 long */
enum { MT_TWO = 1, MT_BLEND = 2, MT_FIXED = 4 };
typedef struct {
    uint16_t a, b, c;             /* its vertices, in the display list's order */
    uint8_t flags, grey;          /* MT_..; a fixed-light part's level (D_80271D9C) */
    uint16_t slot, pad;           /* its texture at each brightness level: g_menu_slot[slot] */
    int16_t uv[6];                /* u, v at a, b, c: texels, Q4 */
} MTri;
typedef struct {
    const MVert *v; int nv;
    const MTri *t; int nt;
} MModel;
typedef struct { const char *label; int flags; } MenuRow;
typedef struct {
    int16_t x, y;                 /* where on the GBA's screen */
    uint16_t tile, bank;          /* its sprite: first tile (from 512), palette bank */
} MenuSprite;
typedef struct {
    int32_t l[3];                 /* the directional light's way, Q12 */
    int32_t dir, amb;             /* its level and the ambient's, 0..255 */
} MenuLight;

extern const MModel g_menu_icon[7];
extern const MenuRow g_menu_row[7];
extern const uint16_t g_menu_slot[][8];
extern const Tex g_menu_tex[];
extern const int32_t g_menu_cam[5][3];       /* model x, y, z (spun), the icon's x, 1: to X, Y, W (Q12) */
extern const MenuLight g_menu_light;
extern const int g_menu_front;               /* the screen winding (GBA, y down) of a front face: 1 or -1 */
extern const uint16_t g_menu_backdrop[160 * 128];
extern const TextSet g_menu_tset;
extern const uint16_t g_menu_pal[256];
extern const MenuSprite g_menu_btn[2];
extern const uint8_t g_menu_btn_tiles[];
extern const int g_menu_btn_bytes;
extern const SndSample g_menu_move, g_menu_choose;
extern const int g_menu_sfx_rate, g_menu_sfx_level;
extern const Song g_menu_song;
#endif
