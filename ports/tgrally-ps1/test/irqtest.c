/* irqtest.c: the VBlank interrupt, the clock, and a fault report */
#include <stdio.h>
#include "ps1.h"

int main(void)
{
    uint64_t t0;
    FILE *f;
    ps1_hw_init();
    pcdrv_init();
    ps1_pc_sample(30);
    t0 = ps1_time_ns();
    while (ps1_vblanks() < 120)
        ;
    printf("120 vblanks in %d ms\n", (int)((ps1_time_ns() - t0) / 1000000));
    fflush(stdout);
    f = fopen("done", "w");
    fclose(f);
    *(volatile int *)1 = 0;                 /* an address error: the fault report */
    for (;;) ;
}
