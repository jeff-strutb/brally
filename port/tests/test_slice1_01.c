/* test_slice1_01.c -- behaviour/invariant tests for slice1_01.
 *
 * These assert properties of the ORIGINAL: the documented reserved returns,
 * the asymmetric guards, the composition identity of adler32, and the two
 * quirks that are easy to "fix" by accident (the 30 Hz duplicate step and the
 * cursor's non-carrying position increment).
 */

#include "slice1_01.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fails = 0;

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);           \
            ++g_fails;                                                       \
        }                                                                    \
    } while (0)

/* ------------------------------------------------------------------ adler32 */

static void test_adler32_reserved(void)
{
    /* NULL buffer is the "give me the seed" call: it ignores `adler`. */
    CHECK(BrAdler32(0uL,          NULL, 0u)   == 1uL);
    CHECK(BrAdler32(0x12345678uL, NULL, 999u) == 1uL);

    /* len == 0 is a different case entirely: the running value passes
     * through untouched, NOT reset to 1. */
    CHECK(BrAdler32(0x12345678uL, (const unsigned char *)"x", 0u)
          == 0x12345678uL);
}

static void test_adler32_vectors(void)
{
    static const unsigned char abc[] = "abcdefghijklmnopqrstuvwxyz";

    CHECK(BrAdler32(1uL, (const unsigned char *)"Wikipedia", 9u)
          == 0x11E60398uL);
    CHECK(BrAdler32(1uL, abc, 26u) == 0x90860B20uL);
}

/* Splitting the input anywhere must not change the result. This is the
 * property the DO16 / tail / NMAX chunking has to preserve, and it walks the
 * split across all three code paths. */
static void test_adler32_composition(void)
{
    unsigned char buf[300];
    unsigned long whole;
    unsigned int  i, cut;

    for (i = 0u; i < sizeof buf; ++i) {
        buf[i] = (unsigned char)(i * 37u + 11u);
    }
    whole = BrAdler32(1uL, buf, (unsigned int)sizeof buf);

    for (cut = 0u; cut <= (unsigned int)sizeof buf; ++cut) {
        unsigned long a = BrAdler32(1uL, buf, cut);
        unsigned long b = BrAdler32(a, buf + cut,
                                    (unsigned int)sizeof buf - cut);
        CHECK(b == whole);
    }
}

/* Longer than NMAX (5552), so the outer chunk loop runs more than once and
 * the two modulo reductions actually fire. */
static void test_adler32_over_nmax(void)
{
    unsigned char *p = (unsigned char *)malloc(10240u);
    unsigned int i;

    if (p == NULL) {
        CHECK(0);
        return;
    }
    for (i = 0u; i < 10240u; ++i) {
        p[i] = (unsigned char)(i & 0xFFu);
    }

    CHECK(BrAdler32(1uL, p, 10240u) == 0xF475ED1EuL);

    /* Both halves must come out reduced mod 65521. */
    {
        unsigned long r  = BrAdler32(1uL, p, 10240u);
        CHECK((r & 0xFFFFuL) < 65521uL);
        CHECK(((r >> 16) & 0xFFFFuL) < 65521uL);
    }

    free(p);
}

/* ---------------------------------------------------------------- volume */

static void test_volume_scale(void)
{
    int v, prev;

    CHECK(BrCdVolumeScale(0)   == 0);
    CHECK(BrCdVolumeScale(255) == 10000);

    /* The mask is applied before the scale, so 256 is silence, not full. */
    CHECK(BrCdVolumeScale(256) == 0);
    CHECK(BrCdVolumeScale(511) == BrCdVolumeScale(255));

    prev = -1;
    for (v = 0; v <= 255; ++v) {
        int cur = BrCdVolumeScale(v);
        CHECK(cur >= prev);
        CHECK(cur >= 0 && cur <= 10000);
        prev = cur;
    }
}

/* ------------------------------------------------------------- grid sample */

static uint16_t g_grid[64 * 64];

static void grid_init(void)
{
    unsigned int i;
    for (i = 0u; i < 64u * 64u; ++i) {
        g_grid[i] = (uint16_t)(i * 7u + 3u);
    }
}

static void test_grid_bounds(void)
{
    /* Lower bound is inclusive, upper bound exclusive -- and both ends of
     * BOTH axes reject to the same value a real cell could hold. */
    CHECK(BrGrid64Sample(g_grid, -0.001f,  10.0f) == 0u);
    CHECK(BrGrid64Sample(g_grid,  10.0f,  -0.001f) == 0u);
    CHECK(BrGrid64Sample(g_grid, 2048.0f,  10.0f) == 0u);
    CHECK(BrGrid64Sample(g_grid,  10.0f, 2048.0f) == 0u);

    /* NaN takes the reject path (the C0 bit is set by fcomp with a NaN, and
     * the first guard rejects on C0). */
    CHECK(BrGrid64Sample(g_grid, nanf(""), 10.0f) == 0u);
    CHECK(BrGrid64Sample(g_grid, 10.0f, nanf("")) == 0u);

    /* -0.0f compares as >= 0.0f, so it is inside. */
    CHECK(BrGrid64Sample(g_grid, -0.0f, 0.0f) != 0u
          || g_grid[0] == 0);

    /* Just inside the top is still the last cell, not a reject. */
    CHECK(BrGrid64Sample(g_grid, 2047.9f, 2047.9f) != 0u);
}

static void test_grid_indexing(void)
{
    struct { float x, y; unsigned int col, row; } cases[] = {
        {   0.0f,    0.0f,  0,  0 },
        {  31.9f,    0.0f,  0,  0 },
        {  32.0f,    0.0f,  1,  0 },
        {   0.0f,   32.0f,  0,  1 },
        { 100.0f,  200.0f,  3,  6 },
        {2047.9f, 2047.9f, 63, 63 }
    };
    unsigned int i;

    for (i = 0u; i < sizeof cases / sizeof cases[0]; ++i) {
        unsigned int idx = (cases[i].row << 6) + cases[i].col;
        uint32_t r = BrGrid64Sample(g_grid, cases[i].x, cases[i].y);

        /* Low half is the cell itself. x picks the column, y the row --
         * getting these the other way round is the easy mistake. */
        CHECK((r & 0xFFFFu) == g_grid[idx]);
    }
}

/* The packed high half must reconstruct the right-hand neighbour exactly. */
static void test_grid_delta_reconstructs(void)
{
    unsigned int row, col;

    for (row = 0u; row < 64u; ++row) {
        for (col = 0u; col < 63u; ++col) {   /* idx+1 stays in the grid */
            unsigned int idx = (row << 6) + col;
            float x = (float)col * 32.0f + 1.0f;
            float y = (float)row * 32.0f + 1.0f;
            uint32_t r  = BrGrid64Sample(g_grid, x, y);
            uint16_t t0 = (uint16_t)(r & 0xFFFFu);
            uint16_t d  = (uint16_t)(r >> 16);

            CHECK(t0 == g_grid[idx]);
            CHECK((uint16_t)(t0 + d) == g_grid[idx + 1u]);
        }
    }
}

/* ------------------------------------------------------------- u16 cursor */

static void test_cursor_walk(void)
{
    static const uint16_t table[8] = { 10, 20, 30, 40, 50, 60, 70, 80 };
    BrU16Cursor cur;
    unsigned int i;

    cur.pos = 2;
    cur.remaining = 4;

    for (i = 0u; i < 4u; ++i) {
        uint16_t before_pos = cur.pos;
        uint16_t before_rem = cur.remaining;
        uint16_t v = BrU16CursorNext(table, &cur);

        CHECK(v == table[before_pos]);
        CHECK(cur.pos == (uint16_t)(before_pos + 1));
        CHECK(cur.remaining == (uint16_t)(before_rem - 1));
    }

    CHECK(cur.remaining == 0);
    CHECK(cur.pos == 6);
}

static void test_cursor_exhausted_is_inert(void)
{
    static const uint16_t table[4] = { 1, 2, 3, 4 };
    BrU16Cursor cur;
    unsigned int i;

    cur.pos = 3;
    cur.remaining = 0;

    /* Exhausted returns 0 and must not touch the cursor, however many times
     * it is called. Note 0 is not reserved in the table, so a caller cannot
     * distinguish this from a genuine zero entry. */
    for (i = 0u; i < 5u; ++i) {
        CHECK(BrU16CursorNext(table, &cur) == 0);
        CHECK(cur.pos == 3);
        CHECK(cur.remaining == 0);
    }
}

/* The position increment is done in 32 bits and ORed into the packed dword,
 * so at 0xFFFF the carry lands in bit 0 of the count instead of being lost.
 * The count then fails to decrease -- preserved from the original. */
static void test_cursor_wrap_quirk(void)
{
    uint16_t *table = (uint16_t *)calloc(65536u, sizeof(uint16_t));
    BrU16Cursor cur;

    if (table == NULL) {
        CHECK(0);
        return;
    }
    table[0xFFFF] = 0xBEEF;

    cur.pos = 0xFFFF;
    cur.remaining = 5;

    CHECK(BrU16CursorNext(table, &cur) == 0xBEEF);
    CHECK(cur.pos == 0);
    CHECK(cur.remaining == 5);   /* NOT 4 -- the carry ORs a 1 back in */

    free(table);
}

/* ------------------------------------------------------------------ ticks */

static void test_ticks30(void)
{
    uint32_t ms, prev;

    CHECK(BrTicks30FromMs(0u)    == 0u);
    CHECK(BrTicks30FromMs(32u)   == 0u);
    CHECK(BrTicks30FromMs(33u)   == 1u);
    CHECK(BrTicks30FromMs(1000u) == 30u);
    CHECK(BrTicks30FromMs(2000u) == 60u);

    /* The quirk: (ms % 100) / 33 saturates at 3, so the last tick of each
     * 100 ms window repeats the first tick of the next one. */
    CHECK(BrTicks30FromMs(99u)  == 3u);
    CHECK(BrTicks30FromMs(100u) == 3u);
    CHECK(BrTicks30FromMs(199u) == 6u);
    CHECK(BrTicks30FromMs(200u) == 6u);

    /* Non-decreasing, and never more than one step ahead of ms*30/1000. */
    prev = 0u;
    for (ms = 0u; ms <= 20000u; ++ms) {
        uint32_t t = BrTicks30FromMs(ms);
        CHECK(t >= prev);
        CHECK(t <= prev + 1u);
        prev = t;
    }
}

/* --------------------------------------------------------------- CHK_* I/O */

#define TEST_PATH "/tmp/br_slice1_01_scratch.bin"

static void test_chk_file(void)
{
    static const unsigned char payload[16] = {
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
    };
    unsigned char got[16];
    FILE *pFile;
    FILE **ppFile = &pFile;

    pFile = fopen(TEST_PATH, "wb");
    CHECK(pFile != NULL);
    if (pFile == NULL) {
        return;
    }
    CHECK(fwrite(payload, 1u, sizeof payload, pFile) == sizeof payload);
    fclose(pFile);

    CHECK(BrChkFileExists(TEST_PATH) == 1);
    CHECK(BrChkFileExists(TEST_PATH ".nope") == 0);

    pFile = fopen(TEST_PATH, "rb");
    CHECK(pFile != NULL);
    if (pFile == NULL) {
        return;
    }

    /* A zero-length request short-circuits before fread, so it neither fails
     * nor consumes anything. */
    memset(got, 0xAA, sizeof got);
    CHECK(BrFChkFRead(got, 0u, 8u, ppFile) == 1);
    CHECK(BrFChkFRead(got, 8u, 0u, ppFile) == 1);
    CHECK(got[0] == 0xAA);

    /* The handle argument is a FILE **, not a FILE *. */
    CHECK(BrChkFRead(got, 1u, sizeof got, ppFile) == (void *)got);
    CHECK(memcmp(got, payload, sizeof got) == 0);

    /* At EOF fread yields nothing, which is the recoverable 0 return rather
     * than the fatal short-read path. */
    CHECK(BrFChkFRead(got, 1u, 4u, ppFile) == 0);

    fclose(pFile);
    remove(TEST_PATH);
}

static void test_chk_mem(void)
{
    void *p;

    /* Zero size is a quiet NULL, not a fatal out-of-memory. */
    CHECK(BrChkAlloc(0u, "nothing") == NULL);

    p = BrChkAlloc(64u, "block");
    CHECK(p != NULL);
    memset(p, 0x5A, 64u);

    p = BrChkRealloc(p, 128u, "block");
    CHECK(p != NULL);
    CHECK(((unsigned char *)p)[0] == 0x5A);

    /* Shrinking to zero returns NULL and releases the block. */
    CHECK(BrChkRealloc(p, 0u, "block") == NULL);

    /* And the documented leak: (NULL, 0) still allocates internally, then
     * discards it. Only the observable return is asserted. */
    CHECK(BrChkRealloc(NULL, 0u, "block") == NULL);
}

int main(void)
{
    grid_init();

    test_adler32_reserved();
    test_adler32_vectors();
    test_adler32_composition();
    test_adler32_over_nmax();

    test_volume_scale();

    test_grid_bounds();
    test_grid_indexing();
    test_grid_delta_reconstructs();

    test_cursor_walk();
    test_cursor_exhausted_is_inert();
    test_cursor_wrap_quirk();

    test_ticks30();

    test_chk_file();
    test_chk_mem();

    if (g_fails == 0) {
        printf("slice1_01: all checks passed\n");
        return 0;
    }
    printf("slice1_01: %d check(s) FAILED\n", g_fails);
    return 1;
}
