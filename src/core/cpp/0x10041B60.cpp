/* WHAT IT DOES: creates the object that holds one whole menu -- its pages,
 * which page is showing, and two lists of a hundred default names each,
 * pre-filled as "Driver 1", "Driver 2" and so on from a pattern in the
 * game's text table.  Either list failing to allocate raises the
 * out-of-memory error.  It leaves the "what to do when this menu opens"
 * slot untouched, so it holds rubbish until whoever created the menu fills
 * it in. */
/* @implements 0x10041B60 glide BrOptObjCtor
 * @cpp_kind method
 * @cpp_symbol ??0OptObj41B60@@QAE@XZ
 *
 * Thiscall constructor with EH: the two `new NameList` expressions are
 * unwind states 0 and 1 (the raw allocation is freed if the list's own
 * constructor throws), `this` spilled to the frame for the funclet.  The
 * seven stores before the vptr are member initialisers in declaration
 * order; the vptr lands before the body.  sprintf is the /MD import,
 * cached in ebp across the fill loop; the loop's test is on the
 * strength-reduced byte offset (`cmp esi,0x6590`).
 */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#endif
#include <stdio.h>
#include <string.h>

class NameList55 {
public:
    int  hdr;
    char asz[100][0x104];

    NameList55();
};

typedef char chk_nl[sizeof(NameList55) == 0x6594 ? 1 : -1];

extern "C" {
const char *BrStrGet(int id);
void        Br73Err(int code);
}

class OptObj41B60 {
public:
    virtual void v0();

    int         f004;
    void       *f008;
    int         f00C;
    short       w010;
    short       w012;
    char        pad014[0x50];
    void       *f064;
    int         f068;
    int         a06C[20];
    short       w0BC;
    char        pad0BE[2];
    NameList55 *pC0;
    NameList55 *pC4;

    OptObj41B60();
};

typedef char chk_c0[(unsigned)&((OptObj41B60 *)0)->pC0 == 0xC0 ? 1 : -1];
typedef char chk_bc[(unsigned)&((OptObj41B60 *)0)->w0BC == 0xBC ? 1 : -1];
typedef char chk_6c[(unsigned)&((OptObj41B60 *)0)->a06C == 0x6C ? 1 : -1];

OptObj41B60::OptObj41B60()
    : f008(0), f00C(0), w010(0), w012(0), f064(0), f068(1), w0BC(0)
{
    int i;

    pC0 = new NameList55;
    if (pC0 == 0)
        Br73Err(6);
    pC4 = new NameList55;
    if (pC4 == 0)
        Br73Err(6);

    for (i = 0; i < 100; i++) {
        sprintf(pC0->asz[i], BrStrGet(0xBE), i);
        sprintf(pC4->asz[i], BrStrGet(0xBE), i);
    }

    memset(a06C, 0, sizeof(a06C));
}
