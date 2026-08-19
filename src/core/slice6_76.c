/* slice6_76.c -- BRD3D.dll, packet 76 (slice 6).  See slice6_76.h for how the
 * targets were chosen, what was declined and why, and the signature conflicts
 * found on the way.
 *
 * WHY THIS FILE INCLUDES ALMOST NOTHING
 * -------------------------------------
 * Twelve of the sixteen entry points here forward to a body owned by another
 * module, and those owners' headers cannot all coexist in one translation
 * unit: they carry conflicting partial models of the same objects.
 * port/host/brally.c hit this first, packet 74 followed it, and this file
 * follows both.  Every cross-module declaration below is copied VERBATIM from
 * the owning header and is tagged with that header, so a later divergence
 * shows up as a compile error at the owner rather than as silent disagreement
 * here.
 *
 * Struct types that only ever appear behind a pointer are declared as
 * incomplete tag types (`struct BrFfb;`).  That is enough to write the
 * signature, it is the SAME type the owner declares -- these are all tagged
 * structs, checked -- and it commits this file to no layout of its own.
 */

#include <stddef.h>
#include <stdint.h>

#include "slice6_76.h"

/* ==========================================================================
 * 0. Cross-module declarations (see the banner)
 * ========================================================================== */

/* slice1_05.h -- 0x1002F900.  slice2_15.h calls the same address with its own
 * name for the command pair; both structs are {uint32_t w0, w1;}. */
struct BrGfxWords;
struct BrGfxCmd;
extern void BrRdpSetCombineLERP(struct BrGfxWords *pOut,
                                int a0,  int b0,  int c0,  int d0,
                                int Aa0, int Ab0, int Ac0, int Ad0,
                                int a1,  int b1,  int c1,  int d1,
                                int Aa1, int Ab1, int Ac1, int Ad1);

/* slice5_61.h -- 0x10042AF0 and 0x10060E90. */
extern void    BrGfx42AF0_1(void *p0);
extern int32_t BrTimeNow(void);

/* slice2_15.h / slice5_62.h -- 0x10069490, an adapter over br_pool.c. */
struct BrMat4;
extern struct BrMat4 *BrSub_10069490(void);

/* slice3_41.h -- 0x10069530. */
extern void *BrPool32Alloc(void);

/* slice5_63.h -- 0x1003E310. */
extern void BrSub1003E310(void);

/* slice4_53.h -- 0x1006A4A0. */
extern void BrSub1006A4A0(void *pThis, void *pArg);

/* slice1_10.h -- 0x10079550; slice3_45.h owns the one instance. */
struct BrFfb;
extern void BrFfbShutdown(struct BrFfb *pFfb);
extern struct BrFfb g_brFfb;

/* slice2_25.h -- 0x100443E0 and 0x10044280.  Both return int; both callers
 * (slice2_26.c) declare void and ignore it. */
struct BrGameObj;
extern int BrOptOpen2950A(struct BrGameObj *pUnused);
extern int BrOptOpen2950B(struct BrGameObj *pUnused);

/* slice4_50.h -- 0x10043BF0. */
extern void BrSub10043BF0(struct BrGameObj *p);

/* slice1_08.h -- 0x10072550, and the three "is sound usable" gates. */
struct BrSndVoice;
extern int32_t BrSndVoiceStop(struct BrSndVoice *pVoice);
extern int32_t   BrSndG0B5DE8;    /* 0x100B5DE8 */
struct BrDSound;
extern struct BrDSound *BrSndPDS; /* 0x118290F8 */
extern void     *BrSndG18290FC;   /* 0x118290FC */

/* slice1_08.h / slice3_40.h -- 0x100BBAE0, a BYTE master volume. */
extern uint8_t BrSndMasterVolume;

/* slice3_40.h -- 0x100BBAD8, and the two ten-entry level tables. */
extern uint8_t BrG_0BBAD8;
extern const int32_t BrOptLevelATable[10];   /* 0x100ADF68 */
extern const int32_t BrOptLevelBTable[10];   /* 0x100ADF90 */

/* slice2_25.h / slice3_40.h -- the two slider positions. */
extern int32_t g_brB4E708;   /* 0x10B4E708 */
extern int32_t g_brB4E70C;   /* 0x10B4E70C */

/* slice2_18.h -- 0x106C65E4, the hi-res flag: non-zero doubles every rect. */
extern int32_t BrG_6C65E4;

/* slice2_20.h -- 0x100B8C90.  br_data.c defines it as 1. */
extern int g_i0B8C90;

/* slice4_50.h:250 -- 0x10094294, the local slot / palette index.  slice4_50.c
 * OWNS the storage; this packet only reads it.  See the note in section 1. */
extern int32_t g_br094294;

/* ==========================================================================
 * 1. Storage owned here (see the ownership notes in the header)
 * ========================================================================== */

/* 0x10094294 is NOT defined here.  slice4_50.c already owns it -- found by the
 * link, because both packets happened to pick the same name; had they not, the
 * result would have been two objects for one address, drifting apart after the
 * first write, which is exactly the aliased-storage bug CONVENTIONS describes.
 * This packet aliases into slice4_50's storage (declared in section 0 above)
 * and corrected its initialiser from 0 to the image's -1; see the report. */

/* 0x11828F08.  Genuine .bss in the original -- it lies past the end of
 * .data's raw bytes -- so zero here is the original's value, not a default. */
void *g_aBrSndBankVoice[BR_SND_BANK_VOICES];

/* The 0x100029F0 seam; see the header. */
void (*g_pfnBrMusicVolume0029F0)(int32_t volume) = NULL;

/* ==========================================================================
 * 2. Adapters -- the body already exists, this only wires the stub name to it
 * ========================================================================== */

/* 0x1002F900, 33 call sites in .text -- the highest-demand stub in the tree
 * after 0x10073E70, and it was never missing: slice1_05.c transcribed it as
 * BrRdpSetCombineLERP.  Prototype copied verbatim from slice2_15.h:495. */
void BrSub_1002F900(struct BrGfxCmd *pCmd,
                    int32_t a01, int32_t a02, int32_t a03, int32_t a04,
                    int32_t a05, int32_t a06, int32_t a07, int32_t a08,
                    int32_t a09, int32_t a10, int32_t a11, int32_t a12,
                    int32_t a13, int32_t a14, int32_t a15, int32_t a16)
{
    /* Two names, one 8-byte object; see the conflict note in the header. */
    BrRdpSetCombineLERP((struct BrGfxWords *)pCmd,
                        (int)a01, (int)a02, (int)a03, (int)a04,
                        (int)a05, (int)a06, (int)a07, (int)a08,
                        (int)a09, (int)a10, (int)a11, (int)a12,
                        (int)a13, (int)a14, (int)a15, (int)a16);
}

/* 0x10042AF0, 16 call sites.  Six bytes: `mov eax, 1 / ret`.  It reads no
 * argument and returns 1; all three host declarations disagree about both.
 * Both stub names forward to slice5_61's single body so the address keeps one
 * owner.  Prototypes copied verbatim from slice2_17.c:97 and slice2_18.h:135. */
void BrX10042AF0(void *p, int a1, int a2)
{
    (void)a1;
    (void)a2;                   /* neither is read by the original */
    BrGfx42AF0_1(p);
}

void BrGfx42AF0_3(void *p0, int32_t a1, int32_t a2)
{
    (void)a1;
    (void)a2;
    BrGfx42AF0_1(p0);
}

/* 0x10069490, 13 call sites.  This IS br_pool.h's BrPoolAlloc; slice5_62.c
 * already owns the BrPool instance behind it.  Prototype from slice2_17.c:103. */
void *BrX10069490(void)
{
    return (void *)BrSub_10069490();
}

/* 0x1003E310, 9 call sites.  slice5_63.c has the body.  Called with no
 * arguments from slice3_31.c:508. */
void BrExt_1003E310(void)
{
    BrSub1003E310();
}

/* 0x1006A4A0, 8 call sites -- the config writer.  slice4_53.c has the body,
 * including the emit list and the "+0x2A4 is never written" gap.  Prototype
 * copied verbatim from slice3_31.h:289. */
/* WHAT IT DOES: writes the game's settings out. This is only the name other
 * modules call it by -- the actual writing lives in slice4_53.c, and this
 * hands both arguments straight through. */
/* @implements 0x1006A4A0 d3d BrExt_1006A4A0 */
void BrExt_1006A4A0(void *pThis, void *pArg)
{
    BrSub1006A4A0(pThis, pArg);
}

/* 0x10060E90, 7 call sites -- the fake monotonic timer.  Prototype from
 * slice2_17.c:68; slice5_61.h returns int32_t, which is the same type here. */
/* WHAT IT DOES: answers "what time is it now?" in the game's own steadily
 * counting clock, which is what everything that has to happen after a delay
 * measures against. The real clock lives in slice5_61.c; this is a second name
 * for it. */
/* @implements 0x10060E90 d3d BrX10060E90 */
int BrX10060E90(void)
{
    return (int)BrTimeNow();
}

/* 0x10069530, 4 call sites -- the 32-byte per-frame slot bank.  Prototype
 * from slice2_17.c:101. */
void *BrX10069530(void)
{
    return BrPool32Alloc();
}

/* 0x10079550, 4 call sites -- force-feedback teardown, with its underflow
 * clamp that skips the teardown entirely.  slice1_10.c has the body and
 * slice3_45.c owns the single BrFfb, so this adapter binds no new storage. */
/* WHAT IT DOES: switches the force-feedback effects off and releases the wheel,
 * as part of shutting down. The work is slice1_10.c's; this just supplies the
 * one force-feedback object the game has. */
/* @implements 0x10079550 d3d BrExt_10079550 */
void BrExt_10079550(void)
{
    BrFfbShutdown(&g_brFfb);
}

/* 0x100443E0 and 0x10044280, 3 and 2 call sites -- open the 0x10AA2950
 * options object in networked / local form.  slice2_25.c has both bodies.
 *
 * Both take a BrGameObj * the original never reads (`(void)pUnused` in
 * slice2_25.c) and return int.  slice2_26.h declares them as
 * `void f(int32_t)` and calls them with 0, so the adapters honour that
 * declaration, pass NULL, and discard the result -- which the caller ignores
 * in any case.  Prototypes from slice2_26.h:246 and :247. */
void BrExt_100443E0(int32_t a)
{
    (void)a;
    (void)BrOptOpen2950B(NULL);
}

void BrExt_10044280(int32_t a)
{
    (void)a;
    (void)BrOptOpen2950A(NULL);
}

/* 0x10043BF0, 2 call sites.  slice4_50.c has the body and its header already
 * records that the argument is unread.  Prototype from slice2_26.h:242. */
void BrExt_10043BF0(int32_t a)
{
    (void)a;
    BrSub10043BF0(NULL);
}

/* ==========================================================================
 * 3. 0x10060D90 -- push both volume sliders through their tables
 * ==========================================================================
 *
 * Six call sites.  Two lookups that look symmetric and are not: the level-A
 * slider (0x10B4E70C) goes through the LINEAR table and is then handed to the
 * volume setter, while the level-B slider (0x10B4E708) goes through the
 * PERCEPTUAL curve and is only stored.  README records that these two tables
 * must not be merged; this is the function that reads both.
 *
 * Prototype copied verbatim from slice2_25.h:457. */
void BrSub10060D90(void)
{
    int32_t  idxA = g_brB4E70C;
    int32_t  idxB;
    uint32_t arg;

    /* GOTCHA, transcribed rather than simplified.  The original loads the
     * INDEX into eax, then overwrites only AL with the table byte, then
     * pushes the whole of eax.  The argument is therefore
     * (index & 0xFFFFFF00) | tableByte -- equal to the table byte only while
     * the index is below 0x100, which it is for the shipped 0..9 range.
     *
     * No bounds check on either index; the original has none and both are
     * only ever written by the cyclers, which cap at 9. */
    arg = ((uint32_t)idxA & 0xFFFFFF00u)
        | ((uint32_t)BrOptLevelATable[idxA] & 0xFFu);

    BrG_0BBAD8 = (uint8_t)(arg & 0xFFu);

    /* DEVIATION: the original calls 0x100029F0 here.  See the header -- a
     * NULL hook skips it. */
    if (g_pfnBrMusicVolume0029F0 != NULL)
        g_pfnBrMusicVolume0029F0((int32_t)arg);

    /* The second slider is read AFTER the call, from a fresh load of the
     * global -- so a volume setter that moved it would be seen here. */
    idxB = g_brB4E708;
    BrSndMasterVolume = (uint8_t)((uint32_t)BrOptLevelBTable[idxB] & 0xFFu);
}

/* ==========================================================================
 * 4. 0x100193C0 -- width of a string in the proportional font
 * ==========================================================================
 *
 * Four call sites.  Tables read out of BRD3D.dll rather than assumed. */

/* 0x100A5FEF + 0x21 .. + 0x7F. */
const signed char BrTextClassMap[BR_TEXT_CLASS_N] = {
    /* !  "  #  $  %  &  '  (  )  *  +  ,  -  .  / */
       13,22,14,15,11,16,21,11,11,24,18,10,17,11,23,
    /* 0  1  2  3  4  5  6  7  8  9 */
        9, 0, 1, 2, 3, 4, 5, 6, 7, 8,
    /* :  ;  <  =  >  ?  @ */
       20,19,11,25,11,12,26,
    /* A..Z */
       28,29,30,31,32,33,34,35,36,37,38,39,40,
       41,42,43,44,45,46,47,48,49,50,51,52,53,
    /* [  \  ]  ^  _  ` */
       11,11,11,11,11,11,
    /* a..z -- the same classes as the upper case */
       28,29,30,31,32,33,34,35,36,37,38,39,40,
       41,42,43,44,45,46,47,48,49,50,51,52,53,
    /* {  |  }  ~  DEL */
       11,11,11,11,11
};

/* 0x100A6070. */
const int32_t BrTextWidthLarge[BR_TEXT_GLYPHS] = {
      0, 26, 52, 78,104,130,156,182,208,234,
    260,274,290,315,332,381,407,437,462,488,
    505,522,539,565,596,626,648,674,
    /* class 27 is the gap between the two runs; see the header */
      0, 29, 56, 82,109,134,160,185,213,229,
    248,276,298,332,360,386,413,439,466,492,
    517,545,575,610,641,671,697
};

/* 0x100A6150. */
const int32_t BrTextWidthSmall[BR_TEXT_GLYPHS] = {
      0, 14, 28, 42, 56, 70, 84, 98,112,126,
    140,148,156,172,181,207,222,238,252,266,
    276,285,296,310,327,344,355,371,
      0, 16, 31, 46, 61, 75, 89,103,119,128,
    139,154,166,184,199,214,229,243,258,272,
    286,301,317,337,353,370,384
};

/* Prototype copied verbatim from slice6_70.h:374.
 *
 * `scale` is the caller's nominal glyph height.  The hi-res flag doubles it on
 * the way in and halves the total on the way out, which is not a no-op: the
 * doubling happens BEFORE the large/small threshold test, so hi-res can select
 * the large font for a scale that would otherwise have taken the small one. */
/* WHAT IT DOES: measures how wide a line of text will come out at a given
 * size, which is what lets the menus centre captions and fit them into boxes.
 * It picks the large or small lettering the same way the drawing code does,
 * adds up each character's width, and treats the inline "%" colour codes as
 * taking no space -- except that a code like "%rw" swallows one character too
 * many, so text after a colour code measures narrower than it draws. Spaces are
 * short by a pixel each for the same reason. */
/* @implements 0x100193C0 d3d BrSub_100193C0 */
int BrSub_100193C0(const char *psz, int scale)
{
    const int32_t *pTable;
    const char    *p;
    int            divisor;
    int            total = 0;
    int            c;

    if (BrG_6C65E4 != 0)
        scale <<= 1;

    /* Signed compares in the original (`jg`, `jl`). */
    if (g_i0B8C90 <= 1 && scale >= BR_TEXT_LARGE_MIN) {
        divisor = BR_TEXT_DIV_LARGE;
        pTable  = BrTextWidthLarge;
    } else {
        divisor = BR_TEXT_DIV_SMALL;
        pTable  = BrTextWidthSmall;
    }

    p = psz;
    c = (unsigned char)*p;
    while (c != 0) {
        int fGlyph = 1;

        /* The original compares AL SIGNED, so anything with the high bit set
         * -- every byte from 0x80 up -- takes the same branch as a space or a
         * control character, not the glyph branch. */
        if ((signed char)(unsigned char)c < BR_TEXT_CLASS_LO ||
            (signed char)(unsigned char)c > BR_TEXT_CLASS_HI) {
            /* Fixed width, and NOT scaled by the font divisor: the original
             * computes ((scale*8 - scale) * 2) / 40, i.e. 14*scale/40, by
             * reciprocal multiply.  The +sign-bit correction after the shift
             * makes it truncate toward zero, which C division already does. */
            total += (14 * scale) / 40;
            fGlyph = 0;
        } else if (c == '%' && p[1] != '\0') {
            if ((unsigned char)p[1] == (unsigned char)c) {
                /* "%%" -- consume one, then measure '%' as a glyph. */
                ++p;
            } else if (p[1] == 'i' || p[1] == 'n') {
                /* Consumed, no width. */
                ++p;
                fGlyph = 0;
            } else if (p[2] != '\0') {
                /* Any other "%X" followed by more text.  ORIGINAL BUG,
                 * preserved: this steps the cursor by 2 and then the shared
                 * advance below steps it again, so THREE characters are
                 * consumed and none of them is measured.  "%di" contributes
                 * nothing at all -- not even the 'i'.  The `%%`, `%i` and
                 * `%n` paths step by 1 and consume two, which is right. */
                p += 2;
                fGlyph = 0;
            }
            /* else: p[2] is NUL, so the original falls through to the glyph
             * branch with AL still '%' -- the '%' is measured here and the
             * directive letter is measured as a plain glyph on the next
             * iteration.  Preserved; see the header. */
        }

        if (fGlyph) {
            int k = BrTextClassMap[c - BR_TEXT_CLASS_LO];
            total += ((pTable[k + 1] - pTable[k]) * scale) / divisor;
        }

        /* Advance exactly as the original does: read the NEXT byte, then step.
         * The loop re-tests that byte, so a NUL ends it. */
        c = (unsigned char)p[1];
        ++p;
    }

    if (BrG_6C65E4 != 0)
        total >>= 1;            /* arithmetic shift in the original (`sar`) */

    return total;
}

/* ==========================================================================
 * 5. 0x10072580 -- stop one bank voice
 * ==========================================================================
 *
 * Three call sites.  Four guards, then a Stop; the original returns 1 from
 * every guard and (hr == 0) from the tail, expressed as `neg/sbb/inc`.
 *
 * Prototype copied verbatim from slice2_17.c:95, which declares it void.  The
 * result is therefore discarded -- see the conflict note in the header. */
/* WHAT IT DOES: silences one of the game's sound-effect slots. If sound was
 * never brought up, or that slot is not holding a sound, it quietly does
 * nothing. */
/* @implements 0x10072580 d3d BrX10072580 */
void BrX10072580(int a0)
{
    struct BrSndVoice *pVoice;

    if (BrSndG0B5DE8 == 0)
        return;                         /* the original returns 1 */
    if (BrSndPDS == NULL)
        return;
    if (BrSndG18290FC == NULL)
        return;

    /* No bounds check on a0 in the original.  Preserved. */
    pVoice = (struct BrSndVoice *)g_aBrSndBankVoice[a0];
    if (pVoice == NULL)
        return;

    /* The original's result is `BrSndVoiceStop(pVoice) == 0`. */
    (void)BrSndVoiceStop(pVoice);
}

/* ==========================================================================
 * 6. 0x10005D30 -- read the local slot index
 * ==========================================================================
 *
 * Two call sites.  Six bytes: one load and a ret.  Prototype copied verbatim
 * from slice3_40.h:169. */
int32_t BrSub10005D30(void)
{
    return g_br094294;
}
