/* Dialog templates read out of the retail SetVideo.exe by tools/rsrc_dump.py.
 *
 * The table itself is GENERATED at build time and never committed — the
 * captions are retail content, and this repository holds code only. build.sh
 * pulls SetVideo.exe off the disc image (or uses orig/) and regenerates it.
 *
 * Positions and sizes are in dialog units, exactly as the resource compiler
 * stored them; the AppKit renderer converts.
 */
#ifndef BR_DIALOGRES_H
#define BR_DIALOGRES_H

typedef struct BrDlgItem {
    int          id;        /* control id; -1 is IDC_STATIC */
    const char  *cls;       /* "STATIC", "PUSHBUTTON", "AUTOCHECKBOX", … */
    const char  *text;
    short        x, y, cx, cy;
    unsigned     style;
} BrDlgItem;

typedef struct BrDlgTemplate {
    int                id;      /* resource id: 102..108 (0x66..0x6c) */
    const char        *title;
    short              cx, cy;
    int                nitems;
    const BrDlgItem   *items;
} BrDlgTemplate;

extern const BrDlgTemplate br_dialogs[];
extern const int           br_dialog_count;

#endif /* BR_DIALOGRES_H */
