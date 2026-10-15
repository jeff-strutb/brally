/* brr_metal_vsm_api.h: the sun's virtual shadow map's entry points
 * (brr_metal_vsm.m; the map itself: brr_vsm.h) */
#ifndef BRR_METAL_VSM_API_H
#define BRR_METAL_VSM_API_H
#import <Metal/Metal.h>
#include "light.h"
#include "brr_vsm.h"

/* a frame's inputs */
typedef struct BrrVsmFrame {
    float sun[3];                  /* toward the sun (or the moon), unit */
    float eye[3];
    float pixang;                  /* radians a lit pixel, vertically */
    float sbias[4];                /* LU.sbias: the receiver's offset and depth bias, texels */
    const LightTrack *trk;         /* the static casters */
    id<MTLBuffer> trkbuf;          /* their triangles on the GPU, 3 packed_float3 each */
    id<MTLTexture> gpos, gnrm;     /* the G-buffer: world position, normal */
    id<MTLBuffer> movers, moverargs;   /* brr_metal_cast.h cast_pick's moving casters and its count (vertices, at word 0) */
    uint32_t movercap;             /* the list's room, vertices */
    id<MTLBuffer> floors;          /* the cars' floor pans, nfloor vertices */
    uint32_t nfloor;
} BrrVsmFrame;

/* the frame's pages marked, allocated and drawn into the command buffer,
 * after the G-buffer and before the lighting reads them; 0 when the map is
 * off this frame (no track, a pipeline missing): the lighting then has no
 * sun shadow from it */
int  brr_vsm_frame(id<MTLDevice> dev, id<MTLLibrary> lib, id<MTLCommandBuffer> cb, const BrrVsmFrame *f);
/* the frame's constants, for the lighting's VSU */
const VsmU *brr_vsm_u(void);
/* the map bound to a render encoder's fragment stage: VSU at buffer ub, the
 * page table at eb, the dynamic slots at db; the pool, the dynamic pool and
 * the track map at textures pt, dt, tt */
void brr_vsm_bind(id<MTLRenderCommandEncoder> e, int ub, int eb, int db, int pt, int dt, int tt);
/* the track map alone (the fog's sun) */
id<MTLTexture> brr_vsm_tmap(void);
#endif
