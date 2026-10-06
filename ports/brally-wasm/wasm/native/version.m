/* version.m -- which version of Boss Rally the port is being: the PC game or
 * the N64 one, switched live with Tab.
 *
 * Every part of the port that has a PC and an N64 form (the soundtrack, for
 * now) follows the one switch here, all at once. The choice is remembered
 * between launches. Tab belongs to the switch: the game never sees it
 * (host_app.m), so a control bound to Tab in the game's own Controls screen
 * does nothing.
 *
 * On a switch, and when the window first comes up, the version's icon shows
 * in the top right corner of the window and fades away: the N64 logo, or a
 * beige PC. The fade is a Core Animation keyframe animation, so it runs at
 * the display's rate whatever the game's frame loop is doing, and once it
 * ends the icon's layer is hidden and costs the compositor nothing.
 *
 * Icons: the app's Resources/assets (package_app.sh, rasterised from the
 * SVGs), else the tree's ports/common/assets. Headless there is no icon;
 * the switch still works (scripts: `version`, a real Tab key event).
 */
#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>
#include <math.h>
#include <stdlib.h>
#include "host.h"

NSWindow *happ_window(void);

#define ICON_H      0.10     /* of the view's height */
#define ICON_MIN    40.0     /* points */
#define ICON_MAX    112.0
#define MARGIN      0.025    /* of the view's height */
#define SHOW_IN     0.12     /* seconds */
#define SHOW_HOLD   1.40
#define SHOW_OUT    0.60
#define SHOW_ALPHA  0.92

static int g_version = -1;
static NSImageView *g_icon;
static NSImage *g_img[2];

static NSImage *load(int v)
{
    NSString *png = v == HV_N64 ? @"n64.png" : @"retro_pc.png";
    NSString *src = v == HV_N64 ? @"n64.svg" : @"retro_pc.svg";
    NSString *res = [[[NSBundle mainBundle] resourcePath] stringByAppendingPathComponent:@"assets"];
    NSString *tree = [@(getenv("BR_ROOT") ? getenv("BR_ROOT") : ".") stringByAppendingPathComponent:@"ports/common/assets"];
    NSImage *i = [[NSImage alloc] initWithContentsOfFile:[res stringByAppendingPathComponent:png]];
    if (!i) i = [[NSImage alloc] initWithContentsOfFile:[tree stringByAppendingPathComponent:src]];
    if (!i) fprintf(stderr, "version: no icon %s (%s, %s)\n", png.UTF8String, res.UTF8String, tree.UTF8String);
    return i;
}

int hversion(void)
{
    if (g_version < 0) {
        NSUserDefaults *d = [NSUserDefaults standardUserDefaults];
        NSString *p = [d stringForKey:@"Version"];
        if (!p) p = [d stringForKey:@"Soundtrack"];        /* the Music menu's choice, before Tab */
        g_version = [p isEqualToString:@"n64"] ? HV_N64 : HV_PC;
    }
    return g_version;
}

/* The icon of the version playing, in the top right corner, faded in, held
 * and faded out. */
static void show(void)
{
    NSWindow *win = happ_window();
    NSView *v = win.contentView;
    NSImage *img;
    CAKeyframeAnimation *a;
    double h, m, w, t;
    if (!v) return;
    if (!g_img[HV_PC]) { g_img[HV_PC] = load(HV_PC); g_img[HV_N64] = load(HV_N64); }
    img = g_img[hversion()];
    if (!img) return;
    if (!g_icon) {
        g_icon = [NSImageView new];
        g_icon.imageScaling = NSImageScaleProportionallyUpOrDown;
        g_icon.autoresizingMask = NSViewMinXMargin | NSViewMinYMargin;
        g_icon.wantsLayer = YES;
        g_icon.layer.opacity = 0;
        g_icon.layer.shadowColor = NSColor.blackColor.CGColor;
        g_icon.layer.shadowOpacity = 0.55f;
        g_icon.layer.shadowRadius = 3;
        g_icon.layer.shadowOffset = CGSizeMake(0, -1);
        [v addSubview:g_icon];
    }
    h = fmin(fmax(v.bounds.size.height * ICON_H, ICON_MIN), ICON_MAX);
    w = h * img.size.width / img.size.height;
    m = fmax(v.bounds.size.height * MARGIN, 8);
    g_icon.image = img;
    g_icon.frame = NSMakeRect(v.bounds.size.width - m - w, v.bounds.size.height - m - h, w, h);
    t = SHOW_IN + SHOW_HOLD + SHOW_OUT;
    a = [CAKeyframeAnimation animationWithKeyPath:@"opacity"];
    a.values = @[ @0, @(SHOW_ALPHA), @(SHOW_ALPHA), @0 ];
    a.keyTimes = @[ @0, @(SHOW_IN / t), @((SHOW_IN + SHOW_HOLD) / t), @1 ];
    a.duration = t;
    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    g_icon.hidden = NO;
    g_icon.layer.opacity = 0;                        /* where it rests when the fade ends */
    [CATransaction setCompletionBlock:^{
        if (![g_icon.layer animationForKey:@"show"]) g_icon.hidden = YES;
    }];
    [g_icon.layer removeAnimationForKey:@"show"];
    [g_icon.layer addAnimation:a forKey:@"show"];
    [CATransaction commit];
}

void hversion_toggle(void)
{
    int v = !hversion();
    g_version = v;
    [[NSUserDefaults standardUserDefaults] setObject:v == HV_N64 ? @"n64" : @"pc" forKey:@"Version"];
    [[NSUserDefaults standardUserDefaults] removeObjectForKey:@"Soundtrack"];
    fprintf(stderr, "version: %s\n", v == HV_N64 ? "N64" : "PC");
    nmusic_version(v);
    show();
}

/* host_app.m, for every key event: 1 if it was the switch's (Tab, with no
 * Command held) and must not reach the game. A held Tab switches once. */
int hversion_key(NSEvent *e)
{
    if (e.keyCode != 0x30 || (e.modifierFlags & NSEventModifierFlagCommand)) return 0;
    HLOG("version: Tab %s (window %ld, t %.3f)\n", e.type == NSEventTypeKeyDown ? "down" : "up",
         (long)e.windowNumber, e.timestamp);
    if (e.type == NSEventTypeKeyDown && !e.isARepeat) hversion_toggle();
    return 1;
}

/* scripts: `version` -- Tab pressed and released, queued as real key events,
 * so it takes the keyboard's path */
void hversion_script(void)
{
    NSWindow *win = happ_window();
    NSEventType t[2] = { NSEventTypeKeyDown, NSEventTypeKeyUp };
    int i;
    if (!win) { hversion_toggle(); return; }
    for (i = 0; i < 2; i++)
        [NSApp postEvent:[NSEvent keyEventWithType:t[i] location:NSZeroPoint modifierFlags:0
                                         timestamp:NSProcessInfo.processInfo.systemUptime
                                      windowNumber:win.windowNumber context:nil characters:@"\t"
                             charactersIgnoringModifiers:@"\t" isARepeat:NO keyCode:0x30]
                  atStart:NO];
}

/* the first time the window comes up, which version this is */
__attribute__((constructor)) static void version_init(void)
{
    @autoreleasepool {
        [NSNotificationCenter.defaultCenter addObserverForName:NSWindowDidBecomeKeyNotification object:nil
                                                          queue:nil usingBlock:^(NSNotification *note) {
            static int shown;
            if (shown || note.object != happ_window()) return;
            shown = 1;
            fprintf(stderr, "version: %s\n", hversion() == HV_N64 ? "N64" : "PC");
            show();
        }];
    }
}
