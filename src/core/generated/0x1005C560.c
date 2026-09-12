/* 0x1005C560 -- a car's between-races reset. */
#ifdef BR_MATCHING_BUILD

/* The original binary is /MD: CRT calls resolve through the import table. */
#define _CRTIMP __declspec(dllimport)

#include "slice2_15.h"    /* BR_CAR_STRIDE */

/* Both callees are thiscall(this) -- __fastcall carries the one register
 * argument exactly (see 0x1005C490.c). */
void __fastcall FUN_1006ff00(void *pCar);
void __fastcall BrRaceCarPickIndex(unsigned char *pCar);   /* 0x1005C490 */

extern unsigned char DAT_10af1208[];   /* car0 base                        */
extern int           DAT_100a9360;     /* the game mode                    */
extern unsigned char DAT_100b2fd8[];   /* 3 bytes of livery colour per car */
extern unsigned char DAT_10b1cbf0[];   /* 0x14C-byte comms records, 2 of   */
extern int           DAT_100aa2a8;     /* entries in the comms tables      */

/* WHAT IT DOES: puts one car back to its starting state between races.
 * Works out which car this is from its own address, restores its livery
 * colour from the table (skipped in mode 6), and for the two player cars
 * wires up and wipes the comms record -- header fields to their fixed
 * values and both per-entry tables to zero. Non-player cars drop their
 * record instead. Ends by re-picking which model the car drives. */
/* @implements 0x1005C560 glide BrRaceCarReset */
void __fastcall BrRaceCarReset(unsigned char *pCar)
{
    int idx;
    int i;

    FUN_1006ff00(pCar);
    idx = (int)(pCar - DAT_10af1208) / (int)BR_CAR_STRIDE;
    *(int *)(pCar + 0x140) = idx;

    if (DAT_100a9360 != 6) {
        /* Three int-typed locals: the original zero-extends all three
         * bytes into registers before any store. */
        int c2 = DAT_100b2fd8[idx * 3 + 2];
        int c1 = DAT_100b2fd8[idx * 3 + 1];
        int c0 = DAT_100b2fd8[idx * 3];
        pCar[0x29ad] = (unsigned char)c1;
        pCar[0x29ac] = (unsigned char)c0;
        pCar[0x29ae] = (unsigned char)c2;
    }

    if (idx < 2) {
        *(unsigned char **)(pCar + 0xe8c) = &DAT_10b1cbf0[idx * 0x14c];
        **(int **)(pCar + 0xe8c) = 0;
        (*(unsigned char **)(pCar + 0xe8c))[4] = 0;
        (*(unsigned char **)(pCar + 0xe8c))[5] = 0;
        *(short *)(*(int *)(pCar + 0xe8c) + 0xf0)  = 0x102;
        *(short *)(*(int *)(pCar + 0xe8c) + 0xf2)  = 4;
        *(short *)(*(int *)(pCar + 0xe8c) + 0xf4)  = 1;
        *(int *)(*(int *)(pCar + 0xe8c) + 0xfc)  = 1;
        *(int *)(*(int *)(pCar + 0xe8c) + 0xf8)  = 0;
        *(int *)(*(int *)(pCar + 0xe8c) + 0x104) = 1;
        *(int *)(*(int *)(pCar + 0xe8c) + 0x100) = 2;
        *(int *)(*(int *)(pCar + 0xe8c) + 0x108) = 0;
        *(int *)(*(int *)(pCar + 0xe8c) + 0x108) = 5;
        for (i = 0; i < DAT_100aa2a8; i++) {
            *(int *)(*(int *)(pCar + 0xe8c) + 0xb0 + i * 4)  = 0;
            *(int *)(*(int *)(pCar + 0xe8c) + 0x10c + i * 4) = 0;
        }
    } else {
        *(int *)(pCar + 0xe8c) = 0;
    }

    BrRaceCarPickIndex(pCar);
    *(int *)(pCar + 0xe88) = 0;
}

#endif /* BR_MATCHING_BUILD */
