#include "br_bits.h"
#include <stdio.h>
#include <string.h>

static int g_fail;
static void check(int c, const char *w)
{ printf("  [%s] %s\n", c ? "PASS" : "FAIL", w); if (!c) g_fail = 1; }

int main(void)
{
    BrBitLatch l;
    unsigned char buf[12];
    void *tab[0x130];
    int i;

    l.pending = 0xF0F0; l.latched = 0x0001;
    BrBitLatchTake(&l, 0x00F0);
    check(l.latched == 0x00F1, "masked bits OR into latched, existing kept");
    check(l.pending == 0xF000, "taken bits cleared from pending");
    BrBitLatchTake(&l, 0x00F0);
    check(l.pending == 0xF000 && l.latched == 0x00F1, "re-taking is a no-op");

    for (i = 0; i < 12; i++) buf[i] = (unsigned char)i;
    BrSwapVec3(buf);
    check(buf[0]==3 && buf[1]==2 && buf[2]==1 && buf[3]==0, "component 0 swapped");
    check(buf[4]==7 && buf[7]==4, "component 1 swapped");
    check(buf[8]==11 && buf[11]==8, "component 2 swapped");
    BrSwapVec3(buf);
    for (i = 0; i < 12; i++) if (buf[i] != i) g_fail = 1;
    check(1, "swapping twice restores the original");

    /* 0x10018A50 == 0x1002B9E0. br_track.c and slice2_16.c each used to have
     * their own copy of these 29 bytes; this is the one that survived.
     *
     * MUTATION: the count is SIGNED and the guard is `test ecx,ecx / jle`.
     * br_track.c's copy took an `uint32_t` and looped `i < c`, which turns a
     * negative count into a run of four billion swaps off the end of the
     * buffer. Nothing passed one, so nothing showed it. */
    for (i = 0; i < 12; i++) buf[i] = (unsigned char)i;
    BrSwapU16Array(buf, 3);
    check(buf[0]==1 && buf[1]==0 && buf[4]==5 && buf[5]==4,
          "three u16s swap, byte-wise");
    check(buf[6]==6 && buf[7]==7, "and the fourth is untouched -- count is exact");
    BrSwapU16Array(buf, 3);
    for (i = 0; i < 12; i++) if (buf[i] != i) g_fail = 1;
    check(1, "swapping twice restores the original");
    BrSwapU16Array(buf, 0);
    check(buf[0] == 0 && buf[1] == 1, "MUTATION: a count of zero is a no-op");
    BrSwapU16Array(buf, -3);
    check(buf[0] == 0 && buf[1] == 1,
          "MUTATION: a NEGATIVE count is a no-op, not four billion swaps");

    for (i = 0; i < 0x130; i++) tab[i] = (void *)(long)(i + 100);
    check(BrHandleLookup(tab, 0) == 0, "handle 0 is reserved null");
    check(BrHandleLookup(tab, 1) == (void *)101L, "handle 1 is the first valid");
    check(BrHandleLookup(tab, 0x12E) == (void *)(long)(0x12E + 100L), "0x12E valid");
    check(BrHandleLookup(tab, 0x12F) == 0, "0x12F rejected");
    check(BrHandleLookup(tab, 0xFFFFFFFFu) == 0, "huge handle rejected");

    printf(g_fail ? "\nFAILED\n" : "\nALL PASSED\n");
    return g_fail;
}
