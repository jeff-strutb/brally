/* @implements 0x10041ED0 glide BrPhaseReleasePages_10048AA0
 * @cpp_kind method
 * @cpp_symbol ?ReleasePages@Phase32P@@QAEXXZ
 *
 * 119 B thiscall, void. Outer loop over the +0x14 page array (this and i
 * spilled -- sub esp,8), inner countdown over the page's 200 member
 * pointers at +0x18: delete (slot-0 scalar-deleting vcall, flag 1) and
 * zero each, then delete the page itself. Word global cleared at the end.
 */
class PageMember {
public:
    virtual ~PageMember();
};

class Page {
public:
    virtual ~Page();
    char pad[0x14];             /* +0x04 */
    PageMember *m[200];         /* +0x18 */
};

class Phase32P {
public:
    virtual void v0();
    char pad04[0xC];            /* +0x04 */
    unsigned short f10;         /* +0x10 */
    unsigned short f12;         /* +0x12 */
    Page *a14[20];              /* +0x14 */
    void ReleasePages();
};

extern "C" {
extern unsigned short DAT_10ac5bc4;
}

void Phase32P::ReleasePages()
{
    int i;

    for (i = 0; i < f10; ++i) {
        Page *p = a14[i];
        int k;

        for (k = 0; k < 200; ++k) {
            delete p->m[k];
            p->m[k] = 0;
        }
        delete p;
    }
    DAT_10ac5bc4 = 0;
}
