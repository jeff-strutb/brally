/* slice3_39.h -- Boss Rally (BRD3D.dll) slice 3, a later pass.
 *
 * Packet range 0x1005AE70 - 0x100607B0 (43 functions).  Only the parts that
 * could be resolved with confidence are ported; see the report / the
 * "NOT PORTED" list at the bottom of this file.
 *
 * What is in here:
 *
 *   1. The glyph-metric font tables and the two string-measuring methods
 *      that walk them                              (0x1005B0D0, 0x1005B160)
 *   2. The 0x438-byte text widget those methods live on -- constructor,
 *      scalar deleting destructor, horizontal centring
 *                              (0x1005B050, 0x1005B0A0, 0x1005B200)
 *   3. The WM_CHAR -> character map                          (0x1005B540)
 *   4. The container that owns 100 of those widgets plus 100 opaque blobs
 *           (0x1005B7F0, 0x1005B8D0, 0x1005B8F0, 0x1005C200)
 *   5. DirectInput keyboard / button edge detection
 *      (0x1005FF60, 0x1005FFB0, 0x1005FFD0, 0x1005FFF0)
 *   6. Small utilities             (0x10060210, 0x100602B0, 0x10060780)
 *
 * Field names that could not be justified are positional (fNN = the byte
 * offset in the ORIGINAL layout, which this port does not reproduce
 * bit-for-bit -- pointers are wider here, so every struct is indexed by
 * element, never by byte).
 */
#ifndef SLICE3_39_H
#define SLICE3_39_H

#include <stdint.h>
#include <stddef.h>

/* 0x100602B0 works on the object slice1_07.h calls BrDevSlot (0x10060280
 * clears it).  Only a forward declaration is taken here: slice1_07.h and
 * slice1_06.h currently declare `BrTriContainsPoint` with incompatible
 * signatures, so pulling slice1_07.h in from this header would spread that
 * clash.  slice3_39.c includes it; a caller of BrDevSlotReleaseIface must
 * include it too. */
struct BrDevSlot;

/* =====================================================================
 * 1. Glyph metrics
 *
 * Two tables, both indexed by `ch - 0x20` with a stride of 12 bytes:
 *
 *     0x100AC6E4  font A  ("small", 16 px tall, proportional)
 *     0x100ACB5C  font B  ("large", 40 x 45, digits only)
 *
 * The three fields the code reads are all at the front of the 12-byte
 * record; the trailing 4 bytes are a `const char *` glyph name that belongs
 * to a NEIGHBOURING entry (record i's +8 points at the name of char i+1).
 * The exact framing of that pointer array was not resolved, so it is not
 * reproduced -- nothing in this packet reads it.
 *
 * 0xFFFF in `advance` / `height` / `sprite` means "no glyph".
 *
 * GOTCHA -- font B is SHORTER THAN THE INDEX RANGE.  Only 64 records
 * (0x20..0x5F) are real; 0x100ACB5C + 64*12 == 0x100ACE58, which is where
 * the character map (section 3) starts.  Both 0x1005B160 (up to 0x7E) and
 * 0x1005B460 (up to 0x7A) index past that, so the original reads the
 * character map as if it were font metrics.  Entries 64..94 of
 * `g_BrGlyphFontB` below are those exact bytes, so the over-read is
 * reproduced rather than papered over.
 * ===================================================================== */

#define BR_GLYPH_FIRST  0x20    /* first char the tables cover            */
#define BR_GLYPH_COUNT  95      /* 0x20..0x7E -- the range the code indexes */
#define BR_GLYPH_NONE   0xFFFFu

typedef struct BrGlyphMetric {
    uint16_t advance;   /* +0x00 */
    uint16_t height;    /* +0x02 */
    uint16_t sprite;    /* +0x04 */
    uint16_t f06;       /* +0x06 -- 0 everywhere in the image */
} BrGlyphMetric;

extern BrGlyphMetric g_BrGlyphFontA[BR_GLYPH_COUNT];  /* 0x100AC6E4 */
extern BrGlyphMetric g_BrGlyphFontB[BR_GLYPH_COUNT];  /* 0x100ACB5C */

/* The literal the space character is worth when it has no glyph. */
#define BR_GLYPH_SPACE_ADVANCE 6

/* =====================================================================
 * 2. The text widget (vtable 0x1008F728, 0x438 bytes in the original)
 * ===================================================================== */

struct BrTextBox;

/* 0x1008F728.  12 slots.  Only the four the ported code reaches are typed;
 * the others keep their original index under a positional name.
 *
 *   +0x00 0x1005B0A0  scalar deleting destructor
 *   +0x04 0x1005B0D0  measure with font A
 *   +0x08 0x1005B160  measure with font B
 *   +0x0C 0x1005B390
 *   +0x10 0x1005B2B0
 *   +0x14 0x1005B570
 *   +0x18 0x1005B730  draw one glyph, font A   (4 args)
 *   +0x1C 0x1005B7E0
 *   +0x20 0x1005B7A0  draw one glyph, font B   (4 args)
 *   +0x24 0x1005B250
 *   +0x28 0x1005B200  centre horizontally, RETURNS THE FLOAT
 *   +0x2C 0x1005B460
 */
typedef struct BrTextBoxVtbl {
    struct BrTextBox *(*pfn00)(struct BrTextBox *pThis, uint32_t flags);
    void  (*pfn04)(struct BrTextBox *pThis);
    void  (*pfn08)(struct BrTextBox *pThis);
    void  (*pfn0C)(struct BrTextBox *pThis);
    void  (*pfn10)(struct BrTextBox *pThis);
    void  (*pfn14)(struct BrTextBox *pThis);
    void  (*pfn18)(struct BrTextBox *pThis);
    void  (*pfn1C)(struct BrTextBox *pThis);
    void  (*pfn20)(struct BrTextBox *pThis);
    void  (*pfn24)(struct BrTextBox *pThis);
    float (*pfn28)(struct BrTextBox *pThis);
    void  (*pfn2C)(struct BrTextBox *pThis);
} BrTextBoxVtbl;

/* The original buffer really is 0x400 bytes at +0x09 (the constructor's
 * `rep stosd` with ecx = 0x100 starting at this+9). */
#define BR_TEXTBOX_MAX 0x400

typedef struct BrTextBox {
    const BrTextBoxVtbl *pVtbl;      /* +0x000 */
    uint32_t  f04;                   /* +0x004  bit 0 selects a vtable path */
    uint8_t   f08;                   /* +0x008  set to 1 by the constructor */
    char      sz[BR_TEXTBOX_MAX];    /* +0x009 */
    int16_t   width;                 /* +0x40A  written by both measurers   */
    int16_t   height;                /* +0x40C  SEEDED, not reset -- see below */
    float     x;                     /* +0x410  written by 0x1005B200       */
    float     y;                     /* +0x414 */
    uint32_t  f418;                  /* +0x418 */
    int16_t   f41C;                  /* +0x41C  a width limit (0x1005B570)  */
    uint32_t  f420;                  /* +0x420  non-zero -> vtbl +0x24 runs */
    int32_t   left;                  /* +0x424 */
    int32_t   f428;                  /* +0x428 */
    int32_t   right;                 /* +0x42C */
    int32_t   f430;                  /* +0x430 */
    int32_t   f434;                  /* +0x434 */
} BrTextBox;

/* The constructor plants 0x1008F728 in +0x00.  That vtable's slots are
 * mostly outside this packet, so the value is taken from this hook instead
 * of being hard-coded; set it before calling BrTextBoxInit if the vtable
 * matters.  Default NULL. */
extern const BrTextBoxVtbl *g_pBrTextBoxVtbl;   /* stands in for 0x1008F728 */

/* 0x1005B050 (thiscall).  Zeroes sz[], width, height, x, y, f418, f41C,
 * f420 and f04; sets f08 = 1; returns pBox.
 *
 * GOTCHA: left/f428/right/f430/f434 are NOT initialised, and the allocator
 * (0x1007DFE0) does not zero.  Reproduced -- this function leaves them
 * alone. */
BrTextBox *BrTextBoxInit(BrTextBox *pBox);

/* 0x1005B0A0 (thiscall, __stdcall tail).  MSVC scalar deleting destructor:
 * run the destructor, then `operator delete` if bit 0 of flags is set.
 * Returns pBox EVEN WHEN IT HAS JUST BEEN FREED -- that is the original. */
BrTextBox *BrTextBoxDeleteDtor(BrTextBox *pBox, uint32_t flags);

/* 0x1005B0D0 (thiscall) -- measure sz[] with font A.
 * 0x1005B160 (thiscall) -- measure sz[] with font B.
 *
 * Both write `width` and `height`.  Shared behaviour, all of it load-bearing:
 *
 *   - `height` is SEEDED with the field's current value and only ever grows
 *     (signed 16-bit max).  It is not reset, so measuring twice with
 *     different strings keeps the taller of the two.
 *   - Any byte < 0x20 or >= 0x80 STOPS the walk; the partial width and
 *     height computed so far are still stored.
 *   - 0x20 with no glyph contributes BR_GLYPH_SPACE_ADVANCE (6).
 *   - Both accumulate in 16 bits and store 16 bits, so a long string wraps.
 *
 * The two differ in three ways:
 *
 *   - A adds `advance`; B adds `advance - 4`.
 *   - A takes metrics from font A; B takes metrics from font B but tests
 *     FONT A's advance/height for the 0xFFFF "no glyph" sentinel.  A char
 *     that font A can draw and font B cannot still goes down the B path.
 *   - A covers 0x21..0x7E; B covers 0x21..0x7E as well, which over-reads
 *     font B (see the GOTCHA in section 1).
 */
void BrTextBoxMeasureA(BrTextBox *pBox);
void BrTextBoxMeasureB(BrTextBox *pBox);

/* 0x1005B200 (thiscall) -- centre the measured text between left and right.
 *
 *     v = left + 0.5f * (right - left - width)
 *
 * Stores v in `x` AND returns it (the original leaves it on the x87 stack).
 * `width` is read as a SIGNED 16-bit value. */
float BrTextBoxCentreX(BrTextBox *pBox);

/* =====================================================================
 * 3. The character map (0x100ACE58)
 *
 * 8-byte records {uint32 code; uint32 ch}.  0x1005B570 feeds it the WM_CHAR
 * wParam that 0x10060060 stashed in 0x10AA33E4 and appends the result to a
 * text widget.  Mostly identity over 0x20..0x7E, plus three VK_OEM codes
 * (0xBA -> ':', 0xBD -> '-', 0xBE -> '.').
 *
 * GOTCHA: the original's loop bound is the ADDRESS 0x100AE6D8, i.e. 784
 * records, but only the first 98 are real -- 0x100ACE58 + 98*8 == 0x100AD168
 * is where the string pool starts.  The remaining 686 "records" are string
 * literals and pointers.  A code that happens to equal one of those dwords
 * gets a garbage character back.
 *
 * DEVIATION: only the 98 real records are reproduced.  Codes that would have
 * matched the string pool return 0 here instead of garbage.
 * ===================================================================== */

#define BR_CHARMAP_COUNT 98

typedef struct BrCharMapEntry {
    uint32_t code;   /* +0x00 */
    uint32_t ch;     /* +0x04 -- only the low byte is returned */
} BrCharMapEntry;

extern BrCharMapEntry g_BrCharMap[BR_CHARMAP_COUNT];   /* 0x100ACE58 */

/* 0x1005B540 (cdecl).  First match wins; 0 if there is none. */
uint8_t BrCharMapLookup(int32_t code);

/* =====================================================================
 * 4. The container (vtable 0x1008F758)
 *
 * Original layout, all of it forced by the constructor at 0x1005B7F0 and by
 * the index arithmetic elsewhere:
 *
 *     +0x00000  vtable (0x1008F758)
 *     +0x0002C  BrTextBox items[100]           (stride 0x438)
 *     +0x1A60C  { uint32 size; void *p } blobs[100]   (stride 8)
 *     +0x1A92C  int16 count
 *     ...
 *     +0x1A9D0  last field the constructor zeroes
 *
 * 0x2C + 100 * 0x438 == 0x1A60C and 0x1A60C + 100 * 8 == 0x1A92C, so the
 * three arrays are exactly back to back.  Only the members this packet
 * needs are modelled.
 * ===================================================================== */

#define BR_TEXTLIST_ITEMS 100

struct BrTextList;

/* 0x1008F758.  Sixteen slots.
 *
 * +0x10 and +0x14 ARE called -- by the menu builders in slice6_72.c and
 * slice6_73.c, through the BrTextList embedded at control +0x3838 (br_ui.h
 * ADJ-6).  Both packets derived the same two signatures independently, so
 * they are typed here rather than being re-declared as a private vtable view
 * in each caller.  The other fourteen stay `void *`: nothing calls them, and
 * a guessed signature is worse than none. */
typedef struct BrTextListVtbl {
    void *f00, *f04, *f08, *f0C;                          /* +0x00 .. +0x0C */

    /* +0x10 __thiscall -- append one row of text. */
    void (*f10)(struct BrTextList *pThis, const void *pText, int32_t a2,
                int32_t a3, const void *pStyle, int32_t a5);

    /* +0x14 __thiscall -- configure the list. */
    void (*f14)(struct BrTextList *pThis, int32_t a1, const void *pStyle,
                int32_t a3, int32_t a4, int32_t a5);

    void *f18, *f1C, *f20, *f24, *f28, *f2C;              /* +0x18 .. +0x2C */
    void *f30, *f34, *f38, *f3C;                          /* +0x30 .. +0x3C */
} BrTextListVtbl;

/* One raw dword of the list's tail.  The original writes those slots as
 * plain dwords in some paths and as x87 floats in others -- 0x1004DFC0 does
 * `mov ecx,[ebx+0x1E200] / mov [ebx+0x1E1E8],ecx` on two arms and
 * `fld/fsubr/fstp` on the third, over the SAME three addresses.  A union of
 * the three readings is what a raw dword actually is here; naming it one of
 * them would make two of the three arms a pun. */
typedef union BrTextWord {
    uint32_t u;
    int32_t  i;
    float    f;
} BrTextWord;

typedef struct BrTextBlob {
    uint32_t size;   /* +0x00  (list +0x1A60C + i*8) */
    void    *p;      /* +0x04  (list +0x1A610 + i*8) */
} BrTextBlob;

/* The three header words the original stores CODE ADDRESSES into.
 *
 * f04, f0C and f14 were `uint32_t`, and that is a LP64 truncation waiting to
 * happen rather than a claim about the object: the callers store function
 * addresses there.  The one that forced the change is
 *
 *     1004F96F  mov dword ptr [ebp+0x383c], 0x10042170
 *
 * i.e. control +0x383C, which br_ui.h's ADJ-6 maps to list +0x04 -- a code
 * address, not a number.  slice6_73.h independently types control +0x383C and
 * +0x384C (list +0x04 and +0x14) as function pointers for the same reason.
 * Nothing in the port CALLS them, so the type stays deliberately shapeless:
 * a caller that learns a slot's real signature casts at the call site.
 *
 * `= 0` still works on every one of them (a null pointer constant), which is
 * all slice3_39.c's constructor does with them. */
typedef void (*BrTextListCbFn)(void);

typedef struct BrTextList {
    const BrTextListVtbl *pVtbl;   /* +0x00000 */
    BrTextListCbFn f04;            /* +0x00004  a callback -- see above */
    uint32_t   f08;                /* +0x00008 */
    BrTextListCbFn f0C;            /* +0x0000C  a callback in the original */
    uint32_t   f10;                /* +0x00010 */
    BrTextListCbFn f14;            /* +0x00014  a callback in the original */
    uint32_t   f18;                /* +0x00018  flag word */
    uint32_t   f1C;                /* +0x0001C */
    uint32_t   f20;                /* +0x00020 */
    BrTextBox  aItems[BR_TEXTLIST_ITEMS];   /* +0x0002C */
    BrTextBlob aBlobs[BR_TEXTLIST_ITEMS];   /* +0x1A60C */
    int16_t    count;              /* +0x1A92C */
    int16_t    f1A92E;             /* +0x1A92E  scroll offset */
    int16_t    f1A930;             /* +0x1A930  visible row count */
    int16_t    f1A932;             /* +0x1A932  init -1, later '0' (0x30) */
    int16_t    f1A934;             /* +0x1A934  init -1, later '.' (0x2E) */
    int16_t    f1A936;             /* +0x1A936  init -1 */
    int16_t    f1A938;             /* +0x1A938  init -1, later ':' (0x3A) */

    /* +0x1A93A .. +0x1A99C -- NEITHER constructor touches one byte of this.
     * 0x1005B7F0 jumps straight from the word at +0x1A938 to the dword at
     * +0x1A99C, so the whole span is indeterminate after construction
     * (`operator new` does not zero).  It was unmodelled until 0x1004DFC0
     * turned up writing two addresses inside it:
     *
     *     1004E314  mov dword ptr [ebx + 0x1e1c8], eax   ; __ftol of +0x1E1E8
     *     1004E31D  mov dword ptr [ebx + 0x1e1d0], eax   ; ... + 0x10
     *
     * and control +0x1E1C8 / +0x1E1D0 are list +0x1A990 / +0x1A998 under
     * br_ui.h's ADJ-6, which named them as the one thing the canonical
     * control could NOT express.  They are dwords; nothing reads them yet.
     * The two runs either side are spelled as byte padding rather than as
     * invented fields -- the pad SIZES are exact because the region holds no
     * pointer, so they are the same on a 32- and a 64-bit host. */
    uint8_t    pad1A93A[0x1A990u - 0x1A93Au];  /* +0x1A93A */
    int32_t    f1A990;                         /* +0x1A990  (ctl +0x1E1C8) */
    uint8_t    pad1A994[0x1A998u - 0x1A994u];  /* +0x1A994 */
    int32_t    f1A998;                         /* +0x1A998  (ctl +0x1E1D0) */

    /* +0x1A99C..+0x1A9D0, zeroed together.  Raw dwords -- see BrTextWord.
     * [5], [8], [11] and [12] are control +0x1E1E8, +0x1E1F4, +0x1E200 and
     * +0x1E204 (br_ui.h ADJ-6). */
    BrTextWord f1A99C[14];         /* +0x1A99C..+0x1A9D0 */
} BrTextList;

extern const BrTextListVtbl *g_pBrTextListVtbl;   /* stands in for 0x1008F758 */

/* 0x1005B7F0 (thiscall).  Constructs all 100 items, zeroes the blob array
 * and the header words, sets f1A932/34/36/38 to -1, returns pList.
 *
 * GOTCHA: the original sets the vtable pointer LAST, after the item array
 * has already been constructed.  Order preserved. */
BrTextList *BrTextListInit(BrTextList *pList);

/* 0x1005B8F0 (thiscall).  Sets the vtable, then runs the MSVC vector
 * destructor iterator over the 100 items (last to first). */
void BrTextListDtor(BrTextList *pList);

/* 0x1005B8D0 (thiscall, __stdcall tail).  Scalar deleting destructor. */
BrTextList *BrTextListDeleteDtor(BrTextList *pList, uint32_t flags);

/* 0x1005C200 (thiscall, __stdcall, 3 args).  Copies `size` bytes from pSrc
 * into blob slot `index`, allocating the slot on first use, and records the
 * size.  Always returns 1.
 *
 * GOTCHA: index == -1 means "the last item", i.e. count - 1, clamped up to
 * 0 when count is 0.  No other value is validated.
 *
 * GOTCHA (a real bug, reproduced): the allocation happens only when the slot
 * pointer is NULL.  A second call on the same slot with a LARGER size copies
 * the larger size into the original, smaller allocation. */
int32_t BrTextListSetBlob(BrTextList *pList, const void *pSrc,
                          uint32_t size, int32_t index);

/* =====================================================================
 * 5. Input edge detection
 *
 * DirectInput keyboard state: 256 bytes, bit 7 = down.  The prev/edge
 * arrays are 256 DWORDS each and sit either side of it:
 *
 *     0x10AA2A80  int32 edge[256]     ("pressed this frame")
 *     0x10AA2E80  (a pointer, section 6 -- immediately after edge[])
 *     0x10AA2E88  int32 prev[256]
 *     0x10AA3288  uint8 state[256]    (the raw DirectInput buffer)
 *
 * Indices are DIK scancodes, not virtual keys: 0x1005B570 tests edge[0x3B]
 * .. edge[0x3E], which are DIK_F1..DIK_F4.
 *
 * GOTCHA: 0x1005FF30 (ported in slice1_07 as BrTables64Clear) clears only
 * 0x40 dwords of each of the three.  For state[] that is the whole 256-byte
 * buffer, but for prev[] and edge[] it is the first 64 entries only.  The
 * other 192 are never cleared.
 * ===================================================================== */

#define BR_DIK_COUNT 256
#define BR_BTN_COUNT 4

extern uint8_t g_BrDikState[BR_DIK_COUNT];   /* 0x10AA3288 */
extern int32_t g_BrDikPrev [BR_DIK_COUNT];   /* 0x10AA2E88 */
extern int32_t g_BrDikEdge [BR_DIK_COUNT];   /* 0x10AA2A80 */

/* The 4-entry analogue.  NOTE for integration: slice2_25.h already
 * declares `g_brAA33D0` as a scalar at 0x10AA33D0 and slice2_24.h models
 * 0x10AA33C0 as `gAA33C0[4]`; those are the same storage as g_BrBtnEdge[0]
 * and g_BrBtnRaw. */
extern int32_t g_BrBtnRaw  [BR_BTN_COUNT];   /* 0x10AA33C0 */
extern int32_t g_BrBtnPrev [BR_BTN_COUNT];   /* 0x10AA3388 */
extern int32_t g_BrBtnEdge [BR_BTN_COUNT];   /* 0x10AA33D0 */

/* 0x1005FF60.  Name taken from slice2_24.h, which already declares it.
 *
 *     down    = (state[i] >> 7) & 1;
 *     edge[i] = (prev[i] == 0) & down;
 *     prev[i] = down;
 *
 * so edge[i] is 1 only on the frame the key goes down. */
void BrMenuSub1005FF60(void);

/* 0x1005FFF0.  Name taken from slice2_24.h.  Same shape over 4 entries, but
 * the source is a DWORD (g_BrBtnRaw), not a bit-7 byte, and the AND is a
 * bitwise AND with the raw value rather than with a 0/1 flag. */
void BrMenuSub1005FFF0(void);

/* 0x1005FFD0.  Name taken from slice2_23.h.  Index of the first non-zero
 * g_BrDikEdge entry, or -1. */
int32_t BrFn1005FFD0(void);

/* 0x1005FFB0.  Polls the device into g_BrDikState via 0x100771B0 and, only
 * if that returns >= 0, runs BrMenuSub1005FF60. */
void BrDikPollAndEdge(void);

/* =====================================================================
 * 6. Small utilities
 * ===================================================================== */

typedef struct BrPointI { int32_t x, y; } BrPointI;

extern int32_t   g_Br0A81C0;      /* 0x100A81C0  screen width  */
extern int32_t   g_Br0A81C4;      /* 0x100A81C4  screen height */
extern int32_t   g_BrAA33B8;      /* 0x10AA33B8 */
extern int32_t   g_BrAA33B4;      /* 0x10AA33B4 */
extern BrPointI *g_pBrAA2E80;     /* 0x10AA2E80 */
extern int32_t   g_BrAA3398[7];   /* 0x10AA3398 */

/* 0x10060210 (stdcall, 1 arg).  Copies the two screen dimensions into
 * 0x10AA33B8 / 0x10AA33B4, puts half of each into *g_pBrAA2E80, zeroes the
 * seven dwords at 0x10AA3398, returns 1.
 *
 * GOTCHA: the argument is never read.  It is declared here so the calling
 * convention survives.
 *
 * The halving is `cdq / sub / sar 1`, i.e. division truncated toward zero,
 * not an arithmetic shift -- it differs from `>> 1` for odd negatives. */
int32_t BrFn10060210(void *pUnused);

/* The object BrDevSlot::pIface points at.  No slot's meaning is
 * established, so they are named for their byte offset.  0x100602B0 calls
 * +0x20 then +0x08; 0x10060750 calls +0x20 or +0x1C.  `this` is passed as an
 * ordinary first stack argument, not in ecx. */
typedef struct BrDevIfaceVtbl {
    void (*pfn00)(void *pThis);
    void (*pfn04)(void *pThis);
    void (*pfn08)(void *pThis);   /* +0x08 */
    void (*pfn0C)(void *pThis);
    void (*pfn10)(void *pThis);
    void (*pfn14)(void *pThis);
    void (*pfn18)(void *pThis);
    void (*pfn1C)(void *pThis);
    void (*pfn20)(void *pThis);   /* +0x20 */
} BrDevIfaceVtbl;

typedef struct BrDevIface {
    const BrDevIfaceVtbl *pVtbl;
} BrDevIface;

/* 0x100602B0 (thiscall).  If pSlot->pIface is set, calls its +0x20 and then
 * its +0x08 and NULLs the field.  Note the order: +0x20 first. */
void BrDevSlotReleaseIface(struct BrDevSlot *pSlot);

/* 0x10060780 (cdecl).  A byte fill.
 *
 * GOTCHA: the arguments are (dst, COUNT, value) -- the value comes LAST,
 * the opposite way round from memset(dst, value, count).  Only the low byte
 * of `value` is used. */
void BrMemFill(void *pDst, uint32_t count, int32_t value);

/* =====================================================================
 * 7. Cross-slice imports
 * ===================================================================== */

/* XSLICE 0x1005B0C0 */
extern void BrTextBoxDtor(BrTextBox *pBox);

/* XSLICE 0x1007DE40 */
extern void BrOperatorDelete(void *p);

/* XSLICE 0x1007DFE0 -- signature matched to slice2_26.h / slice3_33.h,
 * which already declare it. */
extern void *BrOperatorNew(uint32_t cb);

/* 0x100771B0 -- fills the 256-byte DirectInput keyboard buffer it is given
 * and returns an HRESULT-ish value; < 0 means "no new state". */
/* XSLICE 0x100771B0 */
extern int32_t BrDikGetDeviceState(uint8_t *pState);

/* =====================================================================
 * NOT PORTED from this packet -- see the pass report for why.
 *
 *   0x1005AE70 0x1005B250 0x1005B2B0 0x1005B390 0x1005B460 0x1005B570
 *   0x1005B730 0x1005B7A0 0x1005B910 0x1005BB80 0x1005BC10 0x1005C000
 *   0x1005C270 0x1005C2C0 0x1005C510 0x1005C590 0x1005CB40 0x1005CBF0
 *   0x1005CC20 0x1005CCD0 0x1005CE30 0x10060060 0x10060260 0x100603A0
 *   0x10060750 0x100607B0
 * ===================================================================== */

#endif /* SLICE3_39_H */
