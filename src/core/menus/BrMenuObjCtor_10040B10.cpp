/* WHAT IT DOES: builds a fresh menu-screen object: every counter, flag and
 * table starts at zero (a few tables at "none", -1), the fade factor at
 * 0.99, the three text items and the text list are constructed, and the
 * screen is marked live with its one flag set. */
/* @implements 0x10040B10 glide BrMenuObjCtor_10040B10
 * @cpp_kind method
 * @cpp_symbol ??0MenuObj40B10@@QAE@XZ
 *
 * Thiscall constructor, `this` spilled to the frame for the unwind funclet,
 * maxState=1 (the Item438 array is the one member with a destructor; the
 * text list has none).  The stores before the vector-ctor call are the
 * member initialisers up to +0x4C in declaration order; the +0x3804..+0x3836
 * stores are the initialisers between the array and the text list; the
 * +0x1E20C word is the last initialiser; then the vptr, then the body.
 * The fills are memset intrinsics (`mov ecx,N; xor eax,eax / or eax,-1;
 * lea edi`); the 50-byte one is `rep stosd` + `stosw`.
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

class TextList {
    char b[0x1E20C - 0x3838];
public:
    TextList();
};

class MenuObj40B10 {
public:
    virtual void v0();

    int            f004;
    int            f008;
    int            f00C;
    int            f010;
    int            f014;
    int            f018;
    int            f01C;
    int            f020;
    int            f024;
    int            f028;
    unsigned char  b02C;
    char           pad02D[3];
    int            f030;
    int            f034;
    int            f038;
    int            f03C;
    int            f040;
    float          f044;
    short          w048;
    short          w04A;
    int            f04C;
    char           pad050[0x10];
    int            a060[0x32];
    short          w128;
    char           a12A[0x2710];
    char           pad283A[2];
    int            a283C[0x32];
    int            a2904[0x19];
    int            f2968;
    int            f296C;
    int            f2970;
    int            f2974;
    int            a2978[0x32];
    int            a2A40[0x19];
    int            f2AA4;
    int            f2AA8;
    short          w2AAC;
    char           pad2AAE[6];
    short          w2AB4;
    char           a2AB6[0x32];
    int            f2AE8;
    int            f2AEC;
    int            a2AF0[0x19];
    int            f2B54;
    int            f2B58;
    Item438        items[3];        /* +0x2B5C */
    int            f3804;
    int            f3808;
    unsigned char  b380C;
    unsigned char  b380D;
    char           pad380E[2];
    int            f3810;
    int            f3814;
    int            f3818;
    int            f381C;
    int            f3820;
    int            f3824;
    int            f3828;
    int            f382C;
    int            f3830;
    short          w3834;
    short          w3836;
    TextList       text;            /* +0x3838 */
    short          w1E20C;
    char           pad1E20E[2];

    MenuObj40B10();
};

typedef char chk_items[(unsigned)&((MenuObj40B10 *)0)->items == 0x2B5C ? 1 : -1];
typedef char chk_text[(unsigned)&((MenuObj40B10 *)0)->text == 0x3838 ? 1 : -1];
typedef char chk_w1e20c[(unsigned)&((MenuObj40B10 *)0)->w1E20C == 0x1E20C ? 1 : -1];
typedef char chk_a12a[(unsigned)&((MenuObj40B10 *)0)->a12A == 0x12A ? 1 : -1];
typedef char chk_a2ab6[(unsigned)&((MenuObj40B10 *)0)->a2AB6 == 0x2AB6 ? 1 : -1];

MenuObj40B10::MenuObj40B10()
    : f01C(1), f020(0), f024(0), f028(0), b02C(0xFF),
      f030(0), f034(0), f038(0), f03C(0), f040(0), f044(0.99f),
      w048(0), w04A(0), f04C(0),
      f3804(0), f3808(0), b380C(0), b380D(0),
      f3810(0), f3814(0), f3818(0), f381C(0), f3820(0), f3824(0),
      f3828(0), f382C(0), f3830(0), w3834(0), w3836(0),
      w1E20C(0)
{
    f004 = 0;
    f008 = 0;
    f00C = 0;
    f014 = 0;
    f018 = 0;
    f2AA4 = 0;
    f2AA8 = 0;
    w2AAC = 0;
    w128 = 0;
    f2970 = 0;
    f2974 = 0;
    f2968 = 0;
    f296C = 0;
    memset(a2904, 0, sizeof(a2904));
    memset(a2978, 0, sizeof(a2978));
    memset(a2A40, 0xFF, sizeof(a2A40));
    memset(a12A, 0xFF, sizeof(a12A));
    memset(a060, 0, sizeof(a060));
    memset(a283C, 0, sizeof(a283C));
    w2AB4 = 0;
    memset(a2AB6, 0, sizeof(a2AB6));
    f2AE8 = 0;
    f2AEC = 1;
    memset(a2AF0, 0xFF, sizeof(a2AF0));
    f2B54 = 1;
    f2B58 = 0;
    f010 = 0;
}
