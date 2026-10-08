/* menu.c -- the main menu: mainmenu.c BrMainMenu on frontbuttons.c BrMenu,
 * one frame at a time.  The carousel of rows, one shown at a time: its icon
 * turning on the ring (BrMenuIconDraw, BrModelDraw: lit by the menu's two
 * lights, textured), turned to the next or the last row by the pad with the
 * move sound, the row's label at rest; A or START on a row that races (the
 * proof of concept has the race only) plays the choose sound and wipes the
 * screen out (BrFadeTo, BrFadeStep's bars).  The icons go through the race's
 * rasterizers (raster.s) by depth, as the race's triangles do; the backdrop,
 * which never moves, is one picture (tools/menu.py); the text and the
 * buttons are sprites from the race HUD's printer (hud.s). */
#include <stdint.h>
#include "menu.h"

#define SW 160
#define SH 128
#define NBUCKET 256
#define MAXPOLY 2400
#define MAXV 480                         /* (menu.py: the largest icon has 471) */

typedef struct { int32_t x, y, u, v; } PolyV;
typedef struct { PolyV v[3]; uint16_t col, next, tex, pad; } Poly;
extern Poly s_poly[MAXPOLY];
extern uint16_t s_bucket[NBUCKET];
extern int s_npoly;
extern uint32_t s_rec[4096];
extern uint16_t *s_back;
extern const Tex *s_textab;
extern volatile uint32_t s_vbl;
extern uint32_t s_winh;
int draw(void);                                         /* raster.s */
void txt_use(const TextSet *t);                         /* hud.s */
void txt_print(const char *s, int x, int y);
void put_sprite(int a0, int a1, int a2);
extern uint16_t s_oam[128 * 4];
extern int32_t s_oamn, s_txt[10];
extern volatile int32_t s_oam_ready;
uint32_t pad_take(void);
extern int32_t s_mus_gain;
extern const int16_t g_sin1024[1024];

typedef struct { int32_t x, y, w, i; } MV;              /* screen (Q4), depth (W >> 10, Q12), light 0..255 */
static MV s_mv[MAXV] __attribute__((section(".ewram_bss")));
void icon_xform(const MVert *v, int n, const int32_t *k, MV *out);   /* menu.s */
void icon_tris(const MTri *t, int n, const MV *mv, int front);

/* one icon: spun by spin (65536ths of a turn), at x (N64 pixels, Q4), into the buckets */
static void icon_draw(const MModel *m, int32_t x, uint32_t spin)
{
    int32_t s = g_sin1024[(spin >> 6) & 1023], c = g_sin1024[((spin >> 6) + 256) & 1023];
    const int32_t (*Q)[3] = g_menu_cam;
    int32_t M[3][3], K[3], L[3], k, i;
    for (k = 0; k < 3; k++) {                           /* Rz(spin) before the rest (row vectors) */
        M[0][k] = (c * Q[0][k] + s * Q[1][k]) >> 12;
        M[1][k] = (c * Q[1][k] - s * Q[0][k]) >> 12;
        M[2][k] = Q[2][k];
        K[k] = ((int32_t)(((int64_t)x * Q[3][k]) >> 4)) + Q[4][k];
    }
    L[0] = (c * g_menu_light.l[0] + s * g_menu_light.l[1]) >> 12;   /* the light's way in the spun model */
    L[1] = (c * g_menu_light.l[1] - s * g_menu_light.l[0]) >> 12;
    L[2] = g_menu_light.l[2];
#ifdef MENU_C
    for (i = 0; i < m->nv; i++) {
        const MVert *v = &m->v[i];
        int32_t X = M[0][0] * v->x + M[1][0] * v->y + M[2][0] * v->z + K[0];
        int32_t Y = M[0][1] * v->x + M[1][1] * v->y + M[2][1] * v->z + K[1];
        int32_t W = M[0][2] * v->x + M[1][2] * v->y + M[2][2] * v->z + K[2];
        int32_t w = W >> 10, d, r;
        if (w < 1)
            w = 1;
        if (w > 4095)
            w = 4095;
        r = (int32_t)s_rec[w];                          /* 2^24 / w: 2^34 / W */
        s_mv[i].x = (int32_t)(((int64_t)X * r) >> 30);  /* X / W, Q4 */
        s_mv[i].y = (int32_t)(((int64_t)Y * r) >> 30);
        s_mv[i].w = w;
        d = v->nx * L[0] + v->ny * L[1] + v->nz * L[2]; /* 127 x Q12 */
        d = d > 0 ? (d * g_menu_light.dir) >> 19 : 0;
        d += g_menu_light.amb;
        s_mv[i].i = d > 255 ? 255 : d;
    }
    for (i = 0; i < m->nt; i++) {
        const MTri *t = &m->t[i];
        const MV *a = &s_mv[t->a], *b = &s_mv[t->b], *e = &s_mv[t->c];
        int32_t cr = (b->x - a->x) * (e->y - a->y) - (e->x - a->x) * (b->y - a->y);
        int32_t lit, lv, key, minx, maxx, miny, maxy;
        Poly *p;
        if (cr == 0 || (!(t->flags & MT_TWO) && (cr > 0) != (g_menu_front > 0)))
            continue;                                    /* facing away (BrModelDraw's cull, but where a part clears it) */
        minx = a->x < b->x ? a->x : b->x;  minx = e->x < minx ? e->x : minx;
        maxx = a->x > b->x ? a->x : b->x;  maxx = e->x > maxx ? e->x : maxx;
        miny = a->y < b->y ? a->y : b->y;  miny = e->y < miny ? e->y : miny;
        maxy = a->y > b->y ? a->y : b->y;  maxy = e->y > maxy ? e->y : maxy;
        if (maxx < 0 || minx >= SW * 16 || maxy < 0 || miny >= SH * 16 || s_npoly >= MAXPOLY)
            continue;
        lit = t->flags & MT_FIXED ? t->grey : (a->i + b->i + e->i) * 85 >> 8;   /* the shade's mean */
        lv = (lit * 8 + 128) >> 8;
        if (lv < 1)
            lv = 1;
        if (lv > 8)
            lv = 8;
        key = (a->w + b->w + e->w) * 85 >> 9;           /* depth: W / 3, half units about the icon's */
        key = key - 542 + 128;
        key = key < 0 ? 0 : key > NBUCKET - 1 ? NBUCKET - 1 : key;
        p = &s_poly[s_npoly];
        p->v[0].x = a->x; p->v[0].y = a->y; p->v[0].u = t->uv[0]; p->v[0].v = t->uv[1];
        p->v[1].x = b->x; p->v[1].y = b->y; p->v[1].u = t->uv[2]; p->v[1].v = t->uv[3];
        p->v[2].x = e->x; p->v[2].y = e->y; p->v[2].u = t->uv[4]; p->v[2].v = t->uv[5];
        p->tex = g_menu_slot[t->slot][lv - 1];
        p->col = 0;
        p->next = s_bucket[key];
        s_bucket[key] = (uint16_t)++s_npoly;
    }
#else
    {
        int32_t kb[18];
        for (i = 0; i < 3; i++) {                       /* the X, Y and W rows, the light, s_rec */
            kb[i * 4] = M[0][i];
            kb[i * 4 + 1] = M[1][i];
            kb[i * 4 + 2] = M[2][i];
            kb[i * 4 + 3] = K[i];
            kb[12 + i] = L[i];
        }
        kb[15] = g_menu_light.dir;
        kb[16] = g_menu_light.amb;
        kb[17] = (int32_t)s_rec;
        icon_xform(m->v, m->nv, kb, s_mv);
        icon_tris(m->t, m->nt, s_mv, g_menu_front);
    }
#endif
}

/* ---- the screen ---- */
enum { T_HIGHLIGHT, T_ALT, T_ALIGN, T_CUSTOM, T_SIZE = 6 };

static void say(const char *pre, const char *s, int x, int y, int size, int align)
{
    char buf[40];
    int n = 0;
    while (*pre)
        buf[n++] = *pre++;
    while (*s && n < 39)
        buf[n++] = *s++;
    buf[n] = 0;
    s_txt[T_HIGHLIGHT] = 0;
    s_txt[T_ALT] = 0;
    s_txt[T_CUSTOM] = 0;
    s_txt[T_ALIGN] = align;
    s_txt[T_SIZE] = size;
    txt_print(buf, x, y);
}

static int s_sel;                         /* D_80316244: the selected row */
static int s_last;                        /* D_80316220: the row before the last move */
static int32_t s_turn;                    /* D_80316224: the carousel's turn, -1..1 (Q16), 0 at rest */
static uint32_t s_spin;                   /* D_80316228: the icon's spin (65536ths of a turn) */
int32_t s_fade;                           /* BrFadeStep's level (Q16) */
static int32_t s_fspeed;          /* and its speed (Q16 a retrace) */
static int32_t s_bl, s_br;                /* its bars: the picture between (N64 pixels) */
static uint32_t s_t;                      /* the retrace last frame */
extern volatile uint32_t s_press[2];

/* the buttons pressed since the last call (latched every retrace: span.s) */
uint32_t pad_take(void)
{
    uint32_t p;
    __asm__ volatile("" ::: "memory");
    *(volatile uint16_t *)0x04000208 = 0;               /* (IME off: the interrupt may add one) */
    p = s_press[0];
    s_press[0] = 0;
    *(volatile uint16_t *)0x04000208 = 1;
    return p;
}

#define REG_WIN0V (*(volatile uint16_t *)0x04000044)
#define REG_WININ (*(volatile uint16_t *)0x04000048)
#define REG_WINOUT (*(volatile uint16_t *)0x0400004A)
#define FADE_STEP 5461                    /* 1 / 0.2 s, a 60th of a second at a time (Q16) */
#define TURN_STEP 2194                    /* the turn's 2 a second (Q16), a retrace (59.73 a second) */
#define SPIN_STEP 349                     /* the spin's 2 radians a second, 65536ths of a turn a retrace */

/* BrFadeTo and BrFadeStep: the level towards 0 or 1, the bars from it (the wipe) */
void fade_to(int up)
{
    s_fspeed = up ? FADE_STEP : -FADE_STEP;
    if (up)
        s_bl = 0;
}

int fade_step(int dv)
{
    int32_t l;
    s_fade += s_fspeed * dv;
    if (s_fade <= 0)
        s_fade = 0;
    if (s_fade >= 65536)
        s_fade = 65536;
    l = (320 * s_fade) >> 16;
    if (s_fspeed > 0) {
        s_bl = 0;
        s_br = (l + 3) & ~3;
    } else {
        int32_t d = (320 - l - s_bl + 3) & ~3;
        s_bl += d;
        s_br += d;
        if (s_br > 320)
            s_br = 320;
    }
    if (s_br > 320)
        s_br = 320;
    return s_fade;
}

/* the wipe's window (WIN0: the picture between the bars, the backdrop's black outside) */
uint16_t fade_window(uint16_t dispcnt)
{
    if (s_fade >= 65536 && s_fspeed > 0)
        return dispcnt;
    s_winh = (uint32_t)((s_bl * 3 / 4) << 8 | (s_br * 3 / 4));   /* (set with the page, at the blank: span.s) */
    REG_WIN0V = 160;
    REG_WININ = 0x14;                     /* BG2 and the sprites inside */
    REG_WINOUT = 0;
    return dispcnt | 1 << 13;
}

void menu_enter(void)
{
    const uint16_t *src;
    uint16_t *dst;
    int i;
    s_textab = g_menu_tex;
    txt_use(&g_menu_tset);
    src = (const uint16_t *)g_menu_btn_tiles;           /* the buttons after the glyphs */
    dst = (uint16_t *)(0x06014000 + g_menu_tset.bytes);
    for (i = 0; i < g_menu_btn_bytes / 2; i++)
        dst[i] = src[i];
    for (i = 0; i < 256; i++)
        ((volatile uint16_t *)0x05000200)[i] = g_menu_pal[i];
    s_turn = 0;
    s_spin = 49152;                                      /* 4.712389: three quarters of a turn */
    s_fade = 0;
    s_bl = s_br = 0;
    fade_to(1);
    s_t = s_vbl;
    pad_take();                                          /* (nothing pressed before the menu counts) */
    snd_play(&g_menu_song);                              /* BrMainMenu: the title music (BrMusicStart) */
}

/* a frame of the menu into s_back: 0 while running, 1 when a row that races is chosen and
   the screen has wiped out */
int menu_frame(void)
{
    uint32_t now = s_vbl;
    int dv = (int)(now - s_t), i, k;
    uint16_t press;
    int32_t off;
    if (dv > 8)
        dv = 8;
    s_t = now;
    press = (uint16_t)pad_take();
#ifdef MENU_FIXED_DT
    {                                                    /* (comparing builds: the same frames, held at */
        static int n;                                    /* the Nth; the next row chosen at the 3rd) */
        dv = ++n < MENU_FIXED_DT ? 2 : 0;
        press = n == 3 && MENU_FIXED_DT > 10 ? 1 << 4 : 0;
    }
#endif
    fade_step(dv);
    s_mus_gain = 256;
    s_spin += SPIN_STEP * dv;
    if (s_turn < 0) {
        s_turn += TURN_STEP * dv;
        if (s_turn > 0)
            s_turn = 0;
    } else if (s_turn > 0) {
        s_turn -= TURN_STEP * dv;
        if (s_turn < 0)
            s_turn = 0;
    }
    off = (int32_t)(((int64_t)s_turn * s_turn) >> 16) * 5;   /* turn * turn * +-5, Q16 */
    if (s_turn < 0)
        off = -off;
    if (s_fspeed < 0) {
        if (s_fade == 0)
            return 1;
    } else if (s_turn == 0) {
        if (press & 1 << 5) {                           /* left: the row before (pad & 4) */
            s_last = s_sel;
            s_sel = s_sel ? s_sel - 1 : 6;
            s_turn = 65536;
        } else if (press & 1 << 4) {                    /* right: the next (pad & 1) */
            s_last = s_sel;
            s_sel = s_sel < 6 ? s_sel + 1 : 0;
            s_turn = -65536;
        }
        if (s_turn)
            snd_sfx(&g_menu_move, g_menu_sfx_rate, g_menu_sfx_level, g_menu_sfx_level);
        else if ((press & (1 << 0 | 1 << 3)) && s_sel < 4) {   /* A or START: a row that races */
            snd_sfx(&g_menu_choose, g_menu_sfx_rate, g_menu_sfx_level, g_menu_sfx_level);
            fade_to(0);
        }
    }
    /* the picture */
    for (i = 0; i < NBUCKET; i++)
        s_bucket[i] = 0;
    s_npoly = 0;
    if (off != 0) {
        int32_t p = (off < 0 ? 5 << 16 : -(5 << 16)) + off;   /* x = 160 - off * 320 / 6 (Q4: 2560 / 3 a unit) */
        icon_draw(&g_menu_icon[s_sel], 160 * 16 - (int32_t)(((int64_t)off * 55924053) >> 32), s_spin);
        icon_draw(&g_menu_icon[s_last], 160 * 16 - (int32_t)(((int64_t)p * 55924053) >> 32), s_spin);
    } else
        icon_draw(&g_menu_icon[s_sel], 160 * 16, s_spin);
    {
        volatile uint32_t *dma = (volatile uint32_t *)0x040000D4;
        dma[0] = (uint32_t)g_menu_backdrop;
        dma[1] = (uint32_t)s_back;
        dma[2] = SW * SH / 2 | 0x84000000u;             /* 32-bit, now */
    }
    draw();
    /* the sprites: the title, the label at rest, the prompts and buttons */
    while (s_oam_ready)                                  /* the last list taken */
        ;
    s_oamn = 127;
    say("%ry", "TOP GEAR RALLY", 160, 240 / 6 - 2, 30, 2);
    if (off == 0)
        say("%ry", g_menu_row[s_sel].label, 160, 240 * 20 / 64, 20, 2);
    for (k = 0; k < 2; k++)
        put_sprite(g_menu_btn[k].y & 0xFF, (g_menu_btn[k].x & 0x1FF) | 1 << 14,
                   (512 + g_menu_btn[k].tile) | g_menu_btn[k].bank << 12);
    say("%ww", "Select", 0x73, 240 * 19 / 20 - 3, 11, 0);
    say("%ww", "Go Back", 0xaf, 240 * 19 / 20 - 3, 11, 0);
    for (i = s_oamn; i >= 0; i--)
        s_oam[i * 4] = 2 << 8;
    s_oam_ready = 1;
    return 0;
}
