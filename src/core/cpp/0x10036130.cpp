/* WHAT IT DOES: handle a DirectPlay callback for one player record, ignoring
 * the notifications flagged as uninteresting and otherwise updating that
 * player's slot. */
/* @implements 0x10036130 glide BrWmHook36130
 * @cpp_kind method
 * @cpp_symbol ?BrWmHook36130@@YGHHHPAURec36130@@IH@Z
 *
 * Sibling of 0x10035A30: same Sel slot-4 vcall through a Sel* temp
 * (different arg global), then a store into the 1080-byte slot array
 * indexed by the unsigned short at +0x1E164, and a local call.
 * Early-outs: 0 when the game object is null (eax reuse — return the
 * pointer itself), 1 when bit 9 of arg4 is set.
 */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#endif

class Sel {
public:
    virtual void s0();
    virtual void s1();
    virtual void s2();
    virtual void s3();
    virtual void s4(int, int, int, void *, int);
};

struct Rec36130 {
    int f0;
    int f4;
    int f8;
};

struct Slot36130 {
    int f0;
    char pad[1076];
};

class GameObjS {
public:
    char pad[0x3838];
    Sel sel;
    char pad2[0x24];
    Slot36130 slots[100];
    char pad3[0x324];
    unsigned short wIdx;
};

typedef char chk_sel[(unsigned)&((GameObjS *)0)->sel == 0x3838 ? 1 : -1];
typedef char chk_sl[(unsigned)&((GameObjS *)0)->slots == 0x3860 ? 1 : -1];
typedef char chk_ix[(unsigned)&((GameObjS *)0)->wIdx == 0x1E164 ? 1 : -1];

extern "C" {
GameObjS *g_pGame2;
int g_selArg2;
void BrTick36080(int);
}

int __stdcall BrWmHook36130(int a1, int a2, Rec36130 *a3, unsigned int a4, int a5)
{
    GameObjS *p;

    p = g_pGame2;
    if (p == 0)
        return (int)p;
    if (a4 & 0x200)
        return 1;
    {
        Sel *s = &p->sel;
        s->s4(a3->f8, 0, 1, &g_selArg2, 1);
    }
    g_pGame2->slots[g_pGame2->wIdx].f0 = a1;
    BrTick36080(a1);
    return 1;
}
