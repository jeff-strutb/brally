/* 0x100386B0 (d3d 0x1003F170) -- hand the edited name off and clear both
 * buffers.
 *
 * WHAT IT DOES: copy the +0x2B5C item's label into the shared name buffer,
 * pass it to the commit helper along with the current slot, then blank the
 * shared buffer and the item label with the empty string. Returns 1.
 *
 * The port body in slice2_23.c reaches every global through a
 * BrUiGlobals* it takes as a second parameter; the original is cdecl with
 * ONE argument and direct global addresses, which is the whole 129-diff
 * gap (six extra `mov r,[r+disp]`, an extra push and an extra call). Same
 * split as the 0x10038650 sibling next door.
 *
 * All three copies are the inline strcpy form, and the original interleaves
 * the helper's argument pushes into the first copy's expansion -- that
 * falls out of writing the call as the next statement, since the arguments
 * are plain loads.
 */
#ifdef BR_MATCHING_BUILD

/* The original binary is /MD: CRT calls resolve through the import table. */
#define _CRTIMP __declspec(dllimport)
#include <string.h>

extern char g_szBrName4DB0[];       /* 0x10AC4DB0 */
extern int  g_brSlot4098;           /* 0x10AC4098 */
extern int  g_brOwner5BC72C;        /* 0x105BC72C */
extern char g_szBrEmpty396F08[];    /* 0x10396F08 */

extern int BrFn1003D210_glide(int a, int b, int c);     /* 0x100368A0 */

/* @implements 0x100386B0 glide BrUiFn1003F170 */
int BrUiFn1003F170(int param_1)
{
    char *pText = (char *)(param_1 + 0x2b65);

    strcpy(g_szBrName4DB0, pText);

    BrFn1003D210_glide(g_brOwner5BC72C, g_brSlot4098, 0);

    strcpy(g_szBrName4DB0, g_szBrEmpty396F08);
    strcpy(pText, g_szBrEmpty396F08);

    return 1;
}

#endif /* BR_MATCHING_BUILD */
