/* br_volume.c -- the host's volume service. See br_volume.h for what this
 * stands in for, which parts are transcribed and which are host, and why it
 * carries no @implements.
 *
 * SHORT VERSION: the game asks "is there a CD-ROM drive holding a disc whose
 * volume label is Boss Rally". There is never a drive here, but the disc's
 * contents are on the disk, extracted by tools/extract_assets.sh, and the
 * extraction records the disc's real ISO 9660 volume identifier. This reports
 * the extracted asset root as a volume carrying that recorded label. The
 * comparison the game applies to it is the game's own and is unchanged.
 *
 * NOTE ON THE FILE PLACEMENT: br_volume.h describes this as a host seam and
 * the brief asked for port/host/br_volume.c. It is here instead because
 * build.sh -- which this pass must not edit -- compiles exactly three things
 * out of port/host/: brally.c, br_stubs.c and the br_wire*.c glob. A file named
 * port/host/br_volume.c would never be compiled, which is the silent no-op
 * build.sh's own header warns about. port/src/ is discovered recursively, and
 * gamedata/ is the concern that "locate, read and decode the game's own files"
 * names, which is what this does.
 */
#include "br_volume.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Read cap for the manifest. The retail extraction's file list is ~274 entries
 * and about 12 KB; a megabyte is generous and bounded, and a manifest larger
 * than this is not one this tool wrote. */
#define BR_VOLUME_JSON_MAX (1u << 20)

#define BR_VOLUME_PATH_MAX 1024

static char        s_szRoot[BR_VOLUME_PATH_MAX];
static int         s_fRootSet;

static char        s_aszLabel[BR_VOLUME_MAX][BR_VOLUME_LABEL_MAX];
static int         s_cVol;
static const char *s_pszWhy = "no scan has been run";

/* ==========================================================================
 * A very small JSON reader
 * ==========================================================================
 *
 * Two values are needed out of the manifest -- one string and one array of
 * strings -- so this reads those two and nothing else rather than pulling in a
 * parser. It is deliberately strict about the shape it accepts.
 *
 * A key is matched INCLUDING both of its quotes, so "volume_label" cannot
 * match the longer key "volume_label_joliet", and the next non-space character
 * is required to be ':' so a key-shaped string appearing in a VALUE is not
 * mistaken for the key.
 */
static const char *json_skip_ws(const char *p)
{
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
        p++;
    return p;
}

static const char *json_find_key(const char *pszDoc, const char *pszKey)
{
    char   szQuoted[64];
    size_t cb;
    const char *p = pszDoc;

    cb = (size_t)snprintf(szQuoted, sizeof szQuoted, "\"%s\"", pszKey);
    if (cb >= sizeof szQuoted)
        return NULL;

    while ((p = strstr(p, szQuoted)) != NULL) {
        const char *q = json_skip_ws(p + cb);
        if (*q == ':')
            return json_skip_ws(q + 1);
        p += cb;
    }
    return NULL;
}

/* Decode one JSON string starting at *pp (which must point at the opening
 * quote) into pszOut. Advances *pp past the closing quote. Returns 0 on a
 * malformed string.
 *
 * TRUNCATION IS NOT AN ERROR: the original's label buffer is 0x104 bytes and a
 * longer label would have been truncated there too. A truncated label simply
 * does not compare equal to "Boss Rally", which is the honest outcome.
 *
 * A \uXXXX escape above U+007F becomes '?'. Nothing this port compares against
 * is non-ASCII, so the only effect is that an exotic label cannot match -- and
 * a label that cannot be represented is a label we should not claim to have
 * matched. */
static int json_string(const char **pp, char *pszOut, size_t cbOut)
{
    const char *p = *pp;
    size_t      n = 0;

    if (*p != '"')
        return 0;
    p++;
    while (*p != '"') {
        char ch;
        if (*p == '\0')
            return 0;                    /* unterminated */
        if (*p == '\\') {
            p++;
            switch (*p) {
            case '"':  ch = '"';  break;
            case '\\': ch = '\\'; break;
            case '/':  ch = '/';  break;
            case 'b':  ch = '\b'; break;
            case 'f':  ch = '\f'; break;
            case 'n':  ch = '\n'; break;
            case 'r':  ch = '\r'; break;
            case 't':  ch = '\t'; break;
            case 'u': {
                unsigned u = 0;
                int      i;
                for (i = 0; i < 4; i++) {
                    char c = p[1 + i];
                    unsigned d;
                    if (c >= '0' && c <= '9')      d = (unsigned)(c - '0');
                    else if (c >= 'a' && c <= 'f') d = (unsigned)(c - 'a') + 10u;
                    else if (c >= 'A' && c <= 'F') d = (unsigned)(c - 'A') + 10u;
                    else return 0;
                    u = (u << 4) | d;
                }
                p += 4;
                ch = (u <= 0x7Fu) ? (char)u : '?';
                break;
            }
            default:
                return 0;                /* not an escape this writer emits */
            }
        } else {
            ch = *p;
        }
        if (n + 1 < cbOut)
            pszOut[n++] = ch;
        p++;
    }
    pszOut[n < cbOut ? n : cbOut - 1] = '\0';
    *pp = p + 1;
    return 1;
}

/* ==========================================================================
 * The scan
 * ========================================================================== */

const char *BrVolumeRoot(void)
{
    const char *psz;

    if (s_fRootSet)
        return s_szRoot;
    psz = getenv(BR_VOLUME_ROOT_ENV);
    if (psz != NULL && psz[0] != '\0')
        return psz;
    return BR_VOLUME_ROOT_DEFAULT;
}

void BrVolumeSetRoot(const char *pszRoot)
{
    if (pszRoot == NULL || pszRoot[0] == '\0') {
        s_fRootSet   = 0;
        s_szRoot[0]  = '\0';
        return;
    }
    snprintf(s_szRoot, sizeof s_szRoot, "%s", pszRoot);
    s_fRootSet = 1;
}

static char *read_whole(const char *pszPath)
{
    FILE  *fh = fopen(pszPath, "rb");
    long   cb;
    char  *p;
    size_t cbRead;

    if (fh == NULL)
        return NULL;
    if (fseek(fh, 0, SEEK_END) != 0) { fclose(fh); return NULL; }
    cb = ftell(fh);
    if (cb < 0 || (unsigned long)cb > BR_VOLUME_JSON_MAX) { fclose(fh); return NULL; }
    if (fseek(fh, 0, SEEK_SET) != 0) { fclose(fh); return NULL; }
    p = (char *)malloc((size_t)cb + 1);
    if (p == NULL) { fclose(fh); return NULL; }
    cbRead = fread(p, 1, (size_t)cb, fh);
    fclose(fh);
    p[cbRead] = '\0';
    return p;
}

/* Does the manifest name at least one extracted file that is actually there?
 *
 * This is the "no assets means NO" rule, and it is what stops a manifest alone
 * from vouching for an empty tree. One present file is enough -- the manifest
 * is a provenance record, not an integrity check, and claiming to verify all
 * 274 would be a stronger claim than the fingerprint supports. */
static int any_listed_file_present(const char *pszDoc, const char *pszRoot)
{
    const char *p = json_find_key(pszDoc, "files");

    if (p == NULL || *p != '[')
        return 0;
    p = json_skip_ws(p + 1);
    while (*p == '"') {
        char szRel[BR_VOLUME_PATH_MAX];
        char szPath[BR_VOLUME_PATH_MAX * 2];
        FILE *fh;

        if (!json_string(&p, szRel, sizeof szRel))
            return 0;
        snprintf(szPath, sizeof szPath, "%s/%s", pszRoot, szRel);
        fh = fopen(szPath, "rb");
        if (fh != NULL) { fclose(fh); return 1; }
        p = json_skip_ws(p);
        if (*p != ',')
            break;
        p = json_skip_ws(p + 1);
    }
    return 0;
}

/* Rebuild the volume list. Runs on every query; see br_volume.h. */
static void scan(void)
{
    const char *pszRoot = BrVolumeRoot();
    char        szPath[BR_VOLUME_PATH_MAX * 2];
    char       *pszDoc;
    const char *p;

    s_cVol = 0;

    snprintf(szPath, sizeof szPath, "%s/%s", pszRoot, BR_VOLUME_MANIFEST);
    pszDoc = read_whole(szPath);
    if (pszDoc == NULL) {
        s_pszWhy = "no " BR_VOLUME_MANIFEST " under the asset root -- nothing "
                   "has been extracted here (run tools/extract_assets.sh)";
        return;
    }

    p = json_find_key(pszDoc, "volume_label");
    if (p == NULL || !json_string(&p, s_aszLabel[0], sizeof s_aszLabel[0])) {
        s_pszWhy = BR_VOLUME_MANIFEST " carries no readable volume_label";
        free(pszDoc);
        return;
    }

    if (!any_listed_file_present(pszDoc, pszRoot)) {
        s_pszWhy = BR_VOLUME_MANIFEST " lists no extracted file that is "
                   "present -- a manifest alone vouches for nothing";
        free(pszDoc);
        return;
    }

    free(pszDoc);
    s_cVol   = 1;
    s_pszWhy = "the extracted asset root, labelled from the disc it came off";
}

int BrVolumeCount(void)
{
    scan();
    return s_cVol;
}

const char *BrVolumeLabel(int i)
{
    scan();
    if (i < 0 || i >= s_cVol)
        return NULL;
    return s_aszLabel[i];
}

const char *BrVolumeWhy(void)
{
    return s_pszWhy;
}

int BrVolumePresent(const char *pszLabel)
{
    int i;

    if (pszLabel == NULL)
        return 0;
    scan();
    for (i = 0; i < s_cVol; i++) {
        /* 0x10037823..0x10037852. Case-sensitive, whole string, NUL-stopped.
         * Not a prefix test and not a substring test. */
        if (strcmp(s_aszLabel[i], pszLabel) == 0)
            return 1;
    }
    return 0;
}

/* ==========================================================================
 * The game's entry point
 * ==========================================================================
 *
 * WHAT IT DOES: answers the question the Championship screen asks before it
 * will open -- is the Boss Rally disc available. On the original that meant
 * finding a CD-ROM drive with the disc in it; here it means finding the disc's
 * contents where the builder extracted them, still carrying the label the disc
 * itself had.
 *
 * NO @implements. This is not a rendering of 0x1003EE90 / 0x10045A00; see
 * br_volume.h for exactly which parts of those bytes survive into it.
 *
 * The 1/0 shape is theirs: 0x1003EE9A seeds the result with 0 and 0x1003EF0F
 * replaces it with 1 the moment a volume answers.
 */
int32_t BrExt_10045A00(void)
{
    return BrVolumePresent(BR_VOLUME_WANT) ? 1 : 0;
}
