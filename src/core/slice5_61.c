/* slice5_61.c -- decompiled from BRD3D.dll, pass-61 packet (slice 5).
 *
 * See slice5_61.h for the full inventory, including the six addresses in this
 * packet that turned out to be ALREADY IMPLEMENTED under a different name and
 * the six that are not tractable.
 *
 * FLOAT CONSTANTS -- read out of the images' .rdata with tools/pe.py, not
 * guessed:
 *     BRD3D    0x1008F3EC =  0.25f     (viewport scale/translate, 2.2 fixed)
 *     BRD3D    0x1008F3F0 = -0.25f     (the Y SCALE only)
 *     BRGlide  0x1007740C =  0.25f     (ALL FOUR -- Glide has no -0.25f here)
 *     BRGlide  0x100A7518 =  480       (an INT, `fild`: the screen height)
 * See BrGbiCall10024260, which now transcribes the Glide arithmetic.
 */
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include "slice5_61.h"

#include <string.h>

#include "slice1_03.h"   /* BrTextState / BrTextGetState, BR_TEXT_ALIGN_* */
#include "slice5_63.h"   /* g_br4B035C -- raw text-align global */
#include "slice2_15.h"   /* BrRdpRegs / BrRdpGetRegs -- 0x104BBF08 etc.   */
#include "slice2_25.h"   /* the option globals, index tables and callees   */

/* ==========================================================================
 * Storage this file owns
 * ========================================================================== */

/* 0x1003D0B0, see the note in the header. NULL until integration wires
 * it; BrSub1003CE80 then behaves as if the call failed without writing. */
int32_t (*g_brPfn1003D0B0)(void *pObj, void **ppvOut) = NULL;

/* See the DEVIATION in BrGbiCall10024260. NULL means "reinterpret w1". */
const void *(*g_brPfnDerefW1)(uint32_t w1) = NULL;

/* 0x104BC198. slice2_15.h's BrRdpRegs gathers the OTHER three viewport
 * globals (0x104BBF08, 0x104C0BB0, 0x104C0BB8) but not this one, because
 * nothing in that packet writes it. Owned here. */
float g_br4BC198;

/* 0x118ABDE0 -- .bss, therefore zero at load. */
static uint64_t g_br61Counter18ABDE0;

uint64_t *BrTimeNowCounter(void)
{
    return &g_br61Counter18ABDE0;
}

/* ==========================================================================
 * Byte-wise access to objects that reach past the models in the existing
 * headers (the same escape hatch slice3_31.c uses).
 * ========================================================================== */

static void Br61St32(void *pBase, size_t off, int32_t v)
{
    memcpy((unsigned char *)pBase + off, &v, sizeof(v));
}

static int32_t Br61Ld32(const void *pBase, size_t off)
{
    int32_t v;
    memcpy(&v, (const unsigned char *)pBase + off, sizeof(v));
    return v;
}

/* ==========================================================================
 * 0x10042AF0  `mov eax, 1 / ret`
 * ========================================================================== */

void BrGfx42AF0_1(void *p0)
{
    (void)p0;   /* genuinely never read; see the CONFLICT note in the header */
}

/* ==========================================================================
 * 0x10060E90 -> 0x10078C10  the fake monotonic timer
 * ========================================================================== */

#define BR61_TIME_STEP  0x0017D784u   /* 1,562,500 */

int32_t BrTimeNow(void)
{
    g_br61Counter18ABDE0 += BR61_TIME_STEP;
    return (int32_t)(uint32_t)(g_br61Counter18ABDE0 & 0xFFFFFFFFu);
}

/* ==========================================================================
 * The two "advance to the next selectable index" sweeps
 *
 * slice2_22.h ports the FIRST of these out of 0x1003CE80 as
 * BrDPlayAdvanceAvail(pCaps, pIdx). It is not reused here because its
 * availability predicate takes a caps pointer, whereas 0x1003CE80 and
 * 0x1003E510 call 0x1003F320 / 0x1003F2B0 with a bare index -- which is the
 * form slice2_25.h declares. Both sweeps are byte-identical apart from the
 * predicate and the upper bound.
 * ========================================================================== */

/* 0x1003CF3E..0x1003CF64 and 0x1003E543..0x1003E569, upper bound 0x1F. */
static void Br61AdvanceTrack(void)
{
    int32_t start = g_br0AC654;

    if (BrSub1003F320(start) != 0)
        return;                        /* the start index is tested FIRST */

    for (;;) {
        if (++g_br0AC654 > 0x1F)
            g_br0AC654 = 0;
        /* NOTE: unlike the car sweep below, the wrap FALLS INTO this test,
         * so a full circle really does end the search. */
        if (g_br0AC654 == start)       /* full circle: give up, no signal */
            return;
        if (BrSub1003F320(g_br0AC654) != 0)
            return;
    }
}

/* 0x1003E5D5..0x1003E60C. The bound is 14 when 0x10AA28FC is set, else 11. */
static void Br61AdvanceCar(void)
{
    int32_t start = g_br0AC648;
    int32_t limit;

    if (BrSub1003F2B0(start) != 0)
        return;

    for (;;) {
        /* `neg edx / sbb edx,edx / and edx,3 / add edx,0xB` */
        limit = (g_brAA28FC != 0 ? 3 : 0) + 0xB;

        if (++g_br0AC648 > limit) {
            g_br0AC648 = 0;
            /* GOTCHA (reproduced, and slice2_25.c records the same thing for
             * 0x10042EE0): the wrap path JUMPS PAST the full-circle test, so
             * index 0 is probed twice -- and if the sweep STARTED at 0 with
             * nothing selectable, the full-circle test is never reachable and
             * the loop does not terminate. That hang is in the original. */
        } else if (g_br0AC648 == start) {
            return;
        }

        if (BrSub1003F2B0(g_br0AC648) != 0)
            return;
    }
}

/* ==========================================================================
 * 0x1003CE80  adopt the host's session settings
 * ========================================================================== */

/* DPSESSIONDESC2, extended past slice2_25.h's BrDPSessionDesc (which stops at
 * +0x30 because that is all its packet needed). The four dwUser fields at
 * +0x40..+0x4C and the name pointer at +0x30 are what 0x1003CE80 reads, and
 * their offsets are exactly stock DPSESSIONDESC2, which corroborates
 * slice2_25.h's prefix. Kept file-local so it cannot collide. */
typedef struct Br61DPSessionDesc2 {
    uint32_t      dwSize;              /* +0x00 */
    uint32_t      dwFlags;             /* +0x04 */
    unsigned char aGuids[0x20];        /* +0x08 guidInstance+guidApplication */
    uint32_t      dwMaxPlayers;        /* +0x28 */
    uint32_t      dwCurrentPlayers;    /* +0x2C */
    char         *lpszSessionNameA;    /* +0x30 */
    char         *lpszPasswordA;       /* +0x34 */
    uint32_t      dwReserved1;         /* +0x38 */
    uint32_t      dwReserved2;         /* +0x3C */
    int32_t       dwUser1;             /* +0x40 */
    int32_t       dwUser2;             /* +0x44 */
    int32_t       dwUser3;             /* +0x48 */
    int32_t       dwUser4;             /* +0x4C */
} Br61DPSessionDesc2;

/* The original's two-step release, verbatim. */
static void Br61GlobalRelease(void *pv)
{
    BrGlobalUnlock(BrGlobalHandle(pv));
    BrGlobalFree(BrGlobalHandle(pv));
}

void BrSub1003CE80(void)
{
    Br61DPSessionDesc2 *pDesc;
    void               *pv = NULL;
    int32_t             hr;

    if (g_brP277B40 == NULL) {
        /* return 0x88770082 -- a DirectPlay HRESULT. Every caller declares
         * this function void, so the value is dropped here too. */
        return;
    }

    /* The original also zeroes 0x50 bytes of stack below the out-pointer and
     * then never touches them; that is dead and is not reproduced. */
    hr = (g_brPfn1003D0B0 != NULL)
             ? g_brPfn1003D0B0(g_brP277B40, &pv)
             : -1;

    if (hr < 0) {
        /* 0x1003D0B0 leaves the out-parameter untouched on every failure
         * path, which is why the local was zeroed first. */
        if (pv != NULL)
            Br61GlobalRelease(pv);
        return;                        /* returns hr in the original */
    }

    pDesc = (Br61DPSessionDesc2 *)pv;

    g_br0AC648 = pDesc->dwUser1;
    g_br0B380C = pDesc->dwUser1;
    g_brAA2A00 = pDesc->dwUser2;
    g_br22B350 = pDesc->dwUser2;
    g_brAA2A18 = pDesc->dwUser3;
    g_br0BD3E0 = pDesc->dwUser4;
    g_br0AC658 = pDesc->dwUser4;

    BrSub10044540();
    Br61AdvanceTrack();

    if (pDesc->lpszSessionNameA != NULL) {
        /* `repne scasb` + `rep movsd/movsb`, i.e. an unbounded strcpy. The
         * true size of 0x10A9D018 is not established (the next referenced
         * buffer, 0x10A9D078, is 0x60 bytes later), so no clamp is applied
         * and the original's overflow hazard is preserved. */
        strcpy(g_aBrA9D018, pDesc->lpszSessionNameA);
    }

    Br61GlobalRelease(pv);
    /* returns 0 in the original */
}

/* ==========================================================================
 * 0x1003E510  mode selection / derived-global refresh
 * ========================================================================== */

/* WHAT IT DOES: refreshes all the derived settings after the player has
 * changed something in the options: it looks up the chosen track, car and
 * half a dozen other choices in their respective tables and publishes the
 * values the rest of the game reads. It also nudges the track and car
 * selections forward to the next one that is actually available, so a locked
 * choice cannot be left selected. */
/* @implements 0x1003E510 d3d BrSub1003E510 */
#ifdef BR_MATCHING_BUILD
/* The two sweeps are INLINE in the original -- 99 instructions against 47
 * with them factored out, which is the whole 137-byte gap. They stay as
 * static helpers for their other caller (0x1003E4?? above); MSVC declines to
 * inline a static with two call sites, so this arm spells both out. Their
 * shape notes, including the non-terminating wrap in the car sweep, are on
 * the helpers themselves. */
void BrSub1003E510(void)
{
    int32_t start;

    BrSub1003E3A0();
    g_br094350 = g_br0AC65C;

    if (g_br0AA010 == 6)
        BrSub10044540();

    /* ---- Br61AdvanceTrack, inline ----
     * The probe takes the GLOBAL, not the saved start: the original loads it
     * once into eax, pushes that, and copies eax to esi for the start. Passing
     * the local instead loses the copy. */
    start = g_br0AC654;
    if (BrSub1003F320(g_br0AC654) == 0) {
        for (;;) {
            if (++g_br0AC654 > 0x1F)
                g_br0AC654 = 0;
            if (g_br0AC654 == start)
                break;
            if (BrSub1003F320(g_br0AC654) != 0)
                break;
        }
    }

    g_br22B34C = g_aBrAC420[g_br0AC654];
    g_br09435C = g_aBrAC4A0[g_br0AC64C];
    g_br094358 = g_aBrAC4B0[g_br0AC650];
    g_br094354 = g_aBrAC518[g_brAA2A08];

    if (g_br0AA010 != 0) {
        /* ---- Br61AdvanceCar, inline ---- */
        start = g_br0AC648;
        if (BrSub1003F2B0(g_br0AC648) == 0) {
            for (;;) {
                int32_t limit = (g_brAA28FC != 0 ? 3 : 0) + 0xB;

                if (++g_br0AC648 > limit) {
                    g_br0AC648 = 0;
                } else if (g_br0AC648 == start) {
                    break;
                }
                if (BrSub1003F2B0(g_br0AC648) != 0)
                    break;
            }
        }

        g_br0B380C = g_aBrAC4D8[g_br0AC648];
        g_br0BD3E0 = g_br0AC658;
        g_br22B350 = g_aBrAC4C0[g_brAA2A00];
    } else {
        /* The original loads the DWORD at 0x10AA26F4 and uses byte 0 and
         * byte 1 of it:  index = byte1 + 12 * byte0.
         * DEVIATION: decoded from the two bytes directly instead of from a
         * dword, so the result does not depend on host endianness. */
        size_t idx = (size_t)g_brAA26F5 + 12u * (size_t)g_brAA26F4;

        /* The DOUBLED index in its own local: written `[idx*2]` and
         * `[idx*2+1]` inline, VC5 folds the x2 into the SIB scale
         * (`[eax*2 + base]`); the original materialises it with one
         * `shl eax,1` and then indexes `[eax+base]` / `[eax+base+1]`. */
        size_t k = idx * 2u;

        g_br0B380C = g_aBr0B3820[k];       /* zero-extended, `mov cl` */
        g_br22B350 = g_aBr0B3820[k + 1u];  /* zero-extended, `mov dl` */
    }

    BrSub1005FCF0();
}
#else
void BrSub1003E510(void)
{
    BrSub1003E3A0();
    g_br094350 = g_br0AC65C;

    if (g_br0AA010 == 6)
        BrSub10044540();

    Br61AdvanceTrack();

    g_br22B34C = g_aBrAC420[g_br0AC654];
    g_br09435C = g_aBrAC4A0[g_br0AC64C];
    g_br094358 = g_aBrAC4B0[g_br0AC650];
    g_br094354 = g_aBrAC518[g_brAA2A08];

    if (g_br0AA010 != 0) {
        Br61AdvanceCar();

        g_br0B380C = g_aBrAC4D8[g_br0AC648];
        g_br0BD3E0 = g_br0AC658;
        g_br22B350 = g_aBrAC4C0[g_brAA2A00];
    } else {
        size_t idx = (size_t)g_brAA26F5 + 12u * (size_t)g_brAA26F4;

        g_br0B380C = g_aBr0B3820[idx * 2u + 0u];
        g_br22B350 = g_aBr0B3820[idx * 2u + 1u];
    }

    BrSub1005FCF0();
}
#endif

/* ==========================================================================
 * 0x10042410  commit the edited name into record 0x100AB3F4
 * ========================================================================== */

/* WHAT IT DOES: commits an edited name into the record it belongs to, saving
 * the record's old name first so it can be put back. It also flips a flag on
 * the record, and the flip is written in an odd way: it reads the flag,
 * writes the opposite, then reads it back, so what gets published is always
 * 0 or 1 rather than whatever the flag held before, and the name is only
 * committed when the flag had previously been clear. */
/* port-only body; Glide match is src/core/generated/0x1003B970.c */
int32_t BrExt_10042410(void *pArg)
{
    unsigned char *pRec;
    int32_t        flag;

    /* pArg->pSub (+0x2AE8) then that object's +0x70. slice2_25.h's BrGameSub
     * model stops at +0x68, so the store goes through raw bytes. */
    Br61St32(((BrGameObj *)pArg)->pSub, 0x70, 0);

    /* n * 3 * 5 * 9 * 8 == n * 0x438. g_br0AB3F4 is signed and is set to -1
     * by the name-reset paths, which the original does not guard against. */
    pRec = g_brPAA29D0 + (ptrdiff_t)g_br0AB3F4 * (ptrdiff_t)BR61_REC29D0_STRIDE;

    /* Read-modify-write of the SAME slot: it becomes "it used to be zero". */
    flag = Br61Ld32(pRec, BR61_REC29D0_OFF_FLAG);
    Br61St32(pRec, BR61_REC29D0_OFF_FLAG, (flag == 0) ? 1 : 0);

    /* ...and then it is read back and published. Because of the store above
     * this is ALWAYS 0 or 1 -- never the original value. */
    flag = Br61Ld32(pRec, BR61_REC29D0_OFF_FLAG);
    g_brAA28D8 = flag;

    if (flag != 0) {
        char *pszName = (char *)pRec + BR61_REC29D0_OFF_NAME;

        strcpy(g_aBrA9D078, pszName);      /* save the record's name    */
        strcpy(pszName, g_aBr39B720);      /* install the edited name   */
    }

    return 1;
}
