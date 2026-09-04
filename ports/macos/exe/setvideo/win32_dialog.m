/* win32_dialog.m — the eleven Win32 calls SetVideo.exe makes, on AppKit.
 *
 * This is what lets the ORIGINAL dialog procedures run. DlgProcRadio,
 * DlgProcComboA, DlgProcComboB, DlgProc and DlgProcOKCancel are compiled
 * byte-for-byte from the decompiled sources and are not touched: they still
 * receive WM_INITDIALOG and WM_COMMAND, still stash their Sel pointer with
 * SetWindowLongA(hWnd, 8, …), still drive the combo with CB_ADDSTRING /
 * CB_SETITEMDATA / CB_SETCURSEL, and still finish with EndDialog. Only the
 * other side of those calls is new.
 *
 * The dialog layouts are the real ones, read out of the retail binary's
 * .rsrc by tools/rsrc_dump.py — same captions, same control ids, same
 * positions in dialog units.
 *
 * NOT byte-matched.
 */
#import <Cocoa/Cocoa.h>

#include "setvideo_port.h"
#include "br_dialogres.h"
#include <windows.h>

/* MS Sans Serif 8pt dialog base units are 6x13 pixels, and a dialog unit is
 * a quarter of that horizontally and an eighth vertically. Every coordinate
 * in the templates is in those units. */
static const CGFloat kDluX = 6.0 / 4.0;
static const CGFloat kDluY = 13.0 / 8.0;

@interface BrDialog : NSObject
@property (nonatomic, strong) NSWindow                 *window;
@property (nonatomic, strong) NSMutableDictionary      *controls;  /* id -> view */
@property (nonatomic, strong) NSMutableArray           *comboData; /* item data */
@property (nonatomic, assign) LPARAM                    userData;
@property (nonatomic, assign) DLGPROC                   proc;
@property (nonatomic, assign) int                       result;
@property (nonatomic, assign) BOOL                      ended;
@end

@implementation BrDialog
- (void)command:(id)sender
{
    NSInteger cid = [(NSView *)sender tag];
    if (self.ended)
        return;
    /* WM_COMMAND's wParam is the control id in its low word, which is all
     * the original procedures ever look at. */
    self.proc((__bridge HWND)self, WM_COMMAND, (WPARAM)cid, 0);
}
@end

/* The dialog currently running. SetVideo is strictly modal and never nests,
 * so one is enough; the HWND handed to the procedures is this object. */
static BrDialog *gCurrent = nil;

static const BrDlgTemplate *FindTemplate(int id)
{
    for (int i = 0; i < br_dialog_count; i++)
        if (br_dialogs[i].id == id)
            return &br_dialogs[i];
    return NULL;
}

static BOOL IsClass(const BrDlgItem *it, const char *name)
{
    return strcmp(it->cls, name) == 0;
}

/* ------------------------------------------------------------------ */
/* Building a window from a template                                  */
/* ------------------------------------------------------------------ */

static void BuildControls(BrDialog *dlg, const BrDlgTemplate *t)
{
    NSView  *content = dlg.window.contentView;
    CGFloat  h       = t->cy * kDluY;
    NSFont  *font    = [NSFont systemFontOfSize:11.0];

    for (int i = 0; i < t->nitems; i++) {
        const BrDlgItem *it = &t->items[i];

        NSRect r = NSMakeRect(it->x * kDluX,
                              /* Win32 measures y downward from the top. */
                              h - (it->y + it->cy) * kDluY,
                              it->cx * kDluX,
                              it->cy * kDluY);
        NSString  *text = [NSString stringWithUTF8String:it->text];
        NSControl *view = nil;
        BOOL       isPush = NO;

        if (IsClass(it, "STATIC")) {
            NSTextField *f = [[NSTextField alloc] initWithFrame:r];
            f.stringValue     = text;
            f.editable        = NO;
            f.selectable      = NO;
            f.bordered        = NO;
            f.drawsBackground = NO;
            f.font            = font;
            /* The long explanatory paragraphs are multi-line statics. */
            f.lineBreakMode   = NSLineBreakByWordWrapping;
            f.cell.wraps      = YES;
            view = f;
        } else if (IsClass(it, "COMBOBOX")) {
            /* CBS_DROPDOWNLIST: pick from the list, no typing. The template
             * height covers the dropped-down list, so use a standard row. */
            r.size.height = 22;
            r.origin.y    = h - (it->y * kDluY) - 22;
            NSPopUpButton *p = [[NSPopUpButton alloc] initWithFrame:r
                                                          pullsDown:NO];
            p.font = font;
            view = p;
        } else if (IsClass(it, "AUTOCHECKBOX") || IsClass(it, "CHECKBOX")) {
            NSButton *b = [[NSButton alloc] initWithFrame:r];
            b.title      = text;
            b.buttonType = NSButtonTypeSwitch;
            b.font       = font;
            view = b;
        } else if (IsClass(it, "AUTORADIOBUTTON") ||
                   IsClass(it, "RADIOBUTTON")) {
            NSButton *b = [[NSButton alloc] initWithFrame:r];
            b.title      = text;
            b.buttonType = NSButtonTypeRadio;
            b.font       = font;
            /* AppKit groups radios by superview, which is exactly the
             * grouping CheckRadioButton(0x3eb, 0x3ed, …) expects. */
            view = b;
        } else {                                  /* PUSHBUTTON / DEF… */
            NSButton *b = [[NSButton alloc] initWithFrame:r];
            b.title       = text;
            b.bezelStyle  = NSBezelStyleRounded;
            b.buttonType  = NSButtonTypeMomentaryPushIn;
            b.font        = font;
            if (IsClass(it, "DEFPUSHBUTTON"))
                b.keyEquivalent = @"\r";
            else if (it->id == IDCANCEL)
                b.keyEquivalent = @"\033";
            isPush = YES;
            view = b;
        }

        view.tag = it->id;
        /* Only the push buttons post WM_COMMAND. Checkboxes and radios are
         * auto-* controls: in Win32 they toggle themselves, and the dialog
         * procedures read them back with IsDlgButtonChecked at IDOK rather
         * than acting on each click. */
        if (isPush) {
            view.target = dlg;
            view.action = @selector(command:);
        }
        [content addSubview:view];
        if (it->id != -1)
            dlg.controls[@(it->id)] = view;
    }
}

/* ------------------------------------------------------------------ */
/* DialogBoxParamA and EndDialog                                      */
/* ------------------------------------------------------------------ */

int DialogBoxParamA(HINSTANCE hInst, LPSTR templ, HWND parent,
                    DLGPROC proc, LPARAM lParam)
{
    (void)hInst; (void)parent;

    int id = (int)(unsigned long)templ;      /* MAKEINTRESOURCE ordinal */
    const BrDlgTemplate *t = FindTemplate(id);
    if (t == NULL) {
        fprintf(stderr, "setvideo: no dialog template %d (0x%x)\n", id, id);
        return 0;
    }

    @autoreleasepool {
        BrDialog *dlg  = [BrDialog new];
        dlg.controls   = [NSMutableDictionary dictionary];
        dlg.comboData  = [NSMutableArray array];
        dlg.proc       = proc;
        dlg.result     = 0;
        dlg.ended      = NO;

        NSRect frame = NSMakeRect(0, 0, t->cx * kDluX, t->cy * kDluY);
        NSWindow *w = [[NSWindow alloc]
            initWithContentRect:frame
                      styleMask:(NSWindowStyleMaskTitled |
                                 NSWindowStyleMaskClosable)
                        backing:NSBackingStoreBuffered
                          defer:NO];
        w.title = [NSString stringWithUTF8String:t->title];
        /* A programmatically created NSWindow releases itself on close, which
         * under ARC double-frees the reference held below. */
        w.releasedWhenClosed = NO;
        [w center];
        dlg.window = w;

        BrDialog *prev = gCurrent;
        gCurrent = dlg;

        BuildControls(dlg, t);

        /* WM_INITDIALOG carries the creation parameter, which for the two
         * combo dialogs is the &gSel the procedures write their choice into,
         * and for the radio dialog is the loop-carried method. */
        proc((__bridge HWND)dlg, WM_INITDIALOG, 0, lParam);

        [w makeKeyAndOrderFront:nil];
        [NSApp activateIgnoringOtherApps:YES];

        NSModalSession session = [NSApp beginModalSessionForWindow:w];
        while (!dlg.ended) {
            if ([NSApp runModalSession:session] != NSModalResponseContinue)
                break;                        /* the window was closed */
        }
        [NSApp endModalSession:session];
        [w orderOut:nil];
        [w close];

        gCurrent = prev;
        return dlg.result;
    }
}

int EndDialog(HWND hWnd, int result)
{
    BrDialog *dlg = (__bridge BrDialog *)hWnd;
    /* DlgProcComboA's IDOK path calls EndDialog twice when sel is null; Win32
     * keeps the first result, so the first call wins here too. */
    if (dlg.ended)
        return 1;
    dlg.result = result;
    dlg.ended  = YES;
    return 1;
}

/* ------------------------------------------------------------------ */
/* Control access                                                     */
/* ------------------------------------------------------------------ */

static NSView *Ctl(HWND hWnd, int id)
{
    BrDialog *dlg = (__bridge BrDialog *)hWnd;
    return dlg.controls[@(id)];
}

HWND GetDlgItem(HWND hWnd, int id)
{
    return (__bridge HWND)Ctl(hWnd, id);
}

LPARAM SetWindowLongA(HWND hWnd, int index, LPARAM value)
{
    BrDialog *dlg = (__bridge BrDialog *)hWnd;
    LPARAM old = dlg.userData;
    if (index == DWL_USER)
        dlg.userData = value;
    return old;
}

LPARAM GetWindowLongA(HWND hWnd, int index)
{
    BrDialog *dlg = (__bridge BrDialog *)hWnd;
    return index == DWL_USER ? dlg.userData : 0;
}

int CheckDlgButton(HWND hWnd, int id, UINT check)
{
    NSButton *b = (NSButton *)Ctl(hWnd, id);
    b.state = check ? NSControlStateValueOn : NSControlStateValueOff;
    return 1;
}

UINT IsDlgButtonChecked(HWND hWnd, int id)
{
    NSButton *b = (NSButton *)Ctl(hWnd, id);
    return b.state == NSControlStateValueOn ? 1 : 0;
}

int CheckRadioButton(HWND hWnd, int first, int last, int check)
{
    for (int id = first; id <= last; id++) {
        NSButton *b = (NSButton *)Ctl(hWnd, id);
        b.state = (id == check) ? NSControlStateValueOn
                                : NSControlStateValueOff;
    }
    return 1;
}

/* The five combo box messages FillComboA/FillComboB and ComboGetItemData
 * send. Item data is an int per row — the section's ordinal in the device
 * database — kept in a parallel array the way Win32 keeps it per item. */
LPARAM SendDlgItemMessageA(HWND hWnd, int id, UINT msg,
                           WPARAM wParam, LPARAM lParam)
{
    BrDialog      *dlg = (__bridge BrDialog *)hWnd;
    NSPopUpButton *p   = (NSPopUpButton *)Ctl(hWnd, id);
    if (p == nil)
        return -1;

    switch (msg) {
    case CB_ADDSTRING: {
        NSString *s = [NSString stringWithUTF8String:(const char *)lParam];
        /* NSPopUpButton drops duplicate titles; the device database has
         * none, but keep the index authoritative regardless. */
        [p addItemWithTitle:s];
        [dlg.comboData addObject:@(0)];
        return (LPARAM)([p numberOfItems] - 1);
    }
    case CB_SETITEMDATA:
        if (wParam < (WPARAM)dlg.comboData.count)
            dlg.comboData[wParam] = @(lParam);
        return 1;
    case CB_GETITEMDATA:
        if (wParam < (WPARAM)dlg.comboData.count)
            return [dlg.comboData[wParam] intValue];
        return -1;
    case CB_SETCURSEL:
        if ((NSInteger)wParam < [p numberOfItems])
            [p selectItemAtIndex:(NSInteger)wParam];
        return (LPARAM)wParam;
    case CB_GETCURSEL: {
        NSInteger i = [p indexOfSelectedItem];
        return i < 0 ? -1 : (LPARAM)i;
    }
    default:
        return 0;
    }
}

/* ------------------------------------------------------------------ */

HWND GetDesktopWindow(void)
{
    return NULL;                              /* only ever a message parent */
}

int MessageBoxA(HWND hWnd, LPCSTR text, LPCSTR caption, UINT type)
{
    (void)hWnd; (void)type;
    @autoreleasepool {
        NSAlert *a = [[NSAlert alloc] init];
        a.messageText     = [NSString stringWithUTF8String:caption];
        a.informativeText = [NSString stringWithUTF8String:text];
        [a runModal];
    }
    return IDOK;
}

/* ------------------------------------------------------------------ */
/* GUI entry point                                                    */
/* ------------------------------------------------------------------ */

int __stdcall WinMain(HINSTANCE, HINSTANCE, LPSTR, int);

/* Run the real WinMain inside a live NSApplication. */
int BrRunWizard(char *cmdline)
{
    @autoreleasepool {
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
        [NSApp finishLaunching];
        return WinMain(NULL, NULL, cmdline, 1);
    }
}
