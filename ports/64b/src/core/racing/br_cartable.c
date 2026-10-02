/* br_cartable.c -- racing: the car table and its save/restore.
 *
 * RESPONSIBILITY: racing/ -- the race itself: cars, entrants, results.
 *
 * Adds a car to the race table (0x1001C6A0), removes a leaving player's
 * cars (0x1001C7A0), and saves / restores each car's championship figures
 * around an event (0x1001C810, 0x1001C890).
 *
 * Filed out of the address batch slice2_17.c.  That file's preamble is
 * carried verbatim (its #ifdef blocks, includes, .rdata constants,
 * cross-slice declarations and small helpers -- the helper set changes
 * /O2 register choice, so it is kept whole); its state block g_s17 is
 * declared in slice2_17.h and defined there.
 */
/* slice2_17.h prototypes a list pointer the original never takes. */
#define BrPtrListContains BrPtrListContains_port
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include "br_race.h"   /* br_globals: its objects */
#include "slice3_41.h"   /* br_globals: its objects */
#include "slice2_17.h"
#undef BrPtrListContains

#include <math.h>
#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* .rdata constants, read from the DLL.                                */

/* 0x1008F448  dword 0x400F5C29 -- 2.24f, the m/s -> mph factor. */
#define BR_MPH_PER_MS      2.24f

/* 0x1008F4E0  0x3F91DF46A2529D39 -- pi/180. */
#define BR_DEG_TO_RAD      0.017453292519943295

/* 0x1008F4B0  0xC0545F30B4E4E30A and 0x1008F4B8 0xBFD45F30B4E4E30A.
 * Exactly 256x apart. Neither is the correctly rounded -256/pi or -1/pi;
 * the shipped values are used verbatim. */
#define BR_ANG_K256      (-81.48734781601175)
#define BR_ANG_K1        (-0.3183099524062959)

/* ------------------------------------------------------------------ */
/* Module state.                                                       */

/* g_s17 is declared in slice2_17.h and defined in slice2_17.c. */

/* ------------------------------------------------------------------ */
/* Cross-slice callees. Stand-ins live in the test file.               */

/* XSLICE 0x10008B80 */  /* a bare `ret` in this build -- see the contract */
extern void BrStub10008B80(intptr_t a0, ...);
/* XSLICE 0x10060E90 */
extern int   BrX10060E90(void);
/* XSLICE 0x100751D0
 * 0x1002C2A0 tail-jumps into it with the object in ecx and nothing on the
 * stack: it is a C++ __thiscall method. MSVC 5.0's C front end cannot spell
 * __thiscall, but for a single pointer argument __fastcall is byte-identical
 * at the call site (arg1 in ecx, no stack cleanup), so that is what the
 * matching build uses. Off MSVC the qualifier vanishes and it is an ordinary
 * one-argument function. */
#define BRS17_THISCALL __fastcall
/* BrX100751D0: prototype in br_funcs.h */
/* XSLICE 0x1002C2C0 */
/* BrX1002C2C0: prototype in br_funcs.h */
/* XSLICE 0x1003563A */
/* BrX1003563A: prototype in br_funcs.h */
/* XSLICE 0x100397C0 */
extern void  BrX100397C0(void);
/* XSLICE 0x10034C66 */
extern void  BrX10034C66(void (*pfn)(void));
/* XSLICE 0x1002C500 */
extern void  BrX1002C500(void);
/* XSLICE 0x10075F10 */
extern void  BrX10075F10(void *pThis);
/* XSLICE 0x100664C0 */
extern void  BrX100664C0(void *pThis);
/* XSLICE 0x10005DE0 */
/* BrX10005DE0: prototype in br_funcs.h */
/* XSLICE 0x10076AE0 */
/* BrX10076AE0: prototype in br_funcs.h */
/* XSLICE 0x10005E70 */
/* BrX10005E70: prototype in br_funcs.h */
/* XSLICE 0x10068260 */
/* BrX10068260: prototype in br_funcs.h */
/* XSLICE 0x10072580 */
/* BrX10072580: prototype in br_funcs.h */
/* XSLICE 0x10042AF0 */
extern void  BrX10042AF0(void *p, int a1, int a2);
/* XSLICE 0x10035BBA */
/* BrX10035BBA: prototype in br_funcs.h */
/* XSLICE 0x10069530 */
/* BrX10069530: prototype in br_funcs.h */
/* XSLICE 0x10069490 */
/* BrX10069490: prototype in br_funcs.h */
/* 0x1007E8B0 is the CRT's atexit (0x1007E820 wrapped, returning 0 or -1).
 * Anything at or above 0x1007CC40 is statically linked MSVC CRT, so the
 * platform's own atexit is used instead of porting it. */
/* BrXAtExit: prototype in br_funcs.h */

/* ------------------------------------------------------------------ */
/* Small helpers. Loads and stores go through memcpy so that the byte
 * offsets recovered from the disassembly stay valid without relying on
 * pointer casts being aligned.                                         */

/* (port-only s17_ld32 removed) */


/* (port-only s17_st32 removed) */


/* (port-only s17_ldf removed) */


/* (port-only s17_stf removed) */


/* g_6C0680 is advanced by 8 bytes and then the two words are written --
 * the original reads the cursor, bumps the global, and only then stores. */
#define s17_emit(w0_, w1_)                                              \
    do {                                                                \
        uint32_t *p_ = g_s17.pGfx;                                      \
                                                                        \
        g_s17.pGfx = p_ + 2;                                            \
        p_[0] = (w0_);                                                  \
        p_[1] = (w1_);                                                  \
    } while (0)

/* DEVIATION: the original stores raw 32-bit pointers into the display list
 * (`mov [eax+4], esi`). On a 64-bit host that cannot round-trip, so the low
 * 32 bits are stored, exactly as the original would have. Consumers of the
 * stream in this port must not dereference these words. */
#define s17_ptrword(p_)   ((uint32_t)(uintptr_t)(const void *)(p_))

/* (port-only s17_car removed) */


/* 0x1002F130 */
/* WHAT IT DOES: adds a car to the race: fetches its colour and its driver's
 * name from the owning player, registers its engine sound, and files it in the
 * car table. The counter is re-read between almost every step rather than being
 * kept, which matters because the owner is recorded against the slot the
 * counter held BEFORE it was bumped. */
/* @implements 0x1002F130 d3d BrCarTableAdd */
/* 64-bit core: declared once, in br_globals.h or its struct's header */           /* nEntB */
/* 64-bit core: declared once, in br_globals.h or its struct's header */           /* nEntA */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* car0 base */
/* Glide arm, hand-transcribed from 0x1001C6A0.  Every field is addressed as
 * `&cars[count].field` straight off the counter, so the record base folds into
 * each displacement (`lea ecx,[eax+0x10af3bb6]`); a `car` pointer local keeps
 * the base in a register instead.  0x1006FD50 is __thiscall with one stack
 * argument: a __fastcall whose second parameter is a one-int struct passes
 * ecx = car and leaves edx alone, exactly as BrEntitySetIndex is defined. */
typedef struct BrS17CarRec { unsigned char b[BR_CAR_STRIDE]; } BrS17CarRec;
typedef struct { int n; } BrS17EntArg;
typedef void (__fastcall *BrS17EntSetFn)(void *pThis, BrS17EntArg a0);
#define BR_S17_CARS ((BrS17CarRec *)(*(unsigned char (*)[])&g_aBrRaceCar))
void BrCarTableAdd(void *pOwner)
{
    BrS17EntArg a;

    a.n = BrNetSlotGetF030(pOwner,
                      &BR_S17_CARS[g_BrCarCount].b[BR_CAR_OFF_RGB + 0],
                      &BR_S17_CARS[g_BrCarCount].b[BR_CAR_OFF_RGB + 1],
                      &BR_S17_CARS[g_BrCarCount].b[BR_CAR_OFF_RGB + 2]);
    ((BrS17EntSetFn)BrEntitySetIndex)(&BR_S17_CARS[g_BrCarCount], a);
    strcpy((char *)&BR_S17_CARS[g_BrCarCount].b[BR_CAR_OFF_NAME],
           BrNetSlotName(pOwner));
    BrSfxCarBankInit(g_BrCarCount,
                *(uint32_t *)&BR_S17_CARS[g_BrCarCount].b[BR_CAR_OFF_TAG]);
    *(void **)&BR_S17_CARS[g_BrCarCount++].b[BR_CAR_OFF_OWNER] = pOwner;
    g_brRaceNDriver++;
}

/* 0x1002F230 */
/* WHAT IT DOES: takes a player's car out of the race when that player leaves.
 * It marks the car inactive, silences its engine, and clears it out of every
 * slot that was pointing at it. It scans the whole table rather than stopping
 * at the first match, so a player owning more than one car loses all of
 * them. */
/* @implements 0x1002F230 d3d BrCarTableRemove */
/* @implements 0x1001C7A0 glide BrCarTableRemove */
/* Orig walks DAT_10af2110 (active field) at stride 0x2B68; owner is
 * [esi-0xDC4], car-base for the slot scan is esi-0xF08. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */           /* nEntB */
/* 64-bit core: declared once, in br_globals.h or its struct's header */           /* nEntA */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* car0.active */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* slots */
void BrCarTableRemove(const void *pOwner)
{
    int i = 0;
    unsigned char *esi;

    /* xor ebx,ebx is before the jle: i must be live on the early-out. */
    if (g_BrCarCount <= 0)
        return;

    {
    int arg = 0;
    esi = (*(unsigned char (*)[])((char *)&g_aBrRaceCar + 0xF08)) /* BR_LP64_BYTE_VIEW */;
    do {
        if (*(uint32_t *)(esi - 0xDC4) == (uint32_t)pOwner) {
            int n;
            unsigned char *slot;
            unsigned char *car;

            *(uint32_t *)esi = 0;
            BrX10072580(arg);

            n = g_brRaceNDriver;
            if (n > 0) {
                car = esi - 0xF08;
                slot = (*(unsigned char (*)[])((char *)&g_aBrRaceDriver + 0x60)) /* BR_LP64_BYTE_VIEW */;
                do {
                    if (*(uint32_t *)slot == (uint32_t)car)
                        *(uint32_t *)slot = 0;
                    slot += BR_SLOT_STRIDE;
                    --n;
                } while (n != 0);
            }
        }

        ++i;
        arg += 2;
        esi += BR_CAR_STRIDE;
    } while (i < g_BrCarCount);
    }
}

/* 0x1002F2A0 */
/* WHAT IT DOES: takes a copy of each car's championship figures -- points and
 * the other per-car running totals -- and notes that a copy now exists, so the
 * results can be put back after whatever is about to happen. */
/* @implements 0x1002F2A0 d3d BrCarStateSave */
/* Glide arm, hand-transcribed from 0x1001C810: loose globals, and the car
 * record addressed INLINE in every statement (`DAT_10af1208 + i*0x2B68 + off`).
 * A `car` pointer local reorders the three induction-variable bumps; the
 * dword loop is what VC5 turns into the bare `rep movsd` (a memcpy of n*4
 * keeps the count in ecx the same way). */
/* 64-bit core: declared once, in br_globals.h or its struct's header */          /* car count */
/* 64-bit core: declared once, in br_globals.h or its struct's header */          /* dwords in the saved vector */
/* 64-bit core: declared once, in br_globals.h or its struct's header */          /* "a saved copy exists" */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */        /* 12 dwords per car */
void BrCarStateSave(void)
{
    int i;

    for (i = 0; i < g_brRaceNEntrant; ++i) {
        int k;

        DAT_105bc770[i] = g_aBrRaceCar[i].fFF8;
        DAT_105bc8d0[i] = (*(int32_t *)&g_aBrRaceCar[i].tFinal);
        DAT_105ccb68[i] = (*(int32_t *)&g_aBrRaceCar[i].tBest);
        DAT_105bc8f0[i] = g_aBrRaceCar[i].lapBest;
        DAT_105bc758[i] = g_aBrRaceCar[i].lap;
        for (k = 0; k < (*(int32_t *)&g_CBE8); k++)
            DAT_105ccaf8[i * 12 + k] =
                *(int32_t *)((*(unsigned char (*)[])&g_aBrRaceCar) + i * 0x2B68 + 0xFB4 + k * 4);
    }

    (*(int32_t *)&DAT_105ccb60) = 1;
}

/* 0x1002F320 */
/* WHAT IT DOES: puts each car's saved championship figures back. When the game
 * is in the right mode and a saved copy exists, it also patches the numbers
 * straight into the drawing commands for the standings display, so the points
 * on screen match what has just been restored. One of the five saved values is
 * written by the save and never read back by anything. */
/* @implements 0x1002F320 d3d BrCarStateRestore */
/* Glide arm, hand-transcribed from 0x1001C890; same globals as the save.
 * The standings block hangs off car+0xE8C: a (row, col) cursor at +4/+5 and
 * three [6][4] tables indexed by it.  The block pointer is re-read for every
 * statement (it is `*pp`, not a local).  The dword store takes its value
 * through a named temp: that alone makes VC5 fetch col before row there. */
typedef struct BrStandBlk {
    uint8_t  pad0[4];
    uint8_t  row;               /* +0x04 */
    uint8_t  col;               /* +0x05 */
    uint8_t  place[6][4];       /* +0x06 */
    int16_t  pts[6][4];         /* +0x1E */
    int16_t  pad4e;
    int32_t  tot[6][4];         /* +0x50 */
} BrStandBlk;
/* 64-bit core: declared once, in br_globals.h or its struct's header */          /* gate: restore the table only if 0 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* points per finishing position */
/* FUN_10008d60: prototype in br_funcs.h */
void BrCarStateRestore(void)
{
    int i;

    if ((*(int32_t *)((char *)&g_brRaceRules + 0xC)) /* BR_LP64_BYTE_VIEW */ == 0 && (*(int32_t *)&DAT_105ccb60) != 0) {
        for (i = 0; i < g_brRaceNEntrant; ++i) {
            BrStandBlk **pp = ((BrStandBlk **)&g_aBrRaceCar[i].pEquip);

            (*pp)->pts[(*pp)->row][(*pp)->col] = (*(signed char (*)[])&g_tblBrA9560)[DAT_105bc770[i]];
            (*pp)->place[(*pp)->row][(*pp)->col] = (uint8_t)DAT_105bc770[i];
            {
                int32_t t = DAT_105bc8d0[i];
                (*pp)->tot[(*pp)->row][(*pp)->col] = t;
            }
            BrPodNop();
        }
    }

    for (i = 0; i < g_brRaceNEntrant; ++i) {
        int k;

        for (k = 0; k < (*(int32_t *)&g_CBE8); k++)
            *(int32_t *)((*(unsigned char (*)[])&g_aBrRaceCar) + i * 0x2B68 + 0xFB4 + k * 4) =
                DAT_105ccaf8[i * 12 + k];
        g_aBrRaceCar[i].lap = DAT_105bc758[i];
        g_aBrRaceCar[i].lapBest = DAT_105bc8f0[i];
        (*(int32_t *)&g_aBrRaceCar[i].tFinal) = DAT_105bc8d0[i];
        g_aBrRaceCar[i].fFF8 = DAT_105bc770[i];
    }
}
