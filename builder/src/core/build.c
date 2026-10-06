/* build.c: checking the player's dumps and assembling the build.
 *
 * macOS: an app bundle
 *   Boss Rally.app/Contents/MacOS/brally64           the game
 *                 /Contents/Resources/disc/          the data track
 *                 /Contents/Resources/music/cd/      tracks 2..13 as FLAC
 *                 /Contents/Resources/BossRally.icns the disc's icon
 *   Top Gear Rally.app/Contents/MacOS/tgrally
 *                     /Contents/Resources/romdata.bin
 * Windows: a folder
 *   Boss Rally\Boss Rally.exe, icon.ico, disc\, music\cd\
 *   Top Gear Rally\Top Gear Rally.exe, romdata.bin
 *
 * The build is made in a hidden folder beside its destination and renamed
 * into place when complete; an earlier build is replaced only when it carries
 * this builder's marker file. */
#include <stdlib.h>
#include <string.h>
#include "rb_int.h"

#ifdef __APPLE__
#include <spawn.h>
#include <sys/wait.h>
extern char **environ;
#endif

#ifndef RB_VERSION
#define RB_VERSION "0.0"
#endif

#define MARKER ".rally-builder"
#define BRGLIDE_MD5 "5d6417cd547097fd00954c6874f85e97"   /* BRGlide.dll on the retail disc */
#define TGR_DATA 0x70AB0u                                 /* ROM bytes the port reads: 0x70AB0 to the end */
#define TGR_ROM_SIZE 0x800000u

const char *rb_game_name(int game) { return game == RB_BOSS_RALLY ? "Boss Rally" : "Top Gear Rally"; }

const char *rb_output_name(int game)
{
#ifdef __APPLE__
    return game == RB_BOSS_RALLY ? "Boss Rally.app" : "Top Gear Rally.app";
#else
    return rb_game_name(game);
#endif
}

void rb_output_path(const rb_job *job, char *out, size_t n) { rb_join(out, n, job->dest_dir, rb_output_name(job->game)); }

/* ---- checks --------------------------------------------------------------------------- */
static int md5_file(const char *path, char hex[33], rb_progress cb, void *ctx, volatile int *cancel)
{
    static uint8_t buf[1 << 20];
    uint64_t total = rb_file_size(path), done = 0;
    FILE *f = rb_fopen(path, "rb");
    rb_md5 m;
    uint8_t d[16];
    size_t n;
    if (!f)
        return 0;
    rb_md5_init(&m);
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) {
        if (cancel && *cancel) {
            fclose(f);
            return 0;
        }
        rb_md5_update(&m, buf, n);
        done += n;
        if (cb)
            cb(ctx, total ? (double)done / (double)total : 1, NULL);
    }
    fclose(f);
    rb_md5_final(&m, d);
    rb_md5_hex(d, hex);
    return 1;
}

int rb_check_bin(const char *path, rb_check *out, rb_progress cb, void *ctx, volatile int *cancel)
{
    memset(out, 0, sizeof *out);
    if (!md5_file(path, out->md5, cb, ctx, cancel)) {
        snprintf(out->note, sizeof out->note, "The file could not be read.");
        return 0;
    }
    out->ok = !strcmp(out->md5, RB_BR_BIN_MD5);
    if (!out->ok)
        snprintf(out->note, sizeof out->note, "This is not the retail Boss Rally disc image (a raw BIN of the whole CD).");
    return out->ok;
}

/* the retail disc's tracks: INDEX 01 of tracks 1..13 */
static const char *const RETAIL[] = { "00:00:00", "12:55:18", "16:22:22", "20:37:55", "24:08:09", "27:27:49",
                                      "32:25:28", "35:56:07", "39:53:05", "43:32:05", "47:12:07", "50:25:57", "55:15:52" };

int rb_check_cue(const char *path, rb_check *out)
{
    rb_cue c;
    int i, same;
    memset(out, 0, sizeof *out);
    if (!md5_file(path, out->md5, NULL, NULL, NULL)) {
        snprintf(out->note, sizeof out->note, "The file could not be read.");
        return 0;
    }
    if (!strcmp(out->md5, RB_BR_CUE_MD5))
        return out->ok = 1;
    /* another ripper's sheet: the same tracks at the same places will do */
    same = rb_cue_parse(path, &c, 0, NULL, 0) && c.ntracks == 13;
    for (i = 0; same && i < 13; i++) {
        int m, s, f;
        sscanf(RETAIL[i], "%d:%d:%d", &m, &s, &f);
        same = c.t[i].number == i + 1 && c.t[i].audio == (i > 0) && c.t[i].start == (uint32_t)((m * 60 + s) * 75 + f);
    }
    out->ok = same;
    snprintf(out->note, sizeof out->note, same ? "A different cue sheet, with the retail disc's track layout."
                                               : "This cue sheet does not describe the retail Boss Rally disc.");
    return out->ok;
}

/* the ROM in the cartridge's own (.z64) byte order */
static void rom_z64(uint8_t *r, size_t n, int *order)
{
    size_t i;
    *order = 0;
    if (n >= 4 && r[0] == 0x37 && r[1] == 0x80) {           /* .v64: bytes swapped in pairs */
        for (i = 0; i + 1 < n; i += 2) {
            uint8_t t = r[i];
            r[i] = r[i + 1];
            r[i + 1] = t;
        }
        *order = 1;
    } else if (n >= 4 && r[0] == 0x40 && r[1] == 0x12) {    /* .n64: words little-endian */
        for (i = 0; i + 3 < n; i += 4) {
            uint8_t a = r[i], b = r[i + 1];
            r[i] = r[i + 3];
            r[i + 1] = r[i + 2];
            r[i + 2] = b;
            r[i + 3] = a;
        }
        *order = 2;
    }
}

static uint8_t *load_rom(const char *path, size_t *n, char md5_as_is[33], int *order)
{
    uint8_t *r = rb_read_file(path, n), d[16];
    rb_md5 m;
    if (!r)
        return NULL;
    rb_md5_init(&m);
    rb_md5_update(&m, r, *n);
    rb_md5_final(&m, d);
    rb_md5_hex(d, md5_as_is);
    rom_z64(r, *n, order);
    return r;
}

int rb_check_rom(const char *path, rb_check *out, rb_progress cb, void *ctx, volatile int *cancel)
{
    size_t n;
    int order;
    uint8_t *r, d[16];
    char z[33];
    rb_md5 m;
    (void)cancel;
    memset(out, 0, sizeof *out);
    if (cb)
        cb(ctx, 0, NULL);
    if (!(r = load_rom(path, &n, out->md5, &order))) {
        snprintf(out->note, sizeof out->note, "The file could not be read.");
        return 0;
    }
    rb_md5_init(&m);
    rb_md5_update(&m, r, n);
    rb_md5_final(&m, d);
    rb_md5_hex(d, z);
    free(r);
    if (cb)
        cb(ctx, 1, NULL);
    out->ok = !strcmp(z, RB_TGR_ROM_MD5);
    if (out->ok && order)
        snprintf(out->note, sizeof out->note, "A %s dump: in .z64 byte order its MD5 is %s.", order == 1 ? ".v64" : ".n64", z);
    else if (!out->ok)
        snprintf(out->note, sizeof out->note, "This is not Top Gear Rally (USA).");
    return out->ok;
}

/* ---- the output ------------------------------------------------------------------------- */
static void marker_path(const char *root, char *out, size_t n)
{
#ifdef __APPLE__
    char r[2048];
    rb_join(r, sizeof r, root, "Contents/Resources");
    rb_join(out, n, r, MARKER);
#else
    rb_join(out, n, root, MARKER);
#endif
}

int rb_output_exists(const rb_job *job, int *ours)
{
    char out[2048], mk[2048];
    rb_output_path(job, out, sizeof out);
    marker_path(out, mk, sizeof mk);
    if (ours)
        *ours = rb_exists(mk);
    return rb_exists(out);
}

typedef struct ctx {
    const rb_job *job;
    rb_progress cb;
    void *cctx;
    volatile int *cancel;
    char *err;
    size_t errlen;
    char root[2048], res[2048], exe[2048];    /* the staged build, its resources, its executable */
} ctx;

static void say(ctx *c, double f, const char *s)
{
    if (c->cb)
        c->cb(c->cctx, f, s);
}

static int plist(ctx *c, const char *name, const char *exe, const char *id, const char *min, const char *icon)
{
#ifdef __APPLE__
    char path[2048], buf[4096], ic[256] = "", contents[2048];
    if (icon)
        snprintf(ic, sizeof ic, "    <key>CFBundleIconFile</key><string>%s</string>\n", icon);
    snprintf(buf, sizeof buf,
             "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
             "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
             "<plist version=\"1.0\">\n<dict>\n"
             "    <key>CFBundleName</key><string>%s</string>\n"
             "    <key>CFBundleDisplayName</key><string>%s</string>\n"
             "    <key>CFBundleIdentifier</key><string>%s</string>\n"
             "    <key>CFBundleExecutable</key><string>%s</string>\n"
             "    <key>CFBundlePackageType</key><string>APPL</string>\n"
             "    <key>CFBundleShortVersionString</key><string>%s</string>\n"
             "    <key>CFBundleVersion</key><string>%s</string>\n"
             "%s"
             "    <key>LSApplicationCategoryType</key><string>public.app-category.racing-games</string>\n"
             "    <key>LSMinimumSystemVersion</key><string>%s</string>\n"
             "    <key>NSHighResolutionCapable</key><true/>\n"
             "    <key>GCSupportsControllerUserInteraction</key><true/>\n"
             "</dict>\n</plist>\n",
             name, name, id, exe, RB_VERSION, RB_VERSION, ic, min);
    rb_join(contents, sizeof contents, c->root, "Contents");
    rb_join(path, sizeof path, contents, "Info.plist");
    if (!rb_write_file(path, buf, strlen(buf))) {
        rb_err(c->err, c->errlen, "cannot write the app's Info.plist");
        return 0;
    }
    rb_join(path, sizeof path, contents, "PkgInfo");
    return rb_write_file(path, "APPL????", 8);
#else
    (void)c, (void)name, (void)exe, (void)id, (void)min, (void)icon;
    return 1;
#endif
}

/* the staged folder layout and the game executable */
static int stage(ctx *c, const char *payload)
{
    char out[2048], dir[2048], tmpname[512];
    rb_output_path(c->job, out, sizeof out);
    snprintf(tmpname, sizeof tmpname, ".%s.building", rb_output_name(c->job->game));
    rb_join(c->root, sizeof c->root, c->job->dest_dir, tmpname);
    if (rb_exists(c->root) && !rb_rmtree(c->root)) {        /* a build that stopped partway */
        rb_err(c->err, c->errlen, "cannot clear %s", c->root);
        return 0;
    }
#ifdef __APPLE__
    rb_join(dir, sizeof dir, c->root, "Contents/MacOS");
    rb_join(c->res, sizeof c->res, c->root, "Contents/Resources");
    rb_join(c->exe, sizeof c->exe, dir, payload);
#else
    snprintf(dir, sizeof dir, "%s", c->root);
    snprintf(c->res, sizeof c->res, "%s", c->root);
    {
        char exe[512];
        snprintf(exe, sizeof exe, "%s.exe", rb_game_name(c->job->game));
        rb_join(c->exe, sizeof c->exe, dir, exe);
    }
#endif
    if (!rb_mkdirs(dir) || !rb_mkdirs(c->res)) {
        rb_err(c->err, c->errlen, "cannot make folders in %s (is it writable?)", c->job->dest_dir);
        return 0;
    }
    if (!rb_host_payload(payload, c->exe, c->err, c->errlen))
        return 0;
    rb_set_exec(c->exe);
    return 1;
}

static int finish(ctx *c)
{
    char out[2048], mk[2048];
    int ours = 0;
    marker_path(c->root, mk, sizeof mk);
    if (!rb_write_file(mk, RB_VERSION "\n", strlen(RB_VERSION "\n"))) {
        rb_err(c->err, c->errlen, "cannot write in %s", c->root);
        return 0;
    }
#ifdef __APPLE__
    {   /* sign the bundle as a whole (ad hoc), so its parts are sealed together */
        pid_t pid;
        int st = 0;
        char *argv[] = { "codesign", "--force", "--sign", "-", c->root, NULL };
        say(c, 0.99, "Signing the app");
        if (posix_spawn(&pid, "/usr/bin/codesign", NULL, NULL, argv, environ) == 0)
            waitpid(pid, &st, 0);
    }
#endif
    rb_output_path(c->job, out, sizeof out);
    if (rb_output_exists(c->job, &ours)) {
        if (!ours) {
            rb_err(c->err, c->errlen, "%s already exists and was not made by this builder; move it away or pick another folder", out);
            return 0;
        }
        if (!rb_rmtree(out)) {
            rb_err(c->err, c->errlen, "cannot replace the earlier build at %s", out);
            return 0;
        }
    }
    if (!rb_rename(c->root, out)) {
        rb_err(c->err, c->errlen, "cannot move the build into place at %s", out);
        return 0;
    }
    return 1;
}

static int build_br(ctx *c)
{
    char disc[2048], music[2048], path[2048], icon[2048] = "", hex[33];
    rb_cue cue;
    FILE *bin;
    uint64_t bin_size = rb_file_size(c->job->bin), audio = 0, done = 0;
    int i;
    if (!rb_cue_parse(c->job->cue, &cue, bin_size, c->err, c->errlen))
        return 0;
    say(c, 0, "Preparing");
    if (!stage(c, "brally64"))
        return 0;
    rb_join(disc, sizeof disc, c->res, "disc");
    if (!rb_extract_data_track(c->job->bin, disc, c->cb, c->cctx, 0.0, 0.1, c->cancel, c->err, c->errlen))
        return 0;
    rb_join(path, sizeof path, disc, "BRGlide.dll");
    if (!md5_file(path, hex, NULL, NULL, NULL) || strcmp(hex, BRGLIDE_MD5)) {
        rb_err(c->err, c->errlen, "the disc's BRGlide.dll is not the retail one");
        return 0;
    }
    /* the soundtrack */
    rb_join(path, sizeof path, c->res, "music");
    rb_join(music, sizeof music, path, "cd");
    if (!rb_mkdirs(music) || !(bin = rb_fopen(c->job->bin, "rb"))) {
        rb_err(c->err, c->errlen, "cannot make %s", music);
        return 0;
    }
    for (i = 0; i < cue.ntracks; i++)
        audio += cue.t[i].audio ? cue.t[i].count : 0;
    for (i = 0; i < cue.ntracks; i++) {
        char name[32], status[64];
        double lo = 0.1 + 0.88 * (double)done / (double)(audio ? audio : 1);
        if (!cue.t[i].audio)
            continue;
        snprintf(name, sizeof name, "track%02d.flac", cue.t[i].number);
        snprintf(status, sizeof status, "Encoding the soundtrack: track %d", cue.t[i].number);
        say(c, lo, status);
        rb_join(path, sizeof path, music, name);
        if (!rb_seek(bin, (uint64_t)cue.t[i].start * 2352) ||
            !rb_flac_encode(bin, (uint64_t)cue.t[i].count * 588, path, c->cb, c->cctx, lo,
                            0.1 + 0.88 * (double)(done + cue.t[i].count) / (double)(audio ? audio : 1),
                            c->cancel, c->err, c->errlen)) {
            fclose(bin);
            return 0;
        }
        done += cue.t[i].count;
    }
    fclose(bin);
    /* the icon */
    say(c, 0.98, "Finishing");
    rb_join(path, sizeof path, disc, "Boss.ico");
    if (rb_file_size(path)) {
#ifdef __APPLE__
        rb_join(icon, sizeof icon, c->res, "BossRally.icns");
        if (!rb_ico_to_icns(path, icon, c->err, c->errlen))
            return 0;
#else
        {   /* the game sets its window's icon from this at start-up */
            char ico[2048];
            size_t n;
            void *d = rb_read_file(path, &n);
            rb_join(ico, sizeof ico, c->root, "icon.ico");
            if (!d || !rb_write_file(ico, d, n)) {
                free(d);
                rb_err(c->err, c->errlen, "cannot write %s", ico);
                return 0;
            }
            free(d);
        }
#endif
    }
    return plist(c, "Boss Rally", "brally64", "com.strutb.bossrally64", "11.0", icon[0] ? "BossRally" : NULL) && finish(c);
}

static int build_tgr(ctx *c)
{
    char path[2048], md5[33];
    size_t n;
    int order;
    uint8_t *r;
    say(c, 0, "Reading the ROM");
    if (!(r = load_rom(c->job->rom, &n, md5, &order))) {
        rb_err(c->err, c->errlen, "cannot read the ROM");
        return 0;
    }
    if (n != TGR_ROM_SIZE) {
        free(r);
        rb_err(c->err, c->errlen, "the ROM is not 8 MB");
        return 0;
    }
    if (!stage(c, "tgrally")) {
        free(r);
        return 0;
    }
    say(c, 0.5, "Writing the game's data");
    rb_join(path, sizeof path, c->res, "romdata.bin");
    if (!rb_write_file(path, r + TGR_DATA, n - TGR_DATA)) {
        free(r);
        rb_err(c->err, c->errlen, "cannot write %s", path);
        return 0;
    }
    free(r);
    say(c, 0.9, "Finishing");
    return plist(c, "Top Gear Rally", "tgrally", "com.strutb.tgrally", "12.0", NULL) && finish(c);
}

int rb_build(const rb_job *job, rb_progress cb, void *cctx, volatile int *cancel, char *err, size_t errlen)
{
    ctx c;
    int ok;
    memset(&c, 0, sizeof c);
    c.job = job;
    c.cb = cb;
    c.cctx = cctx;
    c.cancel = cancel;
    c.err = err;
    c.errlen = errlen;
    if (err && errlen)
        err[0] = 0;
    if (!rb_is_dir(job->dest_dir) && !rb_mkdirs(job->dest_dir)) {
        rb_err(err, errlen, "cannot make the folder %s", job->dest_dir);
        return 0;
    }
    ok = job->game == RB_BOSS_RALLY ? build_br(&c) : build_tgr(&c);
    if (!ok) {
        if (c.root[0])
            rb_rmtree(c.root);                         /* leave nothing half-made behind */
        if (err && errlen && !err[0])
            rb_err(err, errlen, "the build failed");
    } else {
        say(&c, 1.0, "Done");
    }
    return ok;
}
