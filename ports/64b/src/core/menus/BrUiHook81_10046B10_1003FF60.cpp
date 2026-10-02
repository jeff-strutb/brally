/* WHAT IT DOES: leave this menu page: run its leave routine, destroy the
 * page object, and make its parent current again. One of a family that
 * differ only in which parent they return to and which state flags they
 * clear, and this one also restores the track name buffer from its saved
 * copy, so backing out of the page undoes any edit the player made. */
/* @implements 0x1003FF60 glide BrUiHook81_10046B10
 * @cpp_kind method
 * @cpp_symbol ?BrUiHook81_10046B10@@YAHPAVGameObj@@@Z
 *
 * Phase-leave prefix (slot+0x1C vcall, slot-0 one-arg vcall on g_cur)
 * then a double-strcpy tail: the same source string copied into two
 * global buffers by the VC5 strcpy intrinsic, with the zero stores and
 * the -1 store scheduled into the intrinsic's latency slots. Seven
 * siblings differ only in the phase-source global (byte 145). No EH.
 */
/* Twin of 0x1003FBE0 BrMenuResetTrackStr (tools/gen_cpptwin.py): identical machine code,
 * only the reloc slots differ. */
#define _CRTIMP __declspec(dllimport)
#include "slice2_25.h"   /* br_globals: its objects */
#include <string.h>

class GameSub {
public:
    virtual void s0();
    virtual void s1();
    virtual void s2();
    virtual void s3();
    virtual void s4();
    virtual void s5();
    virtual void s6(int);
    virtual void s7();
};

class GameObj {
public:
    char pad[0x2AE8];
    GameSub *pSub;
};

class Phase {
public:
    virtual void *f00(int);
};

typedef char chk_sub[(unsigned)&((GameObj *)0)->pSub == 0x2AE8 ? 1 : -1];

/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
#define g_5C84 g_5C94
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

int BrUiHook81_10046B10(GameObj *pGame)
{
    Phase *pObj;

    pGame->pSub->s7();
    pObj = (Phase *)((*(Phase * *)&g_brPAA29B8));
    if (pObj != 0)
        pObj->f00(1);
    g_5C80 = 0;
    DAT_10ac5d18 = 0;
    g_5D24 = 0;
    g_5C3C = 0;
    strcpy(g_aBrAA2518, g_aBr39B720);
    g_AB94 = -1;
    strcpy(DAT_10ac46a0, g_aBr39B720);
    (*(Phase * *)&g_brPAA29B8) = (Phase *)((BrOptObj *)(g_5C84));
    return 0;
}
