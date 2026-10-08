#ifndef HUD_H
#define HUD_H
#include <stdint.h>

/* the race HUD's data (tools/hud.py): see gba/hud.s */
typedef struct {
    uint8_t scheme, size, glyph, sw;    /* its colour set (a palette bank), size, font glyph, sprite (16 or 32) */
    uint16_t tile;                      /* first tile (32 bytes) in g_glyph_tiles */
    uint8_t gw, gh;                     /* its size on the GBA's screen */
} HudGlyph;
typedef struct {
    uint32_t prim, env, alt;            /* the printer's colours (0xRRGGBB) and combine */
} HudScheme;
typedef struct {
    int32_t x, y, w, h, lampX, lampY, needleX, needleY, rest, max;   /* N64 pixels; angles Q12 radians */
} HudDial;
typedef struct {                        /* what the HUD reads, a retrace */
    int16_t totalM, totalS, totalC;     /* BrHudTimeDraw's numbers: minutes, seconds, hundredths */
    int16_t lapM, lapS, lapC;
    int16_t laps, nlaps, pos, speed;    /* speed as printed (mph or kph) */
    int16_t rev, lamp, flags;           /* flags: 1 mph, 2 the needle held (no shake) */
} HudState;

extern const uint8_t g_txt_map[96];
extern const int32_t g_txt_small[59], g_txt_large[55];
extern const uint8_t g_txt_adv[3 * 64], g_txt_space[3][4];
extern const uint16_t g_hud_pal[256];
extern const uint8_t g_glyph_tiles[];
extern const HudGlyph g_glyphs[];
extern const int g_nglyphs;
extern const uint8_t g_glyph_lut[];
extern const HudScheme g_schemes[];
extern const int g_nschemes, g_needle_bank;
extern const uint8_t g_dial_face[64 * 64], g_dial_lamps[], g_needle[];
extern const int g_dial_lamp_frames;
extern const HudDial g_dial;
extern const HudState g_hud_state[];
extern const int g_hud_frames;
extern const int16_t g_sin1024[1024];
extern const int32_t g_view[5];
extern const int g_glyph_bytes;
extern const uint32_t g_rand_seed;
#endif
