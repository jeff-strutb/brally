/* glide.h: the 3Dfx Glide 2.x API the core draws through.
 *
 * The original links glide2x.dll; on every 64-bit platform the renderer
 * (Metal, OpenGL, and the Windows choice) implements these entry points.
 * Types follow the Glide 2.x SDK with the game's build settings (two TMUs,
 * so GrVertex is 0x3C bytes). Only GrTexInfo holds a pointer; everything
 * else lays out as the original. Enumerations are plain 32-bit values. */
#ifndef BR_GLIDE_H
#define BR_GLIDE_H

#include <stdint.h>

typedef int32_t  FxI32;
typedef uint32_t FxU32;
typedef uint16_t FxU16;
typedef uint8_t  FxU8;
typedef int32_t  FxBool;

typedef FxI32 GrChipID_t;
typedef FxU32 GrColor_t;
typedef FxU8  GrAlpha_t;
typedef FxU8  GrFog_t;
typedef FxI32 GrLOD_t;
typedef FxI32 GrAspectRatio_t;
typedef FxI32 GrTextureFormat_t;
typedef FxI32 GrTextureFilterMode_t;
typedef FxI32 GrTextureClampMode_t;
typedef FxI32 GrMipMapMode_t;
typedef FxI32 GrCombineFunction_t;
typedef FxI32 GrCombineFactor_t;
typedef FxI32 GrCombineLocal_t;
typedef FxI32 GrCombineOther_t;
typedef FxI32 GrAlphaBlendFnc_t;
typedef FxI32 GrCmpFnc_t;
typedef FxI32 GrCullMode_t;
typedef FxI32 GrDepthBufferMode_t;
typedef FxI32 GrBuffer_t;
typedef FxI32 GrLfbSrcFmt_t;
typedef FxI32 GrScreenResolution_t;
typedef FxI32 GrScreenRefresh_t;
typedef FxI32 GrColorFormat_t;
typedef FxI32 GrOriginLocation_t;

#define GLIDE_NUM_TMU      2
#define GR_FOG_TABLE_SIZE  64

typedef struct GrTmuVertex {
    float sow, tow, oow;
} GrTmuVertex;

typedef struct GrVertex {
    float x, y, z;          /* +0x00 */
    float r, g, b;          /* +0x0C */
    float ooz;              /* +0x18 */
    float a;                /* +0x1C */
    float oow;              /* +0x20 */
    GrTmuVertex tmuvtx[GLIDE_NUM_TMU];   /* +0x24 */
} GrVertex;

typedef struct GrTexInfo {
    GrLOD_t           smallLod;
    GrLOD_t           largeLod;
    GrAspectRatio_t   aspectRatio;
    GrTextureFormat_t format;
    void             *data;
} GrTexInfo;

/* the hardware description grSstQueryHardware fills: the core keeps it as
 * an opaque block and reads only the board count */
typedef struct GrHwConfiguration GrHwConfiguration;

#ifdef __cplusplus
extern "C" {
#endif

void   grGlideInit(void);
FxBool grSstQueryHardware(GrHwConfiguration *hwconfig);
void   grSstSelect(int which_sst);
FxBool grSstWinOpen(void *hWnd, GrScreenResolution_t res, GrScreenRefresh_t ref,
                    GrColorFormat_t cformat, GrOriginLocation_t org,
                    int nColBuffers, int nAuxBuffers);
void   grSstWinClose(void);

void   grBufferClear(GrColor_t color, GrAlpha_t alpha, FxU16 depth);
void   grBufferSwap(int swap_interval);
int    grBufferNumPending(void);
void   grClipWindow(FxU32 minx, FxU32 miny, FxU32 maxx, FxU32 maxy);

void   grColorCombine(GrCombineFunction_t function, GrCombineFactor_t factor,
                      GrCombineLocal_t local, GrCombineOther_t other, FxBool invert);
void   grAlphaCombine(GrCombineFunction_t function, GrCombineFactor_t factor,
                      GrCombineLocal_t local, GrCombineOther_t other, FxBool invert);
void   grAlphaBlendFunction(GrAlphaBlendFnc_t rgb_sf, GrAlphaBlendFnc_t rgb_df,
                            GrAlphaBlendFnc_t alpha_sf, GrAlphaBlendFnc_t alpha_df);
void   grAlphaTestFunction(GrCmpFnc_t function);
void   grAlphaTestReferenceValue(GrAlpha_t value);
void   grConstantColorValue(GrColor_t value);
void   grCullMode(GrCullMode_t mode);
void   grDepthBufferMode(GrDepthBufferMode_t mode);
void   grDepthBufferFunction(GrCmpFnc_t function);
void   grDepthMask(FxBool mask);

void   grDrawTriangle(const GrVertex *a, const GrVertex *b, const GrVertex *c);
void   grDrawPolygonVertexList(int nVerts, const GrVertex vlist[]);

void   grFogColorValue(GrColor_t fogcolor);
void   grFogTable(const GrFog_t ft[GR_FOG_TABLE_SIZE]);
void   guFogGenerateLinear(GrFog_t fogtable[GR_FOG_TABLE_SIZE], float nearZ, float farZ);

void   grTexCombine(GrChipID_t tmu, GrCombineFunction_t rgb_function,
                    GrCombineFactor_t rgb_factor, GrCombineFunction_t alpha_function,
                    GrCombineFactor_t alpha_factor, FxBool rgb_invert, FxBool alpha_invert);
void   grTexFilterMode(GrChipID_t tmu, GrTextureFilterMode_t minfilter_mode,
                       GrTextureFilterMode_t magfilter_mode);
void   grTexClampMode(GrChipID_t tmu, GrTextureClampMode_t s_clampmode,
                      GrTextureClampMode_t t_clampmode);
void   grTexMipMapMode(GrChipID_t tmu, GrMipMapMode_t mode, FxBool lodBlend);
void   grTexLodBiasValue(GrChipID_t tmu, float bias);
void   grTexSource(GrChipID_t tmu, FxU32 startAddress, FxU32 evenOdd, GrTexInfo *info);
void   grTexDownloadMipMap(GrChipID_t tmu, FxU32 startAddress, FxU32 evenOdd, GrTexInfo *info);
FxU32  grTexTextureMemRequired(FxU32 evenOdd, GrTexInfo *info);
FxU32  grTexCalcMemRequired(GrLOD_t smallLod, GrLOD_t largeLod,
                            GrAspectRatio_t aspect, GrTextureFormat_t format);
FxU32  grTexMinAddress(GrChipID_t tmu);
FxU32  grTexMaxAddress(GrChipID_t tmu);

FxBool grLfbWriteRegion(GrBuffer_t dst_buffer, FxU32 dst_x, FxU32 dst_y,
                        GrLfbSrcFmt_t src_format, FxU32 src_width, FxU32 src_height,
                        FxI32 src_stride, void *src_data);

#ifdef __cplusplus
}
#endif
#endif
