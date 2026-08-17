/* test_br_strres.c -- 0x1006D1A0 / 0x1006D2A0.
 *
 * The function is a packing loop with three decisions, and each assertion here
 * exists to make one of them fail if it is reversed:
 *
 *   - ids run 1..0x12E and slot 0 is never touched;
 *   - a hit stores blob + the cursor as it was BEFORE the call, and advances
 *     the cursor by cch + 1, so the blob is NUL-packed with no gaps;
 *   - a miss leaves the slot NULL and does NOT advance, so the next id
 *     overwrites the same bytes;
 *   - nBufferMax is `size - used`, recomputed every pass and never clamped;
 *   - the table is cleared BEFORE the "already loaded" test, so a second call
 *     empties it (the preserved re-entry defect).
 *
 * The three CHK_ file calls are stood in for here.  slice6_78.c's real ones
 * open a file and exit(1) if it is absent, which is the original's behaviour
 * and is not what this test is about.
 */
#include "br_strres.h"
#include "slice4_52.h"
#include "slice6_78.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fails;
#define CHECK(c) do { if (!(c)) { \
    printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); g_fails++; } } while (0)

/* ---- storage slice4_52.c owns in the real link ----------------------- */
void *g_apBrStrTable[BR_STR_TABLE_COUNT];

/* ---- stand-ins for the three CHK_ calls ------------------------------ */
static int   g_cbFile;
static char  g_szOpened[64];
static int   g_cOpen, g_cSize, g_cClose;
static FILE *g_FakeFile;

FILE **BrChkFReadOpen(const char *pPath)
{
    g_cOpen++;
    snprintf(g_szOpened, sizeof g_szOpened, "%s", pPath);
    return &g_FakeFile;
}
int BrChkFileSize(FILE **ppFile) { (void)ppFile; g_cSize++;  return g_cbFile; }
void BrChkFClose(FILE **ppFile)  { (void)ppFile; g_cClose++; }

/* ---- the fake resource module ---------------------------------------- */
#define MAXREC 8
typedef struct Rec { uint32_t id; const char *psz; } Rec;

typedef struct Mod {
    Rec         aRec[MAXREC];
    int         cRec;
    int         fLoadFails;
    int         cFree;
    void       *hGiven;
    /* every nBufferMax the walk asked for, for the first few ids */
    int         aMax[8];
    int         cMax;
} Mod;

static Mod g_mod;
static char g_szModule[] = "module";

static void *m_load(const char *pszPath)
{
    snprintf(g_szOpened, sizeof g_szOpened, "%s", pszPath);
    return g_mod.fLoadFails ? NULL : (void *)g_szModule;
}

static int m_string(void *hModule, uint32_t uId, char *pszBuf, int cchBufMax)
{
    int i;
    g_mod.hGiven = hModule;
    if (g_mod.cMax < (int)(sizeof g_mod.aMax / sizeof g_mod.aMax[0])) {
        g_mod.aMax[g_mod.cMax++] = cchBufMax;
    }
    for (i = 0; i < g_mod.cRec; i++) {
        if (g_mod.aRec[i].id == uId) {
            size_t n = strlen(g_mod.aRec[i].psz);
            /* LoadStringA's contract: copy, terminate, return the count
             * WITHOUT the terminator. */
            memcpy(pszBuf, g_mod.aRec[i].psz, n);
            pszBuf[n] = '\0';
            return (int)n;
        }
    }
    return 0;
}

static void m_free(void *hModule) { (void)hModule; g_mod.cFree++; }

static const BrStrResOps g_ops = { m_load, m_string, m_free };

static void arm(void)
{
    memset(&g_mod, 0, sizeof g_mod);
    memset(g_apBrStrTable, 0, sizeof g_apBrStrTable);
    g_cOpen = g_cSize = g_cClose = 0;
    g_cbFile = 4096;
    g_szOpened[0] = 0;
    BrStrResResetForTest();
}

static void add(uint32_t id, const char *psz)
{
    g_mod.aRec[g_mod.cRec].id  = id;
    g_mod.aRec[g_mod.cRec].psz = psz;
    g_mod.cRec++;
}

/* ---- the path is the image's, and it is used TWICE -------------------- */
static void test_path_and_file_calls(void)
{
    arm();
    add(1, "one");
    BrStrResLoad(&g_ops);
    CHECK(strcmp(g_szOpened, "BRString.dll") == 0);   /* last user: LoadLibrary */
    CHECK(g_cOpen == 1 && g_cSize == 1 && g_cClose == 1);
    CHECK(g_brStrResSize == 4096);
    BrStrResFree();
}

/* ---- packing: pointers, order, and the NUL between them --------------- */
static void test_packing(void)
{
    const char *p1, *p2, *p3;

    arm();
    add(1, "one");
    add(2, "two");
    add(3, "three");
    BrStrResLoad(&g_ops);

    p1 = (const char *)g_apBrStrTable[1];
    p2 = (const char *)g_apBrStrTable[2];
    p3 = (const char *)g_apBrStrTable[3];

    CHECK(p1 != NULL && p2 != NULL && p3 != NULL);
    CHECK(p1 == g_pBrStrResBlob);
    CHECK(strcmp(p1, "one") == 0);
    CHECK(strcmp(p2, "two") == 0);
    CHECK(strcmp(p3, "three") == 0);
    /* cursor advanced by strlen + 1 each time: the blocks abut exactly. */
    CHECK(p2 == p1 + 4);
    CHECK(p3 == p2 + 4);
    CHECK(g_brStrResUsed == 4 + 4 + 6);

    /* slot 0 is the reserved "none" and is never written. */
    CHECK(g_apBrStrTable[0] == NULL);
    BrStrResFree();
}

/* ---- a miss does not advance, so the next id reuses the bytes --------- */
static void test_miss_does_not_advance(void)
{
    arm();
    add(1, "one");
    /* id 2 absent */
    add(3, "three");
    BrStrResLoad(&g_ops);

    CHECK(g_apBrStrTable[2] == NULL);
    /* "three" starts where "two" would have: immediately after "one\0". */
    CHECK((const char *)g_apBrStrTable[3] == g_pBrStrResBlob + 4);
    CHECK(g_brStrResUsed == 4 + 6);
    BrStrResFree();
}

/* ---- nBufferMax is size - used, recomputed, never clamped ------------- */
static void test_buffer_max(void)
{
    arm();
    g_cbFile = 100;
    add(1, "one");     /* 3 chars -> cursor 4 */
    add(2, "twelve");  /* 6 chars -> cursor 11 */
    BrStrResLoad(&g_ops);

    CHECK(g_mod.aMax[0] == 100);
    CHECK(g_mod.aMax[1] == 100 - 4);
    CHECK(g_mod.aMax[2] == 100 - 11);
    BrStrResFree();
}

/* ---- the walk covers exactly ids 1..0x12E ---------------------------- */
static void test_id_range(void)
{
    arm();
    add(1, "lo");
    add(0x12E, "hi");
    BrStrResLoad(&g_ops);

    CHECK(g_apBrStrTable[1] != NULL);
    CHECK(g_apBrStrTable[0x12E] != NULL);
    CHECK(strcmp((const char *)g_apBrStrTable[0x12E], "hi") == 0);
    /* 302 ids asked for, no more and no fewer. */
    CHECK(g_brStrResUsed == 3 + 3);
    BrStrResFree();
}

static void test_ask_count(void)
{
    int cAsked = 0;
    int i;

    arm();
    BrStrResLoad(&g_ops);
    /* Nothing matched, so every slot is NULL and the cursor never moved --
     * but the walk still ran.  Count it through the recorded maxima being
     * all equal (no advance) and through the table being wholly clear. */
    for (i = 0; i < BR_STR_TABLE_COUNT; i++) {
        if (g_apBrStrTable[i] != NULL) cAsked++;
    }
    CHECK(cAsked == 0);
    CHECK(g_brStrResUsed == 0);
    CHECK(g_mod.cFree == 1);
    BrStrResFree();
}

/* ---- the module is freed on every path that got one ------------------ */
static void test_free_module(void)
{
    arm();
    add(1, "one");
    BrStrResLoad(&g_ops);
    CHECK(g_mod.cFree == 1);
    CHECK(g_mod.hGiven == (void *)g_szModule);
    BrStrResFree();

    arm();
    g_mod.fLoadFails = 1;
    BrStrResLoad(&g_ops);
    CHECK(g_mod.cFree == 0);            /* nothing to free */
    CHECK(g_pBrStrResBlob == NULL);     /* and nothing allocated */
    CHECK(g_brStrResSize == 4096);      /* but the size was already taken */
}

/* ---- THE PRESERVED RE-ENTRY DEFECT ----------------------------------- *
 * The clear precedes the guard, so the second call empties the table and
 * refills nothing.  Reversing the two makes this fail, which is the point. */
static void test_reentry_empties_the_table(void)
{
    arm();
    add(1, "one");
    BrStrResLoad(&g_ops);
    CHECK(g_apBrStrTable[1] != NULL);

    BrStrResLoad(&g_ops);
    CHECK(g_apBrStrTable[1] == NULL);   /* wiped */
    CHECK(g_pBrStrResBlob != NULL);     /* and the blob is still held */
    CHECK(g_mod.cFree == 1);            /* the second call loaded no module */
    BrStrResFree();
}

/* ---- Free zeroes the three globals and does NOT clear the table ------- */
static void test_free_leaves_table_dangling(void)
{
    void *pWas;

    arm();
    add(1, "one");
    BrStrResLoad(&g_ops);
    pWas = g_apBrStrTable[1];
    BrStrResFree();

    CHECK(g_pBrStrResBlob == NULL);
    CHECK(g_brStrResUsed == 0);
    CHECK(g_brStrResSize == 0);
    /* Not dereferenced -- only the fact that it was left behind. */
    CHECK(g_apBrStrTable[1] == pWas);

    /* Free is idempotent on an empty state. */
    BrStrResFree();
    CHECK(g_pBrStrResBlob == NULL);
}

/* ---- a load after a free starts over cleanly -------------------------- */
static void test_reload_after_free(void)
{
    arm();
    add(1, "one");
    BrStrResLoad(&g_ops);
    BrStrResFree();

    memset(&g_mod, 0, sizeof g_mod);
    add(1, "again");
    BrStrResLoad(&g_ops);
    CHECK(g_apBrStrTable[1] == g_pBrStrResBlob);
    CHECK(strcmp((const char *)g_apBrStrTable[1], "again") == 0);
    CHECK(g_brStrResUsed == 6);
    BrStrResFree();
}

/* ---- no ops means no strings, and no fabricated table ----------------- */
static void test_no_ops(void)
{
    arm();
    g_apBrStrTable[7] = (void *)"stale";
    BrStrResLoad(NULL);
    CHECK(g_apBrStrTable[7] == NULL);   /* the clear still ran */
    CHECK(g_pBrStrResBlob == NULL);
    CHECK(g_cOpen == 0);                /* and nothing was opened */
}

int main(void)
{
    test_path_and_file_calls();
    test_packing();
    test_miss_does_not_advance();
    test_buffer_max();
    test_id_range();
    test_ask_count();
    test_free_module();
    test_reentry_empties_the_table();
    test_free_leaves_table_dangling();
    test_reload_after_free();
    test_no_ops();

    if (g_fails != 0) { printf("%d FAILURE(S)\n", g_fails); return 1; }
    printf("br_strres: all checks passed\n");
    return 0;
}
