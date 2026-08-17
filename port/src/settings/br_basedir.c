/* br_basedir.c -- see br_basedir.h. 0x10063860. */
#include "br_basedir.h"

#include <stddef.h>
#include <string.h>

/* 0x10B73540. */
static char             s_szBase[BR_BASEDIR_MAX];
static BrBaseDirReadFn  s_pfnRead;
static void            *s_pUser;

void BrBaseDirSetHost(BrBaseDirReadFn pfnRead, void *pUser)
{
    s_pfnRead = pfnRead;
    s_pUser   = pUser;
}

const char *BrBaseDir(void) { return s_szBase; }

void BrBaseDirResetForTest(void)
{
    memset(s_szBase, 0, sizeof s_szBase);
    s_pfnRead = NULL;
    s_pUser   = NULL;
}

/* 0x10063860 */
void BrBaseDirInit(void)
{
    size_t len;

    /* No host read installed is the same outcome as the key being absent:
     * 0x10063883 and 0x100638BC both jump to 0x10063909, and the original
     * makes no distinction between "no key" and "no value". Inventing one
     * here would be inventing behaviour. */
    if (s_pfnRead == NULL ||
        s_pfnRead(s_pUser, BR_BASEDIR_REGKEY, BR_BASEDIR_REGVAL,
                  s_szBase, sizeof s_szBase) != 0) {
        /* 0x10063909: strcpy(base, "c:\") and return. NOT the empty string --
         * a machine with no registry entry gets an ABSOLUTE path at the root
         * of C:, which is why a mis-installed copy looks for c:\TRACKS\...
         * rather than ./TRACKS. */
        s_szBase[0] = 0;
        strncat(s_szBase, BR_BASEDIR_FALLBACK, sizeof s_szBase - 1);
        return;
    }

    s_szBase[sizeof s_szBase - 1] = 0;   /* the host must not run us off the end */
    len = strlen(s_szBase);

    /* 0x100638CD: cmp byte ptr [ecx + 0x10B7353F], 0x5C, with ecx = strlen.
     * 0x10B7353F is one byte BEFORE the buffer, so the address is
     * base[len - 1] -- the last character. Read the displacement without the
     * register and it looks like a fixed global.
     *
     * DEVIATION, and the only one here: with len == 0 the original reads
     * base[-1], one byte before the buffer, because it has no guard. That is a
     * real out-of-bounds read on an empty "Directory" value, and this port
     * does not reproduce it -- an empty value takes the append path, which is
     * what the original does for every value that does not already end in a
     * separator. The difference is observable only when the byte before the
     * buffer happens to be 0x5C, which is not a state any caller can arrange
     * and not a behaviour worth preserving. Stated rather than silently
     * "fixed". */
    if (len > 0 && s_szBase[len - 1] == BR_BASEDIR_SEP) {
        return;                          /* 0x100638D4 je -> the epilogue */
    }

    /* 0x100638D6: strcat(base, "\") -- 0x100ACACC is a one-character string. */
    if (len + 1 < sizeof s_szBase) {
        s_szBase[len]     = BR_BASEDIR_SEP;
        s_szBase[len + 1] = 0;
    }
}
