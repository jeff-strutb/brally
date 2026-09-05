/* br_timefmt.c -- racing.
 *
 * The lap / race time caption: a time in seconds in, "m:ss.hh" out.  The
 * port's bounds-checked twin (three arguments, snprintf) lives in
 * slice2_11.c; this is the original's two-argument sprintf form, byte-exact.
 */
#ifdef BR_MATCHING_BUILD
/* The original is /MD: sprintf goes through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include <stdio.h>

/* WHAT IT DOES: turns a time in seconds into the on-screen caption
 * "minutes:seconds.hundredths" -- e.g. 1:07.42 -- by first converting the
 * float to a whole number of hundredths (truncating, via __ftol) and then
 * splitting that integer three ways.  Negative times split with C's
 * truncating division, so they would print oddly; nothing feeds one.
 *
 * The split is spelled exactly as BrHudDrawTimeEntry (0x10014760) spells
 * it: a named quotient and a compound `-=` mul-back for each remainder is
 * what makes VC5 emit the magic-multiply divides and the lea/neg chains
 * instead of one `idiv` per pair (docs/VC5-IDIOMS.md, "div/mod-by-constant
 * pairs"). */
/* @implements 0x100023F0 glide BrTimeFormat */
void BrTimeFormat(char *psz, float t)
{
    int total = (int)(t * 100.0f);
    int whole = total / 100;
    int minutes;

    total  -= whole * 100;      /* total is now the hundredths */
    minutes = whole / 60;
    whole  -= minutes * 60;     /* whole is now the seconds */

    sprintf(psz, "%d:%02d.%02d", minutes, whole, total);
}
#endif /* BR_MATCHING_BUILD */
