/* libctest.c: the C library's files and strings on the console */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "ps1.h"

int main(void)
{
    char line[256];
    FILE *f;
    int n = 0;
    ps1_hw_init();
    pcdrv_init();
    ps1_tty("open\n");
    f = fopen("options.txt", "r");
    ps1_tty(f ? "opened\n" : "no file\n");
    while (f && fgets(line, sizeof line, f)) {
        if (n % 20 == 0) ps1_tty(line);
        n++;
    }
    ps1_tty("loop done\n");
    printf("lines %d\n", n);
    ps1_tty("printed\n");
    fflush(stdout);
    f = fopen("done", "w");
    fclose(f);
    for (;;) ;
}
