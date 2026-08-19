/* br_drawenv.h -- the track/environment display-list emitter.
 *
 *   0x10017110 (glide) / 0x10019B50 (d3d)  BrEnvEmit
 *
 * Sets up the track surface rendering: combiner, texture, prim colour,
 * a projected visibility bitmap for cars, and per-segment tile
 * coordinates.  Called once per frame by the scene driver 0x10011FA0.
 *
 * NOT CLAIMED (@implements withheld): the two FP-dense inner loops
 * (per-car projection and per-segment tile computation, ~115 x87
 * instructions total) are deferred as TODO pending x87emu verification.
 */
#ifndef BR_DRAWENV_H
#define BR_DRAWENV_H

#include <stdint.h>

#include "br_mat.h"
#include "br_vec.h"

/* The track section index -- selects into several BSS arrays. */
extern int32_t  g_BrEnvSection;           /* glide 0x106EC798 */

/* Track-flag gate: count + array of uint16 indices, plus the 84-byte
 * record array they index into (g_BrDrawTrackFlags in br_drawcar.h). */
extern int32_t  g_BrEnvFlagCount;         /* glide 0x106E8A18 */
extern uint16_t *g_BrEnvFlagIndices;      /* glide 0x106ED528 (fixed BSS) */

/* Camera/reference pointer; BrMtxInvert uses it. */
extern void    *g_BrEnvCamPtr;            /* glide 0x106ED520 */

/* Othermode payload. */
extern uint32_t g_BrEnvOthermode;         /* glide 0x106E72E8 */

/* The environment light direction (vec3 at a per-section offset). */
extern void    *g_BrEnvLightDirs;         /* glide 0x104B15D0 (BSS array) */

/* Per-section texture lookup + default. */
extern uint32_t *g_BrEnvTexLookup;        /* glide 0x1184C460 (BSS) */
extern uint32_t  g_BrEnvTexDefault;       /* glide 0x1184C478 */

/* The per-section segment data + count. */
extern int32_t  g_BrEnvSegCount;          /* glide 0x104ADD38 */
extern void    *g_BrEnvSegBase;           /* computed from 0x104ADD50 + section*3132 */

/* The visibility bitmap -- written by the per-car loop, read by the
 * segment loop.  Lives at glide 0x104AF5C8, stride 0x1000 per section. */
extern uint8_t *g_BrEnvBitmap;            /* glide 0x104AF5C8 (BSS) */

/* A light/fog direction vec3. */
extern BrVec3   g_BrEnvFogDir;            /* glide 0x106E72A8 */

void BrEnvEmit(void);

#endif /* BR_DRAWENV_H */
