/* cycles.c: the emulated clock against known work: a loop of register
 * instructions, and one of RAM loads */
#include <stdio.h>
#include "ps1.h"

static volatile uint32_t s_buf[1024];

int main(void)
{
    uint64_t t0, t1;
    uint32_t i, acc = 0;
    FILE *f;
    ps1_hw_init();
    pcdrv_init();
    ps1_log_open("stderr.txt");
    t0 = ps1_time_ns();
    for (i = 0; i < 10000000; i++)              /* ~4 instructions an iteration: 40M cycles, 1.18 s at 33.87 MHz */
        __asm__ volatile("" : "+r"(acc));
    t1 = ps1_time_ns();
    printf("register loop: %u ms\n", (unsigned)((t1 - t0) / 1000000));
    t0 = ps1_time_ns();
    for (i = 0; i < 2000000; i++)
        acc += s_buf[i & 1023];
    t1 = ps1_time_ns();
    printf("load loop: %u ms (%u)\n", (unsigned)((t1 - t0) / 1000000), (unsigned)acc);
    fflush(stdout);
    f = fopen("done", "w");
    fclose(f);
    for (;;) ;
}
