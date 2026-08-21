/* slice2_24.h -- decompiled from BRD3D.dll, pass-24 packet
 * (address range 0x10040450 - 0x10042740).
 *
 * The packet is one module: the front-end menu.  Every function in it is a
 * per-menu-item callback with one of two shapes
 *
 *     int32_t cb(BrMenuItem *pItem);
 *     int32_t cb(BrMenuItem *pItem, int32_t *pArg);
 *
 * and almost all of them return 1.  They come in three families:
 *
 *   1. "caption setters" -- look a small integer up in a table and store it,
 *      as a 16-bit value, at pItem + 0x1E20C.  That field is a string id;
 *      0x10074030 (outside this packet) turns ids into strings.
 *
 *   2. "text setters" -- format a string into pItem + 0x2B65 and then poke
 *      the embedded sub-object at pItem + 0x2B5C through two of its vtable
 *      slots.  Which two depends on whether the text is a caption
 *      (slots +0x04 then +0x10) or a value (slots +0x08 then +0x2C).
 *
 *   3. "flag pokers" -- mask bits 0x1010 in and out of pItem + 0x1C.
 *
 * Everything the original reached through fixed addresses lives in a single
 * file-static state block reachable through BrMenuGetState(), so the ported
 * functions keep the original's argument lists exactly.  State fields are
 * named for their original address (gAA28C8 is 0x10AA28C8) because this
 * packet does not establish what most of them mean, and guessing was
 * explicitly out of scope.
 */
#ifndef SLICE2_24_H
#define SLICE2_24_H

#include <stddef.h>
#include <stdint.h>

/* =====================================================================
 * 1. The menu item
 * ===================================================================== */

struct BrMenuText;

/* The vtable of the sub-object embedded at pItem + 0x2B5C.  Only four slots
 * are reached from this packet; the rest are here so the reached ones keep
 * their original indices.  No slot's meaning is established, so they are
 * named for their byte offset. */
typedef struct BrMenuTextVtbl {
    void (*pfn00)(struct BrMenuText *pThis);
    void (*pfn04)(struct BrMenuText *pThis);  /* after a CAPTION assignment */
    void (*pfn08)(struct BrMenuText *pThis);  /* after a VALUE assignment   */
    void (*pfn0C)(struct BrMenuText *pThis);
    void (*pfn10)(struct BrMenuText *pThis);  /* follows pfn04 */
    void (*pfn14)(struct BrMenuText *pThis);
    void (*pfn18)(struct BrMenuText *pThis);
    void (*pfn1C)(struct BrMenuText *pThis);
    void (*pfn20)(struct BrMenuText *pThis);
    void (*pfn24)(struct BrMenuText *pThis);
    void (*pfn28)(struct BrMenuText *pThis);
    void (*pfn2C)(struct BrMenuText *pThis);  /* follows pfn08 */
} BrMenuTextVtbl;

/* DEVIATION: the original's text buffer is unbounded -- it strcpy()s and
 * strcat()s into pItem + 0x2B65 with no limit at all.  A fixed size is given
 * here and every write is bounded. 256 is a choice, not a fact. */
#define BR_MENUTEXT_MAX 256

/* The sub-object at pItem + 0x2B5C.  Its layout is forced by the code: the
 * vtable pointer is at its +0x00 (item +0x2B5C), the byte item +0x2B64 is at
 * its +0x08, and the text buffer item +0x2B65 is at its +0x09. */
typedef struct BrMenuText {
    const BrMenuTextVtbl *pVtbl;   /* +0x00  (item +0x2B5C) */
    uint32_t              f04;     /* +0x04  (item +0x2B60) -- untouched here */
    uint8_t               f08;     /* +0x08  (item +0x2B64) */
    char                  sz[BR_MENUTEXT_MAX]; /* +0x09 (item +0x2B65) */
} BrMenuText;

/* TRUE OFFSETS, not a compression.  The three reached fields sit at the
 * displacements the original encodes into the instruction stream --
 * `mov word ptr [edx + 0x1E20C], cx` is the store every caption setter ends
 * with -- so the struct has to be a byte image or those functions can never
 * come out bit-identical.  The padding is dead weight to the port and load
 * bearing to the matching build; it is not a guess about what lives in the
 * gaps, and nothing here reads it.  (It WAS a three-field compression; that
 * cost the caption family every match it could have had.) */
typedef struct BrMenuItem {
    uint8_t    _pad00[0x1C];
    uint32_t   f1C;      /* +0x001C  -- bits 0x1010 are masked in and out */
    uint8_t    _pad20[0x2B5C - 0x20];
    BrMenuText text;     /* +0x2B5C */
    uint8_t    _padText[0x1E20C - (0x2B5C + sizeof(BrMenuText))];
    int16_t    f1E20C;   /* +0x1E20C -- string id, written as a 16-bit word */
} BrMenuItem;

/* Compile-time proof, in the project's usual idiom: wrong padding is a
 * negative array size here rather than a store into the wrong field.
 *
 * Only asserted on a 32-bit target.  `text` leads with a pointer, and 0x2B5C
 * is 4-aligned but not 8-aligned, so an LP64 build legitimately slides it to
 * 0x2B60.  That is harmless -- the port never aliases one of these over a
 * BrUiCtl_, slice8_90.c's marshal copies field by field -- and the matching
 * build, which is the only one that needs the byte image, is 32-bit. */
#if !defined(__LP64__) && !defined(_WIN64)
#define BR_MI_AT(name, off) \
    typedef char BrMenuItemAt_##name[(offsetof(BrMenuItem, name) == (off)) ? 1 : -1]
BR_MI_AT(f1C,    0x0001C);
BR_MI_AT(f1E20C, 0x1E20C);
BR_MI_AT(text,   0x02B5C);
#endif

/* =====================================================================
 * 2. The stage table
 * ===================================================================== */

/* The array based at 0x100B3810.  Stride 0x18 is proved by three independent
 * access patterns in this packet: `[esi*8 + 0x100B3810]` with esi = 3*i,
 * `[eax*8 + 0x100B3818]` with eax = 3*i, and `[edx*2 + 0x100B3820]` with
 * edx = k + 12*i.  Nothing here says what the fields mean.
 *
 * f10 is read one byte at a time: the low byte of f10[k] indexes the caption
 * table used by BrMenuCap0730 and the two best-time float arrays, and the
 * high byte of f10[k] indexes the one used by BrMenuCap07E0. */
typedef struct BrMenuStage {
    int32_t  f00;      /* +0x00 -- a string id, fed to 0x10074030 */
    int32_t  f04;      /* +0x04 -- not read by this packet */
    int32_t  f08;      /* +0x08 */
    int32_t  f0C;      /* +0x0C -- not read by this packet */
    uint16_t f10[4];   /* +0x10 */
} BrMenuStage;

/* =====================================================================
 * 3. Module state
 * ===================================================================== */

typedef struct BrMenuState {
    /* The three float arrays and the stage table are supplied by the host.
     * The original's are fixed arrays at 0x10AA25A0, 0x10AA27A0, 0x10AA27FC
     * and 0x100B3810; their extents are not determinable from this packet,
     * so they are pointers rather than guessed-at arrays. */
    const BrMenuStage *pStages;    /* 0x100B3810 */
    const float       *pTimes25A0; /* 0x10AA25A0 */
    const float       *pTimes27A0; /* 0x10AA27A0 */
    const float       *pTimes27FC; /* 0x10AA27FC */

    /* .data globals, shown with the value the image ships them with. */
    uint32_t g0AA010;    /* 0x100AA010  init 0 */
    uint32_t g0AC648;    /* 0x100AC648  init 2 */
    uint32_t g0AC64C;    /* 0x100AC64C  init 1 */
    uint32_t g0AC650;    /* 0x100AC650  init 1 */
    int32_t  g0AC6A0;    /* 0x100AC6A0 */
    int32_t  g0BD3E0;    /* 0x100BD3E0 */
    uint32_t g220B24;    /* 0x10220B24 */
    uint32_t g18ABDBC;   /* 0x118ABDBC */
    int32_t  gACEE50;    /* 0x10ACEE50 */
    uint32_t gACED34_present; /* 0x10ACED34 != 0 -- see BrMenuAutoSaveName */

    uint32_t gAA2840;    /* 0x10AA2840 */
    uint32_t gAA2844;    /* 0x10AA2844 */
    uint32_t gAA2850;    /* 0x10AA2850 */
    uint32_t gAA287C;    /* 0x10AA287C */
    uint32_t gAA289C;    /* 0x10AA289C */
    uint32_t gAA28A0;    /* 0x10AA28A0 */
    uint32_t gAA28A4;    /* 0x10AA28A4 */
    uint8_t  gAA28A8;    /* 0x10AA28A8 -- read as a byte */
    uint32_t gAA28AC;    /* 0x10AA28AC */
    uint8_t  gAA28B8;    /* 0x10AA28B8 -- read with movsx, so SIGNED */
    int32_t  gAA28C4;    /* 0x10AA28C4 */
    float    gAA28C8;    /* 0x10AA28C8 */
    float    gAA28CC;    /* 0x10AA28CC */
    uint32_t gAA28D0;    /* 0x10AA28D0 */
    uint32_t gAA28D8;    /* 0x10AA28D8 */
    uint32_t gAA28E0;    /* 0x10AA28E0 */
    uint32_t gAA28E4;    /* 0x10AA28E4 */
    uint32_t gAA28E8;    /* 0x10AA28E8 */
    uint32_t gAA2904;    /* 0x10AA2904 */
    uint32_t gAA2964;    /* 0x10AA2964 */
    uint32_t gAA2A00;    /* 0x10AA2A00 */
    uint32_t gAA2A08;    /* 0x10AA2A08 */
    uint32_t gAA2A0C;    /* 0x10AA2A0C -- 0..3 selects the branch in 0x10040450 */
    uint32_t gAA2A1C;    /* 0x10AA2A1C */
    uint32_t gAA2A20;    /* 0x10AA2A20 */
    uint32_t gAA2A24;    /* 0x10AA2A24 */
    uint32_t gAA2A28;    /* 0x10AA2A28 */
    uint32_t gAA2A38;    /* 0x10AA2A38 */
    uint32_t gAA2A3C;    /* 0x10AA2A3C */
    uint32_t gAA33C0[4]; /* 0x10AA33C0 .. 0x10AA33CC, scanned as a group */
    uint32_t gAA33E4;    /* 0x10AA33E4 */

    /* source globals copied by BrMenuSeedFrom25D4 / BrMenuSeedFrom26F0 */
    uint8_t  gAA25D4;    /* 0x10AA25D4 */
    uint32_t gAA25D8;    /* 0x10AA25D8 */
    uint32_t gAA25DC;    /* 0x10AA25DC */
    uint32_t gAA26F0;    /* 0x10AA26F0 */
    uint8_t  gAA26F4;    /* 0x10AA26F4 */
    uint8_t  gAA26F5;    /* 0x10AA26F5 */

    /* global scratch buffers the original sprintf()s into.
     * DEVIATION: their real sizes are unknown; 32 is a choice. */
    char     gAA2518[32];  /* 0x10AA2518 */
    char     gA9D618[32];  /* 0x10A9D618 */

    /* 0x11782CD0 -- where BrMenuAutoSaveName drops "AutoSave.brf". */
    char     g1782CD0[64];
} BrMenuState;

BrMenuState *BrMenuGetState(void);

/* =====================================================================
 * 4. Cross-slice imports
 * ===================================================================== */

/* 0x10074030: id in [1, 0x12F) ? g_StringTable[id] : NULL. */
/* XSLICE 0x10074030 */
extern char *BrStringById(int32_t id);

/* XSLICE 0x1005FF30 */
extern void BrMenuSub1005FF30(void);
/* XSLICE 0x1005FF60 */
extern void BrMenuSub1005FF60(void);
/* XSLICE 0x1005FFF0 */
extern void BrMenuSub1005FFF0(void);
/* XSLICE 0x100709A0 */
extern void BrMenuSub100709A0(void);
/* XSLICE 0x10044B90 */
extern void BrMenuSub10044B90(int32_t n);
/* XSLICE 0x10044E20 */
extern void BrMenuSub10044E20(int32_t n);

/* The record at 0x10ACED34 that BrMenuAutoSaveName clears.  Its layout is not
 * established beyond the offsets touched (bytes +4 and +5, then three runs of
 * zeroes at +6, +0x1E and +0x50), so it is a byte pointer. */
extern uint8_t *g_pBrMenuACED34;

/* =====================================================================
 * 5. Shared helpers
 * ===================================================================== */

/* The MM:SS.hh formatter that 0x10040C00, 0x10040D70, 0x10040EE0,
 * 0x10041040 and 0x10041180 each inline verbatim.  Writes "--:--" when
 * fTime is not strictly greater than zero (NaN lands there too, because the
 * original tests the x87 C3|C0 pair).  See the comment at the definition for
 * the two float multiplies that make this NOT equal to integer division. */
void BrMenuFormatLapTime(char *pszOut, size_t cbOut, float fTime);

/* =====================================================================
 * 6. Callbacks
 * ===================================================================== */

/* -- 0x10040680 ------------------------------------------------------- */
int32_t BrMenuEnter(void);

/* -- caption setters, family 1 ---------------------------------------- */
int32_t BrMenuCap0730(BrMenuItem *pItem);
int32_t BrMenuCap07A0(BrMenuItem *pItem);
int32_t BrMenuCap07E0(BrMenuItem *pItem);
int32_t BrMenuCap0870(BrMenuItem *pItem);
int32_t BrMenuCap0890(BrMenuItem *pItem);
int32_t BrMenuCap08B0(BrMenuItem *pItem);
int32_t BrMenuCap0930(BrMenuItem *pItem);
int32_t BrMenuCap0950(BrMenuItem *pItem);
int32_t BrMenuCap0990(BrMenuItem *pItem);
int32_t BrMenuCap09B0(BrMenuItem *pItem);
int32_t BrMenuCap09D0(BrMenuItem *pItem);
int32_t BrMenuCap1870(BrMenuItem *pItem);

/* -- state seeding ---------------------------------------------------- */
int32_t BrMenuSeedFrom25D4(void);   /* 0x100409F0 */
int32_t BrMenuSeedFrom26F0(void);   /* 0x10040A20 */

/* -- text setters, family 2 ------------------------------------------- */
int32_t BrMenuText08D0(BrMenuItem *pItem);   /* 0x100408D0 */
int32_t BrMenuText0A50(BrMenuItem *pItem);   /* 0x10040A50 */
int32_t BrMenuText0AC0(BrMenuItem *pItem);   /* 0x10040AC0 */
int32_t BrMenuText0B30(BrMenuItem *pItem);   /* 0x10040B30 */
int32_t BrMenuTime0C00(BrMenuItem *pItem);   /* 0x10040C00 */
int32_t BrMenuTime0D70(BrMenuItem *pItem);   /* 0x10040D70 */
int32_t BrMenuTime0EE0(BrMenuItem *pItem);   /* 0x10040EE0 */
int32_t BrMenuTime1040(BrMenuItem *pItem);   /* 0x10041040 */
int32_t BrMenuTime1180(BrMenuItem *pItem);   /* 0x10041180 */
int32_t BrMenuText1300(BrMenuItem *pItem);   /* 0x10041300 */
int32_t BrMenuText15A0(BrMenuItem *pItem);   /* 0x100415A0 */
int32_t BrMenuText1670(BrMenuItem *pItem);   /* 0x10041670 */
int32_t BrMenuText1710(BrMenuItem *pItem);   /* 0x10041710 */
int32_t BrMenuText17B0(BrMenuItem *pItem);   /* 0x100417B0 */

/* -- flag pokers, family 3 -------------------------------------------- */
int32_t BrMenuFlags1890(BrMenuItem *pItem);  /* 0x10041890 */
int32_t BrMenuFlags18D0(BrMenuItem *pItem);  /* 0x100418D0 */
int32_t BrMenuFlags18F0(BrMenuItem *pItem);  /* 0x100418F0 */

/* -- misc ------------------------------------------------------------- */
int32_t BrMenuLeaveTo2(void);       /* 0x10041930 */
void    BrMenuAutoSaveName(void);   /* 0x10041B50 */

#endif /* SLICE2_24_H */
