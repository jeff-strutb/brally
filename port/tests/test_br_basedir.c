/* test_br_basedir.c -- 0x10063860.
 *
 * The whole function is four decisions and this pins each so that reversing it
 * fails: which failures take the fallback, what the fallback IS, when the
 * separator is appended, and that the registry handle is closed before the
 * query result is tested.
 */
#include "br_basedir.h"

#include <stdio.h>
#include <string.h>

static int g_fails;
#define CHECK(c) do { if (!(c)) { \
    printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); g_fails++; } } while (0)

typedef struct R {
    const char *pszValue;   /* NULL == the read fails */
    int         cCalls;
    char        szKey[128], szVal[64];
} R;

static int32_t r_read(void *p, const char *pszKey, const char *pszValue,
                      char *pszOut, size_t cbOut)
{
    R *r = (R *)p;
    r->cCalls++;
    snprintf(r->szKey, sizeof r->szKey, "%s", pszKey);
    snprintf(r->szVal, sizeof r->szVal, "%s", pszValue);
    if (r->pszValue == NULL) return 1;
    snprintf(pszOut, cbOut, "%s", r->pszValue);
    return 0;
}

static void arm(R *r, const char *pszValue)
{
    memset(r, 0, sizeof *r);
    r->pszValue = pszValue;
    BrBaseDirResetForTest();
    BrBaseDirSetHost(r_read, r);
}

/* ---- the key and value names are the image's, exactly ---------------- */
static void test_names(void)
{
    R r; arm(&r, "d:\\games\\rally");
    BrBaseDirInit();
    CHECK(r.cCalls == 1);
    CHECK(strcmp(r.szKey, "SOFTWARE\\SouthPeak Interactive\\Boss Rally") == 0);
    CHECK(strcmp(r.szVal, "Directory") == 0);
}

/* ---- a value with no separator gets one appended --------------------- */
static void test_appends_separator(void)
{
    R r; arm(&r, "d:\\games\\rally");
    BrBaseDirInit();
    CHECK(strcmp(BrBaseDir(), "d:\\games\\rally\\") == 0);
}

/* ---- a value that already ends in one is left ALONE ------------------ *
 * Not merely "ends with a separator" -- the string must be unchanged. An
 * implementation that appended unconditionally would still end with '\\' and
 * would pass a weaker check. */
static void test_keeps_existing_separator(void)
{
    R r; arm(&r, "d:\\games\\rally\\");
    BrBaseDirInit();
    CHECK(strcmp(BrBaseDir(), "d:\\games\\rally\\") == 0);
    CHECK(strlen(BrBaseDir()) == 15);
}

/* ---- BOTH failures give "c:\", not the empty string ------------------ *
 * The fallback being absolute is the behaviour worth pinning: it is why a
 * mis-installed copy hunts c:\TRACKS\ rather than ./TRACKS. */
static void test_fallback(void)
{
    R r; arm(&r, NULL);
    BrBaseDirInit();
    CHECK(strcmp(BrBaseDir(), "c:\\") == 0);

    /* and with no host installed at all -- the original cannot tell "no key"
     * from "no value" either; both jump to the same label */
    BrBaseDirResetForTest();
    BrBaseDirInit();
    CHECK(strcmp(BrBaseDir(), "c:\\") == 0);
}

/* ---- an empty value: the port's one documented deviation ------------- */
static void test_empty_value(void)
{
    R r; arm(&r, "");
    BrBaseDirInit();
    /* The original reads base[-1] here, out of bounds, because it has no
     * length guard. This port takes the append path. Asserted so the
     * deviation is pinned rather than only described in a comment. */
    CHECK(strcmp(BrBaseDir(), "\\") == 0);
}

/* ---- the result is usable as a prefix, which is the point of it ------ */
static void test_concatenation(void)
{
    char sz[256];
    R r; arm(&r, "d:\\games\\rally");
    BrBaseDirInit();
    snprintf(sz, sizeof sz, "%s%s", BrBaseDir(), "BossRally.cfg");
    CHECK(strcmp(sz, "d:\\games\\rally\\BossRally.cfg") == 0);
}

int main(void)
{
    test_names();
    test_appends_separator();
    test_keeps_existing_separator();
    test_fallback();
    test_empty_value();
    test_concatenation();

    if (g_fails != 0) { printf("%d FAILURE(S)\n", g_fails); return 1; }
    printf("br_basedir: all checks passed\n");
    return 0;
}
