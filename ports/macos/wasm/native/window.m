/* window.m -- the game window's size, live: resizing and full screen.
 *
 * The renderer follows the drawable by itself (host_glide.m re-sizes its
 * target at every swap from the layer's drawableSize), so a new size needs
 * no restart -- but a layer set on a view does not re-size its drawable when
 * the view changes size.  This module keeps the drawable at the view's pixel
 * size through window resizes, full screen and moves between displays of
 * different scale, and gives the window the Mac's full-screen controls: the
 * green button and View > Enter Full Screen, Ctrl-Cmd-F (host_app.m hands
 * Command chords to the menu bar).
 */
#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>

NSWindow *happ_window(void);
CAMetalLayer *happ_metal_layer(void);

/* The drawable matches the view: its bounds at the window's backing scale. */
static void fit(void)
{
    NSWindow *win = happ_window();
    CAMetalLayer *l = happ_metal_layer();
    NSView *v;
    CGSize px;
    double k;
    if (!win || !l) return;
    v = win.contentView;
    k = win.backingScaleFactor;
    px = CGSizeMake(floor(v.bounds.size.width * k), floor(v.bounds.size.height * k));
    if (px.width < 1 || px.height < 1) return;
    if (l.contentsScale != k) l.contentsScale = k;
    if (!CGSizeEqualToSize(l.drawableSize, px)) l.drawableSize = px;
}

/* View > Enter / Exit Full Screen, aimed at the game window whether or not
 * it is key */
@interface BRFullScreen : NSObject
@end
@implementation BRFullScreen
- (void)toggle:(id)sender { (void)sender; [happ_window() toggleFullScreen:nil]; }
- (BOOL)validateMenuItem:(NSMenuItem *)item
{
    NSWindow *win = happ_window();
    item.title = (win.styleMask & NSWindowStyleMaskFullScreen) ? @"Exit Full Screen" : @"Enter Full Screen";
    return win != nil;
}
@end

static void menu(void)
{
    static int done;
    static BRFullScreen *fs;
    NSMenu *bar = NSApp.mainMenu, *view;
    NSMenuItem *it;
    if (done) return;
    done = 1;
    if (!bar) {                          /* no menu bar yet: the app menu slot first */
        bar = [NSMenu new];
        [bar addItemWithTitle:@"" action:nil keyEquivalent:@""].submenu = [NSMenu new];
        NSApp.mainMenu = bar;
    }
    fs = [BRFullScreen new];
    view = [[NSMenu alloc] initWithTitle:@"View"];
    it = [view addItemWithTitle:@"Enter Full Screen" action:@selector(toggle:) keyEquivalent:@"f"];
    it.target = fs;
    it.keyEquivalentModifierMask = NSEventModifierFlagControl | NSEventModifierFlagCommand;
    [bar addItemWithTitle:@"View" action:nil keyEquivalent:@""].submenu = view;
}

__attribute__((constructor)) static void window_init(void)
{
    @autoreleasepool {
        NSNotificationCenter *nc = NSNotificationCenter.defaultCenter;
        for (NSNotificationName n in @[ NSWindowDidResizeNotification,
                                         NSWindowDidChangeBackingPropertiesNotification,
                                         NSWindowDidChangeScreenNotification,
                                         NSWindowDidEnterFullScreenNotification,
                                         NSWindowDidExitFullScreenNotification ])
            [nc addObserverForName:n object:nil queue:nil usingBlock:^(NSNotification *note) {
                if (note.object == happ_window()) fit();
            }];
        [nc addObserverForName:NSWindowDidBecomeKeyNotification object:nil queue:nil
                    usingBlock:^(NSNotification *note) {
            NSWindow *win = happ_window();
            if (note.object != win) return;
            win.collectionBehavior |= NSWindowCollectionBehaviorFullScreenPrimary;
            menu();
            fit();
        }];
    }
}

/* scripts: `window W H` */
void hwindow_resize(int w, int h)
{
    NSWindow *win = happ_window();
    if (!win || w < 160 || h < 120) return;
    if (win.styleMask & NSWindowStyleMaskFullScreen) return;
    [win setContentSize:NSMakeSize(w, h)];
}

/* scripts: `keycode` -- a real key down or up with that macOS virtual key
 * code, so it takes the keyboard's path (happ_pump's key table) */
void hwindow_keycode(int code, int down)
{
    NSWindow *win = happ_window();
    if (!win) return;
    [NSApp postEvent:[NSEvent keyEventWithType:down ? NSEventTypeKeyDown : NSEventTypeKeyUp
                                      location:NSZeroPoint modifierFlags:0
                                     timestamp:NSProcessInfo.processInfo.systemUptime
                                  windowNumber:win.windowNumber context:nil characters:@""
                   charactersIgnoringModifiers:@"" isARepeat:NO keyCode:(unsigned short)code]
             atStart:NO];
}

/* scripts: `fullscreen` */
void hwindow_fullscreen(void)
{
    NSWindow *win = happ_window();
    if (win) [win toggleFullScreen:nil];
}

/* scripts: `chord ctrl+cmd+f` -- a key press with modifiers, queued as a real
 * event, so it takes the path a keystroke takes (menu shortcuts included) */
void hwindow_chord(const char *spec)
{
    NSEventModifierFlags m = 0;
    NSString *keys = @"";
    NSEvent *e;
    NSWindow *win = happ_window();
    for (NSString *part in [[NSString stringWithUTF8String:spec] componentsSeparatedByString:@"+"]) {
        if ([part isEqualToString:@"ctrl"]) m |= NSEventModifierFlagControl;
        else if ([part isEqualToString:@"cmd"]) m |= NSEventModifierFlagCommand;
        else if ([part isEqualToString:@"alt"]) m |= NSEventModifierFlagOption;
        else if ([part isEqualToString:@"shift"]) m |= NSEventModifierFlagShift;
        else keys = part;
    }
    if (!win || !keys.length) return;
    e = [NSEvent keyEventWithType:NSEventTypeKeyDown location:NSZeroPoint modifierFlags:m
                        timestamp:NSProcessInfo.processInfo.systemUptime windowNumber:win.windowNumber
                          context:nil characters:keys charactersIgnoringModifiers:keys isARepeat:NO keyCode:0];
    [NSApp postEvent:e atStart:NO];
}

