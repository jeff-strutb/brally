/* WHAT IT DOES: leave this menu page: run its leave routine, destroy the
 * page object, and make its parent current again. One of a family that
 * differ only in which parent they return to and which state flags they
 * clear, and this one also restores the track name buffer from its saved
 * copy, so backing out of the page undoes any edit the player made. */
/* @implements 0x1003FDA0 glide FUN_1003fda0
 * @cpp_kind method
 * @cpp_symbol ?FUN_1003fda0@@YAHPAVGameObj@@@Z
 *
 * Phase-leave prefix (slot+0x1C vcall, slot-0 one-arg vcall on g_cur)
 * then a double-strcpy tail: the same source string copied into two
 * global buffers by the VC5 strcpy intrinsic, with the zero stores and
 * the -1 store scheduled into the intrinsic's latency slots. Seven
 * siblings differ only in the phase-source global (byte 145). No EH.
 */
/* Twin of 0x1003FBE0 BrMenuResetTrackStr (tools/gen_cpptwin.py): identical machine code,
 * only the reloc slots differ. */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#endif
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

Phase *g_cur;
Phase *g_5C84;
int g_5C80;
int g_5D18;
int g_5D24;
int g_5C3C;
int g_AB94;
char g_srcStr[64];
char g_bufA[64];
char g_bufB[64];

int FUN_1003fda0(GameObj *pGame)
{
    Phase *pObj;

    pGame->pSub->s7();
    pObj = g_cur;
    if (pObj != 0)
        pObj->f00(1);
    g_5C80 = 0;
    g_5D18 = 0;
    g_5D24 = 0;
    g_5C3C = 0;
    strcpy(g_bufA, g_srcStr);
    g_AB94 = -1;
    strcpy(g_bufB, g_srcStr);
    g_cur = g_5C84;
    return 0;
}
