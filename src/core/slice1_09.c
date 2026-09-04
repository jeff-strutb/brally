/* slice1_09.c -- decompiled from BRD3D.dll, range 100734F0-10078CD0.
 * See slice1_09.h for the recovered layouts and the argument-order notes.
 *
 * Skipped functions and the reason for each are listed at the bottom of this
 * file so the information does not get lost.
 */
#ifdef BR_MATCHING_BUILD
/* slice1_09.h declares these cdecl; the originals are thiscall with stack
 * args.  Hide those prototypes so the matching bodies can use __fastcall
 * plus a struct-typed second argument (never register-eligible, so forced
 * onto the stack).  Same split as thiscall; do not redefine BR_THISCALL. */
#define BrBitStreamReadBits  BrBitStreamReadBits_cdecl
#define BrBitStreamInit      BrBitStreamInit_cdecl
#define BrBitStreamSkipBytes BrBitStreamSkipBytes_cdecl
#define BrBitStreamWriteU8   BrBitStreamWriteU8_cdecl
#define BrBitStreamWriteU24  BrBitStreamWriteU24_cdecl
#define BrBitStreamWriteU32  BrBitStreamWriteU32_cdecl
#define BrEntitySetIndex     BrEntitySetIndex_cdecl
#define BrEntityBindAux      BrEntityBindAux_cdecl
#endif
#include "slice1_09.h"
#ifdef BR_MATCHING_BUILD
#undef BrBitStreamReadBits
#undef BrBitStreamInit
#undef BrBitStreamSkipBytes
#undef BrBitStreamWriteU8
#undef BrBitStreamWriteU24
#undef BrBitStreamWriteU32
#undef BrEntitySetIndex
#undef BrEntityBindAux
#endif

#include <math.h>
#include <stddef.h>

/* ================================================================== */
/* Bit/byte stream                                                     */
/* ================================================================== */

/* 0x10073D20 -- align the READ cursor.
 *
 * This is the routine already exported as BrObjConsumeFlag() in br_obj.h
 * (its "flag" is readBit and its "count" is readByte). It is reproduced as a
 * static here for one reason only: so this translation unit links without
 * br_obj.o. At integration the two should be collapsed into one.
 * DEVIATION: duplicate of an existing symbol, kept private (static). */
/* WHAT IT DOES: moves the read position to the start of the next whole byte,
 * so that a byte-sized read after some loose bits lands on a byte boundary.
 * Every byte-at-a-time read in this file begins by calling it. */
/* @implements 0x10073D20 d3d BrBitStreamAlignRead */
/* thiscall like the rest of the class: its callers pass the stream in ecx and
 * push nothing.  File-static, so no header change is involved. */
static void BR_THISCALL1 BrBitStreamAlignRead(BrBitStream *pBs)
{
    if (pBs->readBit != 0) {
        pBs->readBit = 0;
        pBs->readByte++;
    }
}

/* 0x10073F20 */
/* WHAT IT DOES: the same rounding-up for the write position: finishes off a
 * part-written byte so the next whole-byte write starts cleanly. */
/* @implements 0x10073F20 d3d BrBitStreamAlignWrite */
void BR_THISCALL1 BrBitStreamAlignWrite(BrBitStream *pBs)
{
    if (pBs->writeBit != 0) {
        pBs->writeBit = 0;
        pBs->writeByte++;
    }
}

/* 0x10073B60  __thiscall(ecx=this), ret 8.
 * Note the original zeroes +0x08 and +0x00/+0x04 and then stores arg2 into
 * +0x0C and arg1 into +0x10 -- the LENGTH goes into the write byte cursor. */
/* WHAT IT DOES: sets up a buffer for reading and writing packed data --
 * network packets and the game's own files. Note the buffer length is put
 * into the write position, so a freshly initialised stream is set up to be
 * read from the start and appended to at the end. */
/* @implements 0x10073B60 d3d BrBitStreamInit */
#ifdef BR_MATCHING_BUILD
/* thiscall, two stack args.  Both extra arguments are structs so neither
 * claims edx.  Size-exact (29) but register-walled: original copies this
 * to eax and zeroes via ecx; VC5 keeps this in ecx and zeroes via eax, and
 * loads pBuf before nBytes.  Two attempts (named nBytes, dummy edx) did
 * not move it. */
typedef struct { void *p; } BrBitStreamInitBuf;
typedef struct { int n; }   BrBitStreamInitLen;
void __fastcall BrBitStreamInit(BrBitStream *pBs, BrBitStreamInitBuf pBuf,
                                BrBitStreamInitLen nBytes)
{
    pBs->writeBit  = 0;
    pBs->readBit   = 0;
    pBs->readByte  = 0;
    pBs->writeByte = nBytes.n;
    pBs->pBuf      = (unsigned char *)pBuf.p;
}
#else
void BrBitStreamInit(BrBitStream *pBs, void *pBuf, int nBytes)
{
    pBs->writeBit  = 0;
    pBs->readBit   = 0;
    pBs->readByte  = 0;
    pBs->writeByte = nBytes;
    pBs->pBuf      = (unsigned char *)pBuf;
}
#endif

/* 0x10073BA0  __thiscall, ret 4 */
/* WHAT IT DOES: skips forward a number of whole bytes in the stream,
 * rounding up to a byte boundary first. */
/* @implements 0x10073BA0 d3d BrBitStreamSkipBytes */
/* @n64 0x8023FF34 located */
#ifdef BR_MATCHING_BUILD
typedef struct { int n; } BrBitStreamSkipArg;
void __fastcall BrBitStreamSkipBytes(BrBitStream *pBs, BrBitStreamSkipArg n)
{
    BrBitStreamAlignRead(pBs);
    pBs->readByte += n.n;
}
#else
void BrBitStreamSkipBytes(BrBitStream *pBs, int n)
{
    BrBitStreamAlignRead(pBs);
    pBs->readByte += n;
}
#endif

/* 0x10073BC0  __thiscall.
 * The original returns the byte in AL only; the upper 24 bits of EAX are
 * left holding the top of the buffer pointer. Callers that used it as an
 * int would therefore see garbage, so it is typed as a byte here.
 * DEVIATION: return narrowed from the register-width EAX to unsigned char. */
/* WHAT IT DOES: reads the next whole byte out of the stream. The original
 * left junk in the upper part of the return register, so anything treating
 * the answer as a full-width number would have seen garbage; here it is a
 * byte and only a byte. */
/* @implements 0x10073BC0 d3d BrBitStreamReadU8 */
unsigned char BR_THISCALL1 BrBitStreamReadU8(BrBitStream *pBs)
{
    BrBitStreamAlignRead(pBs);
    /* Orig `inc ecx` after the byte load: post-increment the cursor, do not
     * write `i + 1` (that is `lea ecx,[ecx+1]`). */
    return pBs->pBuf[pBs->readByte++];
}

/* 0x10073BE0  big-endian u16; DH gets byte 0, DL byte 1, EDX pre-zeroed. */
/* WHAT IT DOES: reads the next two bytes as a single number, most
 * significant byte first. Boss Rally's data came from the N64 and is stored
 * that way round throughout. */
/* @implements 0x10073BE0 d3d BrBitStreamReadU16 */
unsigned int BR_THISCALL1 BrBitStreamReadU16(BrBitStream *pBs)
{
    const unsigned char *p;
    int i;
    BrBitStreamAlignRead(pBs);
    i = pBs->readByte;
    p = pBs->pBuf + i;
    {
        /* RESIDUE (4 B): orig loads DH (p[0]) before DL (p[1]); VC5 emits
         * the low byte first from every probed spelling. Value-before-cursor-
         * store via this block temp is what the original does prove.
         *
         * PROBED AND DEAD, do not re-run. Earlier: |-order, +, byte temps,
         * |=-accumulate, u16 temp. Added after WriteU16/WriteU32 fell to a
         * named-local lever (see their notes) -- it does NOT carry over here,
         * because this function already names both halves:
         *   inert (33 B / 14 insns / 4 diffs, unchanged): seeding the
         *     accumulate from the LOW byte instead of the high; casting
         *     outside the `|` instead of on each operand; splitting the
         *     shift-or into its own statement after a plain `v = p[0]`;
         *     spelling the loads `*p` / `*(p+1)` rather than p[0] / p[1].
         *   worse: `p[0] * 256u + p[1]` (+7 B, +4 insns -- the multiply is
         *     NOT folded to a shift here); a `unsigned short` value temp
         *     (+5 B, the narrowing costs a movzx). */
        unsigned int v = ((unsigned int)p[0] << 8) | (unsigned int)p[1];
        pBs->readByte = i + 2;
        return v;
    }
}

/* 0x10073C10  big-endian u24, zero-extended. */
/* WHAT IT DOES: reads the next three bytes as a single number, most
 * significant byte first. */
/* @implements 0x10073C10 d3d BrBitStreamReadU24 */
unsigned int BR_THISCALL1 BrBitStreamReadU24(BrBitStream *pBs)
{
    const unsigned char *p;
    int i;
    BrBitStreamAlignRead(pBs);
    i = pBs->readByte;
    p = pBs->pBuf + i;
    {
        /* RESIDUE (15 B tail): orig loads p[2] into AL over the dying
         * pointer reg (mov al; and eax,0xff) before the cursor store; VC5
         * sinks the load after the store into a fresh zeroed reg from
         * every probed spelling (inline, |=, pre-store uchar temp spills). */
        unsigned int v = (((unsigned int)p[0] << 8)
                          | (unsigned int)p[1]) << 8;
        pBs->readByte = i + 3;
        return v | p[2];
    }
}

/* 0x10073C40  big-endian 32-bit.
 * The original builds the value as ((((s8)p[0] << 8 | p[1]) << 8 | p[2]) << 8
 * | p[3]); the movsx on p[0] is shifted out entirely, so the result is the
 * plain big-endian word. Built here through unsigned to avoid signed
 * overflow, then converted -- the bit pattern is identical.
 * DEVIATION: unsigned intermediate, because a signed left shift into the
 * sign bit is undefined in C99. */
/* WHAT IT DOES: reads the next four bytes as a single signed number, most
 * significant byte first. */
/* @implements 0x10073C40 d3d BrBitStreamReadS32 */
int BR_THISCALL1 BrBitStreamReadS32(BrBitStream *pBs)
{
    const unsigned char *p;
    int i, v;
    BrBitStreamAlignRead(pBs);
    i = pBs->readByte;
    /* p[1] is read through the two-part sum BEFORE p is bound (the
     * lea-late idiom); the chain seeds from a SIGNED char read of p[0]. */
    /* RESIDUE (12 B): orig reads p[1] through the unbound two-part sum,
     * binds p with a late lea, and widens p[1..3] in dirty regs
     * (mov dl / and 0xff); VC5 binds p first and zero-widens (xor + mov)
     * from every probed spelling -- the register-byte analogue of the
     * byte-slot wall. The signed-char Horner seed IS proven right. */
    v = pBs->pBuf[i + 1];
    p = pBs->pBuf + i;
    v |= (int)*(const signed char *)p << 8;
    v = ((v << 8) | p[2]) << 8;
    pBs->readByte = i + 4;
    return v | p[3];
}

/* 0x10073C90  __thiscall, ret 4.
 *
 * Per round it takes  take = min(8 - readBit, remaining)  bits out of the
 * current byte, from the position  shift = 8 - readBit - take, and appends
 * them to the accumulator low end after shifting the accumulator left by
 * `take`. So the stream is MSB-first.
 *
 * The original sign-extends the source byte before masking; the mask never
 * reaches above bit 7, so that has no effect and is not reproduced.
 *
 * The original also keeps a running total of bits consumed in a stack local
 * ([esp+0x10]) that nothing ever reads -- a dead store, omitted.
 *
 * DEVIATION: nBits < 0 is not reproduced. The original would shift by
 * (nBits & 31) and return garbage after a single round; in C that is
 * undefined, so this function simply must not be called with nBits < 0.
 * nBits > 32 loses the high bits in both versions. */
/* The tag lives HERE, not on slice6_74.c's BrBitReaderRead, which is the
 * second NAME this address carries.  That body is a 32-byte thunk into this
 * one and can never reproduce the 133-byte original -- the same trap
 * slice6_74.c's own BrVec3Len note records, and it had put the address in the
 * measured set as permanently unmatchable.
 *
 * Thiscall: `mov esi,ecx` captures pBs and the bit count arrives at [esp+4]
 * (`ret 4`).  Spelled with the struct-typed second argument this file already
 * uses for its other thiscall bodies, so nBits cannot claim edx.  The loop
 * counter IS the argument slot -- 1006CF41 reads [esp+0x18], subtracts and
 * writes back, which is the incoming argument, not a copy -- so `nBits.n` is
 * decremented in place rather than copied into a `remaining` local.
 *
 * The running total the earlier reading called "a dead store, omitted" is
 * back: the original keeps it in its one stack local (the `push ecx`
 * prologue, [esp+4] before the inner pushes and [esp+0x10] after), zeroes it
 * before the early-out test, and accumulates into it every round.  VC5 keeps
 * a plain `int` for it; no volatile is needed.
 *
 * RESIDUE 128 bytes, 156 against 133.  The frame is one dword too big:
 * the original's single local IS the counter and `acc` lives in eax for the
 * whole function, while the recompile spills `acc` to a second slot
 * (`sub esp, 8`, acc at [esp+0x10]) and duplicates the early-out epilogue
 * instead of jumping to the shared tail.  Probed and ruled out: `volatile`
 * on the counter (identical output), and folding the mask temp into the
 * value expression to cut register pressure (also identical).  The loop body
 * needs reshaping against the original register by register; this is a
 * workable target now rather than an unmatchable one. */
/* WHAT IT DOES: pull the next n bits out of a packed bit stream and return
 * them, advancing the read position. The primitive underneath every
 * compressed format the game reads. */
/* @implements 0x10073C90 d3d BrBitStreamReadBits */
#ifdef BR_MATCHING_BUILD
typedef struct { int n; } BrBitStreamReadArg;
/* RESIDUE (12 regnorm, +20 bytes): the original is frameless with ONE stack
 * local -- `push ecx` for the dead `consumed` counter -- and keeps the
 * accumulator in eax for the whole loop. Ours allocates TWO slots because it
 * builds the mask in eax (`mov eax,1; shl; dec; shl`), which evicts the
 * accumulator; the original builds it in ebp and reads the buffer byte
 * through ecx. Everything else now lines up: the else-arm is the original's
 * single `xor <shift>,<shift>`, the buffer byte is read signed, and the
 * bits-available value shares the take register.
 *
 * Probed and dead: swapping the mask and byteIndex statements; folding the
 * AND into the mask variable (`mask &= byte; v = mask >> shift`); and all
 * five sweep variants (/O2 wins). The remaining SIB shape --
 * `[byteIndex + pBuf]` where the original has `[pBuf + byteIndex]` -- is the
 * known emitter residue, see docs/VC5-IDIOMS.md. */
unsigned int __fastcall BrBitStreamReadBits(BrBitStream *pBs,
                                            BrBitStreamReadArg nBits)
{
    unsigned int acc = 0;
    int consumed = 0;

    if (nBits.n == 0)
        return 0;

    do {
        /* ONE variable, not an `avail` and a `take`: the original's
         * else-arm is the single `xor edi,edi` at 0x1006CEFA, which only
         * works because the bits-available value is already sitting in the
         * take register. Two variables cost a copy and a register, and the
         * register is what pushes the accumulator out of eax into a second
         * stack slot. */
        int take = 8 - pBs->readBit;
        int shift;
        int byteIndex;
        unsigned int mask, v;

        if (take > nBits.n) {
            shift = take - nBits.n;
            take  = nBits.n;
        } else {
            shift = 0;
        }

        mask = ((1u << take) - 1u) << shift;
        byteIndex = pBs->readByte;
        /* `movsx ecx, byte ptr [ecx+ebx]` at 0x1006CF10: the buffer byte is
         * read SIGNED. slice1_09.h types pBuf unsigned (right for every other
         * accessor), so the sign is applied here rather than in the header. */
        v    = (mask & (unsigned int)((const signed char *)pBs->pBuf)[byteIndex])
               >> shift;

        pBs->readBit += take;
        acc = (acc << take) | v;

        if (pBs->readBit >= 8) {
            pBs->readBit  = 0;
            pBs->readByte = byteIndex + 1;
        }

        consumed += take;
        nBits.n -= take;
    } while (nBits.n != 0);

    return acc;
}
#else
unsigned int BrBitStreamReadBits(BrBitStream *pBs, int nBits)
{
    unsigned int acc = 0;
    int remaining = nBits;

    if (remaining == 0)
        return 0;

    do {
        int avail = 8 - pBs->readBit;
        int take, shift;
        int byteIndex;
        unsigned int mask, v;

        if (avail > remaining) {
            take  = remaining;
            shift = avail - remaining;
        } else {
            take  = avail;
            shift = 0;
        }

        byteIndex = pBs->readByte;
        mask = ((1u << take) - 1u) << shift;
        v    = (mask & (unsigned int)pBs->pBuf[byteIndex]) >> shift;

        pBs->readBit += take;
        acc = (acc << take) | v;

        if (pBs->readBit >= 8) {
            pBs->readBit  = 0;
            pBs->readByte = byteIndex + 1;
        }

        remaining -= take;
    } while (remaining != 0);

    return acc;
}
#endif

/* 0x10073D40  __thiscall, no stack args. Signed compare (setge). */
/* WHAT IT DOES: reports whether the reader has caught up with the end of the
 * data, counting a partly consumed byte as consumed. */
/* @implements 0x10073D40 d3d BrBitStreamAtEnd */
int BR_THISCALL1 BrBitStreamAtEnd(const BrBitStream *pBs)
{
    /* NOT `pos = readByte; if (readBit) pos++`. That pre-loads readByte and
     * only then tests readBit, so both fields are live at once and the tested
     * value needs its own register (`mov edx,[ecx]; mov eax,[ecx+4]; test
     * edx,edx`). The original loads readBit into eax, tests it, and REUSES eax
     * for readByte -- readBit dies at the test -- which only happens when the
     * step is an alternative ASSIGNMENT rather than an in-place bump. The
     * plain ternary `(readBit != 0) ? readByte + 1 : readByte` is byte-exact
     * too; the if/else is kept for the port's sake. */
    int pos;
    if (pBs->readBit != 0)
        pos = pBs->readByte + 1;
    else
        pos = pBs->readByte;
    return pos >= pBs->writeByte ? 1 : 0;
}

/* 0x10073D60  __thiscall, ret 4. Only the low byte of the argument is used. */
/* WHAT IT DOES: writes one byte into the stream, rounding up to a byte
 * boundary first. */
/* @implements 0x10073D60 d3d BrBitStreamWriteU8 */
#ifdef BR_MATCHING_BUILD
typedef struct { unsigned int v; } BrBitStreamByteArg;
void __fastcall BrBitStreamWriteU8(BrBitStream *pBs, BrBitStreamByteArg v)
{
    BrBitStreamAlignWrite(pBs);
    pBs->pBuf[pBs->writeByte] = (unsigned char)v.v;
    pBs->writeByte++;
}
#else
void BrBitStreamWriteU8(BrBitStream *pBs, unsigned int v)
{
    BrBitStreamAlignWrite(pBs);
    pBs->pBuf[pBs->writeByte] = (unsigned char)v;
    pBs->writeByte++;
}
#endif

/* 0x10073D80  big-endian 16-bit (glide 0x1006CFC0). */
/* WHAT IT DOES: writes a two-byte number into the stream, most significant
 * byte first. */
/* @implements 0x1006CFC0 glide BrBitStreamWriteU16 */
#ifdef BR_MATCHING_BUILD
/* thiscall, ret 4; the argument is a SHORT (`mov ax,[esp+0xc]`).
 *
 * BYTE-EXACT.  Same two-byte residue as WriteU32 and the same cause -- the
 * first store's `mov edx,[esi+0x10]` / `mov edi,[esi+0xc]` pair came out
 * swapped -- but this one needs BOTH halves named, not just the pointer:
 * WriteU32's `pb` local alone leaves it at 2 diffs, and `pb` plus `w` closes
 * it.  The difference between the two is the argument width: WriteU32 already
 * has a full-register `x` local for the value, WriteU16 takes the byte
 * straight out of `ah`, so there is one less live value to order around.
 *
 * PROBED AND DEAD, do not re-run (all at 52 B / 21 insns / 2 diffs unless
 * noted): `*(pBs->writeByte + pBs->pBuf)`; `*(pBs->pBuf + pBs->writeByte)`;
 * a `unsigned short x = v.v` local; naming `pb` for BOTH stores instead of
 * the first.  And WORSE: a `unsigned int x = v.v` local, with or without
 * `pb`, widens the argument load and costs 8 bytes (60 B, 23 insns) -- the
 * original's `mov ax,` is a WORD load and an int local destroys it; a
 * `unsigned char hi` value local costs 3 (55 B). */
typedef struct { unsigned short v; } BrBitStreamWordArg;
void __fastcall BrBitStreamWriteU16(BrBitStream *pBs, BrBitStreamWordArg v)
{
    unsigned char *pb;
    int            w;

    BrBitStreamAlignWrite(pBs);
    pb = pBs->pBuf;
    w  = pBs->writeByte;
    pb[w] = (unsigned char)(v.v >> 8);
    pBs->writeByte++;
    pBs->pBuf[pBs->writeByte] = (unsigned char)v.v;
    pBs->writeByte++;
}
#else
void BrBitStreamWriteU16(BrBitStream *pBs, unsigned int v)
{
    int w;
    BrBitStreamAlignWrite(pBs);
    w = pBs->writeByte;
    pBs->pBuf[w]     = (unsigned char)(v >> 8);
    pBs->pBuf[w + 1] = (unsigned char)v;
    pBs->writeByte = w + 2;
}
#endif

/* 0x10073DC0  big-endian 24-bit. */
/* WHAT IT DOES: writes a three-byte number into the stream, most significant
 * byte first. */
/* @implements 0x10073DC0 d3d BrBitStreamWriteU24 */
#ifdef BR_MATCHING_BUILD
/* thiscall.  Size-exact (70) but register-walled on the first pair:
 * original loads writeByte into edx and pBuf into edi; VC5 swaps them.
 * Naming writeByte first dropped below orig size. */
void __fastcall BrBitStreamWriteU24(BrBitStream *pBs, BrBitStreamByteArg v)
{
    unsigned int x = v.v;
    BrBitStreamAlignWrite(pBs);
    pBs->pBuf[pBs->writeByte] = (unsigned char)(x >> 16);
    pBs->writeByte++;
    pBs->pBuf[pBs->writeByte] = (unsigned char)(x >> 8);
    pBs->writeByte++;
    pBs->pBuf[pBs->writeByte] = (unsigned char)x;
    pBs->writeByte++;
}
#else
void BrBitStreamWriteU24(BrBitStream *pBs, unsigned int v)
{
    int w;
    BrBitStreamAlignWrite(pBs);
    w = pBs->writeByte;
    pBs->pBuf[w]     = (unsigned char)(v >> 16);
    pBs->pBuf[w + 1] = (unsigned char)(v >> 8);
    pBs->pBuf[w + 2] = (unsigned char)v;
    pBs->writeByte = w + 3;
}
#endif

#ifdef BR_MATCHING_BUILD
/* br_obj.h's BrObjClear (0x10073B80 / glide 0x1006CDC0), redeclared over
 * BrBitStream -- same layout, this TU does not pull br_obj.h. */
void BR_THISCALL1 BrObjClear(BrBitStream *pBs);
extern int DAT_1184c070;

/* WHAT IT DOES: reset the message stream and write the 24-bit id at
 * 0x1184C070 as its header. */
/* @implements 0x1006AB60 glide BrObjResetMsgHdr */
void BrObjResetMsgHdr(BrBitStream *pBs)
{
    BrBitStreamByteArg a;

    BrObjClear(pBs);
    a.v = (unsigned int)DAT_1184c070;
    BrBitStreamWriteU24(pBs, a);
}
#endif

/* 0x10073E10  big-endian 32-bit. */
/* WHAT IT DOES: writes a four-byte number into the stream, most significant
 * byte first. */
/* @implements 0x10073E10 d3d BrBitStreamWriteU32 */
#ifdef BR_MATCHING_BUILD
/* thiscall.  BYTE-EXACT.  The last two bytes were the FIRST store's two
 * address loads coming out in the wrong order: the original loads pBuf into
 * edx and writeByte into edi, VC5 the reverse.  Naming pBuf in a local that is
 * assigned AFTER the align call and used for that one store flips the pair.
 * Every later store keeps the plain `pBs->pBuf[pBs->writeByte]` form, which is
 * index-first and already matched -- so the local goes on the FIRST store
 * only.  See the WriteU16 note for the rest of the rule. */
void __fastcall BrBitStreamWriteU32(BrBitStream *pBs, BrBitStreamByteArg v)
{
    unsigned int   x = v.v;
    unsigned char *pb;

    BrBitStreamAlignWrite(pBs);
    pb = pBs->pBuf;
    pb[pBs->writeByte] = (unsigned char)(x >> 24);
    pBs->writeByte++;
    pBs->pBuf[pBs->writeByte] = (unsigned char)(x >> 16);
    pBs->writeByte++;
    pBs->pBuf[pBs->writeByte] = (unsigned char)(x >> 8);
    pBs->writeByte++;
    pBs->pBuf[pBs->writeByte] = (unsigned char)x;
    pBs->writeByte++;
}
#else
void BrBitStreamWriteU32(BrBitStream *pBs, unsigned int v)
{
    int w;
    BrBitStreamAlignWrite(pBs);
    w = pBs->writeByte;
    pBs->pBuf[w]     = (unsigned char)(v >> 24);
    pBs->pBuf[w + 1] = (unsigned char)(v >> 16);
    pBs->pBuf[w + 2] = (unsigned char)(v >> 8);
    pBs->pBuf[w + 3] = (unsigned char)v;
    pBs->writeByte = w + 4;
}
#endif

/* ================================================================== */
/* Float math                                                          */
/* ================================================================== */

/* BrVec3Normalise / BrVec4Normalise live in br_vecnorm.c. */

/* 0x100747C0.
 * Written out longhand rather than with temporaries so that the write order
 * matches the original exactly: each output component is zeroed and fully
 * accumulated before the next one begins, and the translation row is added
 * to all three only afterwards. That ordering is observable when pOut
 * aliases pV. */
/* WHAT IT DOES: moves a point through a transform: rotates and scales it by
 * the matrix and then adds the matrix's translation. The awkward longhand
 * here is deliberate, because the original writes each result component out
 * before starting the next, which is visible if the caller passes the same
 * point as both input and output. */
/* @implements 0x1006DA20 glide BrMat4TransformPoint */
/* @implements 0x100747C0 d3d BrMat4TransformPoint */
void BrMat4TransformPoint(BrVec3 *pOut, const BrMat4 *pM, const BrVec3 *pV)
{
    /* Orig is two counted loops (ebp=3 outer, esi=3 inner), not unrolled
     * products: `mov [eax],0`; inner `fld [v]; fmul [m]; add m,0x10; add v,4;
     * dec esi; fadd [eax]; fstp [eax]`.  `sub edi,eax` is pM-pOut so the
     * column pointer is `lea r,[edi+eax]` as eax walks the output.
     *
     * INDEXED, NOT CURSORS.  Hand-rolled walking pointers (`col = m; v = pV;`
     * bumped by `col += 4; v++`) reproduce this exactly to 7 bytes and then
     * stop: VC5 binds the copy-from-register cursor to ecx and the lea-derived
     * one to edx, where the original has them the other way round, and the lea
     * comes out `[eax+edi]` instead of `[edi+eax]`.  Swapping the assignment
     * order, swapping the declaration order and block-scoping the pair inside
     * the outer loop all fail (9 / 7 / 5 diffs) -- a previous note here called
     * this a register-allocation wall and told the reader not to grind it, and
     * that was WRONG.  Letting the compiler build both induction variables
     * itself, from plain `pv[j]` and `pM->m[j][i]` subscripts, is byte-exact:
     * the two cursors then come into existence in the order VC5 wants them
     * and pick up ecx/edx accordingly.  Semantics are unchanged -- `pv[j]`
     * re-reads the live vector every outer pass, exactly as the reloaded
     * cursor did, which is what keeps the aliasing case above honest. */
    float       *o  = (float *)pOut;
    const float *pv = (const float *)pV;
    int i, j;

    for (i = 0; i < 3; i++) {
        o[i] = 0.0f;
        for (j = 0; j < 3; j++)
            o[i] += pv[j] * pM->m[j][i];
    }
    pOut->x += pM->m[3][0];
    pOut->y += pM->m[3][1];
    pOut->z += pM->m[3][2];
}

/* ================================================================== */
/* Entity array offsets                                                */
/* ================================================================== */

/* 0x100307D0 -- BrMat4Identity (br_mat.h). Reproduced as a static for the
 * same reason as BrBitStreamAlignRead above: 0x10076C90 calls it and this
 * translation unit must link on its own.
 * DEVIATION: duplicate of an existing symbol, kept private (static). */
/* WHAT IT DOES: resets a transform matrix to "no transform at all" -- ones
 * down the diagonal, zeroes everywhere else -- so whatever it is applied to
 * comes through unchanged. */
/* @implements 0x100307D0 d3d BrMat4IdentityLocal */
#ifdef BR_MATCHING_BUILD
/* The original is fully unrolled: sixteen sequential stores, 1.0f and 0.0f
 * hoisted into edx/ecx as integer patterns. */
static void BrMat4IdentityLocal(BrMat4 *pM)
{
    pM->m[0][0] = 1.0f; pM->m[0][1] = 0.0f; pM->m[0][2] = 0.0f; pM->m[0][3] = 0.0f;
    pM->m[1][0] = 0.0f; pM->m[1][1] = 1.0f; pM->m[1][2] = 0.0f; pM->m[1][3] = 0.0f;
    pM->m[2][0] = 0.0f; pM->m[2][1] = 0.0f; pM->m[2][2] = 1.0f; pM->m[2][3] = 0.0f;
    pM->m[3][0] = 0.0f; pM->m[3][1] = 0.0f; pM->m[3][2] = 0.0f; pM->m[3][3] = 1.0f;
}
#else
static void BrMat4IdentityLocal(BrMat4 *pM)
{
    int r, c;
    for (r = 0; r < 4; ++r)
        for (c = 0; c < 4; ++c)
            pM->m[r][c] = (r == c) ? 1.0f : 0.0f;
}
#endif

/* 0x10076AE0  __thiscall, ret 4. `cmp eax,0x10 / jl` -- signed. */
/* WHAT IT DOES: records which of two banks of sixteen an object belongs to.
 * Anything numbered sixteen or above is stored as the second bank with its
 * number reduced by sixteen; anything below it is the first bank. */
/* @implements 0x10076AE0 d3d BrEntitySetIndex */
#ifdef BR_MATCHING_BUILD
/* thiscall, one stack arg.  Size-exact (50) but encoding-walled:
 * original `sub eax, 0x10`, VC5 `add eax, -0x10`.  `i - 16`, `i -= 16`,
 * unsigned subtract, and inline-in-store all emit the add form under
 * every VC5 flag probed (/O1 /O2 /Os /Ox /G3 /G5, C and C++ front ends).
 * VC4.2 DOES emit `sub eax,0x10` -- but schedules it AFTER the bank
 * store for every probed source order, where the original subs first.
 * Neither compiler reproduces both; parked. */
typedef struct { int n; } BrEntityIndexArg;
/* RESIDUE 2 bytes, FIRSTDIV +0xa, and the whole of it is one instruction:
 * the original has `sub eax,0x10` where we emit `add eax,-0x10`.  That is NOT
 * a spelling choice -- MSVC5 canonicalises every straight-line constant
 * subtraction to add-negative.  Probed and DEAD, do not re-run: `i = i - 16`,
 * `i -= 0x10`, the subtraction inlined into the store, `i = index.n - 16`, a
 * const-propagated `base` local, an in-place bump on the parameter member,
 * and the compile variants /O2 /Op, /O2 /Oy-, /O1 and /Ox.  An isolated
 * one-line probe confirms the rule for int, long, unsigned, short and both
 * pointer spellings.  See the `sub reg, imm` entry in docs/VC5-IDIOMS.md: the
 * three MSVC5 constructs known to keep a real `sub` are a loop-carried
 * decrement, a 16-bit-typed subtraction whose result stays live narrow, and a
 * pointer difference feeding further arithmetic -- this function fits none of
 * them, so the answer is still open.  It is NOT the operator. */
void __fastcall BrEntitySetIndex(void *pEntity, BrEntityIndexArg index)
{
    int i = index.n;
    if (i >= 16) {
        i -= 16;
        *(int *)((unsigned char *)pEntity + BR_ENTITY_OFF_BANK)  = 1;
        *(int *)((unsigned char *)pEntity + BR_ENTITY_OFF_INDEX) = i;
    } else {
        *(int *)((unsigned char *)pEntity + BR_ENTITY_OFF_BANK)  = 0;
        *(int *)((unsigned char *)pEntity + BR_ENTITY_OFF_INDEX) = i;
    }
}
#else
void BrEntitySetIndex(void *pEntity, int index)
{
    unsigned char *p = (unsigned char *)pEntity;
    int *pIndex = (int *)(void *)(p + BR_ENTITY_OFF_INDEX);
    int *pBank  = (int *)(void *)(p + BR_ENTITY_OFF_BANK);

    if (index >= 16) {
        *pBank  = 1;
        *pIndex = index - 16;
    } else {
        *pBank  = 0;
        *pIndex = index;
    }
}
#endif

/* 0x10076C90  __thiscall.
 *
 * The original is  idx = (this - 0x10ACDEA8) / 0x2B68  performed with the
 * magic multiply 0x5E5D422B followed by `sar edx,12` and the usual
 * shr/add sign fix -- i.e. plain signed division truncating toward zero.
 * The 348-byte scale is assembled as 348 = ((idx*8 - idx)*4 + idx)*3*4
 * through three LEAs.
 *
 * DEVIATION: the two array bases are parameters instead of the hardcoded
 * 0x10ACDEA8 / 0x106C6678, and the aux pointer is stored as a host pointer
 * (8 bytes on a 64-bit build) where the original stored a 32-bit value. The
 * two strides below are the ORIGINAL 32-bit strides and are not adjusted --
 * they are the sizes of the game's own structures, not of anything declared
 * here. */
/* WHAT IT DOES: links a world object to its matching record in a second,
 * parallel array, by working out how far along the main array the object
 * sits and stepping the same distance into the other one, and then resets
 * the object's transform matrix to no transform. */
/* @implements 0x10076C90 d3d BrEntityBindAux */
/* @n64 0x802207A4 located */
#ifdef BR_MATCHING_BUILD
/* thiscall, no stack args. Both array bases are pinned globals; the index
 * is a signed magic-divide by the 0x2B68 entity stride. */
extern char DAT_10af1208;   /* entity[0] */
extern char DAT_106ed708;   /* aux[0], stride 348 */

void __fastcall BrEntityBindAux(void *pThis, int _edx_unused)
{
    char *p  = (char *)pThis;
    int  idx = (int)((p - &DAT_10af1208) / BR_ENTITY_STRIDE);

    (void)_edx_unused;
    *(void **)(p + BR_ENTITY_OFF_AUX) =
        &DAT_106ed708 + idx * BR_ENTITY_AUX_STRIDE;
    BrMat4IdentityLocal((BrMat4 *)(void *)(p + BR_ENTITY_OFF_MATRIX));
}
#else
void BrEntityBindAux(void *pEntity, void *pEntityArrayBase,
                     void *pAuxArrayBase)
{
    unsigned char *p    = (unsigned char *)pEntity;
    unsigned char *pAux = (unsigned char *)pAuxArrayBase;
    ptrdiff_t      idx  = (p - (unsigned char *)pEntityArrayBase)
                          / BR_ENTITY_STRIDE;
    void **ppAux = (void **)(void *)(p + BR_ENTITY_OFF_AUX);

    *ppAux = pAux + idx * BR_ENTITY_AUX_STRIDE;
    BrMat4IdentityLocal((BrMat4 *)(void *)(p + BR_ENTITY_OFF_MATRIX));
}
#endif

/* ================================================================== */
/* Misc                                                                */
/* ================================================================== */

/* 0x10073A10 (PARTIAL).
 *
 * The full original does three things: two calls through the import pointer
 * at 0x118AA0B0 with fourteen constant arguments (an unidentified 0x40x0x40
 * surface/texture creation), then this table build, then a call to
 * sub_100098A0(dst=0x11829118, src=0x11829330, size=0x40, format=2) whose
 * byte return is divided by 16. Only the table build is portable and
 * identifiable, so only it is ported; the rest is reported as skipped.
 *
 * The loop bound in the original is `cmp eax, 0x11829371 / jl` against a
 * cursor that starts at base+1 and steps 4, giving exactly 16 iterations
 * over a 0x40-byte table at 0x11829330. */
/* WHAT IT DOES: fills a sixteen-entry colour table: every entry pure white,
 * with the transparency stepping evenly from fully see-through to fully
 * solid. Only this table build is transcribed; the rest of the original
 * routine creates two textures through an unidentified backend call and is
 * not ported. */
/* 0x10073A10 CARRIES NO @implements LINE.  The banner above already called
 * this PARTIAL and it is: the original is four statements and this is one of
 * them, 37 of its 167 bytes.  The two surface creations write 0x11829100 and
 * 0x11829104 and the sub_100098A0 call writes 0x11829318 -- three globals a
 * caller of 0x10073A10 gets and a caller of this does not.  The manifest form
 * is whole-function only, so "@implements 0x10073A10" asserted all four, the
 * address counted as ported, and nobody was going to come back for the other
 * three.  It is better read as unported until the backend call at 0x118AA0B0
 * is identified.  The table build itself stands and is used; only the CLAIM
 * was wrong. */
void BrAlphaRampBuild(unsigned char *pOut)
{
    int i;
    for (i = 0; i < 16; ++i) {
        pOut[i * 4 + 0] = 0xFF;
        pOut[i * 4 + 1] = 0xFF;
        pOut[i * 4 + 2] = 0xFF;
        pOut[i * 4 + 3] = (unsigned char)((i << 4) | i);
    }
}

/* 0x10074F70.
 *
 * DEVIATION: the original brackets the whole body with
 * WaitForSingleObject(g_18AA0A0, INFINITE) / ReleaseMutex(g_18AA0A0). The
 * mutex is dropped here -- callers must serialise. The ring itself is
 * otherwise verbatim, including the fact that the write index is stored
 * back before the bounds test and then overwritten with 0 when it reached
 * 0x100. There is no read cursor and no fullness check anywhere in the
 * original: entry 0 is simply overwritten on the 257th push. */
void BrPairRingPush(BrPairRing *pRing, int a, int b)
{
    int i = pRing->write;

    pRing->aItems[i].a = a;
    pRing->aItems[i].b = b;

    i++;
    pRing->write = i;
    if (i >= BR_PAIR_RING_SLOTS)
        pRing->write = 0;
}

/* 0x10075100.
 *
 * The original calls the platform timer at 0x10075020 for `ms` and then
 * does the arithmetic below with `div esi` (esi = 100) and two unsigned
 * magic multiplies: 0x51EB851F >> 37 is ms/100 and 0x3E0F83E1 >> 35 is
 * (ms % 100) / 33.
 *
 * DEVIATION: `ms` is a parameter instead of a call into
 * QueryPerformanceCounter / timeGetTime (0x10075020, skipped -- see below).
 * Everything else is verbatim, including the order of the three stores. */
/* WHAT IT DOES: converts a time in milliseconds into the game's own clock,
 * which counts thirty ticks a second -- the N64's frame rate, kept in the PC
 * build. It also clears one other field of the timing record. */
/* port-only body; the Glide twin is src/core/generated/0x1006E360.c -- the
 * original takes NO arguments: it calls 0x1006E280 for the millisecond count
 * and writes three absolute globals, so neither parameter here exists. */
void BrTimeUpdate(BrTimeState *pState, unsigned int ms)
{
    pState->f12C   = 0;
    pState->ms     = ms;
    pState->tick30 = (ms % 100u) / 33u + 3u * (ms / 100u);
}

/* ==================================================================
 * SKIPPED, with reasons
 * ==================================================================
 *
 * Already implemented elsewhere (not re-done):
 *   0x10073B40 0x10073B80 0x10073D20 0x10073F40 0x10073F50  br_obj.h
 *   0x10074030  BrHandleLookup (br_bits.h)
 *   0x10074720 0x10074770                                   br_mat.h
 *
 * Windows / COM / platform-only, nothing portable inside:
 *   0x100734F0  tears down the g_1828F48 object and clears a 0x48-stride
 *               table plus a 60-byte block; pure global bookkeeping around
 *               two calls into an unrecovered class.
 *   0x10073560  DirectSound-family init: GlobalAlloc/GlobalLock a 0x12-byte
 *               descriptor, fill it (0x5622, 0x15888, 4, 0x10, 0),
 *               CoCreateInstance, then five vtable calls with the usual
 *               release-on-failure ladder. Nothing to port.
 *   0x10073950  one call through the import at 0x118AA0B0 with 14 constant
 *               arguments (0x40 x 0x40, format 4). Callee unidentified.
 *   0x100739E0  the same call with a different source and all-zero flags.
 *   0x100770F0  COM/DirectSound init behind a +1 refcount guard; three
 *               vtable calls, each with its own bail-out.
 *   0x10078CD0  SEH frame, MessageBoxA on failure, two more vtable calls.
 *   0x10075020  QueryPerformanceFrequency / QueryPerformanceCounter with a
 *               64-bit multiply/divide pair (0x1007ED20, 0x1007FD10) and a
 *               timeGetTime fallback. The portable core is
 *               ms = (ticks * 1000 + 500) / freq  -- note the +500, i.e.
 *               round-to-nearest, not truncation -- minus a base captured on
 *               the first call. Not ported because it is entirely a wrapper
 *               over two Win32 clocks.
 *   0x10076CE0 0x10076E90 0x10076ED0 0x10076FA0  RIFF/WAVE loading built
 *               entirely on WINMM's mmio* API. Error codes, for whoever
 *               reimplements them: 0xE000 out of memory, 0xE100 open
 *               failed, 0xE101 malformed/short RIFF, 0xE102 short read of
 *               the fmt chunk, 0xE103 mmio buffer exhausted mid-copy.
 *               0x10076CE0 additionally hardcodes the assumption that a
 *               PCM (wFormatTag == 1) header has no cbSize field and reads
 *               only 16 bytes for it.
 *
 * Layout not established well enough to port:
 *   0x100770C0  zeroes 14 dwords at 0x118ABD38, zeroes 0x118ABAD4 and sets
 *               0x118ABD80 to 1. The three globals are 0x2A4 apart and
 *               0x48 apart respectively with nothing to tie them into one
 *               structure, so any struct here would be invented.
 */

#ifdef BR_MATCHING_BUILD
/* 0x100739B0
 *
 * Fourteen constant arguments through the backend texture constructor at
 * 0x118AA0B0 -- the same cdecl as 0x10073980, last-arg-first: 0x40 x 0x40,
 * fmt 0, siz 4, source 0x100B94A8, result stored at 0x11829314. */
/* WHAT IT DOES: turns a baked-in 64-by-64 picture into a texture the rest of
 * the game can draw with, and remembers the handle the graphics backend
 * returns. */
/* @implements 0x100739B0 d3d BrSub100739B0 */
typedef void *(*BrSub100739B0Fn)(void *pSrc, int a2, int w, int h,
                                 int fmt, int siz, int b31, int b30,
                                 int b29, int b28, int a11, int a12,
                                 int a13, int a14);

extern BrSub100739B0Fn g_18AA0B0;     /* 0x118AA0B0 */
extern void           *g_1829314;     /* 0x11829314 */
extern unsigned char   g_0B94A8[];    /* 0x100B94A8 */

void BrSub100739B0(void)
{
    g_1829314 = g_18AA0B0(g_0B94A8, 0, 0x40, 0x40, 0, 4,
                          0, 0, 0, 0, 0, 0, 1, 0);
}
#endif

/* ── Ghidra-matched functions ─────────────────────────── */
#ifdef BR_MATCHING_BUILD
typedef int (*funcptr)();
#include <windows.h>
extern int DAT_100b84a8;
extern int DAT_104af5c8;
extern int DAT_104b05c8;
extern int DAT_1184c480;
extern int _DAT_1184c460;
extern int _DAT_1184c464;
extern int g_br18AB118_S_S1499;
extern funcptr g_pfn18AA0B0;



/* WHAT IT DOES: return the current timer subsystem state. */
/* @implements 0x1006E350 glide BrGetTimerState */

int BrGetTimerState(void)

{
  return g_br18AB118_S_S1499;
}


extern int DAT_117b3250;
extern int DAT_11849e60;
extern int DAT_1184c070;
extern int DAT_1184c074;
extern int g_aBr178FEF8;
extern int g_aBrPeer71;

/* WHAT IT DOES: the networking worker thread's wait loop: blocks until
 * either the quit event or a peer's mutex is signalled, exits the thread on
 * quit, and otherwise checks each peer's state and bails out of the scan as
 * soon as one is not ready. */
/* @implements 0x1006A650 glide FUN_1006a650 */
/* auto-filed from ghidra --refine; transforms: as-is */

void FUN_1006a650(void)
{
  DWORD wr;
  int *pPeer;
  int *pAlt;
  HANDLE h1[2];
  HANDLE h2[2];
  char skip;
  int st;
  int t;

  pPeer = &g_aBrPeer71;
  do {
    h1[0] = (HANDLE)DAT_11849e60;
    h1[1] = (HANDLE)*pPeer;
    wr = WaitForMultipleObjects(2, h1, 0, 0xffffffff);
    if (wr == 0) {
      ExitThread(0);
    }
    st = pPeer[0xb] & 0x3f;
    if (st < 2 || st == 3) {
      skip = 0;
    }
    else {
      skip = 1;
    }
    ReleaseMutex((HANDLE)*pPeer);
    if (skip) {
      return;
    }
    pPeer = pPeer + 0x25b;
  } while ((int)pPeer < 0x117b3248);

  pAlt = &g_aBr178FEF8;
  pPeer = &g_aBrPeer71;
  for (;;) {
    h1[0] = (HANDLE)DAT_11849e60;
    h1[1] = (HANDLE)*pPeer;
    wr = WaitForMultipleObjects(2, h1, 0, 0xffffffff);
    if (wr == 0) {
      ExitThread(0);
    }
    st = pPeer[0xb];
    skip = ((st & 0x3f) == 3);
    ReleaseMutex((HANDLE)*pPeer);
    if (skip) {
      h2[0] = (HANDLE)DAT_11849e60;
      h2[1] = (HANDLE)*pAlt;
      wr = WaitForMultipleObjects(2, h2, 0, 0xffffffff);
      if (wr == 0) {
        ExitThread(0);
      }
      st = pAlt[0xb];
      skip = ((st & 0x3f) != 3);
      ReleaseMutex((HANDLE)*pAlt);
      if (skip) {
        return;
      }
    }
    pPeer = pPeer + 0x25b;
    pAlt = pAlt + 0x280b;
    if ((int)pPeer >= 0x117b3248) {
      pPeer = &g_aBrPeer71;
      t = 4;
      do {
        h2[0] = (HANDLE)DAT_11849e60;
        h2[1] = (HANDLE)*pPeer;
        wr = WaitForMultipleObjects(2, h2, 0, 0xffffffff);
        if (wr == 0) {
          ExitThread(0);
        }
        if ((pPeer[0xb] & 0x3f) == 3) {
          pPeer[0xb] = t;
          DAT_117b3250 = 1;
          DAT_1184c074 = DAT_1184c070 + 3000;
        }
        ReleaseMutex((HANDLE)*pPeer);
        pPeer = pPeer + 0x25b;
      } while ((int)pPeer < 0x117b3248);
      return;
    }
  }
}

#endif /* BR_MATCHING_BUILD */
