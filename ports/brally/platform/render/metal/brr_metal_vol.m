/* brr_metal_vol.m: the volumetric fog's frame (brr_metal_vol.h): its 3D
 * textures (the injected cells, two, last frame's and this; the integrated
 * volume), its three pipelines, and their passes after the lighting. */
#import <Metal/Metal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "brr_metal_vol.h"

static id<MTLComputePipelineState> s_vinj, s_vint;
static id<MTLRenderPipelineState> s_vapp;
static id<MTLTexture> s_vcell[2], s_vint3;
static BrrVU s_vu;
static float s_pscr[16], s_peye[3];
static int s_vcur, s_vhave, s_vfailed;
static unsigned s_vframe;
static LightSpot s_vbeam[VOL_BEAMS];
static int s_nvbeam;

void brr_vol_beams(const LightSpot *s, int n)
{
    s_nvbeam = n < 0 ? 0 : n > VOL_BEAMS ? VOL_BEAMS : n;
    if (s_nvbeam)
        memcpy(s_vbeam, s, (size_t)s_nvbeam * sizeof *s);
}

/* the tier: the volume on the GPUs of the Remastered tier (Apple GPU family 7
 * or Metal 3); BR_LIGHT_VOL=0 turns it off, =1 forces it on */
int brr_vol_on(id<MTLDevice> dev)
{
    const char *e = getenv("BR_LIGHT_VOL");
    if (s_vfailed)
        return 0;
    if (e)
        return atoi(e) != 0;
    return [dev supportsFamily:MTLGPUFamilyApple7] || [dev supportsFamily:MTLGPUFamilyMetal3];
}

static id<MTLTexture> vol_tex(id<MTLDevice> dev, MTLTextureUsage use)
{
    MTLTextureDescriptor *td = [MTLTextureDescriptor new];
    td.textureType = MTLTextureType3D;
    td.pixelFormat = MTLPixelFormatRGBA16Float;
    td.width = VOL_NX;
    td.height = VOL_NY;
    td.depth = VOL_NZ;
    td.usage = use;
    td.storageMode = MTLStorageModePrivate;
    return [dev newTextureWithDescriptor:td];
}

static int vol_init(id<MTLDevice> dev, id<MTLLibrary> lib)
{
    NSError *err = nil;
    MTLRenderPipelineDescriptor *d = [MTLRenderPipelineDescriptor new];
    int k;
    s_vinj = [dev newComputePipelineStateWithFunction:[lib newFunctionWithName:@"vol_inject"] error:&err];
    s_vint = [dev newComputePipelineStateWithFunction:[lib newFunctionWithName:@"vol_integrate"] error:&err];
    d.vertexFunction = [lib newFunctionWithName:@"lvs"];
    d.fragmentFunction = [lib newFunctionWithName:@"vol_apply"];
    d.colorAttachments[0].pixelFormat = MTLPixelFormatRGBA16Float;
    d.colorAttachments[0].blendingEnabled = YES;          /* colour * T + S */
    d.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorOne;
    d.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorSourceAlpha;
    d.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorZero;
    d.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOne;
    s_vapp = [dev newRenderPipelineStateWithDescriptor:d error:&err];
    if (!s_vinj || !s_vint || !s_vapp) {
        fprintf(stderr, "brr: volume pipelines: %s\n", [[err description] UTF8String]);
        s_vfailed = 1;
        return 0;
    }
    for (k = 0; k < 2; k++)
        s_vcell[k] = vol_tex(dev, MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite);
    s_vint3 = vol_tex(dev, MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite);
    return 1;
}

static uint8_t s_vgiu[512];
static size_t s_vgiu_n;
static id<MTLBuffer> s_vgoff, s_vgoff0;
static id<MTLTexture> s_vgirr, s_vgdst;
void brr_vol_gi(const void *giu, size_t n, id<MTLBuffer> off, id<MTLTexture> irr, id<MTLTexture> dst)
{
    s_vgiu_n = giu && n <= sizeof s_vgiu ? n : 0;
    if (s_vgiu_n)
        memcpy(s_vgiu, giu, n);
    s_vgoff = off;
    s_vgirr = irr;
    s_vgdst = dst;
}

void brr_vol_frame(id<MTLDevice> dev, id<MTLLibrary> lib, id<MTLCommandBuffer> cmd, const void *lu, size_t lusz,
                   id<MTLBuffer> lset, id<MTLTexture> shadow, id<MTLTexture> gpos, id<MTLTexture> hdr,
                   const float eye[3], const float scr[16], int w, double time)
{
    BrrVU *v = &s_vu;
    id<MTLComputeCommandEncoder> ce;
    id<MTLRenderCommandEncoder> re;
    MTLRenderPassDescriptor *rp;
    int prev;
    if (!s_vinj && !vol_init(dev, lib))
        return;
    if (w < 0 || w > 4)
        w = 0;
    memset(v, 0, sizeof *v);
    vol_inverse(scr, v);
    memcpy(v->pscr, s_pscr, sizeof s_pscr);
    memcpy(v->peye, s_peye, sizeof s_peye);
    v->peye[3] = s_vhave ? 1.0f : 0.0f;
    v->grid[0] = VOL_NX; v->grid[1] = VOL_NY; v->grid[2] = VOL_NZ; v->grid[3] = VOL_FAR;
    v->tgt[0] = (float)hdr.width; v->tgt[1] = (float)hdr.height;
    v->tgt[2] = fmodf(0.5f + 0.6180340f * (float)(s_vframe++ % 4096u), 1.0f);   /* the sample's depth in its slice: golden-ratio steps */
    v->tgt[3] = VOL_NEAR;
    v->med[0] = VOL_KOSCHMIEDER / k_vol[w].vis; v->med[1] = k_vol[w].albedo; v->med[2] = k_vol[w].height;
    v->med[3] = eye[2] - 2.0f;          /* the ground, about a car's camera height below the eye */
    v->nse[0] = k_vol[w].noise; v->nse[1] = 1.0f / 24.0f; v->nse[2] = (float)fmod(time, 1000.0); v->nse[3] = k_vol[w].g;
    v->hist[0] = VOL_HISTORY;
    {
        int k;
        for (k = 0; k < s_nvbeam; k++) {
            memcpy(v->bpos[k], s_vbeam[k].pos, 12);
            v->bpos[k][3] = s_vbeam[k].candela;
            memcpy(v->baxis[k], s_vbeam[k].axis, 12);
            v->baxis[k][3] = s_vbeam[k].cosout;
            memcpy(v->bcol, s_vbeam[k].rgb, 12);
            v->bcol[3] = s_vbeam[k].side[0];
        }
    }
    prev = s_vcur;
    s_vcur ^= 1;

    ce = [cmd computeCommandEncoder];
    [ce setComputePipelineState:s_vinj];
    [ce setBytes:lu length:lusz atIndex:0];
    [ce setBytes:v length:sizeof *v atIndex:1];
    [ce setBuffer:lset offset:0 atIndex:2];
    [ce setTexture:shadow atIndex:0];
    [ce setTexture:s_vcell[prev] atIndex:1];
    [ce setTexture:s_vcell[s_vcur] atIndex:2];
    {   /* the irradiance probes (brr_vol_gi): off with a zero volume */
        static const uint8_t zero[512];
        int on = s_vgiu_n > 0 && s_vgoff && s_vgirr && s_vgdst;
        [ce setBytes:on ? s_vgiu : zero length:on ? (NSUInteger)s_vgiu_n : sizeof zero atIndex:3];
        if (!s_vgoff0)
            s_vgoff0 = [dev newBufferWithLength:16 options:MTLResourceStorageModeShared];
        [ce setBuffer:on ? s_vgoff : s_vgoff0 offset:0 atIndex:4];
        [ce setTexture:on ? s_vgirr : s_vcell[prev] atIndex:3];
        [ce setTexture:on ? s_vgdst : s_vcell[prev] atIndex:4];
    }
    [ce dispatchThreads:MTLSizeMake(VOL_NX, VOL_NY, VOL_NZ) threadsPerThreadgroup:MTLSizeMake(8, 8, 4)];
    [ce setComputePipelineState:s_vint];
    [ce setTexture:s_vcell[s_vcur] atIndex:0];
    [ce setTexture:s_vint3 atIndex:1];
    [ce dispatchThreads:MTLSizeMake(VOL_NX, VOL_NY, 1) threadsPerThreadgroup:MTLSizeMake(16, 8, 1)];
    [ce endEncoding];

    rp = [MTLRenderPassDescriptor renderPassDescriptor];
    rp.colorAttachments[0].texture = hdr;
    rp.colorAttachments[0].loadAction = MTLLoadActionLoad;
    rp.colorAttachments[0].storeAction = MTLStoreActionStore;
    re = [cmd renderCommandEncoderWithDescriptor:rp];
    [re setRenderPipelineState:s_vapp];
    [re setFragmentBytes:lu length:lusz atIndex:0];
    [re setFragmentBytes:v length:sizeof *v atIndex:1];
    [re setFragmentTexture:gpos atIndex:0];
    [re setFragmentTexture:s_vint3 atIndex:1];
    [re drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
    [re endEncoding];

    memcpy(s_pscr, scr, sizeof s_pscr);
    memcpy(s_peye, eye, sizeof s_peye);
    s_vhave = 1;
}

/* the forward pass's binding of this frame's volume (fsf: vu, iv), after
 * brr_vol_frame: the blended 3D fogged as the opaque behind it was */
void brr_vol_bind(id<MTLRenderCommandEncoder> e, NSUInteger buf, NSUInteger tex)
{
    static id<MTLTexture> none;         /* the volume off: a texture to bind, never read */
    if (!s_vint3 && !none) {
        MTLTextureDescriptor *td = [MTLTextureDescriptor new];
        td.textureType = MTLTextureType3D;
        td.pixelFormat = MTLPixelFormatRGBA16Float;
        td.width = td.height = td.depth = 1;
        td.usage = MTLTextureUsageShaderRead;
        td.storageMode = MTLStorageModePrivate;
        none = [e.device newTextureWithDescriptor:td];
    }
    [e setFragmentBytes:&s_vu length:sizeof s_vu atIndex:buf];
    [e setFragmentTexture:s_vint3 ? s_vint3 : none atIndex:tex];
}
