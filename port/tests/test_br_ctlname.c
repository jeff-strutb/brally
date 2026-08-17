/* test_br_ctlname.c -- 0x10058AF0 and the three control-name tables.
 *
 * What is pinned, and why each one is here:
 *
 *   - the two split points (4 buttons, 128 buttons) and the two record
 *     counts (10, 134), because those four numbers are what make the three
 *     tables abut and are the only reason the extents are known;
 *   - the two DIFFERENT axis code bases, 0x86 for the mouse and 0x80 for the
 *     joystick, because harmonising them is the obvious tidy-up and it would
 *     renumber every saved mouse-axis binding;
 *   - that both clears are SHORTER than the table they clear, checked by
 *     poisoning the tables and looking for surviving poison past the clear;
 *   - key uniqueness, which is the property that makes slice2_23.c's linear
 *     search by key well defined at all;
 *   - the keyboard table's contents against the image.
 *
 * BrStrGet is stood in for so the strings are fixed and the test does not
 * depend on BRString.dll being present.  The seven strings below are the ones
 * the shipped resource actually holds.
 */
#include "br_ctlname.h"

#include <stdio.h>
#include <string.h>

static int g_fails;
#define CHECK(c) do { if (!(c)) { \
    printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); g_fails++; } } while (0)

/* ---- stand-in for slice4_52.c's BrStrGet ------------------------------ */
static const char *const s_apszAxis[BR_CTLNAME_STR_AXIS_COUNT] = {
    "Left", "Right", "Forward", "Backward", "Z Negative", "Z Positive"
};

const char *BrStrGet(int id);
const char *BrStrGet(int id)
{
    if (id == BR_CTLNAME_STR_BUTTON) return "BUTTON %d";
    if (id >= BR_CTLNAME_STR_AXIS_FIRST &&
        id <  BR_CTLNAME_STR_AXIS_FIRST + BR_CTLNAME_STR_AXIS_COUNT) {
        return s_apszAxis[id - BR_CTLNAME_STR_AXIS_FIRST];
    }
    return NULL;
}

/* ---- the counts are the three tables' abutment ----------------------- */
static void test_counts(void)
{
    CHECK(sizeof(BrCfgRec) == 0x24);
    CHECK(BR_CTLNAME_MOUSE_COUNT == 10);
    CHECK(BR_CTLNAME_JOY_COUNT   == 134);
    CHECK(BR_CTLNAME_KEY_COUNT   == 120);
    /* 0x10B71B08 + 10*0x24 == 0x10B71C70 and + 134*0x24 == 0x10B72F48 */
    CHECK(0x10B71B08u + (unsigned)(BR_CTLNAME_MOUSE_COUNT * 0x24) == 0x10B71C70u);
    CHECK(0x10B71C70u + (unsigned)(BR_CTLNAME_JOY_COUNT   * 0x24) == 0x10B72F48u);
}

/* ---- the mouse table: 4 buttons then 6 axes -------------------------- */
static void test_mouse(void)
{
    int i;

    BrCtlNameInit();

    for (i = 0; i < BR_CTLNAME_MOUSE_BUTTONS; i++) {
        char sz[32];
        sprintf(sz, "BUTTON %d", i);
        CHECK(g_aBrCtlNameMouse[i].key == (uint32_t)i);
        CHECK(strcmp(g_aBrCtlNameMouse[i].szText, sz) == 0);
    }
    for (i = BR_CTLNAME_MOUSE_BUTTONS; i < BR_CTLNAME_MOUSE_COUNT; i++) {
        int k = i - BR_CTLNAME_MOUSE_BUTTONS;
        CHECK(g_aBrCtlNameMouse[i].key ==
              (uint32_t)(BR_CTLNAME_MOUSE_AXIS_BASE + k));
        CHECK(strcmp(g_aBrCtlNameMouse[i].szText, s_apszAxis[k]) == 0);
    }
    /* the split is at 4, not 3 and not 5 */
    CHECK(g_aBrCtlNameMouse[3].key == 3u);
    CHECK(g_aBrCtlNameMouse[4].key == 0x86u);
    CHECK(g_aBrCtlNameMouse[9].key == 0x8Bu);
}

/* ---- the joystick table: 128 buttons then the same 6 axes ------------ */
static void test_joystick(void)
{
    int i;

    BrCtlNameInit();

    for (i = 0; i < BR_CTLNAME_JOY_BUTTONS; i++) {
        char sz[32];
        sprintf(sz, "BUTTON %d", i);
        CHECK(g_aBrCtlNameJoy[i].key == (uint32_t)i);
        if (strcmp(g_aBrCtlNameJoy[i].szText, sz) != 0) {
            printf("FAIL joy[%d] name '%s'\n", i, g_aBrCtlNameJoy[i].szText);
            g_fails++;
            break;                       /* one report, not 128 */
        }
    }
    for (i = BR_CTLNAME_JOY_BUTTONS; i < BR_CTLNAME_JOY_COUNT; i++) {
        int k = i - BR_CTLNAME_JOY_BUTTONS;
        CHECK(g_aBrCtlNameJoy[i].key == (uint32_t)(BR_CTLNAME_JOY_AXIS_BASE + k));
        CHECK(strcmp(g_aBrCtlNameJoy[i].szText, s_apszAxis[k]) == 0);
    }
    CHECK(g_aBrCtlNameJoy[127].key == 127u);
    CHECK(g_aBrCtlNameJoy[128].key == 0x80u);
    CHECK(g_aBrCtlNameJoy[133].key == 0x85u);
}

/* ---- the two axis bases are SIX APART, and that is deliberate -------- */
static void test_axis_bases_differ(void)
{
    BrCtlNameInit();
    CHECK(g_aBrCtlNameMouse[BR_CTLNAME_MOUSE_BUTTONS].key -
          g_aBrCtlNameJoy[BR_CTLNAME_JOY_BUTTONS].key == 6u);
    /* ...while the NAMES are the same six strings in the same order. */
    CHECK(strcmp(g_aBrCtlNameMouse[BR_CTLNAME_MOUSE_BUTTONS].szText,
                 g_aBrCtlNameJoy[BR_CTLNAME_JOY_BUTTONS].szText) == 0);
    CHECK(strcmp(g_aBrCtlNameMouse[9].szText,
                 g_aBrCtlNameJoy[133].szText) == 0);
}

/* ---- both clears stop short of the end of their table ---------------- *
 * Poison, build, then look at one byte inside the cleared region and one
 * byte past it.  Both bytes are name padding no sprintf reaches, so the only
 * thing that can have zeroed either is the clear. */
static void test_short_clears(void)
{
    unsigned char *pM = (unsigned char *)g_aBrCtlNameMouse;
    unsigned char *pJ = (unsigned char *)g_aBrCtlNameJoy;

    memset(g_aBrCtlNameMouse, 0xAA, sizeof g_aBrCtlNameMouse);
    memset(g_aBrCtlNameJoy,   0xAA, sizeof g_aBrCtlNameJoy);
    BrCtlNameInit();

    /* mouse: record 2's name padding at +100 is inside the 220-byte clear */
    CHECK(100 < BR_CTLNAME_MOUSE_CLEAR);
    CHECK(pM[100] == 0x00);
    /* record 8's name padding at +310 is past it and keeps the poison */
    CHECK(310 >= BR_CTLNAME_MOUSE_CLEAR && 310 < (int)sizeof g_aBrCtlNameMouse);
    CHECK(pM[310] == 0xAA);

    /* joystick: +200 inside the 344-byte clear, +410 past it */
    CHECK(200 < BR_CTLNAME_JOY_CLEAR);
    CHECK(pJ[200] == 0x00);
    CHECK(410 >= BR_CTLNAME_JOY_CLEAR);
    CHECK(pJ[410] == 0xAA);

    /* and the poison did not corrupt what the loops DO write */
    CHECK(g_aBrCtlNameMouse[8].key == 0x8Au);
    CHECK(strcmp(g_aBrCtlNameMouse[8].szText, "Z Negative") == 0);
    CHECK(g_aBrCtlNameJoy[11].key == 11u);
    CHECK(strcmp(g_aBrCtlNameJoy[11].szText, "BUTTON 11") == 0);
}

/* ---- a linear search by key needs the keys to be unique -------------- */
static int keys_unique(const BrCfgRec *a, int n)
{
    int i, j;
    for (i = 0; i < n; i++) {
        for (j = i + 1; j < n; j++) {
            if (a[i].key == a[j].key) return 0;
        }
    }
    return 1;
}

static void test_keys_unique(void)
{
    BrCtlNameInit();
    CHECK(keys_unique(g_aBrCtlNameMouse, BR_CTLNAME_MOUSE_COUNT));
    CHECK(keys_unique(g_aBrCtlNameJoy,   BR_CTLNAME_JOY_COUNT));
    CHECK(keys_unique(g_aBrCtlNameKey,   BR_CTLNAME_KEY_COUNT));
}

/* ---- the keyboard table, against the image --------------------------- */
static void test_keyboard(void)
{
    int i;
    int fAscending = 1;

    for (i = 1; i < BR_CTLNAME_KEY_COUNT; i++) {
        if (g_aBrCtlNameKey[i].key <= g_aBrCtlNameKey[i - 1].key) {
            fAscending = 0;
        }
    }
    CHECK(fAscending);

    CHECK(g_aBrCtlNameKey[0].key == 0x01u);
    CHECK(strcmp(g_aBrCtlNameKey[0].szText, "ESCAPE") == 0);
    CHECK(g_aBrCtlNameKey[BR_CTLNAME_KEY_COUNT - 1].key == 0xDDu);
    CHECK(strcmp(g_aBrCtlNameKey[BR_CTLNAME_KEY_COUNT - 1].szText,
                 "APP MENU") == 0);

    /* 0x2B is DIK_BACKSLASH and its name is one backslash. */
    for (i = 0; i < BR_CTLNAME_KEY_COUNT; i++) {
        if (g_aBrCtlNameKey[i].key == 0x2Bu) {
            CHECK(strcmp(g_aBrCtlNameKey[i].szText, "\\") == 0);
        }
        /* DIK_RETURN, and the game spells it ENTER. */
        if (g_aBrCtlNameKey[i].key == 0x1Cu) {
            CHECK(strcmp(g_aBrCtlNameKey[i].szText, "ENTER") == 0);
        }
        /* every name fits the record and is terminated */
        CHECK(strlen(g_aBrCtlNameKey[i].szText) <
              sizeof g_aBrCtlNameKey[i].szText);
    }
}

/* ---- the tables view slice2_23.c's lookup takes ---------------------- */
static void test_tables_view(void)
{
    const BrCfgTables *pT = BrCtlNameTables();

    CHECK(pT->aT0 == g_aBrCtlNameKey);
    CHECK(pT->aT1 == g_aBrCtlNameJoy);      /* kinds 1 AND 2 */
    CHECK(pT->aT3 == g_aBrCtlNameMouse);
    /* the counts the lookup uses must be the counts these arrays have */
    CHECK(BR_CFG_T0_COUNT == (int)(sizeof g_aBrCtlNameKey / sizeof g_aBrCtlNameKey[0]));
    CHECK(BR_CFG_T1_COUNT == (int)(sizeof g_aBrCtlNameJoy / sizeof g_aBrCtlNameJoy[0]));
    CHECK(BR_CFG_T3_COUNT == (int)(sizeof g_aBrCtlNameMouse / sizeof g_aBrCtlNameMouse[0]));
}

/* ---- rebuilding is idempotent --------------------------------------- */
static void test_idempotent(void)
{
    BrCfgRec aSaved[BR_CTLNAME_JOY_COUNT];

    BrCtlNameInit();
    memcpy(aSaved, g_aBrCtlNameJoy, sizeof aSaved);
    BrCtlNameInit();
    CHECK(memcmp(aSaved, g_aBrCtlNameJoy, sizeof aSaved) == 0);
}

int main(void)
{
    test_counts();
    test_mouse();
    test_joystick();
    test_axis_bases_differ();
    test_short_clears();
    test_keys_unique();
    test_keyboard();
    test_tables_view();
    test_idempotent();

    if (g_fails != 0) { printf("%d FAILURE(S)\n", g_fails); return 1; }
    printf("br_ctlname: all checks passed\n");
    return 0;
}
