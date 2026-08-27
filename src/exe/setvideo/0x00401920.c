/* Auto-generated from disassembly — 0x00401920
 * ReadListLine: fgets with #include nesting and comment-char skip.
 * Returns the (possibly replaced) FILE*, or 0 on EOF of the include stack. */
#ifdef BR_MATCHING_BUILD
/* @implements 0x00401920 setvideo.exe ReadListLine */

/* SetVideo.exe is /ML (static CRT): CRT calls are E8, not FF 15. */
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifndef true
#define true 1
#define false 0
#endif

typedef int (*funcptr)();

/* Forward declarations for unknown functions/globals */
int IncludeStackEmpty(void);
void PushInclude(void *);
void *PopInclude(void);
extern char gCommentChar;


FILE *ReadListLine(char *buf, int n, FILE *fp)
{
    char name[0x8000];
    char *p;
    char *end;

    for (;;) {
        if (fgets(buf, n, fp) == 0) {
            if (fp->_flag & 0x10) {
                if (IncludeStackEmpty())
                    return 0;
                fclose(fp);
                fp = (FILE *)PopInclude();
                continue;
            }
            printf("ReadList: error reading file.\n");
            exit(1);
            continue;
        }
        if (strncmp(buf, "#include", 8) == 0) {
            p = strchr(buf, '"');
            if (p != 0) {
                strcpy(name, p + 1);
                end = strrchr(name, '"');
                if (end != 0)
                    *end = 0;
            } else {
                p = strchr(buf, '<');
                if (p != 0) {
                    strcpy(name, p + 1);
                    end = strrchr(name, '>');
                    if (end != 0)
                        *end = 0;
                } else {
                    strcpy(name, buf + 8);
                }
            }
            PushInclude(fp);
            fp = fopen(name, "rt");
            if (fp == 0) {
                printf("ReadList: error opening #include file %s.\n", name);
                exit(1);
            }
            continue;
        }
        if (buf[0] == gCommentChar)
            continue;
        return fp;
    }
}


#endif /* BR_MATCHING_BUILD */
