/* Matching body — 0x100062B0 */
#ifdef BR_MATCHING_BUILD

/* The original binary is /MD: CRT calls resolve through the import table. */
#define _CRTIMP __declspec(dllimport)
#include <windows.h>

extern int DAT_1021ce58;    /* slot[0].hMutex; slot stride 0x978 (0x25E ints) */
extern int BrNetSlotGetF02C(int i);     /* 0x10004D80, re-enters the mutex */

/* WHAT IT DOES: read a slot's flag word and turn it into a small non-
 * negative number: keep the low six bits, subtract four, and clamp at zero.
 * Used where the caller wants a count or level rather than the raw flags. */
/* @implements 0x100062B0 glide BrNetSlotGetF02CBiased */
int BrNetSlotGetF02CBiased(int i)
{
    int v;

    WaitForSingleObject((HANDLE)(&DAT_1021ce58)[i * 0x25e], 0xffffffff);
    v = (BrNetSlotGetF02C(i) & 0x3f) - 4;
    ReleaseMutex((HANDLE)(&DAT_1021ce58)[i * 0x25e]);
    return (v > 0) ? v : 0;
}

#endif /* BR_MATCHING_BUILD */
