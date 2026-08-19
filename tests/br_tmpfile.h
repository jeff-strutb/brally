/* br_tmpfile.h -- per-process scratch filenames for tests.
 *
 * WHY THIS EXISTS
 *
 * Six suites wrote scratch files under FIXED names -- "slice4_53_cfg.tmp",
 * "test_slice6_78_tmp.bin", "/tmp/br_slice1_01_scratch.bin" and so on. Run one
 * at a time that is fine. Run two copies of the suite at once and they open,
 * truncate and unlink each other's files, and the failures land on whichever
 * test lost the race -- so the reported failure is never the test with the bug,
 * because there is no bug in any of them.
 *
 * That is exactly how it showed up: a single "1 failed" that never reproduced
 * in twenty-five serial runs, then four different failures the moment six runs
 * were started concurrently. A test that only fails under contention is worse
 * than one that always fails, because the natural response is to re-run it and
 * move on.
 *
 * BrTmpPath() appends the process id, so concurrent runs cannot collide. It is
 * NOT mkstemp: these tests need a PATH they can open, close, reopen and pass to
 * code under test by name, which an already-open descriptor does not give them.
 *
 * The buffer is per-call-site static, so two live names in one function need
 * two call sites with different slots -- pass a distinct `slot` for each.
 */
#ifndef BR_TMPFILE_H
#define BR_TMPFILE_H

#include <stdio.h>
#include <unistd.h>

#define BR_TMP_SLOTS 4

static const char *BrTmpPath(int slot, const char *pszStem)
{
    static char aBuf[BR_TMP_SLOTS][256];
    if (slot < 0 || slot >= BR_TMP_SLOTS)
        slot = 0;
    snprintf(aBuf[slot], sizeof aBuf[slot], "%s.%ld.tmp",
             pszStem, (long)getpid());
    return aBuf[slot];
}

#endif /* BR_TMPFILE_H */
