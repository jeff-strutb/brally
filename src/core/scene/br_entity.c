/* br_entity.c -- the world's objects and where they sit.
 *
 * RESPONSIBILITY: what is in the world and where -- setting an entity up,
 * linking it to its record in the parallel table, and moving it.
 *
 * Moved here out of the address batches under src/core/; the bodies are the
 * text that was matched there, unchanged.
 */
#include "slice1_05.h"

/* 0x10035FE0 */
/* WHAT IT DOES: prepares one entity for use -- clears its state, works out its
 * own number from where it sits in the array, and links it to the matching
 * record in the parallel table so the two can find each other later. */
BrEnt    g_aBrEnts[16];      /* 0x106ED708 */
BrEntRec g_aBrEntRecs[16];   /* 0x106ED630 */

/* @implements 0x10035FE0 d3d BrEntInit */
/* @n64 0x80255B54 located */
void __fastcall BrEntInit(BrEnt *pEnt)
{
    long idx;

    /* Written in this order by the original: +0x30, +0x2C, +0x44. */
    pEnt->f30 = 0;
    pEnt->f2C = 0;
    pEnt->f44 = 0;

    idx = (long)(pEnt - g_aBrEnts);
    pEnt->f154 = (int32_t)idx;
    pEnt->f158 = &g_aBrEntRecs[idx];
}
