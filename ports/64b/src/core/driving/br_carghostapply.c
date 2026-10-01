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
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include <stdint.h>
#include "slice3_41.h"
#include <string.h>


/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* the "unset" sentinel both float tests use */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* the "last seen" clock's forward tolerance */

/* FUN_10062640: prototype in br_funcs.h */
/* FUN_1006d530: prototype in br_funcs.h */


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
void BrCarGhostApply_10059A80(BrDriverCar *pCar, const float *pRec)
{
    char *block = ((void *)&pCar->aBody[0].rb.st.pos.x);

    ((pCar->aBody[0].rb.st.quat.f00)) = pRec[0];
    ((pCar->aBody[0].rb.st.quat.f04)) = pRec[1];
    ((pCar->aBody[0].rb.st.quat.f08)) = pRec[2];
    ((pCar->aBody[0].rb.st.quat.f0C)) = pRec[3];
    ((pCar->aBody[0].rb.st.pos.x)) = pRec[4];
    ((pCar->aBody[0].rb.st.pos.y)) = pRec[5];
    ((pCar->aBody[0].rb.st.pos.z)) = pRec[6];

    FUN_10062640(((void *)&pCar->aBody[0].rb.m.m[0]), pRec);

    ((pCar->aBody[0].rb.st.vel.x)) = pRec[7];
    ((pCar->aBody[0].rb.st.vel.y)) = pRec[8];
    ((pCar->aBody[0].rb.st.vel.z)) = pRec[9];
    ((pCar->aBody[0].rb.st.angVel.x)) = pRec[10];
    ((pCar->aBody[0].rb.st.angVel.y)) = pRec[0xb];
    ((pCar->aBody[0].rb.st.angVel.z)) = pRec[0xc];
    ((pCar->aBody[0].rb.f1D4)) = pRec[0xd];
    ((pCar->aBody[2].rb.f1C0)) = pRec[0xe];
    ((pCar->aBody[4].rb.f1C0)) = pRec[0xe];
    ((pCar->aBody[1].rb.f1D4)) = pRec[0xf];
    ((pCar->aBody[3].rb.f1D4)) = pRec[0x10];
    ((pCar->aBody[2].rb.f1D4)) = pRec[0x11];
    ((pCar->aBody[4].rb.f1D4)) = pRec[0x12];

    ((*(int *)&pCar->aBody[1].rb.f1B4)) = (int)pRec[0x13];
    ((*(int *)&pCar->aBody[3].rb.f1B4)) = (int)pRec[0x14];
    ((*(int *)&pCar->aBody[2].rb.f1B4)) = (int)pRec[0x15];
    ((*(int *)&pCar->aBody[4].rb.f1B4)) = (int)pRec[0x16];

    ((pCar->aBody[1].rb.f01A0)) = (unsigned char)(int)pRec[0x17];
    ((pCar->aBody[3].rb.f01A0)) = (unsigned char)(int)pRec[0x18];
    ((pCar->aBody[2].rb.f01A0)) = (unsigned char)(int)pRec[0x19];
    ((pCar->aBody[4].rb.f01A0)) = (unsigned char)(int)pRec[0x1a];
    ((*(unsigned char *)&pCar->aBody[0].f0209)) = (unsigned char)(int)pRec[0x1b];

    if (pRec[0x1c] != DAT_10077770)
        *(unsigned *)((((char *)pCar->pCtl))) |= 0x40000u;
    else
        *(unsigned *)((((char *)pCar->pCtl))) &= 0xfffbffffu;

    if (pRec[0x1d] != DAT_10077770)
        ((pCar->fE68)) = -1.0f;
    else
        ((pCar->fE68)) = 1.0f;

    {
        int advance = (((pCar->fFF4)) <= DAT_10077770) ? 1 : (((pCar->fFF4)) - DAT_10077778 > pRec[0x1e]);

        if (advance)
            ((pCar->fFF4)) = pRec[0x1e];
    }

    ((pCar->f0E24)) = pRec[0x1f];

    ((*(unsigned char *)&pCar->aBody[0].f01FE)) = (unsigned char)(int)pRec[0x20];
    ((pCar->aBody[0].f01FF)) = (unsigned char)(int)pRec[0x21];
    ((*(unsigned char *)&pCar->aBody[0].f0208)) = (unsigned char)(int)pRec[0x22];
    ((*(unsigned char *)&pCar->aBody[0].f0202)) = (unsigned char)(int)pRec[0x23];
    ((*(unsigned char *)&pCar->aBody[0].f0203)) = (unsigned char)(int)pRec[0x24];
    ((*(unsigned char *)&pCar->aBody[0].f0204)) = (unsigned char)(int)pRec[0x25];
    ((*(unsigned char *)&pCar->aBody[0].f0205)) = (unsigned char)(int)pRec[0x26];
    ((*(unsigned char *)&pCar->aBody[0].f0206)) = (unsigned char)(int)pRec[0x27];

    FUN_1006d530((void *)block);

    memcpy(&pCar->aBody[0].rb.st1.pos.x, (const void *)block, 0x44);
    memcpy(&pCar->aBody[0].rb.st2.pos.x,   (const void *)block, 0x44);
}

