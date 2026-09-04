/* Matching body — 0x100060B0 */
#ifdef BR_MATCHING_BUILD

/* The original binary is /MD: CRT calls resolve through the import table. */
#define _CRTIMP __declspec(dllimport)
#include <windows.h>

extern int DAT_10226a5c;    /* the stack's mutex handle */
extern int DAT_1021c904;    /* top index; negative = empty */
extern int DAT_1021c8c0[];  /* the stack itself */

/* One shared unlock/return tail in source; VC5 duplicates it into both
 * arms (each gets its own ReleaseMutex + ret). */
/* WHAT IT DOES: pop the next free network slot index off a shared stack,
 * returning -1 when none are left. Locked, because the receive thread also
 * takes slots. */
/* @implements 0x100060B0 glide BrNetStackPop */
int BrNetStackPop(void)
{
    int v;
    int n;

    WaitForSingleObject((HANDLE)DAT_10226a5c, 0xffffffff);
    n = DAT_1021c904;
    if (n >= 0) {
        v = DAT_1021c8c0[n];
        DAT_1021c904 = n - 1;
    } else {
        v = -1;
    }
    ReleaseMutex((HANDLE)DAT_10226a5c);
    return v;
}

#endif /* BR_MATCHING_BUILD */
