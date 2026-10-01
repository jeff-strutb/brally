/* br_objlife.h -- startup: construct, destroy, and atexit the session's
 * C++ object arrays and named buffers.
 *
 * Responsibility: bring objects up and take them down.
 */
#ifndef BR_OBJLIFE_H
#define BR_OBJLIFE_H
#ifdef __cplusplus
extern "C" {  /* BR_CLINK_BEGIN: every original function has C linkage */
#endif

#include <stdint.h>

int  BrInstall_1001BAE0(void);           /* 0x1001BAE0  install ctor/dtor */
/* BrWrap_100715E0: prototype in br_funcs.h */
/* BrWrap_10071610: prototype in br_funcs.h */
/* BrAtexit_10071600: prototype in br_funcs.h */
/* BrAtexit_10038EA0: prototype in br_funcs.h */
/* BrAtexit_10069A70: prototype in br_funcs.h */
/* BrWrap_10067980: prototype in br_funcs.h */
/* BrWrap_100679A0: prototype in br_funcs.h */
/* BrWrap_10067960: prototype in br_funcs.h */
/* BrWrap_10067940: prototype in br_funcs.h */
/* BrFlagInit_1002B950: prototype in br_funcs.h */
/* BrFlagInit_1002F690: prototype in br_funcs.h */
int  BrSet_1002F6E0(void);               /* 0x1002F6E0  dispatch slot 2 */
void BrWrap_1003DAE0(void);              /* 0x1003DAE0  talk to a live COM obj */
void BrTableCopySlot_10024AB0(int dst, int src);
void BrTableSetField_10025800(int idx, uint32_t v);
/* BrArm_100378A0: prototype in br_funcs.h */
void BrSet_10036020(void);
void BrStore_10086B80(uint32_t v);
void BrWrap_10035610(void *p);

/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

#ifdef __cplusplus
}  /* BR_CLINK_END */
#endif
#endif
