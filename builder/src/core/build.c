/* build.c: checking the player's dumps and assembling the build.
 *
 * Mac: an app bundle
 *   Top Gear Rally Remastered.app/Contents/MacOS/remastered     the games
 *                                /Contents/Resources/disc/        Boss Rally's data track
 *                                /Contents/Resources/romdata.bin  the cartridge's data
 *                                /Contents/Resources/AppIcon.icns
 *                                /Contents/Resources/surfaces/    the Remastered textures' sets
 * iPhone: Top Gear Rally Remastered.ipa, the app (Payload/...app) with disc/
 *   and romdata.bin inside, signed with the player's Xcode signing when this
 *   Mac has it (rb_ios_signing), else unsigned for the player to sign
 * Windows: a folder
 *   Top Gear Rally Remastered\Top Gear Rally Remastered.exe, icon.ico, surfaces\, disc\, romdata.bin
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
#include <unistd.h>
extern char **environ;
#endif

#ifndef RB_VERSION
#define RB_VERSION "0.0"
#endif

#define MARKER ".rally-builder"
#define BRGLIDE_MD5 "5d6417cd547097fd00954c6874f85e97"   /* BRGlide.dll on the retail disc */
#define TGR_DATA 0x70AB0u                                 /* ROM bytes the port reads: 0x70AB0 to the end */
#define TGR_ROM_SIZE 0x800000u
#define IOS_APP RB_PRODUCT ".app"

const char *rb_target_name(int t) { return t == RB_MACOS ? "Mac" : t == RB_IOS ? "iPhone" : "Windows"; }

const char *rb_output_name(int t)
{
    return t == RB_MACOS ? RB_PRODUCT ".app" : t == RB_IOS ? RB_PRODUCT ".ipa" : RB_PRODUCT;
}

void rb_output_path(const rb_job *job, char *out, size_t n) { rb_join(out, n, job->dest_dir, rb_output_name(job->target)); }

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
#ifdef __APPLE__
/* a tool, waited for: its exit status (-1 when it could not start); its
 * output to out (n bytes) when out is given */
static int run(char *const argv[], char *out, size_t n)
{
    pid_t pid;
    int st = 0, fd[2] = { -1, -1 };
    posix_spawn_file_actions_t fa;
    posix_spawn_file_actions_init(&fa);
    if (out) {
        if (pipe(fd) != 0)
            return -1;
        posix_spawn_file_actions_adddup2(&fa, fd[1], 1);
        posix_spawn_file_actions_addclose(&fa, fd[0]);
    }
    if (posix_spawn(&pid, argv[0], &fa, NULL, argv, environ) != 0) {
        posix_spawn_file_actions_destroy(&fa);
        if (out) {
            close(fd[0]);
            close(fd[1]);
        }
        return -1;
    }
    posix_spawn_file_actions_destroy(&fa);
    if (out) {
        size_t got = 0;
        ssize_t r;
        close(fd[1]);
        while (got + 1 < n && (r = read(fd[0], out + got, n - 1 - got)) > 0)
            got += (size_t)r;
        out[got] = 0;
        close(fd[0]);
    }
    waitpid(pid, &st, 0);
    return WIFEXITED(st) ? WEXITSTATUS(st) : -1;
}

int rb_ios_signing(char *desc, size_t n)
{
    char sh[2048], err[256], line[2048] = "";
    char *argv[] = { "/bin/sh", sh, "check", NULL };
    if (!rb_host_payload("ios-sign", sh, sizeof sh, err, sizeof err) || run(argv, line, sizeof line) != 0 || !line[0])
        return 0;
    if (strchr(line, '\t'))
        *strchr(line, '\t') = 0;                      /* the certificate's name */
    snprintf(desc, n, "%s", line);
    return 1;
}
#else
int rb_ios_signing(char *desc, size_t n)
{
    (void)desc, (void)n;
    return 0;
}
#endif

/* the marker sits in the app's Resources (Mac), the app inside the .ipa
 * (iPhone), or the folder (Windows) */
static void marker_path(int target, const char *root, char *out, size_t n)
{
    char r[2048], a[2048];
    if (target == RB_MACOS) {
        rb_join(r, sizeof r, root, "Contents/Resources");
        rb_join(out, n, r, MARKER);
    } else if (target == RB_IOS) {
        rb_join(a, sizeof a, root, "Payload");
        rb_join(r, sizeof r, a, IOS_APP);
        rb_join(out, n, r, MARKER);
    } else {
        rb_join(out, n, root, MARKER);
    }
}

/* an .ipa is a zip: this builder's has its marker among the names */
static int ipa_ours(const char *path)
{
    static const char name[] = "Payload/" IOS_APP "/" MARKER;
    size_t n, i;
    char *d = rb_read_file(path, &n);
    int ours = 0;
    for (i = 0; d && !ours && i + sizeof name - 1 <= n; i++)
        ours = d[i] == 'P' && !memcmp(d + i, name, sizeof name - 1);
    free(d);
    return ours;
}

int rb_output_exists(const rb_job *job, int *ours)
{
    char out[2048], mk[2048];
    rb_output_path(job, out, sizeof out);
    if (ours) {
        if (job->target == RB_IOS) {
            *ours = ipa_ours(out);
        } else {
            marker_path(job->target, out, mk, sizeof mk);
            *ours = rb_exists(mk);
        }
    }
    return rb_exists(out);
}

typedef struct ctx {
    const rb_job *job;
    rb_progress cb;
    void *cctx;
    volatile int *cancel;
    char *err;
    size_t errlen;
    char root[2048];           /* the staged build */
    char app[2048];            /* the app inside it (Mac, iPhone) */
    char res[2048];            /* where the data goes */
} ctx;

static void say(ctx *c, double f, const char *s)
{
    if (c->cb)
        c->cb(c->cctx, f, s);
}

static int mac_plist(ctx *c)
{
    char path[2048], buf[4096], contents[2048];
    snprintf(buf, sizeof buf,
             "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
             "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
             "<plist version=\"1.0\">\n<dict>\n"
             "    <key>CFBundleName</key><string>" RB_PRODUCT "</string>\n"
             "    <key>CFBundleDisplayName</key><string>" RB_PRODUCT "</string>\n"
             "    <key>CFBundleIdentifier</key><string>com.strutb.tgrally-remastered</string>\n"
             "    <key>CFBundleExecutable</key><string>remastered</string>\n"
             "    <key>CFBundleIconFile</key><string>AppIcon</string>\n"
             "    <key>CFBundlePackageType</key><string>APPL</string>\n"
             "    <key>CFBundleShortVersionString</key><string>%s</string>\n"
             "    <key>CFBundleVersion</key><string>%s</string>\n"
             "    <key>LSApplicationCategoryType</key><string>public.app-category.racing-games</string>\n"
             "    <key>LSMinimumSystemVersion</key><string>12.0</string>\n"
             "    <key>NSHighResolutionCapable</key><true/>\n"
             "    <key>GCSupportsControllerUserInteraction</key><true/>\n"
             "</dict>\n</plist>\n",
             RB_VERSION, RB_VERSION);
    rb_join(contents, sizeof contents, c->root, "Contents");
    rb_join(path, sizeof path, contents, "Info.plist");
    if (!rb_write_file(path, buf, strlen(buf))) {
        rb_err(c->err, c->errlen, "cannot write the app's Info.plist");
        return 0;
    }
    rb_join(path, sizeof path, contents, "PkgInfo");
    return rb_write_file(path, "APPL????", 8);
}

/* a piece of the payload copied to `to` */
static int payload(ctx *c, const char *name, const char *to, int tree)
{
    char src[2048];
    if (!rb_host_payload(name, src, sizeof src, c->err, c->errlen))
        return 0;
#ifndef _WIN32
    if (tree ? !rb_copy_tree(src, to) : !rb_copy_file(src, to)) {
#else
    (void)tree;
    if (!rb_copy_file(src, to)) {
#endif
        rb_err(c->err, c->errlen, "cannot copy the builder's %s to %s", name, to);
        return 0;
    }
    return 1;
}

/* the Remastered textures' photographed sets (ports/common/surfaces, baked): surfaces.txt and each
 * set's two files, beside the game (the iPhone app carries its own).  The textures are shelved: a
 * builder made without them (builder/build.sh, RM_SHELVED_ASSETS unset) has none to copy */
static int surfaces(ctx *c)
{
    char src[2048], to[2048], dir[2048], name[512], line[512], quiet[256];
    FILE *f;
    if (!rb_host_payload("surfaces/surfaces.txt", src, sizeof src, quiet, sizeof quiet))
        return 1;
    rb_join(dir, sizeof dir, c->res, "surfaces/baked");
    if (!rb_mkdirs(dir)) {
        rb_err(c->err, c->errlen, "cannot make folders in %s (is it writable?)", c->res);
        return 0;
    }
    rb_join(to, sizeof to, c->res, "surfaces/surfaces.txt");
    if (!payload(c, "surfaces/surfaces.txt", to, 0) || !rb_host_payload("surfaces/surfaces.txt", src, sizeof src, c->err, c->errlen))
        return 0;
    if (!(f = fopen(src, "r"))) {
        rb_err(c->err, c->errlen, "cannot read the builder's surfaces.txt");
        return 0;
    }
    while (fgets(line, sizeof line, f)) {
        char id[256];
        int k;
        if (line[0] == '#' || sscanf(line, "%255s", id) != 1)
            continue;
        for (k = 0; k < 2; k++) {
            snprintf(name, sizeof name, "surfaces/baked/%s_%c.raw", id, k ? 'b' : 'a');
            rb_join(to, sizeof to, c->res, name);
            if (!payload(c, name, to, 0)) {
                fclose(f);
                return 0;
            }
        }
    }
    fclose(f);
    return 1;
}

/* the staged layout, with the game and its icon in it */
static int stage(ctx *c)
{
    char tmpname[512], path[2048], dir[2048];
    int t = c->job->target;
    snprintf(tmpname, sizeof tmpname, ".%s.building", rb_output_name(t));
    rb_join(c->root, sizeof c->root, c->job->dest_dir, tmpname);
    if (rb_exists(c->root) && !rb_rmtree(c->root)) {        /* a build that stopped partway */
        rb_err(c->err, c->errlen, "cannot clear %s", c->root);
        return 0;
    }
    if (t == RB_MACOS) {
        snprintf(c->app, sizeof c->app, "%s", c->root);
        rb_join(c->res, sizeof c->res, c->root, "Contents/Resources");
        rb_join(dir, sizeof dir, c->root, "Contents/MacOS");
        if (!rb_mkdirs(dir) || !rb_mkdirs(c->res))
            goto nodir;
        rb_join(path, sizeof path, dir, "remastered");
        if (!payload(c, "remastered", path, 0))
            return 0;
        rb_set_exec(path);
        rb_join(path, sizeof path, c->res, "AppIcon.icns");
        return payload(c, "icon", path, 0) && surfaces(c) && mac_plist(c);
    }
    if (t == RB_IOS) {
        rb_join(dir, sizeof dir, c->root, "Payload");
        rb_join(c->app, sizeof c->app, dir, IOS_APP);
        snprintf(c->res, sizeof c->res, "%s", c->app);
        if (!rb_mkdirs(dir))
            goto nodir;
        return payload(c, "ios-app", c->app, 1);
    }
    snprintf(c->app, sizeof c->app, "%s", c->root);
    snprintf(c->res, sizeof c->res, "%s", c->root);
    if (!rb_mkdirs(c->root))
        goto nodir;
    rb_join(path, sizeof path, c->root, RB_PRODUCT ".exe");
    if (!payload(c, "remastered", path, 0))
        return 0;
    rb_join(path, sizeof path, c->root, "icon.ico");     /* the window's icon (host/windows) */
    return payload(c, "icon", path, 0) && surfaces(c);   /* (host_resource_dir: beside the exe) */
nodir:
    rb_err(c->err, c->errlen, "cannot make folders in %s (is it writable?)", c->job->dest_dir);
    return 0;
}

/* sealed and moved into place */
static int finish(ctx *c)
{
    char out[2048], mk[2048], staged[2048];
    int ours = 0, t = c->job->target;
    marker_path(t, c->root, mk, sizeof mk);
    if (!rb_write_file(mk, RB_VERSION "\n", strlen(RB_VERSION "\n"))) {
        rb_err(c->err, c->errlen, "cannot write in %s", c->root);
        return 0;
    }
    snprintf(staged, sizeof staged, "%s", c->root);
#ifdef __APPLE__
    if (t == RB_MACOS) {       /* sign the bundle as a whole (ad hoc), so its parts are sealed together */
        char *argv[] = { "/usr/bin/codesign", "--force", "--sign", "-", c->root, NULL };
        say(c, 0.97, "Signing the app");
        run(argv, NULL, 0);
    } else if (t == RB_IOS) {
        char sh[2048], payload_dir[2048], dummy[16];
        char *sign[] = { "/bin/sh", sh, "sign", c->app, NULL };
        char *zip[] = { "/usr/bin/ditto", "-c", "-k", "--sequesterRsrc", "--keepParent", payload_dir, staged, NULL };
        if (rb_ios_signing(dummy, sizeof dummy)) {
            say(c, 0.95, "Signing the app with your Xcode signing");
            if (!rb_host_payload("ios-sign", sh, sizeof sh, c->err, c->errlen) || run(sign, NULL, 0) != 0) {
                rb_err(c->err, c->errlen, "signing the iPhone app failed (codesign); check your certificate in Xcode");
                return 0;
            }
        }
        say(c, 0.97, "Packing the .ipa");
        rb_join(payload_dir, sizeof payload_dir, c->root, "Payload");
        snprintf(staged, sizeof staged, "%s.zip", c->root);
        if (run(zip, NULL, 0) != 0) {
            rb_err(c->err, c->errlen, "cannot pack the .ipa");
            return 0;
        }
        rb_rmtree(c->root);
        snprintf(c->root, sizeof c->root, "%s", staged);       /* what is left to clear on failure */
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
    if (!rb_rename(staged, out)) {
        rb_err(c->err, c->errlen, "cannot move the build into place at %s", out);
        return 0;
    }
    return 1;
}

/* the two games' data, from the dumps */
static int data(ctx *c)
{
    char disc[2048], path[2048], hex[33], md5[33];
    rb_cue cue;
    size_t n;
    int order;
    uint8_t *r;
    if (!rb_cue_parse(c->job->cue, &cue, rb_file_size(c->job->bin), c->err, c->errlen))
        return 0;
    say(c, 0.02, "Reading the ROM");
    if (!(r = load_rom(c->job->rom, &n, md5, &order))) {
        rb_err(c->err, c->errlen, "cannot read the ROM");
        return 0;
    }
    if (n != TGR_ROM_SIZE) {
        free(r);
        rb_err(c->err, c->errlen, "the ROM is not 8 MB");
        return 0;
    }
    say(c, 0.05, "Writing Top Gear Rally's data");
    rb_join(path, sizeof path, c->res, "romdata.bin");
    if (!rb_write_file(path, r + TGR_DATA, n - TGR_DATA)) {
        free(r);
        rb_err(c->err, c->errlen, "cannot write %s", path);
        return 0;
    }
    free(r);
    rb_join(disc, sizeof disc, c->res, "disc");
    if (!rb_extract_data_track(c->job->bin, disc, c->cb, c->cctx, 0.1, 0.9, c->cancel, c->err, c->errlen))
        return 0;
    rb_join(path, sizeof path, disc, "BRGlide.dll");
    if (!md5_file(path, hex, NULL, NULL, NULL) || strcmp(hex, BRGLIDE_MD5)) {
        rb_err(c->err, c->errlen, "the disc's BRGlide.dll is not the retail one");
        return 0;
    }
    rb_join(path, sizeof path, disc, "Boss.ico");            /* the disc's icon: the app has its own */
    if (rb_exists(path))
        remove(path);
    return 1;
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
    say(&c, 0, "Preparing");
    ok = stage(&c) && data(&c) && finish(&c);
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
