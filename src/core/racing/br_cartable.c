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
#ifdef BR_MATCHING_BUILD
/* slice2_17.h prototypes a list pointer the original never takes. */
#define BrPtrListContains BrPtrListContains_port
#endif
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include "slice2_17.h"
#ifdef BR_MATCHING_BUILD
#undef BrPtrListContains
#endif

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
#if defined(_MSC_VER)
#define BRS17_THISCALL __fastcall
#else
#define BRS17_THISCALL
#endif
extern void BRS17_THISCALL BrX100751D0(void *pThis);
/* XSLICE 0x1002C2C0 */
extern void  BrX1002C2C0(void);
/* XSLICE 0x1003563A */
extern void  BrX1003563A(int a0);
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
extern int   BrX10005DE0(void *pOwner, unsigned char *pb0,
                         unsigned char *pb1, unsigned char *pb2);
/* XSLICE 0x10076AE0 */
extern void  BrX10076AE0(void *pThis, int a0);
/* XSLICE 0x10005E70 */
extern const char *BrX10005E70(void *pOwner);
/* XSLICE 0x10068260 */
extern void  BrX10068260(int i, uint32_t tag);
/* XSLICE 0x10072580 */
extern void  BrX10072580(int a0);
/* XSLICE 0x10042AF0 */
extern void  BrX10042AF0(void *p, int a1, int a2);
/* XSLICE 0x10035BBA */
extern void  BrX10035BBA(const char *psz);
/* XSLICE 0x10069530 */
extern void *BrX10069530(void);
/* XSLICE 0x10069490 */
extern void *BrX10069490(void);
/* 0x1007E8B0 is the CRT's atexit (0x1007E820 wrapped, returning 0 or -1).
 * Anything at or above 0x1007CC40 is statically linked MSVC CRT, so the
 * platform's own atexit is used instead of porting it. */
extern int   BrXAtExit(void (*pfn)(void));

/* ------------------------------------------------------------------ */
/* Small helpers. Loads and stores go through memcpy so that the byte
 * offsets recovered from the disassembly stay valid without relying on
 * pointer casts being aligned.                                         */

static uint32_t s17_ld32(const unsigned char *p)
{
    uint32_t v;
    memcpy(&v, p, sizeof v);
    return v;
}

static void s17_st32(unsigned char *p, uint32_t v)
{
    memcpy(p, &v, sizeof v);
}

static float s17_ldf(const unsigned char *p)
{
    float v;
    memcpy(&v, p, sizeof v);
    return v;
}

static void s17_stf(unsigned char *p, float v)
{
    memcpy(p, &v, sizeof v);
}

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

static unsigned char *s17_car(int i)
{
    return g_s17.pCars + (size_t)i * BR_CAR_STRIDE;
}

/* 0x1002F130 */
/* WHAT IT DOES: adds a car to the race: fetches its colour and its driver's
 * name from the owning player, registers its engine sound, and files it in the
 * car table. The counter is re-read between almost every step rather than being
 * kept, which matters because the owner is recorded against the slot the
 * counter held BEFORE it was bumped. */
/* @implements 0x1002F130 d3d BrCarTableAdd */
#ifdef BR_MATCHING_BUILD
extern int32_t DAT_100b2f04;           /* nEntB */
extern int32_t DAT_100b2f00;           /* nEntA */
extern unsigned char DAT_10af1208[];   /* car0 base */
/* Glide arm, hand-transcribed from 0x1001C6A0.  Every field is addressed as
 * `&cars[count].field` straight off the counter, so the record base folds into
 * each displacement (`lea ecx,[eax+0x10af3bb6]`); a `car` pointer local keeps
 * the base in a register instead.  0x1006FD50 is __thiscall with one stack
 * argument: a __fastcall whose second parameter is a one-int struct passes
 * ecx = car and leaves edx alone, exactly as BrEntitySetIndex is defined. */
typedef struct BrS17CarRec { unsigned char b[BR_CAR_STRIDE]; } BrS17CarRec;
typedef struct { int n; } BrS17EntArg;
typedef void (__fastcall *BrS17EntSetFn)(void *pThis, BrS17EntArg a0);
#define BR_S17_CARS ((BrS17CarRec *)DAT_10af1208)
void BrCarTableAdd(void *pOwner)
{
    BrS17EntArg a;

    a.n = BrX10005DE0(pOwner,
                      &BR_S17_CARS[DAT_100b2f04].b[BR_CAR_OFF_RGB + 0],
                      &BR_S17_CARS[DAT_100b2f04].b[BR_CAR_OFF_RGB + 1],
                      &BR_S17_CARS[DAT_100b2f04].b[BR_CAR_OFF_RGB + 2]);
    ((BrS17EntSetFn)BrX10076AE0)(&BR_S17_CARS[DAT_100b2f04], a);
    strcpy((char *)&BR_S17_CARS[DAT_100b2f04].b[BR_CAR_OFF_NAME],
           BrX10005E70(pOwner));
    BrX10068260(DAT_100b2f04,
                *(uint32_t *)&BR_S17_CARS[DAT_100b2f04].b[BR_CAR_OFF_TAG]);
    *(void **)&BR_S17_CARS[DAT_100b2f04++].b[BR_CAR_OFF_OWNER] = pOwner;
    DAT_100b2f00++;
}
#else
void BrCarTableAdd(void *pOwner)
{
    int n = g_s17.nEntB;
    unsigned char *car = s17_car(n);
    int r;

    r = BrX10005DE0(pOwner,
                    car + BR_CAR_OFF_RGB + 0,
                    car + BR_CAR_OFF_RGB + 1,
                    car + BR_CAR_OFF_RGB + 2);

    /* Recomputed from the (unchanged) counter in the original, not cached. */
    BrX10076AE0(s17_car(g_s17.nEntB), r);

    strcpy((char *)(s17_car(g_s17.nEntB) + BR_CAR_OFF_NAME),
           BrX10005E70(pOwner));

    BrX10068260(g_s17.nEntB,
                s17_ld32(s17_car(g_s17.nEntB) + BR_CAR_OFF_TAG));

    n = g_s17.nEntB;
    g_s17.nEntB = n + 1;
    /* The owner pointer lands in the record the OLD counter selects. */
    s17_st32(s17_car(n) + BR_CAR_OFF_OWNER, s17_ptrword(pOwner));
    g_s17.nEntA = g_s17.nEntA + 1;
}
#endif

/* 0x1002F230 */
/* WHAT IT DOES: takes a player's car out of the race when that player leaves.
 * It marks the car inactive, silences its engine, and clears it out of every
 * slot that was pointing at it. It scans the whole table rather than stopping
 * at the first match, so a player owning more than one car loses all of
 * them. */
/* @implements 0x1002F230 d3d BrCarTableRemove */
/* @implements 0x1001C7A0 glide BrCarTableRemove */
#ifdef BR_MATCHING_BUILD
/* Orig walks DAT_10af2110 (active field) at stride 0x2B68; owner is
 * [esi-0xDC4], car-base for the slot scan is esi-0xF08. */
extern int32_t DAT_100b2f04;           /* nEntB */
extern int32_t DAT_100b2f00;           /* nEntA */
extern unsigned char DAT_10af2110[];   /* car0.active */
extern unsigned char DAT_10af0858[];   /* slots */
void BrCarTableRemove(const void *pOwner)
{
    int i = 0;
    unsigned char *esi;

    /* xor ebx,ebx is before the jle — i must be live on the early-out. */
    if (DAT_100b2f04 <= 0)
        return;

    {
    int arg = 0;
    esi = DAT_10af2110;
    do {
        if (*(uint32_t *)(esi - 0xDC4) == (uint32_t)pOwner) {
            int n;
            unsigned char *slot;
            unsigned char *car;

            *(uint32_t *)esi = 0;
            BrX10072580(arg);

            n = DAT_100b2f00;
            if (n > 0) {
                car = esi - 0xF08;
                slot = DAT_10af0858;
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
    } while (i < DAT_100b2f04);
    }
}
#else
void BrCarTableRemove(const void *pOwner)
{
    int i = 0;
    int arg = 0;                      /* edi: bumped by 2 per record */

    if (g_s17.nEntB <= 0)
        return;

    do {
        unsigned char *car = s17_car(i);

        if (s17_ld32(car + BR_CAR_OFF_OWNER) == s17_ptrword(pOwner)) {
            int j;

            s17_st32(car + BR_CAR_OFF_ACTIVE, 0);
            BrX10072580(arg);

            for (j = 0; j < g_s17.nEntA; ++j) {
                unsigned char *slot = g_s17.pSlots
                                    + (size_t)j * BR_SLOT_STRIDE
                                    + BR_SLOT_OFF_CARPTR;
                if (s17_ld32(slot) == s17_ptrword(car))
                    s17_st32(slot, 0);
            }
        }

        ++i;
        arg += 2;
    } while (i < g_s17.nEntB);         /* the count is re-read every pass */
}
#endif

/* 0x1002F2A0 */
/* WHAT IT DOES: takes a copy of each car's championship figures -- points and
 * the other per-car running totals -- and notes that a copy now exists, so the
 * results can be put back after whatever is about to happen. */
/* @implements 0x1002F2A0 d3d BrCarStateSave */
#ifdef BR_MATCHING_BUILD
/* Glide arm, hand-transcribed from 0x1001C810: loose globals, and the car
 * record addressed INLINE in every statement (`DAT_10af1208 + i*0x2B68 + off`).
 * A `car` pointer local reorders the three induction-variable bumps; the
 * dword loop is what VC5 turns into the bare `rep movsd` (a memcpy of n*4
 * keeps the count in ecx the same way). */
extern int32_t DAT_100b3858;          /* car count */
extern int32_t DAT_100bcbe8;          /* dwords in the saved vector */
extern int32_t DAT_105ccb60;          /* "a saved copy exists" */
extern int32_t DAT_105bc770[];
extern int32_t DAT_105bc8d0[];
extern int32_t DAT_105ccb68[];
extern int32_t DAT_105bc8f0[];
extern int32_t DAT_105bc758[];
extern int32_t DAT_105ccaf8[];        /* 12 dwords per car */
void BrCarStateSave(void)
{
    int i;

    for (i = 0; i < DAT_100b3858; ++i) {
        int k;

        DAT_105bc770[i] = *(int32_t *)(DAT_10af1208 + i * 0x2B68 + 0xFF8);
        DAT_105bc8d0[i] = *(int32_t *)(DAT_10af1208 + i * 0x2B68 + 0xFEC);
        DAT_105ccb68[i] = *(int32_t *)(DAT_10af1208 + i * 0x2B68 + 0xFE4);
        DAT_105bc8f0[i] = *(int32_t *)(DAT_10af1208 + i * 0x2B68 + 0xFE8);
        DAT_105bc758[i] = *(int32_t *)(DAT_10af1208 + i * 0x2B68 + 0xFA8);
        for (k = 0; k < DAT_100bcbe8; k++)
            DAT_105ccaf8[i * 12 + k] =
                *(int32_t *)(DAT_10af1208 + i * 0x2B68 + 0xFB4 + k * 4);
    }

    DAT_105ccb60 = 1;
}
#else
void BrCarStateSave(void)
{
    int i;

    for (i = 0; i < g_s17.nCars; ++i) {
        unsigned char *car = s17_car(i);
        int n;

        g_s17.pSave5C8[i] = s17_ld32(car + BR_CAR_OFF_SAVE4);
        g_s17.pSave728[i] = s17_ld32(car + BR_CAR_OFF_SAVE3);
        g_s17.pSave9C0[i] = s17_ld32(car + BR_CAR_OFF_SAVE1);
        g_s17.pSave748[i] = s17_ld32(car + BR_CAR_OFF_SAVE2);
        g_s17.pSave5B0[i] = s17_ld32(car + BR_CAR_OFF_SAVE0);

        n = g_s17.nSaveDwords;
        if (n > 0)
            memcpy(g_s17.pSave950 + (size_t)i * 12,
                   car + BR_CAR_OFF_SAVEVEC, (size_t)n * 4);
    }

    g_s17.f6909B8 = 1;
}
#endif

/* 0x1002F320 */
/* WHAT IT DOES: puts each car's saved championship figures back. When the game
 * is in the right mode and a saved copy exists, it also patches the numbers
 * straight into the drawing commands for the standings display, so the points
 * on screen match what has just been restored. One of the five saved values is
 * written by the save and never read back by anything. */
/* @implements 0x1002F320 d3d BrCarStateRestore */
#ifdef BR_MATCHING_BUILD
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
extern int32_t DAT_100a9360;          /* gate: restore the table only if 0 */
extern signed char DAT_100a9560[];    /* points per finishing position */
int FUN_10008d60();                   /* debug printf, a bare `ret` */
void BrCarStateRestore(void)
{
    int i;

    if (DAT_100a9360 == 0 && DAT_105ccb60 != 0) {
        for (i = 0; i < DAT_100b3858; ++i) {
            BrStandBlk **pp = (BrStandBlk **)(DAT_10af1208 + i * 0x2B68 + 0xE8C);

            (*pp)->pts[(*pp)->row][(*pp)->col] = DAT_100a9560[DAT_105bc770[i]];
            (*pp)->place[(*pp)->row][(*pp)->col] = (uint8_t)DAT_105bc770[i];
            {
                int32_t t = DAT_105bc8d0[i];
                (*pp)->tot[(*pp)->row][(*pp)->col] = t;
            }
            FUN_10008d60("points = %d\n",
                         (uint16_t)(*pp)->pts[(*pp)->row][(*pp)->col]);
        }
    }

    for (i = 0; i < DAT_100b3858; ++i) {
        int k;

        for (k = 0; k < DAT_100bcbe8; k++)
            *(int32_t *)(DAT_10af1208 + i * 0x2B68 + 0xFB4 + k * 4) =
                DAT_105ccaf8[i * 12 + k];
        *(int32_t *)(DAT_10af1208 + i * 0x2B68 + 0xFA8) = DAT_105bc758[i];
        *(int32_t *)(DAT_10af1208 + i * 0x2B68 + 0xFE8) = DAT_105bc8f0[i];
        *(int32_t *)(DAT_10af1208 + i * 0x2B68 + 0xFEC) = DAT_105bc8d0[i];
        *(int32_t *)(DAT_10af1208 + i * 0x2B68 + 0xFF8) = DAT_105bc770[i];
    }
}
#else
void BrCarStateRestore(void)
{
    int i;

    if (g_s17.f0AA010 == 0 && g_s17.f6909B8 != 0) {
        for (i = 0; i < g_s17.nCars; ++i) {
            unsigned char *car = s17_car(i);
            unsigned char *blk;
            uint32_t c = g_s17.pSave5C8[i];
            unsigned n1, n2;
            uint16_t v;

            /* DEVIATION: +0x0E8C holds a host pointer, not a 32-bit one, so
             * it is read at the host's pointer width rather than through
             * s17_ld32. Nothing else in this packet touches that field. */
            memcpy(&blk, car + BR_CAR_OFF_CMDPTR, sizeof blk);

            /* movsx of a signed byte into a 16-bit slot. */
            n1 = blk[4];
            n2 = blk[5];
            v = (uint16_t)(int16_t)g_s17.pTblAA210[c];
            memcpy(blk + (size_t)(n2 + n1 * 4) * 2 + 0x1E, &v, sizeof v);

            /* GOTCHA: three different addressings of the same index. This
             * one adds n2 as BYTES and n1*4 as bytes, then +6. */
            n2 = blk[5];
            n1 = blk[4];
            blk[n2 + n1 * 4 + 6] = (unsigned char)(g_s17.pSave5C8[i] & 0xFFu);

            /* ...this one scales (n2 + n1*4 + 0x14) by 4. */
            n2 = blk[5];
            n1 = blk[4];
            s17_st32(blk + (size_t)(n2 + n1 * 4 + 0x14) * 4,
                     g_s17.pSave728[i]);

            /* ...and this reads back the u16 written first. */
            n2 = blk[5];
            n1 = blk[4];
            memcpy(&v, blk + (size_t)(n2 + n1 * 4) * 2 + 0x1E, sizeof v);
            /* DEVIATION: the stub takes an intptr_t first argument so that a
             * string literal and the integer call sites in 0x1002C210 can
             * share one declaration. 0x10008B80 is a bare `ret` anyway. */
            BrStub10008B80((intptr_t)(const void *)"points = %d\n",
                           (unsigned)v);
        }
    }

    for (i = 0; i < g_s17.nCars; ++i) {
        unsigned char *car = s17_car(i);
        int n = g_s17.nSaveDwords;

        if (n > 0)
            memcpy(car + BR_CAR_OFF_SAVEVEC,
                   g_s17.pSave950 + (size_t)i * 12, (size_t)n * 4);

        s17_st32(car + BR_CAR_OFF_SAVE0, g_s17.pSave5B0[i]);
        s17_st32(car + BR_CAR_OFF_SAVE2, g_s17.pSave748[i]);
        s17_st32(car + BR_CAR_OFF_SAVE3, g_s17.pSave728[i]);
        s17_st32(car + BR_CAR_OFF_SAVE4, g_s17.pSave5C8[i]);
        /* pSave9C0 (0x106909C0) is written by the save and never read. */
    }
}
#endif
