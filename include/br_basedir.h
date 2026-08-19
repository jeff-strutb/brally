/* br_basedir.h -- 0x10063860, the game's INSTALL DIRECTORY.
 *
 * ARCHITECTURAL CONCERN: platform / configuration.
 *
 * WHAT IT IS
 *
 * RallyMain calls this at 0x1001CCA5, before anything reads a file. It fills
 * the 0x104-byte buffer at 0x10B73540 with the directory the installer
 * recorded, and guarantees a trailing separator. Everything downstream
 * concatenates onto it -- RallyMain itself immediately builds
 * <basedir> + "BossRally.cfg", and br_appstart builds <basedir> +
 * "BossRally.ini". With this unported the base is empty and every such path is
 * bare and relative, which is why nothing in this port has ever found a
 * configuration file.
 *
 * THE LISTING
 *
 *   RegOpenKeyExA(HKEY_LOCAL_MACHINE,
 *                 "SOFTWARE\SouthPeak Interactive\Boss Rally",
 *                 0, KEY_READ, &hKey)              0x1006387B
 *   failure -> the fallback below
 *   cbData = 0x104                                 0x1006389F
 *   RegQueryValueExA(hKey, "Directory", NULL, NULL, 0x10B73540, &cbData)
 *   RegCloseKey(hKey)                              ALWAYS, before the result
 *                                                  of the query is tested
 *   query failed -> the fallback below
 *   if the last byte is already '\\'  -> done       0x100638CD
 *   else strcat(base, "\\")                         0x100ACACC is "\\"
 *
 *   fallback (0x10063909): strcpy(base, "c:\\")     0x10077A6C
 *
 * FOUR THINGS A READER GETS WRONG, all established from the bytes:
 *
 * 1. RegCloseKey runs on the QUERY-FAILED path too. The handle is closed at
 *    0x100638B4 and only then is the saved result tested at 0x100638BA, so
 *    there is no leak on that arm -- unlike several other functions in this
 *    binary.
 *
 * 2. The fallback is "c:\\", not the empty string, and it is used for BOTH
 *    failures -- key missing and value missing. A machine with no registry
 *    entry does not get a relative path, it gets an absolute one pointing at
 *    the root of the C: drive. That is a meaningful behaviour to reproduce: it
 *    is why a mis-installed copy looks for c:\TRACKS\... rather than ./TRACKS.
 *
 * 3. The trailing-separator test is `cmp byte ptr [ecx + 0x10B7353F], 0x5C`
 *    with ecx = strlen. 0x10B7353F is the byte BEFORE the buffer, so
 *    [0x10B7353F + len] is base[len-1] -- the last character. Read the
 *    displacement without the register and it looks like a fixed address.
 *
 * 4. WITH len == 0 THAT READ IS OUT OF BOUNDS. `repne scasb` on an empty
 *    string gives len 0, and the compare then reads 0x10B7353F, one byte
 *    before the buffer. The original has no guard. An empty "Directory" value
 *    is what triggers it. See br_basedir.c for what this port does instead and
 *    why.
 *
 * The registry read itself is Win32, so it goes through a host hook; the
 * DECISIONS -- which failure goes to the fallback, when the separator is
 * appended, and that the close happens before the test -- are the game's and
 * are transcribed.
 */
#ifndef BR_BASEDIR_H
#define BR_BASEDIR_H

#include <stdint.h>
#include <stddef.h>

/* 0x10B73540. The original's buffer is 0x104 bytes and RegQueryValueExA is
 * told so at 0x1006389F. */
#define BR_BASEDIR_MAX 0x104

/* The two literals, from the image. */
#define BR_BASEDIR_REGKEY  "SOFTWARE\\SouthPeak Interactive\\Boss Rally"
#define BR_BASEDIR_REGVAL  "Directory"
#define BR_BASEDIR_FALLBACK "c:\\"          /* 0x10077A6C */
#define BR_BASEDIR_SEP      '\\'            /* 0x100ACACC */

/* The host's registry read.
 *
 *   returns 0 and fills pszOut  -- the value was read
 *   returns non-zero            -- key or value missing; either way the
 *                                  caller takes the "c:\" fallback
 *
 * One hook covers both Win32 calls because the original's two failures are
 * indistinguishable downstream: 0x10063883 and 0x100638BC jump to the same
 * label. Splitting them here would invent a distinction the game does not make.
 */
typedef int32_t (*BrBaseDirReadFn)(void *pUser, const char *pszKey,
                                   const char *pszValue,
                                   char *pszOut, size_t cbOut);

void        BrBaseDirSetHost(BrBaseDirReadFn pfnRead, void *pUser);

/* 0x10063860. */
void        BrBaseDirInit(void);

/* 0x10B73540, for the callers that concatenate onto it. */
const char *BrBaseDir(void);

void        BrBaseDirResetForTest(void);

#endif /* BR_BASEDIR_H */
