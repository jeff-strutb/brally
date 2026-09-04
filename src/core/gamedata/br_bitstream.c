/* br_bitstream.c -- gamedata: the packed bit/byte stream.
 *
 * The reader and writer over a shared buffer that every compressed format
 * the game reads and every network message it sends goes through. Filed out
 * of slice1_09.c as one block: BrBitStreamAlignRead is file-static and every
 * byte-at-a-time reader calls it, so the translation unit moves whole.
 * BrObjResetMsgHdr (0x1006AB60) travels with it because it is spelled in
 * terms of this file's __fastcall BrBitStreamWriteU24 and its argument
 * struct.
 *
 * See slice1_09.h for the recovered layouts and the argument-order notes.
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

