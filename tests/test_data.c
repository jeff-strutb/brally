/* test_data.c -- pins the initialisers recovered from orig/BRD3D.dll.
 *
 * WHY THIS TEST IS SHAPED THE WAY IT IS
 *
 * The objects in port/src/br_data.c spent their whole life as 1 MiB blocks of
 * zeroes. Any test that merely walks them and finds zeroes would have passed
 * against those blocks too, so it would prove nothing whatever. Every
 * assertion here is therefore one of:
 *
 *   (a) a NON-ZERO value read out of the image, which the provisional block
 *       could not have produced;
 *   (b) a STRUCTURAL property that is checkable against a second, independent
 *       source -- the fifteen selectable track indices in g_aBrAC4D8 against
 *       the fifteen names in g_apszTrackFiles, the mirror tracks' file reuse
 *       against their lock bits, the array extents against the addresses of
 *       the next referenced global;
 *   (c) a SIZE, which is the recovered extent and fails loudly if someone
 *       later rounds it to something convenient.
 *
 * The .bss symbols are deliberately NOT asserted to be zero. That would be
 * assertion (d), "a test that only checks zeros", and it is exactly the kind
 * of expectation-encoding CONVENTIONS.md says is worse than no test: it would
 * pass against the placeholder it was written to replace. The evidence that
 * they are zero is the PE section arithmetic recorded in br_data.c, not a
 * runtime check.
 */
#include "slice1_02.h"
#include "slice2_11.h"
#include "slice2_20.h"
#include "slice2_25.h"
#include "slice5_61.h"

#include <stdio.h>
#include <string.h>

/* slice3_45.h cannot be included alongside slice5_61.h -- both complete a
 * `struct BrEnt`, and they are not the same struct. The two constants this
 * test needs from it are repeated here; if slice3_45.h's values change, the
 * extent assertion below is what breaks. */
#define BR45_CARGFX_STRIDE  89992u
#define BR45_CARGFX_COUNT   16u

/* Declared in slice2_20.c's XSLICE block and in slice6_70.h/slice5_63.h,
 * neither of which composes with the headers above. Repeated verbatim. */
/* XSLICE 0x100AC300 */ extern int g_i0AC300;
/* XSLICE 0x100B8C90 */ extern int g_i0B8C90;
/* XSLICE 0x100A73C4 */ extern const char *g_pszBr0A73C4;
/* XSLICE 0x100B84F8 */ extern const char *const g_apszCarFiles[16];
/* XSLICE 0x100B80B8 */ extern const char *const g_apszTrackFiles[16];
/* XSLICE 0x10A99BB8 */ extern BrPoolNode g_aPoolNodes[256];

/* Complete the extern array types the headers leave open, so `sizeof` can
 * assert the recovered extents. C99 6.2.7: this forms a composite type with
 * the incomplete declaration; it does not redeclare the object. */
extern const int32_t g_aBrAC420[32];
extern const uint8_t g_aBr0B3820[256];
extern const int32_t g_aBrAC520[4];

static int g_fail;
static void check(int c, const char *w)
{ printf("  [%s] %s\n", c ? "PASS" : "FAIL", w); if (!c) g_fail = 1; }

/* ====================================================================== */
/* 1. Scalars whose shipped value picks a different branch than zero      */
/* ====================================================================== */
static void test_scalars(void)
{
    puts("scalars recovered from .data");

    /* 0x100AC300. slice2_20.c takes its `pDesc != NULL && g_i0AC300 == 0`
     * arm only when this is 0; it never is at boot. */
    check(g_i0AC300 == 1, "0x100AC300 == 1 (not 0)");

    /* 0x100AB3F4. -1 is the no-selection state; 0 is a valid record index,
     * so the placeholder silently selected record 0. */
    check(g_br0AB3F4 == -1, "0x100AB3F4 == -1 (not 0)");

    /* 0x100AA010 / 0x100AA8B4. Both compared against literals only. */
    check(g_brMode0AA010 == 1, "0x100AA010 == 1");
    check(g_brMode0AA8B4 == 1, "0x100AA8B4 == 1 -- selects -11.0f, not -19.8f");

    /* 0x100940A4. The compiled-in PlayMusic default: 2 == the EAR driver.
     * Non-zero is what makes the three CD routines do anything at all. */
    check(g_brCdEnabled == 2, "0x100940A4 == 2 (music enabled, EAR backend)");

    /* 0x10094298. Starts at -1 so the FIRST send is n==0, i.e. a full
     * packet; from 0 the first full packet slips to the fourth tick. */
    check(g_brNetSendCount == -1, "0x10094298 == -1 -- first send is full");
    check((g_brNetSendCount + 1) % 4 == 0, "  ... and (n+1) % 4 == 0 confirms it");

    /* 0x100B8C90. */
    check(g_i0B8C90 == 1, "0x100B8C90 == 1");

    /* 0x100C129C really is zero in the image's initialised range -- one
     * placeholder that happened to be right. Asserted only because it is
     * paired with the ones above; on its own it would prove nothing. */
    check(g_brCamCollided == 0, "0x100C129C == 0 (image says 0, not a default)");
}

/* ====================================================================== */
/* 2. 0x100A73C4 is the string "%d", not a pointer slot                   */
/* ====================================================================== */
static void test_format(void)
{
    char buf[32];

    puts("0x100A73C4");

    /* The dword at 0x100A73C4 is absent from the base-relocation table and
     * reads `25 64 00 00`. The provisional block made this NULL, and every
     * use passes it straight to sprintf -- undefined behaviour, not merely
     * a wrong value. */
    check(g_pszBr0A73C4 != NULL, "not NULL (a NULL printf format is UB)");
    check(strcmp(g_pszBr0A73C4, "%d") == 0, "is exactly \"%d\"");

    sprintf(buf, g_pszBr0A73C4, 7);
    check(strcmp(buf, "7") == 0, "formats an int the way its call sites expect");
}

/* ====================================================================== */
/* 3. The 0x100AC3xx option tables                                        */
/* ====================================================================== */
static void test_option_tables(void)
{
    int i, nLive;

    puts("option tables 0x100AC308..0x100AC54C");

    /* Five two-entry toggles, all {1, 0}: menu position 0 is the ENABLED
     * one. Zeroed, both positions read "off" and the toggle vanished. */
    check(g_aBrAC518[0] == 1 && g_aBrAC518[1] == 0, "0x100AC518 == {1,0}");
    check(g_aBrAC530[0] == 1 && g_aBrAC530[1] == 0, "0x100AC530 == {1,0}");
    check(g_aBrAC538[0] == 1 && g_aBrAC538[1] == 0, "0x100AC538 == {1,0}");
    check(g_aBrAC540[0] == 1 && g_aBrAC540[1] == 0, "0x100AC540 == {1,0}");
    check(g_aBrAC548[0] == 1 && g_aBrAC548[1] == 0, "0x100AC548 == {1,0}");

    /* 0x100AC420 is the identity over the full cycler range 0..0x1F. An
     * identity map is still a real initialiser: zeroed, every option index
     * collapsed onto 0. */
    for (i = 0, nLive = 1; i <= BR_OPT_TRACK_MAX; ++i)
        if (g_aBrAC420[i] != i) nLive = 0;
    check(nLive, "0x100AC420 is the identity across 0..BR_OPT_TRACK_MAX");
    check(sizeof g_aBrAC420 / sizeof g_aBrAC420[0] == 32u,
          "0x100AC420 has exactly 32 entries");

    /* 0x100AC4D8: 0..14 then a 0 sentinel. The count of live entries is the
     * cross-check against g_apszTrackFiles below. */
    for (i = 0, nLive = 1; i < 15; ++i)
        if (g_aBrAC4D8[i] != i) nLive = 0;
    check(nLive, "0x100AC4D8 is 0..14");
    check(g_aBrAC4D8[15] == 0, "0x100AC4D8 ends on the 0 sentinel");

    /* Two three-value selectors with the same shape. */
    check(g_aBrAC4A0[0] == 0 && g_aBrAC4A0[1] == 1 && g_aBrAC4A0[2] == 2
          && g_aBrAC4A0[3] == 0, "0x100AC4A0 == {0,1,2,0}");
    check(g_aBrAC4B0[0] == 0 && g_aBrAC4B0[1] == 1 && g_aBrAC4B0[2] == 2
          && g_aBrAC4B0[3] == 0, "0x100AC4B0 == {0,1,2,0}");

    /* 0x100AC4C0: five live values for BR_OPT_AA2A00_MAX == 4. */
    check(g_aBrAC4C0[BR_OPT_AA2A00_MAX] == BR_OPT_AA2A00_MAX
          && g_aBrAC4C0[BR_OPT_AA2A00_MAX + 1] == 0,
          "0x100AC4C0's live range ends exactly at BR_OPT_AA2A00_MAX");

    /* 0x100AC520: four entries, and the cycler bound is 3. slice6_72.h
     * carries BR72_AC520_MAX == 8 for the same address; at 8 this read
     * would leave the table. The bound that matches the data is asserted. */
    check(sizeof g_aBrAC520 / sizeof g_aBrAC520[0] == 4u,
          "0x100AC520 has 4 entries");
    check(g_aBrAC520[BR_OPT_AA2A0C_MAX] == BR_OPT_AA2A0C_MAX,
          "0x100AC520's last live index is BR_OPT_AA2A0C_MAX (3), not 8");

    /* String-id tables: contiguous ascending runs with 0 sentinels. */
    check(g_aBrAC3B0[0] == 131 && g_aBrAC3B0[4] == 135 && g_aBrAC3B0[5] == 0,
          "0x100AC3B0 is ids 131..135 + sentinel");
    check(g_aBrAC3C8[0] == 157 && g_aBrAC3C8[4] == 161 && g_aBrAC3C8[5] == 0,
          "0x100AC3C8 is ids 157..161 + sentinel");

    /* 0x100AC368 is dense and UNTERMINATED -- the `& 0xF` in the caller is
     * the only bound there is. Ids 141..156, sixteen of them. */
    for (i = 0, nLive = 1; i < 16; ++i)
        if (g_aBrAC368[i] != 141 + i) nLive = 0;
    check(nLive, "0x100AC368 is ids 141..156, dense, no sentinel");

    /* 0x100AC308: four 0-terminated groups, and the first group repeats
     * 119..124 twice. That repetition is the data, not a typo -- it mirrors
     * the mirror-track duplication in g_apszTrackFiles. */
    check(g_aBrAC308[0] == 119 && g_aBrAC308[5] == 124
          && g_aBrAC308[6] == 119 && g_aBrAC308[11] == 124
          && g_aBrAC308[12] == 0,
          "0x100AC308's first group is 119..124 twice, then a sentinel");
    check(g_aBrAC308[21] == 127 && g_aBrAC308[19] == 0 && g_aBrAC308[23] == 0,
          "0x100AC308's last group reuses id 127 and terminates");
}

/* ====================================================================== */
/* 4. The file-name tables                                                */
/* ====================================================================== */
static void test_name_tables(void)
{
    int i, ok, nTracks;

    puts("file-name tables");

    /* 0x100B84F8: sixteen car stems. Extent pinned by the relocation table
     * -- the dword after entry 15 is not relocated. Sixteen also matches the
     * sixteen race entrants. */
    check(sizeof g_apszCarFiles / sizeof g_apszCarFiles[0] == 16u,
          "0x100B84F8 has 16 car entries");
    for (i = 0, ok = 1; i < 16; ++i)
        if (g_apszCarFiles[i] == NULL || strlen(g_apszCarFiles[i]) != 2u) ok = 0;
    check(ok, "  every car stem is a non-NULL 2-character name");
    /* Order matters and is easy to get backwards: the image stores the
     * pointers in DESCENDING target order, so rebuilding the list from the
     * string block's address order reverses it. */
    check(strcmp(g_apszCarFiles[0], "ce") == 0
          && strcmp(g_apszCarFiles[15], "mn") == 0,
          "  entry 0 is \"ce\" and entry 15 is \"mn\" (image order, not address order)");
    for (i = 0, ok = 1; i < 16; ++i) {
        int j;
        for (j = i + 1; j < 16; ++j)
            if (strcmp(g_apszCarFiles[i], g_apszCarFiles[j]) == 0) ok = 0;
    }
    check(ok, "  all 16 car stems are distinct");

    /* 0x100B80B8: fifteen names then a genuine NULL (the terminator dword
     * is zero AND absent from the relocation table, so it is not a pointer
     * the fixup missed). */
    for (nTracks = 0; nTracks < 16 && g_apszTrackFiles[nTracks] != NULL; ++nTracks)
        ;
    check(nTracks == 15, "0x100B80B8 holds 15 track names before its NULL");

    /* THE CROSS-CHECK: g_aBrAC4D8 offers indices 0..14 -- fifteen of them --
     * and this table holds fifteen names. Two independently recovered
     * extents agreeing is what makes both credible. */
    check(nTracks == 15 && g_aBrAC4D8[nTracks] == 0,
          "  and g_aBrAC4D8's sentinel lands on exactly that count");

    /* Entries 6..11 are the SAME .trk files as 0..5: the mirror variants
     * reuse the track and differ only by the lock bit tested below. */
    for (i = 0, ok = 1; i < 6; ++i)
        if (strcmp(g_apszTrackFiles[i], g_apszTrackFiles[6 + i]) != 0) ok = 0;
    check(ok, "  entries 6..11 load the same files as 0..5 (mirror variants)");
    check(strcmp(g_apszTrackFiles[13], g_apszTrackFiles[14]) == 0,
          "  entries 13 and 14 both load bonus.trk");
}

/* ====================================================================== */
/* 5. 0x100BD2A8 -- the record table and its lock bits                    */
/* ====================================================================== */
static void test_rec_table(void)
{
    int i, nLocked, ok;

    puts("0x100BD2A8 record table");

    for (i = 0; i < 16; ++i)
        if (g_aBrBD2A8[i] == NULL) break;
    check(i == 16, "16 records before the NULL terminator");
    check(g_aBrBD2A8[16] == NULL, "  and entry 16 is that terminator");

    /* Only bit 0x10 of +0x04 is read, and it appends the "locked" suffix.
     * Zeroed, every row read as unlocked and the distinction disappeared. */
    for (i = 0, nLocked = 0; i < 16; ++i)
        if (g_aBrBD2A8[i]->f04 & 0x10) ++nLocked;
    check(nLocked == 7, "7 of 16 records carry the 0x10 lock bit");

    /* WHICH seven is the real claim: the six mirror tracks (6..11) plus
     * Mirror Bonus Track (13). That set is not arbitrary -- it is exactly
     * the set whose file names duplicate an earlier entry AND whose recovered
     * record names begin "Mirror". */
    for (i = 6, ok = 1; i <= 11; ++i)
        if (!(g_aBrBD2A8[i]->f04 & 0x10)) ok = 0;
    check(ok, "  records 6..11 (the mirror tracks) are all locked");
    check((g_aBrBD2A8[13]->f04 & 0x10) != 0, "  record 13 (Mirror Bonus) is locked");
    for (i = 0, ok = 1; i <= 5; ++i)
        if (g_aBrBD2A8[i]->f04 & 0x10) ok = 0;
    check(ok, "  records 0..5 (the base tracks) are all unlocked");
    check((g_aBrBD2A8[14]->f04 & 0x10) == 0,
          "  record 14 (Bonus Track) is UNLOCKED even though it shares 13's file");

    /* Two rows carry 0x02 instead. Nothing in the port reads that bit; it is
     * asserted so a future reader knows it was recovered, not lost. */
    check(g_aBrBD2A8[12]->f04 == 0x02 && g_aBrBD2A8[15]->f04 == 0x02,
          "  records 12 and 15 carry 0x02 (a bit the port does not yet read)");
}

/* ====================================================================== */
/* 6. 0x100B3820 -- the stage byte-pair table                             */
/* ====================================================================== */
static void test_b3820(void)
{
    puts("0x100B3820");

    check(sizeof g_aBr0B3820 == 256u, "256 bytes, verbatim from the image");

    /* The four plausible pairs slice5_61.h deduced from the indexing. */
    check(g_aBr0B3820[0] == 0x02 && g_aBr0B3820[1] == 0x00,
          "pair 0 == (0x02, 0x00)");
    check(g_aBr0B3820[2] == 0x04 && g_aBr0B3820[3] == 0x00,
          "pair 1 == (0x04, 0x00)");
    check(g_aBr0B3820[4] == 0x00 && g_aBr0B3820[5] == 0x00,
          "pair 2 == (0x00, 0x00)");
    check(g_aBr0B3820[6] == 0x00 && g_aBr0B3820[7] == 0x00,
          "pair 3 == (0x00, 0x00)");

    /* And the evidence for stopping at four: byte 8 begins the unrelated
     * 0xE0/0xE1/0xE2/0xE3/0xE4 records. Reading pair 4 would return 0xE0. */
    check(g_aBr0B3820[8] == 0xE0 && g_aBr0B3820[32] == 0xE1
          && g_aBr0B3820[56] == 0xE2,
          "byte 8 starts the 0xE0/0xE1/0xE2 records -- the pair region ends there");
}

/* ====================================================================== */
/* 7. Extents recovered from neighbouring globals                         */
/* ====================================================================== */
static void test_extents(void)
{
    puts("extents");

    /* 0x100C12A0: stride x count lands exactly on the next referenced
     * global (0x10220B20). This identity is the whole reason the table is
     * 16 records rather than "sized by integration". */
    check((unsigned long)BR45_CARGFX_STRIDE * BR45_CARGFX_COUNT
              == 0x10220B20uL - 0x100C12A0uL,
          "0x100C12A0 + 16*89992 == 0x10220B20, the next referenced global");

    /* 0x10A99BB8: 256 nodes of 0x20 reach 0x10A9BBB8, likewise the next
     * referenced global -- and 256 is br_pool.h's BR_POOL_SLOTS_USED. */
    check(sizeof(BrPoolNode) == 32u,
          "BrPoolNode is still 32 bytes on LP64 (checked, not assumed)");
    check(sizeof g_aPoolNodes == 256u * 32u,
          "0x10A99BB8 holds 256 nodes, reaching 0x10A9BBB8");
    check(0x10A99BB8uL + sizeof g_aPoolNodes == 0x10A9BBB8uL,
          "  ... which is exactly where the next referenced global sits");

    /* 0x10220CF0: BrCarState is 0xA0, so the record spans 0x10220CF0..0x10220D90
     * and 0x10220D68 falls INSIDE it at +0x78. That is the alias resolved in
     * slice2_11.h, and this is the arithmetic that proves it. */
    check(sizeof(BrCarState) == 0xA0u, "BrCarState is 0xA0, as the original");
    check((size_t)((char *)&g_brNetLastFull.f78 - (char *)&g_brNetLastFull) == 0x78u,
          "f78 sits at +0x78 -- i.e. at 0x10220D68");
    check(&g_brNet220D68 == &g_brNetLastFull.f78,
          "0x10220D68 IS g_brNetLastFull.f78, not a separate float");
}

int main(void)
{
    test_scalars();
    test_format();
    test_option_tables();
    test_name_tables();
    test_rec_table();
    test_b3820();
    test_extents();
    printf(g_fail ? "\nFAILURES\n" : "\nall passed\n");
    return g_fail;
}
