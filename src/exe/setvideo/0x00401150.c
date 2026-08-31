/* Auto-generated from disassembly — 0x00401150
 * CHK_FGets: getc line reader. Translates a bare CR (and CR/LF) to LF,
 * NUL-terminates, and returns the write cursor. Walks the `buf` parameter
 * in place (no separate cursor) so the empty-input path reloads buf and the
 * count `n` stays in ebp (frameless). getc() inlines _cnt/_ptr/_filbuf. */
#ifdef BR_MATCHING_BUILD
/* @implements 0x00401150 setvideo.exe CHK_FGets */

/* SetVideo.exe is /ML (static CRT): CRT calls are E8, not FF 15. */
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct CHKFile {
    FILE *fp;
    char *name;
} CHKFile;

char *CHK_FGets(char *buf, int n, CHKFile *p)
{
    int i;
    int c;

    for (i = 0; i < n; i++) {
        c = getc(p->fp);
        if (c != -1) {
            if (c == 13) {
                *buf++ = 10;
                *buf++ = 0;
                c = getc(p->fp);
                if (c == -1) return buf;
                if (c == 10) return buf;
                ungetc(c, p->fp);
                return buf;
            }
            if (c == 10) {
                *buf++ = 10;
                *buf = 0;
                return buf + 1;
            }
            *buf++ = (char)c;
        } else {
            if (i == 0) return 0;
            *buf = 0;
            return buf + 1;
        }
    }
    return buf;
}

#endif /* BR_MATCHING_BUILD */
