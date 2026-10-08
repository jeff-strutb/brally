/* si.c: the controllers, the Controller Pak and the Rumble Pak.
 *
 * Input is either a script (n64box's format: `frame buttons [x y]` lines,
 * `p2 ...` for port 2, `pak`, `rumble`, `frames N`) or the host's keyboard
 * and game pad.  The pads are sampled once per retrace, so a run reads the
 * same input at the same frames as the original under n64box. */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "host.h"
#include "plat.h"
#include "tgr_view.h"

enum { B_A = 0x8000, B_B = 0x4000, B_Z = 0x2000, B_START = 0x1000, B_DU = 0x0800, B_DD = 0x0400,
       B_DL = 0x0200, B_DR = 0x0100, B_L = 0x0020, B_R = 0x0010, B_CU = 0x0008, B_CD = 0x0004,
       B_CL = 0x0002, B_CR = 0x0001 };

typedef struct { int frame; uint16_t b; int8_t x, y; } Ev;
static Ev *s_ev[2];
static int s_nev[2];
static int s_pads = 1;
static int s_pak, s_rumble;
static int s_scripted;
static struct { uint16_t b; int8_t x, y; } s_now[4];

int tgr_pads(void) { return s_pads; }

static uint16_t bits_of(const char *s)
{
    static const struct { const char *n; uint16_t b; } names[] = {
        {"A", B_A}, {"B", B_B}, {"Z", B_Z}, {"START", B_START}, {"DU", B_DU}, {"DD", B_DD},
        {"DL", B_DL}, {"DR", B_DR}, {"L", B_L}, {"R", B_R}, {"CU", B_CU}, {"CD", B_CD},
        {"CL", B_CL}, {"CR", B_CR}};
    char buf[128], *tok, *save;
    uint16_t b = 0;
    unsigned i;
    if (strcmp(s, "-") == 0)
        return 0;
    snprintf(buf, sizeof buf, "%s", s);
    for (tok = strtok_r(buf, "+", &save); tok; tok = strtok_r(NULL, "+", &save))
        for (i = 0; i < sizeof names / sizeof *names; i++)
            if (strcmp(tok, names[i].n) == 0)
                b |= names[i].b;
    return b;
}

static int ev_cmp(const void *a, const void *b)
{
    return ((const Ev *)a)->frame - ((const Ev *)b)->frame;
}

void tgr_script_load(const char *path)
{
    FILE *f = fopen(path, "r");
    char line[256];
    int p;
    if (!f) {
        fprintf(stderr, "tgr: no script %s\n", path);
        exit(1);
    }
    s_scripted = 1;
    while (fgets(line, sizeof line, f)) {
        char *c = strchr(line, '#'), *w[5];
        int n = 0, port = 0;
        Ev e;
        if (c)
            *c = 0;
        for (c = strtok(line, " \t\r\n"); c && n < 5; c = strtok(NULL, " \t\r\n"))
            w[n++] = c;
        if (!n)
            continue;
        if (strcmp(w[0], "pak") == 0) { s_pak = 1; continue; }
        if (strcmp(w[0], "rumble") == 0) { s_rumble = 1; continue; }
        if (strcmp(w[0], "frames") == 0) {
            if (n > 1 && !g_tgr.frames)
                g_tgr.frames = atoi(w[1]);
            continue;
        }
        if (strcmp(w[0], "p2") == 0) {
            port = 1;
            memmove(w, w + 1, sizeof w - sizeof *w);
            n--;
        }
        if (n < 2)
            continue;
        e.frame = atoi(w[0]);
        e.b = bits_of(w[1]);
        e.x = n > 2 ? (int8_t)atoi(w[2]) : 0;
        e.y = n > 3 ? (int8_t)atoi(w[3]) : 0;
        s_ev[port] = (Ev *)realloc(s_ev[port], (size_t)(s_nev[port] + 1) * sizeof(Ev));
        s_ev[port][s_nev[port]++] = e;
    }
    fclose(f);
    for (p = 0; p < 2; p++)
        if (s_nev[p])
            qsort(s_ev[p], (size_t)s_nev[p], sizeof(Ev), ev_cmp);   /* stable enough: frames are distinct */
    s_pads = s_nev[1] ? 2 : 1;
}

/* the host's keyboard and pad: the N64 pad's buttons and stick; Tab and the
 * pad's View button, which the N64 pad has no counterpart for, switch the
 * port's profile (tgr_view.h) */
static uint16_t s_keys;
static int s_kx, s_ky;

void tgr_input_key(int vk, int down)
{
    static int s_tab;
    uint16_t b = 0;
    switch (vk) {
    case 0x09:                                      /* Tab: original / remastered */
        if (down && !s_tab)
            tgr_profile_toggle();
        s_tab = down;
        return;
    case 'X': case 0x0D: b = B_A; break;            /* X or Return */
    case 'Z': b = B_B; break;
    case ' ': b = B_Z; break;
    case 0x1B: case 'P': b = B_START; break;        /* Escape or P */
    case 'Q': b = B_L; break;
    case 'W': case 'E': b = B_R; break;
    case 'I': b = B_CU; break;
    case 'K': b = B_CD; break;
    case 'J': b = B_CL; break;
    case 'L': b = B_CR; break;
    case 0x26: s_ky = down ? 80 : (s_ky > 0 ? 0 : s_ky); return;     /* arrows: the stick */
    case 0x28: s_ky = down ? -80 : (s_ky < 0 ? 0 : s_ky); return;
    case 0x25: s_kx = down ? -80 : (s_kx < 0 ? 0 : s_kx); return;
    case 0x27: s_kx = down ? 80 : (s_kx > 0 ? 0 : s_kx); return;
    }
    if (down)
        s_keys |= b;
    else
        s_keys &= (uint16_t)~b;
}

/* everything released (the window lost the keyboard: its key-ups go elsewhere) */
void tgr_input_release(void)
{
    s_keys = 0;
    s_kx = s_ky = 0;
}

/* a modern stick (a circle, -1..1) as the N64's: past a small dead zone, the
 * travel rescaled to the full range and reaching the N64 stick's octagonal
 * gate, 80 at the four cardinal points and 70 on each axis at the diagonals
 * (a diagonal on a round stick would otherwise only reach 57) */
static void n64_stick(float fx, float fy, int *ox, int *oy)
{
    const float dz = 0.12f;
    float r = sqrtf(fx * fx + fy * fy), ax = fabsf(fx), ay = fabsf(fy), t, g, k;
    *ox = *oy = 0;
    if (r <= dz)
        return;
    /* the gate's edge in this octant runs from (80, 0) to (70, 70), the line
       x = 80 - y / 7 (or mirrored); met by the stick's direction y = t x, it
       lies 80 / (1 + t / 7) along x: the octagon's radius that way */
    t = ax >= ay ? ay / ax : ax / ay;
    g = 80.0f / (1.0f + t / 7.0f) * sqrtf(1.0f + t * t);
    k = (r > 1 ? 1 : (r - dz) / (1 - dz)) * g / r;   /* the travel past the dead zone, to the gate */
    *ox = (int)lroundf(fx * k);
    *oy = (int)lroundf(fy * k);
}

void tgr_input_frame(uint32_t frame)
{
    int p, i;
    if (s_scripted) {
        for (p = 0; p < 2; p++) {
            s_now[p].b = 0;
            s_now[p].x = s_now[p].y = 0;
            for (i = 0; i < s_nev[p] && s_ev[p][i].frame <= (int)frame; i++) {
                s_now[p].b = s_ev[p][i].b;
                s_now[p].x = s_ev[p][i].x;
                s_now[p].y = s_ev[p][i].y;
            }
        }
        return;
    }
    {
        host_pad hp;
        int x = s_kx, y = s_ky;
        uint16_t b = s_keys;
        if (host_pad_read(&hp)) {
            int px, py;
            n64_stick(hp.x, -hp.y, &px, &py);
            if (px || py) {
                x = px;
                y = py;
            }
            if (hp.rx > 0.5f) b |= B_CR;              /* the right stick: the C buttons */
            if (hp.rx < -0.5f) b |= B_CL;
            if (hp.ry > 0.5f) b |= B_CD;
            if (hp.ry < -0.5f) b |= B_CU;
            if (hp.buttons & 1) b |= B_A;
            if (hp.buttons & 2) b |= B_B;
            if (hp.buttons & 4) b |= B_CL;
            if (hp.buttons & 8) b |= B_CU;
            if (hp.buttons & 16) b |= B_L;
            if (hp.buttons & 32) b |= B_R;
            if (hp.buttons & 128) b |= B_START;
            if (hp.buttons & (1 << 10)) b |= B_Z;
            if (hp.buttons & (1 << 11)) b |= B_R;
            if (hp.buttons & (1 << 12)) b |= B_DU;
            if (hp.buttons & (1 << 13)) b |= B_DR;
            if (hp.buttons & (1 << 14)) b |= B_DD;
            if (hp.buttons & (1 << 15)) b |= B_DL;
        }
        {
            static int s_view;                    /* View: original / remastered */
            int v = (hp.buttons & 64) != 0;
            if (v && !s_view)
                tgr_profile_toggle();
            s_view = v;
        }
        s_now[0].b = b;
        s_now[0].x = (int8_t)x;
        s_now[0].y = (int8_t)y;
    }
}

/* ---- libultra ---------------------------------------------------------------- */
int32_t osContInit(OSMesgQueue *mq, uint8_t *bitpattern, OSContStatus *status)
{
    int i;
    (void)mq;
    *bitpattern = (uint8_t)((1 << s_pads) - 1);
    for (i = 0; i < 4; i++) {
        status[i].type = i < s_pads ? 0x0500 : 0;     /* n64box's bytes 05 00 */
        status[i].status = 0;
        status[i].errnum = i < s_pads ? 0 : 8;
    }
    return 0;
}

int32_t osContStartReadData(OSMesgQueue *mq)
{
    (void)mq;
    tgr_os_lock();
    tgr_post_event_at(tgr_count(), OS_EVENT_SI);
    tgr_os_unlock();
    return 0;
}

void osContGetReadData(OSContPad *pad)
{
    int i;
    if (s_scripted)
        tgr_input_frame(tgr_frame());   /* the script's input at this frame, as n64box reads it */
    for (i = 0; i < 4; i++) {
        if (i < s_pads) {
            pad[i].button = s_now[i].b;
            pad[i].stick_x = s_now[i].x;
            pad[i].stick_y = s_now[i].y;
            pad[i].errnum = 0;
        } else {
            memset(&pad[i], 0, sizeof pad[i]);
            pad[i].errnum = 8;
        }
    }
}

/* ---- the Controller Pak: absent unless a script plugs one in -------------------- */
int32_t osPfsIsPlug(OSMesgQueue *mq, uint8_t *pattern)
{
    (void)mq;
    *pattern = (s_pak || s_rumble) ? 1 : 0;
    return 0;
}

int32_t osPfsInit(OSMesgQueue *mq, OSPfs *pfs, int32_t channel)
{
    if (!s_pak)
        return s_rumble ? PFS_ERR_ID_FATAL : PFS_ERR_NOPACK;
    pfs->status = 0;
    pfs->queue = tgr_addr32(mq);
    pfs->channel = channel;
    {
        int i;
        for (i = 0; i < 32; i++)
            pfs->id[i] = (uint8_t)(0x40 + i);
    }
    return 0;
}

int32_t osPfsInitPak(OSMesgQueue *mq, OSPfs *pfs, int32_t channel) { return osPfsInit(mq, pfs, channel); }
int32_t osPfsRepairId(OSPfs *pfs) { (void)pfs; return s_pak ? 0 : PFS_ERR_NOPACK; }
int32_t osPfsChecker(OSPfs *pfs) { (void)pfs; return s_pak ? 0 : PFS_ERR_NOPACK; }

/* the pak's notes: in a scripted run an empty pak whose saves last the run
 * (as n64box models it); in play, a pak kept in a file in the host's save
 * folder, written after every change */
#define PAK_BYTES (123 * 256)
#define PAK_NOTES 16
static struct { int used; uint16_t company; uint32_t game; uint8_t name[16], ext[4]; uint8_t *data; int size; }
    s_notes[PAK_NOTES];

static char s_pak_path[1024];

static void put32(FILE *f, uint32_t v) { fputc((int)(v >> 24), f); fputc((int)(v >> 16) & 255, f); fputc((int)(v >> 8) & 255, f); fputc((int)v & 255, f); }
static uint32_t get32(FILE *f)
{
    uint32_t v = 0;
    int k;
    for (k = 0; k < 4; k++)
        v = v << 8 | (uint32_t)(fgetc(f) & 255);
    return v;
}

/* the file: "TGRPAK1\n", the note count, then each note's company, game,
 * name, extension, size and bytes (big-endian numbers) */
static void pak_save(void)
{
    char tmp[1100];
    FILE *f;
    int i, n = 0;
    if (!s_pak_path[0])
        return;
    snprintf(tmp, sizeof tmp, "%s.tmp", s_pak_path);
    f = fopen(tmp, "wb");
    if (!f)
        return;
    for (i = 0; i < PAK_NOTES; i++)
        n += s_notes[i].used;
    fwrite("TGRPAK1\n", 1, 8, f);
    put32(f, (uint32_t)n);
    for (i = 0; i < PAK_NOTES; i++) {
        if (!s_notes[i].used)
            continue;
        put32(f, s_notes[i].company);
        put32(f, s_notes[i].game);
        fwrite(s_notes[i].name, 1, 16, f);
        fwrite(s_notes[i].ext, 1, 4, f);
        put32(f, (uint32_t)s_notes[i].size);
        fwrite(s_notes[i].data, 1, (size_t)s_notes[i].size, f);
    }
    if (fclose(f) == 0)
        rename(tmp, s_pak_path);
}

static void pak_load(void)
{
    char magic[8];
    FILE *f = fopen(s_pak_path, "rb");
    uint32_t n, i;
    if (!f)
        return;
    if (fread(magic, 1, 8, f) == 8 && !memcmp(magic, "TGRPAK1\n", 8)) {
        n = get32(f);
        for (i = 0; i < n && i < PAK_NOTES; i++) {
            s_notes[i].company = (uint16_t)get32(f);
            s_notes[i].game = get32(f);
            if (fread(s_notes[i].name, 1, 16, f) != 16 || fread(s_notes[i].ext, 1, 4, f) != 4)
                break;
            s_notes[i].size = (int)get32(f);
            if (s_notes[i].size <= 0 || s_notes[i].size > PAK_BYTES)
                break;
            s_notes[i].data = (uint8_t *)calloc(1, (size_t)s_notes[i].size);
            if (fread(s_notes[i].data, 1, (size_t)s_notes[i].size, f) != (size_t)s_notes[i].size)
                break;
            s_notes[i].used = 1;                      /* only a note read whole */
        }
        if (i < PAK_NOTES && !s_notes[i].used) {      /* a file cut short: its last note is dropped */
            free(s_notes[i].data);
            memset(&s_notes[i], 0, sizeof s_notes[i]);
        }
    }
    fclose(f);
}

/* in play (no script) a Controller Pak is plugged into port 1, kept in
 * <save folder>/controller-pak-1.bin */
void tgr_pak_init(void)
{
    const char *dir;
    if (s_scripted) {
        /* TGR_PAK_FILE: the scripted run's pak kept in that file (a test of the
           saves themselves; the script still plugs and unplugs it) */
        const char *e = getenv("TGR_PAK_FILE");
        if (e) {
            snprintf(s_pak_path, sizeof s_pak_path, "%s", e);
            pak_load();
        }
        return;
    }
    dir = host_save_dir();
    if (!dir)
        return;
    host_mkdir(dir);
    snprintf(s_pak_path, sizeof s_pak_path, "%s/controller-pak-1.bin", dir);
    s_pak = 1;
    pak_load();
}

static int note_find(uint16_t company, uint32_t game, uint8_t *name, uint8_t *ext)
{
    int i;
    for (i = 0; i < PAK_NOTES; i++)
        if (s_notes[i].used && s_notes[i].company == company && s_notes[i].game == game &&
            !memcmp(s_notes[i].name, name, 16) && !memcmp(s_notes[i].ext, ext, 4))
            return i;
    return -1;
}

int32_t osPfsFindFile(OSPfs *pfs, uint16_t company, uint32_t game, uint8_t *name, uint8_t *ext, int32_t *file_no)
{
    int i;
    (void)pfs;
    if (!s_pak)
        return PFS_ERR_NOPACK;
    i = note_find(company, game, name, ext);
    if (i < 0)
        return PFS_ERR_INVALID;
    *file_no = i;
    return 0;
}

int32_t osPfsAllocateFile(OSPfs *pfs, uint16_t company, uint32_t game, uint8_t *name, uint8_t *ext,
                          int32_t length, int32_t *file_no)
{
    int i, used = 0;
    (void)pfs;
    if (!s_pak)
        return PFS_ERR_NOPACK;
    for (i = 0; i < PAK_NOTES; i++)
        if (s_notes[i].used)
            used += s_notes[i].size;
    for (i = 0; i < PAK_NOTES && s_notes[i].used; i++)
        ;
    if (i == PAK_NOTES || used + length > PAK_BYTES)
        return PFS_DATA_FULL;
    s_notes[i].used = 1;
    s_notes[i].company = company;
    s_notes[i].game = game;
    memcpy(s_notes[i].name, name, 16);
    memcpy(s_notes[i].ext, ext, 4);
    s_notes[i].size = (length + 255) & ~255;
    s_notes[i].data = (uint8_t *)calloc(1, (size_t)s_notes[i].size);
    *file_no = i;
    pak_save();
    return 0;
}

int32_t osPfsDeleteFile(OSPfs *pfs, uint16_t company, uint32_t game, uint8_t *name, uint8_t *ext)
{
    int i;
    (void)pfs;
    if (!s_pak)
        return PFS_ERR_NOPACK;
    i = note_find(company, game, name, ext);
    if (i < 0)
        return PFS_ERR_INVALID;
    free(s_notes[i].data);
    memset(&s_notes[i], 0, sizeof s_notes[i]);
    pak_save();
    return 0;
}

int32_t osPfsReadWriteFile(OSPfs *pfs, int32_t file_no, uint8_t flag, int32_t offset, int32_t size, uint8_t *data)
{
    (void)pfs;
    if (!s_pak)
        return PFS_ERR_NOPACK;
    if (file_no < 0 || file_no >= PAK_NOTES || !s_notes[file_no].used || offset + size > s_notes[file_no].size)
        return PFS_ERR_INVALID;
    if (flag == 0)
        memcpy(data, s_notes[file_no].data + offset, (size_t)size);
    else {
        memcpy(s_notes[file_no].data + offset, data, (size_t)size);
        pak_save();
    }
    return 0;
}

int32_t osPfsFreeBlocks(OSPfs *pfs, int32_t *bytes)
{
    int i, used = 0;
    (void)pfs;
    if (!s_pak)
        return PFS_ERR_NOPACK;
    for (i = 0; i < PAK_NOTES; i++)
        if (s_notes[i].used)
            used += s_notes[i].size;
    *bytes = PAK_BYTES - used;
    return 0;
}

int32_t osPfsNumFiles(OSPfs *pfs, int32_t *max_files, int32_t *files_used)
{
    int i, n = 0;
    (void)pfs;
    if (!s_pak)
        return PFS_ERR_NOPACK;
    for (i = 0; i < PAK_NOTES; i++)
        n += s_notes[i].used;
    *max_files = PAK_NOTES;
    *files_used = n;
    return 0;
}

int32_t osPfsFileState(OSPfs *pfs, int32_t file_no, OSPfsState *st)
{
    (void)pfs;
    if (!s_pak)
        return PFS_ERR_NOPACK;
    if (file_no < 0 || file_no >= PAK_NOTES || !s_notes[file_no].used)
        return PFS_ERR_INVALID;
    st->file_size = (uint32_t)s_notes[file_no].size;
    st->game_code = s_notes[file_no].game;
    st->company_code = s_notes[file_no].company;
    memcpy(st->ext_name, s_notes[file_no].ext, 4);
    memcpy(st->game_name, s_notes[file_no].name, 16);
    return 0;
}

/* ---- the Rumble Pak ------------------------------------------------------------ */
int32_t osMotorInit(OSMesgQueue *mq, OSPfs *pfs, int32_t channel)
{
    if (!s_rumble || channel != 0)
        return PFS_ERR_NOPACK;
    pfs->status = 0;
    pfs->queue = tgr_addr32(mq);
    pfs->channel = 0;
    pfs->activebank = 0xFF;
    return 0;
}

int32_t osMotorStart(OSPfs *pfs) { (void)pfs; return 0; }
int32_t osMotorStop(OSPfs *pfs) { (void)pfs; return 0; }
