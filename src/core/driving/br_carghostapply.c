/* br_carghostapply.c -- driving: applies a decoded ghost/replay record to
 * a live car.
 *
 * RESPONSIBILITY: driving/ -- turn per-frame inputs into car motion.
 *
 * One function, 0x10059A80: the consumer of a 40-float decoded record (the
 * same shape 0x10007AA0/br_fix.c's BrFixDecodeRecord_10007AA0 produces --
 * box dimensions land at the same +0x1DC/1E0/1E4 br_carphys.c's
 * BrCarPhysApplyCarData already pins).  Scatters the record across the car,
 * truncates four float fields to eight/four/one-byte HUD counters, flips a
 * fog-ish bit on the car's view record, sets a forward/reverse sign field,
 * advances a "last seen" clock only when the new value is ahead, mirrors
 * the whole applied block into two shadow copies (+0x278 and +700), and
 * resets the wheel-force accumulator (0x1006D530) once.
 */
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include <stdint.h>
#include <string.h>

#ifdef BR_MATCHING_BUILD

extern float DAT_10077770;    /* the "unset" sentinel both float tests use */
extern float DAT_10077778;    /* the "last seen" clock's forward tolerance */

int  ftol(void);                                        /* real cdecl     */
void FUN_10062640(int pDst, const float *pRec);          /* 0x10062640     */
void FUN_1006d530(void *pForces);                         /* 0x1006D530     */

#define CI(p, off)  (*(int   *)((char *)(p) + (off)))
#define CF(p, off)  (*(float *)((char *)(p) + (off)))
#define CB(p, off)  (*(unsigned char *)((char *)(p) + (off)))

/* WHAT IT DOES: scatters a decoded ghost/replay record across a live car
 * record.  Most of the 40 floats copy straight across; five are truncated
 * to integers/bytes for HUD counters, two drive on/off fields (a view-
 * record fog bit and a forward/reverse sign), one only advances the car's
 * "last seen" clock when the new value is not behind it by more than a
 * tolerance, and the whole applied block is then mirrored into two shadow
 * copies before the wheel-force list is reset. */
/* T2, not yet byte-exact: 593/673 B (80 B short), REGNORM 5+23. Switching
 * the two tail block-copies from a hand loop to memcpy() closed most of the
 * gap (40 -> 28 register-blind, FIRSTDIV +0x1 -> +0xD2 -- CLAUDE.md's own
 * "manual copy loops are memcpy" idiom, docs/VC5-IDIOMS.md ~line 7197).
 * Residue: 17 `fld [R+I]` the original has that this recomp does not, past
 * +0xD2 -- one of the twenty scattered field copies is likely staying on
 * the stack ([esp+S]) here instead of through a kept base register; not
 * isolated further this session. */
/* @implements 0x10059A80 glide BrCarGhostApply_10059A80 */
void BrCarGhostApply_10059A80(int pCar, const float *pRec)
{
    int  block = pCar + 0x1dc;
    unsigned uVal;

    CF(pCar, 500) = pRec[0];
    CF(pCar, 0x1f8) = pRec[1];
    CF(pCar, 0x1fc) = pRec[2];
    CF(pCar, 0x200) = pRec[3];
    CF(pCar, 0x1dc) = pRec[4];
    CF(pCar, 0x1e0) = pRec[5];
    CF(pCar, 0x1e4) = pRec[6];

    FUN_10062640(pCar + 0x220, pRec);

    CF(pCar, 0x1e8) = pRec[7];
    CF(pCar, 0x1ec) = pRec[8];
    CF(pCar, 0x1f0) = pRec[9];
    CF(pCar, 0x204) = pRec[10];
    CF(pCar, 0x208) = pRec[0xb];
    CF(pCar, 0x20c) = pRec[0xc];
    CF(pCar, 0x338) = pRec[0xd];
    CF(pCar, 0x73c) = pRec[0xe];
    CF(pCar, 0xb54) = pRec[0xe];
    CF(pCar, 0x544) = pRec[0xf];
    CF(pCar, 0x95c) = pRec[0x10];
    CF(pCar, 0x750) = pRec[0x11];
    CF(pCar, 0xb68) = pRec[0x12];

    CI(pCar, 0x524) = ftol();
    CI(pCar, 0x93c) = ftol();
    CI(pCar, 0x730) = ftol();
    CI(pCar, 0xb48) = ftol();

    CB(pCar, 0x510) = (unsigned char)ftol();
    CB(pCar, 0x928) = (unsigned char)ftol();
    CB(pCar, 0x71c) = (unsigned char)ftol();
    CB(pCar, 0xb34) = (unsigned char)ftol();
    CB(pCar, 0x36d) = (unsigned char)ftol();

    uVal = *(unsigned *)CI(pCar, 0x29c0);
    if (pRec[0x1c] == DAT_10077770)
        uVal &= 0xfffbffffu;
    else
        uVal |= 0x40000u;
    *(unsigned *)CI(pCar, 0x29c0) = uVal;

    if (pRec[0x1d] == DAT_10077770)
        CI(pCar, 0xe68) = 0x3f800000;
    else
        CI(pCar, 0xe68) = (int)0xbf800000;

    {
        int advance;

        if (DAT_10077770 < CF(pCar, 0xff4)) {
            if (CF(pCar, 0xff4) - DAT_10077778 <= pRec[0x1e])
                advance = 0;
            else
                advance = 1;
        } else {
            advance = 1;
        }
        if (advance)
            CF(pCar, 0xff4) = pRec[0x1e];
    }

    CF(pCar, 0xe24) = pRec[0x1f];

    CB(pCar, 0x362) = (unsigned char)ftol();
    CB(pCar, 0x363) = (unsigned char)ftol();
    CB(pCar, 0x36c) = (unsigned char)ftol();
    CB(pCar, 0x366) = (unsigned char)ftol();
    CB(pCar, 0x367) = (unsigned char)ftol();
    CB(pCar, 0x368) = (unsigned char)ftol();
    CB(pCar, 0x369) = (unsigned char)ftol();
    CB(pCar, 0x36a) = (unsigned char)ftol();

    FUN_1006d530((void *)block);

    memcpy((void *)(pCar + 0x278), (const void *)block, 0x44);
    memcpy((void *)(pCar + 700),   (const void *)block, 0x44);
}

#endif /* BR_MATCHING_BUILD */
