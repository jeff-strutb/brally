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
 *      ... and its two public vtable methods, the ones the menu builders call
 *      through control +0x3838       (0x1005B910 +0x14, 0x1005BC10 +0x10)
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

/* The "style" every caller passes as an opaque `const void *` is a RECTANGLE:
 * 0x1005B910 reads four consecutive int32s out of it (`fild [eax]`,
 * `[eax+4]`, `[eax+8]`, `[eax+0xC]`) and 0x1005BC10 reads [0] and [2] into the
 * item's `left` and `right`.  The pointers the builders pass all come out of
 * one 18-entry, 16-byte-stride table -- see g_aBrUiStyle below.
 *
 * The four members are named for what the two consumers do with them: [0] and
 * [2] become BrTextBox::left and ::right, [1] is a top edge that the scroll
 * geometry adds a sprite height to, and [3] a bottom edge it subtracts one
 * from.  Nothing here depends on the names. */
typedef struct BrTextStyle {
    int32_t left;    /* +0x00 */
    int32_t top;     /* +0x04 */
    int32_t right;   /* +0x08 */
    int32_t bottom;  /* +0x0C */
} BrTextStyle;

/* 0x1008F758.  Sixteen slots.  Six are known:
 *
 *   +0x00  0x1005B8D0  scalar deleting destructor  (BrTextListDeleteDtor)
 *   +0x04  0x1005C270  NOT PORTED -- draws one sprite; __stdcall, 3 args
 *   +0x08  0x1005C2C0  NOT PORTED -- draws the whole list; __thiscall, 1 arg
 *   +0x0C  0x10042AF0  `mov eax,1 / ret` (slice5_61's BrGfx42AF0_1)
 *   +0x10  0x1005BC10  append one row       (BrTextListAddRow)
 *   +0x14  0x1005B910  configure the list   (BrTextListConfig)
 *
 * SIGNATURE CONFLICT, resolved in favour of the disassembly: this header used
 * to type f10 and f14 as returning `void`.  Both end `mov eax,1 / ret 0x14`,
 * and f10 has two more `return 0` exits besides.  They return int32_t.  Every
 * call site in the tree discards the result, so nothing else had to change --
 * but a `void`-returning function pointer assigned an int32-returning function
 * is a constraint violation, so the slot type is the one that had to move.
 *
 * The remaining slots stay `void *`: nothing calls them and a guessed
 * signature is worse than none.  +0x2C is the exception -- it is not guessed,
 * it is OBSERVED, at 0x1005BC38 (`push 0 / mov ecx,ebp / call [eax+0x2C]`), so
 * it is typed. */
typedef struct BrTextListVtbl {
    void *f00, *f04, *f08, *f0C;                          /* +0x00 .. +0x0C */

    /* +0x10 __thiscall -- append one row of text.  0x1005BC10. */
    int32_t (*f10)(struct BrTextList *pThis, const void *pText, int32_t a2,
                   int32_t a3, const void *pStyle, int32_t a5);

    /* +0x14 __thiscall -- configure the list.  0x1005B910. */
    int32_t (*f14)(struct BrTextList *pThis, int32_t a1, const void *pStyle,
                   int32_t a3, int32_t a4, int32_t a5);

    void *f18, *f1C, *f20, *f24, *f28;                    /* +0x18 .. +0x28 */

    /* +0x2C __thiscall, one argument -- "make room", called by
     * BrTextListAddRow when the item array is already full. */
    void (*f2C)(struct BrTextList *pThis, int32_t a1);

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

    /* +0x1C and +0x20 were `uint32_t`, and both are FLOATS in the original:
     * 0x1005B910 writes them with `fild [eax] / fstp [esi+0x1C]` and
     * `fild [eax+4] / fstp [esi+0x20]`, and 0x1005BC10 reads +0x20 back with
     * `fld [ebp+0x20] / call __ftol`.  BrTextWord (below) is the type that
     * already exists in this header for exactly this situation. */
    BrTextWord f1C;                /* +0x0001C  (float) pStyle->left  */
    BrTextWord f20;                /* +0x00020  (float) pStyle->top   */
    BrTextBox  aItems[BR_TEXTLIST_ITEMS];   /* +0x0002C */
    BrTextBlob aBlobs[BR_TEXTLIST_ITEMS];   /* +0x1A60C */
    int16_t    count;              /* +0x1A92C */
    int16_t    f1A92E;             /* +0x1A92E  scroll offset */
    int16_t    f1A930;             /* +0x1A930  visible row count */
    int16_t    f1A932;             /* +0x1A932  init -1, later '0' (0x30) */
    int16_t    f1A934;             /* +0x1A934  init -1, later '.' (0x2E) */
    int16_t    f1A936;             /* +0x1A936  init -1 */
    int16_t    f1A938;             /* +0x1A938  init -1, later ':' (0x3A) */

    /* +0x1A93A .. +0x1A99C.  NEITHER constructor touches one byte of this --
     * 0x1005B7F0 jumps straight from the word at +0x1A938 to the dword at
     * +0x1A99C, so the whole span is indeterminate after construction
     * (`operator new` does not zero).  It used to be spelled as byte padding
     * with f1A990 / f1A998 poking out of it, because only those two had known
     * writers (0x1004DFC0's `mov [ebx+0x1E1C8]` / `[ebx+0x1E1D0]`, which are
     * list +0x1A990 / +0x1A998 under br_ui.h's ADJ-6).
     *
     * 0x1005B910 settles the rest: it writes EVERY dword from +0x1A93C to
     * +0x1A998 inclusive -- twenty-four of them, on a clean 4-byte grid, with
     * a 2-byte hole at +0x1A93A left by the int16 run above.  So the region is
     * not padding at all, and the two named fields keep their names and their
     * offsets while the other twenty-two stop being anonymous.
     *
     * Three groups, and which one is live depends on which of f1A99C[7] /
     * f1A99C[8] is set when 0x1005B910 runs:
     *
     *   +0x1A93C..+0x1A948  the style rectangle, copied verbatim.  Always.
     *   +0x1A94C..+0x1A968  the VERTICAL scrollbar boxes  (f1A99C[8] arm)
     *   +0x1A96C..+0x1A988  the HORIZONTAL scrollbar boxes (f1A99C[7] arm)
     *   +0x1A98C..+0x1A998  the truncated-to-int handle position.  Always.
     *
     * The pad SIZE is exact on any host because the region holds no pointer. */
    uint16_t   pad1A93A;           /* +0x1A93A  never read, never written    */

    int32_t    f1A93C;             /* +0x1A93C  = pStyle->left               */
    int32_t    f1A940;             /* +0x1A940  = pStyle->top                */
    int32_t    f1A944;             /* +0x1A944  = pStyle->right              */
    int32_t    f1A948;             /* +0x1A948  = pStyle->bottom             */

    int32_t    f1A94C;             /* +0x1A94C */
    int32_t    f1A950;             /* +0x1A950 */
    int32_t    f1A954;             /* +0x1A954 */
    int32_t    f1A958;             /* +0x1A958 */
    int32_t    f1A95C;             /* +0x1A95C */
    int32_t    f1A960;             /* +0x1A960 */
    int32_t    f1A964;             /* +0x1A964 */
    int32_t    f1A968;             /* +0x1A968 */

    int32_t    f1A96C;             /* +0x1A96C */
    int32_t    f1A970;             /* +0x1A970 */
    int32_t    f1A974;             /* +0x1A974 */
    int32_t    f1A978;             /* +0x1A978 */
    int32_t    f1A97C;             /* +0x1A97C */
    int32_t    f1A980;             /* +0x1A980 */
    int32_t    f1A984;             /* +0x1A984 */
    int32_t    f1A988;             /* +0x1A988 */

    int32_t    f1A98C;                         /* +0x1A98C */
    int32_t    f1A990;                         /* +0x1A990  (ctl +0x1E1C8) */
    int32_t    f1A994;                         /* +0x1A994 */
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

/* ---------------------------------------------------------------------
 * The two data tables 0x1005B910 needs.
 *
 * 0x100AB568 is a 24-byte-stride sprite table: `uint16 id` at +0x00, an
 * `int32 rect[4]` at +0x04 and one more int32 at +0x14.  The base and the
 * stride are pinned by the table itself -- record i's id word reads back i for
 * every record checked -- and by 0x1005C270, which indexes it as
 * `id  @ 0x100AB568 + 24*i`, `rect @ 0x100AB56C + 24*i`, `dw @ 0x100AB57C + 24*i`.
 *
 * 0x1005B910 does not index it.  It hard-codes six dwords out of two records:
 *
 *     0x100AB9C4 / 0x100AB9C8  ==  0x100AB56C + 46*24 + 8 and + 0xC
 *     0x100AB9EC .. 0x100AB9F8 ==  0x100AB56C + 48*24 + 0 .. + 0xC
 *
 * i.e. records 46 and 48, both of which are 16x16 in the image.  Those are the
 * scrollbar's two arrow sprites, and the code uses record 48's width and
 * height as the arrow size and record 46's right/bottom as an inset.
 *
 * Only the two rects are modelled, because only the two rects are read.
 *
 * ALIASING NOTE (see CONVENTIONS, "Aliased storage"): a later pass that models
 * the whole 0x100AB568 table must ALIAS these two rects into it, not define a
 * second copy.  Grep 0x100AB9BC and 0x100AB9EC before writing that pass. */
extern int32_t g_BrSprRect46[4];   /* 0x100AB9BC */
extern int32_t g_BrSprRect48[4];   /* 0x100AB9EC */

/* 0x100AB438 -- the UI style-rectangle pool, 19 entries of 16 bytes.
 *
 * Four modules (slice3_33, slice6_71, slice6_72, slice6_73) carry a context
 * field per entry -- p0AB438, p0AB448, p0AB4D8, p0AB538 and so on -- typed
 * `const void *` because none of them ever looked inside one.  Every such
 * address in the tree is `0x100AB438 + 16*i`, and the values behind them are
 * four plausible rectangles apiece: entry 0 is {0,0,639,479}, the screen.
 *
 * The UPPER bound is pinned: 0x100AB438 + 19*16 == 0x100AB568, exactly where
 * the sprite table above starts.
 *
 * The LOWER bound is NOT pinned, and this is the weakest claim in the block.
 * 0x100AB438 is only the lowest address that any module in this tree passes as
 * a style; the 16-byte grid plainly continues below it (0x100AB428 reads
 * {0,380,200,480}, 0x100AB418 {0,0,200,200}), and slice6_73.h names
 * 0x100AB428 and 0x100AB42C as two separate `fild`-ed scalars, which is what a
 * rectangle's first two members look like to a caller that only needs those
 * two.  So the pool very likely starts lower and the port has simply not
 * found the first entry.  Nothing here depends on it: the macro below is
 * address-based, so extending the array downward later moves the base and
 * leaves every call site correct.
 *
 * This module defines the storage because this module is the only one that
 * READS through the pointer; the others only pass it along.  A wiring layer
 * points its context fields at BR_UI_STYLE(0x...) rather than at NULL. */
#define BR_UI_STYLE_BASE  0x100AB438u
#define BR_UI_STYLE_COUNT 19
extern const BrTextStyle g_aBrUiStyle[BR_UI_STYLE_COUNT];   /* 0x100AB438 */

/* Index the pool by the ORIGINAL address, so a wiring site reads the way the
 * disassembly does: BR_UI_STYLE(0x100AB538) is entry 16. */
#define BR_UI_STYLE(addr) \
    (&g_aBrUiStyle[((unsigned)(addr) - BR_UI_STYLE_BASE) / 16u])

/* 0x1005B910 (thiscall, __stdcall, 5 stack args).  Configure the list:
 * copy the style rectangle in, set the flag word, the visible row count, the
 * scroll offset and f1A936, plant the three literals '0' / '.' / ':' that
 * f1A932 / f1A934 / f1A938 were initialised to -1 with, and then lay out ONE
 * scrollbar -- horizontal if f1A99C[7] is set, vertical if f1A99C[8] is,
 * neither if both are clear.  Always returns 1.
 *
 * `a2`, `a3` and `a4` are int32 in the caller and are stored as the LOW WORD
 * of int16 fields; `a1` is a full dword.
 *
 * GOTCHA (a real bug, reproduced): when f1A99C[7] and f1A99C[8] are BOTH zero
 * the function skips straight to its tail, which reads f1A99C[4] and
 * f1A99C[5] and truncates them to int.  Neither constructor writes those two,
 * so on that path it converts whatever `operator new` left behind.  The port
 * does the same rather than short-circuiting, because the tail also writes
 * three fields that a caller can observe. */
int32_t BrTextListConfig(BrTextList *pList, int32_t a1, const void *pStyle,
                         int32_t a2, int32_t a3, int32_t a4);

/* 0x1005BC10 (thiscall, __stdcall, 5 stack args).  Append one row.
 *
 * Returns 0 and does nothing when pText is NULL.  Otherwise it fills
 * aItems[count] -- text, flags, style-derived left/right, a y position of
 * `(int)f20 + 19*count`, and a measure through the item's OWN vtable (slot
 * +0x08 when a3 == 3, slot +0x04 otherwise, i.e. font B or font A) -- then
 * increments count and returns 1.
 *
 * `a5` selects how the text is stored:
 *     a5 != 0   strcpy(item.sz, pText)
 *     a5 == 0   strncpy(item.sz, pText, 10) then strcat(item.sz, <see below>)
 *
 * GOTCHA: `count` is only ever compared against 100 with an UNSIGNED compare,
 * and when it is at 100 the function calls vtable slot +0x2C and then forces
 * count to 99 -- so the hundredth row is overwritten again and again rather
 * than the list growing.  +0x2C is NOT ported, so that path faults.
 *
 * GOTCHA: the a5 == 0 path's strncpy(…, 10) does not NUL-terminate a source of
 * ten or more characters, and the strcat that follows then scans past it.  In
 * practice the constructor has zeroed the whole 0x400-byte buffer, so byte 10
 * is a NUL and the append lands there.  Preserved, not fixed.
 *
 * The block guarded by `f18 & 0x800000` -- a case-insensitive compare against
 * the shared edit buffer at 0x1039B720, a scroll-offset bump and a callback
 * through f0C -- is reproduced too, but no ported caller reaches it: every
 * one of them passes 0x40001 to BrTextListConfig. */
int32_t BrTextListAddRow(BrTextList *pList, const void *pText, int32_t a2,
                         int32_t a3, const void *pStyle, int32_t a5);

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

/* XSLICE 0x1007C8A0 -- __ftol.  Declared here, matching br_crt.h exactly,
 * rather than including br_crt.h: this header is pulled in by br_ui.h and the
 * fewer transitive includes it drags along the better. */
extern int32_t BrFtolTrunc(float f);

/* XSLICE 0x1039B720 -- the shared edit buffer.  slice2_25.h defines the
 * storage as `char g_aBr39B720[BR_OPT_TEXT_MAX]`; slice6_73.h already
 * re-declares it incomplete for the same reason, so this is the third
 * DECLARATION of one definition, not a fourth object. */
extern char g_aBr39B720[];

/* 0x10AA2A70, and it is not the game's.
 *
 * 0x1005BC10's a5 == 0 path ends with `strcat(item.sz, (char *)0x10AA2A70)`.
 * The only other references to that address in the whole image are three CRT
 * call sites at 0x100847E9, 0x100882D9 and 0x1008844E, each pushing it as the
 * one-byte SOURCE string of an LCMapStringA / MultiByteToWideChar probe -- so
 * it is MSVC's mbcs scratch byte, in .bss (past 0x100C1420, hence genuinely
 * zero at process start), and nothing writes it through that immediate.
 *
 * Appending it therefore appends an empty string.  The port models it as its
 * own zeroed byte buffer rather than pretending to share the CRT's, and says
 * so here because "the game strcats a CRT buffer" is a finding, not a typo:
 * if the CRT ever did leave a byte there, the original would splice it onto
 * every truncated row and the port would not. */
#define BR_AA2A70_SIZE 8u    /* 0x10AA2A78 is separately referenced */
extern char g_BrAA2A70[BR_AA2A70_SIZE];   /* 0x10AA2A70 */

/* =====================================================================
 * NOT PORTED from this packet -- see the pass report for why.
 *
 *   0x1005AE70 0x1005B250 0x1005B2B0 0x1005B390 0x1005B460 0x1005B570
 *   0x1005B730 0x1005B7A0 0x1005BB80 0x1005C000
 *   0x1005C270 0x1005C2C0 0x1005C510 0x1005C590 0x1005CB40 0x1005CBF0
 *   0x1005CC20 0x1005CCD0 0x1005CE30 0x10060060 0x10060260 0x100603A0
 *   0x10060750 0x100607B0
 *
 * 0x1005C270 and 0x1005C2C0 are vtable slots +0x04 and +0x08 and were looked
 * at in this pass.  They are the list's DRAW path and they were left out on
 * purpose:  0x1005C270 is a five-line wrapper whose only real content is a
 * call to the unported 0x1005F5A0, and 0x1005C2C0 (581 bytes) reaches
 * 0x100586D0 plus five more vtable slots.  Porting either would mostly add
 * cross-slice externs that resolve to NULL stubs, which is the same crash one
 * frame later and a lot more code to be wrong in.  No builder calls them --
 * the menu build path only reaches +0x10 and +0x14.
 * ===================================================================== */

#endif /* SLICE3_39_H */
