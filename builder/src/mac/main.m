/* main.m: the release builder's macOS front end (AppKit).
 *
 *   1 the game, and the folder to build it into
 *   2 the player's own dump of it (BIN and cue, or the ROM), checked by MD5
 *   3 the build (core/build.c), with progress
 *   4 the result: the app, or what went wrong
 *
 * Arguments fill in the choices for a scripted run:
 *   --game br|tgr  --dest DIR  --bin FILE  --cue FILE  --rom FILE
 * RB_SNAPSHOT=DIR writes each page the window shows to DIR as a PNG.
 */
#import <Cocoa/Cocoa.h>
#include <stdatomic.h>
#include "rb.h"

int rb_host_payload(const char *name, const char *path, char *err, size_t errlen)
{
    @autoreleasepool {
        NSString *src = [[NSBundle mainBundle] pathForResource:@(name) ofType:nil inDirectory:@"payload"];
        NSData *d = src ? [NSData dataWithContentsOfFile:src] : nil;
        if (!d || ![d writeToFile:@(path) atomically:NO]) {
            snprintf(err, errlen, "the builder is missing its copy of the game (%s); download it again", name);
            return 0;
        }
        return 1;
    }
}

@interface Builder : NSObject <NSApplicationDelegate, NSWindowDelegate>
@property (strong) NSWindow *window;
@property (strong) NSView *page;
@property int game;
@property (copy) NSString *dest, *bin, *cue, *rom;
@property BOOL binOK, cueOK, romOK;
@property (strong) NSTextField *destLabel;
@property (strong) NSMutableDictionary<NSString *, NSTextField *> *rows;
@property (strong) NSButton *buildButton;
@property (strong) NSProgressIndicator *bar;
@property (strong) NSTextField *status;
@property (copy) NSString *lastStatus;
@end

static volatile int g_cancel;
static atomic_int g_checks;      /* hashes still running */

static NSTextField *label(NSString *s, CGFloat size, BOOL bold)
{
    NSTextField *t = [NSTextField wrappingLabelWithString:s];
    t.font = bold ? [NSFont boldSystemFontOfSize:size] : [NSFont systemFontOfSize:size];
    t.selectable = YES;
    return t;
}

static NSTextField *mono(NSString *s)
{
    NSTextField *t = [NSTextField labelWithString:s];
    t.font = [NSFont monospacedSystemFontOfSize:11 weight:NSFontWeightRegular];
    t.textColor = NSColor.secondaryLabelColor;
    t.selectable = YES;
    return t;
}

static NSStackView *vstack(NSArray<NSView *> *views, CGFloat spacing)
{
    NSStackView *s = [NSStackView stackViewWithViews:views];
    s.orientation = NSUserInterfaceLayoutOrientationVertical;
    s.alignment = NSLayoutAttributeLeading;
    s.spacing = spacing;
    return s;
}

static NSStackView *hstack(NSArray<NSView *> *views)
{
    NSStackView *s = [NSStackView stackViewWithViews:views];
    s.orientation = NSUserInterfaceLayoutOrientationHorizontal;
    s.spacing = 8;
    return s;
}

static void progress_cb(void *ctx, double f, const char *s)
{
    Builder *b = (__bridge Builder *)ctx;
    NSString *str = s ? @(s) : nil;
    dispatch_async(dispatch_get_main_queue(), ^{
        b.bar.doubleValue = f * 100;
        if (str)
            b.status.stringValue = str;
    });
}

@implementation Builder

- (void)applicationDidFinishLaunching:(NSNotification *)n
{
    NSMenu *bar = [NSMenu new], *app = [NSMenu new];
    NSMenuItem *item = [NSMenuItem new];
    [app addItemWithTitle:@"Quit Rally Builder" action:@selector(terminate:) keyEquivalent:@"q"];
    item.submenu = app;
    [bar addItem:item];
    NSApp.mainMenu = bar;

    self.dest = [NSSearchPathForDirectoriesInDomains(NSApplicationDirectory, NSUserDomainMask, YES) firstObject];
    {
        NSArray<NSString *> *a = NSProcessInfo.processInfo.arguments;
        NSUInteger i;
        for (i = 1; i + 1 < a.count; i++) {
            NSString *k = a[i], *v = a[i + 1];
            if ([k isEqualToString:@"--game"]) self.game = [v isEqualToString:@"tgr"] ? RB_TOP_GEAR_RALLY : RB_BOSS_RALLY;
            else if ([k isEqualToString:@"--dest"]) self.dest = v;
            else if ([k isEqualToString:@"--bin"]) self.bin = v;
            else if ([k isEqualToString:@"--cue"]) self.cue = v;
            else if ([k isEqualToString:@"--rom"]) self.rom = v;
            else continue;
            i++;
        }
    }
    self.window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 600, 420)
                                              styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable
                                                backing:NSBackingStoreBuffered defer:NO];
    self.window.title = @"Rally Builder";
    self.window.delegate = self;
    [self.window center];
    [self showChoose];
    [self.window makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)a { return YES; }

- (void)setPage:(NSView *)content buttons:(NSArray<NSButton *> *)buttons
{
    NSView *root = [NSView new];
    NSStackView *bb = hstack(buttons);
    content.translatesAutoresizingMaskIntoConstraints = NO;
    bb.translatesAutoresizingMaskIntoConstraints = NO;
    [root addSubview:content];
    [root addSubview:bb];
    [NSLayoutConstraint activateConstraints:@[
        [content.topAnchor constraintEqualToAnchor:root.topAnchor constant:24],
        [content.leadingAnchor constraintEqualToAnchor:root.leadingAnchor constant:28],
        [content.trailingAnchor constraintEqualToAnchor:root.trailingAnchor constant:-28],
        [bb.trailingAnchor constraintEqualToAnchor:root.trailingAnchor constant:-20],
        [bb.bottomAnchor constraintEqualToAnchor:root.bottomAnchor constant:-20],
        [content.bottomAnchor constraintLessThanOrEqualToAnchor:bb.topAnchor constant:-16],
    ]];
    self.window.contentView = root;
    [self snapshot];
}

- (void)snapshot
{
    static int n;
    NSString *dir = NSProcessInfo.processInfo.environment[@"RB_SNAPSHOT"];
    if (!dir)
        return;
    int k = ++n;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 300 * NSEC_PER_MSEC), dispatch_get_main_queue(), ^{
        NSView *v = self.window.contentView;
        NSRect r = v.bounds;
        NSImage *pdf = [[NSImage alloc] initWithData:[v dataWithPDFInsideRect:r]];
        NSImage *img = [[NSImage alloc] initWithSize:r.size];
        [v.effectiveAppearance performAsCurrentDrawingAppearance:^{
            [img lockFocus];
            [NSColor.windowBackgroundColor setFill];
            NSRectFill(r);
            [pdf drawInRect:r];
            [img unlockFocus];
        }];
        NSBitmapImageRep *rep = [[NSBitmapImageRep alloc] initWithData:img.TIFFRepresentation];
        [[rep representationUsingType:NSBitmapImageFileTypePNG properties:@{}]
            writeToFile:[dir stringByAppendingPathComponent:[NSString stringWithFormat:@"page%02d.png", k]] atomically:YES];
    });
}

- (NSButton *)button:(NSString *)t action:(SEL)a
{
    NSButton *b = [NSButton buttonWithTitle:t target:self action:a];
    return b;
}

/* ---- 1: the game and the folder ------------------------------------------------------------ */
- (void)showChoose
{
    NSButton *br = [NSButton radioButtonWithTitle:@"Boss Rally (PC, 1999)" target:self action:@selector(pickGame:)];
    NSButton *tg = [NSButton radioButtonWithTitle:@"Top Gear Rally (Nintendo 64, 1997)" target:self action:@selector(pickGame:)];
    br.tag = RB_BOSS_RALLY;
    tg.tag = RB_TOP_GEAR_RALLY;
    (self.game == RB_BOSS_RALLY ? br : tg).state = NSControlStateValueOn;
    self.destLabel = mono(self.dest);
    self.destLabel.lineBreakMode = NSLineBreakByTruncatingMiddle;
    [self.destLabel setContentCompressionResistancePriority:NSLayoutPriorityDefaultLow forOrientation:NSLayoutConstraintOrientationHorizontal];
    NSButton *choose = [self button:@"Choose..." action:@selector(chooseDest:)];
    NSButton *next = [self button:@"Continue" action:@selector(toDumps:)];
    next.keyEquivalent = @"\r";
    [self setPage:vstack(@[
        label(@"Build a game", 20, YES),
        label(@"Rally Builder makes a native Mac build of a game from your own copy of it. Choose the game, and where to put it.", 13, NO),
        br, tg,
        label(@"Build into:", 13, YES),
        hstack(@[ self.destLabel, choose ]),
    ], 12) buttons:@[ next ]];
}

- (void)pickGame:(NSButton *)b { self.game = (int)b.tag; }

- (void)chooseDest:(id)s
{
    NSOpenPanel *p = [NSOpenPanel openPanel];
    p.canChooseDirectories = YES;
    p.canChooseFiles = NO;
    p.canCreateDirectories = YES;
    p.prompt = @"Choose";
    p.directoryURL = [NSURL fileURLWithPath:self.dest];
    [p beginSheetModalForWindow:self.window completionHandler:^(NSModalResponse r) {
        if (r == NSModalResponseOK) {
            self.dest = p.URL.path;
            self.destLabel.stringValue = self.dest;
        }
    }];
}

/* ---- 2: the player's dump ------------------------------------------------------------------- */
- (NSView *)row:(NSString *)key title:(NSString *)title expect:(const char *)md5
{
    NSButton *b = [self button:@"Choose..." action:@selector(chooseFile:)];
    b.identifier = key;
    NSTextField *state = mono(@"No file chosen");
    state.lineBreakMode = NSLineBreakByTruncatingMiddle;
    self.rows[key] = state;
    return vstack(@[ hstack(@[ label(title, 13, YES), b ]),
                     mono([NSString stringWithFormat:@"Expected MD5  %s", md5]),
                     state ], 4);
}

- (void)toDumps:(id)s
{
    self.rows = [NSMutableDictionary new];
    NSButton *back = [self button:@"Back" action:@selector(backToChoose:)];
    self.buildButton = [self button:@"Build" action:@selector(build:)];
    self.buildButton.keyEquivalent = @"\r";
    NSMutableArray *v = [NSMutableArray arrayWithObjects:
        label([NSString stringWithFormat:@"Your copy of %s", rb_game_name(self.game)], 20, YES), nil];
    if (self.game == RB_BOSS_RALLY) {
        [v addObject:label(@"The game's data is copyrighted, so the builder cannot include it. Provide a BIN/CUE image of the retail Boss Rally CD (the data track and the 12 music tracks). Choosing either file finds the other beside it.", 13, NO)];
        [v addObject:[self row:@"bin" title:@"Disc image (.bin)" expect:RB_BR_BIN_MD5]];
        [v addObject:[self row:@"cue" title:@"Cue sheet (.cue)" expect:RB_BR_CUE_MD5]];
        if (self.bin) [self check:@"bin" path:self.bin];
        if (self.cue) [self check:@"cue" path:self.cue];
    } else {
        [v addObject:label(@"The game's data is copyrighted, so the builder cannot include it. Provide a ROM of the Top Gear Rally (USA) cartridge, in .z64, .v64 or .n64 byte order.", 13, NO)];
        [v addObject:[self row:@"rom" title:@"Cartridge ROM" expect:RB_TGR_ROM_MD5]];
        if (self.rom) [self check:@"rom" path:self.rom];
    }
    [self setPage:vstack(v, 14) buttons:@[ back, self.buildButton ]];
    [self updateBuildButton];
}

- (void)backToChoose:(id)s { [self showChoose]; }

- (void)updateBuildButton
{
    BOOL ok = self.game == RB_BOSS_RALLY ? (self.binOK && self.cueOK) : self.romOK;
    self.buildButton.enabled = ok && atomic_load(&g_checks) == 0;
}

- (void)chooseFile:(NSButton *)b
{
    NSString *key = b.identifier;
    NSOpenPanel *p = [NSOpenPanel openPanel];
    p.canChooseFiles = YES;
    p.canChooseDirectories = NO;
    [p beginSheetModalForWindow:self.window completionHandler:^(NSModalResponse r) {
        if (r != NSModalResponseOK)
            return;
        NSString *path = p.URL.path;
        if ([key isEqualToString:@"rom"]) {
            [self check:@"rom" path:path];
            return;
        }
        /* either file of the pair: find the other beside it */
        NSString *ext = path.pathExtension.lowercaseString;
        if ([ext isEqualToString:@"cue"]) {
            char other[2048];
            [self check:@"cue" path:path];
            if (rb_cue_bin_path(path.fileSystemRepresentation, other, sizeof other))
                [self check:@"bin" path:@(other)];
        } else if ([key isEqualToString:@"cue"]) {
            [self check:@"cue" path:path];
        } else {
            [self check:@"bin" path:path];
            NSString *base = path.stringByDeletingPathExtension;
            for (NSString *e in @[ @"cue", @"CUE", @"Cue" ]) {
                NSString *c = [base stringByAppendingPathExtension:e];
                if ([NSFileManager.defaultManager fileExistsAtPath:c]) {
                    [self check:@"cue" path:c];
                    break;
                }
            }
        }
    }];
}

- (void)check:(NSString *)key path:(NSString *)path
{
    NSTextField *row = self.rows[key];
    NSString *name = path.lastPathComponent;
    if ([key isEqualToString:@"bin"]) { self.bin = path; self.binOK = NO; }
    if ([key isEqualToString:@"cue"]) { self.cue = path; self.cueOK = NO; }
    if ([key isEqualToString:@"rom"]) { self.rom = path; self.romOK = NO; }
    row.stringValue = [NSString stringWithFormat:@"%@: checking...", name];
    row.textColor = NSColor.secondaryLabelColor;
    atomic_fetch_add(&g_checks, 1);
    [self updateBuildButton];
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        rb_check c;
        const char *p = path.fileSystemRepresentation;
        if ([key isEqualToString:@"bin"])
            rb_check_bin(p, &c, NULL, NULL, NULL);
        else if ([key isEqualToString:@"cue"])
            rb_check_cue(p, &c);
        else
            rb_check_rom(p, &c, NULL, NULL, NULL);
        dispatch_async(dispatch_get_main_queue(), ^{
            BOOL current = [key isEqualToString:@"bin"] ? [self.bin isEqualToString:path]
                         : [key isEqualToString:@"cue"] ? [self.cue isEqualToString:path] : [self.rom isEqualToString:path];
            atomic_fetch_sub(&g_checks, 1);
            if (current) {
                NSString *verdict = c.ok ? @"\u2713 matches" : @"\u2717 does not match";
                NSString *note = c.note[0] ? [NSString stringWithFormat:@"\n%s", c.note] : @"";
                row.stringValue = [NSString stringWithFormat:@"%@\nYour file's MD5  %s  %@%@", name, c.md5, verdict, note];
                row.textColor = c.ok ? NSColor.systemGreenColor : NSColor.systemRedColor;
                if ([key isEqualToString:@"bin"]) self.binOK = c.ok;
                if ([key isEqualToString:@"cue"]) self.cueOK = c.ok;
                if ([key isEqualToString:@"rom"]) self.romOK = c.ok;
            }
            [self updateBuildButton];
            [self snapshot];
        });
    });
}

/* ---- 3: the build ------------------------------------------------------------------------------ */
- (void)build:(id)s
{
    rb_job job = { 0 };
    int ours = 0;
    job.game = self.game;
    job.dest_dir = self.dest.fileSystemRepresentation;
    if (rb_output_exists(&job, &ours)) {
        char out[2048];
        rb_output_path(&job, out, sizeof out);
        NSAlert *a = [NSAlert new];
        if (!ours) {
            a.messageText = [NSString stringWithFormat:@"%s already exists", rb_output_name(self.game)];
            a.informativeText = [NSString stringWithFormat:@"%s was not made by Rally Builder, so it will not be replaced. Move it away or choose another folder.", out];
            [a beginSheetModalForWindow:self.window completionHandler:nil];
            return;
        }
        a.messageText = [NSString stringWithFormat:@"Replace %s?", rb_output_name(self.game)];
        a.informativeText = [NSString stringWithFormat:@"An earlier build is at %s. Your saved games are kept; they live in your Library, not in the app.", out];
        [a addButtonWithTitle:@"Replace"];
        [a addButtonWithTitle:@"Cancel"];
        [a beginSheetModalForWindow:self.window completionHandler:^(NSModalResponse r) {
            if (r == NSAlertFirstButtonReturn)
                [self startBuild];
        }];
        return;
    }
    [self startBuild];
}

- (void)startBuild
{
    NSButton *cancel = [self button:@"Cancel" action:@selector(cancelBuild:)];
    self.bar = [NSProgressIndicator new];
    self.bar.indeterminate = NO;
    self.bar.minValue = 0;
    self.bar.maxValue = 100;
    [self.bar.widthAnchor constraintEqualToConstant:540].active = YES;
    self.status = label(@"Starting", 13, NO);
    [self setPage:vstack(@[ label([NSString stringWithFormat:@"Building %s", rb_game_name(self.game)], 20, YES),
                            self.bar, self.status ], 14) buttons:@[ cancel ]];
    g_cancel = 0;
    int game = self.game;
    NSString *dest = self.dest, *bin = self.bin, *cue = self.cue, *rom = self.rom;
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        rb_job job = { 0 };
        char err[1024] = "", out[2048];
        job.game = game;
        job.dest_dir = dest.fileSystemRepresentation;
        job.bin = bin.fileSystemRepresentation;
        job.cue = cue.fileSystemRepresentation;
        job.rom = rom.fileSystemRepresentation;
        int ok = rb_build(&job, progress_cb, (__bridge void *)self, &g_cancel, err, sizeof err);
        rb_output_path(&job, out, sizeof out);
        NSString *e = @(err), *o = @(out);
        dispatch_async(dispatch_get_main_queue(), ^{ [self showResult:ok error:e output:o]; });
    });
}

- (void)cancelBuild:(NSButton *)b
{
    g_cancel = 1;
    b.enabled = NO;
    self.status.stringValue = @"Stopping...";
}

/* ---- 4: the result ------------------------------------------------------------------------------ */
- (void)showResult:(int)ok error:(NSString *)err output:(NSString *)out
{
    if (ok) {
        NSButton *show = [self button:@"Show in Finder" action:@selector(reveal:)];
        NSButton *play = [self button:@"Open the Game" action:@selector(play:)];
        NSButton *done = [self button:@"Done" action:@selector(terminate:)];
        show.identifier = play.identifier = out;
        play.keyEquivalent = @"\r";
        [self setPage:vstack(@[ label([NSString stringWithFormat:@"%s is ready", rb_game_name(self.game)], 20, YES),
                                label(@"The build is complete. It carries everything it needs, so your disc image or ROM is no longer required to play.", 13, NO),
                                mono(out) ], 14) buttons:@[ done, show, play ]];
    } else {
        NSButton *back = [self button:@"Back" action:@selector(toDumps:)];
        NSButton *quit = [self button:@"Quit" action:@selector(terminate:)];
        NSTextField *msg = label(err.length ? err : @"The build failed.", 13, NO);
        msg.textColor = NSColor.systemRedColor;
        [self setPage:vstack(@[ label(@"The build did not finish", 20, YES), msg ], 14) buttons:@[ quit, back ]];
    }
}

- (void)reveal:(NSButton *)b { [NSWorkspace.sharedWorkspace activateFileViewerSelectingURLs:@[ [NSURL fileURLWithPath:b.identifier] ]]; }

- (void)play:(NSButton *)b
{
    [NSWorkspace.sharedWorkspace openApplicationAtURL:[NSURL fileURLWithPath:b.identifier]
                                        configuration:[NSWorkspaceOpenConfiguration configuration]
                                    completionHandler:nil];
}

- (void)terminate:(id)s { [NSApp terminate:nil]; }

@end

int main(int argc, const char **argv)
{
    (void)argc;
    (void)argv;
    @autoreleasepool {
        NSApplication *app = [NSApplication sharedApplication];
        Builder *b = [Builder new];
        app.activationPolicy = NSApplicationActivationPolicyRegular;
        app.delegate = b;
        [app run];
    }
    return 0;
}
