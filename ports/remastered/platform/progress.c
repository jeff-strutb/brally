/* progress.c: the app's one save (progress.h), "progress.sav" in the save
 * folder, written when it changes.
 *
 * The two games' championships are one: the same six rounds with the same
 * races, points to go on, courses and weather, the season block laid out alike
 * but for Boss Rally's points, per race where Top Gear Rally's are per round.
 * The block is kept in Boss Rally's shape, the fuller; Top Gear Rally's round
 * totals are the sum of its races, and a total that grows lands on the first
 * race of the round without points.
 *
 * Cars are numbered alike, Boss Rally's 13 and 14 the versions of 3 and 6 (a
 * pair is one car: either opens both; its 15 follows 9, BrOptAvailB).  Courses
 * are Boss Rally's numbers, which has one more, each mirror six on; Top Gear
 * Rally's mirrors are five on, in its masks and its records alike. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "host.h"
#include "progress.h"

#define DEF_CARS 0x102                  /* the two games' fresh seasons alike */
#define DEF_TRACKS 4
#define DEF_WEATHER 1
#define PAIR(m, a, b) (((m) >> (a) | (m) >> (b)) & 1 ? (m) | 1u << (a) | 1u << (b) : (m))

#define BR_BLOCK 0x14C
#define TGR_BLOCK 0x128
#define B_PLACE_END 0x1E                /* state, round, race, places: both alike */
#define B_POINTS 0x1E                   /* unsigned short[6][4] */
#define B_TIMES 0x50                    /* float[6][4] */
#define B_LAP 0xB0                      /* float[16], by course */
#define B_MASKS 0xF0
#define B_TOTAL 0x10C                   /* float[16], by course */
#define T_POINTS 0x1E                   /* unsigned short[7] */
#define T_TIMES 0x2C
#define T_LAP 0x8C
#define T_TOTAL 0xE8

static struct {
    char magic[8];
    uint32_t cars, tracks, weather, seeded, have;
    uint8_t block[2][BR_BLOCK];
} s;
static int s_loaded, s_gen, s_ver;

static void path(char *out, size_t n)
{
    snprintf(out, n, "%s/progress.sav", host_save_dir());
}

static void save(void)
{
    char p[1100];
    FILE *f;
    path(p, sizeof p);
    if (!(f = fopen(p, "wb")))
        return;
    memcpy(s.magic, "RMSAVE1\n", 8);
    fwrite(&s, 1, sizeof s, f);
    fclose(f);
}

/* a fresh block: Boss Rally's starting set-up (each game keeps its own) */
static void fresh(uint8_t *b)
{
    int v[5] = { 0, 1, 2, 1, 5 };
    memset(b, 0, BR_BLOCK);
    memcpy(b + 0xF8, v, sizeof v);
}

static void load(void)
{
    char p[1100];
    FILE *f;
    if (s_loaded)
        return;
    s_loaded = 1;
    s.cars = DEF_CARS;
    s.tracks = DEF_TRACKS;
    s.weather = DEF_WEATHER;
    fresh(s.block[0]);
    fresh(s.block[1]);
    path(p, sizeof p);
    if ((f = fopen(p, "rb"))) {
        static uint8_t in[sizeof s];
        if (fread(in, 1, sizeof in, f) == sizeof in && !memcmp(in, "RMSAVE1\n", 8))
            memcpy(&s, in, sizeof s);
        fclose(f);
    }
    if (s.have)
        s_ver = 1;
}

static void merge(unsigned cars, unsigned tracks, unsigned weather)
{
    unsigned c = s.cars | cars, t = s.tracks | tracks, w = s.weather | weather;
    c = PAIR(c, 3, 13);
    c = PAIR(c, 6, 14);
    if (c != s.cars || t != s.tracks || w != s.weather) {
        s.cars = c;
        s.tracks = t;
        s.weather = w;
        save();
    }
}

/* Top Gear Rally's course numbers (five and five) as Boss Rally's (six and six) */
static int course(int i) { return i < 5 ? i : i + 1; }
static unsigned to_br(unsigned t) { return (t & 0x1F) | (t >> 5 & 0x1F) << 6; }
static unsigned to_tgr(unsigned t) { return (t & 0x1F) | (t >> 6 & 0x1F) << 5; }

void rm_progress_tgr(unsigned short *cars, unsigned short *tracks, unsigned short *weather)
{
    load();
    merge(*cars, to_br(*tracks), *weather);
    *cars |= (unsigned short)s.cars;
    *tracks |= (unsigned short)to_tgr(s.tracks);
    *weather |= (unsigned short)s.weather;
}

void rm_progress_br(unsigned short *cars, unsigned short *tracks, unsigned short *weather)
{
    load();
    merge(*cars, *tracks, *weather);
    *cars |= (unsigned short)s.cars;
    *tracks |= (unsigned short)s.tracks;
    *weather |= (unsigned short)s.weather;
}

/* ---- the two shapes of the block ------------------------------------------------- */
static void from_tgr(uint8_t *b, const uint8_t *t)
{
    int r, j, i;
    memcpy(b, t, B_PLACE_END);
    for (r = 0; r < 6; r++) {
        unsigned short row[4], total;
        unsigned sum;
        memcpy(row, b + B_POINTS + r * 8, sizeof row);
        memcpy(&total, t + T_POINTS + r * 2, 2);
        sum = (unsigned)row[0] + row[1] + row[2] + row[3];
        if (total < sum) {              /* (a round begun again) */
            memset(row, 0, sizeof row);
            sum = 0;
        }
        if (total > sum) {
            for (j = 0; j < 3 && row[j]; j++) {
            }
            row[j] = (unsigned short)(row[j] + (total - sum));
        }
        memcpy(b + B_POINTS + r * 8, row, sizeof row);
    }
    memcpy(b + B_TIMES, t + T_TIMES, 6 * 4 * 4);
    for (i = 0; i < 10; i++) {
        memcpy(b + B_LAP + course(i) * 4, t + T_LAP + i * 4, 4);
        memcpy(b + B_TOTAL + course(i) * 4, t + T_TOTAL + i * 4, 4);
    }
}

static void to_tgrblock(uint8_t *t, const uint8_t *b)
{
    int r, i;
    memcpy(t, b, B_PLACE_END);
    memset(t + T_POINTS, 0, 7 * 2);
    for (r = 0; r < 6; r++) {
        unsigned short row[4], total;
        memcpy(row, b + B_POINTS + r * 8, sizeof row);
        total = (unsigned short)(row[0] + row[1] + row[2] + row[3]);
        memcpy(t + T_POINTS + r * 2, &total, 2);
    }
    memcpy(t + T_TIMES, b + B_TIMES, 6 * 4 * 4);
    for (i = 0; i < 10; i++) {
        memcpy(t + T_LAP + i * 4, b + B_LAP + course(i) * 4, 4);
        memcpy(t + T_TOTAL + i * 4, b + B_TOTAL + course(i) * 4, 4);
    }
}

int rm_progress_season_ver(void)
{
    load();
    return s_ver;
}

static int changed(void)
{
    s.have = 1;
    save();
    return ++s_ver;
}

int rm_progress_season_put_tgr(const void *seasons)
{
    int i;
    load();
    for (i = 0; i < 2; i++)
        from_tgr(s.block[i], (const uint8_t *)seasons + i * TGR_BLOCK);
    return changed();
}

void rm_progress_season_get_tgr(void *seasons)
{
    int i;
    load();
    for (i = 0; i < 2; i++)
        to_tgrblock((uint8_t *)seasons + i * TGR_BLOCK, s.block[i]);
}

int rm_progress_season_put_br(const void *p0, const void *p1)
{
    load();
    memcpy(s.block[0], p0, BR_BLOCK);
    memcpy(s.block[1], p1, BR_BLOCK);
    return changed();
}

void rm_progress_season_get_br(void *p0, void *p1)
{
    unsigned short m[3];
    load();
    m[0] = (unsigned short)s.cars;
    m[1] = (unsigned short)s.tracks;
    m[2] = (unsigned short)s.weather;
    memcpy(p0, s.block[0], BR_BLOCK);
    memcpy(p1, s.block[1], BR_BLOCK);
    memcpy((uint8_t *)p0 + B_MASKS, m, sizeof m);
    memcpy((uint8_t *)p1 + B_MASKS, m, sizeof m);
}

/* the better of two records: 0 is none */
static int better(uint8_t *keep, uint8_t *other)
{
    float a, b;
    memcpy(&a, keep, 4);
    memcpy(&b, other, 4);
    if (b != 0.0f && (a == 0.0f || b < a)) {
        memcpy(keep, other, 4);
        return 1;
    }
    if (a != 0.0f && (b == 0.0f || a < b))
        memcpy(other, keep, 4);
    return 0;
}

void rm_progress_records_br(void *block)
{
    uint8_t *p = block;
    int i, n = 0;
    load();
    for (i = 0; i < 12; i++) {
        n += better(s.block[0] + B_LAP + i * 4, p + B_LAP + i * 4);
        n += better(s.block[0] + B_TOTAL + i * 4, p + B_TOTAL + i * 4);
    }
    if (n)
        changed();
}

/* ---- the saves from before ------------------------------------------------------- */
static int far_on(const uint8_t *b)
{
    int state;
    memcpy(&state, b, 4);
    return state * 100 + b[4] * 10 + b[5];
}

static void seed_block(const uint8_t *b)
{
    unsigned short m[3];
    memcpy(m, b + B_MASKS, sizeof m);
    if (m[0] != 0xFFFF || m[1] != 0xFFFF || m[2] != 0xFFFF)
        merge(m[0], m[1], m[2]);
    if (far_on(b) > far_on(s.block[0])) {
        memcpy(s.block[0], b, BR_BLOCK);
        memcpy(s.block[1], b, BR_BLOCK);
        changed();
    }
}

int rm_progress_seeded(void)
{
    load();
    return (int)s.seeded;
}

void rm_progress_seed_tgr(const void *season)
{
    uint8_t b[BR_BLOCK];
    const uint8_t *t = season;
    unsigned short m[3];
    load();
    fresh(b);
    from_tgr(b, t);
    memcpy(m, t + 0xCC, sizeof m);
    m[1] = (unsigned short)to_br(m[1]);
    memcpy(b + B_MASKS, m, sizeof m);
    seed_block(b);
}

/* Boss Rally's .brf: its magic, an Adler-32 of the block's 0x200 bytes, the block */
static void seed_brf(const char *name)
{
    char p[1100];
    static uint8_t f[8 + 0x200];
    uint32_t a = 1, b = 0, want;
    FILE *fp;
    int i, n;
    snprintf(p, sizeof p, "%s/Boss Rally/%s", host_save_dir(), name);
    if (!(fp = fopen(p, "rb")))
        return;
    n = (int)fread(f, 1, sizeof f, fp);
    fclose(fp);
    if (n != (int)sizeof f)
        return;
    for (i = 8; i < n; i++) {
        a = (a + f[i]) % 65521;
        b = (b + a) % 65521;
    }
    memcpy(&want, f + 4, 4);
    if ((b << 16 | a) == want)
        seed_block(f + 8);
}

void rm_progress_seed_done(void)
{
    char name[32];
    int i;
    load();
    seed_brf("autosave.brf");     /* (the port keeps its file names in lower case) */
    for (i = 0; i < 64; i++) {
        snprintf(name, sizeof name, "rallyseason%d.brf", i);
        seed_brf(name);
    }
    s.seeded = 1;
    save();
}

/* ---- erased ---------------------------------------------------------------------- */
int rm_progress_gen(void) { return s_gen; }

void rm_progress_erase(void)
{
    load();
    s.cars = DEF_CARS;
    s.tracks = DEF_TRACKS;
    s.weather = DEF_WEATHER;
    fresh(s.block[0]);
    fresh(s.block[1]);
    s_gen++;
    changed();
}
