/* menu.h -- the front end's menu items and the records that embed them
 * (n64/docs/menu-analysis.md).
 */
#ifndef TGR_MENU_H
#define TGR_MENU_H

/* One row of a menu screen (20 bytes, static in .data).  A screen is a
 * NULL-terminated array of MenuItem pointers. */
typedef struct MenuItem {
    const char *label;          /* 0x00  may carry %XY colour codes */
    unsigned int flags;         /* 0x04 */
    void *icon;                 /* 0x08  decompressed icon, 0 in ROM */
    unsigned int iconRomStart;  /* 0x0C */
    unsigned int iconRomEnd;    /* 0x10 */
} MenuItem;

/* A track (0x17C bytes), a table at 0x80270854; its menu row comes first. */
typedef struct BrTrack {
    MenuItem item;              /* 0x00 */
    int x14;                    /* 0x14 */
    int present;                /* 0x18  non-zero when the track's data is there */
    char pad1c[0x17C - 0x1C];
} BrTrack;

extern BrTrack D_80270854[];

#endif
