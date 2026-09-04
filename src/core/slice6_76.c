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
/* NOT tagged: the body is BrGlCfgSave in slice4_53.c, which carries the
 * @implements and matches byte-for-byte.  This is the second NAME the image
 * gives that address, and as a thunk it compiles to a 32-byte call that can
 * never reproduce the 930-byte original -- the BrVec3Len trap in
 * slice6_74.c, which had one address in the measured set twice with one of
 * the pair permanently unmatchable. */
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
/* @d3donly 0x10060E90 BrX10060E90 -- exists in BRGlide only as folded/duplicated stubs; no unique twin locatable by bytes */
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
/* port-only body; Glide match is src/core/generated/0x10072840.c */
#ifdef BR_MATCHING_BUILD
/* Literal: the original works on the force-feedback globals directly (the
 * port factors this as slice1_10.c's BrFfbShutdown over a struct).  Same
 * refcount clamp, same three vtable releases, same unacquire-then-release
 * on the device.  See slice1_10.c for the annotated walkthrough. */
typedef struct BrComVt76 {
    int (__stdcall *f00)(void *);
    int (__stdcall *f04)(void *);
    int (__stdcall *pfnRelease)(void *);            /* +0x08 */
    int (__stdcall *f0c[5])(void *);
    int (__stdcall *pfnUnacquire)(void *);          /* +0x20 */
} BrComVt76;
typedef struct BrComObj76 { BrComVt76 *pVtbl; } BrComObj76;
extern int DAT_118eef18;
extern BrComObj76 *DAT_118eef14;
extern BrComObj76 *DAT_118eef04;
extern BrComObj76 *DAT_118eeeec;
void BrExt_10079550(void)
{
    BrComObj76 *pObj;
    int count;

    count = DAT_118eef18 - 1;
    DAT_118eef18 = count;
    if (count < 0) {
        DAT_118eef18 = 0;
        return;
    }
    if (DAT_118eef18) {
        return;
    }
    pObj = DAT_118eef14;
    if (pObj) {
        pObj->pVtbl->pfnRelease(pObj);
        DAT_118eef14 = 0;
    }
    pObj = DAT_118eef04;
    if (pObj) {
        pObj->pVtbl->pfnRelease(pObj);
        DAT_118eef04 = 0;
    }
    pObj = DAT_118eeeec;
    if (pObj) {
        pObj->pVtbl->pfnUnacquire(pObj);
        DAT_118eeeec->pVtbl->pfnRelease(DAT_118eeeec);
        DAT_118eeeec = 0;
    }
}
#else
void BrExt_10079550(void)
{
    BrFfbShutdown(&g_brFfb);
}
#endif

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
/* @d3donly 0x100193C0 BrSub_100193C0 -- glide twin 0x10016980 claimed by br_font.c:BrFontMeasure */
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
 * slice2_17.c:95 declares this void and discards the result.  The original
 * returns int -- 1 from every guard, (hr == 0) from the tail -- and matching
 * needs that, so the definition follows the image rather than the host
 * prototype.  Callers still ignore eax. */
/* WHAT IT DOES: silences one of the game's sound-effect slots. If sound was
 * never brought up, or that slot is not holding a sound, it quietly does
 * nothing. */
/* @implements 0x10072580 d3d BrX10072580 */
int BrX10072580(int a0)
{
    struct BrSndVoice *pVoice;

    /* Nested so /O2 shares one `mov eax, 1 / ret` epilogue (`je` to it). */
    if (BrSndG0B5DE8 != 0) {
        if (BrSndPDS != NULL) {
            if (BrSndG18290FC != NULL) {
                /* No bounds check on a0 in the original.  Preserved. */
                pVoice = (struct BrSndVoice *)g_aBrSndBankVoice[a0];
                if (pVoice != NULL)
                    return BrSndVoiceStop(pVoice) == 0;
            }
        }
    }
    return 1;
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

/* ==========================================================================
 * 7. 0x10073B00 -- create the 32x128 backend texture and keep the handle
 * ==========================================================================
 *
 * Twin of 0x10073AC0: same fourteen-argument call through the backend
 * texture constructor at 0x118AA0B0 (slice2_16.h's BrGbiTexCreateFn), same
 * 0x20 x 0x80 / fmt 0 / siz 2, but this one writes the returned handle to
 * BOTH 0x100A64A0 and 0x100A649C.  Store order is load-bearing: 0x100A64A0
 * first.  The two pointer arguments are the ADDRESSES of the globals, not
 * their contents -- `push imm32` of each VA, not `push dword ptr [VA]`.
 *
 * Matching-only: the constructor pointer and the two source objects have no
 * owner in the port tree, and inventing a size for the pixel buffers would
 * be a second view of storage this packet does not own. */
#ifdef BR_MATCHING_BUILD
typedef void *(*BrTexCreateFn10073B00)(void *pSrc, void *pArg2,
                                       int w, int h, int fmt, int siz,
                                       int a7, int a8, int a9, int a10,
                                       int a11, int a12, int a13, int a14);

BrTexCreateFn10073B00 g_pfn18AA0B0; /* 0x118AA0B0 */
char                  g_18AA0F8;    /* 0x118AA0F8 */
char                  g_18AB0F8;    /* 0x118AB0F8 */
void                 *g_0A64A0;     /* 0x100A64A0 */
void                 *g_0A649C;     /* 0x100A649C */

/* WHAT IT DOES: asks the graphics backend to turn one of the game's pixel
 * buffers into a 32-by-128 texture and then remembers the handle in two
 * slots so later readers of either slot see the same texture. */
/* @implements 0x10073B00 d3d BrSub10073B00 */
void BrSub10073B00(void)
{
    /* Chained so eax is stored twice without a reload. */
    g_0A649C = g_0A64A0 = g_pfn18AA0B0(&g_18AA0F8, &g_18AB0F8,
                                       0x20, 0x80, 0, 2,
                                       0, 0, 0, 0,
                                       0, 0, 0, 0);
}
#endif

/* ── Ghidra-matched functions ─────────────────────────── */
#ifdef BR_MATCHING_BUILD
extern int DAT_11849e64;
int FUN_1006a650();
int FUN_1006a7e0();
int FUN_1006aaf0();
int FUN_1006ab80();
int FUN_1006b0e0();
extern unsigned int DAT_11849ea8;
extern unsigned int DAT_1184c070;
extern int g_brP277B40;
int BrDelta_100713A0();
#include <windows.h>
extern int DAT_11849e60;
extern int DAT_1184c078;
extern int DAT_1184c07c;
void BrSndVoiceApplyFreq(int);
void BrSndVoiceApplyPan(int);

/* WHAT IT DOES: signal the sound-mixing thread to exit, wait for it, and close its handles. */
/* @implements 0x1006B1E0 glide BrSndThreadStop */

int BrSndThreadStop(void)

{
  if (DAT_1184c078 != 0) {
    SetEvent(DAT_11849e60);
    WaitForSingleObject(DAT_1184c07c,0xffffffff);
    CloseHandle(DAT_1184c07c);
    DAT_1184c07c = (HANDLE)0x0;
    CloseHandle(DAT_11849e60);
    DAT_11849e60 = (HANDLE)0x0;
    DAT_1184c078 = 0;
  }
  return;
}

/* BrSndVoiceSetPan (0x1006B5B0) stays in ghidra_batch.c — context-sensitive codegen. */

typedef void (__stdcall *dsbuf_fn2)(int, int);

typedef int (__stdcall *dsbuf_fn1)(int);

/* WHAT IT DOES: release the voice's DirectSound buffer (vtable +8 = Release)
 * and clear the pointer; always returns 0. */
/* @implements 0x1006B490 glide BrSndVoiceBufRelease */

int BrSndVoiceBufRelease(int param_1)

{
  int *piVar1;

  piVar1 = *(int **)(param_1 + 0x9c);
  if (piVar1 != (int *)0x0) {
    (*(dsbuf_fn1 *)(*piVar1 + 8))((int)piVar1);
    *(int *)(param_1 + 0x9c) = 0;
  }
  return 0;
}

/* WHAT IT DOES: if the voice is playing, call IDirectSoundBuffer::Stop
 * (vtable +0x48) and clear the playing flag on S_OK; returns the HRESULT. */
/* @implements 0x1006B4C0 glide BrSndVoiceBufStop */

int BrSndVoiceBufStop(int param_1)

{
  int iVar1;

  if (*(int *)(param_1 + 0x1c) == 0) {
    return 0;
  }
  iVar1 = (*(dsbuf_fn1 *)(**(int **)(param_1 + 0x9c) + 0x48))(*(int *)(param_1 + 0x9c));
  if (iVar1 == 0) {
    *(int *)(param_1 + 0x1c) = iVar1;
  }
  return iVar1;
}

/* WHAT IT DOES: call IDirectSoundBuffer::SetPan with a computed pan value. */
/* @implements 0x1006B400 glide BrSndVoiceApplyPan */

void BrSndVoiceApplyPan(int param_1)

{
  dsbuf_fn2 fn = *(dsbuf_fn2 *)(**(int **)(param_1 + 0x9c) + 0x40);
  fn(*(int *)(param_1 + 0x9c), (*(int *)(param_1 + 0x10) + -400) * 10);
  return;
}

/* WHAT IT DOES: call IDirectSoundBuffer::SetFrequency from the voice struct. */
/* @implements 0x1006B420 glide BrSndVoiceApplyFreq */

void BrSndVoiceApplyFreq(int param_1)

{
  dsbuf_fn2 fn = *(dsbuf_fn2 *)(**(int **)(param_1 + 0x9c) + 0x44);
  fn(*(int *)(param_1 + 0x9c), *(int *)(param_1 + 0xc));
  return;
}

/* WHAT IT DOES: push the voice's stored volume to DirectSound
 * (IDirectSoundBuffer::SetVolume, vtable +0x3c).  The stored level is scaled
 * by the global master level 0..255 and mapped onto DirectSound's
 * hundredths-of-a-decibel scale by (level - 400) * 10.  A master level of 0
 * short-circuits to DSBVOLUME_MIN (-10000) rather than computing silence.
 *
 * RESIDUE (T3a, parked): 84 vs 79 bytes, regnorm 3+1.  The arithmetic, the
 * unsigned /255 reciprocal, the branch polarity and both call sites are
 * already identical; the whole gap is allocation:
 *   - orig loads the parameter ONCE into ecx above `test al,al`; we load it
 *     per arm (+4 B), which costs a second callee-saved register (push/pop
 *     edi, +2 B) because the vtable fetch lands before `sub edx,0x190`
 *     instead of after it, while orig reuses edx for the vtable;
 *   - orig `mov eax,0xffffd8f0 / push eax`, we `push 0xffffd8f0` (-1 B).
 * DO NOT RE-RUN THESE -- six spellings, all BYTE-IDENTICAL output:
 *   (1) `dsbuf_fn2 fn = ...` assigned before the call, sibling style;
 *   (2) the call written inline with no local at all;
 *   (3) a `static __inline` two-arg helper called from both arms;
 *   (4) `int vol;` declared above the if and assigned in both arms;
 *   (5) a named `pBuf` local assigned AFTER the value (the "name the
 *       pointer" lever) with the call through `*pBuf`;
 *   (6) the multiply written master-first instead of level-first.
 * The next lever has to come from outside the statement spelling. */
/* @implements 0x1006B440 glide BrSndVoiceApplyVolume */

void BrSndVoiceApplyVolume(int param_1)

{
  int       vol;
  dsbuf_fn2 fn;

  if (BrSndMasterVolume != 0) {
    vol = ((*(unsigned int *)(param_1 + 0x14) * BrSndMasterVolume) / 0xff - 400) * 10;
    fn = *(dsbuf_fn2 *)(**(int **)(param_1 + 0x9c) + 0x3c);
    fn(*(int *)(param_1 + 0x9c), vol);
    return;
  }
  vol = -10000;
  fn = *(dsbuf_fn2 *)(**(int **)(param_1 + 0x9c) + 0x3c);
  fn(*(int *)(param_1 + 0x9c), vol);
  return;
}

/* br_musiccmd.c -- 0x1006BB60 and 0x1006BB90, the two list walkers. */
extern int BrSndBufStopAll(int param_1);
extern int BrSndBufFreeAll(int param_1);

/* 0x1184C2A8, the DirectSound object the two walkers hang their list off;
 * 0x1184C260, the live group count 0x1006C290 stores; 0x100B55F8, the voice
 * table, 0x12 dwords per group row (see br_sfx.h). */
extern int DAT_1184c2a8;
extern int DAT_1184c260;
extern int DAT_100b55f8[];

void *memset(void *, int, size_t);

/* WHAT IT DOES: tear the sound bank down -- stop every buffer on the device's
 * list, free the memory behind them, then zero the per-group voice rows (the
 * first 15 dwords of each 0x12-dword row, for as many groups as are loaded)
 * and the 15-slot bank voice array.  The device itself is left open, so a
 * reload can refill the same tables.  Sound disabled is a silent success. */
/* @implements 0x1006C460 glide BrSndBankFree */

int BrSndBankFree(void)

{
  int *pRow;
  int  cGroups;

  if (BrSndG0B5DE8 == 0) {
    return 1;
  }
  if (BrSndPDS == 0) {
    return 1;
  }
  if (BrSndG18290FC == 0) {
    return 1;
  }
  BrSndBufStopAll((int)&DAT_1184c2a8);
  BrSndBufFreeAll((int)&DAT_1184c2a8);
  cGroups = DAT_1184c260;
  if (0 < cGroups) {
    pRow = DAT_100b55f8;
    do {
      memset(pRow, 0, 60);
      pRow = pRow + 0x12;
    } while (--cGroups != 0);
  }
  memset(g_aBrSndBankVoice, 0, sizeof(g_aBrSndBankVoice));
  return 1;
}

/* 0x1184C1E8 -- each channel's base rate; br_sfxsrc.h owns the model. */
extern double g_aBrSfxChanRate[];

/* WHAT IT DOES: bind one of a group's voices to a playback channel.  Copies
 * the group row's 8-byte base rate into the channel's rate slot, silences
 * whatever the channel was already holding, then stores the new voice.
 * Returns whether the channel ended up holding a voice -- and, as everywhere
 * else on this path, a silent 1 when sound is not up. */
/* @implements 0x1006B530 glide BrSndChanBind */

int BrSndChanBind(int iGroup, int iSlot)

{
  int pVoice;

  if (((BrSndG0B5DE8 != 0) && (BrSndPDS != 0)) && (BrSndG18290FC != 0)) {
    g_aBrSfxChanRate[iSlot] = ((double *)DAT_100b55f8)[iGroup * 9 + 8];
    if (g_aBrSndBankVoice[iSlot] != 0) {
      BrX10072580(iSlot);
    }
    pVoice = DAT_100b55f8[iGroup * 0x12 + iSlot];
    g_aBrSndBankVoice[iSlot] = (void *)pVoice;
    return pVoice != 0;
  }
  return 1;
}

/* 0x1184C080 stride 24 -- br_sfxsrc.h's "applied" channel array; only its
 * +0x08 ratio field is touched here, so it is indexed as int64 elements,
 * three per channel.  0x10077C00 is the ratio-to-hertz scale constant. */
extern int64_t DAT_1184c088[];
extern double  DAT_10077c00;

int BrSndBufSetVolume(int param_1, int param_2);

/* WHAT IT DOES: push a channel's 32.32 pitch ratio at its voice.  The ratio
 * is scaled by the channel's base rate and the fixed-point constant to give
 * a frequency in hertz, which goes to the DirectSound buffer; only if that
 * succeeds is the ratio recorded as the one actually applied, so the record
 * never claims a pitch the device refused.  Sound down is a silent 1. */
/* @implements 0x1006B5F0 glide BrSndChanSetRatio */

int BrSndChanSetRatio(int iSlot, int64_t ratio)

{
  if (((BrSndG0B5DE8 != 0) && (BrSndPDS != 0)) && (BrSndG18290FC != 0)) {
    if (BrSndBufSetVolume((int)g_aBrSndBankVoice[iSlot],
                          (unsigned int)((double)ratio * g_aBrSfxChanRate[iSlot]
                                         * DAT_10077c00)) != 0) {
      DAT_1184c088[iSlot * 3] = ratio;
      return 1;
    }
    return 0;
  }
  return 1;
}

typedef int (__stdcall *dsbuf_fn2i)(int, int);
typedef int (__stdcall *dsbuf_fn4i)(int, int, int, int);

/* WHAT IT DOES: start a voice's buffer.  If the buffer is already playing
 * (GetStatus, vtable +0x24, reports DSBSTATUS_PLAYING) it is rewound instead
 * -- SetCurrentPosition(0), vtable +0x34 -- so retriggering a live sound
 * restarts it rather than layering a second Play on it.  Otherwise Play
 * (vtable +0x30) runs, looping iff the voice's +0x18 flag is set, and the
 * voice's "playing" flag at +0x1c is raised only when Play returns S_OK. */
/* @implements 0x1006B970 glide BrSndVoiceBufStart */

void BrSndVoiceBufStart(int param_1)

{
  unsigned int status;
  int          bLoop;

  status = 0;
  bLoop  = 0;
  if (*(int *)(param_1 + 0x18) != 0) {
    bLoop = 1;
  }
  if (((*(dsbuf_fn2i *)(**(int **)(param_1 + 0x9c) + 0x24))
         (*(int *)(param_1 + 0x9c), (int)&status) == 0) && ((status & 1) == 1)) {
    (*(dsbuf_fn2i *)(**(int **)(param_1 + 0x9c) + 0x34))
      (*(int *)(param_1 + 0x9c), 0);
    return;
  }
  if ((*(dsbuf_fn4i *)(**(int **)(param_1 + 0x9c) + 0x30))
        (*(int *)(param_1 + 0x9c), 0, 0, bLoop) == 0) {
    *(int *)(param_1 + 0x1c) = 1;
  }
  return;
}

/* WHAT IT DOES: silence the whole sound bank -- for every occupied voice slot
 * drive its DirectSound buffer to DSBVOLUME_MIN (vtable +0x3c) and recentre
 * the pan (vtable +0x40).  The buffers keep playing; only their output is
 * killed, so a later volume/pan restore resumes them mid-sound.  Empty slots
 * are skipped and the sound-disabled case is a silent success. */
/* @implements 0x1006BD70 glide BrSndBankMute */

int BrSndBankMute(void)

{
  void **ppVoice;
  int    pVoice;

  if (BrSndG0B5DE8 == 0) {
    return 1;
  }
  if (BrSndPDS == 0) {
    return 1;
  }
  if (BrSndG18290FC == 0) {
    return 1;
  }
  ppVoice = g_aBrSndBankVoice;
  do {
    pVoice = (int)*ppVoice;
    if (pVoice != 0) {
      (*(dsbuf_fn2 *)(**(int **)(pVoice + 0x9c) + 0x3c))
        (*(int *)(pVoice + 0x9c), -10000);
      (*(dsbuf_fn2 *)(**(int **)(pVoice + 0x9c) + 0x40))
        (*(int *)(pVoice + 0x9c), 0);
    }
    ppVoice = ppVoice + 1;
  } while ((int)ppVoice < (int)&g_aBrSndBankVoice[BR_SND_BANK_VOICES]);
  return 1;
}

/* WHAT IT DOES: set the volume on a DirectSound buffer and commit the change. */
/* @implements 0x1006B670 glide BrSndBufSetVolume */

int BrSndBufSetVolume(int param_1,int param_2)

{
  if (((BrSndG0B5DE8 != 0) && (BrSndPDS != 0)) && (BrSndG18290FC != 0)) {
    if (param_1 != 0) {
      *(int *)(param_1 + 0xc) = param_2;
      BrSndVoiceApplyFreq(param_1);
      return 1;
    }
    return 0;
  }
  return 1;
}

/* WHAT IT DOES: a 1 Hz service loop that never returns: read the elapsed-ms counter; if the
 * next second has not arrived, Sleep until it does, otherwise run the five once-a-second
 * steps and advance the deadline by 1000 ms. Both globals are unsigned (jb). */
/* @implements 0x1006A5F0 glide BrSecondTickLoop */

void BrSecondTickLoop(void)

{
  do {
    while( 1 ) {
      DAT_1184c070 = BrDelta_100713A0();
      if (DAT_1184c070 < DAT_11849ea8) break;
      FUN_1006a650();
      FUN_1006a7e0();
      FUN_1006aaf0();
      FUN_1006ab80();
      FUN_1006b0e0(&g_brP277B40);
      DAT_11849ea8 = DAT_11849ea8 + 1000;
    }
    Sleep(DAT_11849ea8 - DAT_1184c070);
  } while( 1 );
}

/* WHAT IT DOES: append node `param_2` to the singly linked list (next pointer at +0x1A8)
 * headed at `param_1`, clearing the new node's next and its +0x1C word. Returns 0. */
/* @implements 0x1006B3C0 glide BrSndListAppend */

int BrSndListAppend(int param_1,int param_2)
{
  *(int *)(param_2 + 0x1a8) = 0;
  *(int *)(param_2 + 0x1c) = 0;
  while (*(int *)(param_1 + 0x1a8) != 0) {
    param_1 = *(int *)(param_1 + 0x1a8);
  }
  *(int *)(param_1 + 0x1a8) = param_2;
  return 0;
}

/* 0x1006A5A0 BrSecondTickStart now lives in
 * src/core/startup/br_secondtick.c. */

extern int DAT_100bcbe8;
extern double _DAT_10077c40;
extern int BrG_6C7CB8;
int BrStrGet(int);
float BrVec3Dot(int, int);

/* WHAT IT DOES: check whether a car has come to rest facing the wrong way
 * and, if so, put the 'wrong way' warning on screen and count it. Only
 * applies while the car is below a speed threshold and not already flagged. */
/* @implements 0x1006EB00 glide FUN_1006eb00 */
/* auto-filed from ghidra --refine; transforms: as-is */

void __fastcall FUN_1006eb00(int param_1)

{
  int iVar1;
  int iVar2;
  
  if (BrG_6C7CB8 != 0) {
    if ((*(int *)(param_1 + 0xfa8) < DAT_100bcbe8) && (*(int *)(param_1 + 0xf7c) == 0) &&
        ((double)BrVec3Dot(param_1 + 0xf94, param_1) < _DAT_10077c40)) {
      iVar1 = BrStrGet(0xf3);
      iVar2 = *(int *)(param_1 + 0x29b8) + 1;
      *(int *)(param_1 + 0x29b8) = iVar2;
      if ((iVar2 > 0x1f) && ((iVar2 & 0x10) == 0x10)) {
        if (*(int *)(param_1 + 0xffc) != 0) {
          return;
        }
        *(int *)(param_1 + 0xffc) = iVar1;
        *(int *)(param_1 + 0x1004) = 0;
        *(int *)(param_1 + 0x1000) = 0x3e800000;
        return;
      }
      if (*(int *)(param_1 + 0xffc) != iVar1) {
        return;
      }
      *(int *)(param_1 + 0x1004) = 0;
      *(int *)(param_1 + 0xffc) = 0;
      return;
    }
    *(int *)(param_1 + 0x29b8) = 0;
  }
  return;
}


/* 0x10068600 FUN_10068600 now lives in src/core/driving/br_wheelvel.c. */

#endif /* BR_MATCHING_BUILD */
