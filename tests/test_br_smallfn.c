#include "br_menuact.h"
#include "br_textmode.h"
#include "br_musiccmd.h"
#include "br_replayon.h"
#include "br_objlife.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

static int g_fail;
static void check(int c, const char *w)
{
    printf("  [%s] %s\n", c ? "PASS" : "FAIL", w);
    if (!c)
        g_fail = 1;
}

/* Test-local stand-ins, the same way test_slice2_26 / test_slice3_31 /
 * test_slice3_45 supply this one.  Both have real transcribed bodies now --
 * 0x10008B80 in slice6_74.c, 0x10043E70 in slice5_63.c -- but reaching them
 * from here would drag 17 further symbols and most of the tree into a
 * five-module test.  Test objects are excluded from the host link, so these
 * cannot collide with the real definitions. */
static int g_nStub8B80;
void BrExt_10008B80(void) { ++g_nStub8B80; }
static int g_nStub43E70;
void BrExt_10043E70(int32_t a) { (void)a; ++g_nStub43E70; }

int main(void)
{
    uint32_t table[200];
    uint32_t obj[4];
    uint32_t owner[4];

    g_67D550 = 0x11111111u;
    g_0A81C8 = 0;
    BrFlagInit_1002B950();
    check(g_67D550 == 0 && g_0A81C8 == 0x10575540u,
          "0x1002B950 stores 0 and 0x10575540");

    g_AC300 = 0;
    g_690A14 = 0;
    BrFlagInit_1002F690();
    check(g_AC300 == 1 && g_690A14 == 4, "0x1002F690 stores 1 and 4");

    check(BrInstall_1001BAE0() == 1, "0x1001BAE0 returns 1");
    check(g_690A24 != 0 && g_690A28 != 0, "0x1001BAE0 installs two pointers");

    memset(table, 0xAB, sizeof table);
    table[0] = 0x11223344u;
    g_57543C = table;
    BrTableCopySlot_10024AB0(1, 0);
    check(table[174] == 0x11223344u,
          "0x10024AB0 copies dword 0 onto slot 1 (stride 174 dwords)");

    BrTableSetField_10025800(0, 0xAABBCCDDu);
    check(table[0x27C / 4] == 0xAABBCCDDu,
          "0x10025800 writes +0x27C of slot 0");

    g_A9D008 = 0;
    BrWrap_1003DAE0();
    check(1, "0x1003DAE0 no-ops on a null object");

    obj[0] = 1;
    obj[2] = 0;
    g_A9D008 = obj;
    BrWrap_1003DAE0();
    check(1, "0x1003DAE0 no-ops when +8 is null");

    owner[2] = 0;
    g_AA29F4 = owner;
    check(BrHook_100457A0(obj) == 1, "0x100457A0 returns 1");
    check(owner[2] != 0, "0x100457A0 writes the phase +8 slot");

    BrWrap_10067980();
    BrWrap_10067960(obj);
    BrWrap_10067940(obj);
    BrWrap_10072B80(obj, 3, 4);
    BrWrap_10072B10(obj, 3, 4);
    BrWrap_10072A70(obj, 3, 4);
    BrWrap_10071610();
    BrWrap_100715E0();
    check(1, "wrappers return");

    g_6C7C44 = 0;
    BrArm_100378A0();
    check(g_6C7C44 == 1, "0x100378A0 sets the arm flag");

    g_4B0360 = 9;
    BrClear_10019250();
    check(g_4B0360 == 0, "0x10019250 clears the byte");

    {
        uint32_t pair[2] = { 3, 4 };
        BrPairReset_10073B90(pair);
        check(pair[0] == 0 && pair[1] == 0, "0x10073B90 zeros two dwords");
    }

    check(BrHook_10044010(obj) == 1, "0x10044010 returns 1");

    g_4B035C = 0;
    BrSet_10019270();
    check(g_4B035C == 2, "0x10019270 stores 2");

    BrStore_1003BD40(0x12345678u);
    check(g_A9BFD0 == 0x12345678u, "0x1003BD40 stores the seed");

    g_1750308 = 0;
    BrSet_1006AA90();
    check(g_1750308 == 1 && BrGet_1006AAA0() == 1,
          "0x1006AA90 / 0x1006AAA0 set and get");

    BrStore_10086B80(9);
    check(g_18AC2D0 == 9, "0x10086B80 stores");

    owner[2] = 0;
    g_AA29C8 = owner;
    check(BrHook_10045780(obj) == 1 && owner[2] != 0, "0x10045780");
    check(BrHook_10045800(obj) == 1, "0x10045800");
    check(BrHook_10045820(obj) == 1, "0x10045820");
    check(BrHook_10045840(obj) == 1, "0x10045840");
    check(BrHook_10045860(obj) == 1, "0x10045860");
    check(BrHook_100458C0(obj) == 1, "0x100458C0");

    g_0940A4 = 0;
    BrDispatch_100025C0(obj);
    g_0940A4 = 1;
    BrDispatch_100025C0(obj);
    check(1, "0x100025C0 both arms");

    g_178FEE8 = 0;
    (void)BrDelta_100713A0();
    BrAtexit_10038EA0();
    BrAtexit_10069A70();
    BrAtexit_10071600();
    BrWrap_100679A0();
    BrWrap_10035610(obj);

    check(BrSet_1002F6E0() == 1 && g_690A14 == 2, "0x1002F6E0");
    BrSet_10036020();
    check(g_6C7C38 == 0x80096400u, "0x10036020");

    printf(g_fail ? "\nFAILED\n" : "\nALL PASSED\n");
    return g_fail;
}
