/* test_pod.c -- verify the decompiled POD reader against the retail archive.
 *
 * This is what replaces byte-matching: the decompiled code is correct if it
 * parses real shipped game data correctly.
 */
#include "br_pod.h"
#include "br_testdata.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fail;

static void check(int cond, const char *pszWhat)
{
    printf("  [%s] %s\n", cond ? "PASS" : "FAIL", pszWhat);
    if (!cond)
        g_fail = 1;
}

int main(int argc, char **argv)
{
    BR_REQUIRE_TESTDATA("testdata/BossRally.pod", "pod");
    const char *pszPath = (argc > 1) ? argv[1] : "testdata/BossRally.pod";
    BrPod pod;
    uint32_t i, cb = 0;
    void *pv;
    int iEntry;
    char szClean[BR_POD_NAME_LEN + 1];

    printf("opening %s\n", pszPath);
    if (BrPodOpen(&pod, pszPath) != 0) {
        printf("  [FAIL] BrPodOpen\n");
        return 1;
    }
    check(1, "BrPodOpen succeeded");
    printf("  %u entries\n", pod.cEntries);

    for (i = 0; i < pod.cEntries && i < 20; i++) {
        printf("    [%2u] off=%-8u len=%-8u %s\n", i,
               pod.aEntries[i].offData, pod.aEntries[i].cbData,
               pod.aEntries[i].szName);
    }

    /* name cleanup: lowercase input must match the uppercased stored name */
    BrPodCleanupName("modellights.blob", szClean);
    check(strcmp(szClean, "MODELLIGHTS.BLOB") == 0,
          "CleanupName uppercases");

    iEntry = BrPodGetNumForName(&pod, "modellights.blob");
    check(iEntry >= 0, "GetNumForName finds entry case-insensitively");

    check(BrPodGetNumForName(&pod, "nosuchfile.dat") == -1,
          "GetNumForName returns -1 for a missing name");

    if (iEntry >= 0) {
        uint32_t cbExpect = BrPodGetLength(&pod, iEntry);
        pv = BrPodLoad(&pod, iEntry, &cb);
        check(pv != NULL, "LoadPod returns data");
        check(cb == cbExpect && cb > 0, "LoadPod length matches GetPodLength");

        /* the entry must lie inside the archive, before the directory */
        check(pod.aEntries[iEntry].offData >= 16, "data starts after header");
        free(pv);
    }

    check(BrPodGetLength(&pod, 9999) == 0, "out-of-range index is rejected");

    BrPodClose(&pod);
    printf(g_fail ? "\nFAILED\n" : "\nALL PASSED\n");
    return g_fail;
}
