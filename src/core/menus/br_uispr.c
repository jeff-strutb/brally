/* br_uispr.c -- see br_uispr.h for the derivation.  Three parts:
 *
 *   1. the 145-entry sprite table, read out of the image
 *   2. the file names, recovered from the loader at 0x10056260
 *   3. 0x10048010 / 0x10047A10 / 0x10047930 / 0x10047980 / 0x10001320's clip,
 *      transcribed as one query: "what does this control draw?"
 */
#include "br_uispr.h"

#include <stddef.h>

#include "br_crt.h"     /* BrFtolTrunc -- 0x1007C8A0, truncate toward zero */

/* ======================================================================
 * PART 1 -- 0x100AB568 (D3D) / 0x100AAD08 (Glide), 145 * 24 bytes
 *
 * The count is pinned at both ends.  Below: 0x100AB418 + 21*16 == 0x100AB568,
 * so the style pool ends exactly where this begins.  Above:
 * 0x100AB568 + 145*24 == 0x100ABAA0, and the dwords there are 1, 1, 0x77,
 * 0x78, 0x79, 0x7A ... -- a plainly different array, not a 146th entry whose
 * id happens to be 1.  Every entry from 0 to 144 has iImage == its own index,
 * left == 0 and top == 0; the 146th candidate has none of those.
 *
 * That every rect starts at (0,0) is worth stating rather than glossing: the
 * "source rectangle" is always the WHOLE image, so the rect's right/bottom
 * are the bitmap's width and height.  The mechanism supports sub-rectangles
 * and the shipped data never uses one.
 * ====================================================================== */

const BrUiSprite g_aBrUiSprite[BR_UI_SPR_COUNT] = {
    {   0, {   0,   0, 640, 480}, 0 },  /*   0 work1a.bmp     640x480 */
    {   1, {   0,   0,  28,  30}, 1 },  /*   1 cursor.bmp      28x30  keyed */
    {   2, {   0,   0, 128, 144}, 1 },  /*   2 type_gry.bmp   128x144 keyed */
    {   3, {   0,   0, 128, 144}, 1 },  /*   3 type_wit.bmp   128x144 keyed */
    {   4, {   0,   0, 128, 144}, 1 },  /*   4 type_mid.bmp   128x144 keyed */
    {   5, {   0,   0, 204,  93}, 0 },  /*   5 bignums.bmp    204x93  */
    {   6, {   0,   0, 101, 109}, 0 },  /*   6 champ.bmp      101x109 */
    {   7, {   0,   0, 101, 109}, 0 },  /*   7 mp.bmp         101x109 */
    {   8, {   0,   0, 101, 109}, 0 },  /*   8 ta.bmp         101x109 */
    {   9, {   0,   0, 101, 109}, 0 },  /*   9 op.bmp         101x109 */
    {  10, {   0,   0, 101, 109}, 0 },  /*  10 qr.bmp         101x109 */
    {  11, {   0,   0,  97,  64}, 0 },  /*  11 carmt.bmp       97x64  */
    {  12, {   0,   0,  53,  59}, 0 },  /*  12 mshft.bmp       53x59  */
    {  13, {   0,   0,  28,  49}, 0 },  /*  13 softshok.bmp    28x49  */
    {  14, {   0,   0,  28,  49}, 0 },  /*  14 medshok.bmp     28x49  */
    {  15, {   0,   0,  28,  49}, 0 },  /*  15 hardshok.bmp    28x49  */
    {  16, {   0,   0, 132,  77}, 0 },  /*  16 (unnamed)      132x77  */
    {  17, {   0,   0, 132,  77}, 0 },  /*  17 coasttrk.bmp   132x77  */
    {  18, {   0,   0, 132,  77}, 0 },  /*  18 trakc.bmp      132x77  */
    {  19, {   0,   0,  84,  59}, 0 },  /*  19 fog.bmp         84x59  */
    {  20, {   0,   0,  84,  59}, 0 },  /*  20 nite.bmp        84x59  */
    {  21, {   0,   0,  84,  59}, 0 },  /*  21 rain.bmp        84x59  */
    {  22, {   0,   0,  84,  59}, 0 },  /*  22 snow.bmp        84x59  */
    {  23, {   0,   0,  84,  59}, 0 },  /*  23 sunweth.bmp     84x59  */
    {  24, {   0,   0,  46,  54}, 0 },  /*  24 drytire.bmp     46x54  */
    {  25, {   0,   0,  46,  54}, 0 },  /*  25 medtire.bmp     46x54  */
    {  26, {   0,   0,  46,  54}, 0 },  /*  26 wettire.bmp     46x54  */
    {  27, {   0,   0, 132,  77}, 0 },  /*  27 trakd.bmp      132x77  */
    {  28, {   0,   0, 132,  77}, 0 },  /*  28 trake.bmp      132x77  */
    {  29, {   0,   0,  53,  59}, 0 },  /*  29 ashft.bmp       53x59  */
    {  30, {   0,   0,  97,  64}, 0 },  /*  30 carTR.bmp       97x64  */
    {  31, {   0,   0,  97,  64}, 0 },  /*  31 carCE.bmp       97x64  */
    {  32, {   0,   0,  97,  64}, 0 },  /*  32 carCU.bmp       97x64  */
    {  33, {   0,   0,  97,  64}, 0 },  /*  33 carES.bmp       97x64  */
    {  34, {   0,   0,  97,  64}, 0 },  /*  34 carFH.bmp       97x64  */
    {  35, {   0,   0,  97,  64}, 0 },  /*  35 carIP.bmp       97x64  */
    {  36, {   0,   0,  97,  64}, 0 },  /*  36 carLD.bmp       97x64  */
    {  37, {   0,   0,  97,  64}, 0 },  /*  37 carM3.bmp       97x64  */
    {  38, {   0,   0,  97,  64}, 0 },  /*  38 carMN.bmp       97x64  */
    {  39, {   0,   0,  97,  64}, 0 },  /*  39 carNS.bmp       97x64  */
    {  40, {   0,   0,  97,  64}, 0 },  /*  40 carPJ.bmp       97x64  */
    {  41, {   0,   0,  97,  64}, 0 },  /*  41 carPS.bmp       97x64  */
    {  42, {   0,   0,  97,  64}, 0 },  /*  42 carRS.bmp       97x64  */
    {  43, {   0,   0,  97,  64}, 0 },  /*  43 carSP.bmp       97x64  */
    {  44, {   0,   0,  97,  64}, 0 },  /*  44 carBB.bmp       97x64  */
    {  45, {   0,   0,  16,  16}, 1 },  /*  45 arrowdd.bmp     16x16  keyed */
    {  46, {   0,   0,  16,  16}, 1 },  /*  46 (unnamed)       16x16  keyed */
    {  47, {   0,   0,  16,  16}, 1 },  /*  47 arrowud.bmp     16x16  keyed */
    {  48, {   0,   0,  16,  16}, 1 },  /*  48 arrowuu.bmp     16x16  keyed */
    {  49, {   0,   0, 137, 113}, 1 },  /*  49 joystk.bmp     137x113 keyed */
    {  50, {   0,   0, 137, 113}, 1 },  /*  50 keybd.bmp      137x113 keyed */
    {  51, {   0,   0, 137, 113}, 1 },  /*  51 steerinp.bmp   137x113 keyed */
    {  52, {   0,   0, 128, 144}, 1 },  /*  52 type_yel.bmp   128x144 keyed */
    {  53, {   0,   0,  34,  48}, 1 },  /*  53 steerarr.bmp    34x48  keyed */
    {  54, {   0,   0, 113,  50}, 0 },  /*  54 steeradj.bmp   113x50  */
    {  55, {   0,   0, 123, 105}, 0 },  /*  55 spkr.bmp       123x105 */
    {  56, {   0,   0, 123, 105}, 0 },  /*  56 monitr.bmp     123x105 */
    {  57, {   0,   0, 192,  20}, 0 },  /*  57 namebar.bmp    192x20  */
    {  58, {   0,   0,  16,  16}, 1 },  /*  58 slidebox.bmp    16x16  keyed */
    {  59, {   0,   0,  16,  36}, 0 },  /*  59 boxtile2.bmp    16x36  */
    {  60, {   0,   0,   8,  36}, 0 },  /*  60 rboxend.bmp      8x36  */
    {  61, {   0,   0,   8,  36}, 0 },  /*  61 lboxend.bmp      8x36  */
    {  62, {   0,   0, 100,  53}, 0 },  /*  62 trakc_.bmp     100x53  */
    {  63, {   0,   0, 100,  53}, 0 },  /*  63 (unnamed)      100x53  */
    {  64, {   0,   0, 100,  53}, 0 },  /*  64 trake_.bmp     100x53  */
    {  65, {   0,   0, 100,  53}, 0 },  /*  65 desrttr_.bmp   100x53  */
    {  66, {   0,   0, 100,  53}, 0 },  /*  66 coasttr_.bmp   100x53  */
    {  67, {   0,   0, 106,  96}, 0 },  /*  67 mpmodem.bmp    106x96  */
    {  68, {   0,   0, 106,  96}, 0 },  /*  68 mptcpip.bmp    106x96  */
    {  69, {   0,   0, 106,  96}, 0 },  /*  69 mpipx.bmp      106x96  */
    {  70, {   0,   0, 106,  96}, 0 },  /*  70 mpserial.bmp   106x96  */
    {  71, {   0,   0, 233, 157}, 0 },  /*  71 seasn2a.bmp    233x157 */
    {  72, {   0,   0, 229, 126}, 0 },  /*  72 seasn2b.bmp    229x126 */
    {  73, {   0,   0, 233, 157}, 0 },  /*  73 seasn3a.bmp    233x157 */
    {  74, {   0,   0, 229, 126}, 0 },  /*  74 seasn3b.bmp    229x126 */
    {  75, {   0,   0, 233, 157}, 0 },  /*  75 seasn4a.bmp    233x157 */
    {  76, {   0,   0, 229, 126}, 0 },  /*  76 seasn4b.bmp    229x126 */
    {  77, {   0,   0, 233, 157}, 0 },  /*  77 seasn5a.bmp    233x157 */
    {  78, {   0,   0, 640, 292}, 0 },  /*  78 bgdim.bmp      640x292 */
    {  79, {   0,   0, 607, 105}, 0 },  /*  79 congrat.bmp    607x105 */
    {  80, {   0,   0, 640, 384}, 1 },  /*  80 (unnamed)      640x384 keyed */
    {  81, {   0,   0, 384, 384}, 1 },  /*  81 noadv2.bmp     384x384 keyed */
    {  82, {   0,   0, 127,  33}, 0 },  /*  82 but-main.bmp   127x33  */
    {  83, {   0,   0, 127,  33}, 0 },  /*  83 but-maind.bmp  127x33  */
    {  84, {   0,   0, 127,  33}, 0 },  /*  84 but-op.bmp     127x33  */
    {  85, {   0,   0, 127,  33}, 0 },  /*  85 but-opd.bmp    127x33  */
    {  86, {   0,   0, 103,  44}, 0 },  /*  86 cars1a.bmp     103x44  */
    {  87, {   0,   0, 103,  44}, 0 },  /*  87 cars2a.bmp     103x44  */
    {  88, {   0,   0, 104,  43}, 0 },  /*  88 cars2b.bmp     104x43  */
    {  89, {   0,   0, 103,  44}, 0 },  /*  89 cars3a.bmp     103x44  */
    {  90, {   0,   0, 104,  43}, 0 },  /*  90 cars3b.bmp     104x43  */
    {  91, {   0,   0, 103,  44}, 0 },  /*  91 cars4a.bmp     103x44  */
    {  92, {   0,   0, 104,  43}, 0 },  /*  92 cars4b.bmp     104x43  */
    {  93, {   0,   0, 103,  44}, 0 },  /*  93 cars5a.bmp     103x44  */
    {  94, {   0,   0, 104,  43}, 0 },  /*  94 cars5b.bmp     104x43  */
    {  95, {   0,   0, 640, 159}, 0 },  /*  95 chatbar2.bmp   640x159 */
    {  96, {   0,   0, 137, 113}, 0 },  /*  96 mousinpt.bmp   137x113 */
    {  97, {   0,   0, 112, 120}, 0 },  /*  97 (unnamed)      112x120 */
    {  98, {   0,   0,  90,  60}, 0 },  /*  98 carwnoshad2.bmp  90x60  */
    {  99, {   0,   0,  90,  60}, 0 },  /*  99 carwshad2.bmp   90x60  */
    { 100, {   0,   0, 101,  79}, 0 },  /* 100 specoff.bmp    101x79  */
    { 101, {   0,   0, 101,  79}, 0 },  /* 101 specon.bmp     101x79  */
    { 102, {   0,   0, 112, 120}, 0 },  /* 102 noffstik.bmp   112x120 */
    { 103, {   0,   0, 320, 159}, 0 },  /* 103 listbox.bmp    320x159 */
    { 104, {   0,   0,  79,  45}, 0 },  /* 104 engsound.bmp    79x45  */
    { 105, {   0,   0,  79,  45}, 0 },  /* 105 music.bmp       79x45  */
    { 106, {   0,   0,  85,  14}, 0 },  /* 106 soundtik.bmp    85x14  */
    { 107, {   0,   0,  13,  14}, 1 },  /* 107 soundptr.bmp    13x14  keyed */
    { 108, {   0,   0, 229, 126}, 0 },  /* 108 trrwd.bmp      229x126 */
    { 109, {   0,   0, 229, 126}, 0 },  /* 109 pjrwd.bmp      229x126 */
    { 110, {   0,   0, 229, 126}, 0 },  /* 110 mnrwd.bmp      229x126 */
    { 111, {   0,   0, 233, 157}, 0 },  /* 111 mirrwd.bmp     233x157 */
    { 112, {   0,   0, 233, 157}, 0 },  /* 112 bbrwd.bmp      233x157 */
    { 113, {   0,   0, 233, 157}, 0 },  /* 113 curwd.bmp      233x157 */
    { 114, {   0,   0, 233, 157}, 0 },  /* 114 fbrwd.bmp      233x157 */
    { 115, {   0,   0, 233, 157}, 0 },  /* 115 mtrwd.bmp      233x157 */
    { 116, {   0,   0, 104,   5}, 0 },  /* 116 sndlevl2.bmp   104x5   */
    { 117, {   0,   0,  12,   5}, 1 },  /* 117 sndlevl3.bmp    12x5   keyed */
    { 118, {   0,   0, 132,  77}, 0 },  /* 118 trakraceb.bmp  132x77  */
    { 119, {   0,   0, 100,  53}, 0 },  /* 119 trakracel.bmp  100x53  */
    { 120, {   0,   0, 127,  33}, 0 },  /* 120 but-sav.bmp    127x33  */
    { 121, {   0,   0, 127,  33}, 0 },  /* 121 but-savd.bmp   127x33  */
    { 122, {   0,   0,  97,  64}, 0 },  /* 122 z-carMT.bmp     97x64  */
    { 123, {   0,   0,  97,  64}, 0 },  /* 123 z-carTR.bmp     97x64  */
    { 124, {   0,   0,  97,  64}, 0 },  /* 124 z-carCE.bmp     97x64  */
    { 125, {   0,   0,  97,  64}, 0 },  /* 125 z-carCU.bmp     97x64  */
    { 126, {   0,   0,  97,  64}, 0 },  /* 126 z-carES.bmp     97x64  */
    { 127, {   0,   0,  97,  64}, 0 },  /* 127 (unnamed)       97x64  */
    { 128, {   0,   0,  97,  64}, 0 },  /* 128 z-carIP.bmp     97x64  */
    { 129, {   0,   0,  97,  64}, 0 },  /* 129 z-carLD.bmp     97x64  */
    { 130, {   0,   0,  97,  64}, 0 },  /* 130 z-carM3.bmp     97x64  */
    { 131, {   0,   0,  97,  64}, 0 },  /* 131 z-carMN.bmp     97x64  */
    { 132, {   0,   0,  97,  64}, 0 },  /* 132 z-carNS.bmp     97x64  */
    { 133, {   0,   0,  97,  64}, 0 },  /* 133 z-carPJ.bmp     97x64  */
    { 134, {   0,   0,  97,  64}, 0 },  /* 134 z-carPS.bmp     97x64  */
    { 135, {   0,   0,  97,  64}, 0 },  /* 135 z-carRS.bmp     97x64  */
    { 136, {   0,   0,  97,  64}, 0 },  /* 136 z-carSP.bmp     97x64  */
    { 137, {   0,   0,  97,  64}, 0 },  /* 137 z-carBB.bmp     97x64  */
    { 138, {   0,   0,  16,  16}, 0 },  /* 138 lightr.bmp      16x16  */
    { 139, {   0,   0,  16,  16}, 0 },  /* 139 lightg.bmp      16x16  */
    { 140, {   0,   0, 102,  79}, 0 },  /* 140 tire2on.bmp    102x79  */
    { 141, {   0,   0, 102,  79}, 0 },  /* 141 tire2off.bmp   102x79  */
    { 142, {   0,   0, 320, 209}, 0 },  /* 142 listbox2.bmp   320x209 */
    { 143, {   0,   0, 132,  77}, 0 },  /* 143 trakQ.bmp      132x77  */
    { 144, {   0,   0, 100,  53}, 0 },  /* 144 (unnamed)      100x53  */};

/* ======================================================================
 * PART 2 -- the names
 *
 * 0x10056260 zeroes the whole image table (`mov ecx,0x122 / rep stosd` --
 * 0x122 dwords == 145 entries of two dwords, one more independent check on
 * the count), then for each entry mallocs 0x104 bytes and strcpys a literal
 * into it.  These are those literals, paired with the table slot each store
 * targets.  Seven entries could not be paired because the pairing walks the
 * function linearly and those seven store before they load; they are NULL
 * rather than guessed.
 *
 * Four of these names are load-bearing elsewhere and each is corroborated by
 * something other than the pairing:
 *
 *   2/3/4/52  type_gry, type_wit, type_mid, type_yel -- the four MENU FONT
 *             sheets.  0x1005B730 (the text box's per-glyph draw) maps the
 *             box's kind byte 0/1/2/4 to sprite 2/3/4/0x34 and blits a glyph
 *             rectangle out of it, so the menu font is a bitmap in these four
 *             images and NOT the display-list font br_font.c recovers.
 *   5         bignums -- 0x1005B7A0, the font-B glyph draw, pushes a literal
 *             5, and the entry is 204x93, which is a digit strip.
 * ====================================================================== */


/* SLOTS 16, 46, 63, 80, 97, 127 and 144 were NULL until a later pass.
 *
 * The pairing that recovered these names walks the loader at 0x10056260 and
 * matches each `mov edi,<string>` with the `mov [0x10AC5xxx],edx` that follows
 * it. That works for 138 of 145 -- and silently loses seven, because those
 * seven store their table slot BEFORE they load the string, so a linear
 * forward match pairs them with the next block's name or with nothing.
 *
 * Re-walked BLOCK-WISE instead (blocks delimited by the allocator call, one
 * store and one load per block, in either order) it recovers all 145 with zero
 * misses and zero duplicates.
 *
 * Corroborated from OUTSIDE the executable: every one of the seven files
 * exists on the disc and its BMP header matches the sprite table's rectangle
 * exactly -- 132x77, 16x16, 100x53, 640x384, 112x120, 97x64, 100x53. And each
 * completes a series its neighbours already establish (desrttrk beside
 * coasttrk, arrowdu beside arrowdd/ud/uu, noadv1 beside noadv2).
 */
const char *const g_aBrUiSpriteName[BR_UI_SPR_COUNT] = {
    "work1a.bmp",    /*   0 */
    "cursor.bmp",    /*   1 */
    "type_gry.bmp",  /*   2 */
    "type_wit.bmp",  /*   3 */
    "type_mid.bmp",  /*   4 */
    "bignums.bmp",   /*   5 */
    "champ.bmp",     /*   6 */
    "mp.bmp",        /*   7 */
    "ta.bmp",        /*   8 */
    "op.bmp",        /*   9 */
    "qr.bmp",        /*  10 */
    "carmt.bmp",     /*  11 */
    "mshft.bmp",     /*  12 */
    "softshok.bmp",  /*  13 */
    "medshok.bmp",   /*  14 */
    "hardshok.bmp",  /*  15 */
    "desrttrk.bmp",            /*  16 */
    "coasttrk.bmp",  /*  17 */
    "trakc.bmp",     /*  18 */
    "fog.bmp",       /*  19 */
    "nite.bmp",      /*  20 */
    "rain.bmp",      /*  21 */
    "snow.bmp",      /*  22 */
    "sunweth.bmp",   /*  23 */
    "drytire.bmp",   /*  24 */
    "medtire.bmp",   /*  25 */
    "wettire.bmp",   /*  26 */
    "trakd.bmp",     /*  27 */
    "trake.bmp",     /*  28 */
    "ashft.bmp",     /*  29 */
    "carTR.bmp",     /*  30 */
    "carCE.bmp",     /*  31 */
    "carCU.bmp",     /*  32 */
    "carES.bmp",     /*  33 */
    "carFH.bmp",     /*  34 */
    "carIP.bmp",     /*  35 */
    "carLD.bmp",     /*  36 */
    "carM3.bmp",     /*  37 */
    "carMN.bmp",     /*  38 */
    "carNS.bmp",     /*  39 */
    "carPJ.bmp",     /*  40 */
    "carPS.bmp",     /*  41 */
    "carRS.bmp",     /*  42 */
    "carSP.bmp",     /*  43 */
    "carBB.bmp",     /*  44 */
    "arrowdd.bmp",   /*  45 */
    "arrowdu.bmp",            /*  46 */
    "arrowud.bmp",   /*  47 */
    "arrowuu.bmp",   /*  48 */
    "joystk.bmp",    /*  49 */
    "keybd.bmp",     /*  50 */
    "steerinp.bmp",  /*  51 */
    "type_yel.bmp",  /*  52 */
    "steerarr.bmp",  /*  53 */
    "steeradj.bmp",  /*  54 */
    "spkr.bmp",      /*  55 */
    "monitr.bmp",    /*  56 */
    "namebar.bmp",   /*  57 */
    "slidebox.bmp",  /*  58 */
    "boxtile2.bmp",  /*  59 */
    "rboxend.bmp",   /*  60 */
    "lboxend.bmp",   /*  61 */
    "trakc_.bmp",    /*  62 */
    "trakd_.bmp",            /*  63 */
    "trake_.bmp",    /*  64 */
    "desrttr_.bmp",  /*  65 */
    "coasttr_.bmp",  /*  66 */
    "mpmodem.bmp",   /*  67 */
    "mptcpip.bmp",   /*  68 */
    "mpipx.bmp",     /*  69 */
    "mpserial.bmp",  /*  70 */
    "seasn2a.bmp",   /*  71 */
    "seasn2b.bmp",   /*  72 */
    "seasn3a.bmp",   /*  73 */
    "seasn3b.bmp",   /*  74 */
    "seasn4a.bmp",   /*  75 */
    "seasn4b.bmp",   /*  76 */
    "seasn5a.bmp",   /*  77 */
    "bgdim.bmp",     /*  78 */
    "congrat.bmp",   /*  79 */
    "noadv1.bmp",            /*  80 */
    "noadv2.bmp",    /*  81 */
    "but-main.bmp",  /*  82 */
    "but-maind.bmp", /*  83 */
    "but-op.bmp",    /*  84 */
    "but-opd.bmp",   /*  85 */
    "cars1a.bmp",    /*  86 */
    "cars2a.bmp",    /*  87 */
    "cars2b.bmp",    /*  88 */
    "cars3a.bmp",    /*  89 */
    "cars3b.bmp",    /*  90 */
    "cars4a.bmp",    /*  91 */
    "cars4b.bmp",    /*  92 */
    "cars5a.bmp",    /*  93 */
    "cars5b.bmp",    /*  94 */
    "chatbar2.bmp",  /*  95 */
    "mousinpt.bmp",  /*  96 */
    "ffstick.bmp",            /*  97 */
    "carwnoshad2.bmp", /*  98 */
    "carwshad2.bmp", /*  99 */
    "specoff.bmp",   /* 100 */
    "specon.bmp",    /* 101 */
    "noffstik.bmp",  /* 102 */
    "listbox.bmp",   /* 103 */
    "engsound.bmp",  /* 104 */
    "music.bmp",     /* 105 */
    "soundtik.bmp",  /* 106 */
    "soundptr.bmp",  /* 107 */
    "trrwd.bmp",     /* 108 */
    "pjrwd.bmp",     /* 109 */
    "mnrwd.bmp",     /* 110 */
    "mirrwd.bmp",    /* 111 */
    "bbrwd.bmp",     /* 112 */
    "curwd.bmp",     /* 113 */
    "fbrwd.bmp",     /* 114 */
    "mtrwd.bmp",     /* 115 */
    "sndlevl2.bmp",  /* 116 */
    "sndlevl3.bmp",  /* 117 */
    "trakraceb.bmp", /* 118 */
    "trakracel.bmp", /* 119 */
    "but-sav.bmp",   /* 120 */
    "but-savd.bmp",  /* 121 */
    "z-carMT.bmp",   /* 122 */
    "z-carTR.bmp",   /* 123 */
    "z-carCE.bmp",   /* 124 */
    "z-carCU.bmp",   /* 125 */
    "z-carES.bmp",   /* 126 */
    "z-carFH.bmp",            /* 127 */
    "z-carIP.bmp",   /* 128 */
    "z-carLD.bmp",   /* 129 */
    "z-carM3.bmp",   /* 130 */
    "z-carMN.bmp",   /* 131 */
    "z-carNS.bmp",   /* 132 */
    "z-carPJ.bmp",   /* 133 */
    "z-carPS.bmp",   /* 134 */
    "z-carRS.bmp",   /* 135 */
    "z-carSP.bmp",   /* 136 */
    "z-carBB.bmp",   /* 137 */
    "lightr.bmp",    /* 138 */
    "lightg.bmp",    /* 139 */
    "tire2on.bmp",   /* 140 */
    "tire2off.bmp",  /* 141 */
    "listbox2.bmp",  /* 142 */
    "trakQ.bmp",     /* 143 */
    "trakQ_.bmp",            /* 144 */};

/* ======================================================================
 * PART 3 -- the dispatch
 * ====================================================================== */

const BrUiSprite *BrUiSpriteAt(int32_t i)
{
    if (i < 0 || i >= BR_UI_SPR_COUNT) {
        return NULL;
    }
    return &g_aBrUiSprite[i];
}

/* The two ends of the pair.  aStepId is uint16 and the readers sign-extend
 * (`mov dx,[esi+0x2A40]` into a field the draw slot then reads with
 * `test ax,ax / jl`), so 0xFFFF means -1 and not 65535. */
int32_t BrUiCtlSpriteUp(const BrUiCtl_ *pCtl)
{
    return (pCtl != NULL) ? (int32_t)(int16_t)pCtl->aStepId[0] : -1;
}

/* @n64 0x80229530 located */
int32_t BrUiCtlSpriteDown(const BrUiCtl_ *pCtl)
{
    return (pCtl != NULL) ? (int32_t)(int16_t)pCtl->aStepId[1] : -1;
}

/* 0x10001320's first eighteen instructions.
 *
 * The width and height come from the rectangle and are rejected when
 * NEGATIVE, not when zero: `sub ebx,ecx / js return`.  A zero-width sprite
 * therefore survives this test and is handed to the copy loop, which does
 * nothing -- preserved, because a caller that guards on w > 0 and one that
 * guards on w >= 0 disagree about the six style-pool-shaped entries whose
 * rect is degenerate.
 *
 * The two clips are UNSIGNED (`jb`), and they are one-sided: only the right
 * and bottom edges move.  Nothing clamps a negative x or y, so the original
 * blits above and to the left of its surface.  This port reproduces the
 * arithmetic and leaves the caller to decide what to do about it. */
/* @implements 0x10001320 glide BrUiSprClip */
int BrUiSprClip(int32_t x, int32_t y, const int32_t *pRect,
                int32_t cx, int32_t cy, int32_t *pw, int32_t *ph)
{
    int32_t w, h;

    if (pRect == NULL || pw == NULL || ph == NULL) {
        return 0;                   /* DEVIATION: the original faults. */
    }

    w = pRect[2] - pRect[0];
    if (w < 0) {
        return 0;
    }
    h = pRect[3] - pRect[1];
    if (h < 0) {
        return 0;
    }

    /* `lea eax,[ebx+esi] / cmp eax,ecx / jb keep` then `cmp ecx,esi / jb
     * return`.  cx == 0 means "no surface known"; the original always has
     * one, so that arm is this port's and is spelled out. */
    if (cx > 0) {
        if ((uint32_t)(x + w) >= (uint32_t)cx) {
            if ((uint32_t)cx < (uint32_t)x) {
                return 0;
            }
            w = cx - x;
        }
    }
    if (cy > 0) {
        if ((uint32_t)(y + h) >= (uint32_t)cy) {
            if ((uint32_t)cy < (uint32_t)y) {
                return 0;
            }
            h = cy - y;
        }
    }

    *pw = w;
    *ph = h;
    return 1;
}

int BrUiCtlChrome(const BrUiCtl_ *pCtl, int32_t cx, int32_t cy,
                  BrUiChrome *pOut)
{
    const BrUiSprite *pSpr;
    const int32_t    *pRect;
    int32_t           iSprite, iImage;

    if (pOut == NULL) {
        return 0;
    }
    pOut->kind    = BR_UI_CHROME_NONE;
    pOut->iSprite = -1;
    pOut->pSpr    = NULL;
    pOut->iImage  = -1;
    pOut->pRect   = NULL;
    pOut->x = pOut->y = pOut->w = pOut->h = 0;
    pOut->fKeyed = 0;
    pOut->fDown  = 0;
    if (pCtl == NULL) {
        return 0;
    }

    /* --- 0x10048530, the PAGE frame, before any of this ----------------
     *
     * Whether a control's frame runs at all is the page loop's decision, and
     * two of its three tests are on the control's own flags:
     *
     *     if (f & 0x1000) { ...ordinal arm...; if (!(f & 0x10)) continue; }
     *     if (f & 0x0800) continue;
     *
     * They are hoisted here because a caller walking apCtl the way the page
     * does would otherwise draw controls the game never reaches. It is not a
     * hypothetical: the 0x5001 and 0x3001 controls -- which carry 0x1000 and
     * not 0x10 -- have a w1E20C of 0x34, so a caller that skipped this test
     * would paint a 128x144 images\type_yel.bmp over five of BrExt_10054B50's
     * twenty controls. The page frame walks straight past all five.
     *
     * The loop's third test, pfn14 returning zero, is a hook and stays with
     * the caller; so does the +0x800 sibling test 0x10048060 performs. */
    if (((uint32_t)pCtl->flags1C & 0x1000u) != 0 &&
        ((uint32_t)pCtl->flags1C & 0x0010u) == 0) {
        return 0;
    }
    if (((uint32_t)pCtl->flags1C & 0x0800u) != 0) {
        return 0;
    }

    /* --- 0x10048010, control vtable +0x08 ------------------------------ */
    if ((pCtl->flags28 & 1) == 0) {
        return 0;
    }
    if ((pCtl->flags1C & 0x100000) != 0) {
        /* The label arm.  The original also tests `lea eax,[ecx+0x2B65]`
         * against zero, which can never be zero -- the address of a member.
         * Recorded, not reproduced: there is no way to take the false side. */
        pOut->kind = BR_UI_CHROME_TEXT;
        return 1;
    }
    if ((pCtl->flags1C & 0x200000) != 0) {
        return 0;
    }

    /* --- 0x10047A10, control vtable +0x10 ------------------------------ */
    if (pCtl->f296C == 0) {
        /* 0x10047930: the table supplies both the rect and the image id. */
        iSprite = (int32_t)(int16_t)pCtl->w1E20C;
        pSpr    = BrUiSpriteAt(iSprite);
        if (pSpr == NULL) {
            return 0;
        }
        pRect  = pSpr->rect;
        iImage = pSpr->iImage;
    } else {
        /* 0x10047980, reached only through 0x10047A10's second arm, which
         * re-aims w1E20C at aStepId[wStep] and hands the blit the rectangle
         * at p1E210 + wStep*0x10.
         *
         * ASYMMETRY, preserved: this path passes the SPRITE INDEX to the blit
         * as its image selector, where 0x10047930 passes the table entry's
         * own +0x00.  The two agree only because every shipped entry has
         * iImage == its index.  0x10040DD0 really does push the w1E20C it
         * just loaded (`mov ax,[esi+0x1E20C]` ... `push eax`) and never
         * touches the entry's first word. */
        int32_t k = (int32_t)pCtl->wStep;

        if (k < 0 || k >= BR_UI_CTL_STEPS || pCtl->p1E210 == NULL) {
            return 0;               /* DEVIATION: the original faults. */
        }
        iSprite = (int32_t)(int16_t)pCtl->aStepId[k];
        /* DEVIATION (memory safety), and it is a real behavioural
         * difference rather than a tidy-up: 0x10047980 does NOT test the
         * sign of the code the way 0x10047930 does, so in the original a
         * negative one indexes the table out of bounds and blits whatever
         * lies below it. slice3_32.c's independent transcription of the
         * same pair records the same asymmetry. */
        if (iSprite < 0) {
            return 0;
        }
        pRect  = (const int32_t *)pCtl->p1E210 + (size_t)k * 4u;
        pSpr   = BrUiSpriteAt(iSprite);
        iImage = iSprite;
    }

    /* --- the blit's own geometry, 0x10058380 -> 0x10001320 -------------- */
    pOut->iSprite = iSprite;
    pOut->pSpr    = pSpr;
    pOut->iImage  = iImage;
    pOut->pRect   = pRect;
    pOut->fKeyed  = (pSpr != NULL) ? (pSpr->fBlit & 1) : 0;
    pOut->fDown   = (BrUiCtlSpriteDown(pCtl) >= 0 &&
                     iSprite == BrUiCtlSpriteDown(pCtl));

    /* Both draw slots convert the control's own +0x3C / +0x40 with __ftol,
     * the y first.  Truncation toward zero, not floor -- 0x1007C8A0. */
    pOut->y = BrFtolTrunc(pCtl->y);
    pOut->x = BrFtolTrunc(pCtl->x);

    if (!BrUiSprClip(pOut->x, pOut->y, pRect, cx, cy, &pOut->w, &pOut->h)) {
        return 0;
    }

    pOut->kind = BR_UI_CHROME_SPRITE;
    return 1;
}
