/* WHAT IT DOES: sets up a list of text rows -- the widget behind the game's
 * scrolling menus and high-score tables. It builds a hundred empty rows,
 * clears the scroll bookkeeping, plants four "none" markers that get their
 * real values later, and clears the hundred slots that can hold an
 * arbitrary lump of data alongside each row. */
/* @implements 0x10054610 glide BrTextListInit
 * @cpp_kind method
 * @cpp_symbol ??0TextList54610@@QAE@XZ
 *
 * Thiscall constructor, no EH frame: the hundred-item array is the last
 * member with a destructor, so nothing after it can throw.  The three
 * dwords before the array and everything from +0x1A92C on are member
 * initialisers (declaration order); +0x04..+0x14 and the blob memset are
 * the body.  The C twin in slice3_39.c stops at 160/218 B because a C
 * caller cannot emit the vector-constructor-iterator call.
 */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#endif
#include <string.h>

class Item438 {
    char b[0x438];
public:
    Item438();
    ~Item438();
};

class TextList54610 {
public:
    virtual void v0();

    int      f004;
    int      f008;
    int      f00C;
    int      f010;
    int      f014;
    int      f018;
    int      f01C;
    int      f020;
    char     pad024[8];
    Item438  items[100];        /* +0x2C */
    int      aBlob[0xC8];       /* +0x1A60C */
    short    w1A92C;
    short    w1A92E;
    short    w1A930;
    short    w1A932;
    short    w1A934;
    short    w1A936;
    short    w1A938;
    char     pad1A93A[0x62];
    int      f1A99C;
    int      f1A9A0;
    int      f1A9A4;
    int      f1A9A8;
    int      f1A9AC;
    int      f1A9B0;
    int      f1A9B4;
    int      f1A9B8;
    int      f1A9BC;
    int      f1A9C0;
    int      f1A9C4;
    int      f1A9C8;
    int      f1A9CC;
    int      f1A9D0;

    TextList54610();
};

typedef char chk_items[(unsigned)&((TextList54610 *)0)->items == 0x2C ? 1 : -1];
typedef char chk_blob[(unsigned)&((TextList54610 *)0)->aBlob == 0x1A60C ? 1 : -1];
typedef char chk_w[(unsigned)&((TextList54610 *)0)->w1A92C == 0x1A92C ? 1 : -1];
typedef char chk_f[(unsigned)&((TextList54610 *)0)->f1A99C == 0x1A99C ? 1 : -1];
typedef char chk_sz[sizeof(TextList54610) == 0x1A9D4 ? 1 : -1];

TextList54610::TextList54610()
    : f018(0), f01C(0), f020(0),
      w1A92C(0), w1A92E(0), w1A930(0),
      w1A932(-1), w1A934(-1), w1A936(-1), w1A938(-1),
      f1A99C(0), f1A9A0(0), f1A9A4(0), f1A9A8(0), f1A9AC(0), f1A9B0(0),
      f1A9B4(0), f1A9B8(0), f1A9BC(0), f1A9C0(0), f1A9C4(0), f1A9C8(0),
      f1A9CC(0), f1A9D0(0)
{
    f004 = 0;
    f008 = 0;
    f00C = 0;
    f010 = 0;
    f014 = 0;
    memset(aBlob, 0, sizeof(aBlob));
}
