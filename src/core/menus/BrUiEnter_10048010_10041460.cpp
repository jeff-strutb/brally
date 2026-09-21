/* WHAT IT DOES: enter a menu page, taking the shortcut path when the page is
 * flagged as already prepared rather than laying it out again. */
/* @implements 0x10041460 glide BrUiEnter_10048010
 * @cpp_kind method
 * @cpp_symbol ?Enter@BrPhase41E@@QAEHXZ
 *
 * Thiscall on the phase object, receiver stays in ecx (no esi copy).
 * Gate on bit 0 of the +0x28 flags (byte-narrowed test), then on the
 * +0x1C flag word (0x100000 arm: guard on the embedded item's name[]
 * at +0x2B5C+9 -- a lea/test the compiler does not fold -- then the
 * item's +0x10 vcall, return 1; 0x200000 arm: return 1; else the
 * phase's own +0x10 vcall, returning 0 only when it returns 0).
 * No EH (no new).
 */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#endif

class BrPhaseItem41E {
public:
    virtual int i0();
    virtual int i1();
    virtual int i2();
    virtual int i3();
    virtual int i4();          /* slot +0x10 */

    char pad04[9 - 4];
    char name[1];              /* +0x09 */
};

class BrPhase41E {
public:
    virtual int s0();
    virtual int s1();
    virtual int s2();
    virtual int s3();
    virtual int s4();          /* slot +0x10 */

    char            pad04[0x1C - 4];
    int             f1C;       /* +0x1C, the mode flag word */
    char            pad20[0x28 - 0x20];
    int             f28;       /* +0x28, bit 0 gates everything */
    char            pad2C[0x2B5C - 0x2C];
    BrPhaseItem41E  item;      /* +0x2B5C, embedded */

    int Enter();
};

int BrPhase41E::Enter()
{
    if (this->f28 & 1) {
        if (this->f1C & 0x100000) {
            if (this->item.name != 0) {
                this->item.i4();
            }
            return 1;
        }
        if ((this->f1C & 0x200000) == 0) {
            if (this->s4() == 0) {
                return 0;
            }
        }
    }
    return 1;
}
