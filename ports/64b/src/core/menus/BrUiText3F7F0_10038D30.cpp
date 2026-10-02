/* WHAT IT DOES: set this item's caption from the string table by the current
 * index, then lay it out. One of several identical hooks that differ only in
 * which table and index they read. */
/* @implements 0x10038D30 glide BrUiText3F7F0
 * @cpp_kind method
 * @cpp_symbol ?BrUiText3F7F0@@YAHPAVGameObj@@@Z
 *
 * UI text-refresh family: strcpy a looked-up string into the item's
 * text buffer, then a slot-1 virtual thiscall on the embedded item
 * (`lea ecx,[ebx+0x2B5C]; mov edx,[ebx+0x2B5C]; call [edx+4]` with the
 * vtbl load scheduled inside the strcpy intrinsic: C++ frontend order,
 * not reachable from the C fastcall spelling). No EH.
 */
#define _CRTIMP __declspec(dllimport)
#include "br_coretypes.h"   /* br_globals: its objects */
#include "br_ui.h"   /* BrUiCtl_, the canonical record */
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




extern "C" {
/* 64-bit core: declared once, in br_globals.h or its struct's header */
#define g_idx DAT_100abdec
extern "C" char *DAT_100abae8[1];   /* the original global; was a per-file stand-in definition */
#define g_tab DAT_100abae8
/* BrStrGet: prototype in br_funcs.h */
/* Br85ItemApply: prototype in br_funcs.h */
}

int BrUiText3F7F0(GameObj *pGame)
{
    strcpy((*(char (*)[256])&((BrUiCtl_ *)(pGame))->aText[0].sz[0]), BrStrGet(g_tab[g_idx]));
    (*(class Item *)&((BrUiCtl_ *)(pGame))->aText[0]).i1();
    Br85ItemApply((struct BrCtl85 *)(pGame), 0);
    return 1;
}
