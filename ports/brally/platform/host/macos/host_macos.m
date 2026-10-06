/* host_macos.m: the macOS host -- a Cocoa window that shows the frames, the
 * keyboard, mouse and game controllers, Core Audio out and decoded music.
 *
 * The game keeps its own loop on the main thread (it pumps messages through
 * PeekMessage / GetMessage), so this host never runs [NSApp run]: each
 * host_poll_event drains Cocoa's queue itself. Frames arrive either as ARGB
 * pixels through host_present (the software renderer), shown scaled to the
 * window with nearest-neighbour filtering, or drawn by the Metal renderer
 * into the window's CAMetalLayer (host_macos_metal_layer).
 *
 * Directories (environment, else the defaults):
 *   BR_CDROOT   the CD's files       (default reference/brally/data/disc, then the app's Resources/disc)
 *   BR_GAMEDIR  the install          (default: the CD root)
 *   BR_SAVEDIR  saves and settings   (default ~/Library/Application Support/Boss Rally 64)
 */
#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>
#import <Metal/Metal.h>
#import <AudioToolbox/AudioToolbox.h>
#import <GameController/GameController.h>
#include <sys/stat.h>

#include "host.h"

static char s_cd[1024], s_game[1024], s_save[1024], s_music[1024];
static const char *s_app_dir = "Boss Rally 64", *s_app_title = "Boss Rally";

void host_set_app_name(const char *dir, const char *title)
{
    if (dir)
        s_app_dir = dir;
    if (title)
        s_app_title = title;
}

/* ---- process -------------------------------------------------------------------- */
static int is_dir(const char *p)
{
    struct stat st;
    return stat(p, &st) == 0 && S_ISDIR(st.st_mode);
}

void host_init(int argc, char **argv)
{
    const char *e;
    (void)argc;
    (void)argv;
    @autoreleasepool {
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
        {
            NSMenu *bar = [NSMenu new], *app = [NSMenu new];
            NSMenuItem *item = [NSMenuItem new];
            [app addItemWithTitle:[NSString stringWithFormat:@"Quit %s", s_app_title] action:@selector(terminate:) keyEquivalent:@"q"];
            [item setSubmenu:app];
            [bar addItem:item];
            {                                         /* View: full screen (the window's own action) */
                NSMenu *view = [[NSMenu alloc] initWithTitle:@"View"];
                NSMenuItem *vi = [NSMenuItem new], *fs;
                fs = [view addItemWithTitle:@"Enter Full Screen" action:@selector(toggleFullScreen:) keyEquivalent:@"f"];
                [fs setKeyEquivalentModifierMask:NSEventModifierFlagControl | NSEventModifierFlagCommand];
                [vi setSubmenu:view];
                [bar addItem:vi];
            }
            [NSApp setMainMenu:bar];
        }
        [NSApp finishLaunching];

        e = getenv("BR_CDROOT");
        if (e) {
            snprintf(s_cd, sizeof s_cd, "%s", e);
        } else if (is_dir("reference/brally/data/disc")) {
            snprintf(s_cd, sizeof s_cd, "reference/brally/data/disc");
        } else {
            NSString *r = [[NSBundle mainBundle] resourcePath];
            snprintf(s_cd, sizeof s_cd, "%s/disc", r ? [r fileSystemRepresentation] : ".");
        }
        e = getenv("BR_GAMEDIR");
        snprintf(s_game, sizeof s_game, "%s", e ? e : s_cd);
        e = getenv("BR_SAVEDIR");
        if (e) {
            snprintf(s_save, sizeof s_save, "%s", e);
        } else {
            NSString *base = [NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory,
                                                                 NSUserDomainMask, YES) firstObject];
            snprintf(s_save, sizeof s_save, "%s/%s", base ? [base fileSystemRepresentation] : ".", s_app_dir);
        }
        host_mkdir(s_save);
        /* the CD's audio tracks: BR_MUSICDIR, the extracted ones in the tree,
         * or the app's Resources/music/cd */
        e = getenv("BR_MUSICDIR");
        if (e) {
            snprintf(s_music, sizeof s_music, "%s", e);
        } else if (is_dir("build/brally/wasm32/app/extract/music/cd")) {
            snprintf(s_music, sizeof s_music, "build/brally/wasm32/app/extract/music/cd");
        } else {
            NSString *r = [[NSBundle mainBundle] resourcePath];
            snprintf(s_music, sizeof s_music, "%s/music/cd", r ? [r fileSystemRepresentation] : ".");
        }
    }
}

const char *host_music_dir(void) { return is_dir(s_music) ? s_music : NULL; }

void host_shutdown(void)
{
    host_audio_close();
    host_window_close();
}

const char *host_game_dir(void) { return s_game; }
const char *host_cd_dir(void)   { return s_cd; }
const char *host_save_dir(void) { return s_save; }

/* ---- keys: mac virtual key -> DirectInput scan code and Win32 virtual key ---------- */
static const struct { uint16_t mac; uint8_t dik; uint8_t vk; } k_keys[] = {
    { 0x35, 0x01, 0x1B }, { 0x24, 0x1C, 0x0D }, { 0x4C, 0x9C, 0x0D }, { 0x31, 0x39, 0x20 },
    { 0x30, 0x0F, 0x09 }, { 0x33, 0x0E, 0x08 },
    { 0x7B, 0xCB, 0x25 }, { 0x7C, 0xCD, 0x27 }, { 0x7E, 0xC8, 0x26 }, { 0x7D, 0xD0, 0x28 },
    { 0x38, 0x2A, 0x10 }, { 0x3C, 0x36, 0x10 }, { 0x3B, 0x1D, 0x11 }, { 0x3E, 0x9D, 0x11 },
    { 0x3A, 0x38, 0x12 }, { 0x3D, 0xB8, 0x12 },
    { 0x00, 0x1E, 'A' }, { 0x0B, 0x30, 'B' }, { 0x08, 0x2E, 'C' }, { 0x02, 0x20, 'D' },
    { 0x0E, 0x12, 'E' }, { 0x03, 0x21, 'F' }, { 0x05, 0x22, 'G' }, { 0x04, 0x23, 'H' },
    { 0x22, 0x17, 'I' }, { 0x26, 0x24, 'J' }, { 0x28, 0x25, 'K' }, { 0x25, 0x26, 'L' },
    { 0x2E, 0x32, 'M' }, { 0x2D, 0x31, 'N' }, { 0x1F, 0x18, 'O' }, { 0x23, 0x19, 'P' },
    { 0x0C, 0x10, 'Q' }, { 0x0F, 0x13, 'R' }, { 0x01, 0x1F, 'S' }, { 0x11, 0x14, 'T' },
    { 0x20, 0x16, 'U' }, { 0x09, 0x2F, 'V' }, { 0x0D, 0x11, 'W' }, { 0x07, 0x2D, 'X' },
    { 0x10, 0x15, 'Y' }, { 0x06, 0x2C, 'Z' },
    { 0x12, 0x02, '1' }, { 0x13, 0x03, '2' }, { 0x14, 0x04, '3' }, { 0x15, 0x05, '4' },
    { 0x17, 0x06, '5' }, { 0x16, 0x07, '6' }, { 0x1A, 0x08, '7' }, { 0x1C, 0x09, '8' },
    { 0x19, 0x0A, '9' }, { 0x1D, 0x0B, '0' },
    { 0x7A, 0x3B, 0x70 }, { 0x78, 0x3C, 0x71 }, { 0x63, 0x3D, 0x72 }, { 0x76, 0x3E, 0x73 },
    { 0x60, 0x3F, 0x74 }, { 0x61, 0x40, 0x75 }, { 0x62, 0x41, 0x76 }, { 0x64, 0x42, 0x77 },
    { 0x65, 0x43, 0x78 }, { 0x6D, 0x44, 0x79 }, { 0x67, 0x57, 0x7A }, { 0x6F, 0x58, 0x7B },
    { 0x32, 0x29, 0xC0 }, { 0x1B, 0x0C, 0xBD }, { 0x18, 0x0D, 0xBB }, { 0x21, 0x1A, 0xDB },
    { 0x1E, 0x1B, 0xDD }, { 0x2A, 0x2B, 0xDC }, { 0x29, 0x27, 0xBA }, { 0x27, 0x28, 0xDE },
    { 0x2B, 0x33, 0xBC }, { 0x2F, 0x34, 0xBE }, { 0x2C, 0x35, 0xBF },
    { 0x73, 0xC7, 0x24 }, { 0x74, 0xC9, 0x21 }, { 0x77, 0xCF, 0x23 }, { 0x79, 0xD1, 0x22 },
    { 0x72, 0xD2, 0x2D }, { 0x75, 0xD3, 0x2E },
    { 0x52, 0x52, 0x60 }, { 0x53, 0x4F, 0x61 }, { 0x54, 0x50, 0x62 }, { 0x55, 0x51, 0x63 },
    { 0x56, 0x4B, 0x64 }, { 0x57, 0x4C, 0x65 }, { 0x58, 0x4D, 0x66 }, { 0x59, 0x47, 0x67 },
    { 0x5B, 0x48, 0x68 }, { 0x5C, 0x49, 0x69 }, { 0x41, 0x53, 0x6E }, { 0x43, 0x37, 0x6A },
    { 0x45, 0x4E, 0x6B }, { 0x4E, 0x4A, 0x6D }, { 0x4B, 0xB5, 0x6F },
};

/* ---- the event queue the host fills and host_poll_event drains ------------------- */
#define QMAX 256
static host_event s_q[QMAX];
static int s_qh, s_qt;
static uint8_t s_down[128];          /* by mac key code: held, for modifier changes */

static void qpush(const host_event *e)
{
    int n = (s_qt + 1) % QMAX;
    if (n != s_qh) {
        s_q[s_qt] = *e;
        s_qt = n;
    }
}

static void key_event(uint16_t mac, int down)
{
    size_t i;
    if (mac < 128) {
        if (s_down[mac] == down)
            return;               /* auto-repeat and modifier duplicates */
        s_down[mac] = (uint8_t)down;
    }
    for (i = 0; i < sizeof k_keys / sizeof k_keys[0]; i++)
        if (k_keys[i].mac == mac) {
            host_event e;
            memset(&e, 0, sizeof e);
            e.type = HOST_EV_KEY;
            e.vk = k_keys[i].vk;
            e.scan = k_keys[i].dik;
            e.down = down;
            qpush(&e);
            return;
        }
}

/* ---- the window ----------------------------------------------------------------- */
@interface BrView : NSView
@end

@interface BrWindowDelegate : NSObject <NSWindowDelegate>
@end

static NSWindow *s_win;
static BrView *s_view;
static int s_w = 640, s_h = 480;
static CGColorSpaceRef s_rgb;

@implementation BrView
- (BOOL)acceptsFirstResponder { return YES; }
/* the first click on the window reaches the game too, not only activation */
- (BOOL)acceptsFirstMouse:(NSEvent *)ev { (void)ev; return YES; }
/* pointer movement over the view whether or not the window is key, so the
 * game's cursor is already under the pointer when it comes back in; the
 * pointer is never captured or confined */
- (void)updateTrackingAreas
{
    NSArray *old = [[self trackingAreas] copy];
    for (NSTrackingArea *t in old)
        [self removeTrackingArea:t];
    [self addTrackingArea:[[NSTrackingArea alloc] initWithRect:NSZeroRect
                                                       options:NSTrackingMouseMoved | NSTrackingActiveAlways |
                                                               NSTrackingInVisibleRect
                                                         owner:self userInfo:nil]];
    [super updateTrackingAreas];
}
/* the game draws its own cursor: the system's is hidden over the view */
- (void)resetCursorRects
{
    static NSCursor *blank;
    if (!blank) {
        NSImage *img = [[NSImage alloc] initWithSize:NSMakeSize(1, 1)];
        blank = [[NSCursor alloc] initWithImage:img hotSpot:NSZeroPoint];
    }
    [self addCursorRect:[self bounds] cursor:blank];
}
- (BOOL)wantsUpdateLayer { return YES; }
- (void)keyDown:(NSEvent *)ev
{
    NSString *c;
    key_event([ev keyCode], 1);
    c = [ev characters];
    if (![ev isARepeat] && [c length] == 1 && !([ev modifierFlags] & NSEventModifierFlagCommand)) {
        unichar ch = [c characterAtIndex:0];
        if (ch < 0x80 && ch != 0x7F && (ch >= 0x20 || ch == 0x0D || ch == 0x08 || ch == 0x1B)) {
            host_event e;
            memset(&e, 0, sizeof e);
            e.type = HOST_EV_CHAR;
            e.ch = ch;
            qpush(&e);
        }
    }
}
- (void)keyUp:(NSEvent *)ev { key_event([ev keyCode], 0); }
- (void)flagsChanged:(NSEvent *)ev
{
    /* a modifier's own key code: down when its flag is now set */
    NSEventModifierFlags f = [ev modifierFlags];
    uint16_t k = [ev keyCode];
    int down = 0;
    switch (k) {
    case 0x38: case 0x3C: down = (f & NSEventModifierFlagShift) != 0; break;
    case 0x3B: case 0x3E: down = (f & NSEventModifierFlagControl) != 0; break;
    case 0x3A: case 0x3D: down = (f & NSEventModifierFlagOption) != 0; break;
    default: return;
    }
    key_event(k, down);
}
static void mouse_event(NSView *v, NSEvent *ev, int buttons)
{
    NSPoint p = [v convertPoint:[ev locationInWindow] fromView:nil];
    NSRect b = [v bounds];
    host_event e;
    memset(&e, 0, sizeof e);
    e.type = HOST_EV_MOUSE;
    e.x = (int)(p.x * s_w / b.size.width);
    e.y = (int)((b.size.height - p.y) * s_h / b.size.height);
    e.buttons = buttons;
    qpush(&e);
}
- (void)mouseMoved:(NSEvent *)ev { mouse_event(self, ev, (int)[NSEvent pressedMouseButtons] & 1); }
- (void)mouseDragged:(NSEvent *)ev { mouse_event(self, ev, 1); }
- (void)mouseDown:(NSEvent *)ev { mouse_event(self, ev, 1); }
- (void)mouseUp:(NSEvent *)ev { mouse_event(self, ev, 0); }
@end

@implementation BrWindowDelegate
- (BOOL)windowShouldClose:(NSWindow *)w
{
    host_event e;
    (void)w;
    memset(&e, 0, sizeof e);
    e.type = HOST_EV_CLOSE;
    qpush(&e);
    return NO;                /* the game decides, through WM_CLOSE */
}
- (void)windowDidBecomeKey:(NSNotification *)n
{
    host_event e;
    (void)n;
    memset(&e, 0, sizeof e);
    e.type = HOST_EV_FOCUS;
    e.down = 1;
    qpush(&e);
}
- (void)windowDidResignKey:(NSNotification *)n
{
    host_event e;
    size_t i;
    (void)n;
    /* keys held when focus leaves go up, or they stay down for good */
    for (i = 0; i < 128; i++)
        if (s_down[i])
            key_event((uint16_t)i, 0);
    memset(&e, 0, sizeof e);
    e.type = HOST_EV_FOCUS;
    e.down = 0;
    qpush(&e);
}
@end

static BrWindowDelegate *s_delegate;

int host_window_open(int width, int height, const char *title)
{
    @autoreleasepool {
        NSRect r;
        if (s_win)
            return 1;
        s_w = width;
        s_h = height;
        r = NSMakeRect(0, 0, width * 2, height * 2);
        s_win = [[NSWindow alloc] initWithContentRect:r
                                            styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                                                      NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable
                                              backing:NSBackingStoreBuffered
                                                defer:NO];
        [s_win setTitle:[NSString stringWithUTF8String:title ? title : s_app_title]];
        [s_win setContentAspectRatio:NSMakeSize(width, height)];
        s_view = [[BrView alloc] initWithFrame:r];
        [s_view setWantsLayer:YES];
        [[s_view layer] setBackgroundColor:CGColorGetConstantColor(kCGColorBlack)];
        [[s_view layer] setMagnificationFilter:kCAFilterNearest];
        [[s_view layer] setContentsGravity:kCAGravityResize];
        [s_win setContentView:s_view];
        [s_win setAcceptsMouseMovedEvents:YES];
        s_delegate = [BrWindowDelegate new];
        [s_win setDelegate:s_delegate];
        [s_win center];
        [s_win makeKeyAndOrderFront:nil];
        [s_win makeFirstResponder:s_view];
        [NSApp activateIgnoringOtherApps:YES];
        s_rgb = CGColorSpaceCreateDeviceRGB();
    }
    return 1;
}

void host_window_close(void)
{
    if (s_win) {
        [s_win orderOut:nil];
        s_win = nil;
        s_view = nil;
    }
}

void host_window_lock_aspect(int lock)
{
    if (!s_win)
        return;
    if (lock)
        [s_win setContentAspectRatio:NSMakeSize(s_w, s_h)];
    else
        [s_win setContentResizeIncrements:NSMakeSize(1, 1)];   /* clears the ratio */
}

int host_window_visible(void)
{
    return s_win && ([s_win occlusionState] & NSWindowOcclusionStateVisible) != 0;
}

/* the window's CAMetalLayer, for a renderer that draws with Metal (macOS
 * only: render/metal asks for it; the view's layer becomes a Metal layer) */
CAMetalLayer *host_macos_metal_layer(void)
{
    static CAMetalLayer *ml;
    if (!s_view)
        return nil;
    if (!ml) {
        ml = [CAMetalLayer layer];
        [ml setPixelFormat:MTLPixelFormatBGRA8Unorm];
        [ml setFramebufferOnly:YES];
        [ml setMagnificationFilter:kCAFilterNearest];
        [s_view setLayer:ml];
        [s_view setWantsLayer:YES];
        [ml setContentsScale:[s_win backingScaleFactor]];
        [ml setDrawableSize:CGSizeMake([s_view bounds].size.width * [s_win backingScaleFactor],
                                       [s_view bounds].size.height * [s_win backingScaleFactor])];
    }
    return ml;
}

/* the Metal layer's drawable kept at the view's size in pixels, so a
 * renderer draws one pixel per window pixel; *w x *h is that size */
void host_macos_layer_fit(int *w, int *h)
{
    CAMetalLayer *ml = host_macos_metal_layer();
    CGFloat sc;
    CGSize b, want;
    *w = *h = 0;
    if (!ml)
        return;
    sc = [s_win backingScaleFactor];
    b = [s_view bounds].size;
    want = CGSizeMake(floor(b.width * sc + 0.5), floor(b.height * sc + 0.5));
    if ([ml contentsScale] != sc)
        [ml setContentsScale:sc];
    if (!CGSizeEqualToSize([ml drawableSize], want) && want.width >= 1 && want.height >= 1)
        [ml setDrawableSize:want];
    *w = (int)want.width;
    *h = (int)want.height;
}

/* a frame of ARGB pixels (0xAARRGGBB, top row first) onto the window */
void host_present(const uint32_t *argb, int w, int h)
{
    @autoreleasepool {
        CGDataProviderRef dp;
        CGImageRef img;
        NSData *d;
        if (!s_view || !argb)
            return;
        d = [NSData dataWithBytes:argb length:(NSUInteger)w * (NSUInteger)h * 4];
        dp = CGDataProviderCreateWithCFData((__bridge CFDataRef)d);
        img = CGImageCreate((size_t)w, (size_t)h, 8, 32, (size_t)w * 4, s_rgb,
                            kCGBitmapByteOrder32Little | kCGImageAlphaNoneSkipFirst,
                            dp, NULL, false, kCGRenderingIntentDefault);
        [CATransaction begin];
        [CATransaction setDisableActions:YES];
        [[s_view layer] setContents:(__bridge id)img];
        [CATransaction commit];
        CGImageRelease(img);
        CGDataProviderRelease(dp);
    }
}

int host_poll_event(host_event *ev, uint32_t wait_ms)
{
    @autoreleasepool {
        NSDate *until = wait_ms == 0 ? [NSDate distantPast]
                      : wait_ms == 0xFFFFFFFFu ? [NSDate distantFuture]
                      : [NSDate dateWithTimeIntervalSinceNow:wait_ms / 1000.0];
        while (s_qh == s_qt) {
            NSEvent *e = [NSApp nextEventMatchingMask:NSEventMaskAny untilDate:until
                                               inMode:NSDefaultRunLoopMode dequeue:YES];
            if (!e)
                break;
            [NSApp sendEvent:e];
            until = [NSDate distantPast];      /* the rest of what is queued, no more waiting */
        }
    }
    if (s_qh == s_qt)
        return 0;
    *ev = s_q[s_qh];
    s_qh = (s_qh + 1) % QMAX;
    return 1;
}

void host_message_box(const char *text, const char *caption)
{
    @autoreleasepool {
        NSAlert *a = [NSAlert new];
        [a setMessageText:[NSString stringWithUTF8String:caption ? caption : s_app_title]];
        [a setInformativeText:[NSString stringWithUTF8String:text ? text : ""]];
        [a runModal];
    }
}

/* ---- audio out: an output unit pulling the platform's mixer --------------------- */
static AudioComponentInstance s_unit;
static host_audio_fn s_afn;
static void *s_auser;

static OSStatus render(void *ref, AudioUnitRenderActionFlags *flags, const AudioTimeStamp *ts,
                       UInt32 bus, UInt32 frames, AudioBufferList *io)
{
    (void)ref; (void)flags; (void)ts; (void)bus;
    if (s_afn)
        s_afn((float *)io->mBuffers[0].mData, (int)frames, s_auser);
    else
        memset(io->mBuffers[0].mData, 0, io->mBuffers[0].mDataByteSize);
    return noErr;
}

int host_audio_open(int rate, host_audio_fn fn, void *user)
{
    AudioComponentDescription desc = { kAudioUnitType_Output, kAudioUnitSubType_DefaultOutput,
                                       kAudioUnitManufacturer_Apple, 0, 0 };
    AudioStreamBasicDescription fmt;
    AURenderCallbackStruct cb = { render, NULL };
    AudioComponent comp;
    if (s_unit)
        return 1;
    comp = AudioComponentFindNext(NULL, &desc);
    if (!comp || AudioComponentInstanceNew(comp, &s_unit) != noErr)
        return 0;
    memset(&fmt, 0, sizeof fmt);
    fmt.mSampleRate = rate;
    fmt.mFormatID = kAudioFormatLinearPCM;
    fmt.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
    fmt.mFramesPerPacket = 1;
    fmt.mChannelsPerFrame = 2;
    fmt.mBitsPerChannel = 32;
    fmt.mBytesPerFrame = 8;
    fmt.mBytesPerPacket = 8;
    s_afn = fn;
    s_auser = user;
    if (AudioUnitSetProperty(s_unit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0,
                             &fmt, sizeof fmt) != noErr ||
        AudioUnitSetProperty(s_unit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0,
                             &cb, sizeof cb) != noErr ||
        AudioUnitInitialize(s_unit) != noErr || AudioOutputUnitStart(s_unit) != noErr) {
        AudioComponentInstanceDispose(s_unit);
        s_unit = NULL;
        return 0;
    }
    return 1;
}

void host_audio_close(void)
{
    if (s_unit) {
        AudioOutputUnitStop(s_unit);
        AudioUnitUninitialize(s_unit);
        AudioComponentInstanceDispose(s_unit);
        s_unit = NULL;
    }
}

/* ---- decoded audio files: Core Audio reads FLAC, WAV, AIFF, MP3, AAC ----------- */
struct host_stream {
    ExtAudioFileRef f;
};

host_stream *host_stream_open(const char *path, int rate)
{
    @autoreleasepool {
        CFURLRef url = CFURLCreateFromFileSystemRepresentation(NULL, (const UInt8 *)path,
                                                               (CFIndex)strlen(path), false);
        ExtAudioFileRef f = NULL;
        AudioStreamBasicDescription fmt;
        host_stream *s;
        OSStatus st;
        if (!url)
            return NULL;
        st = ExtAudioFileOpenURL(url, &f);
        CFRelease(url);
        if (st != noErr || !f)
            return NULL;
        memset(&fmt, 0, sizeof fmt);
        fmt.mSampleRate = rate;
        fmt.mFormatID = kAudioFormatLinearPCM;
        fmt.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
        fmt.mFramesPerPacket = 1;
        fmt.mChannelsPerFrame = 2;
        fmt.mBitsPerChannel = 32;
        fmt.mBytesPerFrame = 8;
        fmt.mBytesPerPacket = 8;
        if (ExtAudioFileSetProperty(f, kExtAudioFileProperty_ClientDataFormat, sizeof fmt, &fmt) != noErr) {
            ExtAudioFileDispose(f);
            return NULL;
        }
        s = (host_stream *)calloc(1, sizeof *s);
        s->f = f;
        return s;
    }
}

int host_stream_read(host_stream *s, float *lr, int frames)
{
    AudioBufferList bl;
    UInt32 n = (UInt32)frames;
    if (!s)
        return 0;
    bl.mNumberBuffers = 1;
    bl.mBuffers[0].mNumberChannels = 2;
    bl.mBuffers[0].mDataByteSize = (UInt32)frames * 8;
    bl.mBuffers[0].mData = lr;
    if (ExtAudioFileRead(s->f, &n, &bl) != noErr)
        return 0;
    return (int)n;
}

void host_stream_close(host_stream *s)
{
    if (s) {
        ExtAudioFileDispose(s->f);
        free(s);
    }
}

/* ---- game controllers ------------------------------------------------------------- */
/* any controller macOS knows (Xbox, PlayStation, MFi, Switch Pro) through
 * GameController.framework; the current one, else the first with a full
 * gamepad profile. One plugged in while the game runs is picked up at the
 * next read. */
int host_pad_read(host_pad *o)
{
    @autoreleasepool {
        GCController *c = GCController.current;
        GCExtendedGamepad *g;
        unsigned b = 0;
        int u, r, d, l;
        memset(o, 0, sizeof *o);
        o->pov = -1;
        if (!c || !c.extendedGamepad)
            for (GCController *k in GCController.controllers)
                if (k.extendedGamepad) {
                    c = k;
                    break;
                }
        if (!c || !(g = c.extendedGamepad))
            return 0;
        o->x = g.leftThumbstick.xAxis.value;
        o->y = -g.leftThumbstick.yAxis.value;
        o->z = g.rightTrigger.value - g.leftTrigger.value;
        o->rx = g.rightThumbstick.xAxis.value;
        o->ry = -g.rightThumbstick.yAxis.value;
        if (g.buttonA.pressed) b |= 1u << 0;
        if (g.buttonB.pressed) b |= 1u << 1;
        if (g.buttonX.pressed) b |= 1u << 2;
        if (g.buttonY.pressed) b |= 1u << 3;
        if (g.leftShoulder.pressed) b |= 1u << 4;
        if (g.rightShoulder.pressed) b |= 1u << 5;
        if (g.buttonOptions.pressed) b |= 1u << 6;
        if (g.buttonMenu.pressed) b |= 1u << 7;
        if (g.leftThumbstickButton.pressed) b |= 1u << 8;
        if (g.rightThumbstickButton.pressed) b |= 1u << 9;
        if (g.leftTrigger.value > 0.5f) b |= 1u << 10;
        if (g.rightTrigger.value > 0.5f) b |= 1u << 11;
        u = g.dpad.up.pressed;
        r = g.dpad.right.pressed;
        d = g.dpad.down.pressed;
        l = g.dpad.left.pressed;
        b |= (unsigned)u << 12 | (unsigned)r << 13 | (unsigned)d << 14 | (unsigned)l << 15;
        o->buttons = b;
        if (u || r || d || l) {
            static const int ang[3][3] = { { 22500, 27000, 31500 }, { 18000, -1, 0 }, { 13500, 9000, 4500 } };
            o->pov = ang[r - l + 1][u - d + 1];
        }
        return 1;
    }
}
