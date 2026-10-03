/* br_vtables.h -- the original's C++ vtables, as data.
 *
 * Each table is an array of the C entry points of its class's methods, in
 * the original slot order, lifted from BRGlide.dll's .rdata (one pointer per
 * slot at any pointer size).  An object's first field points at one of
 * them; C++ views call through it with ordinary virtual calls, C code
 * through the typed vtable structs. */
#ifndef BR_VTABLES_H
#define BR_VTABLES_H
#ifdef __cplusplus
extern "C" {
#endif

extern void *const g_brVtbl_10077680[];   /* UI control (BrUiCtl_) */
extern void *const g_brVtbl_10077720[];   /* text list (BrTextList) */
extern void *const g_brVtbl_100776C8[];   /* phase (BrPhase_) */
extern void *const g_brVtbl_100776C0[];   /* UI page (BrUiPage_) */
extern void *const g_brVtbl_100776F0[];   /* text box (BrTextBox) */
extern void *const g_brVtbl_10077150[];   /* the POD archive (BrPodArcObj) */
extern void *const g_brVtbl_10077750[];   /* name list (BrNameList) */

#ifdef __cplusplus
}
#endif
#endif
