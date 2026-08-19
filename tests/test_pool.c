#include "br_pool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fail;
static void check(int c, const char *w)
{ printf("  [%s] %s\n", c ? "PASS" : "FAIL", w); if (!c) g_fail = 1; }

int main(void)
{
    size_t frames = 4;
    size_t bytes = frames * BR_POOL_SLOTS_BANK * BR_POOL_SLOT_SIZE;
    uint8_t *mem = calloc(1, bytes);
    BrPool p; uint8_t *a, *b, *last = NULL;
    int i, distinct = 1;

    memset(&p, 0, sizeof(p)); p.pBase = mem; p.frame = 0; p.count = 0;

    a = BrPoolAlloc(&p);
    b = BrPoolAlloc(&p);
    check(a == mem, "first slot is at the base");
    check(b == mem + BR_POOL_SLOT_SIZE, "slots are 64 bytes apart");

    /* fill the frame */
    for (i = 2; i < BR_POOL_SLOTS_USED; i++) last = BrPoolAlloc(&p);
    check(last == mem + (BR_POOL_SLOTS_USED - 1) * BR_POOL_SLOT_SIZE,
          "256 usable slots per frame");

    /* past the limit every request must alias the same overflow slot */
    a = BrPoolAlloc(&p);
    b = BrPoolAlloc(&p);
    check(a == b, "overflow requests alias one shared slot");
    check(a == mem + BR_POOL_SLOTS_USED * BR_POOL_SLOT_SIZE,
          "overflow slot is index 256 of the same bank");
    check(a != NULL, "overflow never returns NULL");
    check(BrPoolCount(&p) == BR_POOL_SLOTS_USED + 2,
          "overflowing requests are still counted");

    /* frames must not collide */
    p.frame = 1; p.count = 0;
    a = BrPoolAlloc(&p);
    check(a == mem + BR_POOL_SLOTS_BANK * BR_POOL_SLOT_SIZE,
          "frame 1 starts one full 257-slot bank in");
    p.frame = 2; p.count = 0;
    b = BrPoolAlloc(&p);
    check(b != a && b > a, "frames do not overlap");
    (void)distinct;

    free(mem);
    printf(g_fail ? "\nFAILED\n" : "\nALL PASSED\n");
    return g_fail;
}
