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

void FUN_10062640(int pDst, const float *pRec);          /* 0x10062640     */
void FUN_1006d530(void *pForces);                         /* 0x1006D530     */

#define CI(p, off)  (*(int   *)((char *)(p) + (off)))
#define CF(p, off)  (*(float *)((char *)(p) + (off)))
#define CB(p, off)  (*(unsigned char *)((char *)(p) + (off)))

/* Transcribed from the Glide bytes: every truncation is a plain (int) cast
 * (fld [esi+off]; call __ftol), the flag bit is read-modified-written in each
 * arm (VC5 hoists the load past the compare and sinks the store), and the
 * "last seen" test is a ?: whose two 1-arms stay separate. */
/* WHAT IT DOES: scatters a decoded ghost/replay record across a live car
 * record.  Most of the 40 floats copy straight across; five are truncated
 * to integers/bytes for HUD counters, two drive on/off fields (a view-
 * record fog bit and a forward/reverse sign), one only advances the car's
 * "last seen" clock when the new value is not behind it by more than a
 * tolerance, and the whole applied block is then mirrored into two shadow
 * copies before the wheel-force list is reset. */
/* @implements 0x10059A80 glide BrCarGhostApply_10059A80 */
void BrCarGhostApply_10059A80(int pCar, const float *pRec)
{
    int  block = pCar + 0x1dc;

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

    CI(pCar, 0x524) = (int)pRec[0x13];
    CI(pCar, 0x93c) = (int)pRec[0x14];
    CI(pCar, 0x730) = (int)pRec[0x15];
    CI(pCar, 0xb48) = (int)pRec[0x16];

    CB(pCar, 0x510) = (unsigned char)(int)pRec[0x17];
    CB(pCar, 0x928) = (unsigned char)(int)pRec[0x18];
    CB(pCar, 0x71c) = (unsigned char)(int)pRec[0x19];
    CB(pCar, 0xb34) = (unsigned char)(int)pRec[0x1a];
    CB(pCar, 0x36d) = (unsigned char)(int)pRec[0x1b];

    if (pRec[0x1c] != DAT_10077770)
        *(unsigned *)CI(pCar, 0x29c0) |= 0x40000u;
    else
        *(unsigned *)CI(pCar, 0x29c0) &= 0xfffbffffu;

    if (pRec[0x1d] != DAT_10077770)
        CF(pCar, 0xe68) = -1.0f;
    else
        CF(pCar, 0xe68) = 1.0f;

    {
        int advance = (CF(pCar, 0xff4) <= DAT_10077770) ? 1 : (CF(pCar, 0xff4) - DAT_10077778 > pRec[0x1e]);

        if (advance)
            CF(pCar, 0xff4) = pRec[0x1e];
    }

    CF(pCar, 0xe24) = pRec[0x1f];

    CB(pCar, 0x362) = (unsigned char)(int)pRec[0x20];
    CB(pCar, 0x363) = (unsigned char)(int)pRec[0x21];
    CB(pCar, 0x36c) = (unsigned char)(int)pRec[0x22];
    CB(pCar, 0x366) = (unsigned char)(int)pRec[0x23];
    CB(pCar, 0x367) = (unsigned char)(int)pRec[0x24];
    CB(pCar, 0x368) = (unsigned char)(int)pRec[0x25];
    CB(pCar, 0x369) = (unsigned char)(int)pRec[0x26];
    CB(pCar, 0x36a) = (unsigned char)(int)pRec[0x27];

    FUN_1006d530((void *)block);

    memcpy((void *)(pCar + 0x278), (const void *)block, 0x44);
    memcpy((void *)(pCar + 700),   (const void *)block, 0x44);
}

#endif /* BR_MATCHING_BUILD */
