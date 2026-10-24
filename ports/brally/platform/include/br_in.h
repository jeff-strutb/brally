/* br_in.h: the game's input devices (platform/common/br_in.c).
 *
 * The game opens the input system, makes a device for the keyboard, the
 * mouse and the joystick it finds (br_in_pads hands each one's description,
 * whose GUID br_in_create takes back), holds each (br_in_acquire) and reads
 * its state into the blocks it was written for. The results are its
 * codes: BR_IN_OK, BR_IN_FALSE for "already so", or a failure below 0.
 *
 * Force-included into the core. */
#ifndef BR_IN_H
#define BR_IN_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BR_IN_OK            0
#define BR_IN_FALSE         1
#define BR_IN_FAIL          ((int32_t)0x80004005)
#define BR_IN_NOTACQUIRED   ((int32_t)0x8007000C)

typedef int32_t (*br_in_pad_fn)(const void *inst, void *ref);

void   *br_in_open(void);
int32_t br_in_create(const void *guid, void **dev);
int32_t br_in_pads(uint32_t type, br_in_pad_fn cb, void *ref, uint32_t flags);
int32_t br_in_acquire(void *dev);
int32_t br_in_unacquire(void *dev);
int32_t br_in_poll(void *dev);
int32_t br_in_state(void *dev, uint32_t n, void *out);
int32_t br_in_caps(void *dev, void *caps);
int32_t br_in_set_property(void *dev, uintptr_t prop, const void *ph);
void    br_in_release(void *obj);

/* force feedback (none of the host's devices makes it) */
int32_t br_in_effect_new(void *dev, const void *guid, const void *params, void **eff);
int32_t br_in_effect_params(void *eff, const void *params, uint32_t flags);
int32_t br_in_effect_start(void *eff, uint32_t n, uint32_t flags);
int32_t br_in_effect_stop(void *eff);

#ifdef __cplusplus
}
#endif
#endif
