/* host_app.m -- the macOS side: window, keyboard/mouse, presenting frames,
 * and main() (port code).
 *
 * The game runs on the main thread, as it did on Windows, and pumps Cocoa
 * from its own message loop (PeekMessage/GetMessage call happ_pump). Frames
 * the Metal Glide layer (host_glide.m) finishes are presented in the
 * window's CAMetalLayer. Headless (BR_HEADLESS=1) there is no window; Metal
 * still renders, and frames can be dumped as PPM with BR_SHOT_DIR, one per
 * N swaps (BR_SHOT_EVERY).
 */
#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>
#include "host.h"
#include <stdlib.h>
#include <string.h>

volatile int g_happ_quit;
static NSWindow *g_window;
static NSView *g_view;
static int g_headless = -1;
static u8 g_dik[256];
static u8 g_vk[256];

void hdx_mouse(int dx, int dy, int btn);

/* macOS virtual key code -> (DirectInput scan code, Windows VK) */
static const struct { u16 mac; u8 dik; u8 vk; } KEYMAP[] = {
    { 0x35, 0x01, 0x1B }, /* esc */      { 0x24, 0x1C, 0x0D }, /* return */
    { 0x4C, 0x9C, 0x0D }, /* enter */    { 0x31, 0x39, 0x20 }, /* space */
    { 0x30, 0x0F, 0x09 }, /* tab */      { 0x33, 0x0E, 0x08 }, /* backspace */
    { 0x7B, 0xCB, 0x25 }, /* left */     { 0x7C, 0xCD, 0x27 }, /* right */
    { 0x7E, 0xC8, 0x26 }, /* up */       { 0x7D, 0xD0, 0x28 }, /* down */
    { 0x38, 0x2A, 0x10 }, /* lshift */   { 0x3C, 0x36, 0x10 }, /* rshift */
    { 0x3B, 0x1D, 0x11 }, /* lctrl */    { 0x3E, 0x9D, 0x11 }, /* rctrl */
    { 0x3A, 0x38, 0x12 }, /* lalt */     { 0x3D, 0xB8, 0x12 }, /* ralt */
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
    { 0x65, 0x43, 0x78 }, { 0x6D, 0x44, 0x79 },
};

static void set_key(u16 mac, int down)
{
    size_t i;
    for (i = 0; i < sizeof KEYMAP / sizeof KEYMAP[0]; i++)
        if (KEYMAP[i].mac == mac) {
            int was = g_dik[KEYMAP[i].dik] != 0;
            g_dik[KEYMAP[i].dik] = down ? 0x80 : 0;
            g_vk[KEYMAP[i].vk] = (u8)down;
            if (hwin_main_hwnd() && down != was) {
                u32 lp = ((u32)KEYMAP[i].dik << 16) | 1 | (down ? 0 : 0xC0000000u);
                hwin_post(hwin_main_hwnd(), down ? 0x100 : 0x101, KEYMAP[i].vk, lp);
                if (down && ((KEYMAP[i].vk >= 'A' && KEYMAP[i].vk <= 'Z') ||
                             (KEYMAP[i].vk >= '0' && KEYMAP[i].vk <= '9') ||
                             KEYMAP[i].vk == 0x20 || KEYMAP[i].vk == 0x0D ||
                             KEYMAP[i].vk == 0x08 || KEYMAP[i].vk == 0x1B))
                    hwin_post(hwin_main_hwnd(), 0x102, KEYMAP[i].vk >= 'A' && KEYMAP[i].vk <= 'Z'
                              ? KEYMAP[i].vk + 32 : KEYMAP[i].vk, lp);
            }
        }
}

int happ_key_down(int vk) { return vk >= 0 && vk < 256 && g_vk[vk]; }
void happ_dik_state(u8 *out) { memcpy(out, g_dik, 256); }

@interface BRWinDelegate : NSObject <NSWindowDelegate>
@end
@implementation BRWinDelegate
- (BOOL)windowShouldClose:(id)sender { (void)sender; g_happ_quit = 1; return NO; }
@end

static int headless(void)
{
    if (g_headless < 0) g_headless = getenv("BR_HEADLESS") != NULL;
    return g_headless;
}

void happ_init(void)
{
    static BRWinDelegate *del;
    if (g_window || headless()) return;
    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    [NSApp finishLaunching];
    g_window = [[NSWindow alloc] initWithContentRect:NSMakeRect(100, 100, 1280, 960)
                                           styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                                                     NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable
                                             backing:NSBackingStoreBuffered defer:NO];
    [g_window setTitle:@"Boss Rally"];
    [g_window setContentAspectRatio:NSMakeSize(4, 3)];
    del = [BRWinDelegate new];
    [g_window setDelegate:del];
    g_view = [g_window contentView];
    [g_view setWantsLayer:YES];
    {
        CAMetalLayer *ml = [CAMetalLayer layer];
        ml.contentsScale = g_window.backingScaleFactor;
        g_view.layer = ml;
    }
    [g_window makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];
}

void happ_pump(int block_ms)
{
    NSDate *until;
    NSEvent *e;
    if (headless() || !g_window) {
        if (block_ms) usleep((useconds_t)block_ms * 1000);
        return;
    }
    until = block_ms ? [NSDate dateWithTimeIntervalSinceNow:block_ms / 1000.0] : [NSDate distantPast];
    @autoreleasepool {
        while ((e = [NSApp nextEventMatchingMask:NSEventMaskAny untilDate:until
                                          inMode:NSDefaultRunLoopMode dequeue:YES])) {
            switch (e.type) {
            case NSEventTypeKeyDown:
                if (!e.isARepeat) set_key(e.keyCode, 1);
                if ((e.modifierFlags & NSEventModifierFlagCommand) && e.keyCode == 0x0C) g_happ_quit = 1;
                continue;                              /* no beep */
            case NSEventTypeKeyUp:
                set_key(e.keyCode, 0);
                continue;
            case NSEventTypeFlagsChanged: {
                NSEventModifierFlags f = e.modifierFlags;
                u16 k = e.keyCode;
                int down = (k == 0x38 || k == 0x3C) ? !!(f & NSEventModifierFlagShift)
                         : (k == 0x3B || k == 0x3E) ? !!(f & NSEventModifierFlagControl)
                         : (k == 0x3A || k == 0x3D) ? !!(f & NSEventModifierFlagOption) : 0;
                set_key(k, down);
                continue;
            }
            case NSEventTypeMouseMoved: case NSEventTypeLeftMouseDragged:
                hdx_mouse((int)e.deltaX, (int)e.deltaY, (int)[NSEvent pressedMouseButtons]);
                break;
            default:
                break;
            }
            [NSApp sendEvent:e];
            until = [NSDate distantPast];
        }
    }
}

CAMetalLayer *happ_metal_layer(void)
{
    return (g_view && !headless()) ? (CAMetalLayer *)g_view.layer : nil;
}

/* ------------------------------------------------------------------ main */
void w_init(const char *dll, const char *portdata);

int main(int argc, char **argv)
{
    const char *root = getenv("BR_ROOT") ? getenv("BR_ROOT") : ".";
    char dll[1024], pd[1024], disc[1024], save[1024];
    u32 cmd;
    const w_fentry *rm;
    u32 r;
    (void)argc; (void)argv;
    g_hlog = getenv("BR_LOG") != NULL;
    snprintf(dll, sizeof dll, "%s/orig/BRGlide.dll", root);
    snprintf(pd, sizeof pd, "%s/build/wasm/c/portdata.bin", root);
    snprintf(disc, sizeof disc, "%s", getenv("BR_CDROOT") ? getenv("BR_CDROOT") : "testdata/disc");
    snprintf(save, sizeof save, "%s/Library/Application Support/Boss Rally", getenv("HOME"));
    w_init(dll, pd);
    vfs_init(disc, save);
    w_run_inits();
    /* What BRGlide.dll's CRT entry (0x10074A30, static MSVC CRT, not in the
     * decomp) does on DLL_PROCESS_ATTACH before anything else runs: the
     * onexit table (0x118EF17C/80), _initterm over the C++ constructor
     * table the original's data holds at 0x1007B000..0x1007B048, the attach
     * count, then DllMain (BrDllMain, 0x10074B00). */
    {
        u32 t = hmem_alloc(0x80, 1), a;
        W_ST(u32, 0x118EF180u, 0, t);
        W_ST(u32, 0x118EF17Cu, 0, t);
        for (a = 0x1007B000u; a < 0x1007B048u; a += 4) {
            u32 fn = W_LD(u32, a, 0);
            if (fn) w_icall__(fn);
        }
        W_ST(u32, 0x118EEF28u, 0, W_LD(u32, 0x118EEF28u, 0) + 1);
        w_icall_iii_i(0x10074B00u, 0x10000000u, 1, 0);
    }
    cmd = hstrdup(argc > 1 ? argv[1] : "");
    /* BRally.exe: gRallyMain(hInstance, hPrev, lpCmdLine, nShowCmd) */
    rm = w_lookup(0x1001CC00u);
    fprintf(stderr, "Boss Rally: RallyMain = %s\n", rm->name);
    r = w_icall_iiii_i(0x1001CC00u, 0x00400000u, 0, cmd, 1);
    fprintf(stderr, "RallyMain returned %u\n", r);
    return (int)r;
}
