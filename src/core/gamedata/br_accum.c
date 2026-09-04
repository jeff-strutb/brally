/* br_accum.c -- gamedata: the clamped running-total table.
 *
 * One entry of a fixed float table, bumped by a clamped amount and held
 * under a ceiling. Filed out of slice2_19.c; the table itself and the two
 * limit constants stay with the constant table in that file and are reached
 * through slice2_19.h.
 */
#ifdef BR_MATCHING_BUILD
/* The original binary is /MD: CRT calls resolve through the import table. */
#define _CRTIMP __declspec(dllimport)
#endif
#include "slice2_19.h"

/* 0x100347BA */
/* WHAT IT DOES: adds to one entry of a running total, refusing to add more
 * than a fixed amount at once and refusing to let the total exceed a fixed
 * ceiling -- the shape of a damage or wear meter, though what this particular
 * table records was not established. */
/* @implements 0x100347BA d3d BrAccumAddClamp */
/* @n64 0x8021BE28 located */
/* NO TABLE POINTER.  The original indexes a FIXED-ADDRESS array --
 * `fld dword ptr [eax*4 + 0x106EC4F8]`, an absolute base with no register --
 * and reads only two arguments, i at [ebp+8] and amt at [ebp+0xc].  The
 * `float *aTable` first parameter never existed; see tools/screen_absglobals.py
 * for the rest of this class.  The element count is not established: nothing
 * in the tree calls this yet, so the array stays incomplete rather than
 * carrying a guessed bound. */
void BrAccumAddClamp(int i, float amt)
{
    if (amt > g_BrK08F520)
        amt = 2.5f;

    g_Br6C5468[i] = g_Br6C5468[i] + amt;

    if (g_Br6C5468[i] > g_BrK08F524)
        g_Br6C5468[i] = 5.0f;
}
