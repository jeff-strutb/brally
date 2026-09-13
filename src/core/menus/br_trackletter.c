/* br_trackletter.c -- the track/car-class letter id on the player record
 * (0x10038A80).  Refiled from ghidra_batch.c 2026-09-13; matching arm only.
 */
#ifdef BR_MATCHING_BUILD


extern int DAT_10ac5a48;
extern int DAT_10ac5a4c;

/* WHAT IT DOES: maps the current track/car-class menu selection onto the
 * two-byte letter id stored on the player record. */
/* THE SLOT IS A SIGNED `short *`.  The "no letter" value is an int -1 stored
 * through it twice; with a SIGNED 16-bit destination VC5 keeps the CSE'd
 * constant full-width (`or edx, -1`, 3 B, hoisted above the first test) and
 * stores `dx`.  Through an `unsigned short *` the same store folds the
 * constant to 0xffff first and the register is `mov edx, 0xffff` (5 B) --
 * seven spellings of the -1 itself could not move it (2026-09-10); the
 * destination's signedness decides the constant's representation.
 * Byte-exact 2026-09-13. */
/* @implements 0x10038A80 glide BrMenuSetTrackLetter */
int BrMenuSetTrackLetter(int param_1)
{
    int sel;
    int none;
    short *slot;

    sel = DAT_10ac5a48;
    slot = (short *)(param_1 + 0x1e20c);
    none = -1;
    if (sel > 0) {
        switch (sel) {
        case 2:
            *slot = 0x6d;
            break;
        case 3:
            *slot = 0x6e;
            break;
        case 4:
            *slot = 0x6c;
            break;
        default:
            *slot = none;
            break;
        }
    }
    if (DAT_10ac5a48 == 0) {
        /* THE `- 1` IS IN THE ORIGINAL, not a simplification to undo: it
         * emits `dec eax` for the subtraction and then a `sub eax, 0` to open
         * the case chain at zero, which is the instruction our `case 1:`
         * spelling was missing.  Same behaviour either way. */
        switch ((DAT_10ac5a4c & 0xff) - 1) {
        case 0:
            *slot = 0x48;
            break;
        case 1:
            *slot = 0x4a;
            return 1;
        case 2:
            *slot = 0x4c;
            return 1;
        default:
            *slot = none;
            return 1;
        }
    }
    return 1;
}

#endif /* BR_MATCHING_BUILD */
