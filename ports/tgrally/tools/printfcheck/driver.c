/* driver.c: the port's sprintf (platform/libc/xprintf.c) over the cases
 * tools/printfcheck.py writes, one per line: a format, a tab, the argument
 * kind (d an int, f a double given as its 16 hex digits, s a string), a tab,
 * the argument.  Prints each result on its own line. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "tgr_libc.h"

int main(void)
{
    char line[512], out[512];
    int r;
    while (fgets(line, sizeof line, stdin)) {
        char *fmt = line, *kind, *arg, *nl;
        if ((nl = strchr(line, '\n')) != NULL)
            *nl = 0;
        if (!(kind = strchr(fmt, '\t')))
            continue;
        *kind++ = 0;
        if (!(arg = strchr(kind, '\t')))
            continue;
        *arg++ = 0;
        if (*kind == 'd') {
            r = tgr_sprintf(out, fmt, (int)strtoll(arg, NULL, 0));
            (void)r;
        } else if (*kind == 'f') {
            uint64_t b = strtoull(arg, NULL, 16);
            double d;
            memcpy(&d, &b, 8);
            tgr_sprintf(out, fmt, d);
        } else {
            tgr_sprintf(out, fmt, arg);
        }
        {
            int n = (int)strlen(out), i;
            for (i = 0; i < n; i++)
                printf("%02x", (unsigned char)out[i]);
            putchar('\n');
        }
    }
    return 0;
}
