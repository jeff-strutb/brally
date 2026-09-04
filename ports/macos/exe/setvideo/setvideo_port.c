/* setvideo — macOS port driver for SetVideo.exe.
 *
 * SetVideo.exe is byte-exact (42/42 functions, 7,228/7,228 B of .text). Of
 * those 42, twenty-nine are plain C — the .vdb/.ini reader, the section
 * cursor, the checked-file helpers — and are compiled here VERBATIM from
 * src/exe/setvideo/ with BR_MATCHING_BUILD defined. The original logic runs;
 * only the shell around it is new.
 *
 * The thirteen that are not built here:
 *   0x00401B30 GetInstallDir   — reads HKLM; replaced below by --dir/cwd
 *   0x00401C10 0x00401C70 0x00401DC0 0x00401EC0 0x00401F00 0x00402030
 *   0x00402160 0x00402260     — the five wizard dialog procedures
 *   0x00402480 WinMain        — the dialog driver; main() below replaces it
 *   0x00403140 0x00405940 0x00406C85 — MSVC CRT hooks with no meaning here
 *
 * The wizard's five dialog templates live in the original binary's .rsrc and
 * have never been extracted into this repo, so there is no UI to port. This
 * driver exposes the same choices as command-line options and reproduces
 * WinMain's read and write paths exactly, statement for statement.
 *
 * NOT byte-matched. No @implements tags. Invisible to the match tooling.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "setvideo_port.h"

/* ------------------------------------------------------------------ */
/* Globals the matched translation units link against.                */
/* Sizes follow the original .data layout where it is known:          */
/* gInstallDir 0x40E090, gIniPath 0x40E198, gLineBuf 0x40E598.        */
/* ------------------------------------------------------------------ */

int   gChkVerbose      = 0;
char  gCommentChar     = '#';       /* 0x40308C in the original */
int   gIncludeDepth    = 0;
void *gIncludeStack[32];
char  gLineBuf[0x400];

char  s_rt[]               = "rt";
char  s_ReadListOpenErr[]  = "ReadList: error opening file %s.\n";
char  s_PRJ[]              = "PRJ";
char  s_rgiObj[]           = "rgiObj";
char  s_rgszObj[]          = "rgszObj";
char  s_szObji[]           = "ReadList():szObji";

char  gInstallDir[0x108];
char  gIniPath[0x400];
INI  *gINI            = 0;
int   gSectionCount   = 0;
Sel   gSel            = { -1, -1, 0 };
int   gPlusD          = 0;
int   gD3DAlphaCompare   = 0;
int   gD3DDrawCarShadow  = 0;
int   gD3DInvSrcAlpha    = 0;
int   gD3DClearZBuffer   = 0;

/* DlgProcComboA and DlgProcComboB call ComboGetItemData; 0x00401EC0 is
 * defined as ComboGetCurText. Same function, two names in the tree. */
int ComboGetItemData(void *hWnd)
{
    return ComboGetCurText(hWnd);
}

/* CHK_FReadOpen's error path calls this with the FILE* first (cdecl). */
int fputs_fp(FILE *fp, char *s)
{
    if (fp == 0)
        return -1;
    return fputs(s, fp);
}

/* The CHK_* helpers trace through this when gChkVerbose is set. */
void OutputDebugStringA(const char *s)
{
    fputs(s, stderr);
}

/* ------------------------------------------------------------------ */
/* Port-side replacements                                             */
/* ------------------------------------------------------------------ */

/* Replaces 0x00401B30, which reads
 * HKLM\SOFTWARE\SouthPeak Interactive\Boss Rally\Directory and falls back to
 * "c:\". There is no registry here, so the directory comes from --dir and
 * defaults to the working directory. The trailing-separator fixup is the
 * same idea, with '/' for '\\'.
 *
 * WinMain calls this with no arguments, exactly as the original did, so the
 * chosen directory is parked in a global first. */
static const char *gPortDir = ".";

void GetInstallDir(void)
{
    const char *dir = gPortDir;

    if (dir == 0 || dir[0] == 0)
        dir = ".";
    if (strlen(dir) >= sizeof(gInstallDir) - 2) {
        fprintf(stderr, "setvideo: --dir path is too long.\n");
        exit(1);
    }
    strcpy(gInstallDir, dir);
    if (gInstallDir[strlen(gInstallDir) - 1] != '/')
        strcat(gInstallDir, "/");
}

/* WinMain's ini pre-read: pick up the four D3D settings the symptoms page
 * edits, so writing them back does not lose the user's existing answers. */
static void ReadExistingD3DFlags(void)
{
    char     line[256];
    CHKFile *fp;

    if (CHK_FileExists(gIniPath) == 0)
        return;

    fp = CHK_FReadOpen(gIniPath);
    while (CHK_FGets(line, 0x100, fp) != 0) {
        if (strncmp(line, "D3DDrawCarShadow=", 0x11) == 0)
            gD3DDrawCarShadow = atoi(line + 0x11);
        else if (strncmp(line, "D3DAlphaCompare=", 0x10) == 0)
            gD3DAlphaCompare = atoi(line + 0x10);
        else if (strncmp(line, "D3DClearZBuffer=", 0x10) == 0)
            gD3DClearZBuffer = atoi(line + 0x10);
        else if (strncmp(line, "D3DInvSrcAlpha=", 0xf) == 0)
            gD3DInvSrcAlpha = atoi(line + 0xf);
    }
    CHK_FClose(fp);
}

/* WinMain's pre-selection walk: find the vdb section the existing
 * BossRally.ini names in [Video] Card=, and remember its ordinal and which
 * wizard page (vendor / chipset) would have chosen it. */
static void PreselectFromIni(void)
{
    INI     *pini2;
    Section *psec;
    Section *pwalk;
    char    *card;
    char    *name;
    int      i;

    if (CHK_FileExists(gIniPath) == 0)
        return;

    pini2 = ReadINI(gIniPath);
    psec  = SetSubstituteDir(pini2, "[Video]");
    card  = GetIniValue(psec, "Card");
    if (card != 0) {
        pwalk = FindFirstSection(gINI);
        for (i = 0; i < gSectionCount; i++) {
            name = GetObj(pwalk);
            if (strcmp(name, card) == 0) {
                gSel.index = i;
                gSel.saved = i;
                if (strncmp(name, "[c:", 3) == 0)
                    gSel.method = 3;
                else if (strncmp(name, "[v:", 3) == 0)
                    gSel.method = 2;
                break;
            }
            pwalk = FindNextSection(pwalk);
        }
    }
    CHK_FreeMemory(psec);
    FreeINI(pini2);
}

/* ------------------------------------------------------------------ */
/* WinMain's two write paths                                          */
/* ------------------------------------------------------------------ */

/* WinMain case 3/4 (vendor and chipset pages write identically): the chosen
 * section's name as Card=, then every line of that section — after following
 * any Use= redirection — copied out verbatim. */
static void WriteCard(char *name)
{
    CHKFile *fp;
    Section *pwalk;
    INI     *bound;
    char    *s;

    fp = CHK_FWriteOpen(gIniPath, "wt");
    CHK_FPutS("[Video]", fp);
    CHK_FPutS("\n", fp);
    CHK_FPutS("Card", fp);
    CHK_FPutS("=", fp);
    CHK_FPutS(name, fp);
    CHK_FPutS("\n", fp);
    pwalk = FollowUse(gINI, name);
    bound = BindSection(pwalk);
    if (bound != 0) {
        for (s = NextObj(bound); s != 0; s = NextObj(bound)) {
            CHK_FPutS(s, fp);
            CHK_FPutS("\n", fp);
        }
    }
    CHK_FreeMemory(pwalk);
    CHK_FClose(fp);
}

/* WinMain case 2: the symptoms page does not pick a card at all, it writes a
 * fixed Direct3D profile carrying the four answered flags. */
static void WriteSymptoms(void)
{
    CHKFile *fp;
    char     buf[0x400];

    fp = CHK_FWriteOpen(gIniPath, "wt");
    CHK_FPutS("[Video]", fp);
    CHK_FPutS("\n", fp);
    CHK_FPutS("Card", fp);
    CHK_FPutS("=", fp);
    CHK_FPutS("[Set via symptoms (use Direct3D)]", fp);
    CHK_FPutS("\n", fp);
    CHK_FPutS("Driver=D3D\n", fp);
    sprintf(buf, "D3DAlphaCompare=%d\n", gD3DAlphaCompare);
    CHK_FPutS(buf, fp);
    CHK_FPutS("D3DAlwaysSquareTextures=0\n", fp);
    sprintf(buf, "D3DClearZBuffer=%d\n", gD3DClearZBuffer);
    CHK_FPutS(buf, fp);
    sprintf(buf, "D3DDrawCarShadow=%d\n", gD3DDrawCarShadow);
    CHK_FPutS(buf, fp);
    CHK_FPutS("D3DWaitCanFlip=0\n", fp);
    CHK_FPutS("D3DWaitFlipDone=0\n", fp);
    sprintf(buf, "D3DInvSrcAlpha=%d\n", gD3DInvSrcAlpha);
    CHK_FPutS(buf, fp);
    CHK_FClose(fp);
}

/* ------------------------------------------------------------------ */
/* Listing — what FillComboA/FillComboB put in the drop-downs         */
/* ------------------------------------------------------------------ */

/* prefix == 0 lists every section; "[v:" and "[c:" reproduce the vendor and
 * chipset drop-downs. Returns how many were shown. */
static int ListSections(const char *prefix)
{
    Section *psec;
    char    *name;
    int      i;
    int      shown = 0;

    psec = FindFirstSection(gINI);
    for (i = 0; i < gSectionCount; i++) {
        name = GetObj(psec);
        if (name == 0)
            break;
        if (prefix == 0 || strncmp(name, prefix, 3) == 0) {
            printf("%4d  %s%s\n", i, name,
                   i == gSel.index ? "   <- current" : "");
            shown++;
        }
        psec = FindNextSection(psec);
    }
    CHK_FreeMemory(psec);
    return shown;
}

/* Resolve a name to its vdb ordinal. Accepts the full bracketed section name
 * or the bare text after a "[v:" / "[c:" prefix, which is what the original
 * drop-downs displayed. */
static int IndexOfSection(const char *want)
{
    Section *psec;
    char    *name;
    int      i;
    int      found = -1;
    size_t   wlen  = strlen(want);

    psec = FindFirstSection(gINI);
    for (i = 0; i < gSectionCount; i++) {
        name = GetObj(psec);
        if (name == 0)
            break;
        if (strcmp(name, want) == 0) {
            found = i;
            break;
        }
        if (name[0] == '[' && name[1] != 0 && name[2] == ':' &&
            strncmp(name + 3, want, wlen) == 0 &&
            (name[3 + wlen] == ']' || name[3 + wlen] == 0)) {
            found = i;
            break;
        }
        psec = FindNextSection(psec);
    }
    CHK_FreeMemory(psec);
    return found;
}

static void ShowCurrent(void)
{
    INI     *pini2;
    Section *psec;
    char    *card;
    char    *line;
    INI     *bound;

    if (CHK_FileExists(gIniPath) == 0) {
        printf("%s: not present — no card selected yet.\n", gIniPath);
        return;
    }
    pini2 = ReadINI(gIniPath);
    psec  = SetSubstituteDir(pini2, "[Video]");
    card  = GetIniValue(psec, "Card");
    printf("%s\n", gIniPath);
    printf("  Card = %s\n", card ? card : "(none)");
    CHK_FreeMemory(psec);

    psec  = SetSubstituteDir(pini2, "[Video]");
    bound = BindSection(psec);
    if (bound != 0) {
        for (line = NextObj(bound); line != 0; line = NextObj(bound))
            printf("  %s\n", line);
    }
    CHK_FreeMemory(psec);
    FreeINI(pini2);
}

/* ------------------------------------------------------------------ */

static void Usage(void)
{
    printf(
"setvideo — Boss Rally Display Wizard (macOS port of SetVideo.exe)\n"
"\n"
"  --dir <path>        directory holding BossRally.ini   (default: .)\n"
"  --vdb <path>        device database                   (default: <dir>/BossRally.vdb)\n"
"\n"
"  --list              list every device-database section\n"
"  --vendors           list the vendor sections   (the first drop-down)\n"
"  --chipsets          list the chipset sections  (the second drop-down)\n"
"  --show              show the current BossRally.ini selection\n"
"\n"
"  --set <name|index>  select a card and write BossRally.ini\n"
"  --symptoms          write the 'set via symptoms' Direct3D profile\n"
"      --alpha-compare <0|1>   --clear-zbuffer <0|1>\n"
"      --draw-car-shadow <0|1> --inv-src-alpha <0|1>\n"
"\n"
"  -v                  trace file operations (the original's gChkVerbose)\n"
"\n"
"The original read the install directory from the registry; this port uses\n"
"--dir, defaulting to the working directory.\n");
}

int main(int argc, char **argv)
{
    const char *dir      = ".";
    const char *vdb      = 0;
    const char *setwhat  = 0;
    int         do_list  = 0;
    int         do_vend  = 0;
    int         do_chip  = 0;
    int         do_show  = 0;
    int         do_sympt = 0;
    int         gui_plus_d = 0;
    int         i;
    char        vdbpath[0x400];
    char       *name;
    int         idx;

    for (i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (strcmp(a, "--dir") == 0 && i + 1 < argc)             dir = argv[++i];
        else if (strcmp(a, "--vdb") == 0 && i + 1 < argc)        vdb = argv[++i];
        else if (strcmp(a, "--set") == 0 && i + 1 < argc)        setwhat = argv[++i];
        else if (strcmp(a, "--list") == 0)                       do_list = 1;
        else if (strcmp(a, "--vendors") == 0)                    do_vend = 1;
        else if (strcmp(a, "--chipsets") == 0)                   do_chip = 1;
        else if (strcmp(a, "--show") == 0)                       do_show = 1;
        else if (strcmp(a, "--symptoms") == 0)                   do_sympt = 1;
        else if (strcmp(a, "--alpha-compare") == 0 && i + 1 < argc)
            gD3DAlphaCompare = atoi(argv[++i]);
        else if (strcmp(a, "--clear-zbuffer") == 0 && i + 1 < argc)
            gD3DClearZBuffer = atoi(argv[++i]);
        else if (strcmp(a, "--draw-car-shadow") == 0 && i + 1 < argc)
            gD3DDrawCarShadow = atoi(argv[++i]);
        else if (strcmp(a, "--inv-src-alpha") == 0 && i + 1 < argc)
            gD3DInvSrcAlpha = atoi(argv[++i]);
        else if (strcmp(a, "+d") == 0)                           gui_plus_d = 1;
        else if (strcmp(a, "-v") == 0)                           gChkVerbose = 1;
        else if (strcmp(a, "-h") == 0 || strcmp(a, "--help") == 0) {
            Usage();
            return 0;
        } else {
            fprintf(stderr, "setvideo: unknown option '%s'\n\n", a);
            Usage();
            return 2;
        }
    }

    /* No command: run the wizard, which is what the original did. WinMain
     * looks for BossRally.vdb in the working directory, so move there first
     * and let it take over from the top. */
    if (!do_list && !do_vend && !do_chip && !do_show && !do_sympt && !setwhat) {
        gPortDir = ".";
        if (strcmp(dir, ".") != 0 && chdir(dir) != 0) {
            fprintf(stderr, "setvideo: cannot enter %s\n", dir);
            return 1;
        }
        return BrRunWizard(gui_plus_d ? "+d" : "");
    }

    /* WinMain's opening: locate the install directory, build the ini path,
     * refuse to run without the device database. */
    gPortDir = dir;
    GetInstallDir();
    strcpy(gIniPath, gInstallDir);
    strcat(gIniPath, "BossRally.ini");

    if (vdb != 0) {
        snprintf(vdbpath, sizeof(vdbpath), "%s", vdb);
    } else {
        snprintf(vdbpath, sizeof(vdbpath), "%sBossRally.vdb", gInstallDir);
    }
    if (CHK_FileExists(vdbpath) == 0) {
        fprintf(stderr, "Error: file %s is missing.\n", vdbpath);
        return 1;
    }

    gINI          = ReadINI(vdbpath);
    gSectionCount = CountSections(gINI);
    gSel.index    = -1;
    gSel.saved    = -1;
    gSel.method   = 0;

    ReadExistingD3DFlags();
    PreselectFromIni();

    if (do_show)
        ShowCurrent();
    if (do_list) {
        printf("%d sections in %s:\n", gSectionCount, vdbpath);
        ListSections(0);
    }
    if (do_vend) {
        printf("Vendors:\n");
        if (ListSections("[v:") == 0)
            printf("  (none)\n");
    }
    if (do_chip) {
        printf("Chipsets:\n");
        if (ListSections("[c:") == 0)
            printf("  (none)\n");
    }

    if (do_sympt) {
        WriteSymptoms();
        printf("Wrote %s (symptoms profile).\n", gIniPath);
    } else if (setwhat != 0) {
        /* Card names in the retail database begin with digits ("3Dfx Voodoo
         * Rush …"), so only an argument that is ENTIRELY digits is an
         * ordinal. */
        for (i = 0; setwhat[i] >= '0' && setwhat[i] <= '9'; i++)
            ;
        if (i > 0 && setwhat[i] == 0)
            idx = atoi(setwhat);
        else
            idx = IndexOfSection(setwhat);
        if (idx < 0 || idx >= gSectionCount) {
            fprintf(stderr, "setvideo: no such card '%s'"
                            " (try --list)\n", setwhat);
            FreeINI(gINI);
            return 1;
        }
        gSel.index = idx;
        name = GetSectionNameByIndex(idx, gINI);
        WriteCard(name);
        printf("Wrote %s\n  Card = %s\n", gIniPath, name);
    }

    FreeINI(gINI);
    return 0;
}
