/* perf_ps1.c -- the game's own performance-meter marks (BrPerfMark) timed
 * on the console: each mark ends the section the previous mark began, and
 * TGR_PERF prints the console time of each section (named by the address
 * of the mark that began it) over the report's 60 retraces. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

uint64_t ps1_time_ns(void);
void __orig_BrPerfMark(int bar, int r, int g, int b, int a);

#define NSEC 48
static uint32_t s_ra[NSEC];
static uint64_t s_ns[NSEC];
static uint32_t s_n[NSEC];
static uint32_t s_last_ra;
static uint64_t s_last;

void BrPerfMark(int bar, int r, int g, int b, int a)
{
    uint32_t ra = (uint32_t)__builtin_return_address(0);
    uint64_t t = ps1_time_ns();
    int i;
    if (s_last) {
        for (i = 0; i < NSEC && s_ra[i] && s_ra[i] != s_last_ra; i++)
            ;
        if (i < NSEC) {
            s_ra[i] = s_last_ra;
            s_ns[i] += t - s_last;
            s_n[i]++;
        }
    }
    s_last_ra = ra;
    s_last = t;
    __orig_BrPerfMark(bar, r, g, b, a);
}

struct ProfEntry { const char *name; uint32_t t8, n; };
extern struct ProfEntry prof_table[] __attribute__((weak));
extern uint32_t prof_count __attribute__((weak));

void tgr_fn_report(void);
void tgr_perf_marks_report(void)
{
    int i;
    tgr_fn_report();
    if (&prof_count)                            /* PROF=list builds: the wrapped functions */
        for (i = 0; i < (int)prof_count; i++) {
            if (prof_table[i].n)
                fprintf(stderr, "perf: fn %-28s %5u ms %6u calls %5u us each\n", prof_table[i].name,
                        (unsigned)((uint64_t)prof_table[i].t8 * 8 / 33869), (unsigned)prof_table[i].n,
                        (unsigned)((uint64_t)prof_table[i].t8 * 8000 / 33869 / prof_table[i].n));
            prof_table[i].t8 = prof_table[i].n = 0;
        }
    for (i = 0; i < NSEC && s_ra[i]; i++) {
        fprintf(stderr, "perf: mark %08X %u ms over %u\n", (unsigned)s_ra[i], (unsigned)(s_ns[i] / 1000000),
                (unsigned)s_n[i]);
        s_ns[i] = 0;
        s_n[i] = 0;
    }
}

/* ---- -finstrument-functions builds (XCFLAGS for the core): every core
 * function's calls, inclusive and exclusive console time, by root counter 2
 * (the system clock / 8), printed with the TGR_PERF report ---- */
#define NOI __attribute__((no_instrument_function))
#define FN_SLOTS 1024
struct FnStat { uint32_t fn, incl, excl, n; };
static struct FnStat s_fn[FN_SLOTS];
/* the open calls, a stack for each of the game's threads: a call is found
 * again at its exit by the function and the stack pointer (the same at both
 * hooks); calls left open above it (a longjmp) are dropped */
uintptr_t host_thread_self(void);
#define NTH 16
#define DEPTH 64
static struct Open { uint32_t fn, sp, t0, child; } s_stk[NTH][DEPTH];
static int s_dep[NTH];
static uint32_t s_t32, s_c2;

static NOI uint32_t now8(void)
{
    uint32_t v = *(volatile uint32_t *)0xBF801120 & 0xFFFF;
    s_t32 += (v - s_c2) & 0xFFFF;
    s_c2 = v;
    return s_t32;
}

static NOI struct FnStat *slot(uint32_t fn)
{
    uint32_t h = (fn >> 2) * 2654435761u >> 22;
    while (s_fn[h].fn && s_fn[h].fn != fn)
        h = (h + 1) & (FN_SLOTS - 1);
    s_fn[h].fn = fn;
    return &s_fn[h];
}

/* the hooks pass on the instrumented function's stack pointer, as it is at their entry */
__asm__(".set push; .set noreorder\n"
        ".globl __cyg_profile_func_enter\n__cyg_profile_func_enter:\n j prof_enter\n move $a2, $sp\n"
        ".globl __cyg_profile_func_exit\n__cyg_profile_func_exit:\n j prof_exit\n move $a2, $sp\n"
        ".set pop\n");

NOI void prof_enter(void *fn, void *site, uint32_t sp)
{
    uint32_t th = (uint32_t)host_thread_self() % NTH;
    struct Open *o = s_stk[th];
    int d = s_dep[th];
    (void)site;
    while (d > 0 && o[d - 1].sp <= sp)         /* frames at or below this one have gone */
        d--;
    if (d < DEPTH) {
        o[d].fn = (uint32_t)fn;
        o[d].sp = sp;
        o[d].t0 = now8();
        o[d].child = 0;
        d++;
    }
    s_dep[th] = d;
}

NOI void prof_exit(void *fn, void *site, uint32_t sp)
{
    uint32_t th = (uint32_t)host_thread_self() % NTH, t;
    struct Open *o = s_stk[th];
    struct FnStat *f;
    int i;
    (void)site;
    for (i = s_dep[th] - 1; i >= 0 && o[i].fn != (uint32_t)fn; i--)
        ;
    if (i < 0)
        return;
    t = now8() - o[i].t0;
    f = slot(o[i].fn);
    f->incl += t;
    f->excl += t - o[i].child;
    f->n++;
    if (i > 0)
        o[i - 1].child += t;
    s_dep[th] = i;
}

/* the functions over 1 ms: address, calls, inclusive and exclusive ms (the
 * names come from the ELF: tools/fnprof.py) */
NOI void tgr_fn_report(void)
{
    int i;

    for (i = 0; i < FN_SLOTS; i++)
        if (s_fn[i].fn) {
            if (s_fn[i].incl > 4234)            /* 1 ms */
                fprintf(stderr, "perf: fn %08X %u %u %u\n", (unsigned)s_fn[i].fn, (unsigned)s_fn[i].n,
                        (unsigned)((uint64_t)s_fn[i].incl * 8 / 33869), (unsigned)((uint64_t)s_fn[i].excl * 8 / 33869));
            s_fn[i].incl = s_fn[i].excl = s_fn[i].n = 0;
        }
}

/* ---- TGR_BENCH: what the console charges for an instruction, a load from
 * RAM, from the fast RAM, a store, a multiply (cycles per 1000) ---- */
static NOI uint32_t c8(void) { return *(volatile uint32_t *)0xBF801120 & 0xFFFF; }

NOI void tgr_bench(void)
{
    static volatile uint32_t ram[256];
    volatile uint32_t *fast = (volatile uint32_t *)0x1F800200u;
    uint32_t t, d[6], i, acc = 0;
    t = c8();
    for (i = 0; i < 1000; i++)
        __asm__ volatile("addu %0, %0, %1; addu %0, %0, %1; addu %0, %0, %1; addu %0, %0, %1" : "+r"(acc) : "r"(i));
    d[0] = (c8() - t) & 0xFFFF;
    t = c8();
    for (i = 0; i < 1000; i++)
        acc += ram[i & 255] + ram[(i + 7) & 255] + ram[(i + 13) & 255] + ram[(i + 29) & 255];
    d[1] = (c8() - t) & 0xFFFF;
    t = c8();
    for (i = 0; i < 1000; i++)
        acc += fast[i & 63] + fast[(i + 7) & 63] + fast[(i + 13) & 63] + fast[(i + 29) & 63];
    d[2] = (c8() - t) & 0xFFFF;
    t = c8();
    for (i = 0; i < 1000; i++) {
        ram[i & 255] = i; ram[(i + 7) & 255] = i; ram[(i + 13) & 255] = i; ram[(i + 29) & 255] = i;
    }
    d[3] = (c8() - t) & 0xFFFF;
    t = c8();
    for (i = 0; i < 1000; i++)
        acc += (uint32_t)((int64_t)(int32_t)acc * (int32_t)i >> 16);
    d[4] = (c8() - t) & 0xFFFF;
    {
        /* the GTE: 6 vertex words in, RTPT, 6 results out */
        uint32_t dg, dn;
        __asm__ volatile("ctc2 %0, $0; ctc2 %0, $2; ctc2 %0, $4; ctc2 %1, $1; ctc2 %1, $3; ctc2 %0, $5; ctc2 %0, $6; ctc2 %2, $7; ctc2 %2, $26"
                         : : "r"(0x1000), "r"(0), "r"(1000));
        t = c8();
        for (i = 0; i < 1000; i++) {
            uint32_t a0, a1, a2;
            __asm__ volatile("mtc2 %3, $0; mtc2 %4, $1; mtc2 %3, $2; mtc2 %4, $3; mtc2 %3, $4; mtc2 %4, $5; nop; nop;"
                             ".word 0x4A280030; mfc2 %0, $12; mfc2 %1, $13; mfc2 %2, $14; nop"
                             : "=r"(a0), "=r"(a1), "=r"(a2) : "r"(i), "r"(100));
            acc += a0 + a1 + a2;
        }
        dg = (c8() - t) & 0xFFFF;
        t = c8();
        for (i = 0; i < 1000; i++)
            __asm__ volatile("nop");
        dn = (c8() - t) & 0xFFFF;
        fprintf(stderr, "bench: RTPT loop %u cycles a loop (an empty loop %u)\n", (unsigned)(dg * 8 / 1000), (unsigned)(dn * 8 / 1000));
    }
    fprintf(stderr, "bench (cycles for 1000 loops): 4 adds %u, 4 RAM loads %u, 4 fast loads %u, 4 RAM stores %u, mult %u (%u)\n",
            (unsigned)(d[0] * 8), (unsigned)(d[1] * 8), (unsigned)(d[2] * 8), (unsigned)(d[3] * 8), (unsigned)(d[4] * 8),
            (unsigned)acc);
}
