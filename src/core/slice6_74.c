/* slice6_74.c -- BRD3D.dll, packet 74 (slice 6).  See slice6_74.h for how the
 * targets were chosen, and for the signature conflicts found on the way.
 *
 * WHY THIS FILE INCLUDES ALMOST NOTHING
 * -------------------------------------
 * Six of the ten functions here forward to a body owned by another module, and
 * those six owners' headers cannot all coexist in one translation unit: they
 * carry conflicting partial models of the same objects.  port/host/brally.c
 * hit this first and solved it by declaring the externs locally; this file
 * follows that precedent.  Each local declaration below is copied verbatim
 * from the owning header and is marked with the header it came from, so a
 * later divergence shows up as a compile error at the owner rather than as
 * silent disagreement here.
 *
 * The three headers that DO get included are the ones that were verified to
 * coexist (br_vec.h, br_mat.h and slice1_09.h) and that supply real TYPES --
 * BrVec3, BrMat4, BrBitStream -- which CONVENTIONS requires be reused rather
 * than redefined.
 */

#include <stdint.h>

#include "slice6_74.h"
#include "br_vec.h"        /* BrVec3   -- BrVec3Length's operand              */
#include "br_mat.h"        /* BrMat4   -- BrMat4FromCarState's output         */
#include "slice1_09.h"     /* BrBitStream -- the canonical bit-stream object  */

/* ==========================================================================
 * 0. Cross-module declarations (see the banner)
 * ========================================================================== */

/* slice1_09.h already declares BrBitStreamReadBits; it is included above. */

/* br_vec.h declares BrVec3Length; included above. */

/* br_crt.h -- 0x1007C8A0.  Not included: br_crt.h's own comment about the
 * out-of-range result contradicts br_crt.c (see slice6_74.h).  The
 * declaration is what matters here and it is unambiguous. */
extern int32_t BrFtolTrunc(float f);

/* slice1_08.h -- 0x10072AF0.  Returns int32_t; slice3_31.h's declaration of
 * the same address says void.  Conflict reported in slice6_74.h. */
extern int32_t BrSndPlaySimple(int32_t group, uint32_t packed);

/* slice6_72.h -- 0x1003CDA0. */
extern void BrExt_1003CDA0(void);

/* slice3_42.h -- 0x100695D0.  `BrCarState` is left incomplete on purpose: the
 * adapter only forwards the pointer, so this file never needs its layout, and
 * naming the tag rather than defining it keeps slice3_42.h's definition the
 * single source of truth.  The tag spelling matches slice3_42.h exactly. */
struct BrCarState;
extern void BrMat4FromCarState(BrMat4 *pOut, const struct BrCarState *pSrc);

/* slice1_02.h -- the bit reader is the same object as slice1_09.h's
 * BrBitStream under a different tag; slice1_02.h keeps it opaque precisely so
 * that whichever module lands first owns the layout.  This one does. */
struct BrBitReader;

/* ==========================================================================
 * 1. 0x10073E70 -- BrBitStreamWriteBits
 *
 * The write twin of 0x10073C90 (slice1_09.h's BrBitStreamReadBits), and by
 * some distance the most-wanted unported function in the tree: 51 call/jmp
 * sites in .text, 50 in the ported sources.  It completes slice1_09.h's
 * bit-stream class, which had every byte-granular accessor but only the READ
 * side of the bit-granular pair.
 *
 * __thiscall in the original with two stack args (`ret 8`); the object comes
 * first here as everywhere else in slice1_09.h.  Argument order confirmed off
 * the stack rather than assumed: after `push ebp/esi/edi/ebx`, `value` is read
 * from [esp+0x14] and `nBits` was taken from [esp+0xC] before the pushes, so
 * value is the lower-addressed (first) argument.  That matches slice2_12.h's
 * existing declaration.
 *
 * Semantics: emit the low `nBits` bits of `value`, MOST SIGNIFICANT FIRST,
 * WITHOUT aligning the write cursor first -- so a partially filled byte is
 * filled from the top down, and a run of small writes packs.  Bits already
 * present in the current byte are preserved through an explicit mask, so the
 * function ORs into the buffer rather than overwriting it.
 *
 * nBits == 0 returns immediately, writing nothing and moving no cursor -- the
 * original's `test ebp,ebp / je` before it touches anything.  This mirrors
 * BrBitStreamReadBits, which likewise treats 0 as a no-op.
 * ========================================================================== */

void BrBitStreamWriteBits(BrBitStream *pBs, int32_t value, int32_t nBits)
{
    int32_t remaining = nBits;

    /* The original's first branch. Note it is `!= 0`, not `> 0`: a NEGATIVE
     * nBits falls through into the loop exactly as the original does. */
    if (remaining == 0) {
        return;
    }

    do {
        /* Bits still free in the byte under the write cursor. The original
         * recomputes this twice per iteration from the same field; it cannot
         * change in between. */
        int32_t freeBits = 8 - pBs->writeBit;
        int32_t take, shift;

        if (freeBits > remaining) {
            /* The whole remainder fits in this byte, and lands `shift` bits
             * above the byte's bottom. */
            shift = freeBits - remaining;
            take  = remaining;
        } else {
            /* Fill this byte to its end and go round again. */
            shift = 0;
            take  = freeBits;
        }

        /* The original decrements BEFORE extracting, and then uses the
         * decremented value as the extraction shift -- so `remaining` below is
         * the count of bits that will be written on LATER iterations, i.e. the
         * bit position within `value` of the chunk taken now. That is what
         * makes the emission most-significant-first. */
        remaining -= take;

        {
            unsigned char *pByte = pBs->pBuf + pBs->writeByte;
            uint32_t keep, chunk;

            /* Mask of the bits ALREADY written in this byte: writeBit ones,
             * pushed up to the top of the byte. writeBit == 0 gives 0, which
             * is why a fresh byte is overwritten rather than merged. */
            keep = ((1u << pBs->writeBit) - 1u) << freeBits;

            /* Take `take` bits out of `value` starting at bit `remaining`.
             *
             * The shift counts are masked to 5 bits to reproduce the x86
             * shift semantics exactly. The original is `shl/shr reg, cl`,
             * which the CPU masks to 0..31; the same expressions in C would be
             * undefined for counts >= 32. Reachable calls use nBits <= 32 and
             * never exercise the masking, but transcribing it costs nothing
             * and keeps a pathological nBits from being UB here when it is
             * merely quirky there. */
            chunk  = ((1u << take) - 1u) << (((uint32_t)remaining) & 31u);
            chunk &= (uint32_t)value;
            chunk >>= (((uint32_t)remaining) & 31u);

            /* `shl bl, cl` -- an EIGHT-BIT shift in the original, so the chunk
             * is truncated to a byte before it is positioned. take <= 8 keeps
             * the chunk inside a byte anyway; the cast makes that explicit. */
            *pByte = (unsigned char)(((unsigned char)chunk << shift) |
                                     (*pByte & (unsigned char)keep));
        }

        pBs->writeBit += take;

        /* Normalise the cursor. This is also what keeps a caller-supplied
         * writeBit of exactly 8 from stalling: that iteration takes 0 bits and
         * writes nothing, and this step then moves to the next byte. */
        if (pBs->writeBit >= 8) {
            pBs->writeBit = 0;
            pBs->writeByte++;
        }
    } while (remaining != 0);
}

/* The original stores its loop scratch into its own incoming `nBits` argument
 * slot at [esp+0x18] -- the "scratch-in-arg-slot" pattern this codebase has
 * seen at 0x10010D10, 0x10010BF0, 0x10030810 and 0x1003D180. It is not an
 * out-parameter and nothing re-reads the slot, so the port keeps the value in
 * a local and the behaviour is identical. */

/* ==========================================================================
 * 2. 0x10073B90 -- reset the READ cursor
 *
 * Two stores and a ret. Included because it is the missing member of
 * slice1_09.h's class that changes observable state, and because its
 * one-instruction-pair neighbour 0x10073B80 (BrObjClear) resets BOTH cursors:
 * having only the two-cursor reset available invites using it where the
 * original used the one-cursor reset, which would silently discard a write
 * position.
 *
 * NOT PORTED, deliberately: 0x10073D80, the align-then-write-2-bytes-BE twin
 * that would complete the U8/U16/U24/U32 run. It was disassembled and read
 * (align via 0x10073F20, then high byte then low byte, writeByte stepped once
 * after each) but a sweep of .text finds ZERO call or jmp sites for it in this
 * build. Porting a function nothing calls is exactly what the brief says can
 * wait, and it is not a stub, so porting it would remove nothing.
 * ========================================================================== */

void BrBitStreamResetRead(void *pBs)
{
    BrBitStream *p = (BrBitStream *)pBs;

    p->readBit  = 0;
    p->readByte = 0;
}

/* ==========================================================================
 * 3. 0x10074DC0 -- store the argument to 0x118AA088
 *
 * `mov eax,[esp+4] / mov [0x118AA088],eax / ret`. Nine call sites in the
 * ported tree. See slice6_74.h for why owning the global here is safe today
 * and what has to happen to it later.
 * ========================================================================== */

int32_t g_br18AA088;

void BrSub10074DC0(int n)
{
    g_br18AA088 = (int32_t)n;
}

/* ==========================================================================
 * 4. 0x10008B80 -- a bare `ret`
 *
 * config/functions.csv gives this address size 1, and the byte at 0x10008B80
 * is 0xC3 with NOP padding after it, so it really is an empty function in this
 * build -- as CONVENTIONS and slice2_18.h both already record. It is called
 * cdecl with 0, 1 and 5 arguments and through two further names, which C99
 * cannot express in one prototype, so the callers declare one name per
 * observed arity. All six are the same original function and all six are
 * correctly empty.
 *
 * These were stubs only in the accidental sense: br_stubs.c's generated stub
 * happened to have the right behaviour for the wrong reason. Defining them
 * here makes "does nothing" a transcribed fact rather than a coincidence, and
 * takes six names off the not-yet-ported list where they never belonged.
 *
 * Each signature below is copied from the caller's own declaration
 * (slice2_18.h for the four arity-split names, slice2_26.h / slice3_45.h for
 * BrExt_10008B80, port/src/slice2_17.c for the variadic one).
 * ========================================================================== */

/* WHAT IT DOES: nothing at all. The shipped game really does have an empty
 * function here, called from a great many places under six different names
 * and with varying numbers of arguments. Doing nothing is the transcribed
 * behaviour, not a gap. */
void BrExt_10008B80(void)
{
}

/* @n64 0x80219A5C exact */
void BrStub10008B80(intptr_t a0, ...)
{
    (void)a0;
}

void BrStub8B80_0(void)
{
}

void BrStub8B80_1i(int32_t a0)
{
    (void)a0;
}

void BrStub8B80_1p(const void *p0)
{
    (void)p0;
}

void BrStub8B80_5i(int32_t a0, int32_t a1, int32_t a2, int32_t a3, int32_t a4)
{
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4;
}

/* ==========================================================================
 * 5. Adapters
 *
 * Each of these is one original address that acquired a second host name. The
 * body already exists in another module; CONVENTIONS says reuse it and say so,
 * because a second transcription is a second thing to keep right. None of
 * these adapters decides anything -- if one of them looks like it is choosing
 * between two behaviours, that is a bug in the adapter.
 * ========================================================================== */

/* 0x10073C90.  slice1_02.h names the bit reader `BrBitReader` and keeps it
 * opaque, "so another module's definition is the single source of truth", and
 * asks to be renamed to whatever a later pass exports. slice1_09.h is that
 * pass: same address, same object, name BrBitStreamReadBits. 50 call sites --
 * the largest single demand in the stub list.
 *
 * The nBits parameter narrows unsigned -> int across the seam. slice1_02.h
 * declares `unsigned`, slice1_09.h declares `int`; every call site passes a
 * small literal width, and the original reads the slot as a plain dword with
 * no signedness of its own, so the two declarations describe the same
 * instruction. The cast is explicit rather than implicit to keep that visible.
 */
/* WHAT IT DOES: pulls the next few bits out of a packed stream of data -- the
 * form the game stores network messages and compressed records in, where
 * values are squeezed together without regard for byte boundaries. This is
 * only a second name for a routine that already exists elsewhere in the tree;
 * it decides nothing itself. */
/* NOT tagged: 0x10073C90's body is BrBitStreamReadBits in slice1_09.c, which
 * carries the @implements.  This is the second NAME the image gives that
 * address, and as a thunk it compiles to a 32-byte call that can never
 * reproduce the 133-byte original -- exactly the trap recorded on BrVec3Len
 * below, which had left this address permanently unmatchable. */
uint32_t BrBitReaderRead(struct BrBitReader *pReader, unsigned nBits)
{
    return (uint32_t)BrBitStreamReadBits((BrBitStream *)pReader, (int)nBits);
}

/* 0x1003B170.  slice2_21.h's own comment records that this address collected
 * three names; br_vec.h owns the body as BrVec3Length, with the sum of squares
 * rounded to float32 before the sqrt.
 *
 * This adapter matters more than its four call sites suggest. The generated
 * stub returned `long` 0, which leaves xmm0 untouched for a float-returning
 * callee -- so every caller of BrVec3Len was reading a GARBAGE length, not a
 * zero one. br_stubs.c's own banner flags this as the failure mode that only
 * shows up as a hit; it never showed up as a hit because nothing on the boot
 * path calls it yet. */
/* WHAT IT DOES: measures how long a direction-and-distance is -- the length
 * of a vector, used everywhere the game needs a speed or a distance from a
 * pair of points. Another second name for an existing routine. */
/* NOT tagged: 0x1003B170's body is BrVec3Length in br_vec.c, which carries the
 * @implements and matches byte-for-byte.  This is the second NAME the image
 * gives that address, and as a thunk it compiles to a 16-byte call that can
 * never reproduce the 65-byte original -- tagging it too put one address in the
 * measured set twice and left one of the pair permanently unmatchable. */
float BrVec3Len(const BrVec3 *pV)
{
    return BrVec3Length(pV);
}

/* 0x1002B920.  Verified by disassembly rather than taken on trust: the entire
 * body is `fld dword [esp+4]` then `jmp 0x1007C8A0`, nine bytes, so this is
 * __ftol reached through a stack argument instead of the x87 stack and is
 * behaviourally identical to br_crt.h's BrFtolTrunc. Out-of-range therefore
 * gives 0, per br_crt.c and CONVENTIONS (and NOT 0x80000000, per br_crt.h's
 * mistaken comment). 12 call sites. */
/* WHAT IT DOES: turns a fractional number into a whole one by throwing away
 * the fraction. Another second name for an existing routine; a number too big
 * to fit comes back as zero. */
/* @implements 0x1002B920 d3d BrFtolArg */
int32_t BrFtolArg(float f)
{
    return (int32_t)f;
}

/* 0x10072AF0.  slice1_08.c already transcribes this address as
 * BrSndPlaySimple(group, packed) -> BrSndPlayGroup(group, packed, 0), which
 * the disassembly confirms exactly: `push 0 / push arg2 / push arg1 / call
 * 0x10072A70`. 11 call sites.
 *
 * SIGNATURE CONFLICT (reported in slice6_74.h, not resolved): slice3_31.h
 * declares this void; the original returns int32_t. The adapter matches the
 * declaration its callers compile against and drops the result, which is what
 * those callers do with it anyway. */
void BrExt_10072AF0(int32_t a, uint32_t b)
{
    (void)BrSndPlaySimple(a, b);
}

/* 0x1003CDA0.  slice5_62.h already recorded this collision in writing --
 * "BrExt_1003CDA0 in slice2_26.h and BrSub1003CDA0 in slice2_25.h, same
 * `void (void)` shape, two names" -- and slice6_72.c owns the body. 8 call
 * sites. */
/* WHAT IT DOES: a second name for a routine that lives in another module,
 * reached under two different spellings by different parts of the game. It
 * forwards and nothing more; what the routine itself does is described where
 * its body is. */
/* @implements 0x1003CDA0 d3d BrSub1003CDA0 */
#ifdef BR_MATCHING_BUILD
/* NOT an adapter in the image. Glide 0x10036430 and D3D 0x1003CDA0 are both
 * 212 bytes of the real body -- the two spellings the tree found are two
 * COPIES the linker did not fold, not a forwarder and an owner. slice6_72.c
 * carries the same body as BrExt_1003CDA0, but through a Br72Env struct of
 * function pointers, which is the port's own indirection and cannot match.
 * This arm calls DirectPlay and KERNEL32 the way the original does.
 *
 * What it does: publish the current race settings into the DirectPlay session
 * description -- four user dwords, then SetSessionDesc -- and free the
 * GlobalAlloc'd descriptor on every path. */
extern void *DAT_10273328;      /* the IDirectPlay4, NULL before a session */
extern int32_t DAT_100b3014;    /* the four settings the session carries */
extern int32_t DAT_10226e80;
extern int32_t DAT_10ac5d70;
extern int32_t DAT_100abdf8;

/* The session description GlobalAlloc'd by 0x10036740; only the four user
 * dwords at +0x40..+0x4C are touched here. */
typedef struct BrDpSessionDesc {
    int32_t aHead[16];          /* +0x00..+0x3F, untouched */
    int32_t dwUser1;            /* +0x40 */
    int32_t dwUser2;            /* +0x44 */
    int32_t dwUser3;            /* +0x48 */
    int32_t dwUser4;            /* +0x4C */
} BrDpSessionDesc;

typedef struct BrDpObj BrDpObj;
typedef struct BrDpVtbl {
    void *apfn00[31];                                        /* +0x00..+0x7B */
    int32_t (__stdcall *SetSessionDesc)(BrDpObj *,
                                        BrDpSessionDesc *,
                                        uint32_t);           /* +0x7C */
} BrDpVtbl;
struct BrDpObj { const BrDpVtbl *pVtbl; };

extern int32_t BrDpGetSessionDesc(void *pDP, BrDpSessionDesc **ppDesc);
extern void    BrDpRefreshSettings(void);                    /* 0x1003DA90 */

__declspec(dllimport) void *__stdcall GlobalHandle(const void *pMem);
__declspec(dllimport) int   __stdcall GlobalUnlock(void *hMem);
__declspec(dllimport) void *__stdcall GlobalFree(void *hMem);

int32_t BrSub1003CDA0(void)
{
    BrDpSessionDesc *pDesc = NULL;
    BrDpObj *pDP;
    int32_t hr;

    pDP = (BrDpObj *)DAT_10273328;
    if (pDP == NULL) {
        return (int32_t)0x88770082;      /* 0x10036444 */
    }

    hr = BrDpGetSessionDesc(pDP, &pDesc);                /* 0x10036453 */
    if (hr >= 0) {
        pDesc->dwUser1 = DAT_100b3014;                   /* 0x1003646A */
        pDesc->dwUser2 = DAT_10226e80;                   /* 0x10036477 */
        pDesc->dwUser3 = DAT_10ac5d70;                   /* 0x10036484 */
        pDesc->dwUser4 = DAT_100abdf8;                   /* 0x10036490 */

        BrDpRefreshSettings();                           /* 0x10036493 */

        /* The original RE-READS the object here rather than reusing it. */
        pDP = (BrDpObj *)DAT_10273328;                   /* 0x10036498 */
        hr = pDP->pVtbl->SetSessionDesc(pDP, pDesc, 0u); /* 0x100364A7 */
    }

    if (hr < 0) {
        /* 0x100364B0 -- the failure path null-checks the descriptor. */
        if (pDesc != NULL) {
            GlobalUnlock(GlobalHandle(pDesc));
            GlobalFree(GlobalHandle(pDesc));
        }
        return hr;
    }
    /* 0x100364DC -- the success path does NOT null-check it. hr >= 0 means
     * 0x10036740 produced one. */
    GlobalUnlock(GlobalHandle(pDesc));
    GlobalFree(GlobalHandle(pDesc));
    return 0;
}
#else
void BrSub1003CDA0(void)
{
    BrExt_1003CDA0();
}
#endif

/* 0x100695D0.  slice3_42.c owns the body as BrMat4FromCarState. This is the
 * quaternion matrix builder that DOES divide by the norm; the other one,
 * 0x10074450 / BrRbBuildMatrix, does not and scales the matrix instead. They
 * are not interchangeable, and this adapter must not be pointed at the other.
 *
 * slice3_40.h's `void *pDst220` is the car record's BrMat4 at +0x220 -- the
 * name says so and slice3_42.h types it as BrMat4 *. The cast is the whole
 * adapter. 3 call sites. */
/* WHAT IT DOES: builds the transform that places and orients a car for
 * drawing, from the position and facing the physics keeps. A second name for
 * an existing routine -- and importantly not the other, similar-looking
 * matrix builder, which scales rather than normalises and is not
 * interchangeable with this one. */
/* NOT tagged. 0x100695D0 is ONE function with ONE body, and slice3_42.c has
 * it as BrMat4FromCarState; this is a C-level alias for the three call sites
 * that reach it under the other spelling, not a second copy. Tagged
 * @implements here until 2026-09-03, which scored this 32-byte forwarder
 * against the 363-byte original and left the real body invisible to triage.
 * (Contrast 0x1003CDA0 above, where the image really does hold two copies.) */
void BrSub100695D0(void *pDst220, const struct BrCarState *pState)
{
    BrMat4FromCarState((BrMat4 *)pDst220, pState);
}

/* ==========================================================================
 * 6. The two DirectPlay mutex hooks
 *
 * These are not decompiled functions and there is nothing to decompile: the
 * original calls KERNEL32 WaitForSingleObject(h, INFINITE) and ReleaseMutex(h)
 * INLINE at every one of the ~69 sites, and slice1_02.h factored those inline
 * sequences out behind two names, marked the split DEVIATION, and asked for
 * "real implementations (or no-ops for a single-threaded build) when linking".
 * This is that link, and this build is single-threaded, so these are the
 * documented no-ops rather than an invented threading model.
 *
 * They are here rather than in the host because they are a property of the
 * PORT's concurrency model, not of one executable: every consumer of
 * slice1_02.h needs the same answer. Wiring a real mutex belongs with the
 * thread that makes the port concurrent, and is a deliberate non-decision
 * here -- picking one now would bake a threading model in ahead of the
 * evidence.
 *
 * 34 and 35 call sites respectively, the second and third largest demands in
 * the stub list.
 * ========================================================================== */

void BrNetMutexLock(void *hMutex)
{
    /* DEVIATION: single-threaded build -- the original blocks on the mutex
     * here. See the banner above; slice1_02.h sanctions this explicitly. */
    (void)hMutex;
}

void BrNetMutexUnlock(void *hMutex)
{
    /* DEVIATION: see BrNetMutexLock. */
    (void)hMutex;
}

/* ==========================================================================
 * 7. 0x10073AC0 -- backend texture constructor wrapper
 *
 * Fourteen constant arguments through the cdecl function pointer at
 * 0x118AA0B0 (the same constructor BrGbiTexCreate / 0x1002A280 uses), then
 * the handle in eax is stored at 0x100A6498.  Neighbours 0x10073950 and
 * 0x100739E0 are the same shape with different source/size/format.
 *
 * C argument order (last push is first arg): source 0x118AA8F8, aux
 * 0x118AA0D8, width 0x20, height 0x80, fmt 0, siz 2, then eight zeros.
 * fmt/siz (0, 2) is the RGBA16 pair BrGbiTexCreate selects for flag 0x1.
 * ========================================================================== */

#ifdef BR_MATCHING_BUILD

/* 0x118AA8F8 -- source texel block; the original pushes the ADDRESS. */
extern uint8_t g_18AA8F8[];
/* 0x118AA0D8 -- second constructor argument; likewise an address. */
extern uint8_t g_18AA0D8[];
/* 0x118AA0B0 -- backend texture constructor. */
extern void *(*g_18AA0B0)(void *pSrc, void *pA2,
                          uint32_t w, uint32_t h,
                          uint32_t fmt, uint32_t siz,
                          uint32_t a7, uint32_t a8,
                          uint32_t a9, uint32_t a10,
                          uint32_t a11, uint32_t a12,
                          uint32_t a13, uint32_t a14);
/* 0x100A6498 -- resulting texture handle. */
extern void *g_0A6498;

/* WHAT IT DOES: asks the graphics backend to turn a fixed 32-by-128 block of
 * pixels already in memory into a texture, and stores the handle it returns. */
/* @implements 0x10073AC0 d3d BrSub10073AC0 */
void BrSub10073AC0(void)
{
    g_0A6498 = g_18AA0B0(g_18AA8F8, g_18AA0D8,
                         0x20u, 0x80u, 0u, 2u,
                         0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u);
}

extern int DAT_11778808;
extern int DAT_11778820;
extern int DAT_11773690;
extern int DAT_100b5170;
extern int DAT_100b4c30;
extern int DAT_100b4e70;
extern int DAT_100b4f30;
extern int DAT_100b4d50;
extern int DAT_100b4ed0;
extern int DAT_100b5050;

/* WHAT IT DOES: configure track surface grip tables by surface type index. */
/* @implements 0x10069530 glide BrTrackSurfaceSet */

void BrTrackSurfaceSet(int param_1)

{
  switch(param_1) {
  case 0:
  case 1:
  case 2:
  case 3:
  case 4:
  case 6:
  case 7:
  case 8:
  case 9:
  case 10:
  case 0xc:
    DAT_11778808 = (int)&DAT_100b4c30;
    DAT_11778820 = (int)&DAT_100b4e70;
    DAT_11773690 = (int)&DAT_100b4f30;
    DAT_100b5170 = 0x3f800000;
    return;
  case 5:
  case 0xb:
  case 0xd:
  case 0xe:
  default:
    DAT_11778808 = (int)&DAT_100b4d50;
    DAT_11778820 = (int)&DAT_100b4ed0;
    DAT_11773690 = (int)&DAT_100b5050;
    DAT_100b5170 = 0x3f666666;
    return;
  }
}

#endif /* BR_MATCHING_BUILD */
