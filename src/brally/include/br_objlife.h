/* br_objlife.h -- startup: construct, destroy, and atexit the session's
 * C++ object arrays and named buffers.
 *
 * Responsibility: bring objects up and take them down.
 */
#ifndef BR_OBJLIFE_H
#define BR_OBJLIFE_H

#include <stdint.h>

int  BrInstall_1001BAE0(void);           /* 0x1001BAE0  install ctor/dtor */
void BrWrap_100715E0(void);              /* 0x100715E0  construct 16 objects */
void BrWrap_10071610(void);              /* 0x10071610  destroy them */
void BrAtexit_10071600(void);            /* 0x10071600  destroy them at exit */
void BrAtexit_10038EA0(void);            /* 0x10038EA0  atexit, other object */
void BrAtexit_10069A70(void);            /* 0x10069A70  atexit, other object */
void BrWrap_10067980(void);              /* 0x10067980  fill a 64-byte buffer */
void BrWrap_100679A0(void);              /* 0x100679A0  bind that buffer */
void BrWrap_10067960(void *pObj);        /* 0x10067960  bind a per-object block */
void BrWrap_10067940(void *pObj);        /* 0x10067940  fill that block (~90 KB) */
void BrFlagInit_1002B950(void);          /* 0x1002B950  empty-state retarget */
void BrFlagInit_1002F690(void);          /* 0x1002F690  suppress part 2, slot 4 */
int  BrSet_1002F6E0(void);               /* 0x1002F6E0  dispatch slot 2 */
void BrWrap_1003DAE0(void);              /* 0x1003DAE0  talk to a live COM obj */
void BrTableCopySlot_10024AB0(int dst, int src);
void BrTableSetField_10025800(int idx, uint32_t v);
void BrArm_100378A0(void);
void BrSet_10036020(void);
void BrStore_10086B80(uint32_t v);
void BrWrap_10035610(void *p);

extern uint32_t g_67D550, g_0A81C8, g_AC300, g_690A14;
extern uint32_t g_690A24, g_690A28;
extern uint32_t g_0B3A68, g_0B39B0, g_1826BD0;
extern uint32_t *g_A9D008;
extern uint32_t *g_57543C;
extern uint32_t g_6C7C44, g_6C7C38, g_18AC2D0;

#endif
