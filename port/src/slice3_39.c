/* slice3_39.c -- Boss Rally (BRD3D.dll) slice 3, a later pass.
 *
 * Packet 0x1005AE70 - 0x100607B0.  See slice3_39.h for the layout notes and
 * the list of functions that were deliberately left out.
 *
 * Every arithmetic width here is deliberate: the original accumulates string
 * widths in 16 bits and stores 16 bits, and the range tests on a character
 * are done on the SIGN-EXTENDED byte.  Both are reproduced literally.
 */

#include <string.h>

#include "slice1_07.h"   /* BrDevSlot -- see the note in slice3_39.h */
#include "slice3_39.h"

/* =====================================================================
 * Data tables, read out of BRD3D.dll's .data
 * ===================================================================== */

BrGlyphMetric g_BrGlyphFontA[BR_GLYPH_COUNT] = {
    { 65535, 65535, 65535, 0 },   /* 0x20 */
    {     8,    16,    42, 0 },   /* 0x21 */
    {     6,    16,    41, 0 },   /* 0x22 */
    {    16,    16,    44, 0 },   /* 0x23 */
    {     8,    16,    45, 0 },   /* 0x24 */
    {    13,    16,    46, 0 },   /* 0x25 */
    {    11,    16,    48, 0 },   /* 0x26 */
    {     5,    16,    40, 0 },   /* 0x27 */
    {     8,    16,    50, 0 },   /* 0x28 */
    {     8,    16,    51, 0 },   /* 0x29 */
    {     9,    16,    49, 0 },   /* 0x2A */
    {     8,    16,    52, 0 },   /* 0x2B */
    {     6,    16,    66, 0 },   /* 0x2C */
    {     7,    16,    39, 0 },   /* 0x2D */
    {     5,    16,    37, 0 },   /* 0x2E */
    {    11,    16,    62, 0 },   /* 0x2F */
    {    11,    16,    26, 0 },   /* 0x30 */
    {     7,    16,    27, 0 },   /* 0x31 */
    {    11,    16,    28, 0 },   /* 0x32 */
    {    10,    16,    29, 0 },   /* 0x33 */
    {    11,    16,    30, 0 },   /* 0x34 */
    {    11,    16,    31, 0 },   /* 0x35 */
    {    11,    16,    32, 0 },   /* 0x36 */
    {     9,    16,    33, 0 },   /* 0x37 */
    {    11,    16,    34, 0 },   /* 0x38 */
    {    11,    16,    35, 0 },   /* 0x39 */
    {     6,    16,    38, 0 },   /* 0x3A */
    {     6,    16,    58, 0 },   /* 0x3B */
    {     6,    16,    60, 0 },   /* 0x3C */
    {     9,    16,    65, 0 },   /* 0x3D */
    {     6,    16,    59, 0 },   /* 0x3E */
    {     8,    16,    61, 0 },   /* 0x3F */
    {    13,    16,    43, 0 },   /* 0x40 */
    {    11,    16,     0, 0 },   /* 0x41 */
    {    11,    16,     1, 0 },   /* 0x42 */
    {    10,    16,     2, 0 },   /* 0x43 */
    {    11,    16,     3, 0 },   /* 0x44 */
    {    10,    16,     4, 0 },   /* 0x45 */
    {    11,    16,     5, 0 },   /* 0x46 */
    {    10,    16,     6, 0 },   /* 0x47 */
    {    11,    16,     7, 0 },   /* 0x48 */
    {     6,    16,     8, 0 },   /* 0x49 */
    {     9,    16,     9, 0 },   /* 0x4A */
    {    12,    16,    10, 0 },   /* 0x4B */
    {     9,    16,    11, 0 },   /* 0x4C */
    {    13,    16,    12, 0 },   /* 0x4D */
    {    10,    16,    13, 0 },   /* 0x4E */
    {    10,    16,    14, 0 },   /* 0x4F */
    {    11,    16,    15, 0 },   /* 0x50 */
    {    10,    16,    16, 0 },   /* 0x51 */
    {    11,    16,    17, 0 },   /* 0x52 */
    {    10,    16,    18, 0 },   /* 0x53 */
    {     9,    16,    19, 0 },   /* 0x54 */
    {    10,    16,    20, 0 },   /* 0x55 */
    {    11,    16,    21, 0 },   /* 0x56 */
    {    13,    16,    22, 0 },   /* 0x57 */
    {    13,    16,    23, 0 },   /* 0x58 */
    {    11,    16,    24, 0 },   /* 0x59 */
    {    11,    16,    25, 0 },   /* 0x5A */
    {     9,    16,    56, 0 },   /* 0x5B */
    {    11,    16,    57, 0 },   /* 0x5C */
    {     8,    16,    55, 0 },   /* 0x5D */
    {     8,    16,    47, 0 },   /* 0x5E */
    {     8,    16,    36, 0 },   /* 0x5F */
    {     5,    16,    40, 0 },   /* 0x60 */
    {    11,    16,     0, 0 },   /* 0x61 */
    {    11,    16,     1, 0 },   /* 0x62 */
    {    10,    16,     2, 0 },   /* 0x63 */
    {    11,    16,     3, 0 },   /* 0x64 */
    {    10,    16,     4, 0 },   /* 0x65 */
    {    11,    16,     5, 0 },   /* 0x66 */
    {    10,    16,     6, 0 },   /* 0x67 */
    {    11,    16,     7, 0 },   /* 0x68 */
    {     6,    16,     8, 0 },   /* 0x69 */
    {     9,    16,     9, 0 },   /* 0x6A */
    {    12,    16,    10, 0 },   /* 0x6B */
    {     9,    16,    11, 0 },   /* 0x6C */
    {    13,    16,    12, 0 },   /* 0x6D */
    {    10,    16,    13, 0 },   /* 0x6E */
    {    10,    16,    14, 0 },   /* 0x6F */
    {    11,    16,    15, 0 },   /* 0x70 */
    {    10,    16,    16, 0 },   /* 0x71 */
    {    11,    16,    17, 0 },   /* 0x72 */
    {    10,    16,    18, 0 },   /* 0x73 */
    {     9,    16,    19, 0 },   /* 0x74 */
    {    10,    16,    20, 0 },   /* 0x75 */
    {    11,    16,    21, 0 },   /* 0x76 */
    {    13,    16,    22, 0 },   /* 0x77 */
    {    13,    16,    23, 0 },   /* 0x78 */
    {    11,    16,    24, 0 },   /* 0x79 */
    {    11,    16,    25, 0 },   /* 0x7A */
    {     7,    16,    54, 0 },   /* 0x7B */
    {     6,    16,    67, 0 },   /* 0x7C */
    {     7,    16,    53, 0 },   /* 0x7D */
    {     8,    16,    63, 0 },   /* 0x7E */
};

BrGlyphMetric g_BrGlyphFontB[BR_GLYPH_COUNT] = {
    { 65535, 65535, 65535, 0 },   /* 0x20 */
    { 65535, 65535, 65535, 0 },   /* 0x21 */
    { 65535, 65535, 65535, 0 },   /* 0x22 */
    { 65535, 65535, 65535, 0 },   /* 0x23 */
    { 65535, 65535, 65535, 0 },   /* 0x24 */
    { 65535, 65535, 65535, 0 },   /* 0x25 */
    { 65535, 65535, 65535, 0 },   /* 0x26 */
    { 65535, 65535, 65535, 0 },   /* 0x27 */
    { 65535, 65535, 65535, 0 },   /* 0x28 */
    { 65535, 65535, 65535, 0 },   /* 0x29 */
    { 65535, 65535, 65535, 0 },   /* 0x2A */
    { 65535, 65535, 65535, 0 },   /* 0x2B */
    { 65535, 65535, 65535, 0 },   /* 0x2C */
    { 65535, 65535, 65535, 0 },   /* 0x2D */
    { 65535, 65535, 65535, 0 },   /* 0x2E */
    { 65535, 65535, 65535, 0 },   /* 0x2F */
    {    40,    45,     0, 0 },   /* 0x30 */
    {    40,    45,     1, 0 },   /* 0x31 */
    {    40,    45,     2, 0 },   /* 0x32 */
    {    40,    45,     3, 0 },   /* 0x33 */
    {    40,    45,     4, 0 },   /* 0x34 */
    {    40,    45,     5, 0 },   /* 0x35 */
    {    40,    45,     6, 0 },   /* 0x36 */
    {    40,    45,     7, 0 },   /* 0x37 */
    {    40,    45,     8, 0 },   /* 0x38 */
    {    40,    45,     9, 0 },   /* 0x39 */
    { 65535, 65535, 65535, 0 },   /* 0x3A */
    { 65535, 65535, 65535, 0 },   /* 0x3B */
    { 65535, 65535, 65535, 0 },   /* 0x3C */
    { 65535, 65535, 65535, 0 },   /* 0x3D */
    { 65535, 65535, 65535, 0 },   /* 0x3E */
    { 65535, 65535, 65535, 0 },   /* 0x3F */
    { 65535, 65535, 65535, 0 },   /* 0x40 */
    { 65535, 65535, 65535, 0 },   /* 0x41 */
    { 65535, 65535, 65535, 0 },   /* 0x42 */
    { 65535, 65535, 65535, 0 },   /* 0x43 */
    { 65535, 65535, 65535, 0 },   /* 0x44 */
    { 65535, 65535, 65535, 0 },   /* 0x45 */
    { 65535, 65535, 65535, 0 },   /* 0x46 */
    { 65535, 65535, 65535, 0 },   /* 0x47 */
    { 65535, 65535, 65535, 0 },   /* 0x48 */
    { 65535, 65535, 65535, 0 },   /* 0x49 */
    { 65535, 65535, 65535, 0 },   /* 0x4A */
    { 65535, 65535, 65535, 0 },   /* 0x4B */
    { 65535, 65535, 65535, 0 },   /* 0x4C */
    { 65535, 65535, 65535, 0 },   /* 0x4D */
    { 65535, 65535, 65535, 0 },   /* 0x4E */
    { 65535, 65535, 65535, 0 },   /* 0x4F */
    { 65535, 65535, 65535, 0 },   /* 0x50 */
    { 65535, 65535, 65535, 0 },   /* 0x51 */
    { 65535, 65535, 65535, 0 },   /* 0x52 */
    { 65535, 65535, 65535, 0 },   /* 0x53 */
    { 65535, 65535, 65535, 0 },   /* 0x54 */
    { 65535, 65535, 65535, 0 },   /* 0x55 */
    { 65535, 65535, 65535, 0 },   /* 0x56 */
    { 65535, 65535, 65535, 0 },   /* 0x57 */
    { 65535, 65535, 65535, 0 },   /* 0x58 */
    { 65535, 65535, 65535, 0 },   /* 0x59 */
    { 65535, 65535, 65535, 0 },   /* 0x5A */
    { 65535, 65535, 65535, 0 },   /* 0x5B */
    { 65535, 65535, 65535, 0 },   /* 0x5C */
    { 65535, 65535, 65535, 0 },   /* 0x5D */
    { 65535, 65535, 65535, 0 },   /* 0x5E */
    { 65535, 65535, 65535, 0 },   /* 0x5F */
    {    32,     0,    33, 0 },   /* 0x60 */
    {    34,     0,    34, 0 },   /* 0x61 */
    {    35,     0,    36, 0 },   /* 0x62 */
    {    37,     0,    37, 0 },   /* 0x63 */
    {    38,     0,    39, 0 },   /* 0x64 */
    {    40,     0,    40, 0 },   /* 0x65 */
    {    41,     0,    42, 0 },   /* 0x66 */
    {    43,     0,    43, 0 },   /* 0x67 */
    {    44,     0,    45, 0 },   /* 0x68 */
    {    46,     0,    46, 0 },   /* 0x69 */
    {    47,     0,   186, 0 },   /* 0x6A */
    {   189,     0,    45, 0 },   /* 0x6B */
    {    46,     0,    48, 0 },   /* 0x6C */
    {    49,     0,    49, 0 },   /* 0x6D */
    {    50,     0,    51, 0 },   /* 0x6E */
    {    52,     0,    52, 0 },   /* 0x6F */
    {    53,     0,    54, 0 },   /* 0x70 */
    {    55,     0,    55, 0 },   /* 0x71 */
    {    56,     0,    57, 0 },   /* 0x72 */
    {    58,     0,    58, 0 },   /* 0x73 */
    {    59,     0,    60, 0 },   /* 0x74 */
    {    61,     0,    61, 0 },   /* 0x75 */
    {    62,     0,    63, 0 },   /* 0x76 */
    {    64,     0,    64, 0 },   /* 0x77 */
    {    65,     0,    66, 0 },   /* 0x78 */
    {    67,     0,    67, 0 },   /* 0x79 */
    {    68,     0,    69, 0 },   /* 0x7A */
    {    70,     0,    70, 0 },   /* 0x7B */
    {    71,     0,    72, 0 },   /* 0x7C */
    {    73,     0,    73, 0 },   /* 0x7D */
    {    74,     0,    75, 0 },   /* 0x7E */
};

BrCharMapEntry g_BrCharMap[BR_CHARMAP_COUNT] = {
    { 0x20, 0x20 },
    { 0x21, 0x21 },
    { 0x22, 0x22 },
    { 0x23, 0x23 },
    { 0x24, 0x24 },
    { 0x25, 0x25 },
    { 0x26, 0x26 },
    { 0x27, 0x27 },
    { 0x28, 0x28 },
    { 0x29, 0x29 },
    { 0x2A, 0x2A },
    { 0x2B, 0x2B },
    { 0x2C, 0x2C },
    { 0x2D, 0x2D },
    { 0x2E, 0x2E },
    { 0x2F, 0x2F },
    { 0xBA, 0x3A },
    { 0xBD, 0x2D },
    { 0xBE, 0x2E },
    { 0x30, 0x30 },
    { 0x31, 0x31 },
    { 0x32, 0x32 },
    { 0x33, 0x33 },
    { 0x34, 0x34 },
    { 0x35, 0x35 },
    { 0x36, 0x36 },
    { 0x37, 0x37 },
    { 0x38, 0x38 },
    { 0x39, 0x39 },
    { 0x3A, 0x3A },
    { 0x3B, 0x3B },
    { 0x3C, 0x3C },
    { 0x3D, 0x3D },
    { 0x3E, 0x3E },
    { 0x3F, 0x3F },
    { 0x40, 0x40 },
    { 0x41, 0x41 },
    { 0x42, 0x42 },
    { 0x43, 0x43 },
    { 0x44, 0x44 },
    { 0x45, 0x45 },
    { 0x46, 0x46 },
    { 0x47, 0x47 },
    { 0x48, 0x48 },
    { 0x49, 0x49 },
    { 0x4A, 0x4A },
    { 0x4B, 0x4B },
    { 0x4C, 0x4C },
    { 0x4D, 0x4D },
    { 0x4E, 0x4E },
    { 0x4F, 0x4F },
    { 0x50, 0x50 },
    { 0x51, 0x51 },
    { 0x52, 0x52 },
    { 0x53, 0x53 },
    { 0x54, 0x54 },
    { 0x55, 0x55 },
    { 0x56, 0x56 },
    { 0x57, 0x57 },
    { 0x58, 0x58 },
    { 0x59, 0x59 },
    { 0x5A, 0x5A },
    { 0x5B, 0x5B },
    { 0x5C, 0x5C },
    { 0x5D, 0x5D },
    { 0x5E, 0x5E },
    { 0x5F, 0x5F },
    { 0x60, 0x60 },
    { 0x61, 0x61 },
    { 0x62, 0x62 },
    { 0x63, 0x63 },
    { 0x64, 0x64 },
    { 0x65, 0x65 },
    { 0x66, 0x66 },
    { 0x67, 0x67 },
    { 0x68, 0x68 },
    { 0x69, 0x69 },
    { 0x6A, 0x6A },
    { 0x6B, 0x6B },
    { 0x6C, 0x6C },
    { 0x6D, 0x6D },
    { 0x6E, 0x6E },
    { 0x6F, 0x6F },
    { 0x70, 0x70 },
    { 0x71, 0x71 },
    { 0x72, 0x72 },
    { 0x73, 0x73 },
    { 0x74, 0x74 },
    { 0x75, 0x75 },
    { 0x76, 0x76 },
    { 0x77, 0x77 },
    { 0x78, 0x78 },
    { 0x79, 0x79 },
    { 0x7A, 0x7A },
    { 0x7B, 0x7B },
    { 0x7C, 0x7C },
    { 0x7D, 0x7D },
    { 0x7E, 0x7E },
};

/* 0x100AB438 -- the 19-entry UI style-rectangle pool, read out of the image.
 * See the derivation of the base and the extent in slice3_39.h -- and the
 * note there that the LOWER bound is the one claim not pinned by anything. */
const BrTextStyle g_aBrUiStyle[BR_UI_STYLE_COUNT] = {
    {   0,   0, 639, 479 },   /*  0  0x100AB438  the screen */
    { 148, 110, 358, 260 },   /*  1  0x100AB448 */
    {  87,  61, 186, 132 },   /*  2  0x100AB458 */
    { 330,  70, 447,  86 },   /*  3  0x100AB468 */
    { 478,   0, 578,   0 },   /*  4  0x100AB478 */
    { 440,   0, 540,   0 },   /*  5  0x100AB488 */
    {  88,   0, 185,   0 },   /*  6  0x100AB498 */
    { 330,   0, 447,   0 },   /*  7  0x100AB4A8 */
    {  67,   0, 167,   0 },   /*  8  0x100AB4B8 */
    { 230,   0, 547,   0 },   /*  9  0x100AB4C8 */
    { 188, 130, 300, 225 },   /* 10  0x100AB4D8 */
    { 188, 130, 300, 197 },   /* 11  0x100AB4E8 */
    { 440, 128, 540, 204 },   /* 12  0x100AB4F8 */
    { 100,  10, 410,  29 },   /* 13  0x100AB508 */
    {  70, 213, 165, 285 },   /* 14  0x100AB518 */
    { 128,  76, 384, 209 },   /* 15  0x100AB528 */
    { 188, 130, 300, 206 },   /* 16  0x100AB538 */
    { 162, 130, 318, 206 },   /* 17  0x100AB548 */
    {  80,  29, 430,  48 },   /* 18  0x100AB558 */
};

/* Records 46 and 48 of the sprite table at 0x100AB568, both 16x16 in the
 * image.  Not const: they are ordinary .data in the original and this port
 * has not established that nothing writes them. */
int32_t g_BrSprRect46[4] = { 0, 0, 16, 16 };   /* 0x100AB9BC */
int32_t g_BrSprRect48[4] = { 0, 0, 16, 16 };   /* 0x100AB9EC */

/* =====================================================================
 * Globals
 * ===================================================================== */

/* 0x10AA2A70 -- see the note in slice3_39.h.  Zero, and it is zero in the
 * original too. */
char g_BrAA2A70[BR_AA2A70_SIZE];

const BrTextBoxVtbl  *g_pBrTextBoxVtbl  = NULL;   /* 0x1008F728 */
const BrTextListVtbl *g_pBrTextListVtbl = NULL;   /* 0x1008F758 */

uint8_t g_BrDikState[BR_DIK_COUNT];   /* 0x10AA3288 */
int32_t g_BrDikPrev [BR_DIK_COUNT];   /* 0x10AA2E88 */
int32_t g_BrDikEdge [BR_DIK_COUNT];   /* 0x10AA2A80 */

int32_t g_BrBtnRaw  [BR_BTN_COUNT];   /* 0x10AA33C0 */
int32_t g_BrBtnPrev [BR_BTN_COUNT];   /* 0x10AA3388 */
int32_t g_BrBtnEdge [BR_BTN_COUNT];   /* 0x10AA33D0 */

int32_t   g_Br0A81C0;                 /* 0x100A81C0 */
int32_t   g_Br0A81C4;                 /* 0x100A81C4 */
int32_t   g_BrAA33B8;                 /* 0x10AA33B8 */
int32_t   g_BrAA33B4;                 /* 0x10AA33B4 */
BrPointI *g_pBrAA2E80 = NULL;         /* 0x10AA2E80 */
int32_t   g_BrAA3398[7];              /* 0x10AA3398 */

/* =====================================================================
 * 0x1005B050 -- BrTextBox constructor (thiscall)
 * ===================================================================== */

BrTextBox *BrTextBoxInit(BrTextBox *pBox)
{
    /* The vtable store comes FIRST, before the buffer clear -- the clear
     * starts at this+9 and so never touches it either way. */
    pBox->pVtbl = g_pBrTextBoxVtbl;

    memset(pBox->sz, 0, sizeof pBox->sz);

    pBox->f418  = 0;
    pBox->y     = 0.0f;
    pBox->x     = 0.0f;
    pBox->height = 0;
    pBox->width  = 0;
    pBox->f41C  = 0;
    pBox->f420  = 0;
    pBox->f04   = 0;
    pBox->f08   = 1;

    /* left / f428 / right / f430 / f434 are deliberately NOT touched. */
    return pBox;
}

/* =====================================================================
 * 0x1005B0A0 -- scalar deleting destructor
 * ===================================================================== */

BrTextBox *BrTextBoxDeleteDtor(BrTextBox *pBox, uint32_t flags)
{
    BrTextBoxDtor(pBox);
    if (flags & 1u) {
        BrOperatorDelete(pBox);
    }
    /* Returns the (possibly freed) pointer, exactly as the original does. */
    return pBox;
}

/* =====================================================================
 * 0x1005B0D0 / 0x1005B160 -- measure sz[]
 * ===================================================================== */

/* The shared prologue of both measurers: decide what kind of character this
 * is.  Returns 1 for "in the glyph range", 0 for "space-or-nothing", -1 for
 * "stop the walk entirely". */
static int BrGlyphClassify(char c)
{
    /* movsx ax, dl ; sub eax, 0x20 ; test ax,ax / cmp ax,0x7f  -- a SIGNED
     * 16-bit test on the sign-extended byte, so 0x80..0xFF land negative. */
    int16_t k = (int16_t)((int16_t)(signed char)c - 0x20);

    if (k < 0 || k > 0x7F) {
        if (c != 0x20) {
            return -1;      /* < 0x20 or >= 0x80: stop measuring here */
        }
    }
    /* cmp dl,0x21 / jl ; cmp dl,0x7e / jg  -- signed byte compares */
    if ((signed char)c < 0x21 || (signed char)c > 0x7E) {
        return 0;
    }
    return 1;
}

void BrTextBoxMeasureA(BrTextBox *pBox)
{
    uint16_t width = 0;
    /* Seeded from the field, never reset. */
    uint16_t maxH  = (uint16_t)pBox->height;
    int      i     = 0;
    char     c     = pBox->sz[0];

    while (c != '\0') {
        int cls = BrGlyphClassify(c);
        int hit = 0;

        if (cls < 0) {
            break;
        }
        if (cls > 0) {
            const BrGlyphMetric *pG = &g_BrGlyphFontA[(unsigned char)c - BR_GLYPH_FIRST];

            if (pG->advance != BR_GLYPH_NONE && pG->height != BR_GLYPH_NONE) {
                width = (uint16_t)(width + pG->advance);
                if ((int16_t)maxH < (int16_t)pG->height) {
                    maxH = pG->height;
                }
                hit = 1;
            }
        }
        if (!hit && c == 0x20) {
            width = (uint16_t)(width + BR_GLYPH_SPACE_ADVANCE);
        }

        ++i;
        /* movsx eax, di -- the index is truncated to 16 bits and
         * sign-extended.  Harmless for a 0x400-byte buffer. */
        c = pBox->sz[(int16_t)i];
    }

    pBox->height = (int16_t)maxH;
    pBox->width  = (int16_t)width;
}

void BrTextBoxMeasureB(BrTextBox *pBox)
{
    uint16_t width = 0;
    uint16_t maxH  = (uint16_t)pBox->height;
    int      i     = 0;
    char     c     = pBox->sz[0];

    while (c != '\0') {
        int cls = BrGlyphClassify(c);
        int hit = 0;

        if (cls < 0) {
            break;
        }
        if (cls > 0) {
            unsigned idx = (unsigned)((unsigned char)c - BR_GLYPH_FIRST);
            /* GOTCHA: the sentinel test reads FONT A, the metrics read
             * FONT B.  Not a transcription slip -- see the header. */
            const BrGlyphMetric *pGate = &g_BrGlyphFontA[idx];
            const BrGlyphMetric *pG    = &g_BrGlyphFontB[idx];

            if (pGate->advance != BR_GLYPH_NONE && pGate->height != BR_GLYPH_NONE) {
                width = (uint16_t)(width + (uint16_t)(pG->advance - 4u));
                if ((int16_t)maxH < (int16_t)pG->height) {
                    maxH = pG->height;
                }
                hit = 1;
            }
        }
        if (!hit && c == 0x20) {
            width = (uint16_t)(width + BR_GLYPH_SPACE_ADVANCE);
        }

        ++i;
        c = pBox->sz[(int16_t)i];
    }

    pBox->height = (int16_t)maxH;
    pBox->width  = (int16_t)width;
}

/* =====================================================================
 * 0x1005B200 -- centre horizontally
 * ===================================================================== */

float BrTextBoxCentreX(BrTextBox *pBox)
{
    float fLeft  = (float)pBox->left;
    float fWidth = (float)(int32_t)pBox->width;          /* movsx from +0x40A */
    float fSpan  = (float)(pBox->right - pBox->left);    /* the sub is 32-bit */
    float v;

    /* 0x1008F678 == 0.5f */
    v = fLeft + 0.5f * (fSpan - fWidth);

    pBox->x = v;
    return v;
}

/* =====================================================================
 * 0x1005B540 -- character map lookup
 * ===================================================================== */

uint8_t BrCharMapLookup(int32_t code)
{
    int i;

    for (i = 0; i < BR_CHARMAP_COUNT; ++i) {
        if (g_BrCharMap[i].code == (uint32_t)code) {
            return (uint8_t)g_BrCharMap[i].ch;
        }
    }
    return 0;
}

/* =====================================================================
 * 0x1005B7F0 / 0x1005B8D0 / 0x1005B8F0 -- BrTextList lifetime
 * ===================================================================== */

BrTextList *BrTextListInit(BrTextList *pList)
{
    int i;

    pList->f18   = 0;
    pList->f1C.u = 0;
    pList->f20.u = 0;

    /* 0x1007F680 is MSVC's vector constructor iterator: forward order. */
    for (i = 0; i < BR_TEXTLIST_ITEMS; ++i) {
        BrTextBoxInit(&pList->aItems[i]);
    }

    pList->count   = 0;
    pList->f1A92E  = 0;
    pList->f1A930  = 0;
    pList->f1A932  = -1;
    pList->f1A934  = -1;
    pList->f1A936  = -1;
    pList->f1A938  = -1;

    for (i = 0; i < 14; ++i) {
        pList->f1A99C[i].u = 0;
    }

    pList->f04 = 0;
    pList->f08 = 0;
    pList->f0C = 0;
    pList->f10 = 0;
    pList->f14 = 0;

    memset(pList->aBlobs, 0, sizeof pList->aBlobs);

    /* The vtable really is planted last. */
    pList->pVtbl = g_pBrTextListVtbl;
    return pList;
}

void BrTextListDtor(BrTextList *pList)
{
    int i;

    pList->pVtbl = g_pBrTextListVtbl;

    /* 0x1007F560 is MSVC's vector destructor iterator: last to first. */
    for (i = BR_TEXTLIST_ITEMS - 1; i >= 0; --i) {
        BrTextBoxDtor(&pList->aItems[i]);
    }
}

BrTextList *BrTextListDeleteDtor(BrTextList *pList, uint32_t flags)
{
    BrTextListDtor(pList);
    if (flags & 1u) {
        BrOperatorDelete(pList);
    }
    return pList;
}

/* =====================================================================
 * 0x1005B910 -- vtable +0x14, "configure the list"
 *
 * Both crashing menu builders (0x1004F700 in slice6_71.c and 0x1005A6E0 in
 * slice6_72.c) die on this slot, and both reach the f1A99C[8] arm because they
 * set f1A99C[8].i = 1 immediately before the call.
 *
 * The float constant at 0x1008F6B0 is -1.0f, read out of the image rather than
 * assumed -- and it matters, because `fsub` against it is an ADD of one.  With
 * it, branch B's f1A9B0 and f1A9C8 come out equal, i.e. the handle starts at
 * the top of its travel; with the sign the other way it would start two pixels
 * ABOVE its own minimum, which is the kind of wrong that renders and never
 * asserts.
 * ===================================================================== */

/* 0x1008F6B0. */
#define BR_TEXTLIST_K (-1.0f)

int32_t BrTextListConfig(BrTextList *pList, int32_t a1, const void *pStyle,
                         int32_t a2, int32_t a3, int32_t a4)
{
    const int32_t *pRect = (const int32_t *)pStyle;
    int32_t dx, dy;

    /* +0x1C / +0x20 take the first two rectangle members as FLOATS, and the
     * four dwords at +0x1A93C take all four verbatim. */
    pList->f1C.f = (float)pRect[0];
    pList->f20.f = (float)pRect[1];

    pList->f1A93C = pRect[0];
    pList->f1A940 = pRect[1];
    pList->f1A944 = pRect[2];
    pList->f1A948 = pRect[3];

    pList->f18 = (uint32_t)a1;

    /* Three int32 arguments, three int16 fields: the original moves them a
     * WORD at a time (`mov dx, word ptr [esp+0x1C]`), so the high half is
     * discarded rather than saturated.  a4 == -1 at every ported call site. */
    pList->f1A930 = (int16_t)a2;
    pList->f1A92E = (int16_t)a3;
    pList->f1A936 = (int16_t)a4;

    /* The three sentinels BrTextListInit left at -1 get their real values
     * here: '0', '.' and ':'. */
    pList->f1A932 = 0x30;
    pList->f1A934 = 0x2E;
    pList->f1A938 = 0x3A;

    /* The arrow sprite's size.  Clamped up to zero, and only afterwards --
     * `test edi,edi / jge / xor edi,edi` -- so a negative rectangle yields 0,
     * not an absolute value. */
    dx = g_BrSprRect48[2] - g_BrSprRect48[0];
    dy = g_BrSprRect48[3] - g_BrSprRect48[1];
    if (dx < 0) { dx = 0; }
    if (dy < 0) { dy = 0; }

    if (pList->f1A99C[7].u != 0) {
        /* Branch A -- the HORIZONTAL bar.  Runs along the rectangle's top
         * edge; the travel is measured on x. */
        int32_t x0 = pRect[0] + dx;             /* left edge of the travel  */
        int32_t yb = pRect[3] + 3;              /* the bar's own top        */
        int32_t xr = pRect[2] - dx;             /* right edge of the travel */
        float   fLo;

        pList->f1A96C = pRect[0];
        pList->f1A970 = yb;
        pList->f1A974 = x0;
        pList->f1A978 = yb + dy;
        pList->f1A97C = xr;
        pList->f1A980 = yb;
        pList->f1A984 = dx + xr;                /* == pRect[2]              */
        pList->f1A988 = dy + yb;

        fLo = (float)x0 - BR_TEXTLIST_K;
        pList->f1A99C[4].f = fLo;
        pList->f1A99C[5].f = (float)yb;
        /* `fld st(0) / fsub st(2)` -- the travel LENGTH, high minus low. */
        pList->f1A99C[13].f = (float)(xr - dx) - fLo;
        pList->f1A99C[9].f  = (float)(x0 + 1);
        pList->f1A99C[10].f = (float)(xr - dx);
    } else if (pList->f1A99C[8].u != 0) {
        /* Branch B -- the VERTICAL bar.  Runs down the rectangle's right
         * edge; the travel is measured on y.  This is the arm both builders
         * take.
         *
         * Note the asymmetry with branch A, which is in the original: A reads
         * the arrow size out of record 48 for BOTH axes, B mixes record 48's
         * right/bottom (as absolute coordinates, not as a size) into the box
         * corners and record 46's right/bottom into the travel.  The two arms
         * were plainly written at different times. */
        int32_t xb = pRect[2] + 3;              /* the bar's own left       */
        int32_t yt = pRect[1];                  /* its top                  */
        int32_t y0 = g_BrSprRect48[3] + yt;     /* top of the travel        */
        int32_t yh = pRect[3] - g_BrSprRect46[3];   /* bottom of the travel */
        float   fLo;

        pList->f1A94C = xb;
        pList->f1A950 = yt;
        pList->f1A954 = g_BrSprRect48[2] + xb;
        pList->f1A958 = y0;
        pList->f1A95C = xb;
        pList->f1A960 = yh;
        pList->f1A964 = g_BrSprRect46[2] + xb;
        pList->f1A968 = pRect[3];

        pList->f1A99C[4].f = (float)xb;         /* no -1 on this arm        */

        fLo = (float)y0 - BR_TEXTLIST_K;
        pList->f1A99C[5].f  = fLo;
        pList->f1A99C[13].f = (float)(yh - dy) - fLo;
        pList->f1A99C[11].f = (float)(y0 + 1);
        pList->f1A99C[12].f = (float)(yh - dy);
    }
    /* else: neither arm.  The tail below still runs, and still reads
     * f1A99C[4] and f1A99C[5] -- see the GOTCHA in slice3_39.h. */

    pList->f1A98C = BrFtolTrunc(pList->f1A99C[4].f);
    pList->f1A990 = BrFtolTrunc(pList->f1A99C[5].f);
    pList->f1A994 = dx + pList->f1A98C;
    pList->f1A998 = dy + pList->f1A990;

    return 1;
}

/* =====================================================================
 * 0x1005BC10 -- vtable +0x10, "append one row"
 * ===================================================================== */

/* 0x1008C320's single-byte-locale arm, which is the arm this build takes.
 *
 * Written out rather than reached for as strcasecmp() because the original's
 * fold is ASCII-only by construction -- `sub al,0x41 / cmp al,0x1A / sbb cl,cl
 * / and cl,0x20` folds exactly 'A'..'Z' and leaves every other byte, including
 * every byte above 0x7F, untouched.  strcasecmp() is locale-sensitive, and the
 * one call site only asks whether the answer is zero, so matching the fold
 * exactly costs nothing and cannot drift with the host's locale. */
static int BrStrICmpAscii(const char *s1, const char *s2)
{
    unsigned char a, b;

    for (;;) {
        b = (unsigned char)*s2++;
        a = (unsigned char)*s1++;
        if (a == b) {
            if (b == 0) {
                return 0;      /* `or al,al / je` -- s2's NUL ends the walk */
            }
            continue;
        }
        if (a - 0x41u < 0x1Au) { a = (unsigned char)(a + 0x20u); }
        if (b - 0x41u < 0x1Au) { b = (unsigned char)(b + 0x20u); }
        if (a != b) {
            return (a < b) ? -1 : 1;
        }
    }
}

int32_t BrTextListAddRow(BrTextList *pList, const void *pText, int32_t a2,
                         int32_t a3, const void *pStyle, int32_t a5)
{
    const int32_t *pRect = (const int32_t *)pStyle;
    BrTextBox     *pItem;
    uint16_t       iRow;

    if (pText == NULL) {
        return 0;
    }

    /* `cmp word ptr [ebp+0x1A92C], 0x64 / jb` -- an UNSIGNED compare, so a
     * count that has somehow gone negative reads as huge and takes this arm.
     * +0x2C is not ported; a NULL there faults, which is the intent. */
    if ((uint16_t)pList->count >= (uint16_t)BR_TEXTLIST_ITEMS) {
        pList->pVtbl->f2C(pList, 0);
        pList->count = (int16_t)(BR_TEXTLIST_ITEMS - 1);
    }

    iRow  = (uint16_t)pList->count;
    pItem = &pList->aItems[iRow];

    if (a5 != 0) {
        strcpy(pItem->sz, (const char *)pText);
    } else {
        /* GOTCHA: no NUL is guaranteed by strncpy here -- see slice3_39.h. */
        strncpy(pItem->sz, (const char *)pText, 10);
        strcat(pItem->sz, g_BrAA2A70);
    }

    pItem->f04  |= (uint32_t)a2;
    pItem->f08   = (uint8_t)a3;
    pItem->f41C  = 0;
    pItem->height = 0;
    pItem->width  = 0;

    pItem->left  = pRect[0];
    pItem->right = pRect[2];

    /* The row pitch is 19, and the origin is the list's own +0x20 -- the
     * float BrTextListConfig put there, truncated toward zero. */
    pItem->f428 = BrFtolTrunc(pList->f20.f) + 19 * (int32_t)iRow;
    pItem->f430 = pItem->f428 + 0x12;

    pItem->x = (float)pRect[0];
    pItem->y = (float)pItem->f428;

    pItem->f418 = 0;
    pItem->f420 = 0;

    /* Measure through the ITEM's vtable, not the list's: slot +0x08 is font B
     * (0x1005B160), slot +0x04 is font A (0x1005B0D0). */
    if ((uint8_t)a3 == 3) {
        pItem->pVtbl->pfn08(pItem);
    } else {
        pItem->pVtbl->pfn04(pItem);
    }

    /* `mov cx,[...+0x458] / sub cx,[...+0x450] / sub ecx,0x10 / mov [..],cx`
     * -- 16-bit throughout on the store, so a rectangle wider than 32 KiB
     * wraps. */
    pItem->f41C = (int16_t)((int16_t)pItem->right - (int16_t)pItem->left - 0x10);

    pList->count++;

    if ((pList->f18 & 0x800000u) != 0) {
        int16_t iSel;
        int32_t nDen;
        float   v;

        /* Which row to compare against: the first one past the visible window
         * unless that is off the end of the array, in which case the last row
         * appended.  Both arms narrow to 16 bits before the sign extension. */
        {
            int16_t k = (int16_t)(pList->f1A930 + pList->f1A92E);
            iSel = (k < 100) ? k : (int16_t)(pList->count - 1);
        }

        /* 0x1008C320 is the CRT's _stricmp.  A row that MATCHES the shared
         * edit buffer stops the whole update and returns 0. */
        if (BrStrICmpAscii(pList->aItems[iSel].sz, g_aBr39B720) == 0) {
            return 0;
        }

        pList->f1A92E++;
        if ((int32_t)pList->f1A92E >= (int32_t)(uint16_t)pList->count) {
            pList->f1A92E = (int16_t)((uint16_t)pList->count - 1);
        }

        if (pList->f0C != NULL) {
            pList->f0C();          /* `call eax` -- no arguments, no result */
        }

        /* The denominator is count - 1 in 16 bits, forced up to 1 when that
         * is zero.  It is NOT forced up when count is 0, because 0 - 1 is
         * 0xFFFF there, which is not zero -- so a single-row list divides by
         * 1 and an empty one divides by 65535. */
        nDen = (uint16_t)(pList->count - 1);
        if (nDen == 0) {
            nDen = 1;
        }

        /* `fdivr` -- the field is the NUMERATOR. */
        v = pList->f1A99C[13].f / (float)nDen + pList->f1A99C[5].f;
        pList->f1A99C[5].f = v;

        /* fcom / test ah,1 tests C0 alone, which is set for UNORDERED as well
         * as for less-than, so a NaN takes the clamp-to-low arm. */
        if (!(v >= pList->f1A99C[11].f)) {
            pList->f1A99C[5] = pList->f1A99C[11];
        } else if (!(v <= pList->f1A99C[12].f)) {
            pList->f1A99C[5] = pList->f1A99C[12];
        }

        pList->f1A990 = BrFtolTrunc(pList->f1A99C[5].f);
        pList->f1A998 = pList->f1A990 + 0x10;
    }

    return 1;
}

/* =====================================================================
 * 0x1005C200 -- store an opaque blob against an item slot
 * ===================================================================== */

int32_t BrTextListSetBlob(BrTextList *pList, const void *pSrc,
                          uint32_t size, int32_t index)
{
    BrTextBlob *pSlot;

    if (index == -1) {
        index = (int32_t)(uint16_t)pList->count - 1;
        if (index < 0) {
            index = 0;
        }
    }

    pSlot = &pList->aBlobs[index];

    if (pSlot->p == NULL) {
        pSlot->p = BrOperatorNew(size);
    }
    /* GOTCHA: no realloc on a size increase -- the original copies `size`
     * bytes into whatever the first call allocated. */
    memcpy(pSlot->p, pSrc, size);
    pSlot->size = size;

    return 1;
}

/* =====================================================================
 * 0x1005FF60 / 0x1005FFB0 / 0x1005FFD0 / 0x1005FFF0 -- input edges
 * ===================================================================== */

void BrMenuSub1005FF60(void)
{
    int i;

    for (i = 0; i < BR_DIK_COUNT; ++i) {
        int32_t notPrev = (g_BrDikPrev[i] == 0) ? 1 : 0;
        int32_t down    = (int32_t)((g_BrDikState[i] >> 7) & 1u);

        g_BrDikPrev[i] = down;
        g_BrDikEdge[i] = notPrev & down;
    }
}

void BrMenuSub1005FFF0(void)
{
    int i;

    for (i = 0; i < BR_BTN_COUNT; ++i) {
        int32_t notPrev = (g_BrBtnPrev[i] == 0) ? 1 : 0;
        int32_t raw     = g_BrBtnRaw[i];

        g_BrBtnPrev[i] = raw;
        /* A bitwise AND with the RAW dword, not with a 0/1 flag. */
        g_BrBtnEdge[i] = notPrev & raw;
    }
}

int32_t BrFn1005FFD0(void)
{
    int32_t i;

    for (i = 0; i < BR_DIK_COUNT; ++i) {
        if (g_BrDikEdge[i] != 0) {
            return i;
        }
    }
    return -1;
}

void BrDikPollAndEdge(void)
{
    if (BrDikGetDeviceState(g_BrDikState) >= 0) {
        BrMenuSub1005FF60();
    }
}

/* =====================================================================
 * 0x10060210 / 0x100602B0 / 0x10060780 -- small utilities
 * ===================================================================== */

int32_t BrFn10060210(void *pUnused)
{
    int i;

    (void)pUnused;   /* never read by the original */

    g_BrAA33B8 = g_Br0A81C0;
    g_pBrAA2E80->x = g_Br0A81C0 / 2;   /* cdq/sub/sar: toward zero */

    g_BrAA33B4 = g_Br0A81C4;
    g_pBrAA2E80->y = g_Br0A81C4 / 2;

    for (i = 0; i < 7; ++i) {
        g_BrAA3398[i] = 0;
    }
    return 1;
}

void BrDevSlotReleaseIface(struct BrDevSlot *pSlot)
{
    BrDevIface *pIface = (BrDevIface *)pSlot->pIface;

    if (pIface != NULL) {
        pIface->pVtbl->pfn20(pIface);
        /* Re-read: the original loads this+0x50 again for the second call. */
        pIface = (BrDevIface *)pSlot->pIface;
        pIface->pVtbl->pfn08(pIface);
        pSlot->pIface = NULL;
    }
}

void BrMemFill(void *pDst, uint32_t count, int32_t value)
{
    /* DEVIATION: the original spells this as `rep stosd` of a dword built
     * from four copies of the byte followed by `rep stosb` for the tail.
     * memset is the same store sequence, without the aliasing hazard. */
    memset(pDst, (int)(uint8_t)value, (size_t)count);
}
