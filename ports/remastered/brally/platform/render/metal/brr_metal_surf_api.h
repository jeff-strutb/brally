/* brr_metal_surf_api.h: the Remastered textures' GPU side on Metal
 * (brr_metal_surf.m; the shading brr_metal_surf.h; everything else, the maps,
 * the constants, the sets' texels, is light_surf.h's, shared by every
 * renderer).  The sets as two texture arrays, uploaded once they are read;
 * each texture's map as two small textures, made the first time a draw needs
 * them. */
#ifndef BRR_METAL_SURF_API_H
#define BRR_METAL_SURF_API_H

#import <Metal/Metal.h>
#include "light_surf.h"

/* the device and queue (once) */
void brr_surf_init(id<MTLDevice> dev, id<MTLCommandQueue> q);
/* a frame with the textures on: the sets read (from the first), uploaded once read */
void brr_surf_frame(void);
/* 1 when an entry (light_surf_entry) can be drawn: its map's textures and the sets on the GPU */
int  brr_surf_ready(int entry);
/* the entry's map, variation and the sets at fragment textures 11..14; entry -1 binds
 * stand-ins (every texture the shaders name bound) */
void brr_surf_bind(id<MTLRenderCommandEncoder> e, int entry);
/* the same at fragment textures at..at+3 (the lighting's blended pass: 14) */
void brr_surf_bind_at(id<MTLRenderCommandEncoder> e, int entry, int at);

#endif
