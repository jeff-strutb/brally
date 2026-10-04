/* br_menucb.c -- menus: the front end's per-row callbacks. Captions, values,
 * lap-time text, the entry/exit hooks and the flag setters that the menu
 * pages install on their rows.
 *
 * This is the whole of the former slice2_24.c, moved rather than split: all
 * 35 of the packet's functions are menu callbacks, and they share one static
 * module state (g_menu), the caption tables and the text/format helpers. Five
 * of the 35 do not match yet; splitting the matched 30 out would have meant
 * two copies of g_menu, which is a behaviour change, so the batch moved whole
 * and the address range stopped being the file's identity.
 *
 * Original range: BRD3D.dll 0x10040450-0x10042740. See slice2_24.h. */

/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include "br_coretypes.h"   /* br_globals: its objects */
#include "br_race.h"   /* br_globals: its objects */
#include "slice2_25.h"   /* br_globals: its objects */
#include "slice3_41.h"   /* br_globals: its objects */
#include "slice2_24.h"

#include <stdio.h>
#include <string.h>

/* =====================================================================
 * 0. Primitives the original reaches through the CRT
 * ===================================================================== */

/* 0x1007C8A0 is MSVC's __ftol: it forces the x87 rounding mode to chop,
 * does `fistp qword`, and returns the LOW dword of the 64-bit result.
 *
 * DEVIATION: a plain (int32_t) cast is undefined once the value leaves
 * int32 range, and the original is well defined there -- x87 stores the
 * "integer indefinite" 0x8000000000000000 when the value does not fit in 64
 * bits, whose low dword is zero, and otherwise simply drops the high dword. */
static int32_t BrFtol(double d)
{
    int64_t  wide;
    uint32_t lo;
    int32_t  out;

    if (!(d >= -9223372036854775808.0) || !(d < 9223372036854775808.0))
        return 0;

    wide = (int64_t)d;
    lo   = (uint32_t)((uint64_t)wide & 0xFFFFFFFFu);
    memcpy(&out, &lo, sizeof out);
    return out;
}

/* The original sign-extends with `movsx`; spelled out so the result does not
 * depend on whether plain `char` is signed here. */
/* (port-only BrSext8 removed) */


/* DEVIATION: the original strcpy()s / strcat()s into unbounded buffers.
 * These two truncate instead.  Everything else about them matches. */
/* (port-only BrStrCopy removed) */


/* (port-only BrStrCat removed) */


/* 0x1007F240 is MSVC's _strupr.  With a single-byte locale (the only path
 * this build ever takes -- 0x118AC360 is the multibyte flag and it is zero)
 * it walks the string comparing each byte with `cmp cl,0x61` / `jl` and
 * `cmp cl,0x7a` / `jg`, i.e. SIGNED byte comparisons, so bytes >= 0x80 are
 * negative and are left alone.  It uppercases in place and returns its
 * argument -- which matters at 0x10041300, where the argument is a string
 * TABLE entry, so the table is permanently uppercased by the first call. */
/* WHAT IT DOES: turns a piece of text into capitals, in place. The menus put
 * their values through this before showing them, which has a side effect worth
 * knowing: one caller hands it a shared string-table entry, so the first time
 * that row is drawn the stored wording itself is permanently capitalised for
 * everyone who reads it afterwards. */
/* @d3donly 0x1007F240 BrStrUpr -- absent from BRGlide (D3D-only / dynamically-imported CRT); no Glide twin exists */
static char *BrStrUpr(char *psz)
{
    char *p = psz;

    if (p == NULL)                     /* DEVIATION: the original would fault */
        return NULL;
    while (*p != '\0') {
        signed char c = (signed char)*p;
        if (c >= 0x61 && c <= 0x7A)
            *p = (char)(c - 0x20);
        p++;
    }
    return psz;
}

/* 0x1008C000 is MSVC's _itoa; every call site in this packet passes base 10.
 *
 * This is a real three-argument call in the original's instruction stream --
 * `push 0xA; push <dest>; push <value>; call _itoa` -- so it has to stay one
 * here.  The wrapper this replaces took (dest, size, value) and was a bounded
 * snprintf -- a different callee with the arguments in a different order,
 * which put a different sequence on the stack and cost every text setter in
 * the packet its match.
 *
 * DEVIATION (unchanged in substance): _itoa is unbounded.  Every caller in
 * this packet hands it a 32-byte buffer and a value that is at most eleven
 * characters, so no call here can overrun; the size argument the wrapper took
 * was never doing any work. */
/* Orig is /MD: `call [__imp__itoa]` / `[__imp__strupr]` (FF 15), not E8. */
/* 64-bit core: _itoa is declared by the platform headers */
/* 64-bit core: _strupr is declared by the platform headers */
#define BrItoa _itoa
#define BrStrUpr _strupr

/* =====================================================================
 * 1. Module state
 * ===================================================================== */

/* The three indices below are the only globals in this packet that the image
 * ships with a non-zero value (0x100AC648 = 2, 0x100AC64C = 1,
 * 0x100AC650 = 1); everything else lands past the end of the DLL's
 * initialised data and therefore starts at zero. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

static BrMenuState g_menu = {
    NULL, NULL, NULL,           /* pTimes25A0, pTimes27A0, pTimes27FC */
    0,                          /* g0AA010 */
    2u,                         /* g0AC648 */
    1u,                         /* g0AC64C */
    1u                          /* g0AC650 */
};

/* (port-only BrMenuGetState removed) */


/* g_menu is the port's gathering of the menu module's scattered originals.
 * The matching build reads the fields below as the separate globals they are,
 * by their DAT_ names (the image gate resolves those from the address they
 * spell).  pSt is always &g_menu, so pSt->x and g_menu.x name the same object. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
#define MENU_g0AA010 (*(uint32_t *)&g_brRaceRules.mode)
/* 64-bit core: declared once, in br_globals.h or its struct's header */
#define MENU_g0AC648 (*(uint32_t *)&g_brIdx0ABDE8)
/* 64-bit core: declared once, in br_globals.h or its struct's header */
#define MENU_g0AC64C DAT_100abdec
/* 64-bit core: declared once, in br_globals.h or its struct's header */
#define MENU_g0AC650 DAT_100abdf0
/* 64-bit core: declared once, in br_globals.h or its struct's header */
#define MENU_g0BD3E0 (*(int32_t *)&g_CBE8)
/* 64-bit core: declared once, in br_globals.h or its struct's header */
#define MENU_g1782CD0 DAT_117a6030
/* 64-bit core: declared once, in br_globals.h or its struct's header */
#define MENU_g18ABDBC (*(uint32_t *)&DAT_118eeed4)
/* 64-bit core: declared once, in br_globals.h or its struct's header */
#define MENU_g220B24 (*(uint32_t *)((char *)&g_a220B20 + 0x4))
/* 64-bit core: declared once, in br_globals.h or its struct's header */
#define MENU_gA9D618 DAT_10ac46a0
/* 64-bit core: declared once, in br_globals.h or its struct's header */
#define MENU_gAA2518 g_aBrAA2518
/* 64-bit core: declared once, in br_globals.h or its struct's header */
#define MENU_gAA2844 (*(uint32_t *)&BrGlNavOff5B9C)
/* 64-bit core: declared once, in br_globals.h or its struct's header */
#define MENU_gAA287C (*(uint32_t *)&DAT_10ac5bd4)
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* declared as the rest of the file does */
#define MENU_gAA289C (*(uint32_t *)&(*(int32_t *)&g_5BF4))
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* declared as the rest of the file does */
#define MENU_gAA28A0 (*(uint32_t *)&g_brPhase5BF8)
/* 64-bit core: declared once, in br_globals.h or its struct's header */
#define MENU_gAA28A4 (*(uint32_t *)&g_brIdx5BFC)
/* 64-bit core: declared once, in br_globals.h or its struct's header */
#define MENU_gAA28A8 (*(uint8_t *)&DAT_10ac5c00)
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* declared as the rest of the file does */
#define MENU_gAA28AC (*(uint32_t *)&(*(int32_t *)&g_brIdx5C04))
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* declared as the rest of the file does */
#define MENU_gAA28B8 (*(uint8_t *)&(*(int8_t *)&DAT_10ac5c10))
/* 64-bit core: declared once, in br_globals.h or its struct's header */
#define MENU_gAA28C4 DAT_10ac5c1c
/* 64-bit core: declared once, in br_globals.h or its struct's header */
#define MENU_gAA28D0 DAT_10ac5c28
/* 64-bit core: declared once, in br_globals.h or its struct's header */
#define MENU_gAA28D8 (*(uint32_t *)&g_5C30)
/* 64-bit core: declared once, in br_globals.h or its struct's header */
#define MENU_gAA28E0 (*(uint32_t *)&g_5C38)
/* 64-bit core: declared once, in br_globals.h or its struct's header */
#define MENU_gAA28E4 (*(uint32_t *)&g_5C3C)
/* 64-bit core: declared once, in br_globals.h or its struct's header */
#define MENU_gAA28E8 (*(uint32_t *)&DAT_10ac5c40)
/* 64-bit core: declared once, in br_globals.h or its struct's header */
#define MENU_gAA2904 g_brPAA29B8
/* 64-bit core: declared once, in br_globals.h or its struct's header */
#define MENU_gAA2964 DAT_10ac5cbc
/* 64-bit core: declared once, in br_globals.h or its struct's header */
#define MENU_gAA2A00 (*(uint32_t *)&DAT_10ac5d58)
/* 64-bit core: declared once, in br_globals.h or its struct's header */
#define MENU_gAA2A08 DAT_10ac5d60
/* 64-bit core: declared once, in br_globals.h or its struct's header */
#define MENU_gAA2A0C (*(uint32_t *)&g_brKind5D64)
/* 64-bit core: declared once, in br_globals.h or its struct's header */
#define MENU_gAA2A1C (*(uint32_t *)&g_brSel5D74)
/* 64-bit core: declared once, in br_globals.h or its struct's header */
#define MENU_gAA2A20 DAT_10ac5d78
/* 64-bit core: declared once, in br_globals.h or its struct's header */
#define MENU_gAA2A24 (*(uint32_t *)&g_brSel5D7C)
/* 64-bit core: declared once, in br_globals.h or its struct's header */
#define MENU_gAA2A28 (*(uint32_t *)&g_brSel5D80)
/* 64-bit core: declared once, in br_globals.h or its struct's header */
#define MENU_gAA33E4 (*(uint32_t *)&DAT_10ac6744)
/* 64-bit core: declared once, in br_globals.h or its struct's header */
#define MENU_gACEE50 (*(int32_t *)&g_aBrRaceCar[0].lap)

/* =====================================================================
 * 2. The caption tables
 *
 * All of these live in initialised .data and nothing in the module writes
 * them, so they are snapshotted from the image.  Their extents are read off
 * the gaps between them, which is why 0x100AC550 and 0x100AC570 come out as
 * 16 entries each (the last four of both look like a separate tail, but they
 * are contiguous with the table and no code proves a shorter length).
 *
 * The original indexes these blind.  The caption one-liners now do too --
 * their guards were the whole reason they could not match.
 * ===================================================================== */

static const uint16_t k_AC550[16] = {
    0x0010, 0x0012, 0x0011, 0x001B, 0x001C, 0x0076, 0x0010, 0x0012,
    0x0011, 0x001B, 0x001C, 0x0076, 0x0000, 0x008F, 0x008F, 0x0000
};
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 0x100AC640 is a dword table but the code reads only its low word. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* One byte out of the stage table at 0x100B3810.  The original address is
 * 0x100B3820 + 2*(k + 12*e) + hi, and 0x100B3820 is the table base plus
 * 0x10, so the byte offset from the table base is 0x10 + 24*e + 2*k + hi.
 * e is SIGNED (it comes from a movsx), so it really can point backwards. */
/* (port-only BrMenuStageByte removed) */


/* The record index the whole "current stage" family derives.  0x10040730,
 * 0x100407E0 and 0x10040C00 all reach for it. */
/* (port-only BrMenuStageIndex removed) */


/* =====================================================================
 * 3. Item plumbing
 * ===================================================================== */

/* The two tails that follow a text assignment.  The original tests the text
 * pointer between the two vtable calls -- but that pointer is `pItem +
 * 0x2B65`, an interior address that can only be NULL when pItem itself is,
 * so the second call always happens.  The test is documented rather than
 * reproduced because writing it out is a tautology the compiler rejects.
 *
 * DEVIATION: pVtbl is checked for NULL.  The original dereferences it
 * unconditionally. */
/* (port-only BrMenuStoreCaption removed) */


/* (port-only BrMenuStoreValue removed) */


/* =====================================================================
 * 4. The lap-time formatter
 * ===================================================================== */

/* Inlined verbatim at 0x10040C00, 0x10040D70, 0x10040EE0, 0x10041040 and
 * 0x10041180.  Constants: 0x1008F65C = 0.0f, 0x1008F668 = 100.0f,
 * 0x1008F66C = 0.01f, 0x1008F670 = 0.016666667f, 0x1008F674 = 60.0f,
 * format string at 0x10094094 = "%d:%02d.%02d", literal at 0x100AD308 =
 * "--:--".
 *
 * The guard is `fcom 0.0 / fnstsw ax / test ah,0x41 / je <format>`: the jump
 * is taken only when both C3 and C0 are clear, i.e. only when the value is
 * strictly greater than zero.  Zero, negatives AND unordered (NaN) all fall
 * into "--:--".
 *
 * GOTCHA -- this is NOT integer division.  The seconds count is obtained as
 * ftol(centiseconds * 0.01f) and 0.01f is 0.00999999977648258..., so any
 * exact multiple of 100 centiseconds rounds DOWN one second: a lap time of
 * exactly 1.0 s renders as "0:00.100", not "0:01.00".  Real lap times are
 * never exact multiples of 10 ms, which is why the bug survived.  The
 * minutes count uses 0.016666667f, which errs the other way and is safe.
 *
 * The seconds value makes a round trip through a 32-bit float (`fst dword`)
 * before the minutes are taken off it, while the hundredths are taken from
 * the full-precision x87 copy; both are reproduced. */
/* (port-only BrMenuFormatLapTime removed) */


/* The tail shared by all five time callbacks and by 0x100415A0 / 0x10041670 /
 * 0x10041710 / 0x100417B0: if the formatted text came out empty the callback
 * returns with EAX still holding the zero the strlen scan left there, which
 * is the only way any of these ever reports failure.  It cannot actually
 * happen -- both branches of the formatter write something -- but the test
 * is in the binary, so it is kept. */
/* (port-only BrMenuStoreFormatted removed) */


/* =====================================================================
 * 5. Callbacks -- entry / exit
 * ===================================================================== */

/* 0x10040680 */
/* WHAT IT DOES: switches the game over into menu mode the first time it is
 * asked, wiping the keyboard, mouse and joystick state so that whatever was
 * being held during play does not immediately act on the menu. Once the game is
 * already in menu mode it does nothing at all. */
/* @implements 0x10040680 d3d BrMenuEnter */
int32_t BrMenuEnter(void)
{
    BrMenuState *pSt = &g_menu;

    if (MENU_gAA2844 == 0) {
        MENU_gAA28D8 = 1;
        MENU_gAA2844 = 1;
        MENU_gAA33E4 = 0;
        BrMenuSub1005FF30();
        BrMenuSub1005FF60();
        BrInputLatchUpdate();
    }
    return 1;
}

/* WHAT IT DOES: leave this menu for session-kind 2, tearing down two
 * related screens first if the player has reached the required stage. */
/* @implements 0x10041930 d3d BrMenuLeaveTo2 */
int32_t BrMenuLeaveTo2(void)
{
    BrMenuState *pSt = &g_menu;

    if (MENU_gACEE50 >= MENU_g0BD3E0) {
        Ctl3E0E0_fn(0);
        Ctl3E370_fn(0);
    }
    MENU_g0AA010 = 2;
    return 1;
}

/* WHAT IT DOES: write the name "AutoSave.brf" into the save slot, and
 * if that slot is empty, wipe the rest of its record so the next save
 * starts clean. */
/* @implements 0x10041B50 d3d BrMenuAutoSaveName */
void BrMenuAutoSaveName(void)
{
    uint8_t *p = (*(uint8_t * *)&g_aBrRaceCar[0].pEquip);
    char    *pszName = "AutoSave.brf";

    /* One pointer, 0x10ACED34.  The port used to thread a separate
     * gACED34_present flag AND a NULL check; the original is `mov edx,
     * [0x10ACED34]; test edx,edx; je ret`.  strcpy, not BrStrCopy -- the
     * original inlines the CRT copy into 0x11782CD0.  pszName hides the
     * length so that expansion is the generic repne-scasb form. */

    if (p == NULL)
        return;

    strcpy(MENU_g1782CD0, pszName);

    if (p[4] == 0 && p[5] == 0) {
        /* three `rep stosd` runs of 6, 0xC and 0x18 dwords.  The original
         * re-reads 0x10ACED34 between them, which is why the pointer is
         * refetched here even though nothing can have changed it. */
        memset(p + 0x06, 0, 6 * 4);
        p = (*(uint8_t * *)&g_aBrRaceCar[0].pEquip);
        memset(p + 0x1E, 0, 0xC * 4);
        p = (*(uint8_t * *)&g_aBrRaceCar[0].pEquip);
        memset(p + 0x50, 0, 0x18 * 4);
    }
    BrMenuSub100709A0();
}

/* =====================================================================
 * 6. Callbacks -- caption setters
 * ===================================================================== */

/* WHAT IT DOES: put the right piece of wording on a stage-dependent
 * caption -- championship vs the backup column, or a fixed options
 * index when that mode is on. */
/* DEAD 2026-09-09 (adds to the parked list below, all at 98 B RAW 3+3
 * REGNORM 0+0): a return local defined before the store (with and without
 * a word temp); a block-scoped word temp; a byte-offset pun store; a
 * pItem copy; a table-pointer local; the sibling's (int16_t)(uint16_t)
 * cast; a uint32 temp (+2); explicit __cdecl; register i; every slot in
 * the TU (47 of 59 compile).  Corpus: the byte-exact sibling
 * BrMenuCap0990 (+0x5, same file) spells the tail on a GLOBAL index --
 * the cx pairing needs the index load fused into the tail, unreachable
 * from a join-carried local.
 * @t4-pass 0x10039C70 1 2026-09-09 probes 10 bytes 98 insns 24 regions 1 rows 0 census yes  (hand, fn.py variants + corpus)
 * @t4-pass 0x10039C70 2 2026-09-09 probes 47 bytes 98 insns 24 regions 1 rows 0 census yes  (position sweep) */
/* @t3 0x10039C70 2026-09-09 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 98/98 insns 24/24 rows 0+0 regions 1 oracle UNCLASSIFIED
 * @t3-effort passes 2 zero-movement 1 2
 * residue is register colouring only: identical register-blind instruction
 * multiset (rows 0+0), 1 masked region;
 * every row pairs under t3.py's canonical classes.  Effort: 2 counted
 * @t4-pass passes (ledger lines above, zero movement on passes 1 and 2);
 * crank candidates and scores in build/match/crank.log, dead probes in the
 * comment block above.  Do not reopen before the end-grind. */
/* @implements 0x10040730 d3d BrMenuCap0730 */
int32_t BrMenuCap0730(BrMenuItem *pItem)
{
    uint32_t i;

    /* Written out.  The original tests 0x100AA010 with jne to g0AC648 and
     * falls into the stage path; the selector byte is tested before the
     * movsx so the flags survive lea.  Arms are duplicated so VC5 does not
     * hoist the movsx above that test. */
    if (MENU_g0AA010 == 0) {
        if (MENU_gAA28A8 != 0) {
            int32_t e3 = (int32_t)(int8_t)MENU_gAA28B8;
            e3 = e3 + e3 * 2;
            i = *((const uint8_t *)g_brStages
                  + 0x10 + 2 * (MENU_gAA28AC + (uint32_t)e3 * 4u));
        } else {
            int32_t e3 = (int32_t)(int8_t)MENU_gAA28B8;
            e3 = e3 + e3 * 2;
            i = *((const uint8_t *)g_brStages
                  + 0x10 + 2 * (MENU_gAA28A4 + (uint32_t)e3 * 4u));
        }
    } else {
        i = MENU_g0AC648;
    }
    /* RESIDUE (glide 0x10039C70, 13 masked byte-diffs, T3a): the original
     * loads the word into CX with pItem in EDX and `mov eax,1` scheduled
     * into the load/store gap; every probed spelling here loads into AX
     * (pItem in ECX, eax freed by the load).  Probed and dead: an early
     * return-value temp, a uint16 temp, a dword-pun store, and an
     * __inline set-caption helper (the inliner dissolves it).  The arms
     * and everything up to +0x4A are byte-exact.  Pure register pairing;
     * parked. */
    pItem->f1E20C = (int16_t)k_AC550[i];
    return 1;
}

/* 0x100407A0.  -2 is the reserved "leave this item alone" answer; nothing
 * else in the packet returns it. */
/* WHAT IT DOES: keeps the second, dimmed track picture on a race-setup row in
 * step with whichever track is currently chosen. Note that the number it
 * writes is a picture index into the game's image list, not a piece of
 * wording. While the menus are sitting idle it returns the reserved "leave this
 * row exactly as it is" answer instead of touching anything. */
/* @implements 0x100407A0 d3d BrMenuCap07A0 */
int32_t BrMenuCap07A0(BrMenuItem *pItem)
{
    /* BrMenuIsIdle, BrTabU16 and BrMenuSetCaptionId are all written out: the
     * original is one straight-line body with a single forward branch, and
     * every call setup the delegating form emits is a byte the original does
     * not spend.  The table read is `mov dx, word ptr [ecx*2 + tab]` -- a
     * plain 16-bit move, no extension and no bounds test. */
    if (MENU_gAA2904 == MENU_gAA2964 && MENU_gAA28E8 == 0)
        return -2;

    pItem->f1E20C = (int16_t)k_AC570[MENU_g0AC648];
    return 1;
}

/* WHAT IT DOES: the same kind of caption as 0x10040730, but from the
 * other byte of the stage word (the high byte of stage[e].f10[k]), and it
 * sits idle (returns "leave this row alone") while the menus are not being
 * used.  Outside the stage path it indexes the caption table with 0x10AA2A00
 * directly. */
/* @implements 0x100407E0 d3d BrMenuCap07E0 */
int32_t BrMenuCap07E0(BrMenuItem *pItem)
{
    if (MENU_gAA2904 == MENU_gAA2964 && MENU_gAA28E8 == 0)
        return -2;

    /* Three separate store-and-return exits.  VC5 cross-jumps them into the
     * one shared `movsx dx, [ecx + tab]` tail, and because the index is not
     * a join-carried local it lands in ecx (the byte is zero-extended with
     * xor ecx,ecx / mov cl) while the stage address stays in eax.  The
     * [e][k] row form keeps the shared e*3 as its own node, so the movsx and
     * the lea are both hoisted between the selector test and its je. */
    if (MENU_g0AA010 == 0) {
        if (MENU_gAA28A8 != 0) {
            pItem->f1E20C = k_AC590[((const uint8_t *)&g_brStages
                [(int8_t)MENU_gAA28B8].f10[MENU_gAA28AC])[1]];
            return 1;
        }
        pItem->f1E20C = k_AC590[((const uint8_t *)&g_brStages
            [(int8_t)MENU_gAA28B8].f10[MENU_gAA28A4])[1]];
        return 1;
    }
    pItem->f1E20C = k_AC590[MENU_gAA2A00];
    return 1;
}

/* 0x10040870 */
/* WHAT IT DOES: puts the right gearbox picture on the transmission row of the
 * car-setup screen -- the automatic-shift or the manual-shift artwork,
 * according to which the player has chosen. */
/* @implements 0x10040870 d3d BrMenuCap0870 */
int32_t BrMenuCap0870(BrMenuItem *pItem)
{
    /* NO BOUNDS TEST, and no delegation.  The original is four instructions --
     * `mov eax,[g]; mov edx,[esp+4]; movsx cx, byte ptr [eax+tab];
     * mov word ptr [edx+0x1E20C], cx` -- with no cmp/jae anywhere.  Routing
     * through BrMenuSetCaptionId/BrTabS8 and guarding the index both cost
     * bytes the original never spends, so the guard is dropped: an
     * out-of-range global reads past the table here exactly as it does there.
     * See the note on BrTabS8 for why the guarded helper still exists. */

    pItem->f1E20C = k_AC598[MENU_gAA2A08];
    return 1;
}

/* 0x10040890 */
/* WHAT IT DOES: puts the right tyre picture on the tyres row of the car-setup
 * screen -- wet, intermediate or dry artwork, according to the player's
 * choice. */
/* @implements 0x10040890 d3d BrMenuCap0890 */
int32_t BrMenuCap0890(BrMenuItem *pItem)
{
    /* NO BOUNDS TEST, and no delegation.  The original is four instructions --
     * `mov eax,[g]; mov edx,[esp+4]; movsx cx, byte ptr [eax+tab];
     * mov word ptr [edx+0x1E20C], cx` -- with no cmp/jae anywhere.  Routing
     * through BrMenuSetCaptionId/BrTabS8 and guarding the index both cost
     * bytes the original never spends, so the guard is dropped: an
     * out-of-range global reads past the table here exactly as it does there.
     * See the note on BrTabS8 for why the guarded helper still exists. */

    pItem->f1E20C = k_AC59C[MENU_g0AC64C];
    return 1;
}

/* 0x100408B0 */
/* WHAT IT DOES: puts the right suspension picture on the shocks row of the
 * car-setup screen -- soft, medium or hard artwork, according to the player's
 * choice. */
/* @implements 0x100408B0 d3d BrMenuCap08B0 */
int32_t BrMenuCap08B0(BrMenuItem *pItem)
{
    /* NO BOUNDS TEST, and no delegation.  The original is four instructions --
     * `mov eax,[g]; mov edx,[esp+4]; movsx cx, byte ptr [eax+tab];
     * mov word ptr [edx+0x1E20C], cx` -- with no cmp/jae anywhere.  Routing
     * through BrMenuSetCaptionId/BrTabS8 and guarding the index both cost
     * bytes the original never spends, so the guard is dropped: an
     * out-of-range global reads past the table here exactly as it does there.
     * See the note on BrTabS8 for why the guarded helper still exists. */

    pItem->f1E20C = k_AC5A0[MENU_g0AC650];
    return 1;
}

/* WHAT IT DOES: put a caption from a play-mode table on this row --
 * keyboard vs wheel vs joystick, that family of pictures. */
/* @implements 0x10040930 d3d BrMenuCap0930 */
int32_t BrMenuCap0930(BrMenuItem *pItem)
{
    /* NO BOUNDS TEST, and no delegation -- see BrMenuCap0870. */

    pItem->f1E20C = k_AC62C[MENU_gAA287C];
    return 1;
}

/* WHAT IT DOES: put a caption from a small table on this row, or a
 * hard-wired second entry when a related flag is clear. */
/* @implements 0x10040950 d3d BrMenuCap0950 */
int32_t BrMenuCap0950(BrMenuItem *pItem)
{
    /* Branch polarity is the original's, not the reader's: it tests the flag
     * and `je`s to the hard-wired entry, so the TABLE path is the one that
     * falls through.  Writing the guard the other way round (`if (flag == 0)
     * return const;`) inverts the jump and costs the match. */
    if (MENU_g18ABDBC != 0) {
        pItem->f1E20C = k_AC630[MENU_gAA2A1C];
        return 1;
    }
    pItem->f1E20C = k_AC630[1];
    return 1;
}

/* WHAT IT DOES: put a caption from another small table on this row --
 * only the low 16 bits of each table entry are the wording id. */
/* @implements 0x10040990 d3d BrMenuCap0990 */
/* @n64 0x8025F10C located */
int32_t BrMenuCap0990(BrMenuItem *pItem)
{
    /* NO BOUNDS TEST, and no delegation -- see BrMenuCap0870. */

    pItem->f1E20C = (int16_t)(uint16_t)k_AC640[MENU_gAA2A28];
    return 1;
}

/* 0x100409B0 */
/* WHAT IT DOES: puts the car-shadow picture on the video-options row -- a car
 * drawn with its shadow or without one, showing the player what the setting
 * they are about to change actually looks like. */
/* @implements 0x100409B0 d3d BrMenuCap09B0 */
int32_t BrMenuCap09B0(BrMenuItem *pItem)
{
    /* NO BOUNDS TEST, and no delegation -- see BrMenuCap0870. */

    pItem->f1E20C = k_AC634[MENU_gAA2A20];
    return 1;
}

/* WHAT IT DOES: put a caption from another small table on this row. */
/* @implements 0x100409D0 d3d BrMenuCap09D0 */
int32_t BrMenuCap09D0(BrMenuItem *pItem)
{
    /* NO BOUNDS TEST, and no delegation -- see BrMenuCap0870. */

    pItem->f1E20C = k_AC638[MENU_gAA2A24];
    return 1;
}

/* 0x10041870 */
/* WHAT IT DOES: puts the picture of the chosen input device on the input row --
 * a keyboard, a steering wheel, a joystick or a mouse. */
/* @implements 0x10041870 d3d BrMenuCap1870 */
int32_t BrMenuCap1870(BrMenuItem *pItem)
{
    /* NO BOUNDS TEST, and no delegation.  The original is four instructions --
     * `mov eax,[g]; mov edx,[esp+4]; movsx cx, byte ptr [eax+tab];
     * mov word ptr [edx+0x1E20C], cx` -- with no cmp/jae anywhere.  Routing
     * through BrMenuSetCaptionId/BrTabS8 and guarding the index both cost
     * bytes the original never spends, so the guard is dropped: an
     * out-of-range global reads past the table here exactly as it does there.
     * See the note on BrTabS8 for why the guarded helper still exists. */

    pItem->f1E20C = k_AC628[MENU_gAA2A0C];
    return 1;
}

/* =====================================================================
 * 7. Callbacks -- state seeding
 * ===================================================================== */

/* 0x100409F0 */
/* (port-only BrMenuSeedFrom25D4 removed) */


/* 0x10040A20.  GOTCHA: this is 0x100409F0's twin but 0x10AA28A4 is filled
 * from a ZERO-EXTENDED BYTE here (`mov dl, byte [0x10AA26F5]` into a
 * pre-zeroed edx) where the other copies a whole dword. */
/* (port-only BrMenuSeedFrom26F0 removed) */


/* WHAT IT DOES: use the primary caption column from now on, not the
 * backup.  Menu caption setters consult this byte.  Always reports
 * success. */
/* @implements 0x10037EE0 glide BrMenuClearAA28A8 */
/* @n64 0x8022F4DC located */
int32_t BrMenuClearAA28A8(void)
{
    MENU_gAA28A8 = 0;
    return 1;
}

/* WHAT IT DOES: use the backup caption column from now on.  Always
 * reports success. */
/* @implements 0x10037ED0 glide BrMenuSetAA28A8 */
int32_t BrMenuSetAA28A8(void)
{
    MENU_gAA28A8 = 1;
    return 1;
}

/* WHAT IT DOES: pick which stored lap-time the next time-caption reads:
 * 0, 1 or 2 index a times array; 3 means "use the live time instead".
 * Always reports success.  The four bodies differ only in the value. */
/* @implements 0x1003A820 glide BrMenuSetAA28D0_0 */
/* @n64 0x8021C6C4 located */
int32_t BrMenuSetAA28D0_0(void)
{
    MENU_gAA28D0 = 0;
    return 1;
}

/* WHAT IT DOES: the next time-caption should show stored slot 1. */
/* @implements 0x1003A830 glide BrMenuSetAA28D0_1 */
int32_t BrMenuSetAA28D0_1(void)
{
    MENU_gAA28D0 = 1;
    return 1;
}

/* WHAT IT DOES: the next time-caption should show stored slot 2. */
/* @implements 0x1003A840 glide BrMenuSetAA28D0_2 */
int32_t BrMenuSetAA28D0_2(void)
{
    MENU_gAA28D0 = 2;
    return 1;
}

/* WHAT IT DOES: the next time-caption should show the live time, not a
 * stored slot. */
/* @implements 0x1003A850 glide BrMenuSetAA28D0_3 */
int32_t BrMenuSetAA28D0_3(void)
{
    MENU_gAA28D0 = 3;
    return 1;
}

/* =====================================================================
 * 8. Callbacks -- text setters
 * ===================================================================== */

/* WHAT IT DOES: write the current stage number onto this row as plain
 * decimal.  While the menus are idle it leaves the row alone. */
/* @implements 0x100408D0 d3d BrMenuText08D0 */
int32_t BrMenuText08D0(BrMenuItem *pItem)
{
    BrMenuState *pSt = &g_menu;
    char        *psz;

    /* BrMenuIsIdle written out: the original tests both globals in line and
     * threads the fall-through straight into the body.  See BrMenuText0A50
     * for why pVtbl / pText are derived in an inner block rather than named
     * up here, and for the pointer test before the second poke. */
    if (MENU_gAA2904 == MENU_gAA2964 && MENU_gAA28E8 == 0)
        return 1;

    psz = pItem->text.sz;
    BrItoa(MENU_g0BD3E0, psz, 10);

    {
        const BrMenuTextVtbl *pVtbl = pItem->text.pVtbl;
        BrMenuText           *pText = &pItem->text;

        pVtbl->pfn08(pText);
        if (psz != NULL)
            pVtbl->pfn2C(pText);
    }
    return 1;
}

/* WHAT IT DOES: write a 1-based setting number onto this row (the value
 * plus one), through a scratch buffer, then refresh the row's text. */
/* @implements 0x10040A50 d3d BrMenuText0A50 */
int32_t BrMenuText0A50(BrMenuItem *pItem)
{
    BrMenuState *pSt = &g_menu;
    char        *psz;

    /* Transcribed inline, not routed through BrItoa/BrMenuStoreValue.  The
     * original spends no call here beyond sprintf: the copy into +0x2B65 is
     * MSVC's inline strcpy (repne scasb + rep movsd/movsb) and the two vtable
     * pokes are thiscall through a vtable pointer fetched ONCE.  `psz` is the
     * destination the original keeps in ebx and re-tests before the second
     * poke -- the tautological test this file used to only document; it is in
     * the original's instruction stream, so it is written out here.
     *
     * SHAPE MATTERS, not just content.  Naming pText/pVtbl at the top of the
     * function makes VC5 compute them BEFORE the sprintf and carry them
     * across the call in callee-saved registers, which costs a spill and an
     * extra `lea`.  The original derives every one of them from a freshly
     * reloaded pItem afterwards, so they live in the inner block below and
     * must stay there.
     *
     * The bounded BrStrCopy is gone from this path.  Both buffers are known
     * sizes (a 32-byte scratch holding at most an 11-character integer, into
     * a 256-byte field), so nothing here can overrun. */

    sprintf(MENU_gAA2518, "%d", (int)(MENU_gAA28A0 + 1u));

    psz = pItem->text.sz;
    strcpy(psz, MENU_gAA2518);

    {
        const BrMenuTextVtbl *pVtbl = pItem->text.pVtbl;
        BrMenuText           *pText = &pItem->text;

        pVtbl->pfn08(pText);
        if (psz != NULL)
            pVtbl->pfn2C(pText);
    }
    return 1;
}

/* WHAT IT DOES: the same 1-based number as 0x10040A50, from a different
 * setting word and a different scratch buffer. */
/* @implements 0x10040AC0 d3d BrMenuText0AC0 */
int32_t BrMenuText0AC0(BrMenuItem *pItem)
{
    BrMenuState *pSt = &g_menu;
    char        *psz;

    /* Transcribed inline, not routed through BrItoa/BrMenuStoreValue.  The
     * original spends no call here beyond sprintf: the copy into +0x2B65 is
     * MSVC's inline strcpy (repne scasb + rep movsd/movsb) and the two vtable
     * pokes are thiscall through a vtable pointer fetched ONCE.  `psz` is the
     * destination the original keeps in ebx and re-tests before the second
     * poke -- the tautological test this file used to only document; it is in
     * the original's instruction stream, so it is written out here.
     *
     * SHAPE MATTERS, not just content.  Naming pText/pVtbl at the top of the
     * function makes VC5 compute them BEFORE the sprintf and carry them
     * across the call in callee-saved registers, which costs a spill and an
     * extra `lea`.  The original derives every one of them from a freshly
     * reloaded pItem afterwards, so they live in the inner block below and
     * must stay there.
     *
     * The bounded BrStrCopy is gone from this path.  Both buffers are known
     * sizes (a 32-byte scratch holding at most an 11-character integer, into
     * a 256-byte field), so nothing here can overrun. */

    sprintf(MENU_gA9D618, "%d", (int)(MENU_gAA28A4 + 1u));

    psz = pItem->text.sz;
    strcpy(psz, MENU_gA9D618);

    {
        const BrMenuTextVtbl *pVtbl = pItem->text.pVtbl;
        BrMenuText           *pText = &pItem->text;

        pVtbl->pfn08(pText);
        if (psz != NULL)
            pVtbl->pfn2C(pText);
    }
    return 1;
}

/* WHAT IT DOES: write a labelled number on this row -- a stock caption,
 * two spaces, then the 1-based setting -- as the row's heading, not its
 * value. */
/* @implements 0x10040B30 d3d BrMenuText0B30 */
int32_t BrMenuText0B30(BrMenuItem *pItem)
{
    char *psz;
    char *pszSp = "  ";

    /* Transcribed inline.  Orig is sprintf into the A9D618 scratch (IAT),
     * then strcpy(StringById(0x37)) / strcat("  ") / strcat(scratch) into
     * the row text, then pfn04/pfn10.  Naming pText/pVtbl at the top of
     * the function makes VC5 compute them before sprintf (see 0A50). */
    sprintf(MENU_gA9D618, "%d", (int)(MENU_gAA28A4 + 1u));

    psz = pItem->text.sz;
    strcpy(psz, BrStrGet(0x37));
    strcat(psz, pszSp);
    strcat(psz, MENU_gA9D618);

    {
        const BrMenuTextVtbl *pVtbl = pItem->text.pVtbl;
        BrMenuText           *pText = &pItem->text;

        pVtbl->pfn04(pText);
        if (psz != NULL)
            pVtbl->pfn10(pText);
    }
    return 1;
}

/* The stage-indexed best-time lookup 0x10040C00 and 0x10040D70 share.
 * GOTCHA: unlike 0x10040730 this pair always takes the column from
 * 0x10AA28AC; the 0x10AA28A8 selector is not consulted. */
/* (port-only BrMenuStageTime removed) */


/* WHAT IT DOES: put a lap time from one stored table onto this row, or
 * "--:--" if times are not available yet. */
/* The GLIDE original (0x1003A140) INLINES every helper this callback's
 * portable spelling factored out -- the stage-byte lookup, the "--:--"
 * copy, the whole lap formatter and the store tail -- so the placed body
 * must spell them inline too: the image build cannot emit a call to a
 * function the original never linked.  Behaviour is the portable body's,
 * statement for statement; the cells are named by address (Ghidra
 * spelling), which the placement resolver treats as evidence.
 *   0x10AC5BF4 times-available flag   0x10AC5C10 stage index (int8)
 *   0x10AC5C04 stage-table bias       0x100B3028 stage byte table
 *   0x10AC5B54 the time table this callback reads (float[])            */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x10AC5BF4 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x10AC5C10 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x10AC5C04 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x100B3028 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x10AC5B54 */

/* @implements 0x10040C00 d3d BrMenuTime0C00 */
int32_t BrMenuTime0C00(BrMenuItem *pItem)
{
    char sz[32];                       /* the original's local is 0x20, zeroed */

    memset(sz, 0, sizeof sz);
    if ((*(int32_t *)&g_5BF4) != 0) {
        /* the stage-byte lookup, inline: table[(bias + 12*stage) * 2] */
        uint32_t i = (&(g_aBr0B3820[0]))[
            ((*(int32_t *)&g_brIdx5C04) + 12 * (int32_t)(*(int8_t *)&DAT_10ac5c10)) * 2];
        float    t = (&(g_brFTbl5B54[0]))[i];

        if (t > 0.0f) {
            /* the lap formatter, inline -- the same spelling as
             * BrMenuFormatLapTime, sprintf as the original imports it */
            int32_t nCenti, nSec, nHund, nMin, nSecOfMin;
            float   fSecStored;

            nCenti     = (int32_t)(t * 100.0f);
            nSec       = (int32_t)((float)nCenti * 0.01f);
            fSecStored = (float)nSec;
            nHund      = (int32_t)((float)nCenti - (float)nSec * 100.0f);
            nMin       = (int32_t)(fSecStored * 0.016666667f);
            nSecOfMin  = (int32_t)(fSecStored - (float)nMin * 60.0f);
            sprintf(sz, "%d:%02d.%02d",
                    (int)nMin, (int)nSecOfMin, (int)nHund);
        } else {
            strcpy(sz, "--:--");
        }
    } else {
        strcpy(sz, "--:--");
    }

    if (strlen(sz) == 0)
        return 0;
    /* the store tail, inline: upcase, copy into the row's text object and
     * poke its two caption vtable slots.  The original tests the DEST
     * pointer (an always-true lea) between the two calls; kept. */
    {
        BrMenuText *pText = &pItem->text;
        char       *pDst  = pText->sz;

        strcpy(pDst, _strupr(sz));
        pText->pVtbl->pfn04(pText);
        if (pDst != NULL)
            pText->pVtbl->pfn10(pText);
    }
    return 1;
}

/* WHAT IT DOES: the same lap-time readout as 0x10040C00, from a second
 * stored table. */
/* declared only (the Mac port keeps its own body in ports/macos/patch/); Glide match is src/core/menus/BrMenuTime0D70_1003A2B0.cpp */
/* BrMenuTime0D70: prototype in br_funcs.h */

/* WHAT IT DOES: put a lap time onto this row from a chosen slot -- 0, 1
 * or 2 index a table; 3 means the live time instead. */
/* declared only (the Mac port keeps its own body in ports/macos/patch/); Glide match is src/core/cpp/0x1003A420.cpp */
/* BrMenuTime0EE0: prototype in br_funcs.h */

/* (port-only BrMenuFillLapTime removed) */


/* WHAT IT DOES: format one stored lap time onto this row as m:ss.hh. */
/* declared only (the Mac port keeps its own body in ports/macos/patch/); Glide match is src/core/cpp/0x1003A580.cpp */
/* BrMenuTime1040: prototype in br_funcs.h */

/* WHAT IT DOES: format a second stored lap time onto this row as m:ss.hh. */
/* declared only (the Mac port keeps its own body in ports/macos/patch/); Glide match is src/core/cpp/0x1003A6D0.cpp */
/* BrMenuTime1180: prototype in br_funcs.h */

/* WHAT IT DOES: put this stage's name on the row, in capitals.  The
 * string-table entry itself is uppercased, so the next reader sees
 * capitals too. */
/* @implements 0x10041300 d3d BrMenuText1300 */
int32_t BrMenuText1300(BrMenuItem *pItem)
{
    int32_t e = 0;
    char   *psz;

    /* Two BrStringById calls: orig strlen-tests the first and copies the
     * uppercased second.  Index is a movsx of gAA28B8, not BrMenuStageIndex. */
    if (MENU_gAA289C != 0)
        e = (int32_t)(int8_t)MENU_gAA28B8;

    if (strlen(BrStrGet(g_brStages[e].f00)) == 0)
        return 0;

    psz = pItem->text.sz;
    strcpy(psz, BrStrUpr(BrStrGet(g_brStages[e].f00)));

    {
        const BrMenuTextVtbl *pVtbl = pItem->text.pVtbl;
        BrMenuText           *pText = &pItem->text;

        pVtbl->pfn04(pText);
        if (psz != NULL)
            pVtbl->pfn10(pText);
    }
    return 1;
}

/* WHAT IT DOES: show how many of something this stage still has left
 * (a count minus what the player has used), never below zero, as a
 * capitalised number. */
/* @implements 0x100415A0 d3d BrMenuText15A0 */
int32_t BrMenuText15A0(BrMenuItem *pItem)
{
    BrMenuState *pSt = &g_menu;
    char         sz[32];
    char        *psz;
    int32_t      v;

    /* Everything the original inlines is inlined: the record index (a plain
     * movsx of the signed byte at 0x10AA28B8, not a call to BrMenuStageIndex),
     * the clamp, and BrMenuStoreFormatted's whole body.  See BrMenuText1670
     * for the strlen test and the BrStrUpr return value, and BrMenuText0A50
     * for why the vtable loads live in an inner block.
     *
     * The branches yield the FIELD, not the index.  Written as
     * `e = cond ? 0 : idx;` followed by one `g_brStages[e].f08`, VC5 keeps a
     * generic index and does the load once at the join.  The original
     * specialises the zero case into an absolute `mov eax, [0x100B3818]` --
     * the table base plus 8 -- and only the indexed branch pays for the lea,
     * so each branch has to name the field itself. */
    memset(sz, 0, sizeof sz);

    if (MENU_gAA289C == 0)
        v = g_brStages[0].f08;
    else
        v = g_brStages[(int32_t)(int8_t)MENU_gAA28B8].f08;

    v -= MENU_gAA28C4;
    if (v < 0)
        v = 0;

    /* THE ONE REMAINING DIVERGENCE, and it is a single instruction.  The
     * original clamps with `sub eax, mem` / `test eax, eax` / `jge`; VC5
     * emits `sub` / `jns` here and drops the redundant `test`, because the
     * subtract already set the flags.  Everything either side of it is
     * byte-identical, sizes included.  Tried and did NOT move it: the ternary
     * form `v = (v >= 0) ? v : 0`.  Do not spend a third attempt on this
     * without a new idea -- it is scheduling, not shape.
     * DEAD 2026-09-09 (10 more, all identical): a tested copy local; an
     * (int32_t) cast; 0 > v; empty-then else; non-compound sub; negation;
     * braces; v-v and v^=v zero forms; `v != 0 && v < 0` gets test+jge
     * but pays an extra je (+2 B) -- the pair is reachable, the guard is
     * not; every slot in the TU; corpus MISS on sub/test/jge/xor. */

    BrItoa(v, sz, 10);

    if (strlen(sz) == 0)
        return 0;

    psz = pItem->text.sz;
    strcpy(psz, BrStrUpr(sz));

    {
        const BrMenuTextVtbl *pVtbl = pItem->text.pVtbl;
        BrMenuText           *pText = &pItem->text;

        pVtbl->pfn08(pText);
        if (psz != NULL)
            pVtbl->pfn2C(pText);
    }
    return 1;
}

/* WHAT IT DOES: write a 1-based setting number onto this row, in
 * capitals.  An empty string leaves the row untouched. */
/* @implements 0x10041670 d3d BrMenuText1670 */
int32_t BrMenuText1670(BrMenuItem *pItem)
{
    char  sz[32];
    char *psz;

    /* BrMenuStoreFormatted written out.  Two details are load bearing:
     * the emptiness test is a real strlen (the original inlines it as
     * repne scasb / not / dec / jne, which `sz[0] == 0` does not produce),
     * and the copy source is BrStrUpr's RETURN value, not sz -- the original
     * copies from the pointer the call leaves in eax.  See BrMenuText0A50
     * for the inner block and the pointer test. */

    memset(sz, 0, sizeof sz);
    BrItoa((int32_t)(MENU_gAA28A4 + 1u), sz, 10);

    if (strlen(sz) == 0)
        return 0;

    psz = pItem->text.sz;
    strcpy(psz, BrStrUpr(sz));

    {
        const BrMenuTextVtbl *pVtbl = pItem->text.pVtbl;
        BrMenuText           *pText = &pItem->text;

        pVtbl->pfn08(pText);
        if (psz != NULL)
            pVtbl->pfn2C(pText);
    }
    return 1;
}

/* WHAT IT DOES: write that setting as stored (not 1-based) onto this
 * row, in capitals. */
/* @implements 0x10041710 d3d BrMenuText1710 */
int32_t BrMenuText1710(BrMenuItem *pItem)
{
    char  sz[32];
    char *psz;

    /* BrMenuStoreFormatted written out.  Two details are load bearing:
     * the emptiness test is a real strlen (the original inlines it as
     * repne scasb / not / dec / jne, which `sz[0] == 0` does not produce),
     * and the copy source is BrStrUpr's RETURN value, not sz -- the original
     * copies from the pointer the call leaves in eax.  See BrMenuText0A50
     * for the inner block and the pointer test. */

    memset(sz, 0, sizeof sz);
    BrItoa(MENU_gAA28C4, sz, 10);

    if (strlen(sz) == 0)
        return 0;

    psz = pItem->text.sz;
    strcpy(psz, BrStrUpr(sz));

    {
        const BrMenuTextVtbl *pVtbl = pItem->text.pVtbl;
        BrMenuText           *pText = &pItem->text;

        pVtbl->pfn08(pText);
        if (psz != NULL)
            pVtbl->pfn2C(pText);
    }
    return 1;
}

/* WHAT IT DOES: the remaining-count readout of 0x100415A0, indexed from
 * a different stage word and without the "times unavailable" special case. */
/* @implements 0x100417B0 d3d BrMenuText17B0 */
int32_t BrMenuText17B0(BrMenuItem *pItem)
{
    BrMenuState *pSt = &g_menu;
    char         sz[32];
    char        *psz;
    int32_t      v;

    /* Everything the original inlines is inlined: the record index (a plain
     * movsx of the signed byte at 0x10AA28B8, not a call to BrMenuStageIndex),
     * the clamp, and BrMenuStoreFormatted's whole body.  See BrMenuText1670
     * for the strlen test and the BrStrUpr return value, and BrMenuText0A50
     * for why the vtable loads live in an inner block. */

    memset(sz, 0, sizeof sz);

    v = g_brStages[MENU_g220B24].f08 - MENU_gAA28C4;
    if (v < 0)
        v = 0;

    BrItoa(v, sz, 10);

    if (strlen(sz) == 0)
        return 0;

    psz = pItem->text.sz;
    strcpy(psz, BrStrUpr(sz));

    {
        const BrMenuTextVtbl *pVtbl = pItem->text.pVtbl;
        BrMenuText           *pText = &pItem->text;

        pVtbl->pfn08(pText);
        if (psz != NULL)
            pVtbl->pfn2C(pText);
    }
    return 1;
}

/* =====================================================================
 * 9. Callbacks -- flag pokers
 *
 * 0xFFFFEFEF is ~0x1010.  The two branches are NOT symmetric: clearing the
 * bits only touches +0x1C, while setting them also forces the string id to 2
 * and clears the byte at +0x2B64.
 * ===================================================================== */

/* 0x10041890 */
/* WHAT IT DOES: greys a menu row out, or brings it back, depending on whether
 * the thing it offers is currently available. Enabling only clears the two
 * "unavailable" bits; disabling also forces the row back to the plain grey
 * lettering and resets its text style, so the two directions are not mirror
 * images and a row switched off loses styling that switching it on does not
 * restore. */
/* @implements 0x10041890 d3d BrMenuFlags1890 */
int32_t BrMenuFlags1890(BrMenuItem *pItem)
{
    if (MENU_gAA28E0 != 0) {
        pItem->f1C &= 0xFFFFEFEFu;
    } else {
        pItem->f1C |= 0x1010u;
        pItem->f1E20C = 2;
        pItem->text.f08 = 0;
    }
    return 1;
}

/* WHAT IT DOES: if a related availability flag is set, un-grey this row
 * (clear the two "dimmed" bits).  If the flag is clear it does not
 * touch the row at all. */
/* @implements 0x100418D0 d3d BrMenuFlags18D0 */
/* @n64 0x80257A1C located */
int32_t BrMenuFlags18D0(BrMenuItem *pItem)
{
    if (MENU_gAA28E4 != 0)
        pItem->f1C &= 0xFFFFEFEFu;
    return 1;
}

/* 0x100418F0.  0x10041890 driven by 0x10AA28E8 instead. */
/* WHAT IT DOES: the same greying-out as its neighbour above, but driven by a
 * different availability flag, so it serves a different family of rows. */
/* @implements 0x100418F0 d3d BrMenuFlags18F0 */
int32_t BrMenuFlags18F0(BrMenuItem *pItem)
{
    if (MENU_gAA28E8 != 0) {
        pItem->f1C &= 0xFFFFEFEFu;
    } else {
        pItem->f1C |= 0x1010u;
        pItem->f1E20C = 2;
        pItem->text.f08 = 0;
    }
    return 1;
}

/* -- Ghidra-matched functions --------------------------- */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: duplicate declaration removed */
/* 64-bit core: declared once, in br_globals.h or its struct's header */


