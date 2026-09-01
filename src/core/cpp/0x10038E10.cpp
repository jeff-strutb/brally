/* @implements 0x10038E10 glide BrUiText1003F8D0
 * @cpp_kind method
 * @cpp_symbol ?BrUiText1003F8D0@@YAHPAVGameObj@@@Z
 *
 * UI text-refresh family (same +0x2B5C item / +0x2B65 text layout as
 * 0x10038D30): gated on g_5BA8, strcpy of either a looked-up string
 * (id 0xAF) or the fixed fallback into the text buffer, a mode byte at
 * +0x2B64 forked on a pinned table lookup, then the slot-1 item vcall
 * and a two-arg cdecl helper. No EH.
 */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#endif
#include <string.h>

class Item {
public:
    virtual void i0();
    virtual void i1();
};

class GameObj {
public:
    char pad[0x2B5C];
    Item item;
    char pad2[4];
    unsigned char flag2B64;
    char text[256];
};

typedef char chk_item[(unsigned)&((GameObj *)0)->item == 0x2B5C ? 1 : -1];
typedef char chk_flag[(unsigned)&((GameObj *)0)->flag2B64 == 0x2B64 ? 1 : -1];
typedef char chk_text[(unsigned)&((GameObj *)0)->text == 0x2B65 ? 1 : -1];

extern "C" {
int g_5BA8;
int g_5B98;
int g_4648[1];
char g_strA[1];
char *BrStrGet(int);
int Fn8380(GameObj *, int);
}

int BrUiText1003F8D0(GameObj *pGame)
{
    if (g_5BA8 != 0) {
        strcpy(pGame->text, BrStrGet(0xAF));
        if (g_4648[g_5B98] != 0)
            pGame->flag2B64 = 4;
        else
            pGame->flag2B64 = 1;
    } else {
        strcpy(pGame->text, g_strA);
    }
    pGame->item.i1();
    Fn8380(pGame, 0);
    return 1;
}
