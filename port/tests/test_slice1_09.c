/* test_slice1_09.c -- behavioural tests for slice1_09.c.
 *
 * These assert properties of the ORIGINAL code (MSB-first bit order,
 * big-endian byte order, the align-before-byte-access rule, the missing
 * zero guard in BrVec3Normalise, the row-vector transform convention, the
 * 16-entry bank split, the ring wrap, the 30 Hz counter's off-by-a-third)
 * rather than restating the implementation.
 */
#include "slice1_09.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fail = 0;

#define CHECK(cond) \
    do { \
        if (!(cond)) { \
            printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); \
            g_fail++; \
        } \
    } while (0)

static int approx(float a, float b)
{
    float d = a - b;
    if (d < 0.0f) d = -d;
    return d <= 1e-5f;
}

/* ------------------------------------------------------------------ */

static void test_bits_msb_first(void)
{
    unsigned char buf[4] = { 0xB4, 0xCD, 0x00, 0x00 };
    BrBitStream bs;

    BrBitStreamInit(&bs, buf, 4);
    /* First bits out are the most significant bits of the first byte. */
    CHECK(BrBitStreamReadBits(&bs, 4) == 0xBu);
    CHECK(BrBitStreamReadBits(&bs, 4) == 0x4u);
    CHECK(BrBitStreamReadBits(&bs, 8) == 0xCDu);

    /* A field that straddles a byte boundary keeps the same MSB-first order. */
    BrBitStreamInit(&bs, buf, 4);
    CHECK(BrBitStreamReadBits(&bs, 12) == 0xB4Cu);
    CHECK(BrBitStreamReadBits(&bs, 4)  == 0xDu);
}

static void test_bits_split_equals_whole(void)
{
    unsigned char buf[8] = { 0x9E, 0x37, 0x79, 0xB9, 0x7F, 0x4A, 0x7C, 0x15 };
    int width;

    /* Reading n bits in single-bit steps must build the same value as one
     * n-bit read, for every width and every starting offset. */
    for (width = 1; width <= 24; ++width) {
        int start;
        for (start = 0; start < 9; ++start) {
            BrBitStream a, b;
            unsigned int va = 0, vb;
            int i;

            BrBitStreamInit(&a, buf, 8);
            BrBitStreamInit(&b, buf, 8);
            if (start != 0) {
                (void)BrBitStreamReadBits(&a, start);
                (void)BrBitStreamReadBits(&b, start);
            }
            for (i = 0; i < width; ++i)
                va = (va << 1) | BrBitStreamReadBits(&a, 1);
            vb = BrBitStreamReadBits(&b, width);

            CHECK(va == vb);
            CHECK(a.readBit == b.readBit);
            CHECK(a.readByte == b.readByte);
        }
    }
}

static void test_bits_zero_width(void)
{
    unsigned char buf[2] = { 0xFF, 0xFF };
    BrBitStream bs;

    BrBitStreamInit(&bs, buf, 2);
    (void)BrBitStreamReadBits(&bs, 3);
    CHECK(BrBitStreamReadBits(&bs, 0) == 0u);
    /* A zero-width read consumes nothing. */
    CHECK(bs.readBit == 3);
    CHECK(bs.readByte == 0);
}

static void test_byte_reads_are_big_endian(void)
{
    unsigned char buf[16];
    BrBitStream bs;
    int i;

    for (i = 0; i < 16; ++i)
        buf[i] = (unsigned char)(0x10 + i);

    BrBitStreamInit(&bs, buf, 16);
    CHECK(BrBitStreamReadU8(&bs)  == 0x10u);
    CHECK(BrBitStreamReadU16(&bs) == 0x1112u);
    CHECK(BrBitStreamReadU24(&bs) == 0x131415u);
    CHECK(BrBitStreamReadS32(&bs) == 0x16171819);
    CHECK(bs.readByte == 10);

    /* The top bit of a 32-bit field is a sign bit, not a lost bit. */
    buf[0] = 0xFF; buf[1] = 0xFF; buf[2] = 0xFF; buf[3] = 0xFF;
    BrBitStreamInit(&bs, buf, 16);
    CHECK(BrBitStreamReadS32(&bs) == -1);

    buf[0] = 0x80; buf[1] = 0x00; buf[2] = 0x00; buf[3] = 0x01;
    BrBitStreamInit(&bs, buf, 16);
    CHECK(BrBitStreamReadS32(&bs) == (int)0x80000001u);
}

static void test_byte_reads_align_first(void)
{
    unsigned char buf[4] = { 0xAA, 0x55, 0x33, 0x0F };
    BrBitStream bs;

    /* One bit consumed out of byte 0 -- the byte accessor must discard the
     * remainder of byte 0 and start at byte 1, not repeat byte 0. */
    BrBitStreamInit(&bs, buf, 4);
    (void)BrBitStreamReadBits(&bs, 1);
    CHECK(BrBitStreamReadU8(&bs) == 0x55u);

    BrBitStreamInit(&bs, buf, 4);
    (void)BrBitStreamReadBits(&bs, 1);
    CHECK(BrBitStreamReadU16(&bs) == 0x5533u);

    /* Exactly on a boundary, nothing is discarded. */
    BrBitStreamInit(&bs, buf, 4);
    (void)BrBitStreamReadBits(&bs, 8);
    CHECK(BrBitStreamReadU8(&bs) == 0x55u);

    /* SkipBytes aligns too. */
    BrBitStreamInit(&bs, buf, 4);
    (void)BrBitStreamReadBits(&bs, 3);
    BrBitStreamSkipBytes(&bs, 1);
    CHECK(BrBitStreamReadU8(&bs) == 0x33u);
}

static void test_write_read_roundtrip(void)
{
    unsigned char buf[32];
    BrBitStream bs;

    memset(buf, 0, sizeof buf);
    BrBitStreamInit(&bs, buf, 0);
    bs.writeByte = 0;                 /* start writing at the beginning */
    BrBitStreamWriteU8(&bs, 0xA5u);
    BrBitStreamWriteU24(&bs, 0x123456u);
    BrBitStreamWriteU32(&bs, 0xDEADBEEFu);
    CHECK(bs.writeByte == 8);

    /* Byte order on the wire is big-endian. */
    CHECK(buf[0] == 0xA5);
    CHECK(buf[1] == 0x12 && buf[2] == 0x34 && buf[3] == 0x56);
    CHECK(buf[4] == 0xDE && buf[5] == 0xAD && buf[6] == 0xBE && buf[7] == 0xEF);

    BrBitStreamInit(&bs, buf, 8);
    CHECK(BrBitStreamReadU8(&bs)  == 0xA5u);
    CHECK(BrBitStreamReadU24(&bs) == 0x123456u);
    CHECK((unsigned int)BrBitStreamReadS32(&bs) == 0xDEADBEEFu);
    CHECK(BrBitStreamAtEnd(&bs) == 1);

    /* Writes align the write cursor first, exactly like reads. */
    memset(buf, 0, sizeof buf);
    BrBitStreamInit(&bs, buf, 0);
    bs.writeBit = 5;
    BrBitStreamWriteU8(&bs, 0x77u);
    CHECK(bs.writeBit == 0);
    CHECK(buf[0] == 0x00);
    CHECK(buf[1] == 0x77);
}

static void test_at_end(void)
{
    unsigned char buf[4] = { 1, 2, 3, 4 };
    BrBitStream bs;

    BrBitStreamInit(&bs, buf, 4);
    CHECK(BrBitStreamAtEnd(&bs) == 0);
    (void)BrBitStreamReadU8(&bs);
    (void)BrBitStreamReadU8(&bs);
    (void)BrBitStreamReadU8(&bs);
    CHECK(BrBitStreamAtEnd(&bs) == 0);
    (void)BrBitStreamReadU8(&bs);
    CHECK(BrBitStreamAtEnd(&bs) == 1);

    /* A partially consumed byte counts as consumed. */
    BrBitStreamInit(&bs, buf, 4);
    (void)BrBitStreamReadBits(&bs, 25);   /* 3 whole bytes + 1 bit */
    CHECK(bs.readByte == 3 && bs.readBit == 1);
    CHECK(BrBitStreamAtEnd(&bs) == 1);

    /* An empty stream is at end immediately. */
    BrBitStreamInit(&bs, buf, 0);
    CHECK(BrBitStreamAtEnd(&bs) == 1);
}

static void test_init_argument_roles(void)
{
    unsigned char buf[4] = { 0, 0, 0, 0 };
    BrBitStream bs;

    /* The length argument goes into the WRITE cursor, not a separate field;
     * that is what makes AtEnd work for a pre-filled buffer. */
    BrBitStreamInit(&bs, buf, 3);
    CHECK(bs.pBuf == buf);
    CHECK(bs.writeByte == 3);
    CHECK(bs.readByte == 0 && bs.readBit == 0 && bs.writeBit == 0);
}

/* ------------------------------------------------------------------ */

static void test_vec3_normalise(void)
{
    BrVec3 v;
    float len;

    v.x = 3.0f; v.y = 4.0f; v.z = 12.0f;   /* |v| = 13 */
    BrVec3Normalise(&v);
    len = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
    CHECK(approx(len, 1.0f));
    /* Direction preserved (still parallel to (3,4,12), same sense). */
    CHECK(approx(v.x, 3.0f / 13.0f));
    CHECK(approx(v.y, 4.0f / 13.0f));
    CHECK(approx(v.z, 12.0f / 13.0f));

    /* Already unit -> unchanged. */
    v.x = 0.0f; v.y = -1.0f; v.z = 0.0f;
    BrVec3Normalise(&v);
    CHECK(approx(v.x, 0.0f) && approx(v.y, -1.0f) && approx(v.z, 0.0f));

    /* NO zero-length guard, unlike BrVec3dNormalise. This is the original's
     * behaviour and the reason the two must not be unified. */
    v.x = 0.0f; v.y = 0.0f; v.z = 0.0f;
    BrVec3Normalise(&v);
    CHECK(!isfinite(v.x) && !isfinite(v.y) && !isfinite(v.z));
}

static void test_vec4_normalise(void)
{
    BrVec4 q;
    float sum;

    q.f00 = 1.0f; q.f04 = 1.0f; q.f08 = 1.0f; q.f0C = 1.0f;
    BrVec4Normalise(&q);
    sum = q.f00 * q.f00 + q.f04 * q.f04 + q.f08 * q.f08 + q.f0C * q.f0C;
    CHECK(approx(sum, 1.0f));
    CHECK(approx(q.f00, 0.5f) && approx(q.f0C, 0.5f));

    /* Scaling the input does not change the output. */
    q.f00 = -6.0f; q.f04 = 0.0f; q.f08 = 8.0f; q.f0C = 0.0f;
    BrVec4Normalise(&q);
    CHECK(approx(q.f00, -0.6f) && approx(q.f08, 0.8f));

    q.f00 = 0.0f; q.f04 = 0.0f; q.f08 = 0.0f; q.f0C = 0.0f;
    BrVec4Normalise(&q);
    CHECK(!isfinite(q.f00));
}

static void test_transform_point(void)
{
    BrMat4 m;
    BrVec3 v, out;
    int r, c;

    /* Identity plus a translation row: out = v + t. */
    for (r = 0; r < 4; ++r)
        for (c = 0; c < 4; ++c)
            m.m[r][c] = (r == c) ? 1.0f : 0.0f;
    m.m[3][0] = 10.0f; m.m[3][1] = 20.0f; m.m[3][2] = 30.0f;

    v.x = 1.0f; v.y = 2.0f; v.z = 3.0f;
    BrMat4TransformPoint(&out, &m, &v);
    CHECK(approx(out.x, 11.0f));
    CHECK(approx(out.y, 22.0f));
    CHECK(approx(out.z, 33.0f));

    /* Convention pin: the upper 3x3 is applied TRANSPOSED, so a lone 1 at
     * m[0][1] moves v.x into out.y (a column-vector reading would give the
     * opposite). Getting this backwards is the whole risk in this routine. */
    for (r = 0; r < 4; ++r)
        for (c = 0; c < 4; ++c)
            m.m[r][c] = 0.0f;
    m.m[0][1] = 1.0f;
    v.x = 5.0f; v.y = 0.0f; v.z = 0.0f;
    BrMat4TransformPoint(&out, &m, &v);
    CHECK(approx(out.x, 0.0f));
    CHECK(approx(out.y, 5.0f));
    CHECK(approx(out.z, 0.0f));

    /* A rotation composed with its inverse (transpose) round-trips. */
    {
        const float cth = 0.6f, sth = 0.8f;   /* 3-4-5 rotation about z */
        BrMat4 rot, inv;
        BrVec3 a, b;

        for (r = 0; r < 4; ++r)
            for (c = 0; c < 4; ++c) {
                rot.m[r][c] = (r == c) ? 1.0f : 0.0f;
                inv.m[r][c] = (r == c) ? 1.0f : 0.0f;
            }
        rot.m[0][0] =  cth; rot.m[0][1] = sth;
        rot.m[1][0] = -sth; rot.m[1][1] = cth;
        inv.m[0][0] =  cth; inv.m[0][1] = -sth;
        inv.m[1][0] =  sth; inv.m[1][1] =  cth;

        v.x = 1.5f; v.y = -2.5f; v.z = 7.0f;
        BrMat4TransformPoint(&a, &rot, &v);
        BrMat4TransformPoint(&b, &inv, &a);
        CHECK(approx(b.x, v.x) && approx(b.y, v.y) && approx(b.z, v.z));

        /* A rotation preserves length. */
        CHECK(approx(sqrtf(a.x * a.x + a.y * a.y + a.z * a.z),
                     sqrtf(v.x * v.x + v.y * v.y + v.z * v.z)));
    }
}

/* ------------------------------------------------------------------ */

static void test_entity_set_index(void)
{
    static unsigned char obj[BR_ENTITY_STRIDE];
    const int *pIndex = (const int *)(const void *)(obj + BR_ENTITY_OFF_INDEX);
    const int *pBank  = (const int *)(const void *)(obj + BR_ENTITY_OFF_BANK);

    BrEntitySetIndex(obj, 0);
    CHECK(*pBank == 0 && *pIndex == 0);

    BrEntitySetIndex(obj, 15);            /* last of bank 0 */
    CHECK(*pBank == 0 && *pIndex == 15);

    BrEntitySetIndex(obj, 16);            /* first of bank 1 */
    CHECK(*pBank == 1 && *pIndex == 0);

    BrEntitySetIndex(obj, 31);
    CHECK(*pBank == 1 && *pIndex == 15);

    /* The compare is signed: negatives take the bank-0 path unchanged. */
    BrEntitySetIndex(obj, -1);
    CHECK(*pBank == 0 && *pIndex == -1);

    /* Nothing clamps: bank 1 keeps counting past 32. */
    BrEntitySetIndex(obj, 100);
    CHECK(*pBank == 1 && *pIndex == 84);
}

static void test_entity_bind_aux(void)
{
    unsigned char *pArray = (unsigned char *)calloc(3, BR_ENTITY_STRIDE);
    unsigned char *pAux   = (unsigned char *)calloc(3, BR_ENTITY_AUX_STRIDE);
    int i;

    CHECK(pArray != NULL && pAux != NULL);
    if (pArray == NULL || pAux == NULL) { free(pArray); free(pAux); return; }

    for (i = 0; i < 3; ++i) {
        unsigned char *p = pArray + (size_t)i * BR_ENTITY_STRIDE;
        void *const *ppAux;
        const BrMat4 *pM;
        int r, c;

        BrEntityBindAux(p, pArray, pAux);

        /* Element i of the entity array binds to element i of the aux array. */
        ppAux = (void *const *)(const void *)(p + BR_ENTITY_OFF_AUX);
        CHECK(*ppAux == (void *)(pAux + (size_t)i * BR_ENTITY_AUX_STRIDE));

        /* ...and the embedded matrix is left as identity. */
        pM = (const BrMat4 *)(const void *)(p + BR_ENTITY_OFF_MATRIX);
        for (r = 0; r < 4; ++r)
            for (c = 0; c < 4; ++c)
                CHECK(approx(pM->m[r][c], (r == c) ? 1.0f : 0.0f));
    }

    free(pArray);
    free(pAux);
}

/* ------------------------------------------------------------------ */

static void test_alpha_ramp(void)
{
    unsigned char t[64];
    int i;

    memset(t, 0x5A, sizeof t);
    BrAlphaRampBuild(t);

    for (i = 0; i < 16; ++i) {
        CHECK(t[i * 4 + 0] == 0xFF);
        CHECK(t[i * 4 + 1] == 0xFF);
        CHECK(t[i * 4 + 2] == 0xFF);
        /* Alpha is a full-range linear ramp: 0x00 .. 0xFF in 16 steps. */
        CHECK(t[i * 4 + 3] == (unsigned char)(17 * i));
    }
    CHECK(t[3] == 0x00);
    CHECK(t[63] == 0xFF);
}

static void test_pair_ring(void)
{
    static BrPairRing ring;
    int i;

    memset(&ring, 0, sizeof ring);

    for (i = 0; i < BR_PAIR_RING_SLOTS; ++i)
        BrPairRingPush(&ring, i, -i);

    /* Exactly full wraps the cursor back to 0. */
    CHECK(ring.write == 0);
    CHECK(ring.aItems[0].a == 0 && ring.aItems[0].b == 0);
    CHECK(ring.aItems[255].a == 255 && ring.aItems[255].b == -255);

    /* One more push silently overwrites slot 0 -- there is no fullness
     * check anywhere in the original. */
    BrPairRingPush(&ring, 999, 888);
    CHECK(ring.write == 1);
    CHECK(ring.aItems[0].a == 999 && ring.aItems[0].b == 888);
    CHECK(ring.aItems[255].a == 255);
}

static void test_time_update(void)
{
    BrTimeState st;
    unsigned int ms;
    unsigned int prev;

    /* Exactly 30 ticks per whole second. */
    BrTimeUpdate(&st, 0u);      CHECK(st.tick30 == 0u);
    BrTimeUpdate(&st, 1000u);   CHECK(st.tick30 == 30u);
    BrTimeUpdate(&st, 2000u);   CHECK(st.tick30 == 60u);
    BrTimeUpdate(&st, 60000u);  CHECK(st.tick30 == 1800u);

    /* The raw timestamp is stored verbatim and the middle field is cleared. */
    st.f12C = 0xDEADBEEFu;
    BrTimeUpdate(&st, 1234u);
    CHECK(st.ms == 1234u);
    CHECK(st.f12C == 0u);

    /* The documented quirk: sub-ticks land at 0/33/66/99 within each 100 ms
     * block, so ms=99 is already tick 3 where a true 30 Hz counter is at 2.
     * This is the original's arithmetic, not a rounding accident. */
    BrTimeUpdate(&st, 99u);   CHECK(st.tick30 == 3u);
    BrTimeUpdate(&st, 100u);  CHECK(st.tick30 == 3u);
    BrTimeUpdate(&st, 132u);  CHECK(st.tick30 == 3u);
    BrTimeUpdate(&st, 133u);  CHECK(st.tick30 == 4u);

    /* Whatever else it is, it never runs backwards. */
    prev = 0u;
    for (ms = 0u; ms <= 5000u; ++ms) {
        BrTimeUpdate(&st, ms);
        CHECK(st.tick30 >= prev);
        prev = st.tick30;
    }
}

/* ------------------------------------------------------------------ */

int main(void)
{
    test_init_argument_roles();
    test_bits_msb_first();
    test_bits_split_equals_whole();
    test_bits_zero_width();
    test_byte_reads_are_big_endian();
    test_byte_reads_align_first();
    test_write_read_roundtrip();
    test_at_end();

    test_vec3_normalise();
    test_vec4_normalise();
    test_transform_point();

    test_entity_set_index();
    test_entity_bind_aux();

    test_alpha_ramp();
    test_pair_ring();
    test_time_update();

    if (g_fail != 0) {
        printf("slice1_09: %d FAILED\n", g_fail);
        return 1;
    }
    printf("slice1_09: all tests passed\n");
    return 0;
}
