/* disc.c: the Boss Rally CD image, as a BIN of raw 2352-byte sectors and its
 * cue sheet.
 *
 * Track 1 is MODE1/2352: 16 bytes of sync and header, 2048 of data, 288 of
 * error correction per sector, holding an ISO 9660 file system whose long
 * names live in a Joliet supplementary descriptor (the primary tree spells
 * them as mangled 8.3 names the game does not ask for). Tracks 2..13 are
 * Redbook audio: the whole 2352 bytes are 16-bit stereo PCM at 44.1 kHz.
 * INDEX 01 times are absolute positions in the image. (The same reading as
 * tools/brally/extract_iso.py, extract_disc.py and extract_cdaudio.py.) */
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "rb_int.h"

#define RAW 2352
#define USER 2048
#define HDR 16

/* ---- the cue sheet ------------------------------------------------------------------ */
static int msf(const char *s, uint32_t *out)
{
    int m, sec, f;
    if (sscanf(s, "%d:%d:%d", &m, &sec, &f) != 3 || sec >= 60 || f >= 75 || m < 0)
        return 0;
    *out = (uint32_t)((m * 60 + sec) * 75 + f);
    return 1;
}

int rb_cue_parse(const char *path, rb_cue *cue, uint64_t bin_size, char *err, size_t errlen)
{
    char line[1024];
    FILE *f = rb_fopen(path, "rb");
    int cur = -1, i;
    memset(cue, 0, sizeof *cue);
    if (!f) {
        rb_err(err, errlen, "cannot open the cue sheet");
        return 0;
    }
    while (fgets(line, sizeof line, f)) {
        char *p = line, *q;
        while (*p == ' ' || *p == '\t')
            p++;
        if (!strncmp(p, "FILE", 4)) {
            if ((p = strchr(p, '"')) != NULL && (q = strchr(p + 1, '"')) != NULL)
                snprintf(cue->file, sizeof cue->file, "%.*s", (int)(q - p - 1), p + 1);
        } else if (!strncmp(p, "TRACK", 5)) {
            int n;
            char mode[32];
            if (sscanf(p + 5, "%d %31s", &n, mode) != 2 || cue->ntracks >= 99)
                break;
            cur = cue->ntracks++;
            cue->t[cur].number = n;
            cue->t[cur].audio = !strncmp(mode, "AUDIO", 5);
            cue->t[cur].start = 0xFFFFFFFFu;
        } else if (!strncmp(p, "INDEX", 5) && cur >= 0) {
            int idx;
            char t[32];
            if (sscanf(p + 5, "%d %31s", &idx, t) == 2 && idx == 1 && !msf(t, &cue->t[cur].start))
                cue->t[cur].start = 0xFFFFFFFFu;
        }
    }
    fclose(f);
    if (cue->ntracks == 0) {
        rb_err(err, errlen, "the cue sheet lists no tracks");
        return 0;
    }
    for (i = 0; i < cue->ntracks; i++) {
        uint32_t end = i + 1 < cue->ntracks ? cue->t[i + 1].start : (uint32_t)(bin_size / RAW);
        if (cue->t[i].start == 0xFFFFFFFFu || (bin_size && end < cue->t[i].start)) {
            rb_err(err, errlen, "track %d of the cue sheet has no usable INDEX 01", cue->t[i].number);
            return 0;
        }
        cue->t[i].count = end - cue->t[i].start;
    }
    return 1;
}

int rb_cue_bin_path(const char *cue, char *out, size_t n)
{
    rb_cue c;
    char dir[1024];
    if (!rb_cue_parse(cue, &c, 0, NULL, 0) || !c.file[0])
        return 0;
    rb_dirname(dir, sizeof dir, cue);
    rb_join(out, n, dir, c.file);
    return rb_file_size(out) != 0;
}

/* ---- the data track's ISO 9660 file system ---------------------------------------- */
typedef struct iso { FILE *f; int joliet; } iso;

static int iso_read(iso *s, uint64_t off, void *dst, size_t n)
{
    uint8_t *d = (uint8_t *)dst;
    while (n) {
        uint64_t lba = off / USER;
        size_t within = (size_t)(off % USER), take = USER - within < n ? USER - within : n;
        if (!rb_seek(s->f, lba * RAW + HDR + within) || fread(d, 1, take, s->f) != take)
            return 0;
        d += take;
        off += take;
        n -= take;
    }
    return 1;
}

static int root_dir(iso *s, uint32_t *lba, uint32_t *size)
{
    uint8_t d[USER];
    int i, found = 0;
    for (i = 16; i < 32; i++) {
        if (!iso_read(s, (uint64_t)i * USER, d, USER) || memcmp(d + 1, "CD001", 5))
            continue;
        if (d[0] == 1 && !found) {
            memcpy(lba, d + 156 + 2, 4);
            memcpy(size, d + 156 + 10, 4);
            found = 1;
        } else if (d[0] == 2 && d[88] == '%' && d[89] == '/' && (d[90] == '@' || d[90] == 'C' || d[90] == 'E')) {
            memcpy(lba, d + 156 + 2, 4);        /* Joliet: the long names */
            memcpy(size, d + 156 + 10, 4);
            s->joliet = 1;
            return 1;
        } else if (d[0] == 255) {
            break;
        }
    }
    return found;
}

/* a directory record's name as UTF-8, the ;1 version dropped */
static void rec_name(const iso *s, const uint8_t *nm, int len, char *out, size_t n)
{
    size_t o = 0;
    int i;
    if (s->joliet) {
        for (i = 0; i + 1 < len && o + 4 < n; i += 2) {
            unsigned c = (unsigned)nm[i] << 8 | nm[i + 1];
            if (c == ';')
                break;
            if (c < 0x80) {
                out[o++] = (char)c;
            } else if (c < 0x800) {
                out[o++] = (char)(0xC0 | c >> 6);
                out[o++] = (char)(0x80 | (c & 0x3F));
            } else {
                out[o++] = (char)(0xE0 | c >> 12);
                out[o++] = (char)(0x80 | (c >> 6 & 0x3F));
                out[o++] = (char)(0x80 | (c & 0x3F));
            }
        }
    } else {
        for (i = 0; i < len && nm[i] != ';' && o + 1 < n; i++)
            out[o++] = (char)nm[i];
    }
    out[o] = 0;
}

static int strcasecmp_ascii(const char *a, const char *b)
{
    while (*a && tolower((unsigned char)*a) == tolower((unsigned char)*b)) {
        a++;
        b++;
    }
    return tolower((unsigned char)*a) - tolower((unsigned char)*b);
}

/* What the port reads from the data track, and nothing else: the disc also
 * carries the original's programs and installers (SETUP.EXE, iasinst.exe,
 * the DirectX redistributable ...), which the native game never runs and
 * which a virus scanner may object to. The folders hold only data (bitmaps,
 * sounds, tracks, cars). Taken from every brbox scenario run with the port's
 * file log (BR_FILELOG) and the names the game's code can open: the two
 * intro files are the title screen's demo races. Boss.ico is the build's
 * icon. */
static const char *const KEEP_DIRS[] = { "Images", "Paint", "cargfx", "cars", "sfx", "tracks" };
static const char *const KEEP_FILES[] = { "BRGlide.dll", "BRString.dll", "BossRally.pod", "RallyCredits.dat",
                                          "RallyIntro1.dat", "RallyIntro2.dat", "RallyOutro.dat", "loading.img",
                                          "splash.img", "Boss.ico" };

static int kept(const char *name, int dir)
{
    const char *const *l = dir ? KEEP_DIRS : KEEP_FILES;
    size_t i, n = dir ? sizeof KEEP_DIRS / sizeof *KEEP_DIRS : sizeof KEEP_FILES / sizeof *KEEP_FILES;
    for (i = 0; i < n; i++)
        if (!strcasecmp_ascii(name, l[i]))
            return 1;
    return 0;
}

typedef struct walk {
    iso *s;
    const char *out;
    uint64_t total, done;
    rb_progress cb;
    void *ctx;
    double lo, hi;
    volatile int *cancel;
    char *err;
    size_t errlen;
    int pass;                                    /* 0: count the bytes, 1: copy */
} walk;

static int copy_file(walk *w, uint32_t lba, uint32_t size, const char *dst)
{
    static uint8_t buf[USER * 64];
    FILE *f;
    uint64_t off = (uint64_t)lba * USER;
    uint32_t left = size;
    char part[2100];
    snprintf(part, sizeof part, "%s.part", dst);
    if (!(f = rb_fopen(part, "wb"))) {
        rb_err(w->err, w->errlen, "cannot write %s", dst);
        return 0;
    }
    while (left) {
        size_t take = left < sizeof buf ? left : sizeof buf;
        if (w->cancel && *w->cancel) {
            fclose(f);
            rb_err(w->err, w->errlen, "cancelled");
            return 0;
        }
        if (!iso_read(w->s, off, buf, take) || fwrite(buf, 1, take, f) != take) {
            fclose(f);
            rb_err(w->err, w->errlen, "the disc image is short or unreadable at %s", dst);
            return 0;
        }
        off += take;
        left -= (uint32_t)take;
        w->done += take;
        if (w->cb)
            w->cb(w->ctx, w->lo + (w->hi - w->lo) * (double)w->done / (double)(w->total ? w->total : 1),
                  "Copying the game's files from the disc");
    }
    if (fclose(f) != 0 || !rb_rename(part, dst)) {
        rb_err(w->err, w->errlen, "cannot write %s", dst);
        return 0;
    }
    return 1;
}

static int walk_dir(walk *w, uint32_t lba, uint32_t size, const char *dir, int depth)
{
    uint8_t *d;
    uint32_t i = 0;
    int ok = 1;
    if (depth > 16 || size > (1u << 24) || !(d = malloc(size ? size : 1)))
        return 0;
    if (!iso_read(w->s, (uint64_t)lba * USER, d, size)) {
        free(d);
        rb_err(w->err, w->errlen, "the disc image's file system is unreadable");
        return 0;
    }
    while (ok && i < size) {
        uint8_t len = d[i];
        uint32_t clba, csize;
        char name[512], path[2048];
        if (len == 0) {                                  /* records never cross a block */
            i = (i / USER + 1) * USER;
            continue;
        }
        if (i + 33 > size || d[i + 32] + 33u > len) {
            ok = 0;
            break;
        }
        memcpy(&clba, d + i + 2, 4);
        memcpy(&csize, d + i + 10, 4);
        if (!(d[i + 32] == 1 && (d[i + 33] == 0 || d[i + 33] == 1))) {   /* not . or .. */
            rec_name(w->s, d + i + 33, d[i + 32], name, sizeof name);
            rb_join(path, sizeof path, dir, name);
            if (depth == 0 && !kept(name, d[i + 25] & 2)) {
                /* not something the game reads */
            } else if (d[i + 25] & 2) {
                if (w->pass && !rb_mkdirs(path)) {
                    rb_err(w->err, w->errlen, "cannot make the folder %s", path);
                    ok = 0;
                } else {
                    ok = walk_dir(w, clba, csize, path, depth + 1);
                }
            } else if (w->pass) {
                ok = copy_file(w, clba, csize, path);
            } else {
                w->total += csize;
            }
        }
        i += len;
    }
    free(d);
    return ok;
}

int rb_extract_data_track(const char *bin, const char *dir, rb_progress cb, void *ctx, double lo, double hi,
                          volatile int *cancel, char *err, size_t errlen)
{
    iso s = { NULL, 0 };
    walk w;
    uint32_t lba, size;
    uint8_t sync[12];
    static const uint8_t SYNC[12] = { 0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0 };
    int ok;
    if (!(s.f = rb_fopen(bin, "rb")) || fread(sync, 1, 12, s.f) != 12 || memcmp(sync, SYNC, 12)) {
        if (s.f)
            fclose(s.f);
        rb_err(err, errlen, "the BIN is not a raw (MODE1/2352) disc image");
        return 0;
    }
    if (!root_dir(&s, &lba, &size)) {
        fclose(s.f);
        rb_err(err, errlen, "the BIN's data track has no ISO 9660 file system");
        return 0;
    }
    memset(&w, 0, sizeof w);
    w.s = &s;
    w.cb = cb;
    w.ctx = ctx;
    w.lo = lo;
    w.hi = hi;
    w.cancel = cancel;
    w.err = err;
    w.errlen = errlen;
    ok = walk_dir(&w, lba, size, dir, 0);
    w.pass = 1;
    ok = ok && rb_mkdirs(dir) && walk_dir(&w, lba, size, dir, 0);
    fclose(s.f);
    if (!ok && err && !err[0])
        rb_err(err, errlen, "the BIN's file system is damaged");
    return ok;
}
