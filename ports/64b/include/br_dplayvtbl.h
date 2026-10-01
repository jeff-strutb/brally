/* br_dplayvtbl.h: the DirectPlay interface's vtable as the game uses it --
 * the two slots it calls, at their positions in the real interface. */
#ifndef BR_DPLAYVTBL_H
#define BR_DPLAYVTBL_H
#ifdef __cplusplus
extern "C" {  /* BR_CLINK_BEGIN: every original function has C linkage */
#endif
#include <stdint.h>
struct BrDPlay;
struct BrDPSessionDesc;
typedef int32_t (*BrComGetFn)(void *pThis, void *pParam, void *pvBuf, uint32_t *pcb);
typedef struct BrDPlayVtbl {
    void      *aSlots0[21];                 /* slots 0..20 */
    BrComGetFn pfnGet;                      /* slot 21, +0x54 */
    void      *aSlots22[9];                 /* slots 22..30 */
    long     (*pfnSetSessionDesc)(struct BrDPlay *pThis, struct BrDPSessionDesc *pDesc,
                                  uint32_t dwFlags);   /* slot 31, +0x7C */
} BrDPlayVtbl;
#ifdef __cplusplus
}  /* BR_CLINK_END */
#endif
#endif
