/* Glide match for BrPfxTick — 0x10033BB0
 *
 * The port body lives in src/core/slice2_21.c (tagged 0x1003A530 d3d) and
 * carries the aggregate parameters `(pPool, pEnv, pFxEnv, pTick, pSeed)`
 * the port introduced.  The original takes NO arguments at all: the pool,
 * the two mode words, the driver count and the driver-slot table are
 * globals, and the three per-car helpers are __fastcall on the car
 * pointer alone (`mov ecx,[esi]` / `call`).  That parameter list is the
 * whole reason the port body could never converge — see the
 * port-safety/globals-struct class in docs/VC5-IDIOMS.md.
 *
 * Shape notes, read off the original:
 *  - the driver table at 0x10AF0858 has a 0x80-byte stride with the car
 *    pointer at +0; VC5 strength-reduces the subscript into the `add
 *    esi,0x80` pointer walk.
 *  - the loop bound `DAT_100b2f00` is RE-READ at the bottom of every
 *    iteration, which is what a plain `i < global` for-loop emits once a
 *    call in the body can clobber it.
 *  - the car pointer is re-loaded for the second call in the first two
 *    loops (the call clobbers ecx), so each arm spells the slot read out
 *    rather than caching it in a local.
 *  - the two early `return`s give loops 1 and 2 their own
 *    `pop edi/pop esi/ret`; all three `jle` exits share loop 3's.
 */
#ifdef BR_MATCHING_BUILD

typedef struct {
    void *pCar;                 /* +0x00 */
    char  pad[0x7C];
} BrPfxDriverSlot;              /* 0x80 */

extern int DAT_10ac2c48;        /* pool-initialised flag */
extern int DAT_106ed6b0;        /* mode: age the B0 family */
extern int DAT_106ed6ac;        /* mode word */
extern int DAT_106ed6b4;        /* mode flag */
extern int DAT_100b2f00;        /* driver count */
extern BrPfxDriverSlot DAT_10af0858[];

void BrPfxReset(void);
void BrPfxUpdateB0(void);
void BrPfxUpdateB4AC(void);
void __fastcall BrCarSub9020(void *pCar);
void __fastcall BrCarWheelFx(void *pCar);
void __fastcall BrCarPfxSpawn(void *pCar);

/* WHAT IT DOES: run the particle system for one frame -- initialises it on
 * the very first call, then updates whichever particle lists the current
 * mode uses and gives every active car a chance to throw up its own wheel
 * effects. */
/* @implements 0x10033BB0 glide BrPfxTick */
void BrPfxTick(void)
{
    int i;

    if (DAT_10ac2c48 == 0) {
        BrPfxReset();
        DAT_10ac2c48 = 1;
    }

    if (DAT_106ed6b0 != 0) {
        BrPfxUpdateB0();
        for (i = 0; i < DAT_100b2f00; i++) {
            if (DAT_10af0858[i].pCar != 0) {
                BrCarSub9020(DAT_10af0858[i].pCar);
                BrCarWheelFx(DAT_10af0858[i].pCar);
            }
        }
        return;
    }

    if (DAT_106ed6ac == 0 && DAT_106ed6b4 == 0) {
        BrPfxUpdateB4AC();
        for (i = 0; i < DAT_100b2f00; i++) {
            if (DAT_10af0858[i].pCar != 0) {
                BrCarPfxSpawn(DAT_10af0858[i].pCar);
                BrCarWheelFx(DAT_10af0858[i].pCar);
            }
        }
        return;
    }

    for (i = 0; i < DAT_100b2f00; i++) {
        if (DAT_10af0858[i].pCar != 0)
            BrCarWheelFx(DAT_10af0858[i].pCar);
    }
}

#endif /* BR_MATCHING_BUILD */
