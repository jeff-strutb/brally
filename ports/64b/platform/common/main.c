/* main.c: the process. What BRally.exe and BRGlide.dll's start-up did, in
 * order: the CRT's static initialisers, DllMain, then RallyMain with the
 * command line. */
#include <stdlib.h>
#include <string.h>

#include "plat.h"

void br_data_lift(void);            /* build/portable/gen/br_data.c */
void br_data_initterm(void);
void plat_dx_init(void);            /* dx.c */
void plat_ear_init(void);           /* ear.c */
void plat_script_files(void);
int  BrDllMain(void *hinst, int reason, int reserved);
int  BrRallyMain(void *hinst, void *hprev, const char *cmdline, int show);

int main(int argc, char **argv)
{
    char cmd[1024] = "";
    int i, r;
    g_plat_log = getenv("BR_LOG") != NULL;
    host_init(argc, argv);
    plat_script_files();            /* script_game.c: save fixtures, before the game reads them */
    plat_dx_init();
    plat_ear_init();
    br_data_lift();
    br_data_initterm();
    BrDllMain(GetModuleHandleA(NULL), DLL_PROCESS_ATTACH, 0);
    for (i = 1; i < argc; i++) {
        if (i > 1)
            strncat(cmd, " ", sizeof cmd - strlen(cmd) - 1);
        strncat(cmd, argv[i], sizeof cmd - strlen(cmd) - 1);
    }
    r = BrRallyMain(GetModuleHandleA(NULL), NULL, cmd, SW_SHOWNORMAL);
    fprintf(stderr, "RallyMain returned %d\n", r);
    host_shutdown();
    return r;
}
