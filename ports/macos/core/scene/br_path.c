/* br_path.c -- 0x10008B90, path to basename.
 *
 * WHY IT IS ITS OWN MODULE
 *
 * Two callers need it: 0x100085F0 (BrPodCleanupName, in br_pod.c) and the
 * POD writer in slice6_78. It was originally transcribed inside slice6_78,
 * and reaching it from br_pod meant linking that whole packet -- which pulled
 * in BrChkAlloc, BrOperatorNew, BrRandom, the text-state setters and more,
 * for a forty-line string function.
 *
 * The alternative was to write a second copy in br_pod.c, which is exactly the
 * "two bodies for one original address" mistake CONVENTIONS.md warns about:
 * it links cleanly, and the two copies drift the moment either is touched.
 *
 * So it lives here, dependency-free, and both callers reach the same body.
 */
#include "br_path.h"
#include <string.h>

/* ==========================================================================
 * 5. 0x10008B90 -- path to basename
 * ========================================================================== */

void BrPodWriterMakeName(void *pStream, const char *pszSrc, char *pszDst)
{
    const char *p;
    size_t      len;

    /* The original's __thiscall `this`.  Both call sites set ecx; the body
     * never reads it.  See the signature-conflict note in the header. */
    (void)pStream;

    len = strlen(pszSrc);

    if (len == 0u) {
        /* BUG IN THE ORIGINAL, and it cannot be reproduced safely.  With an
         * empty string the scan starts at pszSrc - 1, which is already below
         * the terminator it tests for, so it walks BACKWARDS through memory
         * until it happens on a 0x5C byte and then copies from there.  This
         * port copies the empty string instead.  DEVIATION. */
        pszDst[0] = '\0';
        return;
    }

    /* edi = pszSrc + len - 1: the LAST character, which is never itself
     * examined as a separator.  A trailing '\\' is therefore kept. */
    p = pszSrc + (len - 1u);

    if (p != pszSrc) {
        for (;;) {
            if (p[-1] == '\\') {
                break;              /* p is just past the last separator */
            }
            --p;
            if (p == pszSrc) {
                break;              /* ran back to the start; copy it all */
            }
        }
    }

    /* The original recomputes the length of the tail and copies it with the
     * NUL, i.e. strcpy. */
    memcpy(pszDst, p, strlen(p) + 1u);
}
