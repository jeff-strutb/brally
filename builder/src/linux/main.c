/* main.c: the release builder's Linux front end (GTK 3). It builds Boss Rally,
 * Top Gear Rally or Top Gear Rally Remastered for Linux: on a desktop, or a
 * Steam Deck in its desktop mode.
 *
 *   1 which game
 *   2 the player's own dumps the game needs (the Top Gear Rally ROM, the Boss
 *     Rally BIN and cue), checked by MD5, and the folder to build into; a dump
 *     that does not match builds only after a warning the player accepts
 *   3 the build (core/build.c), with progress
 *   4 the result: the game's folder (with its launcher, which the player can
 *     add to the desktop's applications, where Steam's Add a Non-Steam Game
 *     finds it), or the error that stopped it
 *
 * The game executables and the icon are plain files in the games folder: in
 * the AppImage, usr/share/rally-builder/games beside usr/bin; else games/
 * beside the builder.
 *
 * Arguments fill in the choices for a scripted run, and --build starts it:
 *   --game br|tgr|remastered  --dest DIR  --bin FILE  --cue FILE  --rom FILE  --rom-jpn FILE  --build
 * --cli builds without a window (the same arguments), its progress on the
 * terminal: over SSH, say. */
#include <gtk/gtk.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "rb.h"

enum { K_BIN, K_CUE, K_ROM, K_ROMJPN, K_N };   /* K_ROMJPN: Remastered's Japan ROM */

static GtkWidget *s_win, *s_page_box, *s_bar, *s_status, *s_build, *s_cancel_btn;
static GtkWidget *s_rowtext[K_N];
static int s_page, s_game = RB_REMASTERED;
static char s_dest[2048];
static char s_file[K_N][2048];
static int s_ok[K_N], s_checking;
static int s_region = -1;               /* the Top Gear Rally ROM's cartridge (rb_check) */
static volatile int s_cancel;
static char s_err[1024], s_out[2048];
static int s_result;
static int s_unchecked;                 /* building from dumps that do not match, the player's choice */

/* ---- the payload ---------------------------------------------------------------------- */
static void exe_dir(char *out, size_t n)
{
    ssize_t k = readlink("/proc/self/exe", out, n - 1);
    char *s;
    if (k <= 0) {
        snprintf(out, n, ".");
        return;
    }
    out[k] = 0;
    if ((s = strrchr(out, '/')) != NULL)
        *s = 0;
}

static int is_file(const char *p)
{
    struct stat st;
    return stat(p, &st) == 0 && S_ISREG(st.st_mode);
}

/* 1 when this machine has a Vulkan device the games can draw with: then the
 * games' Vulkan builds go in, else their OpenGL ones (a build linked to
 * libvulkan.so.1 does not start without it) */
typedef void *(*vk_gipa)(void *, const char *);
typedef int (*vk_create)(const void *, const void *, void **);
typedef int (*vk_enum)(void *, uint32_t *, void *);
typedef void (*vk_destroy)(void *, const void *);
static int vulkan_usable(void)
{
    static int known = -1;
    struct { int sType; const void *pNext; uint32_t flags; const void *app; uint32_t nl; const char *const *l;
             uint32_t ne; const char *const *e; } ci;
    struct { int sType; const void *pNext; const char *name; uint32_t ver; const char *eng; uint32_t ever;
             uint32_t api; } app;
    void *m;
    vk_gipa gipa;
    vk_create create;
    void *inst = NULL;
    uint32_t n = 0;
    if (known >= 0)
        return known;
    known = 0;
    if (!(m = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL)) || !(gipa = (vk_gipa)dlsym(m, "vkGetInstanceProcAddr")))
        return 0;
    create = (vk_create)gipa(NULL, "vkCreateInstance");
    memset(&app, 0, sizeof app);
    app.sType = 0;                                 /* VK_STRUCTURE_TYPE_APPLICATION_INFO */
    app.api = 1u << 22;                            /* VK_API_VERSION_1_0 */
    memset(&ci, 0, sizeof ci);
    ci.sType = 1;                                  /* VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO */
    ci.app = &app;
    if (create && create(&ci, NULL, &inst) == 0 && inst) {
        vk_enum en = (vk_enum)gipa(inst, "vkEnumeratePhysicalDevices");
        vk_destroy de = (vk_destroy)gipa(inst, "vkDestroyInstance");
        if (en && en(inst, &n, NULL) == 0 && n > 0)
            known = 1;
        if (de)
            de(inst, NULL);
    }
    return known;                                  /* (the library stays loaded: a loader may not unload) */
}

/* the games folder: the AppImage's, else beside the builder */
static void games_dir(char *out, size_t n)
{
    char here[2048], d[2300];
    exe_dir(here, sizeof here);
    snprintf(d, sizeof d, "%s/../share/rally-builder/games", here);
    if (g_file_test(d, G_FILE_TEST_IS_DIR)) {
        snprintf(out, n, "%s", d);
        return;
    }
    snprintf(out, n, "%s/games", here);
}

/* the pieces in the games folder: each game (its Vulkan build, or the
 * OpenGL one on a machine without Vulkan) and Remastered's icon */
int rb_host_payload(const char *name, char *path, size_t n, char *err, size_t errlen)
{
    char dir[2048];
    games_dir(dir, sizeof dir);
    if (!strcmp(name, "remastered") || !strcmp(name, "brally64") || !strcmp(name, "tgrally") ||
        !strcmp(name, "tgrally-eur") || !strcmp(name, "tgrally-jpn"))
        snprintf(path, n, "%s/%s%s", dir, name, vulkan_usable() ? "" : "-gl");
    else if (!strcmp(name, "icon"))
        snprintf(path, n, "%s/icon.png", dir);
    else
        path[0] = 0;
    if (!path[0] || !is_file(path)) {
        snprintf(err, errlen, "the builder's games folder is missing a file (%s); download Rally Builder again", name);
        return 0;
    }
    return 1;
}

/* ---- a scripted build without a window (--cli) ---------------------------------------------- */
static void cli_progress(void *ctx, double f, const char *s)
{
    static int last = -1;
    static char said[256];
    int pct = (int)(f * 100);
    (void)ctx;
    if (s && strcmp(s, said)) {
        snprintf(said, sizeof said, "%s", s);
        printf("\r%3d%%  %-60s\n", pct, s);
        last = pct;
    } else if (pct != last) {
        printf("\r%3d%%", pct);
        last = pct;
    }
    fflush(stdout);
}

static int cli(void)
{
    rb_job job = { 0 };
    rb_check c;
    char out[2048];
    int k, ours = 0, ok;
    static const char *const label[K_N] = { "disc image", "cue sheet", "ROM", "Japan's ROM" };
    for (k = 0; k < K_N; k++) {
        int need = k == K_ROM ? rb_needs_rom(s_game) : k == K_ROMJPN ? rb_needs_rom_jpn(s_game) : rb_needs_disc(s_game);
        if (!need)
            continue;
        if (!s_file[k][0]) {
            fprintf(stderr, "rally-builder: %s needs the %s (--%s)\n", rb_game_name(s_game), label[k],
                    k == K_BIN ? "bin" : k == K_CUE ? "cue" : k == K_ROM ? "rom" : "rom-jpn");
            return 2;
        }
        memset(&c, 0, sizeof c);
        if (k == K_BIN)
            rb_check_bin(s_file[k], &c, NULL, NULL, NULL);
        else if (k == K_CUE)
            rb_check_cue(s_file[k], &c);
        else
            rb_check_rom(s_file[k], &c, NULL, NULL, NULL);
        if (k == K_ROM)
            s_region = c.region;
        printf("%s: %s  %s%s%s\n", s_file[k], c.md5, c.ok ? "matches" : "does not match", c.note[0] ? "  " : "", c.note);
        if (!c.ok)
            s_unchecked = 1;
    }
    if (s_unchecked && !getenv("RB_BUILD_ANYWAY")) {
        fprintf(stderr, "rally-builder: a file does not match the dumps Rally Builder is made for; RB_BUILD_ANYWAY=1 "
                        "builds anyway\n");
        return 2;
    }
    job.game = s_game;
    job.target = RB_LINUX;
    job.dest_dir = s_dest;
    job.bin = s_file[K_BIN];
    job.cue = s_file[K_CUE];
    job.rom = s_file[K_ROM];
    job.rom_jpn = s_file[K_ROMJPN];
    job.region = s_region;
    job.unchecked = s_unchecked;
    if (rb_output_exists(&job, &ours) && !ours) {
        rb_output_path(&job, out, sizeof out);
        fprintf(stderr, "rally-builder: %s already exists and was not made by Rally Builder\n", out);
        return 1;
    }
    printf("Building %s (%s) into %s\n", rb_game_name(s_game), vulkan_usable() ? "Vulkan" : "OpenGL", s_dest);
    ok = rb_build(&job, cli_progress, NULL, &s_cancel, s_err, sizeof s_err);
    rb_output_path(&job, out, sizeof out);
    if (ok)
        printf("\nBuilt %s\n", out);
    else
        fprintf(stderr, "\nrally-builder: %s\n", s_err);
    return ok ? 0 : 1;
}

/* ---- layout ------------------------------------------------------------------------------- */
static void clear_page(void)
{
    if (s_page_box)
        gtk_widget_destroy(s_page_box);
    s_page_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(s_page_box), 24);
    gtk_container_add(GTK_CONTAINER(s_win), s_page_box);
    s_bar = s_status = s_build = s_cancel_btn = NULL;
    memset(s_rowtext, 0, sizeof s_rowtext);
}

static GtkWidget *label(const char *markup, int wrap)
{
    GtkWidget *l = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(l), markup);
    gtk_label_set_xalign(GTK_LABEL(l), 0);
    gtk_label_set_line_wrap(GTK_LABEL(l), wrap);
    gtk_label_set_max_width_chars(GTK_LABEL(l), 70);
    gtk_label_set_selectable(GTK_LABEL(l), FALSE);
    return l;
}

static GtkWidget *text(GtkWidget *box, const char *s)
{
    char *m = g_markup_escape_text(s, -1);
    GtkWidget *l = label(m, 1);
    g_free(m);
    gtk_box_pack_start(GTK_BOX(box), l, FALSE, FALSE, 0);
    return l;
}

static void title(GtkWidget *box, const char *s)
{
    char *m = g_markup_printf_escaped("<span size='x-large' weight='bold'>%s</span>", s);
    gtk_box_pack_start(GTK_BOX(box), label(m, 0), FALSE, FALSE, 0);
    g_free(m);
}

/* the buttons' row, at the bottom right */
static GtkWidget *buttons(void)
{
    GtkWidget *spacer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0), *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_pack_start(GTK_BOX(s_page_box), spacer, TRUE, TRUE, 0);
    gtk_box_pack_end(GTK_BOX(s_page_box), row, FALSE, FALSE, 0);
    return row;
}

static GtkWidget *button(GtkWidget *row, const char *s, GCallback cb, int def)
{
    GtkWidget *b = gtk_button_new_with_mnemonic(s);
    g_signal_connect(b, "clicked", cb, NULL);
    gtk_box_pack_end(GTK_BOX(row), b, FALSE, FALSE, 0);
    if (def) {
        gtk_widget_set_can_default(b, TRUE);
        gtk_style_context_add_class(gtk_widget_get_style_context(b), "suggested-action");
    }
    return b;
}

static void show_page(void) { gtk_widget_show_all(s_win); }

/* ---- pages --------------------------------------------------------------------------------- */
static void page_dumps(void);
static void page_game(void);
static void build(void);

static void on_game(GtkToggleButton *b, gpointer g)
{
    if (gtk_toggle_button_get_active(b))
        s_game = GPOINTER_TO_INT(g);
}

static void on_next(void) { page_dumps(); }

static void page_game(void)
{
    static const char *const what[] = {
        "The 1999 PC game, from your retail CD (BIN/CUE), with its CD soundtrack.",
        "The 1997 Nintendo 64 game, from your cartridge ROM: USA, Europe or Japan.",
        "Top Gear Rally's menus (the USA's and Japan's) with Boss Rally's engine running the races.",
    };
    GtkWidget *group = NULL, *row;
    int g;
    s_page = 1;
    clear_page();
    title(s_page_box, "Rally Builder");
    text(s_page_box, "Rally Builder makes a native Linux build of a game from your own copy of it. Choose the game.");
    for (g = RB_BOSS_RALLY; g <= RB_REMASTERED; g++) {
        char *m = g_markup_printf_escaped("<b>%s</b>", rb_game_name(g));
        GtkWidget *r = gtk_radio_button_new_from_widget(group ? GTK_RADIO_BUTTON(group) : NULL), *l = label(m, 0), *d;
        g_free(m);
        gtk_container_add(GTK_CONTAINER(r), l);
        group = r;
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(r), g == s_game);
        g_signal_connect(r, "toggled", G_CALLBACK(on_game), GINT_TO_POINTER(g));
        gtk_widget_set_margin_top(r, 12);
        gtk_box_pack_start(GTK_BOX(s_page_box), r, FALSE, FALSE, 0);
        d = text(s_page_box, what[g]);
        gtk_widget_set_margin_start(d, 26);
    }
    row = buttons();
    gtk_widget_grab_default(button(row, "_Continue", G_CALLBACK(on_next), 1));
    show_page();
}

/* the dumps this game needs */
static int needed(int k)
{
    return k == K_ROM ? rb_needs_rom(s_game) : k == K_ROMJPN ? rb_needs_rom_jpn(s_game) : rb_needs_disc(s_game);
}

static const char *base(const char *p) { return strrchr(p, '/') ? strrchr(p, '/') + 1 : p; }

/* the chosen dumps that do not match, by name (n: how many) */
static int mismatched(char *out, size_t len, const char *sep)
{
    int k, n = 0;
    out[0] = 0;
    for (k = 0; k < K_N; k++)
        if (needed(k) && !s_ok[k]) {
            if (n++)
                g_strlcat(out, sep, len);
            g_strlcat(out, base(s_file[k]), len);
        }
    return n;
}

static void update_build(void)
{
    int ok = 1, k;
    for (k = 0; k < K_N; k++)
        if (needed(k) && !s_file[k][0])
            ok = 0;                     /* (matching or not: a mismatch asks before it builds) */
    if (s_build)
        gtk_widget_set_sensitive(s_build, ok && !s_checking);
}

static void start_check(int k, const char *path);

/* a chooser: a folder, or a file of the kind named */
static int pick(int folder, char *out, size_t n, const char *kind, const char *const *pats)
{
    GtkFileChooserNative *d = gtk_file_chooser_native_new(folder ? "Choose a folder" : "Choose a file", GTK_WINDOW(s_win),
                                                          folder ? GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER
                                                                 : GTK_FILE_CHOOSER_ACTION_OPEN, "_Choose", "_Cancel");
    int ok = 0;
    if (!folder && pats) {
        GtkFileFilter *f = gtk_file_filter_new(), *all = gtk_file_filter_new();
        gtk_file_filter_set_name(f, kind);
        for (; *pats; pats++)
            gtk_file_filter_add_pattern(f, *pats);
        gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(d), f);
        gtk_file_filter_set_name(all, "All files");
        gtk_file_filter_add_pattern(all, "*");
        gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(d), all);
    }
    if (folder && out[0])
        gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(d), out);
    if (gtk_native_dialog_run(GTK_NATIVE_DIALOG(d)) == GTK_RESPONSE_ACCEPT) {
        char *p = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(d));
        if (p) {
            snprintf(out, n, "%s", p);
            g_free(p);
            ok = 1;
        }
    }
    g_object_unref(d);
    return ok;
}

static void picked_pair(int k, const char *path)
{
    char other[2048];
    const char *ext = strrchr(path, '.');
    if (ext && !g_ascii_strcasecmp(ext, ".cue")) {
        start_check(K_CUE, path);
        if (rb_cue_bin_path(path, other, sizeof other))
            start_check(K_BIN, other);
    } else if (k == K_CUE) {
        start_check(K_CUE, path);
    } else {
        start_check(K_BIN, path);
        snprintf(other, sizeof other, "%s", path);
        if (strrchr(other, '.') && strrchr(other, '.') > strrchr(other, '/'))
            *strrchr(other, '.') = 0;
        g_strlcat(other, ".cue", sizeof other);
        if (is_file(other))
            start_check(K_CUE, other);
    }
}

static void on_pick(GtkButton *b, gpointer kp)
{
    static const char *const disc[] = { "*.bin", "*.BIN", NULL }, *const cue[] = { "*.cue", "*.CUE", NULL },
                             *const rom[] = { "*.z64", "*.v64", "*.n64", "*.Z64", "*.V64", "*.N64", NULL };
    int k = GPOINTER_TO_INT(kp);
    char p[2048] = "";
    (void)b;
    if (k == K_BIN || k == K_CUE) {
        if (pick(0, p, sizeof p, k == K_BIN ? "Disc image (.bin)" : "Cue sheet (.cue)", k == K_BIN ? disc : cue))
            picked_pair(k, p);
    } else if (pick(0, p, sizeof p, "N64 ROM (.z64, .v64, .n64)", rom)) {
        start_check(k, p);
    }
}

static void on_dest(void)
{
    if (pick(1, s_dest, sizeof s_dest, NULL, NULL))
        page_dumps();
}

static void on_back(void)
{
    if (s_page == 4)
        page_dumps();
    else
        page_game();
}

static void row(int k, const char *t, const char *md5)
{
    GtkWidget *h = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8), *b = gtk_button_new_with_label("Choose..."), *e;
    char *m = g_markup_printf_escaped("<b>%s</b>", t), *x;
    gtk_box_pack_start(GTK_BOX(h), label(m, 0), TRUE, TRUE, 0);
    g_free(m);
    g_signal_connect(b, "clicked", G_CALLBACK(on_pick), GINT_TO_POINTER(k));
    gtk_box_pack_end(GTK_BOX(h), b, FALSE, FALSE, 0);
    gtk_widget_set_margin_top(h, 10);
    gtk_box_pack_start(GTK_BOX(s_page_box), h, FALSE, FALSE, 0);
    x = g_markup_printf_escaped("<tt>Expected MD5  %s</tt>", md5);   /* (Top Gear Rally: any of its three, by name) */
    e = label(x, 0);
    g_free(x);
    gtk_box_pack_start(GTK_BOX(s_page_box), e, FALSE, FALSE, 0);
    s_rowtext[k] = label(s_file[k][0] ? "" : "<tt>No file chosen</tt>", 1);
    gtk_box_pack_start(GTK_BOX(s_page_box), s_rowtext[k], FALSE, FALSE, 0);
}

static void page_dumps(void)
{
    GtkWidget *h, *d, *b, *r;
    char *m;
    int k;
    s_page = 2;
    clear_page();
    if (s_game == RB_REMASTERED) {
        title(s_page_box, "Your copies of the games");
        text(s_page_box, "The games' data is copyrighted, so the builder cannot include it. Provide ROMs of the Top Gear "
                         "Rally (USA) and Top Gear Rally (Japan) cartridges (.z64, .v64 or .n64), and a BIN/CUE image of the "
                         "retail Boss Rally CD. Choosing either file of the image finds the other beside it.");
    } else if (s_game == RB_BOSS_RALLY) {
        title(s_page_box, "Your copy of the game");
        text(s_page_box, "The game's data is copyrighted, so the builder cannot include it. Provide a BIN/CUE image of the "
                         "retail Boss Rally CD. Choosing either file of the image finds the other beside it.");
    } else {
        title(s_page_box, "Your copy of the game");
        text(s_page_box, "The game's data is copyrighted, so the builder cannot include it. Provide a ROM of a Top Gear Rally "
                         "cartridge (.z64, .v64 or .n64): the USA's, Europe's or Japan's. The build is that cartridge's game.");
    }
    if (s_game == RB_TOP_GEAR_RALLY)
        row(K_ROM, "Top Gear Rally: cartridge ROM (USA, Europe or Japan)", RB_TGR_ROM_MD5 " (USA)");
    else if (rb_needs_rom(s_game))
        row(K_ROM, "Top Gear Rally (USA): cartridge ROM", RB_TGR_ROM_MD5);
    if (rb_needs_rom_jpn(s_game))
        row(K_ROMJPN, "Top Gear Rally (Japan): cartridge ROM", RB_TGR_JPN_MD5);
    if (rb_needs_disc(s_game)) {
        row(K_BIN, "Boss Rally: disc image (.bin)", RB_BR_BIN_MD5);
        row(K_CUE, "Boss Rally: cue sheet (.cue)", RB_BR_CUE_MD5);
    }
    h = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_pack_start(GTK_BOX(h), label("<b>Build into:</b>", 0), FALSE, FALSE, 0);
    m = g_markup_printf_escaped("<tt>%s</tt>", s_dest);
    d = label(m, 0);
    g_free(m);
    gtk_label_set_ellipsize(GTK_LABEL(d), PANGO_ELLIPSIZE_MIDDLE);
    gtk_box_pack_start(GTK_BOX(h), d, TRUE, TRUE, 0);
    b = gtk_button_new_with_label("Choose...");
    g_signal_connect(b, "clicked", G_CALLBACK(on_dest), NULL);
    gtk_box_pack_end(GTK_BOX(h), b, FALSE, FALSE, 0);
    gtk_widget_set_margin_top(h, 14);
    gtk_box_pack_start(GTK_BOX(s_page_box), h, FALSE, FALSE, 0);
    r = buttons();
    s_build = button(r, "_Build", G_CALLBACK(build), 1);
    button(r, "_Back", G_CALLBACK(on_back), 0);
    show_page();
    for (k = 0; k < K_N; k++)
        if (s_file[k][0] && s_rowtext[k])
            start_check(k, s_file[k]);
    update_build();
}

static void on_cancel(void)
{
    s_cancel = 1;
    if (s_cancel_btn)
        gtk_widget_set_sensitive(s_cancel_btn, FALSE);
    if (s_status)
        gtk_label_set_text(GTK_LABEL(s_status), "Stopping...");
}

static void page_build(void)
{
    char t[128];
    GtkWidget *r;
    s_page = 3;
    clear_page();
    snprintf(t, sizeof t, "Building %s", rb_game_name(s_game));
    title(s_page_box, t);
    s_bar = gtk_progress_bar_new();
    gtk_widget_set_margin_top(s_bar, 20);
    gtk_box_pack_start(GTK_BOX(s_page_box), s_bar, FALSE, FALSE, 0);
    s_status = text(s_page_box, "Starting");
    r = buttons();
    s_cancel_btn = button(r, "_Cancel", G_CALLBACK(on_cancel), 0);
    show_page();
}

static void desktop_file(char *out, size_t n)
{
    char name[600];
    rb_job job = { 0 };
    job.game = s_game;
    job.target = RB_LINUX;
    job.region = s_region;
    snprintf(name, sizeof name, "%s.desktop", rb_output_name(s_game, RB_LINUX, s_region));
    snprintf(out, n, "%s/%s", s_out, name);
}

static void on_show(void)
{
    char *uri = g_filename_to_uri(s_out, NULL, NULL);
    if (uri) {
        gtk_show_uri_on_window(GTK_WINDOW(s_win), uri, GDK_CURRENT_TIME, NULL);
        g_free(uri);
    }
}

static void on_play(void)
{
    char exe[2600];
    char *argv[2];
    snprintf(exe, sizeof exe, "%s/%s", s_out, rb_output_name(s_game, RB_LINUX, s_region));
    argv[0] = exe;
    argv[1] = NULL;
    g_spawn_async(s_out, argv, NULL, G_SPAWN_DEFAULT, NULL, NULL, NULL, NULL);
}

/* the build's launcher copied among the player's applications
 * (~/.local/share/applications): the desktop's menus list it, and so does
 * Steam's Add a Non-Steam Game */
static void on_add_apps(GtkButton *b)
{
    char from[2700], dir[2048], to[2800], name[600];
    const char *data = g_get_user_data_dir();
    GtkWidget *m;
    int ok;
    desktop_file(from, sizeof from);
    snprintf(dir, sizeof dir, "%s/applications", data);
    g_mkdir_with_parents(dir, 0755);
    /* (one file a game: a rebuild replaces it) */
    snprintf(name, sizeof name, "rally-builder-%s.desktop",
             s_game == RB_BOSS_RALLY ? "boss-rally" : s_game == RB_REMASTERED ? "top-gear-rally-remastered"
             : s_region == RB_EUROPE ? "top-gear-rally-europe" : s_region == RB_JAPAN ? "top-gear-rally-japan"
             : "top-gear-rally");
    snprintf(to, sizeof to, "%s/%s", dir, name);
    {
        gchar *d = NULL;
        gsize n = 0;
        ok = g_file_get_contents(from, &d, &n, NULL) && g_file_set_contents(to, d, (gssize)n, NULL);
        g_free(d);
    }
    if (ok)
        chmod(to, 0755);
    m = gtk_message_dialog_new(GTK_WINDOW(s_win), GTK_DIALOG_MODAL, ok ? GTK_MESSAGE_INFO : GTK_MESSAGE_ERROR,
                               GTK_BUTTONS_OK, ok ? "%s is in your applications" : "Could not add %s to your applications",
                               rb_game_name(s_game));
    if (ok)
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(m),
            "Find it in your desktop's menu. In Steam: Games > Add a Non-Steam Game to My Library, then tick it.");
    gtk_dialog_run(GTK_DIALOG(m));
    gtk_widget_destroy(m);
    if (ok)
        gtk_widget_set_sensitive(GTK_WIDGET(b), FALSE);
}

static void on_done(void) { gtk_widget_destroy(s_win); }

static void page_result(void)
{
    GtkWidget *r;
    char t[2048];
    s_page = 4;
    clear_page();
    if (s_result) {
        char *m;
        GtkWidget *p;
        snprintf(t, sizeof t, "%s is ready", rb_game_name(s_game));
        title(s_page_box, t);
        text(s_page_box, "The build is complete. It carries everything it needs, so your copies of the games are no longer "
                         "required to play.");
        m = g_markup_printf_escaped("<tt>%s</tt>", s_out);
        p = label(m, 0);
        g_free(m);
        gtk_label_set_ellipsize(GTK_LABEL(p), PANGO_ELLIPSIZE_MIDDLE);
        gtk_label_set_selectable(GTK_LABEL(p), TRUE);
        gtk_box_pack_start(GTK_BOX(s_page_box), p, FALSE, FALSE, 0);
        /* what the game has beyond the original's own screens (ports/: PORT.md, README.md) */
        text(s_page_box, s_game == RB_BOSS_RALLY
             ? "Alt+Enter switches to full screen and back. A controller works as it is plugged in."
             : s_game == RB_REMASTERED
             ? "Alt+Enter switches to full screen and back. A controller works as it is plugged in (on a Steam Deck, "
               "its own controls); the options screen's controller row sets each player's keys and pad buttons; Quit is "
               "on the main menu."
             : "Alt+Enter switches to full screen and back. A controller works as it is plugged in; Options, Keys and "
               "Pad sets the keys and pad buttons; Escape on the main menu, or the pause menu's Exit Game, quits.");
        text(s_page_box, "Add to Applications puts it in your desktop's menu, where Steam's Add a Non-Steam Game finds it "
                         "(on a Steam Deck, it then plays in Game Mode).");
        r = buttons();
        gtk_widget_grab_default(button(r, "_Play", G_CALLBACK(on_play), 1));
        button(r, "_Add to Applications", G_CALLBACK(on_add_apps), 0);
        button(r, "_Open Folder", G_CALLBACK(on_show), 0);
        button(r, "_Done", G_CALLBACK(on_done), 0);
    } else if (s_cancel) {
        title(s_page_box, "The build was cancelled");
        text(s_page_box, "Nothing was left behind in the folder.");
        r = buttons();
        button(r, "_Back", G_CALLBACK(on_back), 1);
        button(r, "_Quit", G_CALLBACK(on_done), 0);
    } else {
        char bad[1024];
        GtkWidget *e;
        snprintf(t, sizeof t, "%s could not be built", rb_game_name(s_game));
        title(s_page_box, t);
        snprintf(t, sizeof t, "Error: %s", s_err[0] ? s_err : "the build failed.");
        e = text(s_page_box, t);
        {
            char *m = g_markup_printf_escaped("<span foreground='#c00000'>%s</span>", t);
            gtk_label_set_markup(GTK_LABEL(e), m);
            g_free(m);
        }
        if (s_unchecked) {
            mismatched(bad, sizeof bad, ", ");
            snprintf(t, sizeof t, "The build used files that do not match the dumps Rally Builder is made for (%s), which is "
                                  "the likely cause. Choose the matching dumps and build again.", bad);
            text(s_page_box, t);
        }
        text(s_page_box, "Nothing was left behind in the folder.");
        r = buttons();
        button(r, "_Back", G_CALLBACK(on_back), 1);
        button(r, "_Quit", G_CALLBACK(on_done), 0);
    }
    show_page();
}

/* ---- checks ------------------------------------------------------------------------------ */
typedef struct check_job { int k; char path[2048]; rb_check c; } check_job;

static gboolean checked(gpointer p)
{
    check_job *j = (check_job *)p;
    s_checking--;
    if (!strcmp(j->path, s_file[j->k])) {
        /* Remastered's rows want one cartridge each: the USA's, Japan's */
        int want = j->k == K_ROMJPN ? RB_JAPAN : j->k == K_ROM && s_game == RB_REMASTERED ? RB_USA : -1;
        if (j->c.ok && want >= 0 && j->c.region != want) {
            snprintf(j->c.note, sizeof j->c.note, "This is Top Gear Rally (%s); this one is Top Gear Rally (%s).",
                     rb_region_name(j->c.region), rb_region_name(want));
            j->c.ok = 0;
        }
        s_ok[j->k] = j->c.ok;
        if (j->k == K_ROM)
            s_region = j->c.region;
        if (s_rowtext[j->k]) {
            char *m = g_markup_printf_escaped("<tt>%s\nYour file's MD5  %s  </tt><span foreground='%s'>%s</span>%s%s",
                                              base(j->path), j->c.md5, j->c.ok ? "#008000" : "#c00000",
                                              j->c.ok ? "✓ matches" : "✗ does not match",
                                              j->c.note[0] ? "\n" : "", j->c.note);
            gtk_label_set_markup(GTK_LABEL(s_rowtext[j->k]), m);
            g_free(m);
        }
    }
    free(j);
    update_build();
    return G_SOURCE_REMOVE;
}

static gpointer check_thread(gpointer p)
{
    check_job *j = (check_job *)p;
    if (j->k == K_BIN)
        rb_check_bin(j->path, &j->c, NULL, NULL, NULL);
    else if (j->k == K_CUE)
        rb_check_cue(j->path, &j->c);
    else
        rb_check_rom(j->path, &j->c, NULL, NULL, NULL);
    g_idle_add(checked, j);
    return NULL;
}

static void start_check(int k, const char *path)
{
    check_job *j = calloc(1, sizeof *j);
    j->k = k;
    snprintf(j->path, sizeof j->path, "%s", path);
    if (s_file[k] != path)
        snprintf(s_file[k], sizeof s_file[k], "%s", path);
    s_ok[k] = 0;
    if (s_rowtext[k]) {
        char *m = g_markup_printf_escaped("<tt>%s: checking...</tt>", base(path));
        gtk_label_set_markup(GTK_LABEL(s_rowtext[k]), m);
        g_free(m);
    }
    s_checking++;
    update_build();
    g_thread_unref(g_thread_new("check", check_thread, j));
}

/* ---- the build ------------------------------------------------------------------------------- */
static double s_fraction;
static char s_status_text[256];
static GMutex s_pm;
static int s_progress_queued;

static gboolean progressed(gpointer p)
{
    (void)p;
    g_mutex_lock(&s_pm);
    s_progress_queued = 0;
    if (s_bar)
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(s_bar), s_fraction);
    if (s_status && !s_cancel)
        gtk_label_set_text(GTK_LABEL(s_status), s_status_text);
    g_mutex_unlock(&s_pm);
    return G_SOURCE_REMOVE;
}

static void progress(void *ctx, double f, const char *s)
{
    (void)ctx;
    g_mutex_lock(&s_pm);
    s_fraction = f;
    if (s)
        snprintf(s_status_text, sizeof s_status_text, "%s", s);
    if (!s_progress_queued) {          /* (one update queued at a time: the encoder reports often) */
        s_progress_queued = 1;
        g_idle_add(progressed, NULL);
    }
    g_mutex_unlock(&s_pm);
}

static gboolean built(gpointer p)
{
    (void)p;
    page_result();
    return G_SOURCE_REMOVE;
}

static gpointer build_thread(gpointer p)
{
    rb_job job = { 0 };
    (void)p;
    job.game = s_game;
    job.target = RB_LINUX;
    job.dest_dir = s_dest;
    job.bin = s_file[K_BIN];
    job.cue = s_file[K_CUE];
    job.rom = s_file[K_ROM];
    job.rom_jpn = s_file[K_ROMJPN];
    job.region = s_region;
    job.unchecked = s_unchecked;
    s_err[0] = 0;
    s_result = rb_build(&job, progress, NULL, &s_cancel, s_err, sizeof s_err);
    rb_output_path(&job, s_out, sizeof s_out);
    g_idle_add(built, NULL);
    return NULL;
}

static int ask(GtkMessageType type, const char *head, const char *body)
{
    GtkWidget *m = gtk_message_dialog_new(GTK_WINDOW(s_win), GTK_DIALOG_MODAL, type, GTK_BUTTONS_YES_NO, "%s", head);
    int r;
    gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(m), "%s", body);
    gtk_dialog_set_default_response(GTK_DIALOG(m), GTK_RESPONSE_NO);
    r = gtk_dialog_run(GTK_DIALOG(m));
    gtk_widget_destroy(m);
    return r == GTK_RESPONSE_YES;
}

static void build(void)
{
    rb_job job = { 0 };
    char out[2048], msg[4096];
    int ours = 0;
    s_unchecked = 0;
    {
        char bad[1024];
        int n = mismatched(bad, sizeof bad, "\n");
        if (n) {
            snprintf(msg, sizeof msg, "%s\n\nRally Builder is made for the dumps whose MD5s it lists. A file that does not "
                                      "match may be another version of the game, a modified copy or a bad dump: the build "
                                      "may fail, or the game it makes may not work properly.\n\nBuild anyway?", bad);
            if (!ask(GTK_MESSAGE_WARNING, n == 1 ? "This file does not match" : "These files do not match", msg))
                return;
            s_unchecked = 1;
        }
    }
    job.game = s_game;
    job.target = RB_LINUX;
    job.dest_dir = s_dest;
    if (s_game == RB_TOP_GEAR_RALLY && s_region < 0) {      /* (--rom: not checked by a row) */
        rb_check c;
        rb_check_rom(s_file[K_ROM], &c, NULL, NULL, NULL);
        s_region = c.region;
    }
    job.region = s_region;                                  /* the earlier build looked for is this cartridge's */
    if (rb_output_exists(&job, &ours)) {
        rb_output_path(&job, out, sizeof out);
        if (!ours) {
            GtkWidget *m = gtk_message_dialog_new(GTK_WINDOW(s_win), GTK_DIALOG_MODAL, GTK_MESSAGE_WARNING, GTK_BUTTONS_OK,
                                                  "%s already exists", out);
            gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(m), "It was not made by Rally Builder, so it will "
                                                     "not be replaced. Move it away or choose another folder.");
            gtk_dialog_run(GTK_DIALOG(m));
            gtk_widget_destroy(m);
            return;
        }
        snprintf(msg, sizeof msg, "Your saved games are kept; they live in your home folder (~/.local/share), not in "
                                  "the game's.");
        if (!ask(GTK_MESSAGE_QUESTION, "Replace the earlier build?", msg))
            return;
    }
    s_cancel = 0;
    page_build();
    g_thread_unref(g_thread_new("build", build_thread, NULL));
}

/* ---- the window ------------------------------------------------------------------------------ */
static gboolean on_close(void)
{
    if (s_page == 3) {                 /* stop the build first; it cleans up after itself */
        on_cancel();
        return TRUE;
    }
    return FALSE;
}

int main(int argc, char **argv)
{
    int i, go = 0, nogui = 0;
    snprintf(s_dest, sizeof s_dest, "%s/Games", g_get_home_dir());
    for (i = 1; i < argc; i++) {
        const char *k = argv[i], *v = i + 1 < argc ? argv[i + 1] : "";
        if (!strcmp(k, "--build")) { go = 1; continue; }
        if (!strcmp(k, "--cli")) { nogui = 1; continue; }
        if (!strcmp(k, "--version")) { printf("Rally Builder %s\n", RB_VERSION); return 0; }
        if (!strcmp(k, "--game"))
            s_game = !strcmp(v, "br") ? RB_BOSS_RALLY : !strcmp(v, "tgr") ? RB_TOP_GEAR_RALLY : RB_REMASTERED;
        else if (!strcmp(k, "--dest")) snprintf(s_dest, sizeof s_dest, "%s", v);
        else if (!strcmp(k, "--bin")) { snprintf(s_file[K_BIN], sizeof s_file[K_BIN], "%s", v); s_ok[K_BIN] = 1; }
        else if (!strcmp(k, "--cue")) { snprintf(s_file[K_CUE], sizeof s_file[K_CUE], "%s", v); s_ok[K_CUE] = 1; }
        else if (!strcmp(k, "--rom")) { snprintf(s_file[K_ROM], sizeof s_file[K_ROM], "%s", v); s_ok[K_ROM] = 1; }
        else if (!strcmp(k, "--rom-jpn")) { snprintf(s_file[K_ROMJPN], sizeof s_file[K_ROMJPN], "%s", v); s_ok[K_ROMJPN] = 1; }
        else continue;
        i++;
    }
    if (nogui)
        return cli();
    if (!gtk_init_check(&argc, &argv)) {
        fprintf(stderr, "rally-builder: no display; --cli builds without one\n");
        return 1;
    }
    g_set_prgname("rally-builder");
    s_win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(s_win), "Rally Builder");
    gtk_window_set_default_size(GTK_WINDOW(s_win), 640, 640);
    {
        char icon[2100], dir[2048];
        games_dir(dir, sizeof dir);
        snprintf(icon, sizeof icon, "%s/icon.png", dir);
        gtk_window_set_icon_from_file(GTK_WINDOW(s_win), icon, NULL);
    }
    g_signal_connect(s_win, "delete-event", G_CALLBACK(on_close), NULL);
    g_signal_connect(s_win, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    page_game();
    if (go)
        build();                       /* a scripted run: the dumps were checked by whoever named them */
    gtk_main();
    return 0;
}
