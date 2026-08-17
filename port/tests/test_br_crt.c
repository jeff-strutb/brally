/* test_br_crt.c -- the CRT shims, and specifically __ftol's out-of-range arm.
 *
 * WHY THIS FILE EXISTS
 *
 * br_crt.c had no suite at all. Eleven other suites link it, none of them
 * calls BrOperatorNew, and a mutation sweep found that both boundary compares
 * in BrFtolTrunc could be changed without any test noticing.
 *
 * That mattered, because __ftol's out-of-range behaviour is one of the facts
 * CONVENTIONS.md records as settled and not to be re-derived -- and it was
 * settled in FIVE separate comments and asserted in ZERO tests. Under those
 * conditions the fact drifts: slice2_23.c had already contradicted it in prose
 * and then in code, and br_crt.c had the range wrong by 32 bits.
 *
 * The rule this file is written under: a behaviour documented in a comment is
 * not evidence of anything. If it is worth writing down, it is worth an
 * assertion that fails when it changes.
 */
#include "br_crt.h"

#include <stdio.h>
#include <string.h>

static int g_fails;
#define CHECK(c) do { if (!(c)) { \
    printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); g_fails++; } } while (0)

/* ---- __ftol, 0x1007C8A0 ---------------------------------------------- *
 * The original is `fistp QWORD` followed by `mov eax, LOW dword`, so:
 *
 *   - the range that overflows is 64-bit, NOT 32-bit;
 *   - on overflow the x87 stores the 64-bit integer indefinite
 *     0x8000000000000000, whose low half is ZERO -- so the answer is 0, not
 *     0x80000000 and not a saturation;
 *   - a value between 2^31 and 2^63 does NOT overflow. It converts, and the
 *     low dword is the wrapped 32-bit result.
 *
 * That last case is the one this file was written for: br_crt.c clamped at
 * 2^31 and returned 0 for it. */
static void test_ftol_in_range(void)
{
    CHECK(BrFtolTrunc(0.0f) == 0);
    CHECK(BrFtolTrunc(1.0f) == 1);
    CHECK(BrFtolTrunc(-1.0f) == -1);

    /* truncation toward zero, not rounding -- the original sets the control
     * word to truncate before the store */
    CHECK(BrFtolTrunc(1.9f) == 1);
    CHECK(BrFtolTrunc(-1.9f) == -1);

    /* the largest float that is exactly representable below 2^31 */
    CHECK(BrFtolTrunc(2147483520.0f) == 2147483520);
}

static void test_ftol_wraps_between_2p31_and_2p63(void)
{
    /* 3e9 is above INT32_MAX and far below 2^63. The original converts it to
     * a 64-bit integer and hands back the low dword, which as a signed 32-bit
     * value is negative. It is NOT zero, and it is NOT saturated.
     *
     * br_crt.c returned 0 here. The other four copies of the helper did not.
     * This assertion is the difference. */
    int32_t r = BrFtolTrunc(3.0e9f);
    CHECK(r != 0);
    CHECK(r == (int32_t)(int64_t)3.0e9f);
    CHECK(r < 0);                       /* 3e9 wraps to a negative int32 */

    /* and symmetrically below zero */
    CHECK(BrFtolTrunc(-3.0e9f) == (int32_t)(int64_t)-3.0e9f);
}

static void test_ftol_out_of_range_is_zero(void)
{
    /* past 2^63 the store genuinely fails and the low half of the indefinite
     * is what comes back: zero. */
    CHECK(BrFtolTrunc(1.0e30f) == 0);
    CHECK(BrFtolTrunc(-1.0e30f) == 0);

    /* NaN takes the same path -- and note the comparison must be written so
     * that NaN reaches it. `d >= lo && d <= hi` is false for NaN, which is
     * why the original's negated form is preserved in the port. */
    {
        float nan = 0.0f;
        memcpy(&nan, "\x00\x00\xC0\x7F", 4);     /* a quiet NaN */
        CHECK(BrFtolTrunc(nan) == 0);
    }

    /* infinity likewise */
    {
        float inf = 0.0f;
        memcpy(&inf, "\x00\x00\x80\x7F", 4);
        CHECK(BrFtolTrunc(inf) == 0);
        CHECK(BrFtolTrunc(-inf) == 0);
    }
}

int main(void)
{
    test_ftol_in_range();
    test_ftol_wraps_between_2p31_and_2p63();
    test_ftol_out_of_range_is_zero();

    if (g_fails != 0) { printf("test_br_crt: %d failures\n", g_fails); return 1; }
    printf("test_br_crt: 0 failures\n");
    return 0;
}
