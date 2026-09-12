/* WHAT IT DOES: brings the front menu screen up. The first time through it
 * builds the screen -- creating the underlying dialog (a failure puts up
 * message 0xAB and exits the game), running the screen's own enter hook and
 * marking it built. Every time, it then reacts to how the game got here:
 * back from a race it clears the race scratch, finishing race setup it
 * re-opens the main page, and the "just finished" and "quit to menu" paths
 * each open their own follow-up screen. Finally the one-shot post-join
 * screen is opened if the network flag asks for it. */
/* @implements 0x10053D20 glide BrUiFrontActivate
 * @cpp_kind method
 * @cpp_symbol ?Activate@Phase53D20@@QAEHXZ
 *
 * Thiscall, no args (`ret`), 305 B. The phase object's +4 is a CDECL
 * function-pointer field (push esi; call [esi+4]), not a vtable slot.
 * The dialog Create is a true thiscall with one stack arg
 * (`mov ecx,[0x10AC5C58]; push hwnd`), which is what routes this body to
 * the C++ lane. ebx/ebp are the 0/1 constant webs.
 */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#endif
#include <stdlib.h>
#include <string.h>

class BrDlg53 {
public:
    int Create(void *hwnd);          /* 0x10059280 */
};

class Phase53D20;
extern "C" {
extern BrDlg53 *DAT_10ac5c58;        /* the front dialog object          */
extern void *g_brP680584;            /* the game window, 0x105BC72C      */
extern int   g_a220B20;              /* 0x1021C650: how we got here      */
extern int   DAT_100a9360;           /* the pending phase request        */
extern int   DAT_105bc760;           /* race-begin stage                 */
extern int   DAT_10ac5bf4;
extern int   DAT_10ac5bd0;
extern int   DAT_10ac5bfc;
extern int   DAT_10ac46a0[8];        /* race scratch, zeroed on return   */
extern unsigned char DAT_10ac5a4d;
extern int   DAT_10ac4090;
extern int   DAT_10ac5da8;
void        FUN_100379b0(void);
void        FUN_100583c0(void);
const char *BrStrGet(int id);        /* 0x1006D280 */
void        BrMsgBoxAA(void *, int, const char *);   /* 0x100590A0 */
int         FUN_10058680(void);
void        BrPhaseActivate_10045EA0(void);          /* glide 0x1003F340 */
void        BrMenuAutoSaveName(void);                /* glide 0x1003B0B0 */
void        BrPhaseActivate_100447D0(void);          /* glide 0x1003DD20 */
void        BrMenuLeaveTo2(void);                    /* glide 0x1003AE90 */
void        BrOpt41A0(void);                         /* glide 0x1003D6F0 */
}

class Phase53D20 {
public:
    int   f00;
    int (__cdecl *pfnEnter)(Phase53D20 *);   /* +0x04 */
    int   f08;
    int   fBuilt;                            /* +0x0C */
    char  pad10[0x68 - 0x10];
    int   f68;                               /* +0x68 */

    int Activate();
};

typedef char chk_pfn[(unsigned)&((Phase53D20 *)0)->pfnEnter == 4 ? 1 : -1];
typedef char chk_blt[(unsigned)&((Phase53D20 *)0)->fBuilt == 0xC ? 1 : -1];
typedef char chk_f68[(unsigned)&((Phase53D20 *)0)->f68 == 0x68 ? 1 : -1];

int Phase53D20::Activate()
{
    int m;

    FUN_100379b0();
    if (fBuilt == 0) {
        FUN_100583c0();
        if (DAT_10ac5c58->Create(g_brP680584) == 0) {
            BrMsgBoxAA(g_brP680584, 0, BrStrGet(0xAB));
            exit(1);
        }
        pfnEnter(this);
        fBuilt = 1;
        f68    = 1;
    }

    m = g_a220B20;
    if (m == 5) {
        DAT_10ac5bfc = 0;
        memset(DAT_10ac46a0, 0, 0x20);
        DAT_10ac5a4d = 0;
    }

    if (DAT_100a9360 == 4 && DAT_105bc760 == 2 && DAT_10ac5bf4 != 0) {
        BrPhaseActivate_10045EA0();
        DAT_100a9360 = 0;
        m = g_a220B20;
    }

    if (DAT_100a9360 == 0 && (m == 0 || m == 5)) {
        if (FUN_10058680() == 0) {
            return 0;
        }
        BrPhaseActivate_10045EA0();
        BrMenuAutoSaveName();
        DAT_10ac5bf4 = 1;
    } else if (DAT_100a9360 == 6) {
        DAT_10ac5bd0 = 1;
        BrPhaseActivate_100447D0();
        DAT_10ac5bd0 = 0;
    } else if (DAT_100a9360 == 2) {
        DAT_10ac5bd0 = 1;
        BrMenuLeaveTo2();
        DAT_10ac5bd0 = 0;
    }

    if (DAT_10ac4090 != 0 && DAT_10ac5da8 == 0) {
        DAT_10ac5da8 = 1;
        BrOpt41A0();
    }
    return 1;
}
