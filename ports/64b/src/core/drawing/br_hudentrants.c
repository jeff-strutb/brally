/* br_hudentrants.c -- the multiplayer entrant list on the HUD (0x10014E00).
 *
 * Fresh transcription from build/ghidra_decomp/0x10014e00.c against the
 * original bytes, 2026-09-13.  Matching arm only.
 */

/* The original binary is /MD: CRT calls resolve through the import table. */
#define _CRTIMP __declspec(dllimport)
#include <stdio.h>
#include "slice3_41.h"

/* BrSetGlobal_ABB30: prototype in br_funcs.h */
/* BrSub_10019280: prototype in br_funcs.h */
/* BrNetSlotGetF02CBiased: prototype in br_funcs.h */
/* BrSub100714D0: prototype in br_funcs.h */
/* BrSub10071510: prototype in br_funcs.h */
/* BrNetSlotGetF974: prototype in br_funcs.h */
/* BrHudFormatGapString: prototype in br_funcs.h */
/* BrTextDraw: prototype in br_funcs.h */

/* 64-bit core: declared once, in br_globals.h or its struct's header */           /* list enabled */
/* 64-bit core: declared once, in br_globals.h or its struct's header */           /* net mode */
/* 64-bit core: declared once, in br_globals.h or its struct's header */           /* 0x100B2F04 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */           /* show ping column */
/* 64-bit core: declared once, in br_globals.h or its struct's header */         /* 0x105CCB88 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* "%%55%d." */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* "%%55%s"  */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* "%%y1%d." */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* "%%ry%d." */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* "%%11%d." */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* "%%11%s"  */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* WHAT IT DOES: draws the multiplayer standings list on the HUD -- one line
 * per car with its position number and name, the local car and the AI cars
 * in their own colours, a human opponent in the team colour taken from the
 * car record with the gap to the leader beside it, and, when the ping column
 * is on, the connection's round-trip time plus two quality marks derived
 * from the packet-loss counters. Cars flagged as spectators get the plain
 * grey line. */
/* Byte-exact 2026-09-13 (fresh, 4 fn.py compiles).  Three source facts
 * carried the last 7 bytes: the `+0xF08 != 0` arm and the ping-column arm
 * are the IF bodies (VC5 lays the if body inline and the else out of line,
 * which is where the original puts the plain "%%55" and no-ping arms);
 * `isLocal` is declared before `x` so the two land in the frame slots the
 * original spends (x at +8, the F02C flag at +0xC); and the loss counter
 * `v` is a signed int (`cmp eax,3; jge`).  Each arm carries its own
 * sprintf call -- VC5 cross-jumps the identical `call ebp` tails itself. */
/* @implements 0x10014E00 glide BrHudDrawEntrants */
void BrHudDrawEntrants(int *pScr, BrDriverCar *cars)
{
    int  yBase;
    int  i;
    int  isLocal;
    int  x;
    char buf[256];
    BrDriverCar *car;
    int  y;
    int  c1;
    int  c2;
    int  v;

    if (DAT_100bcc04 != 0) {
        BrSetGlobal_ABB30(0xf);
        BrSub_10019280();
        x     = pScr[0] + 0x10;
        yBase = pScr[1] + 0x1d;
        if ((*(int *)&g_brRaceNet) != 0 && (*(int *)&g_BrCarCount) != 0) {
            i = 0;
            if ((*(int *)&g_BrCarCount) > 0) {
                do {
                    car = cars + i;
                    if (cars[i].pfnControl != 0) {
                        isLocal = BrNetSlotGetF02CBiased(car->iNetPlayer);
                        if (isLocal != 0) {
                            sprintf(buf, s___11_d__100a6c3c, car->fFF8 + 1);
                        } else if (i == 0) {
                            sprintf(buf, s___y1_d__100a6c34, cars->fFF8 + 1);
                        } else {
                            sprintf(buf, s___ry_d__100a6c2c, car->fFF8 + 1);
                        }
                        BrTextDraw(buf, x, car->fFF8 * 0x10 + 0x14 + yBase);
                        if (DAT_118eeee0 != 0) {
                            c2 = 0x20;
                            c1 = 0x20;
                            if ((*(int *)&g_brRaceNet) > 1) {
                                v = BrSub100714D0(car->iNetPlayer) & 0x3f;
                                if (v < 3) {
                                    c2 = 0x2a;
                                    c1 = 0x2a;
                                } else if (v == 3) {
                                    v = BrSub10071510(car->iNetPlayer) & 0x3f;
                                    c1 = (v != 3 ? 10 : 0) + 0x20;
                                }
                            }
                            if (isLocal != 0) {
                                sprintf(buf, s___11_s__dms__c_c_100a6c18, ((void *)&car->szName[0]),
                                        BrNetSlotGetF974(car->iNetPlayer), c1, c2);
                            } else if ((*(unsigned char *)(((char *)car->pProfile) + 0x68) & 1) == 0
                                       && (*(int *)&DAT_105ccb68[8]) == 0) {
                                sprintf(buf, s___x_02x_02x_02x_s__s__dms__c_c_100a6bf8,
                                        (unsigned)car->f29AC,
                                        (unsigned)car->f29AD,
                                        (unsigned)car->f29AE,
                                        ((void *)&car->szName[0]),
                                        BrHudFormatGapString((const void *)cars, i),
                                        BrNetSlotGetF974(car->iNetPlayer), c1, c2);
                            } else {
                                sprintf(buf, s___x_02x_02x_02x_s__dms__c_c_100a6bdc,
                                        (unsigned)car->f29AC,
                                        (unsigned)car->f29AD,
                                        (unsigned)car->f29AE,
                                        ((void *)&car->szName[0]),
                                        BrNetSlotGetF974(car->iNetPlayer), c1, c2);
                            }
                            BrTextDraw(buf, x + 0x10, car->fFF8 * 0x10 + 0x14 + yBase);
                        } else {
                            if (isLocal != 0) {
                                sprintf(buf, s___11_s_100a6bd4, ((void *)&car->szName[0]));
                            } else if ((*(unsigned char *)(((char *)car->pProfile) + 0x68) & 1) == 0
                                       && (*(int *)&DAT_105ccb68[8]) == 0) {
                                sprintf(buf, s___x_02x_02x_02x_s__s_100a6bbc,
                                        (unsigned)car->f29AC,
                                        (unsigned)car->f29AD,
                                        (unsigned)car->f29AE,
                                        ((void *)&car->szName[0]),
                                        BrHudFormatGapString((const void *)cars, i));
                            } else {
                                sprintf(buf, s___x_02x_02x_02x_s_100a6ba8,
                                        (unsigned)car->f29AC,
                                        (unsigned)car->f29AD,
                                        (unsigned)car->f29AE,
                                        ((void *)&car->szName[0]));
                            }
                            BrTextDraw(buf, x + 0x10, car->fFF8 * 0x10 + 0x14 + yBase);
                        }
                    } else {
                        sprintf(buf, s___55_d__100a6ba0, car->fFF8 + 1);
                        BrTextDraw(buf, x, car->fFF8 * 0x10 + 0x14 + yBase);
                        sprintf(buf, s___55_s_100a6b98, ((void *)&car->szName[0]));
                        BrTextDraw(buf, x + 0x10, car->fFF8 * 0x10 + 0x14 + yBase);
                    }
                    i++;
                } while (i < (*(int *)&g_BrCarCount));
            }
        }
    }
}

