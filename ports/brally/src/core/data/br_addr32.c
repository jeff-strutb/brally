/* br_addr32.c: see br_addr32.h. */
#include <stdint.h>
#include <stddef.h>
#include "br_addr32.h"

#define BR_A32_SHIFT   24
#define BR_A32_SPAN    ((uintptr_t)1 << BR_A32_SHIFT)       /* 16 MB */
#define BR_A32_FIRST   0x20                                 /* above N64 segments */
#define BR_A32_LAST    0x7F       /* never 0x80000000+: N64 KSEG0 addresses */

static uintptr_t s_aWindow[256];    /* window id -> host base (0 = unused) */
static int       s_nNext = BR_A32_FIRST;

static int window_of(uintptr_t a)
{
    int i, best = 0;

    /* the window whose base is nearest below the address: a block given its
     * own window by br_addr32_window keeps every address inside it on that
     * window, even when an older, aligned window also covers the block --
     * and could end in the middle of it */
    for (i = BR_A32_FIRST; i < s_nNext; i++)
        if (s_aWindow[i] <= a && a - s_aWindow[i] < BR_A32_SPAN
            && (best == 0 || s_aWindow[i] > s_aWindow[best]))
            best = i;
    return best;
}

/* A loaded block's own window: every address inside it is an offset from
 * the block's start, so arithmetic the data does on its addresses stays
 * inside one window. */
void br_addr32_window(const void *base)
{
    uintptr_t a = (uintptr_t)base;
    int i;

    for (i = BR_A32_FIRST; i < s_nNext; i++)
        if (s_aWindow[i] == a)
            return;
    if (s_nNext <= BR_A32_LAST)
        s_aWindow[s_nNext++] = a;
}

uint32_t br_addr32(const void *p)
{
    uintptr_t a = (uintptr_t)p;
    int i;

    if (p == NULL)
        return 0;
    i = window_of(a);
    if (i == 0) {
        if (s_nNext > BR_A32_LAST)
            return 0;               /* out of windows: a bug to find, not paper over */
        i = s_nNext++;
        s_aWindow[i] = a & ~(BR_A32_SPAN - 1);
    }
    return ((uint32_t)i << BR_A32_SHIFT) | (uint32_t)(a - s_aWindow[i]);
}

void *br_ptr32(uint32_t a)
{
    uintptr_t base;

    if (a == 0)
        return NULL;
    base = s_aWindow[a >> BR_A32_SHIFT];
    if (base == 0)
        return NULL;
    return (void *)(base + (a & (uint32_t)(BR_A32_SPAN - 1)));
}
