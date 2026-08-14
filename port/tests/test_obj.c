#include "br_obj.h"
#include <stdio.h>

static int g_fail;
static void check(int c, const char *w)
{ printf("  [%s] %s\n", c ? "PASS" : "FAIL", w); if (!c) g_fail = 1; }

int main(void)
{
    BrObjHeader o;
    o.f00 = 1; o.f04 = 2; o.f08 = 3; o.f0C = 4; o.f10 = 0x5EED;

    BrObjClear(&o);
    check(o.f00==0 && o.f04==0 && o.f08==0 && o.f0C==0, "first four dwords cleared");
    /* the original clears exactly four dwords; +0x10 must survive */
    check(o.f10 == 0x5EED, "+0x10 is NOT cleared");
    check(BrObjGetF10(&o) == 0x5EED, "getter returns +0x10");

    { BrObjInline b;
      BrObjInitInline(&b);
      check(b.pBuf == (void *)b.inline_, "+0x10 points at the inline buffer");
      check(b.f00==0 && b.f04==0 && b.f08==0 && b.f0C==0, "header cleared"); }

    { BrObjFlagCount fc; fc.flag = 0; fc.count = 7;
      BrObjConsumeFlag(&fc);
      check(fc.count == 7, "clear flag does not count");
      fc.flag = 1;
      BrObjConsumeFlag(&fc);
      check(fc.count == 8 && fc.flag == 0, "set flag counted once and cleared");
      BrObjConsumeFlag(&fc);
      check(fc.count == 8, "already-consumed flag does not count again"); }

    printf(g_fail ? "\nFAILED\n" : "\nALL PASSED\n");
    return g_fail;
}
