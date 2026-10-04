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

#include "br_vtables.h"

/* The control constructor.  The C++ lane wrote it as a class whose member
 * initialisers and embedded objects the compiler laid out at the original
 * offsets; here every store names the canonical field, in the original's
 * order: scalars, the three text boxes (0x10074800 runs 0x10053E70 over
 * them), the tween block, the text list (0x10054610), then the vtable
 * (0x10077680) and the body. */
extern "C" void *BrMenuObjCtor_10040B10(void *self)
{
    BrUiCtl_ *c = (BrUiCtl_ *)self;
    int i;

    c->flags1C = 1;
    c->f20 = 0;
    c->flags24 = 0;
    c->flags28 = 0;
    c->b2C = 0xFF;
    c->fTweenX = 0;
    c->fTweenY = 0;
    c->fTweenZ = 0;
    c->x = 0;
    c->y = 0;
    c->f44 = 0.99f;
    c->w48 = 0;
    c->w4A = 0;
    c->f4C = 0;
    for (i = 0; i < BR_UI_CTL_TEXTS; i++)
        BrTextBoxInit(&c->aText[i]);
    c->twXOn = 0;
    c->twYOn = 0;
    c->twXDir = 0;
    c->twYDir = 0;
    c->twXEnd = 0;
    c->twYEnd = 0;
    c->twActive = 0;
    c->twLo = 0;
    c->twHi = 0;
    c->twRate = 0;
    c->twTick = 0;
    c->twMs = 0;
    c->f3830 = 0;
    c->w3834 = 0;
    c->w3836 = 0;
    BrTextListInit(&c->list);
    c->w1E20C = 0;
    c->pVtbl = (const BrUiCtlVtbl_ *)g_brVtbl_10077680;

    c->pfn04 = 0;
    c->pfn08 = 0;
    c->pfn0C = 0;
    c->pfn14 = 0;
    c->pfn18 = 0;
    c->f2AA4 = 0;
    c->f2AA8 = 0;
    c->w2AAC = 0;
    c->wStep = 0;
    c->f2970 = 0;
    c->f2974 = 0;
    c->f2968 = 0;
    c->f296C = 0;
    memset(c->a2904, 0, sizeof c->a2904);
    memset(c->aStepMs, 0, sizeof c->aStepMs);
    memset(c->aStepId, 0xFF, sizeof c->aStepId);
    memset(c->a012A, 0xFF, sizeof c->a012A);
    memset(c->a0060, 0, sizeof c->a0060);
    memset(c->a283C, 0, sizeof c->a283C);
    c->cChild = 0;
    memset(c->aChild, 0, sizeof c->aChild);
    c->pOwner = 0;
    c->f2AEC = 1;
    memset(c->a2AF0, 0xFF, sizeof c->a2AF0);
    c->f2B54 = 1;
    c->f2B58 = 0;
    c->pfn10 = 0;
    return self;
}
