/* br_gfx.h -- renderer interface.
 *
 * Deliberately free of any platform or backend types: no Metal, no Direct3D,
 * no Glide. The original game proved this seam exists -- BRD3D.dll and
 * BRGlide.dll are the same core against two backends, diverging at ~73
 * functions. This header is the portable side of that seam; backends live
 * under gfx/<backend>/.
 *
 * Coordinates are pixels with the origin at the top-left, matching the
 * original's 2D blit conventions.
 */
#ifndef BR_GFX_H
#define BR_GFX_H

#include <stdint.h>

typedef struct BrGfx BrGfx;

/* Opaque texture handle. 0 is never valid. */
typedef uint32_t BrTexture;

/* Create a renderer with an offscreen target of the given size. */
BrGfx *BrGfxCreate(uint32_t width, uint32_t height);
void   BrGfxDestroy(BrGfx *pGfx);

/* Upload RGBA8888 pixels. Returns 0 on failure. */
BrTexture BrGfxCreateTexture(BrGfx *pGfx, uint32_t width, uint32_t height,
                             const uint8_t *pRgba);

void BrGfxBeginFrame(BrGfx *pGfx, float r, float g, float b, float a);
void BrGfxDrawTexture(BrGfx *pGfx, BrTexture tex,
                      float x, float y, float w, float h);
void BrGfxEndFrame(BrGfx *pGfx);

/* Read the target back as RGBA8888 (width*height*4). For headless testing. */
int  BrGfxReadPixels(BrGfx *pGfx, uint8_t *pRgbaOut);

/* --- optional windowed presentation -------------------------------------
 * A backend may present its target in a native window. Headless use (tests)
 * simply never calls these. Signatures stay free of platform types. */

int  BrGfxOpenWindow(BrGfx *pGfx, const char *pszTitle);
/* Process pending OS events. Returns 0 when the user has asked to quit. */
int  BrGfxPumpEvents(BrGfx *pGfx);
/* Blit the render target to the window. */
void BrGfxPresent(BrGfx *pGfx);

/* --- keyboard -----------------------------------------------------------
 * The seam is deliberately TINY, and deliberately not the game's.
 *
 * Boss Rally reads its keyboard through DirectInput (0x1005FFB0 and friends),
 * which is neither portable nor separable from Win32; none of it is dragged
 * in here. What the menu actually needs from an input device is four verbs,
 * and a backend's whole job is to turn its native events into them. The core
 * never sees a key code, a scan code or an event structure.
 *
 * BR_KEY_NONE is returned when the queue is empty, so a caller can drain with
 *
 *     while ((k = BrGfxPollKey(gfx)) != BR_KEY_NONE) ...
 *
 * A backend with no keyboard (the offscreen path) returns BR_KEY_NONE always,
 * which is why nothing has to special-case headless. */
typedef enum BrKey {
    BR_KEY_NONE = 0,
    BR_KEY_UP,
    BR_KEY_DOWN,
    BR_KEY_ACTIVATE,   /* Enter or Space */
    BR_KEY_BACK,       /* Escape         */

    /* HARNESS-ONLY, not the game's. The original has no "show me a different
     * screen" key -- screens are reached by activating a control, and most of
     * those hooks are unported. These let a viewer page through the sixteen
     * ported builders directly, which is a debugging affordance and is
     * labelled as one wherever it is used. */
    BR_KEY_PREV_SCREEN,   /* [ */
    BR_KEY_NEXT_SCREEN    /* ] */
} BrKey;

/* Dequeue one key, or BR_KEY_NONE. Only meaningful after BrGfxPumpEvents. */
BrKey BrGfxPollKey(BrGfx *pGfx);

/* Last backend error, or NULL. */
const char *BrGfxLastError(void);

#endif /* BR_GFX_H */
