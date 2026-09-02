/* @implements 0x10038E10 glide BrUiText1003F8D0
 * @cpp_kind method
 * @cpp_symbol BrUiText1003F8D0
 *
 * Free cdecl (ctl): refills the control's text box.  When 0x10AC5BA8 is
 * set the text comes from string-table entry 0xAF (FUN_1006d280) and the
 * +0x2B64 kind byte is 4 or 1 by g_AC4648[g_iAA2840] (fixed-address array
 * folded as a displacement); otherwise the literal at 0x100ACAD8 is used
 * and the kind byte is left alone.  Both arms are the VC5 INTRINSIC
 * strcpy (repne scasb + rep movsd/movsb) into name[] at box+9.  Then the
 * box's +4 vcall (EDX pattern) and Br85ItemApply(ctl, 0).  No EH.
 */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#endif
#include <string.h>

class BrBox85 {
public:
    virtual int b0();
    virtual int b1();          /* +0x04 */

    char pad04[8 - 4];
    char kind;                 /* +0x08 -> ctl+0x2B64 */
    char name[1];              /* +0x09 -> ctl+0x2B65 */
};

struct BrCtl85 {
    char pad00[0x2B5C];
    class BrBox85 box;         /* +0x2B5C */
};

extern "C" {
extern int  DAT_10ac5ba8;
extern int  g_iAA2840;         /* 0x10AC5B98 */
extern int  DAT_10ac4648[];    /* 0x10AC4648 */
extern char DAT_100acad8[];    /* the fallback text */

char *FUN_1006d280(int id);
int   Br85ItemApply(struct BrCtl85 *pCtl, short index);

int BrUiText1003F8D0(BrCtl85 *pCtl)
{
    if (DAT_10ac5ba8 != 0) {
        strcpy(pCtl->box.name, FUN_1006d280(0xAF));
        if (DAT_10ac4648[g_iAA2840] != 0) {
            pCtl->box.kind = 4;
        } else {
            pCtl->box.kind = 1;
        }
    } else {
        strcpy(pCtl->box.name, DAT_100acad8);
    }

    pCtl->box.b1();
    Br85ItemApply(pCtl, 0);
    return 1;
}
}
