/* @implements 0x10038D30 glide BrUiText3F7F0
 * @cpp_kind method
 * @cpp_symbol ?BrUiText3F7F0@@YAHPAVGameObj@@@Z
 *
 * UI text-refresh family: strcpy a looked-up string into the item's
 * text buffer, then a slot-1 virtual thiscall on the embedded item
 * (`lea ecx,[ebx+0x2B5C]; mov edx,[ebx+0x2B5C]; call [edx+4]` with the
 * vtbl load scheduled inside the strcpy intrinsic — C++ frontend order,
 * not reachable from the C fastcall spelling). No EH.
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
    char pad2[5];
    char text[256];
};

typedef char chk_item[(unsigned)&((GameObj *)0)->item == 0x2B5C ? 1 : -1];
typedef char chk_text[(unsigned)&((GameObj *)0)->text == 0x2B65 ? 1 : -1];

extern "C" {
int g_idx;
char *g_tab[1];
char *BrStrGet(char *);
int Br85ItemApply(GameObj *, int);
}

int BrUiText3F7F0(GameObj *pGame)
{
    strcpy(pGame->text, BrStrGet(g_tab[g_idx]));
    pGame->item.i1();
    Br85ItemApply(pGame, 0);
    return 1;
}
