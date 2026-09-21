/* WHAT IT DOES: apply one row of a menu to the screen: lay it out, and when
 * it is being edited also handle the caret and the text the player is
 * typing. The shared workhorse behind the label hooks around it. */
/* @implements 0x10038380 glide Br85ItemApply
 * @cpp_kind method
 * @cpp_symbol Br85ItemApply
 *
 * Free cdecl (ctl, int16 index): finishes an edit in the indexed control's
 * text box.  The row is ctl + index*1080 (movsx + the *3/*5/*9 lea chain,
 * *8 folded into the row lea); the box is embedded at row+0x2B5C and its
 * vtbl is CSEd into ebp across all four vcalls (+4 poll, +0x10 refresh
 * twice, +0x14 ask).  Early-out when the box's +0x420 edit flag is clear
 * (with the unfoldable name[]-at-+9 guard before the refresh, return 0).
 * The GLIDE original tests the 0x10AC5BB4 POINTER itself against 0 (the
 * D3D-derived port dereferences it -- they differ); ctl-level fields stay
 * on the BASE ctl: +0x10 hook (cdecl, one pushed arg), +0x1C flag word
 * (`and al,0xFD`, full dword store).  No EH (no new).
 */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#endif

class BrBox85 {
public:
    virtual int b0();
    virtual int b1();          /* +0x04, entry poll */
    virtual int b2();
    virtual int b3();
    virtual int b4();          /* +0x10, refresh */
    virtual int b5();          /* +0x14, "keep the edit?" */

    char pad04[9 - 4];
    char name[1];              /* +0x09 */
};

struct BrCtl85 {
    char pad00[0x10];
    int  (*pfn10)(struct BrCtl85 *);   /* +0x10 */
    char pad14[0x1C - 0x14];
    int  flags1C;                      /* +0x1C */
    char pad20[0x2B5C - 0x20];
    class BrBox85 box;                 /* +0x2B5C */
    char padBox[0x420 - 0x0C];         /* the class pads to 0xC */
    int  editing;                      /* +0x2F7C = box + 0x420 */
};

extern "C" {
extern int DAT_10ac5bb4;
extern int g_brAA28D8;                 /* 0x10AC5C30 */
int FUN_10037710();

int Br85ItemApply(BrCtl85 *pCtl, short index)
{
    BrCtl85 *pRow = (BrCtl85 *)((char *)pCtl + index * 1080);

    pRow->box.b1();

    if (pRow->editing == 0) {
        if (pRow->box.name != 0) {
            pRow->box.b4();
        }
        return 0;
    }

    if ((char)pRow->box.b5() <= 0 || (pCtl->flags1C & 2) != 0) {
        if (DAT_10ac5bb4 == 0) {
            g_brAA28D8     = 0;
            pRow->editing  = 0;
            pCtl->flags1C &= ~2;
        }
        FUN_10037710();
        if (pCtl->pfn10 != 0) {
            pCtl->pfn10(pCtl);
        }
    }

    pRow->box.b4();
    return 1;
}
}
