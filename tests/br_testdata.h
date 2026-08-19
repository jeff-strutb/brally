/* br_testdata.h -- skip, don't fail, when the extracted assets are absent.
 *
 * Several suites read real game data out of testdata/. Those files are
 * EXTRACTED at build time from media the person building this supplies; they
 * are deliberately not committed, so a fresh clone does not have them.
 *
 * Without this header those suites FAIL on a clean checkout, which makes a
 * correct tree look broken and trains people to ignore red. A missing asset is
 * not a defect in the code under test -- it is a suite that cannot run. Say so
 * and exit 0.
 *
 * Deliberately NOT the same as passing: the runner counts skips separately, so
 * "everything green" can never quietly mean "nothing actually ran".
 */
#ifndef BR_TESTDATA_H
#define BR_TESTDATA_H

#include <stdio.h>

/* Use at the very top of main(). Exits 0 with a SKIP line if the file is
 * unreadable. `what` names the suite for the log. */
#define BR_REQUIRE_TESTDATA(path, what)                                        \
    do {                                                                       \
        FILE *br__f = fopen((path), "rb");                                     \
        if (!br__f) {                                                          \
            printf("SKIP %s: needs %s -- run the asset extraction first "      \
                   "(see README, 'Asset policy')\n", (what), (path));          \
            return 0;                                                          \
        }                                                                      \
        fclose(br__f);                                                         \
    } while (0)

#endif /* BR_TESTDATA_H */
