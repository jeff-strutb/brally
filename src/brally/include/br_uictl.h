/* br_uictl.h -- the control constructor, 0x100476C0.
 *
 * The type itself lives in slice6_73.h, which is the header that recovered it.
 * This header exists only so the constructor can be defined in its own
 * translation unit without dragging slice6_73.c's module globals along.
 *
 * slice3_33.h declares the same function under the same name; that declaration
 * and this one agree, so including both is safe.
 */
#ifndef BR_UICTL_H
#define BR_UICTL_H

#include "slice6_73.h"

/* XSLICE 0x100476C0 -- __thiscall control constructor; returns `this`. */
BrUiCtl_ *BrUiCtlCtor(BrUiCtl_ *pThis);

/* 0x1008F6B8 -- the vtable the constructor stores. Zeroed on purpose: see the
 * note in br_uictl.c. Exposed so tests can assert the store happened. */
extern const BrUiCtlVtbl_ g_brUiCtlVtbl_1008F6B8;

/* The vtable BrUiCtlCtor stores. Defaults to an all-NULL vtable, which is what
 * the port can honestly claim to know; a NULL slot therefore faults on the
 * first virtual call. Set this to install instrumented or ported slots. */
extern const BrUiCtlVtbl_ *g_pBrUiCtlVtbl;

/* Number of original writes that have no modelled destination yet. A test
 * asserts this, so the count cannot drift silently as fields are added. */
extern const int g_brUiCtlUnmodelledWrites;

#endif /* BR_UICTL_H */
