/* softfloat_check.c: platform/fp/softfloat.c against the host's FPU (built
 * and run on the host: test/softfloat_check.sh) */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

float __gcc_addsf3(float a, float b) { return a + b; }
float __gcc_subsf3(float a, float b) { return a - b; }
float __gcc_mulsf3(float a, float b) { return a * b; }
int __gcc_fixsfsi(float a) { return (int)a; }

#define __mulsf3 my_mulsf3
#define __addsf3 my_addsf3
#define __subsf3 my_subsf3
#define __ltsf2 my_ltsf2
#define __lesf2 my_lesf2
#define __eqsf2 my_eqsf2
#define __nesf2 my_nesf2
#define __gtsf2 my_gtsf2
#define __gesf2 my_gesf2
#define __fixsfsi my_fixsfsi
#define __floatsisf my_floatsisf
#include "../platform/fp/softfloat.c"

static uint32_t s_rng = 12345;
static uint32_t rnd(void) { s_rng ^= s_rng << 13; s_rng ^= s_rng >> 17; s_rng ^= s_rng << 5; return s_rng; }
static uint32_t bits(float f) { uint32_t u; memcpy(&u, &f, 4); return u; }
static float flt(uint32_t u) { float f; memcpy(&f, &u, 4); return f; }

/* operands: random bits, numbers near each other, special values */
static float operand(float other)
{
    static const uint32_t special[] = { 0, 0x80000000, 0x3F800000, 0xBF800000, 0x7F800000, 0xFF800000, 0x7FC00000,
                                        0x00000001, 0x007FFFFF, 0x00800000, 0x7F7FFFFF, 0x4B000000, 0x4F000000 };
    uint32_t r = rnd();
    switch (r & 7) {
    case 0: return flt(special[(r >> 3) % 13]);
    case 1: return flt(bits(other) + (int)((r >> 3) & 63) - 32);         /* a neighbour: cancellation */
    case 2: return flt(bits(other) ^ 0x80000000u);
    case 3: return flt((rnd() & 0x80000000u) | (((rnd() % 40) + 107) << 23) | (rnd() & 0x7FFFFF));  /* game-like */
    default: return flt(rnd());
    }
}

int main(void)
{
    long n, bad = 0;
    for (n = 0; n < 200000000L && bad < 20; n++) {
        float a = operand(1.0f), b = operand(a);
        float r1, r2;
        int i1, i2;
        r1 = my_mulsf3(a, b); r2 = a * b;
        if (bits(r1) != bits(r2) && !(isnan(r1) && isnan(r2))) { printf("mul %08x %08x: %08x %08x\n", bits(a), bits(b), bits(r1), bits(r2)); bad++; }
        r1 = my_addsf3(a, b); r2 = a + b;
        if (bits(r1) != bits(r2) && !(isnan(r1) && isnan(r2))) { printf("add %08x %08x: %08x %08x\n", bits(a), bits(b), bits(r1), bits(r2)); bad++; }
        r1 = my_subsf3(a, b); r2 = a - b;
        if (bits(r1) != bits(r2) && !(isnan(r1) && isnan(r2))) { printf("sub %08x %08x: %08x %08x\n", bits(a), bits(b), bits(r1), bits(r2)); bad++; }
        i1 = my_ltsf2(a, b) < 0; i2 = a < b;
        if (i1 != i2) { printf("lt %08x %08x\n", bits(a), bits(b)); bad++; }
        i1 = my_lesf2(a, b) <= 0; i2 = a <= b;
        if (i1 != i2) { printf("le %08x %08x\n", bits(a), bits(b)); bad++; }
        i1 = my_gtsf2(a, b) > 0; i2 = a > b;
        if (i1 != i2) { printf("gt %08x %08x\n", bits(a), bits(b)); bad++; }
        i1 = my_gesf2(a, b) >= 0; i2 = a >= b;
        if (i1 != i2) { printf("ge %08x %08x\n", bits(a), bits(b)); bad++; }
        i1 = my_eqsf2(a, b) == 0; i2 = a == b;
        if (i1 != i2) { printf("eq %08x %08x\n", bits(a), bits(b)); bad++; }
        if (fabsf(a) < 2.0e9f) {
            i1 = my_fixsfsi(a); i2 = (int)a;
            if (i1 != i2) { printf("fix %08x: %d %d\n", bits(a), i1, i2); bad++; }
        }
        {
            int iv = (int)rnd() >> (rnd() & 31);
            r1 = my_floatsisf(iv); r2 = (float)iv;
            if (bits(r1) != bits(r2)) { printf("float %d: %08x %08x\n", iv, bits(r1), bits(r2)); bad++; }
        }
    }
    printf("%ld operand pairs, %ld differences\n", n, bad);
    return bad != 0;
}
