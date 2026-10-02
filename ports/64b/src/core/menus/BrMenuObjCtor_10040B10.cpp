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
#define _CRTIMP __declspec(dllimport)
#include <string.h>
#include "br_ui.h"   /* BrUiCtl_, the canonical record */

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







MenuObj40B10::MenuObj40B10()
    : f01C(1), f020(0), f024(0), f028(0), b02C(0xFF),
      f030(0), f034(0), f038(0), f03C(0), f040(0), f044(0.99f),
      w048(0), w04A(0), f04C(0),
      f3804(0), f3808(0), b380C(0), b380D(0),
      f3810(0), f3814(0), f3818(0), f381C(0), f3820(0), f3824(0),
      f3828(0), f382C(0), f3830(0), w3834(0), w3836(0),
      w1E20C(0)
{
    (*(int *)&((BrUiCtl_ *)(this))->pfn04) = 0;
    (*(int *)&((BrUiCtl_ *)(this))->pfn08) = 0;
    (*(int *)&((BrUiCtl_ *)(this))->pfn0C) = 0;
    (*(int *)&((BrUiCtl_ *)(this))->pfn14) = 0;
    (*(int *)&((BrUiCtl_ *)(this))->pfn18) = 0;
    (*(int *)&((BrUiCtl_ *)(this))->f2AA4) = 0;
    (*(int *)&((BrUiCtl_ *)(this))->f2AA8) = 0;
    (*(short *)&((BrUiCtl_ *)(this))->w2AAC) = 0;
    (*(short *)&((BrUiCtl_ *)(this))->wStep) = 0;
    (*(int *)&((BrUiCtl_ *)(this))->f2970) = 0;
    (*(int *)&((BrUiCtl_ *)(this))->f2974) = 0;
    (*(int *)&((BrUiCtl_ *)(this))->f2968) = 0;
    (*(int *)&((BrUiCtl_ *)(this))->f296C) = 0;
    memset((*(int (*)[25])&((BrUiCtl_ *)(this))->a2904), 0, sizeof((*(int (*)[25])&((BrUiCtl_ *)(this))->a2904)));
    memset((*(int (*)[50])&((BrUiCtl_ *)(this))->aStepMs), 0, sizeof((*(int (*)[50])&((BrUiCtl_ *)(this))->aStepMs)));
    memset((*(int (*)[25])&((BrUiCtl_ *)(this))->aStepId), 0xFF, sizeof((*(int (*)[25])&((BrUiCtl_ *)(this))->aStepId)));
    memset((*(char (*)[10000])&((BrUiCtl_ *)(this))->a012A), 0xFF, sizeof((*(char (*)[10000])&((BrUiCtl_ *)(this))->a012A)));
    memset((*(int (*)[50])&((BrUiCtl_ *)(this))->a0060), 0, sizeof((*(int (*)[50])&((BrUiCtl_ *)(this))->a0060)));
    memset((*(int (*)[50])&((BrUiCtl_ *)(this))->a283C), 0, sizeof((*(int (*)[50])&((BrUiCtl_ *)(this))->a283C)));
    (*(short *)&((BrUiCtl_ *)(this))->cChild) = 0;
    memset((*(char (*)[50])&((BrUiCtl_ *)(this))->aChild), 0, sizeof((*(char (*)[50])&((BrUiCtl_ *)(this))->aChild)));
    (*(int *)&((BrUiCtl_ *)(this))->pOwner) = 0;
    (*(int *)&((BrUiCtl_ *)(this))->f2AEC) = 1;
    memset((*(int (*)[25])&((BrUiCtl_ *)(this))->a2AF0), 0xFF, sizeof((*(int (*)[25])&((BrUiCtl_ *)(this))->a2AF0)));
    (*(int *)&((BrUiCtl_ *)(this))->f2B54) = 1;
    (*(int *)&((BrUiCtl_ *)(this))->f2B58) = 0;
    (*(int *)&((BrUiCtl_ *)(this))->pfn10) = 0;
}

/* C entry points (generated by ports/64b/tools/methodfwd.py) */
#include <new>
extern "C" void * BrMenuObjCtor_10040B10(void *self)
{
    return (void *)new (self) MenuObj40B10();
}
/* end of C entry points */
