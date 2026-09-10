/* WHAT IT DOES: builds the next auto-numbered file name for a save slot.
 * Strips the input name down to its base (dropping an 'X'/'x' placeholder --
 * which it also rewrites to '0' in the caller's buffer -- and everything from
 * the first '.'), scans a 100-entry table of existing names for the ones that
 * share that base and sort after the input, keeps the last such as the
 * template, lifts that template's extension and its trailing decimal counter,
 * bumps the counter by one, and writes base + newcounter + extension into
 * *ppOut. */
/* @implements 0x10055F40 glide BrSaveNextName_10055F40
 * @cpp_kind method
 * @cpp_symbol ?NextName@Save55F40@@QAEXPAEPAPAD@Z
 *
 * Thiscall, two stack args (`ret 8`), 799 B.  `this` is the name-table
 * object; the 100 candidate strings start at this+4 (stride 0x104).  All of
 * strcpy/strlen/strcmp are inlined intrinsics; strncmp, atoi and _itoa are
 * real /MD imports.  The four masked _itoa calls into digits+0..+3 are the
 * original's, not a slip -- each writes the decimal of one masked byte and
 * the next overwrites all but its first char.
 */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#endif
#include <stdlib.h>
#include <string.h>

class Save55F40 {
public:
    int  head;                      /* +0x00 */
    char table[100][0x104];         /* +0x04 candidate names */

    void NextName(unsigned char *pIn, char **ppOut);   /* 0x10055F40 */
};

void Save55F40::NextName(unsigned char *pIn, char **ppOut)
{
    unsigned char cand[0x104];
    char          digits[0x104];
    unsigned char suffix[0x104];
    unsigned char stripped[0x104];
    int           i;
    int           n;
    int           flag;
    int           count;
    char         *pEntry;
    unsigned char *p;
    int           num;

    memset(cand, 0, sizeof(cand));
    memset(digits, 0, sizeof(digits));
    flag = 0;
    memset(suffix, 0, sizeof(suffix));

    strcpy((char *)cand, (char *)pIn);

    n = 0;
    if (strlen((char *)pIn) != 0) {
        n = 0;
        for (i = 0; i < (int)strlen((char *)pIn); i++) {
            unsigned char b = pIn[i];
            if (b == 'X' || b == 'x') {
                pIn[i] = '0';
                flag = 1;
            } else if (!flag) {
                if (b == '.') {
                    flag = 1;
                } else {
                    stripped[i] = b;
                    n++;
                }
            }
        }
    }

    n = (short)n;
    pEntry = table[0];
    count = 100;
    do {
        if (strncmp((char *)pIn, pEntry, n) == 0) {
            if (strcmp((char *)pIn, pEntry) < 0)
                strcpy((char *)cand, pEntry);
        }
        pEntry += 0x104;
        count--;
    } while (count != 0);

    p = cand + (strlen((char *)cand) - 1);
    if (p != cand) {
        do {
            if (*p == '.')
                break;
            p--;
        } while (p != cand);
    }
    if (p != cand) {
        int len2 = (int)(strlen((char *)cand) - (p - cand) - 1);
        memcpy(suffix, p, len2);
        suffix[len2] = 0;
    }

    if (n < (int)strlen((char *)cand)) {
        i = n;
        while (i < (int)strlen((char *)cand)) {
            unsigned char b = cand[i];
            if (b < '0' || b > '9')
                break;
            digits[i - n] = b;
            i++;
        }
    }

    num = atoi(digits) + 1;
    memset(digits, 0, sizeof(digits));
    _itoa(num & 0xff000000, digits + 0, 10);
    _itoa(num & 0xff0000, digits + 1, 10);
    _itoa(num & 0xff00, digits + 2, 10);
    _itoa(num & 0xff, digits + 3, 10);

    strcpy(*ppOut, (char *)stripped);
    strcat(*ppOut, digits);
    strcat(*ppOut, (char *)suffix);
}
