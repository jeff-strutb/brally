/* test_slice6_74.c -- behavioural tests for packet 74.
 *
 * The substance of this packet is one transcribed function, 0x10073E70
 * (BrBitStreamWriteBits), and it is tested the way CONVENTIONS asks: against
 * PROPERTIES, not against numbers this port happened to produce.
 *
 *   - a round-trip against slice1_09's READER, which is the real check: the
 *     two functions are the write/read halves of one class, so "write n bits
 *     then read n bits gives the value back" is an identity the original must
 *     satisfy and a wrong bit order cannot fake;
 *   - agreement with an INDEPENDENT model of an MSB-first bit writer, written
 *     here from the specification rather than from the transcription, so a
 *     shared misreading cannot pass;
 *   - the boundary conditions actually present in the original: nBits == 0,
 *     writes that exactly fill a byte, writes that straddle a byte, a 32-bit
 *     write, and a caller-supplied writeBit of 8;
 *   - the aliasing/merging property the original's mask exists for: bits
 *     already in the byte survive.
 *
 * The reader is reimplemented here rather than linked, for two reasons: the
 * house build links each test against its own module only, and an independent
 * reader is a stronger oracle than the shipped one.
 *
 * The adapters are tested for the only thing an adapter can get wrong --
 * whether it forwards its arguments unchanged and returns what the callee
 * returned -- using recording stand-ins for the six cross-module bodies.
 *
 * Every stand-in for a cross-slice symbol lives in THIS file and nowhere else,
 * as the contract requires.
 */
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>

#include "slice6_74.h"
#include "br_vec.h"
#include "br_mat.h"
#include "slice1_09.h"

/* ==========================================================================
 * The functions under test.
 *
 * slice6_74.h deliberately does not re-declare these: every one of them
 * already has a declaration in the module that CALLS it, and that declaration
 * is the contract this packet has to satisfy.  Re-declaring them in the
 * module's own header would create a second place for the signature to drift.
 *
 * So the test does what the .c does and what port/host/brally.c does before
 * it: it copies each prototype VERBATIM from the header that owns it, named
 * below.  If an owner ever changes one, this file stops linking -- which is
 * the point.
 * ========================================================================== */

/* slice3_42.h -- incomplete on purpose; only its address is ever forwarded. */
struct BrCarState;

/* slice2_12.h */ void     BrBitStreamWriteBits(BrBitStream *pBs, int32_t value,
                                                int32_t nBits);
/* slice2_19.h */ void     BrSub10074DC0(int n);
/* slice1_02.h */ struct BrBitReader;
/* slice1_02.h */ uint32_t BrBitReaderRead(struct BrBitReader *pReader,
                                           unsigned nBits);
/* slice2_21.h */ float    BrVec3Len(const BrVec3 *pV);
/* slice2_21.h */ int32_t  BrFtolArg(float f);
/* slice3_31.h */ void     BrExt_10072AF0(int32_t a, uint32_t b);
/* slice2_25.h */ void     BrSub1003CDA0(void);
/* slice3_40.h */ void     BrSub100695D0(void *pDst220,
                                         const struct BrCarState *pState);
/* slice1_02.h */ void     BrNetMutexLock(void *hMutex);
/* slice1_02.h */ void     BrNetMutexUnlock(void *hMutex);
/* slice2_26.h */ void     BrExt_10008B80(void);
/* slice2_17.c */ void     BrStub10008B80(intptr_t a0, ...);
/* slice2_18.h */ void     BrStub8B80_0(void);
/* slice2_18.h */ void     BrStub8B80_1i(int32_t a0);
/* slice2_18.h */ void     BrStub8B80_1p(const void *p0);
/* slice2_18.h */ void     BrStub8B80_5i(int32_t a0, int32_t a1, int32_t a2,
                                         int32_t a3, int32_t a4);

static int g_fails;

#define CHECK(cond)                                                      \
    do {                                                                      \
        if (!(cond)) {                                                        \
            printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);            \
            ++g_fails;                                                        \
        }                                                                     \
    } while (0)

/* ==========================================================================
 * STAND-INS for cross-slice symbols.  Test scaffolding only.
 *
 * Each records what it was handed and returns a value the caller cannot have
 * invented, so "the adapter forwarded correctly" and "the adapter returned the
 * callee's answer" are both observable.
 * ========================================================================== */

/* --- slice1_09.h, 0x10073C90 -- the read half of the bit-stream class ----- */
static BrBitStream *s_pReadArg;
static int          s_nReadBitsArg;
static unsigned int s_readResult;

unsigned int BrBitStreamReadBits(BrBitStream *pBs, int nBits)
{
    s_pReadArg     = pBs;
    s_nReadBitsArg = nBits;
    return s_readResult;
}

/* --- br_vec.h, 0x1003B170 ------------------------------------------------ */
static const BrVec3 *s_pLenArg;
static float         s_lenResult;

float BrVec3Length(const BrVec3 *pV)
{
    s_pLenArg = pV;
    return s_lenResult;
}

/* --- br_crt.h, 0x1007C8A0 ------------------------------------------------ */
static float   s_ftolArg;
static int32_t s_ftolResult;

int32_t BrFtolTrunc(float f)
{
    s_ftolArg = f;
    return s_ftolResult;
}

/* --- slice1_08.h, 0x10072AF0 --------------------------------------------- */
static int32_t  s_sndGroup;
static uint32_t s_sndPacked;
static int      s_sndCalls;

int32_t BrSndPlaySimple(int32_t group, uint32_t packed)
{
    s_sndGroup  = group;
    s_sndPacked = packed;
    ++s_sndCalls;
    return 12345;              /* the value the void adapter must discard */
}

/* --- slice6_72.h, 0x1003CDA0 --------------------------------------------- */
static int s_cdA0Calls;

void BrExt_1003CDA0(void)
{
    ++s_cdA0Calls;
}

/* --- slice3_42.h, 0x100695D0 --------------------------------------------- */
struct BrCarState;
static BrMat4                   *s_pMatOut;
static const struct BrCarState  *s_pMatSrc;

void BrMat4FromCarState(BrMat4 *pOut, const struct BrCarState *pSrc)
{
    s_pMatOut = pOut;
    s_pMatSrc = pSrc;
}

/* ==========================================================================
 * An INDEPENDENT MSB-first bit writer and reader, written from the
 * specification in slice1_09.h rather than from slice6_74.c.
 * ========================================================================== */

/* Append the low nBits of value to buf, most significant bit first, starting
 * at absolute bit position *pPos (bit 0 being the top bit of byte 0). */
static void RefWriteBits(unsigned char *buf, int *pPos, uint32_t value, int nBits)
{
    int i;

    for (i = nBits - 1; i >= 0; --i) {
        int bit  = (int)((value >> i) & 1u);
        int pos  = (*pPos)++;
        int byte = pos >> 3;
        int shift = 7 - (pos & 7);

        buf[byte] = (unsigned char)((buf[byte] & ~(1u << shift)) |
                                    ((unsigned)bit << shift));
    }
}

static uint32_t RefReadBits(const unsigned char *buf, int *pPos, int nBits)
{
    uint32_t v = 0;
    int i;

    for (i = 0; i < nBits; ++i) {
        int pos  = (*pPos)++;
        int byte = pos >> 3;
        int shift = 7 - (pos & 7);

        v = (v << 1) | (uint32_t)((buf[byte] >> shift) & 1u);
    }
    return v;
}

/* ==========================================================================
 * 1. 0x10073E70 -- BrBitStreamWriteBits
 * ========================================================================== */

/* The stream the port writes through, and the reference buffer beside it. */
static unsigned char s_buf[64];
static unsigned char s_ref[64];
static BrBitStream   s_bs;
static int           s_refPos;

static void StreamReset(void)
{
    memset(s_buf, 0, sizeof(s_buf));
    memset(s_ref, 0, sizeof(s_ref));
    memset(&s_bs, 0, sizeof(s_bs));
    s_bs.pBuf = s_buf;
    s_refPos  = 0;
}

/* Write through both and assert they still agree, bit for bit and cursor for
 * cursor.  The cursor comparison is the part that catches an off-by-one that
 * happens to produce the right bytes for one sequence. */
static void WriteBoth(uint32_t value, int nBits)
{
    BrBitStreamWriteBits(&s_bs, (int32_t)value, nBits);
    RefWriteBits(s_ref, &s_refPos, value, nBits);

    CHECK(memcmp(s_buf, s_ref, sizeof(s_buf)) == 0);
    CHECK(s_bs.writeByte == s_refPos / 8);
    CHECK(s_bs.writeBit  == s_refPos % 8);
}

static void TestWriteBitsAgainstModel(void)
{
    /* A run of awkward widths that crosses byte boundaries repeatedly and
     * never lands on one by luck. */
    static const int aWidths[] = { 1, 3, 7, 2, 5, 11, 1, 6, 13, 4, 9, 8, 17 };
    unsigned i;
    uint32_t v = 0x9E3779B9u;                     /* an arbitrary bit soup */

    StreamReset();
    for (i = 0; i < sizeof(aWidths) / sizeof(aWidths[0]); ++i) {
        WriteBoth(v, aWidths[i]);
        v = v * 1664525u + 1013904223u;
    }
}

static void TestWriteBitsRoundTrip(void)
{
    /* The identity that matters: what the writer put in, an MSB-first reader
     * takes back out, in order, at every width. */
    static const int aWidths[] = { 1, 2, 3, 5, 8, 12, 16, 20, 31, 32, 7, 4 };
    uint32_t aVals[sizeof(aWidths) / sizeof(aWidths[0])];
    unsigned i, n = sizeof(aWidths) / sizeof(aWidths[0]);
    int pos = 0;
    uint32_t v = 0x12345678u;

    StreamReset();
    for (i = 0; i < n; ++i) {
        /* Only the low nBits can survive; mask so the expectation is the
         * value the format can actually carry.  (32 is special-cased because
         * a 32-bit shift is undefined in C.) */
        aVals[i] = (aWidths[i] >= 32) ? v : (v & ((1u << aWidths[i]) - 1u));
        BrBitStreamWriteBits(&s_bs, (int32_t)v, aWidths[i]);
        v = v * 1103515245u + 12345u;
    }
    for (i = 0; i < n; ++i) {
        CHECK(RefReadBits(s_buf, &pos, aWidths[i]) == aVals[i]);
    }
}

static void TestWriteBitsHighBitsIgnored(void)
{
    /* Only the low nBits of `value` may reach the buffer -- the original masks
     * with ((1<<take)-1)<<remaining and never looks above bit nBits-1. */
    int pos = 0;

    StreamReset();
    BrBitStreamWriteBits(&s_bs, (int32_t)0xFFFFFFF5u, 4);   /* low nibble 5 */
    CHECK(RefReadBits(s_buf, &pos, 4) == 5u);
    CHECK(s_bs.writeBit == 4);
    CHECK(s_bs.writeByte == 0);

    /* The bits above the written width must still be clear in the buffer. */
    CHECK((s_buf[0] & 0x0F) == 0);
}

static void TestWriteBitsZeroIsNoOp(void)
{
    /* The original's first branch: nBits == 0 touches nothing at all -- not
     * the buffer, not either cursor.  Set up a non-trivial cursor first so a
     * "reset" would be visible. */
    StreamReset();
    BrBitStreamWriteBits(&s_bs, 0x3, 3);
    {
        unsigned char before = s_buf[0];
        int wb = s_bs.writeBit, by = s_bs.writeByte;

        BrBitStreamWriteBits(&s_bs, (int32_t)0xFFFFFFFFu, 0);

        CHECK(s_buf[0]      == before);
        CHECK(s_bs.writeBit  == wb);
        CHECK(s_bs.writeByte == by);
    }
}

static void TestWriteBitsExactByte(void)
{
    /* Exactly filling a byte must NORMALISE the cursor -- writeBit back to 0
     * and writeByte stepped -- rather than leaving writeBit == 8. */
    StreamReset();
    BrBitStreamWriteBits(&s_bs, 0xA5, 8);
    CHECK(s_buf[0] == 0xA5);
    CHECK(s_bs.writeBit  == 0);
    CHECK(s_bs.writeByte == 1);

    /* And four writes of two bits must land in the same byte in order. */
    StreamReset();
    BrBitStreamWriteBits(&s_bs, 0x2, 2);   /* 10 */
    BrBitStreamWriteBits(&s_bs, 0x1, 2);   /* 01 */
    BrBitStreamWriteBits(&s_bs, 0x3, 2);   /* 11 */
    BrBitStreamWriteBits(&s_bs, 0x0, 2);   /* 00 */
    CHECK(s_buf[0] == 0x9C);               /* 10 01 11 00 */
    CHECK(s_bs.writeBit  == 0);
    CHECK(s_bs.writeByte == 1);
}

static void TestWriteBitsPreservesExistingBits(void)
{
    /* The mask in the original exists so that a partially filled byte MERGES.
     * Write three bits, then overwrite the buffer byte's low bits with junk,
     * then write five more: the first three must survive untouched. */
    int pos = 0;

    StreamReset();
    BrBitStreamWriteBits(&s_bs, 0x5, 3);        /* 101 in the top 3 bits */
    s_buf[0] |= 0x1F;                           /* junk in the free bits */
    BrBitStreamWriteBits(&s_bs, 0x0, 5);        /* five zeroes fill the byte */

    CHECK(RefReadBits(s_buf, &pos, 3) == 5u);
    CHECK(s_buf[0] == 0xA0);                    /* 101 00000 */
    CHECK(s_bs.writeByte == 1);
}

static void TestWriteBitsFull32(void)
{
    /* A 32-bit write is the widest reachable case and exercises the extraction
     * shift at its largest reachable value. */
    int pos = 0;

    StreamReset();
    BrBitStreamWriteBits(&s_bs, (int32_t)0xDEADBEEFu, 32);
    CHECK(s_buf[0] == 0xDE);
    CHECK(s_buf[1] == 0xAD);
    CHECK(s_buf[2] == 0xBE);
    CHECK(s_buf[3] == 0xEF);
    CHECK(s_bs.writeBit  == 0);
    CHECK(s_bs.writeByte == 4);
    CHECK(RefReadBits(s_buf, &pos, 32) == 0xDEADBEEFu);

    /* Unaligned, so every byte of the result is a merge of two chunks. */
    StreamReset();
    pos = 0;
    BrBitStreamWriteBits(&s_bs, 0x1, 1);
    BrBitStreamWriteBits(&s_bs, (int32_t)0xDEADBEEFu, 32);
    CHECK(s_bs.writeBit  == 1);
    CHECK(s_bs.writeByte == 4);
    CHECK(RefReadBits(s_buf, &pos, 1) == 1u);
    CHECK(RefReadBits(s_buf, &pos, 32) == 0xDEADBEEFu);
}

static void TestWriteBitsCursorAtEight(void)
{
    /* A caller-supplied writeBit of exactly 8 is a state the original can be
     * handed (nothing validates the field).  It must not stall: the iteration
     * takes zero bits and the normalisation step moves to the next byte, so
     * the write lands at the start of byte 1 and terminates. */
    int pos;

    StreamReset();
    s_bs.writeBit  = 8;
    s_bs.writeByte = 0;
    BrBitStreamWriteBits(&s_bs, 0xC3, 8);

    CHECK(s_buf[0] == 0x00);
    CHECK(s_buf[1] == 0xC3);
    CHECK(s_bs.writeBit  == 0);
    CHECK(s_bs.writeByte == 2);

    pos = 8;
    CHECK(RefReadBits(s_buf, &pos, 8) == 0xC3u);
}

static void TestWriteBitsHonoursWriteByte(void)
{
    /* The write cursor is an offset into pBuf, not an assumption that the
     * stream starts at byte 0. */
    StreamReset();
    s_bs.writeByte = 5;
    BrBitStreamWriteBits(&s_bs, 0x7E, 8);
    CHECK(s_buf[5] == 0x7E);
    CHECK(s_buf[4] == 0x00);
    CHECK(s_bs.writeByte == 6);
}

/* ==========================================================================
 * 2. 0x10073B90 -- reset the READ cursor
 * ========================================================================== */

static void TestResetRead(void)
{
    /* The whole point of this function existing separately from 0x10073B80 is
     * that it resets ONE cursor.  Assert the write cursor and the buffer
     * pointer survive -- that is the distinction, and it is the thing a
     * mix-up with BrObjClear would destroy. */
    BrBitStream bs;

    bs.readBit   = 3;
    bs.readByte  = 9;
    bs.writeBit  = 5;
    bs.writeByte = 11;
    bs.pBuf      = s_buf;

    BrBitStreamResetRead(&bs);

    CHECK(bs.readBit   == 0);
    CHECK(bs.readByte  == 0);
    CHECK(bs.writeBit  == 5);
    CHECK(bs.writeByte == 11);
    CHECK(bs.pBuf      == s_buf);

    /* Idempotent, as two unconditional stores must be. */
    BrBitStreamResetRead(&bs);
    CHECK(bs.readBit  == 0);
    CHECK(bs.readByte == 0);
}

/* ==========================================================================
 * 3. 0x10074DC0 -- the global store
 * ========================================================================== */

static void TestSub10074DC0(void)
{
    /* Unconditional: it stores whatever it is given, including 0 and negative
     * values.  0 is a legitimate stored value here, not "empty" -- the
     * function has no sentinel and no guard. */
    BrSub10074DC0(0x1234);
    CHECK(g_br18AA088 == 0x1234);

    BrSub10074DC0(0);
    CHECK(g_br18AA088 == 0);

    BrSub10074DC0(-1);
    CHECK(g_br18AA088 == -1);

    /* Last write wins; there is no accumulation. */
    BrSub10074DC0(7);
    BrSub10074DC0(9);
    CHECK(g_br18AA088 == 9);
}

/* ==========================================================================
 * 4. 0x10008B80 -- the empty family
 * ========================================================================== */

static void TestStub8B80Family(void)
{
    /* There is nothing to observe in a bare `ret` except that it observes
     * nothing.  Park a canary in the module's own global, call all six names
     * with the arities the call sites actually use, and assert nothing moved.
     * That is the real risk here: not that they compute the wrong answer, but
     * that one of them was given a body by mistake. */
    BrSub10074DC0(0x5A5A5A5A);

    BrExt_10008B80();
    BrStub8B80_0();
    BrStub8B80_1i(42);
    BrStub8B80_1p("points = %d\n");
    BrStub8B80_5i(0, 0x80, 0x80, 0xF0, 0xFF);
    BrStub10008B80((intptr_t)(const void *)"points = %d\n", 1, 2);

    CHECK(g_br18AA088 == 0x5A5A5A5A);
}

/* ==========================================================================
 * 5. Adapters -- forwarding and return propagation
 * ========================================================================== */

static void TestAdapterBitReader(void)
{
    BrBitStream bs;
    uint32_t got;

    memset(&bs, 0, sizeof(bs));
    s_pReadArg     = NULL;
    s_nReadBitsArg = -1;
    s_readResult   = 0xABCDEF01u;

    got = BrBitReaderRead((struct BrBitReader *)&bs, 13);

    CHECK(s_pReadArg == &bs);          /* same object, not a copy   */
    CHECK(s_nReadBitsArg == 13);       /* width passed through      */
    CHECK(got == 0xABCDEF01u);         /* result passed back        */

    /* Width 0 is forwarded as 0 rather than special-cased in the adapter --
     * the no-op belongs to the callee, and duplicating it here would be a
     * second place for it to be wrong. */
    s_nReadBitsArg = -1;
    (void)BrBitReaderRead((struct BrBitReader *)&bs, 0);
    CHECK(s_nReadBitsArg == 0);
}

static void TestAdapterVec3Len(void)
{
    BrVec3 v;
    float got;

    v.x = 3.0f; v.y = 4.0f; v.z = 0.0f;
    s_pLenArg   = NULL;
    s_lenResult = 5.0f;

    got = BrVec3Len(&v);

    CHECK(s_pLenArg == &v);
    /* The float must arrive intact.  This is the assertion that would have
     * caught the generated stub, which returned an integer 0 in rax and left
     * xmm0 holding whatever was there. */
    CHECK(got == 5.0f);

    s_lenResult = -1.25f;
    CHECK(BrVec3Len(&v) == -1.25f);
}

static void TestAdapterFtolArg(void)
{
    s_ftolArg    = 0.0f;
    s_ftolResult = -7;

    CHECK(BrFtolArg(2.75f) == -7);
    CHECK(s_ftolArg == 2.75f);

    /* Out-of-range handling belongs to __ftol and is NOT re-implemented in the
     * adapter; assert only that the adapter is transparent to it. */
    s_ftolResult = 0;
    CHECK(BrFtolArg(1.0e30f) == 0);
    CHECK(s_ftolArg == 1.0e30f);
}

static void TestAdapterSndPlaySimple(void)
{
    s_sndCalls  = 0;
    s_sndGroup  = -1;
    s_sndPacked = 0;

    BrExt_10072AF0(3, 0x00200020u);

    CHECK(s_sndCalls == 1);
    CHECK(s_sndGroup == 3);
    CHECK(s_sndPacked == 0x00200020u);
    /* The callee's int32_t return is discarded by the void declaration
     * slice3_31.h gives this address -- the conflict is reported, not fixed
     * here.  Nothing to assert about the value; the point is that discarding
     * it does not stop the call happening. */
}

static void TestAdapterCdA0(void)
{
    s_cdA0Calls = 0;
    BrSub1003CDA0();
    BrSub1003CDA0();
    CHECK(s_cdA0Calls == 2);           /* one call in, one call out, no gate */
}

static void TestAdapterMat4FromCarState(void)
{
    BrMat4 m;
    /* A stand-in for the car record; only its address is forwarded. */
    static char carRecord[64];
    const struct BrCarState *pState = (const struct BrCarState *)carRecord;

    s_pMatOut = NULL;
    s_pMatSrc = NULL;

    BrSub100695D0(&m, pState);

    CHECK(s_pMatOut == &m);            /* the +0x220 matrix, uncast-and-back */
    CHECK(s_pMatSrc == pState);
}

/* ==========================================================================
 * 6. The mutex hooks
 * ========================================================================== */

static void TestMutexHooks(void)
{
    /* The documented single-threaded no-ops.  The only property available is
     * that they are total: any handle, including NULL, and no state change.
     * If these ever grow a body, this test should be replaced, not deleted. */
    BrSub10074DC0(0x2B2B2B2B);

    BrNetMutexLock(NULL);
    BrNetMutexUnlock(NULL);
    BrNetMutexLock((void *)&g_br18AA088);
    BrNetMutexUnlock((void *)&g_br18AA088);

    CHECK(g_br18AA088 == 0x2B2B2B2B);
}

/* ========================================================================== */

int main(void)
{
    TestWriteBitsAgainstModel();
    TestWriteBitsRoundTrip();
    TestWriteBitsHighBitsIgnored();
    TestWriteBitsZeroIsNoOp();
    TestWriteBitsExactByte();
    TestWriteBitsPreservesExistingBits();
    TestWriteBitsFull32();
    TestWriteBitsCursorAtEight();
    TestWriteBitsHonoursWriteByte();

    TestResetRead();
    TestSub10074DC0();
    TestStub8B80Family();

    TestAdapterBitReader();
    TestAdapterVec3Len();
    TestAdapterFtolArg();
    TestAdapterSndPlaySimple();
    TestAdapterCdA0();
    TestAdapterMat4FromCarState();

    TestMutexHooks();

    if (g_fails == 0) {
        printf("test_slice6_74: all checks passed\n");
        return 0;
    }
    printf("test_slice6_74: %d FAILED\n", g_fails);
    return 1;
}
